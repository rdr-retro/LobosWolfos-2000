# render.ps1 - suite de render de trajes, animacion, capas y camara (fase 30).
#
# Casos en pruebas\render:
#   <nombre>.args        argumentos de game.exe; se anaden --captura y --traza
#   <nombre>.logic3      proyecto LOGIC3 con trajes y camara
#   <nombre>.esperado    traza congelada (incluye las sondas --pixel)
#   <nombre>.bmp.sha256  hash del BMP
#   render.mav           mapa comun de los casos
#   tiles/               tiles del mapa
#   trajes/              PNG de los trajes
#
# Uso:
#   pruebas\render.ps1                  pasa la suite
#   pruebas\render.ps1 -Filtro capas*   solo los casos que encajan
#   pruebas\render.ps1 -Actualizar      reescribe esperado y hashes
#   pruebas\render.ps1 -Repetir         ademas comprueba determinismo
#
# Sin acentos: Windows PowerShell 5.1 lee los .ps1 como ANSI si no llevan
# BOM.

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
$casosDir = Join-Path $PSScriptRoot "render"

if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
if (-not (Test-Path -LiteralPath $Motor)) {
    Write-Host "[ERROR] no existe el motor: $Motor" -ForegroundColor Red
    exit 2
}
$Motor = (Resolve-Path -LiteralPath $Motor).Path

$tmp = Join-Path $env:TEMP ("lw2000-render-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Render de trajes, animacion, capas y camara" -ForegroundColor Cyan
Write-Host ("  motor: {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  casos: {0}" -f $casosDir) -ForegroundColor DarkGray

$entradas = New-Object System.Collections.ArrayList
foreach ($f in Get-ChildItem -LiteralPath $casosDir -Filter *.args | Sort-Object Name) {
    [void]$entradas.Add([pscustomobject]@{ Nombre = [IO.Path]::GetFileNameWithoutExtension($f.Name) })
}
if ($entradas.Count -eq 0) {
    Write-Host "[ERROR] no hay casos en la carpeta" -ForegroundColor Red
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

        $argsExtra = Leer-Argumentos (Join-Path $casosDir ($nombre + ".args"))
        if ($argsExtra.Count -eq 0) { $argsExtra = @("--fotogramas", "8") }
        $bmp = Join-Path $tmp ($nombre + ".bmp")
        $traza = Join-Path $tmp ($nombre + ".traza")
        $argumentos = $argsExtra + @("--captura", $bmp, "--traza", $traza)

        $codigo = Ejecutar-Programa $Motor $argumentos $raiz
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
                }
            }
        }

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

        if ($Repetir) {
            $bmp2 = Join-Path $tmp ($nombre + ".2.bmp")
            $traza2 = Join-Path $tmp ($nombre + ".2.traza")
            $argumentos2 = $argsExtra + @("--captura", $bmp2, "--traza", $traza2)
            [void](Ejecutar-Programa $Motor $argumentos2 $raiz)
            if ((Get-Sha256 $bmp2) -ne $hash) {
                [void]$problemas.Add("el BMP cambia entre dos ejecuciones")
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
            $rutaEsperado = Join-Path $casosDir ($nombre + ".esperado")
            if ((Test-Path -LiteralPath $traza) -and (Test-Path -LiteralPath $rutaEsperado)) {
                $esperadoTexto = Get-TextoNormalizado $rutaEsperado
                $obtenidoTexto = Get-TextoNormalizado $traza
                if ($esperadoTexto -ne $obtenidoTexto) {
                    Mostrar-Diferencia ($esperadoTexto -split "`n") ($obtenidoTexto -split "`n")
                }
            }
        }
    }
} finally {
    Pop-Location
}

Write-Host ""
if ($fallos -eq 0) {
    Write-Host ("Render: {0} casos, todo OK" -f $total) -ForegroundColor Green
    exit 0
}
Write-Host ("Render: {0} casos, {1} con fallos" -f $total, $fallos) -ForegroundColor Red
exit 1
