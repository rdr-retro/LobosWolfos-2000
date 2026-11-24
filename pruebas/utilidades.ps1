# utilidades.ps1 - suite de la fase 8: utilidades y recursos conectados.
#
# Congela el panel UTILIDADES del arbol de Inicio, el registro central de
# recursos, el arrastre de un recurso a un nodo y el aviso antes de borrar una
# pieza con referencias entrantes. Los cuatro casos trabajan sobre el mismo
# .log de ejemplo y no cambian ningun formato de archivo.
#
# Casos:
#   utilidades_panel      botones PINTAR, TILES y MAPAS al pie del arbol
#   registro_recursos     guardar/registrar reutiliza la ruta y no duplica
#   arrastrar_recurso     soltar un recurso sobre un nodo escribe la referencia
#   borrar_con_referencias aviso en el primer BORRAR y borrado en el segundo
#
# Uso:
#   pruebas\utilidades.ps1                       pasa la suite
#   pruebas\utilidades.ps1 -Filtro registro*     solo los casos que encajan
#   pruebas\utilidades.ps1 -Actualizar           reescribe los patrones

[CmdletBinding()]
param(
    [string]$Editor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$casosDir = Join-Path $PSScriptRoot "utilidades"
$esperadoDir = Join-Path $casosDir "esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Editor) { $Editor = Join-Path $raiz "LobosWolfos2000.exe" }
if (-not (Test-Path -LiteralPath $Editor)) {
    Write-Host "[ERROR] no existe el ejecutable: $Editor" -ForegroundColor Red
    exit 2
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path

$tmp = Join-Path $env:TEMP ("lw2000-utilidades-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Utilidades y recursos conectados (fase 8)" -ForegroundColor Cyan
Write-Host ("  editor: {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Ejecutar-Editor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Editor $argumentos $dir
}

function Preparar-Escena([string]$dir) {
    Copy-Item (Join-Path $raiz "pruebas\modelo\escena_scripts.log") `
              (Join-Path $dir "escena.log") -Force
}

function Comprobar-Texto([string]$nombre, [string]$ruta, [string]$extension,
                         $problemas) {
    $obtenido = Get-TextoNormalizado $ruta
    if (-not $obtenido) {
        [void]$problemas.Add("no se genero " + $nombre + $extension)
        return
    }
    $patron = Join-Path $esperadoDir ($nombre + $extension)
    if ($Actualizar) {
        Guardar-TextoNormalizado $patron $obtenido
    } elseif (-not (Test-Path -LiteralPath $patron)) {
        [void]$problemas.Add("falta " + $nombre + $extension + " (usa -Actualizar)")
    } elseif ((Get-TextoNormalizado $patron) -ne $obtenido) {
        [void]$problemas.Add("el patron de " + $nombre + $extension + " no coincide")
        Mostrar-Diferencia ((Get-TextoNormalizado $patron) -split "`n") `
                           ($obtenido -split "`n") 12
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

# Sondas del panel UTILIDADES a 1200x700: el fondo de los tres botones, la
# etiqueta gris y el borde del lienzo, que no debe quedar tapado por el panel.
function Get-PixelesUtilidades([string]$bmp) {
    $imagen = [System.Drawing.Bitmap]::FromFile($bmp)
    try {
        $puntos = @("85,655", "170,655", "260,655", "40,628", "280,650")
        $salida = @()
        foreach ($punto in $puntos) {
            $xy = $punto.Split(",")
            $color = $imagen.GetPixel([int]$xy[0], [int]$xy[1])
            $salida += ("{0}={1:X2}{2:X2}{3:X2}" -f $punto, $color.R,
                        $color.G, $color.B)
        }
        return ($salida -join " ")
    } finally {
        $imagen.Dispose()
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
        Write-Host ("  [{0,3}] {1,-24} OK" -f $script:indice, $nombre) -ForegroundColor Green
    } else {
        $script:fallos++
        Write-Host ("  [{0,3}] {1,-24} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
        foreach ($p in $problemas) {
            Write-Host ("        - {0}" -f $p) -ForegroundColor Red
        }
    }
}

Ejecutar-Caso "utilidades_panel" {
    param($dir, $problemas)
    Preparar-Escena $dir
    $bmp = Join-Path $dir "panel.bmp"
    $panel = Join-Path $dir "panel.txt"
    $codigo = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
                                     "--sin-ventanas", "--captura", $bmp,
                                     "--volcar-inicio", $panel)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Texto "utilidades_panel" $panel ".txt" $problemas
    if (-not (Test-Path -LiteralPath $bmp)) {
        [void]$problemas.Add("falta panel.bmp")
    } else {
        $pixeles = Get-PixelesUtilidades $bmp
        $patron = Join-Path $esperadoDir "utilidades_panel.pixeles"
        if ($Actualizar) {
            Guardar-TextoNormalizado $patron $pixeles
        } elseif (-not (Test-Path -LiteralPath $patron)) {
            [void]$problemas.Add("falta utilidades_panel.pixeles (usa -Actualizar)")
        } elseif ((Get-TextoNormalizado $patron) -ne $pixeles) {
            [void]$problemas.Add("los pixeles del panel no coinciden")
            [void]$problemas.Add("  esperado: " + (Get-TextoNormalizado $patron))
            [void]$problemas.Add("  obtenido: " + $pixeles)
        }
    }
    foreach ($boton in @(@("40,650", "pintar"), @("120,650", "tiles"),
                         @("200,650", "mapas"))) {
        $nombre = "utilidades_" + $boton[1]
        $volcado = Join-Path $dir ($nombre + ".txt")
        $codigo = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
                                         "--sin-ventanas", "--clics", $boton[0],
                                         "--captura", (Join-Path $dir ($nombre + ".bmp")),
                                         "--volcar-inicio", $volcado)
        if ($codigo -ne 0) {
            [void]$problemas.Add("el editor devolvio $codigo en $nombre")
        }
        Comprobar-Texto $nombre $volcado ".txt" $problemas
    }
}

Ejecutar-Caso "registro_recursos" {
    param($dir, $problemas)
    Preparar-Escena $dir
    $volcado = Join-Path $dir "registro.txt"
    $codigo = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
        "--sin-ventanas",
        "--registrar-recurso", "tiles/nuevo.png|png|Primero",
        "--registrar-recurso", "tiles/nuevo.png|png|Renombrado",
        "--captura", (Join-Path $dir "registro.bmp"),
        "--volcar-inicio", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Texto "registro_recursos" $volcado ".txt" $problemas
    Comprobar-Lineas $volcado @(
        "inicio_entidades=9 escenas=1 nodos=3 scripts=1 asociaciones=1 recursos=2",
        "inicio_registro=16 tipo=png ruta=tiles/nuevo.png nombre=Renombrado"
    ) $problemas
}

Ejecutar-Caso "arrastrar_recurso" {
    param($dir, $problemas)
    Preparar-Escena $dir
    $volcado = Join-Path $dir "arrastre.txt"
    $codigo = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
        "--sin-ventanas",
        "--registrar-recurso", "tiles/nuevo.png|png|Nuevo",
        "--inicio-arrastrar", "80,213;450,200",
        "--captura", (Join-Path $dir "arrastre.bmp"),
        "--volcar-inicio", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Texto "arrastrar_recurso" $volcado ".txt" $problemas
    Comprobar-Lineas $volcado @(
        "inicio_sel=12",
        "inicio_entidades=9 escenas=1 nodos=3 scripts=1 asociaciones=1 recursos=2",
        "inicio_nodo=12 escena=10 padre=11 nombre=Player recurso=16",
        "inicio_registro=16 tipo=png ruta=tiles/nuevo.png nombre=Nuevo"
    ) $problemas
}

Ejecutar-Caso "borrar_con_referencias" {
    param($dir, $problemas)
    Preparar-Escena $dir
    $aviso = Join-Path $dir "aviso.txt"
    $borrado = Join-Path $dir "borrado.txt"
    $codigo = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
        "--sin-ventanas", "--clics", "80,191;600,68",
        "--captura", (Join-Path $dir "aviso.bmp"),
        "--volcar-inicio", $aviso)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Texto "borrar_aviso" $aviso ".txt" $problemas
    if (-not ((Get-TextoNormalizado $aviso) -match "confirmar_borrado=1")) {
        [void]$problemas.Add("el primer BORRAR no pidio confirmacion")
    }
    $codigo = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
        "--sin-ventanas", "--clics", "80,191;600,68;600,68",
        "--captura", (Join-Path $dir "borrado.bmp"),
        "--volcar-inicio", $borrado)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Texto "borrar_borrado" $borrado ".txt" $problemas
    Comprobar-Lineas $borrado @(
        "inicio_entidades=7 escenas=1 nodos=3 scripts=1 asociaciones=1 recursos=0",
        "inicio_utilidad= arrastre=-1 confirmar_borrado=-1 raton=600,68",
        "inicio_undo=1 redo=0 ultimo=borrado reparentar=-1"
    ) $problemas
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("UTILIDADES: {0} casos, todo OK" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("UTILIDADES: {0} casos, {1} fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
