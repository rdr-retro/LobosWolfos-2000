# programa.ps1 - suite del modelo de programa (fase 46).
#
# Congela la separacion entre escenario y jugador. Un proyecto nuevo guarda
# la pieza sistema.opciones con jugador_implicito=0; un LOGIC3 sin esa pieza
# es legacy y mantiene el lobito clasico.
#
# Casos:
#   opciones_nuevo  el editor crea sistema.opciones en un proyecto nuevo
#   mapa_solo       mapa sin opciones de jugador: lobo quieto y sin sprite
#   legacy          sin sistema.opciones: lobo se mueve y el BMP cambia
#
# Uso:
#   pruebas\programa.ps1                 pasa la suite
#   pruebas\programa.ps1 -Filtro mapa*   solo los casos que encajan
#   pruebas\programa.ps1 -Actualizar     reescribe los patrones
#
# Sin acentos: Windows PowerShell 5.1 lee los .ps1 como ANSI si no llevan
# BOM.

[CmdletBinding()]
param(
    [string]$Editor = "",
    [string]$Motor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$carpeta = Join-Path $PSScriptRoot "programa"
$esperadoDir = Join-Path $carpeta "esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Editor) { $Editor = Join-Path $raiz "LobosWolfos2000.exe" }
if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
foreach ($exe in @($Editor, $Motor)) {
    if (-not (Test-Path -LiteralPath $exe)) {
        Write-Host "[ERROR] no existe el ejecutable: $exe" -ForegroundColor Red
        exit 2
    }
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path
$Motor = (Resolve-Path -LiteralPath $Motor).Path

$tmp = Join-Path $env:TEMP ("lw2000-programa-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Modelo de programa" -ForegroundColor Cyan
Write-Host ("  editor: {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  motor:  {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $carpeta) -ForegroundColor DarkGray

function Ejecutar-Editor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Editor $argumentos $dir
}

function Ejecutar-Motor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Motor $argumentos $dir
}

function Comprobar-Texto([string]$nombre, [string]$extension,
                         [string]$obtenido, $problemas) {
    $ruta = Join-Path $esperadoDir ($nombre + $extension)
    if ($Actualizar) {
        Guardar-TextoNormalizado $ruta $obtenido
        return
    }
    if (-not (Test-Path -LiteralPath $ruta)) {
        [void]$problemas.Add("falta " + $nombre + $extension + " (usa -Actualizar)")
        return
    }
    $esperado = Get-TextoNormalizado $ruta
    if ($esperado -ne $obtenido) {
        [void]$problemas.Add("no coincide " + $nombre + $extension)
        Mostrar-Diferencia ($esperado -split "`n") ($obtenido -split "`n") 8
    }
}

function Comprobar-Hash([string]$nombre, [string]$hash, $problemas) {
    $ruta = Join-Path $esperadoDir ($nombre + ".bmp.sha256")
    if ($Actualizar) {
        if ($hash) { Guardar-TextoNormalizado $ruta $hash }
        return
    }
    if (-not (Test-Path -LiteralPath $ruta)) {
        [void]$problemas.Add("falta " + $nombre + ".bmp.sha256 (usa -Actualizar)")
        return
    }
    $esperado = (Get-TextoNormalizado $ruta).Trim().ToLowerInvariant()
    if ($esperado -ne $hash.ToLowerInvariant()) {
        [void]$problemas.Add("el BMP de " + $nombre + " no coincide")
    }
}

function Get-VolcarNormalizado([string]$ruta, [string]$dir) {
    $texto = Get-TextoNormalizado $ruta
    if ($dir) { $texto = $texto.Replace($dir, "<CASO>") }
    return $texto
}

function Get-PosicionLobo([string]$ruta) {
    if (-not (Test-Path -LiteralPath $ruta)) { return "" }
    $texto = [IO.File]::ReadAllText($ruta)
    $m = [regex]::Match($texto, "(?m)^lobo\s+(-?\d+)\s+(-?\d+)\s*$")
    if (-not $m.Success) { return "" }
    return ($m.Groups[1].Value + " " + $m.Groups[2].Value)
}

$script:total = 0
$script:fallos = 0
$script:indice = 0

function Ejecutar-Caso([string]$nombre, [scriptblock]$cuerpo) {
    if ($Filtro -and ($nombre -notlike $Filtro)) { return }
    $script:indice++
    $script:total++
    $problemas = New-Object System.Collections.ArrayList
    $dir = Join-Path $tmp $nombre
    New-Item -ItemType Directory -Force $dir | Out-Null
    & $cuerpo $dir $problemas
    if ($problemas.Count -eq 0) {
        Write-Host ("  [{0,3}] {1,-15} OK" -f $script:indice, $nombre) -ForegroundColor Green
    } else {
        $script:fallos++
        Write-Host ("  [{0,3}] {1,-15} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
        foreach ($p in $problemas) {
            Write-Host ("        - {0}" -f $p) -ForegroundColor Red
        }
    }
}

Ejecutar-Caso "opciones_nuevo" {
    param($dir, $problemas)
    $logic = Join-Path $dir "nuevo.logic3"
    $bmp = Join-Path $dir "nuevo.bmp"
    $dump = Join-Path $dir "nuevo.txt"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
                                     "--clics", "52,85", `
                                     "--guardar-logic3", $logic, `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    if (-not (Test-Path -LiteralPath $logic)) {
        [void]$problemas.Add("no se guardo el LOGIC3 nuevo")
        return
    }
    $contenido = Get-TextoNormalizado $logic
    Comprobar-Texto "opciones_nuevo" ".logic3" $contenido $problemas
    Comprobar-Texto "opciones_nuevo" ".volcar" `
                    (Get-VolcarNormalizado $dump $dir) $problemas
    if ($contenido -notlike '*tipo "sistema.opciones"*') {
        [void]$problemas.Add("falta la pieza sistema.opciones")
    }
    foreach ($campo in @('campo "programa" "1"',
                         'campo "jugador_implicito" "0"',
                         'campo "auto_compilar" "1"')) {
        if ($contenido -notlike ("*" + $campo + "*")) {
            [void]$problemas.Add("falta " + $campo)
        }
    }
}

Ejecutar-Caso "mapa_solo" {
    param($dir, $problemas)
    $logic = Join-Path $carpeta "mapa_solo.logic3"
    $bmp = Join-Path $dir "mapa_solo.bmp"
    $dump = Join-Path $dir "mapa_solo.txt"
    $codigo = Ejecutar-Motor $raiz @("--logic3", $logic, "--tecla", "derecha", `
                                     "--fotogramas", "6", "--captura", $bmp, `
                                     "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el motor devolvio $codigo") }
    Comprobar-Texto "mapa_solo" ".volcado" `
                    (Get-TextoNormalizado $dump) $problemas
    Comprobar-Hash "mapa_solo" (Get-Sha256 $bmp) $problemas
    $pos = Get-PosicionLobo $dump
    if ($pos -ne "16 16") {
        [void]$problemas.Add("el lobo implicito se movio a $pos (esperado 16 16)")
    }
}

Ejecutar-Caso "legacy" {
    param($dir, $problemas)
    $logic = Join-Path $carpeta "legacy.logic3"
    $bmp = Join-Path $dir "legacy.bmp"
    $dump = Join-Path $dir "legacy.txt"
    $codigo = Ejecutar-Motor $raiz @("--logic3", $logic, "--tecla", "derecha", `
                                     "--fotogramas", "6", "--captura", $bmp, `
                                     "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el motor devolvio $codigo") }
    Comprobar-Texto "legacy" ".volcado" `
                    (Get-TextoNormalizado $dump) $problemas
    Comprobar-Hash "legacy" (Get-Sha256 $bmp) $problemas
    $pos = Get-PosicionLobo $dump
    $x = 16
    if ($pos) { $x = [int]($pos.Split(' ')[0]) }
    if ($x -le 16) {
        [void]$problemas.Add("el lobo legacy no se movio (posicion $pos)")
    }
    $patronSolo = Join-Path $esperadoDir "mapa_solo.bmp.sha256"
    if (Test-Path -LiteralPath $patronSolo) {
        $solo = (Get-TextoNormalizado $patronSolo).Trim().ToLowerInvariant()
        if ((Get-Sha256 $bmp) -eq $solo) {
            [void]$problemas.Add("el BMP legacy es identico al de mapa_solo")
        }
    }
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("PROGRAMA: {0} casos, todo OK" -f $script:total) -ForegroundColor Green
    exit 0
}
Write-Host ("PROGRAMA: {0} casos, {1} fallos" -f $script:total, $script:fallos) -ForegroundColor Red
exit 1
