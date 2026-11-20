# bios.ps1 - suite de paridad BIOS del subconjunto VM (fase 43).
#
# Para cada proyecto portable:
#   * el editor congela vm.lbin en una imagen USB y escribe --bios-avisos;
#   * game.exe produce la traza determinista de Windows;
#   * QEMU arranca la imagen y escribe la serie en build/serial.log;
#   * se comparan las posiciones de los N primeros fotogramas.
#
# El caso no_portable comprueba que el editor avisa de un opcode fuera del
# subconjunto sin dejar de generar la imagen.
#
# Uso:
#   pruebas\bios.ps1                    pasa la suite
#   pruebas\bios.ps1 -Filtro esperar*   solo los casos que encajan
#   pruebas\bios.ps1 -Segundos 8        mas plazo para arrancar QEMU
#   pruebas\bios.ps1 -Actualizar        reescribe los .esperado

[CmdletBinding()]
param(
    [string]$Editor = "",
    [string]$Motor = "",
    [string]$Qemu = "",
    [string]$Filtro = "",
    [int]$Segundos = 6,
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$casosDir = Join-Path $PSScriptRoot "bios"
$vmDir = Join-Path $PSScriptRoot "vm"

if (-not $Editor) { $Editor = Join-Path $raiz "LobosWolfos2000.exe" }
if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
if (-not $Qemu) {
    foreach ($candidato in @(
        (Join-Path $env:LOCALAPPDATA "Programs\qemu\qemu-system-x86_64.exe"),
        "C:\Program Files\qemu\qemu-system-x86_64.exe",
        "C:\Program Files (x86)\qemu\qemu-system-x86_64.exe"
    )) {
        if (Test-Path -LiteralPath $candidato) { $Qemu = $candidato; break }
    }
}
foreach ($exe in @($Editor, $Motor)) {
    if (-not (Test-Path -LiteralPath $exe)) {
        Write-Host "[ERROR] no existe el ejecutable: $exe" -ForegroundColor Red
        exit 2
    }
}
if (-not $Qemu -or -not (Test-Path -LiteralPath $Qemu)) {
    Write-Host "[ERROR] no se encuentra qemu-system-x86_64.exe" -ForegroundColor Red
    exit 2
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path
$Motor = (Resolve-Path -LiteralPath $Motor).Path
$Qemu = (Resolve-Path -LiteralPath $Qemu).Path

$tmp = Join-Path $env:TEMP ("lw2000-bios-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

$casos = @(
    [pscustomobject]@{
        Nombre = "esperar_dos"
        Proyecto = (Join-Path $vmDir "esperar_dos.logic3")
        Portable = 1
        Programas = 2
    },
    [pscustomobject]@{
        Nombre = "no_portable"
        Proyecto = (Join-Path $casosDir "no_portable.logic3")
        Portable = 0
        Programas = 0
    }
)

Write-Host ""
Write-Host "Paridad BIOS del subconjunto VM" -ForegroundColor Cyan
Write-Host ("  editor: {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  motor:  {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  qemu:   {0}" -f $Qemu) -ForegroundColor DarkGray

function Citar-Argumentos([string[]]$argumentos) {
    $partes = @()
    foreach ($a in $argumentos) {
        if ($a -match '[\s"]') { $partes += '"' + ($a -replace '"', '\"') + '"' }
        else { $partes += $a }
    }
    return ($partes -join ' ')
}

function Leer-Posiciones([string]$texto, [string]$patron, [int]$maximo) {
    $lista = New-Object System.Collections.ArrayList
    # Las trazas tienen muchas lineas: los anclajes ^ y $ son por linea.
    foreach ($m in [regex]::Matches($texto, $patron,
                                    [Text.RegularExpressions.RegexOptions]::Multiline)) {
        [void]$lista.Add(($m.Groups[1].Value + " " + $m.Groups[2].Value + " " + $m.Groups[3].Value))
        if ($lista.Count -ge $maximo) { break }
    }
    return $lista.ToArray()
}

function Comprobar-Marcas([string]$imagen, [string[]]$problemas) {
    $fs = [IO.File]::OpenRead($imagen)
    try {
        $fs.Seek((2048 * 512), [IO.SeekOrigin]::Begin) | Out-Null
        $buf = New-Object byte[] 8192
        $n = $fs.Read($buf, 0, $buf.Length)
    } finally {
        $fs.Close()
    }
    $texto = [Text.Encoding]::ASCII.GetString($buf, 0, $n)
    if ($texto -notmatch "vm\.lbin") {
        [void]$problemas.Add("la imagen no lleva la entrada vm.lbin")
    }
    if ($texto -notmatch "LVM1") {
        [void]$problemas.Add("la imagen no lleva el bytecode LVM1")
    }
}

$total = 0
$fallos = 0
$indice = 0

Push-Location $raiz
try {
    foreach ($caso in $casos) {
        $nombre = $caso.Nombre
        if ($Filtro -and ($nombre -notlike $Filtro)) { continue }
        $indice++
        $total++
        $problemas = New-Object System.Collections.ArrayList

        if (-not (Test-Path -LiteralPath $caso.Proyecto)) {
            [void]$problemas.Add("no existe el proyecto " + $caso.Proyecto)
            Write-Host ("  [{0,3}] {1,-16} FALLA" -f $indice, $nombre) -ForegroundColor Red
            Write-Host ("        - no existe el proyecto " + $caso.Proyecto) -ForegroundColor Red
            $fallos++
            continue
        }

        $imagen = Join-Path $tmp ($nombre + ".img")
        $avisos = Join-Path $tmp ($nombre + ".avisos")
        $editorBmp = Join-Path $tmp ($nombre + ".editor.bmp")
        $argsEditor = @("--abrir-logic3", $caso.Proyecto,
                        "--compilar-bios", $imagen,
                        "--bios-avisos", $avisos,
                        "--captura", $editorBmp)
        $codigo = Ejecutar-Programa $Editor $argsEditor $raiz
        if ($codigo -ne 0) {
            [void]$problemas.Add("el editor devolvio el codigo $codigo")
        }
        if (-not (Test-Path -LiteralPath $imagen)) {
            [void]$problemas.Add("no se genero la imagen USB")
        } else {
            Comprobar-Marcas $imagen $problemas
        }

        $avisosTexto = Get-TextoNormalizado $avisos
        $rutaAvisos = Join-Path $casosDir ($nombre + ".avisos.esperado")
        if ($caso.Portable) {
            if ($avisosTexto) {
                [void]$problemas.Add("un proyecto portable no debe dar avisos: $avisosTexto")
            }
        } else {
            if ($Actualizar) {
                Guardar-TextoNormalizado $rutaAvisos $avisosTexto
            } elseif (Test-Path -LiteralPath $rutaAvisos) {
                $esperadoAvisos = Get-TextoNormalizado $rutaAvisos
                if ($avisosTexto -notmatch [regex]::Escape($esperadoAvisos.Trim())) {
                    [void]$problemas.Add("los avisos no contienen: " + $esperadoAvisos.Trim())
                }
            } else {
                [void]$problemas.Add("falta " + $nombre + ".avisos.esperado (usa -Actualizar)")
            }
        }

        if ($caso.Portable -and -not $problemas.Count) {
            # --- traza de Windows -------------------------------------
            $traza = Join-Path $tmp ($nombre + ".traza")
            $bmp = Join-Path $tmp ($nombre + ".bmp")
            $codigo = Ejecutar-Programa $Motor @("--logic3", $caso.Proyecto,
                          "--captura", $bmp, "--traza", $traza,
                          "--fotogramas", "8") $raiz
            if ($codigo -ne 0) {
                [void]$problemas.Add("game.exe devolvio el codigo $codigo")
            }
            $textoWindows = Get-TextoNormalizado $traza
            $posicionesWindows = @(Leer-Posiciones $textoWindows '^(\d+) (-?\d+) (-?\d+)$' 8)

            # --- serie de QEMU ----------------------------------------
            $log = Join-Path $tmp ($nombre + ".serial.log")
            if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
            $qargs = @("-m", "256", "-boot", "order=c",
                       "-drive", ("if=ide,index=0,media=disk,format=raw,file=" + $imagen),
                       "-serial", ("file:" + $log),
                       "-display", "none", "-no-reboot")
            $qemuProc = Start-Process -FilePath $Qemu -ArgumentList (Citar-Argumentos $qargs) `
                                      -WorkingDirectory $raiz -WindowStyle Hidden -PassThru
            try {
                Start-Sleep -Seconds $Segundos
            } finally {
                if (-not $qemuProc.HasExited) { Stop-Process -Id $qemuProc.Id -Force }
                $qemuProc.WaitForExit()
                Start-Sleep -Milliseconds 500
            }
            $textoBios = Get-TextoNormalizado $log
            $posicionesBios = @(Leer-Posiciones $textoBios '^egine: vm (\d+) (-?\d+) (-?\d+)\.$' 8)

            $esperadas = (Join-Path $casosDir ($nombre + ".posiciones.esperado"))
            if ($Actualizar -and $posicionesWindows.Count -gt 0) {
                Guardar-TextoNormalizado $esperadas ($posicionesWindows -join "`n")
            }
            $patron = Get-TextoNormalizado $esperadas

            if ($textoBios -notmatch ("egine: bios vm " + $caso.Programas + " programas\.")) {
                [void]$problemas.Add("la BIOS no cargo " + $caso.Programas + " programas VM")
            }
            if ($textoBios -notmatch ("egine: bios vm " + $caso.Programas + " scripts\.")) {
                [void]$problemas.Add("la BIOS no preparo " + $caso.Programas + " scripts VM")
            }
            if ($textoBios -match "egine: bios: funcion .* sin bytecode") {
                [void]$problemas.Add("un funcion se quedo sin bytecode")
            }
            if ($textoBios -notmatch "egine: bios evento ") {
                [void]$problemas.Add("el planificador no entrego ningun evento")
            }
            if ($textoBios -notmatch "egine: bios entidades ") {
                [void]$problemas.Add("el planificador no cerro ningun fotograma")
            }
            if ($posicionesWindows.Count -lt 8) {
                [void]$problemas.Add("la traza de Windows no tiene 8 fotogramas")
            }
            if ($posicionesBios.Count -lt 8) {
                [void]$problemas.Add("la serie de QEMU no tiene 8 fotogramas")
            }
            if ($patron -and ($posicionesWindows -join "`n") -ne $patron) {
                [void]$problemas.Add("Windows no coincide con " + $nombre + ".posiciones.esperado")
                Mostrar-Diferencia ($patron -split "`n") $posicionesWindows
            }
            if ($patron -and ($posicionesBios -join "`n") -ne $patron) {
                [void]$problemas.Add("QEMU no coincide con " + $nombre + ".posiciones.esperado")
                Mostrar-Diferencia ($patron -split "`n") $posicionesBios
            }
        }

        if ($problemas.Count -eq 0) {
            Write-Host ("  [{0,3}] {1,-16} OK" -f $indice, $nombre) -ForegroundColor Green
        } else {
            $fallos++
            Write-Host ("  [{0,3}] {1,-16} FALLA" -f $indice, $nombre) -ForegroundColor Red
            foreach ($p in $problemas) {
                Write-Host ("        - {0}" -f $p) -ForegroundColor Red
            }
        }
    }
} finally {
    Pop-Location
}

Write-Host ""
Write-Host ("  {0} casos, {1} fallos" -f $total, $fallos) -ForegroundColor DarkGray
if ($fallos -gt 0) { exit 1 }
Write-Host "Paridad BIOS SUPERADA." -ForegroundColor Green
exit 0
