# vm.ps1 - suite de la VM resumible y lobo 3 (fase 25).
#
# Casos en pruebas\vm:
#   <nombre>.lobo     se compila con --vm-test y se compara <nombre>.esperado
#   <nombre>.logic3   se abre con --logic3 --captura; se compara la traza
#                     y el hash del BMP (<nombre>.bmp.sha256)
#
# Ademas comprueba la paridad clasico/VM: los scripts de conformidad que
# corren en modo clasico dan la misma secuencia de x e y en la VM (se
# excluye limites_pasos, cuyo presupuesto por fotograma es distinto).
#
# Uso:
#   pruebas\vm.ps1                 pasa la suite
#   pruebas\vm.ps1 -Filtro espera* solo los casos que encajan
#   pruebas\vm.ps1 -Repetir        ademas comprueba determinismo
#   pruebas\vm.ps1 -Actualizar     reescribe .esperado y .bmp.sha256

[CmdletBinding()]
param(
    [string]$Motor = "",
    [string]$Filtro = "",
    [switch]$Repetir,
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$casosDir = Join-Path $PSScriptRoot "vm"
$confDir = Join-Path $PSScriptRoot "conformidad"

if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
if (-not (Test-Path -LiteralPath $Motor)) {
    Write-Host "[ERROR] no existe el motor: $Motor" -ForegroundColor Red
    exit 2
}
$Motor = (Resolve-Path -LiteralPath $Motor).Path

$tmp = Join-Path $env:TEMP ("lw2000-vm-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "VM resumible y lobo 3" -ForegroundColor Cyan
Write-Host ("  motor: {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  casos: {0}" -f $casosDir) -ForegroundColor DarkGray

$entradas = New-Object System.Collections.ArrayList
foreach ($f in Get-ChildItem -LiteralPath $casosDir -File | Sort-Object Name) {
    $ext = $f.Extension.ToLowerInvariant()
    if ($ext -eq ".lobo" -or $ext -eq ".logic3") {
        [void]$entradas.Add([pscustomobject]@{
            Nombre = [IO.Path]::GetFileNameWithoutExtension($f.Name)
            Ruta = $f.FullName
            Tipo = $ext
        })
    }
}
if ($entradas.Count -eq 0) {
    Write-Host "[ERROR] no hay casos en la carpeta" -ForegroundColor Red
    exit 2
}
# Un .lobo y un .logic3 con el mismo nombre compartirian .esperado.
$repetidos = $entradas | Group-Object Nombre | Where-Object { $_.Count -gt 1 }
if ($repetidos) {
    foreach ($g in $repetidos) {
        Write-Host ("[ERROR] nombre de caso repetido: {0}" -f $g.Name) -ForegroundColor Red
    }
    exit 2
}

$total = 0
$fallos = 0
$indice = 0

Push-Location $raiz
try {
    foreach ($caso in $entradas) {
        $nombre = $caso.Nombre
        if ($Filtro -and ($nombre -notlike $Filtro)) { continue }
        $indice++
        $total++
        $traza = Join-Path $tmp ($nombre + ".traza")
        $bmp = Join-Path $tmp ($nombre + ".bmp")
        $codigo = 0
        $tieneBmp = ($caso.Tipo -eq ".logic3")

        if ($caso.Tipo -eq ".lobo") {
            $codigo = Ejecutar-Programa $Motor @("--vm-test", $caso.Ruta,
                "--traza", $traza, "--fotogramas", "8") $raiz
        } else {
            $codigo = Ejecutar-Programa $Motor @("--logic3", $caso.Ruta,
                "--captura", $bmp, "--traza", $traza,
                "--fotogramas", "8") $raiz
        }

        $problemas = New-Object System.Collections.ArrayList
        if ($codigo -ne 0) {
            [void]$problemas.Add("el motor devolvio el codigo $codigo")
        }
        if (-not (Test-Path -LiteralPath $traza)) {
            [void]$problemas.Add("no se genero la traza")
        } else {
            $obtenido = Get-TextoNormalizado $traza
            $rutaEsperado = Join-Path $casosDir ($nombre + ".esperado")
            if ($Actualizar) {
                Guardar-TextoNormalizado $rutaEsperado $obtenido
            } elseif (-not (Test-Path -LiteralPath $rutaEsperado)) {
                [void]$problemas.Add("falta " + $nombre + ".esperado (usa -Actualizar)")
            } else {
                $esperado = Get-TextoNormalizado $rutaEsperado
                if ($esperado -ne $obtenido) {
                    [void]$problemas.Add("la traza no coincide con " + $nombre + ".esperado")
                    Mostrar-Diferencia ($esperado -split "`n") ($obtenido -split "`n")
                }
            }

            if ($Repetir) {
                $traza2 = Join-Path $tmp ($nombre + ".2.traza")
                if ($caso.Tipo -eq ".lobo") {
                    [void](Ejecutar-Programa $Motor @("--vm-test", $caso.Ruta,
                        "--traza", $traza2, "--fotogramas", "8") $raiz)
                } else {
                    $bmp2 = Join-Path $tmp ($nombre + ".2.bmp")
                    [void](Ejecutar-Programa $Motor @("--logic3", $caso.Ruta,
                        "--captura", $bmp2, "--traza", $traza2,
                        "--fotogramas", "8") $raiz)
                    if ((Get-Sha256 $bmp2) -ne (Get-Sha256 $bmp)) {
                        [void]$problemas.Add("el BMP cambia entre dos ejecuciones")
                    }
                }
                if ((Get-TextoNormalizado $traza2) -ne $obtenido) {
                    [void]$problemas.Add("la traza cambia entre dos ejecuciones")
                }
            }
        }

        if ($tieneBmp) {
            $rutaHash = Join-Path $casosDir ($nombre + ".bmp.sha256")
            $hash = Get-Sha256 $bmp
            if ($Actualizar) {
                if ($hash) { Guardar-TextoNormalizado $rutaHash $hash }
            } elseif (Test-Path -LiteralPath $rutaHash) {
                $esperadoHash = (Get-TextoNormalizado $rutaHash).Trim().ToLowerInvariant()
                if ($esperadoHash -ne $hash) {
                    [void]$problemas.Add("el BMP no coincide con " + $nombre + ".bmp.sha256")
                }
            }
        }

        if ($problemas.Count -eq 0) {
            Write-Host ("  [{0,3}] {1,-22} OK" -f $indice, $nombre) -ForegroundColor Green
        } else {
            $fallos++
            Write-Host ("  [{0,3}] {1,-22} FALLA" -f $indice, $nombre) -ForegroundColor Red
            foreach ($p in $problemas) {
                Write-Host ("        - {0}" -f $p) -ForegroundColor Red
            }
        }
    }

    # --- paridad clasico / VM -----------------------------------------
    $total++
    $indice++
    $paridad = New-Object System.Collections.ArrayList
    $probados = 0
    foreach ($f in Get-ChildItem -LiteralPath $confDir -Filter *.lobo | Sort-Object Name) {
        $nombre = [IO.Path]::GetFileNameWithoutExtension($f.Name)
        if ($nombre -eq "limites_pasos") { continue }
        $esperado = Join-Path $confDir ($nombre + ".esperado")
        if (-not (Test-Path -LiteralPath $esperado)) { continue }
        $texto = Get-TextoNormalizado $esperado
        if ($texto -notmatch "(?m)^lobo: 1 scripts$") { continue }

        $argsExtra = Leer-Argumentos (Join-Path $confDir ($nombre + ".args"))
        $fotogramas = 8
        $tecla = ""
        for ($i = 0; $i -lt $argsExtra.Count; $i++) {
            if ($argsExtra[$i] -eq "--fotogramas" -and ($i + 1) -lt $argsExtra.Count) {
                $fotogramas = [int]$argsExtra[$i + 1]
            } elseif ($argsExtra[$i] -eq "--tecla" -and ($i + 1) -lt $argsExtra.Count) {
                $tecla = $argsExtra[$i + 1]
            }
        }

        $traza = Join-Path $tmp ("paridad-" + $nombre + ".traza")
        $argumentos = @("--vm-test", $f.FullName, "--traza", $traza,
                        "--fotogramas", $fotogramas.ToString())
        if ($tecla) { $argumentos += @("--tecla", $tecla) }
        [void](Ejecutar-Programa $Motor $argumentos $raiz)

        $clasico = New-Object System.Collections.ArrayList
        foreach ($linea in ($texto -split "`n")) {
            if ($linea -match '^(\d+) (-?\d+) (-?\d+)$') {
                [void]$clasico.Add($Matches[1] + " " + $Matches[2] + " " + $Matches[3])
            }
        }
        $vm = New-Object System.Collections.ArrayList
        foreach ($linea in ((Get-TextoNormalizado $traza) -split "`n")) {
            if ($linea -match '^vm (\d+) estado \d+ pc \d+ np \d+ espera \d+ x (-?\d+) y (-?\d+)') {
                [void]$vm.Add($Matches[1] + " " + $Matches[2] + " " + $Matches[3])
            }
        }
        $probados++
        if (($clasico -join "`n") -ne ($vm -join "`n")) {
            [void]$paridad.Add($nombre)
        }
    }
    if ($paridad.Count -eq 0 -and $probados -gt 0) {
        Write-Host ("  [{0,3}] {1,-22} OK ({2} scripts)" -f $indice, "paridad", $probados) -ForegroundColor Green
    } else {
        $fallos++
        Write-Host ("  [{0,3}] {1,-22} FALLA" -f $indice, "paridad") -ForegroundColor Red
        foreach ($p in $paridad) {
            Write-Host ("        - {0}: clasico y VM no coinciden" -f $p) -ForegroundColor Red
        }
        if ($probados -eq 0) {
            Write-Host "        - no hay scripts de conformidad para comparar" -ForegroundColor Red
        }
    }
} finally {
    Pop-Location
}

Write-Host ""
if ($fallos -eq 0) {
    Write-Host ("VM: {0} casos, todo OK" -f $total) -ForegroundColor Green
    exit 0
}
Write-Host ("VM: {0} casos, {1} fallos" -f $total, $fallos) -ForegroundColor Red
exit 1
