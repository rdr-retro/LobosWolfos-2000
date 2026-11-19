# entrada.ps1 - suite de la fase 37: entrada extendida del runtime.
#
# Congela la traza de la VM, el volcado de entrada, el hash del BMP y una
# segunda ejecucion para comprobar determinismo. Ningun caso usa las rutas
# de las catorce suites anteriores: las salidas nuevas solo aparecen con
# --entrada-teclas, --entrada-raton y --volcar-entrada.
#
# Casos:
#   tecla     flanco de pulsacion y tecla mantenida desde --entrada-teclas
#   raton     boton, rueda, posicion y dentro desde --entrada-raton
#   captura   LOGIC3 con los tres estimulos, BMP y volcado congelados
#   neutro    el mismo LOGIC3 sin flags nuevas: entrada siempre a cero
#
# Uso:
#   pruebas\entrada.ps1                         pasa la suite
#   pruebas\entrada.ps1 -Filtro captura*        solo los casos que encajan
#   pruebas\entrada.ps1 -Actualizar             reescribe los patrones

[CmdletBinding()]
param(
    [string]$Motor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$casosDir = Join-Path $PSScriptRoot "entrada"
$esperadoDir = Join-Path $casosDir "esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
if (-not (Test-Path -LiteralPath $Motor)) {
    Write-Host "[ERROR] no existe el motor: $Motor" -ForegroundColor Red
    exit 2
}
$Motor = (Resolve-Path -LiteralPath $Motor).Path

$tmp = Join-Path $env:TEMP ("lw2000-entrada-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Entrada extendida (fase 37)" -ForegroundColor Cyan
Write-Host ("  motor: {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  casos: {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Ejecutar-Motor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Motor $argumentos $dir
}

function Comprobar-Texto([string]$nombre, [string]$ruta, [string]$extension,
                         $problemas) {
    $obtenido = Get-TextoNormalizado $ruta
    $patron = Join-Path $esperadoDir ($nombre + $extension)
    if ($Actualizar) {
        Guardar-TextoNormalizado $patron $obtenido
    } elseif (-not (Test-Path -LiteralPath $patron)) {
        [void]$problemas.Add("falta " + $nombre + $extension + " (usa -Actualizar)")
    } elseif ((Get-TextoNormalizado $patron) -ne $obtenido) {
        [void]$problemas.Add("el patron de " + $nombre + $extension + " no coincide")
        Mostrar-Diferencia ((Get-TextoNormalizado $patron) -split "`n") `
                           ($obtenido -split "`n") 10
    }
}

function Comprobar-Bmp([string]$nombre, [string]$bmp, $problemas) {
    if (-not (Test-Path -LiteralPath $bmp)) {
        [void]$problemas.Add("falta el BMP")
        return
    }
    $hash = Get-Sha256 $bmp
    $patron = Join-Path $esperadoDir ($nombre + ".bmp.sha256")
    if ($Actualizar) {
        Guardar-TextoNormalizado $patron $hash
    } elseif (-not (Test-Path -LiteralPath $patron)) {
        [void]$problemas.Add("falta " + $nombre + ".bmp.sha256 (usa -Actualizar)")
    } elseif (((Get-TextoNormalizado $patron).Trim().ToLowerInvariant()) -ne $hash) {
        [void]$problemas.Add("el BMP no coincide")
    }
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
        Write-Host ("  [{0,3}] {1,-10} OK" -f $script:indice, $nombre) -ForegroundColor Green
    } else {
        $script:fallos++
        Write-Host ("  [{0,3}] {1,-10} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
        foreach ($p in $problemas) {
            Write-Host ("        - {0}" -f $p) -ForegroundColor Red
        }
    }
}

Ejecutar-Caso "tecla" {
    param($dir, $problemas)
    $lobo = Join-Path $casosDir "tecla.lobo"
    $traza = Join-Path $dir "tecla.traza"
    $volcado = Join-Path $dir "tecla.entrada"
    $codigo = Ejecutar-Motor $dir @("--vm-test", $lobo,
        "--traza", $traza, "--fotogramas", "4",
        "--entrada-teclas", "a", "--volcar-entrada", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("tecla devolvio $codigo") }
    Comprobar-Texto "tecla" $traza ".traza" $problemas
    Comprobar-Texto "tecla" $volcado ".entrada" $problemas
}

Ejecutar-Caso "raton" {
    param($dir, $problemas)
    $lobo = Join-Path $casosDir "raton.lobo"
    $traza = Join-Path $dir "raton.traza"
    $volcado = Join-Path $dir "raton.entrada"
    $codigo = Ejecutar-Motor $dir @("--vm-test", $lobo,
        "--traza", $traza, "--fotogramas", "4",
        "--entrada-raton", "40,30,1,2,1",
        "--volcar-entrada", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("raton devolvio $codigo") }
    Comprobar-Texto "raton" $traza ".traza" $problemas
    Comprobar-Texto "raton" $volcado ".entrada" $problemas
}

Ejecutar-Caso "captura" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "respuesta.logic3"
    $bmp = Join-Path $dir "captura.bmp"
    $traza = Join-Path $dir "captura.traza"
    $volcado = Join-Path $dir "captura.entrada"
    $argumentos = @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "4",
        "--entrada-teclas", "a", "--entrada-raton", "40,30,1,2,1",
        "--volcar-entrada", $volcado)
    $codigo = Ejecutar-Motor $dir $argumentos
    if ($codigo -ne 0) { [void]$problemas.Add("captura devolvio $codigo") }
    Comprobar-Texto "captura" $traza ".traza" $problemas
    Comprobar-Texto "captura" $volcado ".entrada" $problemas
    Comprobar-Bmp "captura" $bmp $problemas

    $bmp2 = Join-Path $dir "captura2.bmp"
    $traza2 = Join-Path $dir "captura2.traza"
    $volcado2 = Join-Path $dir "captura2.entrada"
    $argumentos2 = @("--logic3", $logic3, "--captura", $bmp2,
        "--traza", $traza2, "--fotogramas", "4",
        "--entrada-teclas", "a", "--entrada-raton", "40,30,1,2,1",
        "--volcar-entrada", $volcado2)
    [void](Ejecutar-Motor $dir $argumentos2)
    if ((Get-Sha256 $bmp2) -ne (Get-Sha256 $bmp)) {
        [void]$problemas.Add("el BMP cambia entre dos ejecuciones")
    }
    if ((Get-TextoNormalizado $traza2) -ne (Get-TextoNormalizado $traza)) {
        [void]$problemas.Add("la traza cambia entre dos ejecuciones")
    }
    if ((Get-TextoNormalizado $volcado2) -ne (Get-TextoNormalizado $volcado)) {
        [void]$problemas.Add("el volcado cambia entre dos ejecuciones")
    }
}

Ejecutar-Caso "neutro" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "respuesta.logic3"
    $bmp = Join-Path $dir "neutro.bmp"
    $traza = Join-Path $dir "neutro.traza"
    $codigo = Ejecutar-Motor $dir @("--logic3", $logic3,
        "--captura", $bmp, "--traza", $traza, "--fotogramas", "4")
    if ($codigo -ne 0) { [void]$problemas.Add("neutro devolvio $codigo") }
    Comprobar-Texto "neutro" $traza ".traza" $problemas
    Comprobar-Bmp "neutro" $bmp $problemas
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("Fase 37: {0} casos, todos correctos" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("Fase 37: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
