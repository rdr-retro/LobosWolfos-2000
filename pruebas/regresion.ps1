# regresion.ps1 - regresion visual del editor (fase 23).
#
# Congela capturas de las vistas y menus principales y del juego de
# ejemplo. Cada caso compara:
#   - el codigo de salida del editor,
#   - el volcado de estado (--volcar),
#   - el hash SHA-256 del BMP de la captura.
#
# Los patrones viven en pruebas\regresion\esperado (solo texto con
# hashes y volcados: no se guardan imagenes en el repositorio).
#
# Uso:
#   pruebas\regresion.ps1               pasa la suite
#   pruebas\regresion.ps1 -Filtro menu* solo los casos que encajan
#   pruebas\regresion.ps1 -Actualizar   reescribe los patrones

[CmdletBinding()]
param(
    [string]$Editor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$esperadoDir = Join-Path $PSScriptRoot "regresion\esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Editor) { $Editor = Join-Path $raiz "LobosWolfos2000.exe" }
if (-not (Test-Path -LiteralPath $Editor)) {
    Write-Host "[ERROR] no existe el editor: $Editor" -ForegroundColor Red
    exit 2
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path

$tmp = Join-Path $env:TEMP ("lw2000-regresion-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

$casos = @(
    @{ Nombre = "pintar";       Args = @("--sin-ventanas") },
    @{ Nombre = "tiles";        Args = @("--tiles", "--sin-ventanas") },
    @{ Nombre = "mapa";         Args = @("--vista", "mapa", "--sin-ventanas", "--mapa-ruta", "maps\mapa.mav") },
    @{ Nombre = "menu_archivo"; Args = @("--menu", "1", "--sin-ventanas") },
    @{ Nombre = "menu_ayuda";   Args = @("--menu", "9", "--sin-ventanas") },
    @{ Nombre = "gestor";       Args = @("--gestor", "--modulos", "500,250;500,400", "--sin-ventanas") },
    @{ Nombre = "guia";         Args = @("--guia", "--sin-ventanas") }
)

Write-Host ""
Write-Host "Regresion visual del editor" -ForegroundColor Cyan
Write-Host ("  editor: {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $esperadoDir) -ForegroundColor DarkGray

$total = 0
$fallos = 0
$indice = 0

function Comprobar-Caso([string]$nombre, [string]$volcar, [string]$bmp, [string]$inicio) {
    $problemas = New-Object System.Collections.ArrayList
    $hash = Get-Sha256 $bmp
    $texto = Get-TextoNormalizado $volcar

    $rutaVolcar = Join-Path $esperadoDir ($nombre + ".volcar")
    $rutaHash = Join-Path $esperadoDir ($nombre + ".bmp.sha256")

    if ($Actualizar) {
        if ($texto) { Guardar-TextoNormalizado $rutaVolcar $texto }
        if ($hash) { Guardar-TextoNormalizado $rutaHash $hash }
    } else {
        if (-not (Test-Path -LiteralPath $rutaVolcar)) {
            [void]$problemas.Add("falta " + $nombre + ".volcar (usa -Actualizar)")
        } else {
            $esperado = Get-TextoNormalizado $rutaVolcar
            if ($esperado -ne $texto) { [void]$problemas.Add("el volcado no coincide") }
        }
        if (-not (Test-Path -LiteralPath $rutaHash)) {
            [void]$problemas.Add("falta " + $nombre + ".bmp.sha256 (usa -Actualizar)")
        } else {
            $esperadoHash = (Get-TextoNormalizado $rutaHash).Trim().ToLowerInvariant()
            if ($esperadoHash -ne $hash) { [void]$problemas.Add("el BMP no coincide") }
        }
    }

    if ($problemas.Count -eq 0) {
        Write-Host ("  [{0,3}] {1,-14} OK" -f $inicio, $nombre) -ForegroundColor Green
        return 0
    }
    Write-Host ("  [{0,3}] {1,-14} FALLA" -f $inicio, $nombre) -ForegroundColor Red
    foreach ($p in $problemas) { Write-Host ("        - {0}" -f $p) -ForegroundColor Red }
    if ((Test-Path -LiteralPath $volcar) -and (Test-Path -LiteralPath $rutaVolcar)) {
        $esperado = Get-TextoNormalizado $rutaVolcar
        $obtenido = Get-TextoNormalizado $volcar
        if ($esperado -ne $obtenido) {
            Mostrar-Diferencia ($esperado -split "`n") ($obtenido -split "`n") 4
        }
    }
    return 1
}

foreach ($caso in $casos) {
    if ($Filtro -and ($caso.Nombre -notlike $Filtro)) { continue }
    $indice++
    $total++
    $volcar = Join-Path $tmp ($caso.Nombre + ".txt")
    $bmp = Join-Path $tmp ($caso.Nombre + ".bmp")
    $argumentos = @($caso.Args) + @("--captura", $bmp, "--volcar", $volcar)
    $codigo = Ejecutar-Programa $Editor $argumentos $raiz
    if ($codigo -ne 0) {
        Write-Host ("  [{0,3}] {1,-14} FALLA" -f $indice, $caso.Nombre) -ForegroundColor Red
        Write-Host ("        - el editor devolvio el codigo {0}" -f $codigo) -ForegroundColor Red
        $fallos++
        continue
    }
    $fallos += Comprobar-Caso $caso.Nombre $volcar $bmp $indice
}

# Caso especial: el juego de ejemplo escribe archivos. Se ejecuta en una
# carpeta temporal para no ensuciar tiles, maps y logic del repositorio.
$nombre = "ejemplo"
if (-not $Filtro -or ($nombre -like $Filtro)) {
    $indice++
    $total++
    $sandbox = Join-Path $tmp "sandbox-ejemplo"
    if (Test-Path -LiteralPath $sandbox) { Remove-Item -LiteralPath $sandbox -Recurse -Force }
    New-Item -ItemType Directory -Force $sandbox | Out-Null
    # Las carpetas del proyecto viven junto al ejecutable: se copia el
    # editor a la carpeta temporal para no tocar tiles, maps y logic.
    $editorSandbox = Join-Path $sandbox "LobosWolfos2000.exe"
    Copy-Item -LiteralPath $Editor -Destination $editorSandbox -Force
    $volcar = Join-Path $tmp ($nombre + ".txt")
    $bmp = Join-Path $tmp ($nombre + ".bmp")
    $argumentos = @("--ejemplo", "--sin-ventanas", "--captura", $bmp, "--volcar", $volcar)
    $codigo = Ejecutar-Programa $editorSandbox $argumentos $sandbox

    $problemas = New-Object System.Collections.ArrayList
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio el codigo $codigo") }
    $ficheros = @(
        "tiles\ejemplo_arbol.png",
        "tiles\ejemplo_agua.png",
        "tiles\ejemplo_roca.png",
        "maps\primer_juego.mav",
        "logic\primer_juego.logic"
    )
    foreach ($f in $ficheros) {
        if (-not (Test-Path -LiteralPath (Join-Path $sandbox $f))) {
            [void]$problemas.Add("no se creo " + $f)
        }
    }
    $texto = (Get-TextoNormalizado $volcar).Replace($sandbox, "<carpeta>")
    $rutaVolcar = Join-Path $esperadoDir ($nombre + ".volcar")
    $rutaHash = Join-Path $esperadoDir ($nombre + ".bmp.sha256")
    $hash = Get-Sha256 $bmp
    if ($Actualizar) {
        if ($texto) { Guardar-TextoNormalizado $rutaVolcar $texto }
        if ($hash) { Guardar-TextoNormalizado $rutaHash $hash }
    } elseif (Test-Path -LiteralPath $rutaVolcar) {
        if ((Get-TextoNormalizado $rutaVolcar) -ne $texto) {
            [void]$problemas.Add("el volcado no coincide")
        }
        if (Test-Path -LiteralPath $rutaHash) {
            if ((Get-TextoNormalizado $rutaHash).Trim().ToLowerInvariant() -ne $hash) {
                [void]$problemas.Add("el BMP no coincide")
            }
        }
    } else {
        [void]$problemas.Add("falta " + $nombre + ".volcar (usa -Actualizar)")
    }

    if ($problemas.Count -eq 0) {
        Write-Host ("  [{0,3}] {1,-14} OK" -f $indice, $nombre) -ForegroundColor Green
    } else {
        $fallos++
        Write-Host ("  [{0,3}] {1,-14} FALLA" -f $indice, $nombre) -ForegroundColor Red
        foreach ($p in $problemas) { Write-Host ("        - {0}" -f $p) -ForegroundColor Red }
    }
}

Write-Host ""
if ($fallos -eq 0) {
    Write-Host ("Regresion: {0} casos, todo OK" -f $total) -ForegroundColor Green
    exit 0
}
Write-Host ("Regresion: {0} casos, {1} con fallos" -f $total, $fallos) -ForegroundColor Red
exit 1
