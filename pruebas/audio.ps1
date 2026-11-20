# audio.ps1 - suite de la fase 39: sonido y musica simple.
#
# Congela la traza de los builtins, los volcados del mezclador y el hash del
# BMP. Comprueba que sonar("moneda") se lanza una sola vez, que musica("tema")
# da vueltas y que sin --audio no se abre nada ni cambia el juego. Ningun caso
# usa rutas de las quince suites anteriores: las salidas nuevas solo aparecen
# con --audio, --audio-sonido, --volumen y --volcar-audio.
#
# Casos:
#   builtins  los cinco builtins en --vm-test, sin callbacks
#   cli       sonar una vez, musica en bucle y volumen desde la linea de comandos
#   volumen   volumen() devuelve el anterior y volumen_actual() el nuevo
#   detener   detener_sonido() deja libres todos los canales
#   neutro    el mismo LOGIC3 sin --audio: el mezclador queda apagado
#
# Uso:
#   pruebas\audio.ps1                          pasa la suite
#   pruebas\audio.ps1 -Filtro cli*             solo los casos que encajan
#   pruebas\audio.ps1 -Actualizar              reescribe los patrones

[CmdletBinding()]
param(
    [string]$Motor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$casosDir = Join-Path $PSScriptRoot "audio"
$esperadoDir = Join-Path $casosDir "esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
if (-not (Test-Path -LiteralPath $Motor)) {
    Write-Host "[ERROR] no existe el motor: $Motor" -ForegroundColor Red
    exit 2
}
$Motor = (Resolve-Path -LiteralPath $Motor).Path

$tmp = Join-Path $env:TEMP ("lw2000-audio-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Sonido y musica simple (fase 39)" -ForegroundColor Cyan
Write-Host ("  motor: {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  casos: {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Ejecutar-Motor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Motor $argumentos $dir
}

# La ruta del proyecto cambia segun la maquina: se sustituye por <casos>
# antes de congelar el volcado.
function Normalizar-Volcado([string]$ruta, [string]$destino) {
    $t = Get-TextoNormalizado $ruta
    $t = [regex]::Replace($t, "(?m)^audio: base .+$", "audio: base <casos>")
    Guardar-TextoNormalizado $destino $t
    return $t
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
    Comprobar-Lineas $traza @("vm: 25 instrucciones 0 funciones",
        "vm 4 estado 2 pc 25 np 0 espera 0 x 160 y 16 msg 0 clones 0 err 0") `
        $problemas
}

Ejecutar-Caso "cli" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "escena.logic3"
    $moneda = Join-Path $casosDir "moneda.wav"
    $tema = Join-Path $casosDir "tema.wav"
    $bmp = Join-Path $dir "cli.bmp"
    $traza = Join-Path $dir "cli.traza"
    $volcado = Join-Path $dir "cli.volcado"
    $norm = Join-Path $dir "cli.norm"
    $argumentos = @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "4",
        "--audio", "--audio-sonido", "moneda=$moneda",
        "--audio-sonido", "tema=$tema", "--volumen", "70",
        "--volcar-audio", $volcado)
    $codigo = Ejecutar-Motor $dir $argumentos
    if ($codigo -ne 0) { [void]$problemas.Add("cli devolvio $codigo") }
    $texto = Normalizar-Volcado $volcado $norm
    Comprobar-Texto "cli" $traza ".traza" $problemas
    Comprobar-Texto "cli" $norm ".volcado" $problemas
    Comprobar-Bmp "cli" $bmp $problemas
    Comprobar-Lineas $norm @("audio: activo 1", "audio: volumen 70",
        "audio: base <casos>", "audio: sonar 1", "audio: musica 1",
        "audio: vueltas 2", "audio: sonidos 2",
        "audio: sonido 0 moneda 256 22050 512",
        "audio: sonido 1 tema 512 11025 512",
        "audio: canal 7 tema 80 100 1 2") $problemas

    $bmp2 = Join-Path $dir "cli2.bmp"
    $traza2 = Join-Path $dir "cli2.traza"
    $volcado2 = Join-Path $dir "cli2.volcado"
    $norm2 = Join-Path $dir "cli2.norm"
    $argumentos2 = @("--logic3", $logic3, "--captura", $bmp2,
        "--traza", $traza2, "--fotogramas", "4",
        "--audio", "--audio-sonido", "moneda=$moneda",
        "--audio-sonido", "tema=$tema", "--volumen", "70",
        "--volcar-audio", $volcado2)
    [void](Ejecutar-Motor $dir $argumentos2)
    [void](Normalizar-Volcado $volcado2 $norm2)
    if ((Get-Sha256 $bmp2) -ne (Get-Sha256 $bmp)) {
        [void]$problemas.Add("el BMP cambia entre dos ejecuciones")
    }
    if ((Get-TextoNormalizado $traza2) -ne (Get-TextoNormalizado $traza)) {
        [void]$problemas.Add("la traza cambia entre dos ejecuciones")
    }
    if ((Get-TextoNormalizado $norm2) -ne $texto) {
        [void]$problemas.Add("el volcado cambia entre dos ejecuciones")
    }
}

Ejecutar-Caso "volumen" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "volumen.logic3"
    $moneda = Join-Path $casosDir "moneda.wav"
    $bmp = Join-Path $dir "volumen.bmp"
    $traza = Join-Path $dir "volumen.traza"
    $volcado = Join-Path $dir "volumen.volcado"
    $norm = Join-Path $dir "volumen.norm"
    $argumentos = @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "2",
        "--audio", "--audio-sonido", "moneda=$moneda", "--volumen", "70",
        "--volcar-audio", $volcado)
    $codigo = Ejecutar-Motor $dir $argumentos
    if ($codigo -ne 0) { [void]$problemas.Add("volumen devolvio $codigo") }
    $texto = Normalizar-Volcado $volcado $norm
    Comprobar-Texto "volumen" $traza ".traza" $problemas
    Comprobar-Texto "volumen" $norm ".volcado" $problemas
    Comprobar-Bmp "volumen" $bmp $problemas
    Comprobar-Lineas $norm @("audio: volumen 35", "audio: sonar 1",
        "audio: musica 0") $problemas
    # x conserva el volumen anterior (70) e y el nuevo (35).
    Comprobar-Lineas $traza @("1 70 35") $problemas
}

Ejecutar-Caso "detener" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "detener.logic3"
    $tema = Join-Path $casosDir "tema.wav"
    $bmp = Join-Path $dir "detener.bmp"
    $traza = Join-Path $dir "detener.traza"
    $volcado = Join-Path $dir "detener.volcado"
    $norm = Join-Path $dir "detener.norm"
    $argumentos = @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "4",
        "--audio", "--audio-sonido", "tema=$tema",
        "--volcar-audio", $volcado)
    $codigo = Ejecutar-Motor $dir $argumentos
    if ($codigo -ne 0) { [void]$problemas.Add("detener devolvio $codigo") }
    $texto = Normalizar-Volcado $volcado $norm
    Comprobar-Texto "detener" $traza ".traza" $problemas
    Comprobar-Texto "detener" $norm ".volcado" $problemas
    Comprobar-Bmp "detener" $bmp $problemas
    Comprobar-Lineas $norm @("audio: musica 1", "audio: vueltas 0",
        "audio: canal 7 libre") $problemas
}

Ejecutar-Caso "neutro" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "escena.logic3"
    $bmp = Join-Path $dir "neutro.bmp"
    $traza = Join-Path $dir "neutro.traza"
    $volcado = Join-Path $dir "neutro.volcado"
    $norm = Join-Path $dir "neutro.norm"
    $argumentos = @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "4",
        "--volcar-audio", $volcado)
    $codigo = Ejecutar-Motor $dir $argumentos
    if ($codigo -ne 0) { [void]$problemas.Add("neutro devolvio $codigo") }
    $texto = Normalizar-Volcado $volcado $norm
    Comprobar-Texto "neutro" $traza ".traza" $problemas
    Comprobar-Texto "neutro" $norm ".volcado" $problemas
    Comprobar-Bmp "neutro" $bmp $problemas
    Comprobar-Lineas $norm @("audio: activo 0", "audio: volumen 80",
        "audio: sonar 0", "audio: musica 0", "audio: vueltas 0",
        "audio: sonidos 0") $problemas
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("Fase 39: {0} casos, todos correctos" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("Fase 39: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
