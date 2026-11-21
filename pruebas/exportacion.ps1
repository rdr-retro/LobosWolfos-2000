# exportacion.ps1 - suite de exportacion, documentacion y release 2.0 (fase 45).
#
# Congela el empaquetado .wolfo del ejemplo canonico, su restauracion,
# el game.exe autonomo y la paridad Windows/QEMU de la imagen BIOS.
# Usa rutas y volcados propios: no toca los patrones de las 22 suites
# anteriores.
#
# Casos:
#   paquete       .wolfo con LOGIC3 nativo y entrada proyecto.logic3
#   restauracion  abrir el paquete y guardar el LOGIC3 restaurado
#   legado        .wolfo sin la flag --wolfo-logic3
#   game          game.exe incrustado, sin carpetas externas
#   bios          imagen 32 MiB, avisos vacios y paridad con QEMU
#
# Uso:
#   pruebas\exportacion.ps1
#   pruebas\exportacion.ps1 -Filtro game
#   pruebas\exportacion.ps1 -Actualizar

[CmdletBinding()]
param(
    [string]$Editor = "",
    [string]$Motor = "",
    [string]$Qemu = "",
    [string]$Filtro = "",
    [int]$Segundos = 7,
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$esperadoDir = Join-Path $PSScriptRoot "exportacion\esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

$patPaquete = Join-Path $esperadoDir "paquete.entradas.esperado"
$patLegado = Join-Path $esperadoDir "legado.entradas.esperado"
$patPosiciones = Join-Path $esperadoDir "jugable.posiciones.esperado"

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
$Editor = (Resolve-Path -LiteralPath $Editor).Path
$Motor = (Resolve-Path -LiteralPath $Motor).Path
if ($Qemu -and (Test-Path -LiteralPath $Qemu)) {
    $Qemu = (Resolve-Path -LiteralPath $Qemu).Path
} else {
    $Qemu = ""
}

$Canonico = Join-Path $raiz "ejemplos\primer_juego.logic3"
if (-not (Test-Path -LiteralPath $Canonico)) {
    Write-Host "[ERROR] no existe el ejemplo canonico: $Canonico" -ForegroundColor Red
    exit 2
}

$tmp = Join-Path $env:TEMP ("lw2000-exportacion-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

$script:total = 0
$script:fallos = 0
$script:indice = 0
$script:Filtro = $Filtro
$script:Actualizar = [bool]$Actualizar

Write-Host ""
Write-Host "Exportacion, documentacion y release 2.0" -ForegroundColor Cyan
Write-Host ("  editor: {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  motor:  {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  qemu:   {0}" -f $(if ($Qemu) { $Qemu } else { "no encontrado" })) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Debe-Correr([string]$nombre) {
    return ((-not $script:Filtro) -or ($nombre -like $script:Filtro))
}

function Citar-Argumentos([string[]]$argumentos) {
    $partes = @()
    foreach ($a in $argumentos) {
        if ($a -match '[\s"]') { $partes += '"' + ($a -replace '"', '\"') + '"' }
        else { $partes += $a }
    }
    return ($partes -join ' ')
}

# Lee un archivo .wolfo (o un game.exe con blob pegado) y devuelve sus
# entradas. Cada entrada trae nombre, largo y bytes, para poder comprobar
# el LOGIC3 sin escribir archivos temporales.
function Leer-Wolfo([string]$ruta) {
    if (-not (Test-Path -LiteralPath $ruta)) { return $null }
    $bytes = [IO.File]::ReadAllBytes($ruta)
    if ($bytes.Length -lt 8) { return $null }
    $pie = [Text.Encoding]::ASCII.GetString($bytes, $bytes.Length - 8, 4)
    if ($pie -ne "LGB1") { return $null }
    $largo = [BitConverter]::ToUInt32($bytes, $bytes.Length - 4)
    $inicio = $bytes.Length - 8 - [int]$largo
    $fin = $bytes.Length - 8
    if ($inicio -lt 0 -or $inicio -gt $fin) { return $null }
    $lista = New-Object System.Collections.ArrayList
    $pos = $inicio
    while ($pos -lt $fin) {
        if ($pos + 8 -gt $fin) { return $null }
        $ln = [BitConverter]::ToUInt32($bytes, $pos)
        $ld = [BitConverter]::ToUInt32($bytes, $pos + 4)
        $pos += 8
        if ($pos + [int]$ln + [int]$ld -gt $fin) { return $null }
        $nombre = [Text.Encoding]::ASCII.GetString($bytes, $pos, [int]$ln)
        $pos += [int]$ln
        $datos = New-Object byte[] ([int]$ld)
        if ($ld -gt 0) { [Array]::Copy($bytes, $pos, $datos, 0, [int]$ld) }
        $pos += [int]$ld
        [void]$lista.Add([pscustomobject]@{
            Nombre = $nombre
            Longitud = [int]$ld
            Datos = $datos
        })
    }
    return $lista.ToArray()
}

function Get-HashBytes([byte[]]$datos) {
    $ms = New-Object IO.MemoryStream
    try {
        $ms.Write($datos, 0, $datos.Length)
        $ms.Position = 0
        $h = [Security.Cryptography.SHA256]::Create().ComputeHash($ms)
        return ([BitConverter]::ToString($h)).Replace("-", "").ToLowerInvariant()
    } finally {
        $ms.Dispose()
    }
}

function Get-Crc32([byte[]]$datos) {
    $tabla = New-Object 'int64[]' 256
    for ($i = 0; $i -lt 256; $i++) {
        $c = [int64]$i
        for ($k = 0; $k -lt 8; $k++) {
            if (($c -band [int64]1) -ne 0) {
                $c = ((($c -shr 1) -bxor [int64]3988292384) -band [int64]4294967295)
            } else {
                $c = $c -shr 1
            }
        }
        $tabla[$i] = $c
    }
    $crc = [int64]4294967295
    foreach ($b in $datos) {
        $idx = [int](($crc -bxor [int64]$b) -band [int64]0xFF)
        $crc = ((($crc -shr 8) -bxor $tabla[$idx]) -band [int64]4294967295)
    }
    return ('{0:X8}' -f ($crc -bxor [int64]4294967295))
}

# Valida el texto LOGIC3 y su CRC con el mismo polinomio del editor.
function Test-LOGIC3([byte[]]$datos, [string]$etiqueta, $problemas) {
    if (-not $datos -or $datos.Length -lt 16) {
        [void]$problemas.Add("${etiqueta}: no hay datos LOGIC3")
        return
    }
    $texto = [Text.Encoding]::ASCII.GetString($datos)
    if (-not $texto.StartsWith("LOGIC3`n")) {
        [void]$problemas.Add("${etiqueta}: falta la cabecera LOGIC3")
        return
    }
    $fin = $texto.IndexOf("FIN`n")
    if ($fin -lt 0) {
        [void]$problemas.Add("${etiqueta}: falta la linea FIN")
        return
    }
    $n = $fin + 4
    $parte = New-Object byte[] ([int]$n)
    [Array]::Copy($datos, 0, $parte, 0, $n)
    $calculado = Get-Crc32 $parte
    $declarado = ""
    foreach ($linea in ($texto -split "`n")) {
        if ($linea.StartsWith("CRC32 ")) { $declarado = $linea.Substring(6).Trim() }
    }
    if ($declarado -ne $calculado) {
        [void]$problemas.Add(("${etiqueta}: CRC32 declarado {0}, calculado {1}" -f $declarado, $calculado))
    }
    foreach ($frag in @('objeto 1 "Jugador"', 'variable "monedas"', 'tipo "codigo.lobo"')) {
        if (-not $texto.Contains($frag)) {
            [void]$problemas.Add("${etiqueta}: falta $frag")
        }
    }
}

function Comprobar-Patron([string]$etiqueta, [string]$ruta, [string[]]$obtenido, $problemas) {
    $actualizarLocal = $script:Actualizar
    if ($actualizarLocal) {
        Guardar-TextoNormalizado $ruta ($obtenido -join "`n")
        return
    }
    $esperado = Get-TextoNormalizado $ruta
    if (-not $esperado) {
        [void]$problemas.Add("falta el patron $ruta (usa -Actualizar)")
        return
    }
    if (($obtenido -join "`n") -ne $esperado) {
        [void]$problemas.Add("$etiqueta no coincide con $ruta")
        Mostrar-Diferencia ($esperado -split "`n") $obtenido
    }
}

function Leer-Posiciones([string]$texto, [string]$patron, [int]$maximo) {
    $lista = New-Object System.Collections.ArrayList
    foreach ($m in [regex]::Matches($texto, $patron,
                                    [Text.RegularExpressions.RegexOptions]::Multiline)) {
        [void]$lista.Add(($m.Groups[1].Value + " " + $m.Groups[2].Value + " " + $m.Groups[3].Value))
        if ($lista.Count -ge $maximo) { break }
    }
    return $lista.ToArray()
}

function Comprobar-Marcas([string]$imagen, $problemas) {
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

function Informar-Caso([string]$nombre, $problemas) {
    $script:indice++
    $script:total++
    if ($problemas.Count -eq 0) {
        Write-Host ("  [{0,3}] {1,-16} OK" -f $script:indice, $nombre) -ForegroundColor Green
        return
    }
    $script:fallos++
    Write-Host ("  [{0,3}] {1,-16} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
    foreach ($p in $problemas) {
        Write-Host ("        - {0}" -f $p) -ForegroundColor Red
    }
}

# Artefacto comun: el paquete completo del ejemplo canonico. Se genera una
# sola vez para que los casos puedan ejecutarse tambien con -Filtro.
$wolfoCompleto = Join-Path $tmp "canonico.wolfo"
$bmpComun = Join-Path $tmp "comun.bmp"
$codigo = Ejecutar-Programa $Editor @("--abrir-logic3", $Canonico,
              "--guardar-wolfo", $wolfoCompleto, "--wolfo-logic3",
              "--captura", $bmpComun) $raiz
if ($codigo -ne 0 -or -not (Test-Path -LiteralPath $wolfoCompleto)) {
    Write-Host "[ERROR] no se pudo generar el paquete canonico" -ForegroundColor Red
    exit 2
}

Push-Location $raiz
try {
    # --- paquete --------------------------------------------------------
    if (Debe-Correr 'paquete') {
        $problemas = New-Object System.Collections.ArrayList
        $entradas = @(Leer-Wolfo $wolfoCompleto)
        if ($entradas.Count -eq 0) {
            [void]$problemas.Add("el .wolfo no tiene un blob LGB1 valido")
        } else {
            $lineas = @($entradas | ForEach-Object { $_.Nombre + " " + $_.Longitud })
            Comprobar-Patron "entradas del paquete" $patPaquete $lineas $problemas
            $logic3 = @($entradas | Where-Object { $_.Nombre -eq "proyecto.logic3" })
            if ($logic3.Count -ne 1) {
                [void]$problemas.Add("el paquete debe llevar una sola entrada proyecto.logic3")
            } else {
                Test-LOGIC3 $logic3[0].Datos "LOGIC3 del paquete" $problemas
            }
            $clasica = @($entradas | Where-Object { $_.Nombre -eq "proyecto.logic" })
            if ($clasica.Count -ne 1) {
                [void]$problemas.Add("el paquete debe conservar proyecto.logic")
            }
        }
        Informar-Caso 'paquete' $problemas
    }

    # --- restauracion ---------------------------------------------------
    if (Debe-Correr 'restauracion') {
        $problemas = New-Object System.Collections.ArrayList
        $restaurado = Join-Path $tmp "restaurado.logic3"
        $bmp = Join-Path $tmp "restauracion.bmp"
        if (Test-Path -LiteralPath $restaurado) { Remove-Item -LiteralPath $restaurado }
        $codigo = Ejecutar-Programa $Editor @("--abrir-wolfo", $wolfoCompleto,
                      "--guardar-logic3", $restaurado, "--captura", $bmp) $raiz
        if ($codigo -ne 0) {
            [void]$problemas.Add("el editor devolvio el codigo $codigo")
        }
        if (-not (Test-Path -LiteralPath $restaurado)) {
            [void]$problemas.Add("no se restauro el LOGIC3")
        } else {
            $bytes = [IO.File]::ReadAllBytes($restaurado)
            Test-LOGIC3 $bytes "LOGIC3 restaurado" $problemas
            $entradas = @(Leer-Wolfo $wolfoCompleto | Where-Object { $_.Nombre -eq "proyecto.logic3" })
            if ($entradas.Count -eq 1) {
                if ((Get-HashBytes $bytes) -ne (Get-HashBytes $entradas[0].Datos)) {
                    [void]$problemas.Add("el LOGIC3 restaurado no coincide con la entrada del paquete")
                }
            }
        }
        Informar-Caso 'restauracion' $problemas
    }

    # --- legado ---------------------------------------------------------
    if (Debe-Correr 'legado') {
        $problemas = New-Object System.Collections.ArrayList
        $legado = Join-Path $tmp "canonico.legado.wolfo"
        $bmp = Join-Path $tmp "legado.bmp"
        $codigo = Ejecutar-Programa $Editor @("--abrir-logic3", $Canonico,
                      "--guardar-wolfo", $legado, "--captura", $bmp) $raiz
        if ($codigo -ne 0) {
            [void]$problemas.Add("el editor devolvio el codigo $codigo")
        }
        $entradas = @(Leer-Wolfo $legado)
        if ($entradas.Count -eq 0) {
            [void]$problemas.Add("el paquete legado no tiene un blob LGB1 valido")
        } else {
            $lineas = @($entradas | ForEach-Object { $_.Nombre + " " + $_.Longitud })
            Comprobar-Patron "entradas legadas" $patLegado $lineas $problemas
            if (@($entradas | Where-Object { $_.Nombre -eq "proyecto.logic3" }).Count -ne 0) {
                [void]$problemas.Add("el paquete legado no debe llevar proyecto.logic3")
            }
        }
        Informar-Caso 'legado' $problemas
    }

    # --- game -----------------------------------------------------------
    $posicionesGame = @()
    if (Debe-Correr 'game') {
        $problemas = New-Object System.Collections.ArrayList
        $game = Join-Path $tmp "game.exe"
        $bmp = Join-Path $tmp "compilar.bmp"
        $codigo = Ejecutar-Programa $Editor @("--abrir-logic3", $Canonico,
                      "--compilar-windows", $game, "--captura", $bmp) $raiz
        if ($codigo -ne 0 -or -not (Test-Path -LiteralPath $game)) {
            [void]$problemas.Add("no se pudo compilar el game.exe")
        } else {
            $incrustado = @(Leer-Wolfo $game)
            if (@($incrustado | Where-Object { $_.Nombre -eq "proyecto.logic3" }).Count -ne 1) {
                [void]$problemas.Add("el game.exe no lleva el LOGIC3 nativo incrustado")
            }
            $vacio = Join-Path $tmp "juego_vacio"
            New-Item -ItemType Directory -Force $vacio | Out-Null
            $cap = Join-Path $vacio "cap.bmp"
            $traza = Join-Path $vacio "traza.txt"
            foreach ($f in @($cap, $traza)) {
                if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f }
            }
            $codigo = Ejecutar-Programa $game @("--captura", $cap,
                          "--traza", $traza, "--fotogramas", "8") $vacio
            if ($codigo -ne 0) {
                [void]$problemas.Add("game.exe devolvio el codigo $codigo")
            }
            $texto = Get-TextoNormalizado $traza
            if ($texto -notmatch "vm: 4 scripts") {
                [void]$problemas.Add("el juego no cargo los 4 scripts VM")
            }
            if ($texto -match "lobo: error") {
                [void]$problemas.Add("el juego imprimio errores del interprete clasico")
            }
            $posicionesGame = @(Leer-Posiciones $texto '^(\d+) (-?\d+) (-?\d+)$' 8)
            Comprobar-Patron "posiciones del juego" $patPosiciones $posicionesGame $problemas
            $carpetas = @(Get-ChildItem -LiteralPath $vacio -Directory | Select-Object -ExpandProperty Name)
            if ($carpetas.Count -gt 0) {
                [void]$problemas.Add("el juego creo carpetas externas: " + ($carpetas -join ", "))
            }
        }
        Informar-Caso 'game' $problemas
    }

    # --- bios -----------------------------------------------------------
    if (Debe-Correr 'bios') {
        $problemas = New-Object System.Collections.ArrayList
        $imagen = Join-Path $tmp "canonico.img"
        $avisos = Join-Path $tmp "canonico.avisos"
        $editorBmp = Join-Path $tmp "bios_editor.bmp"
        $codigo = Ejecutar-Programa $Editor @("--abrir-logic3", $Canonico,
                      "--compilar-bios", $imagen, "--bios-avisos", $avisos,
                      "--captura", $editorBmp) $raiz
        if ($codigo -ne 0) {
            [void]$problemas.Add("el editor devolvio el codigo $codigo")
        }
        if (-not (Test-Path -LiteralPath $imagen)) {
            [void]$problemas.Add("no se genero la imagen USB")
        } else {
            if ((Get-Item -LiteralPath $imagen).Length -ne 33554432) {
                [void]$problemas.Add("la imagen USB no mide 32 MiB")
            }
            Comprobar-Marcas $imagen $problemas
        }
        if (-not (Test-Path -LiteralPath $avisos)) {
            [void]$problemas.Add("no se genero el archivo de avisos")
        } elseif ((Get-TextoNormalizado $avisos) -ne "") {
            [void]$problemas.Add("un proyecto portable no debe dar avisos")
        }

        $winTraza = Join-Path $tmp "bios_win.traza"
        $winBmp = Join-Path $tmp "bios_win.bmp"
        $codigo = Ejecutar-Programa $Motor @("--logic3", $Canonico,
                      "--captura", $winBmp, "--traza", $winTraza,
                      "--fotogramas", "8") $raiz
        if ($codigo -ne 0) {
            [void]$problemas.Add("game.exe devolvio el codigo $codigo")
        }
        $textoWindows = Get-TextoNormalizado $winTraza
        $posicionesWindows = @(Leer-Posiciones $textoWindows '^(\d+) (-?\d+) (-?\d+)$' 8)
        Comprobar-Patron "posiciones Windows" $patPosiciones $posicionesWindows $problemas

        if (-not $Qemu) {
            [void]$problemas.Add("no se encontro qemu-system-x86_64.exe")
        } elseif (Test-Path -LiteralPath $imagen) {
            $log = Join-Path $tmp "qemu.serial.log"
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
            Comprobar-Patron "posiciones QEMU" $patPosiciones $posicionesBios $problemas
            if ($textoBios -notmatch "egine: bios vm 4 programas\.") {
                [void]$problemas.Add("la BIOS no cargo 4 programas VM")
            }
            if ($textoBios -notmatch "egine: bios vm 4 scripts\.") {
                [void]$problemas.Add("la BIOS no preparo 4 scripts VM")
            }
            if ($textoBios -notmatch "egine: bios evento ") {
                [void]$problemas.Add("el planificador no entrego ningun evento")
            }
        }
        Informar-Caso 'bios' $problemas
    }
} finally {
    Pop-Location
}

Write-Host ""
Write-Host ("  {0} casos, {1} fallos" -f $script:total, $script:fallos) -ForegroundColor DarkGray
if ($script:fallos -gt 0) { exit 1 }
Write-Host "Exportacion SUPERADA." -ForegroundColor Green
exit 0
