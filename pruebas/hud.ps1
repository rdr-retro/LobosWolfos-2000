# hud.ps1 - suite de la fase 38: HUD, texto, UI y dialogos.
#
# Congela la traza de los builtins, los volcados del HUD, el hash del BMP y
# cuatro pixeles que comprueban que la franja superior y el dialogo inferior
# no tapan el mundo. Ningun caso usa las rutas de las quince suites
# anteriores: las salidas nuevas solo aparecen con --hud, --vidas, --puntos,
# --dialogo y --volcar-hud.
#
# Casos:
#   builtins  los nueve builtins en --vm-test, sin callbacks
#   cli       vidas, puntos y dialogo fijados desde la linea de comandos
#   script    los mismos valores escritos desde un bloque codigo.lobo
#   elementos texto, rect, barra, imagen, boton y plantilla
#   neutro    el mismo LOGIC3 sin --hud: el HUD queda desactivado
#
# Uso:
#   pruebas\hud.ps1                          pasa la suite
#   pruebas\hud.ps1 -Filtro cli*             solo los casos que encajan
#   pruebas\hud.ps1 -Actualizar              reescribe los patrones

[CmdletBinding()]
param(
    [string]$Motor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$casosDir = Join-Path $PSScriptRoot "hud"
$esperadoDir = Join-Path $casosDir "esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
if (-not (Test-Path -LiteralPath $Motor)) {
    Write-Host "[ERROR] no existe el motor: $Motor" -ForegroundColor Red
    exit 2
}
$Motor = (Resolve-Path -LiteralPath $Motor).Path

$tmp = Join-Path $env:TEMP ("lw2000-hud-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "HUD, texto, UI y dialogos (fase 38)" -ForegroundColor Cyan
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

function Comprobar-Pixel([string]$traza, [int]$x, [int]$y,
                         [string]$esperado, $problemas) {
    $valor = ""
    foreach ($linea in ((Get-TextoNormalizado $traza) -split "`n")) {
        if ($linea -match ("^pixel " + $x + " " + $y + " ([0-9A-Fa-f]{8})$")) {
            $valor = $Matches[1].ToUpperInvariant()
        }
    }
    if (-not $valor) {
        [void]$problemas.Add("falta el pixel ${x},${y}")
    } elseif ($valor -ne $esperado.ToUpperInvariant()) {
        [void]$problemas.Add("pixel ${x},${y}: esperado $esperado / obtenido $valor")
    }
}

function Comprobar-Lineas([string]$ruta, [string[]]$lineas, $problemas) {
    $contenido = (Get-TextoNormalizado $ruta) -split "`n"
    foreach ($linea in $lineas) {
        if ($contenido -notcontains $linea) {
            [void]$problemas.Add("falta la linea: $linea")
        }
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

Ejecutar-Caso "builtins" {
    param($dir, $problemas)
    $lobo = Join-Path $casosDir "builtins.lobo"
    $traza = Join-Path $dir "builtins.traza"
    $codigo = Ejecutar-Motor $dir @("--vm-test", $lobo,
        "--traza", $traza, "--fotogramas", "4")
    if ($codigo -ne 0) { [void]$problemas.Add("builtins devolvio $codigo") }
    Comprobar-Texto "builtins" $traza ".traza" $problemas
    Comprobar-Lineas $traza @("vm: 81 instrucciones 0 funciones",
        "vm 4 estado 2 pc 81 np 0 espera 0 x 4 y 16 msg 0 clones 0 err 0") `
        $problemas
}

Ejecutar-Caso "cli" {
    param($dir, $problemas)
    $logic3 = Join-Path $PSScriptRoot "entrada\respuesta.logic3"
    $bmp = Join-Path $dir "cli.bmp"
    $traza = Join-Path $dir "cli.traza"
    $volcado = Join-Path $dir "cli.volcado"
    $argumentos = @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "4",
        "--hud", "--vidas", "3", "--puntos", "1250",
        "--dialogo", "Hola, lobo|Encuentra la llave|Sin prisa",
        "--volcar-hud", $volcado,
        "--pixel", "8,0", "--pixel", "8,152", "--pixel", "160,100",
        "--pixel", "16,159")
    $codigo = Ejecutar-Motor $dir $argumentos
    if ($codigo -ne 0) { [void]$problemas.Add("cli devolvio $codigo") }
    Comprobar-Texto "cli" $traza ".traza" $problemas
    Comprobar-Texto "cli" $volcado ".volcado" $problemas
    Comprobar-Bmp "cli" $bmp $problemas
    Comprobar-Lineas $volcado @("hud: activo 1", "hud: vidas 3",
        "hud: puntos 1250", "hud: dialogo 3", "hud: linea 0 Hola, lobo",
        "hud: linea 1 Encuentra la llave", "hud: linea 2 Sin prisa",
        "hud: elementos 0", "hud: plantilla 0") $problemas
    # Franja superior, borde del dialogo, texto acento y mundo al centro.
    Comprobar-Pixel $traza 8 0 "C010141C" $problemas
    Comprobar-Pixel $traza 8 152 "FFD8D8D8" $problemas
    Comprobar-Pixel $traza 160 100 "FFC84A4A" $problemas
    Comprobar-Pixel $traza 16 159 "FFFFC040" $problemas

    $bmp2 = Join-Path $dir "cli2.bmp"
    $traza2 = Join-Path $dir "cli2.traza"
    $volcado2 = Join-Path $dir "cli2.volcado"
    $argumentos2 = @("--logic3", $logic3, "--captura", $bmp2,
        "--traza", $traza2, "--fotogramas", "4",
        "--hud", "--vidas", "3", "--puntos", "1250",
        "--dialogo", "Hola, lobo|Encuentra la llave|Sin prisa",
        "--volcar-hud", $volcado2,
        "--pixel", "8,0", "--pixel", "8,152", "--pixel", "160,100",
        "--pixel", "16,159")
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

Ejecutar-Caso "script" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "escena.logic3"
    $bmp = Join-Path $dir "script.bmp"
    $traza = Join-Path $dir "script.traza"
    $volcado = Join-Path $dir "script.volcado"
    $argumentos = @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "4", "--hud",
        "--volcar-hud", $volcado,
        "--pixel", "8,0", "--pixel", "8,152", "--pixel", "160,100",
        "--pixel", "16,159")
    $codigo = Ejecutar-Motor $dir $argumentos
    if ($codigo -ne 0) { [void]$problemas.Add("script devolvio $codigo") }
    Comprobar-Texto "script" $traza ".traza" $problemas
    Comprobar-Texto "script" $volcado ".volcado" $problemas
    Comprobar-Bmp "script" $bmp $problemas
    Comprobar-Lineas $volcado @("hud: activo 1", "hud: vidas 3",
        "hud: puntos 1250", "hud: dialogo 3") $problemas
    Comprobar-Pixel $traza 8 0 "C010141C" $problemas
    Comprobar-Pixel $traza 8 152 "FFD8D8D8" $problemas
    Comprobar-Pixel $traza 160 100 "FFC84A4A" $problemas
    Comprobar-Pixel $traza 16 159 "FFFFC040" $problemas
}

Ejecutar-Caso "elementos" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "elementos.logic3"
    $bmp = Join-Path $dir "elementos.bmp"
    $traza = Join-Path $dir "elementos.traza"
    $volcado = Join-Path $dir "elementos.volcado"
    $argumentos = @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "4", "--hud",
        "--volcar-hud", $volcado,
        "--pixel", "8,31", "--pixel", "210,31", "--pixel", "211,51",
        "--pixel", "251,41")
    $codigo = Ejecutar-Motor $dir $argumentos
    if ($codigo -ne 0) { [void]$problemas.Add("elementos devolvio $codigo") }
    Comprobar-Texto "elementos" $traza ".traza" $problemas
    Comprobar-Texto "elementos" $volcado ".volcado" $problemas
    Comprobar-Bmp "elementos" $bmp $problemas
    Comprobar-Lineas $volcado @("hud: activo 1", "hud: elementos 5",
        "hud: elem 0 0 8 30 0 0 0 HUD",
        "hud: elem 1 1 210 30 40 14 0 ",
        "hud: elem 2 2 210 50 40 8 0 ",
        "hud: elem 3 3 260 30 0 0 0 ",
        "hud: elem 4 4 250 40 40 14 0 OK",
        "hud: plantilla 1") $problemas
    Comprobar-Pixel $traza 8 31 "00FFFFFF" $problemas
    Comprobar-Pixel $traza 210 31 "00FF0000" $problemas
    Comprobar-Pixel $traza 211 51 "0000FF00" $problemas
    Comprobar-Pixel $traza 251 41 "FF305080" $problemas
}

Ejecutar-Caso "neutro" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "escena.logic3"
    $bmp = Join-Path $dir "neutro.bmp"
    $traza = Join-Path $dir "neutro.traza"
    $volcado = Join-Path $dir "neutro.volcado"
    $argumentos = @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "4",
        "--volcar-hud", $volcado,
        "--pixel", "8,0", "--pixel", "160,100")
    $codigo = Ejecutar-Motor $dir $argumentos
    if ($codigo -ne 0) { [void]$problemas.Add("neutro devolvio $codigo") }
    Comprobar-Texto "neutro" $traza ".traza" $problemas
    Comprobar-Texto "neutro" $volcado ".volcado" $problemas
    Comprobar-Bmp "neutro" $bmp $problemas
    Comprobar-Lineas $volcado @("hud: activo 0", "hud: vidas 0",
        "hud: puntos 0", "hud: dialogo 0", "hud: elementos 0") $problemas
    Comprobar-Pixel $traza 8 0 "00112233" $problemas
    Comprobar-Pixel $traza 160 100 "FFC84A4A" $problemas
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("Fase 38: {0} casos, todos correctos" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("Fase 38: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
