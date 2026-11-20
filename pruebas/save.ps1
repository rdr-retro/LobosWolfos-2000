# save.ps1 - suite de la fase 40: guardado, records y persistencia.
#
# Congela la traza de los builtins, el archivo .sav, los volcados y el hash
# del BMP. Comprueba que un proyecto guarda vidas, puntos, records y ajustes,
# que cerrar y abrir conserva el progreso, que un save corrupto se recupera
# del temporal y que sin --saves no se toca ningun archivo. Las salidas
# nuevas solo aparecen con --saves y --volcar-saves.
#
# Casos:
#   builtins      los siete builtins en --vm-test, sin callbacks
#   guardar       vidas, puntos, record y ajuste en un .sav
#   persistencia  dos ejecuciones del mismo proyecto conservan el progreso
#   recuperacion  un .sav corrupto se recupera desde el .sav.tmp
#   borrar        borrar_save deja el archivo solo con la cabecera
#   herramienta   el editor lista y borra saves con --saves-*
#   neutro        sin --saves no se crea ningun archivo
#
# Uso:
#   pruebas\save.ps1                           pasa la suite
#   pruebas\save.ps1 -Filtro persis*           solo los casos que encajan
#   pruebas\save.ps1 -Actualizar               reescribe los patrones

[CmdletBinding()]
param(
    [string]$Motor = "",
    [string]$Editor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$casosDir = Join-Path $PSScriptRoot "save"
$esperadoDir = Join-Path $casosDir "esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
if (-not (Test-Path -LiteralPath $Motor)) {
    Write-Host "[ERROR] no existe el motor: $Motor" -ForegroundColor Red
    exit 2
}
$Motor = (Resolve-Path -LiteralPath $Motor).Path

if (-not $Editor) { $Editor = Join-Path $raiz "LobosWolfos2000.exe" }
if (-not (Test-Path -LiteralPath $Editor)) {
    Write-Host "[ERROR] no existe el editor: $Editor" -ForegroundColor Red
    exit 2
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path

$tmp = Join-Path $env:TEMP ("lw2000-save-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Guardado, records y persistencia (fase 40)" -ForegroundColor Cyan
Write-Host ("  motor:  {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  editor: {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Ejecutar-Motor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Motor $argumentos $dir
}

function Ejecutar-Editor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Editor $argumentos $dir
}

# La ruta de la carpeta de saves cambia segun la maquina: se sustituye por
# <saves> antes de congelar el volcado.
function Normalizar-Volcado([string]$ruta, [string]$destino) {
    $t = Get-TextoNormalizado $ruta
    $t = [regex]::Replace($t, "(?m)^save: ruta .+$", "save: ruta <saves>")
    Guardar-TextoNormalizado $destino $t
    return $t
}

# La herramienta del editor escribe la carpeta en el volcado: se sustituye
# por <saves> para que el patron no dependa de TEMP ni del usuario.
function Normalizar-Saves([string]$ruta, [string]$destino) {
    $t = Get-TextoNormalizado $ruta
    $t = [regex]::Replace($t, "(?m)^saves: carpeta .+$", "saves: carpeta <saves>")
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
        Write-Host ("  [{0,3}] {1,-12} OK" -f $script:indice, $nombre) -ForegroundColor Green
    } else {
        $script:fallos++
        Write-Host ("  [{0,3}] {1,-12} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
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
    Comprobar-Lineas $traza @("vm: 23 instrucciones 0 funciones",
        "vm 4 estado 2 pc 23 np 0 espera 0 x 0 y 16 msg 0 clones 0 err 0") `
        $problemas
}

Ejecutar-Caso "guardar" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "guardar.logic3"
    $saves = Join-Path $dir "saves"
    New-Item -ItemType Directory -Force $saves | Out-Null
    $bmp = Join-Path $dir "guardar.bmp"
    $traza = Join-Path $dir "guardar.traza"
    $volcado = Join-Path $dir "guardar.volcado"
    $norm = Join-Path $dir "guardar.norm"
    $codigo = Ejecutar-Motor $dir @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "2", "--saves", $saves,
        "--volcar-saves", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("guardar devolvio $codigo") }
    $texto = Normalizar-Volcado $volcado $norm
    Comprobar-Texto "guardar" $traza ".traza" $problemas
    Comprobar-Texto "guardar" $norm ".volcado" $problemas
    Comprobar-Bmp "guardar" $bmp $problemas
    Comprobar-Lineas $norm @("save: activo 1", "save: nombre guardar",
        "save: ruta <saves>", "save: entradas 4",
        "save: clave ajuste.volumen 70", "save: clave puntos 120",
        "save: clave record.puntos 500", "save: clave vidas 3",
        "save: recuperado 0", "save: temporal 0") $problemas
    Comprobar-Texto "guardar" (Join-Path $saves "guardar.sav") ".sav" $problemas
}

Ejecutar-Caso "persistencia" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "cargar.logic3"
    $saves = Join-Path $dir "saves"
    New-Item -ItemType Directory -Force $saves | Out-Null
    $bmp = Join-Path $dir "persistencia.bmp"
    $traza = Join-Path $dir "persistencia.traza"
    $volcado = Join-Path $dir "persistencia.volcado"
    $norm = Join-Path $dir "persistencia.norm"
    $codigo = Ejecutar-Motor $dir @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "2", "--saves", $saves,
        "--volcar-saves", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("ida devolvio $codigo") }
    [void](Normalizar-Volcado $volcado $norm)
    Comprobar-Texto "persistencia_ida" $traza ".traza" $problemas
    Comprobar-Texto "persistencia_ida" $norm ".volcado" $problemas
    Comprobar-Lineas $traza @("1 0 12") $problemas

    $bmp2 = Join-Path $dir "persistencia2.bmp"
    $traza2 = Join-Path $dir "persistencia2.traza"
    $volcado2 = Join-Path $dir "persistencia2.volcado"
    $norm2 = Join-Path $dir "persistencia2.norm"
    $codigo2 = Ejecutar-Motor $dir @("--logic3", $logic3, "--captura", $bmp2,
        "--traza", $traza2, "--fotogramas", "2", "--saves", $saves,
        "--volcar-saves", $volcado2)
    if ($codigo2 -ne 0) { [void]$problemas.Add("vuelta devolvio $codigo2") }
    [void](Normalizar-Volcado $volcado2 $norm2)
    Comprobar-Texto "persistencia" $traza2 ".traza" $problemas
    Comprobar-Texto "persistencia" $norm2 ".volcado" $problemas
    Comprobar-Bmp "persistencia" $bmp2 $problemas
    Comprobar-Lineas $traza2 @("1 10 12") $problemas
    Comprobar-Lineas $norm2 @("save: entradas 2",
        "save: clave record.puntos 120", "save: clave vidas 2") $problemas
    Comprobar-Texto "persistencia" (Join-Path $saves "cargar.sav") ".sav" $problemas
}

Ejecutar-Caso "recuperacion" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "corrupto.logic3"
    $saves = Join-Path $dir "saves"
    New-Item -ItemType Directory -Force $saves | Out-Null
    Guardar-TextoNormalizado (Join-Path $saves "corrupto.sav") "save roto sin valor`n"
    Guardar-TextoNormalizado (Join-Path $saves "corrupto.sav.tmp") `
        "lw2ksave 1`nvidas 3`nrecord.puntos 500`n"
    $bmp = Join-Path $dir "recuperacion.bmp"
    $traza = Join-Path $dir "recuperacion.traza"
    $volcado = Join-Path $dir "recuperacion.volcado"
    $norm = Join-Path $dir "recuperacion.norm"
    $codigo = Ejecutar-Motor $dir @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "2", "--saves", $saves,
        "--volcar-saves", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("recuperacion devolvio $codigo") }
    [void](Normalizar-Volcado $volcado $norm)
    Comprobar-Texto "recuperacion" $traza ".traza" $problemas
    Comprobar-Texto "recuperacion" $norm ".volcado" $problemas
    Comprobar-Bmp "recuperacion" $bmp $problemas
    Comprobar-Lineas $traza @("1 30 500") $problemas
    Comprobar-Lineas $norm @("save: recuperado 1", "save: entradas 2",
        "save: clave record.puntos 500", "save: clave vidas 3") $problemas
}

Ejecutar-Caso "borrar" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "borrar.logic3"
    $saves = Join-Path $dir "saves"
    New-Item -ItemType Directory -Force $saves | Out-Null
    Guardar-TextoNormalizado (Join-Path $saves "borrar.sav") `
        "lw2ksave 1`nvidas 9`npuntos 99`n"
    $bmp = Join-Path $dir "borrar.bmp"
    $traza = Join-Path $dir "borrar.traza"
    $volcado = Join-Path $dir "borrar.volcado"
    $norm = Join-Path $dir "borrar.norm"
    $codigo = Ejecutar-Motor $dir @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "2", "--saves", $saves,
        "--volcar-saves", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("borrar devolvio $codigo") }
    [void](Normalizar-Volcado $volcado $norm)
    Comprobar-Texto "borrar" $traza ".traza" $problemas
    Comprobar-Texto "borrar" $norm ".volcado" $problemas
    Comprobar-Bmp "borrar" $bmp $problemas
    Comprobar-Lineas $norm @("save: entradas 0", "save: recuperado 0",
        "save: temporal 0") $problemas
    Comprobar-Texto "borrar" (Join-Path $saves "borrar.sav") ".sav" $problemas
}

Ejecutar-Caso "herramienta" {
    param($dir, $problemas)
    $saves = Join-Path $dir "saves"
    New-Item -ItemType Directory -Force $saves | Out-Null
    Guardar-TextoNormalizado (Join-Path $saves "herramienta.sav") `
        "lw2ksave 1`nvidas 3`nrecord.puntos 500`n"
    $bmp = Join-Path $dir "herramienta.bmp"
    $volcado = Join-Path $dir "herramienta.volcado"
    $norm = Join-Path $dir "herramienta.norm"
    $codigo = Ejecutar-Editor $dir @("--captura", $bmp,
        "--saves-carpeta", $saves, "--saves-volcar", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("listar devolvio $codigo") }
    [void](Normalizar-Saves $volcado $norm)
    Comprobar-Texto "herramienta" $norm ".volcado" $problemas
    Comprobar-Bmp "herramienta" $bmp $problemas
    Comprobar-Lineas $norm @("saves: carpeta <saves>",
        "saves: save herramienta.sav 2 1", "saves: archivos 1") $problemas

    $bmp2 = Join-Path $dir "herramienta2.bmp"
    $volcado2 = Join-Path $dir "herramienta2.volcado"
    $norm2 = Join-Path $dir "herramienta2.norm"
    $codigo2 = Ejecutar-Editor $dir @("--captura", $bmp2,
        "--saves-carpeta", $saves, "--saves-volcar", $volcado2,
        "--saves-borrar", "herramienta")
    if ($codigo2 -ne 0) { [void]$problemas.Add("borrar devolvio $codigo2") }
    [void](Normalizar-Saves $volcado2 $norm2)
    Comprobar-Texto "herramienta_borrar" $norm2 ".volcado" $problemas
    Comprobar-Lineas $norm2 @("saves: borrar herramienta 1",
        "saves: archivos 0") $problemas
    if (Test-Path -LiteralPath (Join-Path $saves "herramienta.sav")) {
        [void]$problemas.Add("el editor no borro el save")
    }
}

Ejecutar-Caso "neutro" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "neutro.logic3"
    $bmp = Join-Path $dir "neutro.bmp"
    $traza = Join-Path $dir "neutro.traza"
    $volcado = Join-Path $dir "neutro.volcado"
    $norm = Join-Path $dir "neutro.norm"
    $codigo = Ejecutar-Motor $dir @("--logic3", $logic3, "--captura", $bmp,
        "--traza", $traza, "--fotogramas", "2", "--volcar-saves", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("neutro devolvio $codigo") }
    [void](Normalizar-Volcado $volcado $norm)
    Comprobar-Texto "neutro" $traza ".traza" $problemas
    Comprobar-Texto "neutro" $norm ".volcado" $problemas
    Comprobar-Bmp "neutro" $bmp $problemas
    Comprobar-Lineas $norm @("save: activo 0", "save: entradas 0",
        "save: recuperado 0", "save: temporal 0") $problemas
    if (Test-Path -LiteralPath (Join-Path $dir "saves")) {
        [void]$problemas.Add("sin --saves se creo la carpeta saves")
    }
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("Fase 40: {0} casos, todos correctos" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("Fase 40: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
