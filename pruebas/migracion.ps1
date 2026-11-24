# migracion.ps1 - suite de la fase 9: importacion del gestor visual antiguo.
#
# Congela la copia byte a byte, el LOG 2 jerarquico generado, el informe de
# conversiones, la vista Inicio abierta y los scripts lobo producidos. Los
# scripts se compilan con el motor para detectar codigo no portable.
#
# Casos:
#   montaje_piezas     .log con seccion montaje y piezas sistema.*
#   bloques_funciones  LOGIC3 con sistema.opciones/control/accion y funcion
#   log2_jerarquico    LOG 2 ya jerarquico, copia por defecto y sin duplicados
#
# Uso:
#   pruebas\migracion.ps1                       pasa la suite
#   pruebas\migracion.ps1 -Filtro bloques*      solo los casos que encajan
#   pruebas\migracion.ps1 -Actualizar           reescribe los patrones

[CmdletBinding()]
param(
    [string]$Editor = "",
    [string]$Motor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$casosDir = Join-Path $PSScriptRoot "migracion"
$esperadoDir = Join-Path $casosDir "esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Editor) { $Editor = Join-Path $raiz "LobosWolfos2000.exe" }
if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
foreach ($ruta in @($Editor, $Motor)) {
    if (-not (Test-Path -LiteralPath $ruta)) {
        Write-Host "[ERROR] no existe el ejecutable: $ruta" -ForegroundColor Red
        exit 2
    }
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path
$Motor = (Resolve-Path -LiteralPath $Motor).Path

$tmp = Join-Path $env:TEMP ("lw2000-migracion-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Migracion del gestor visual (fase 9)" -ForegroundColor Cyan
Write-Host ("  editor: {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  motor:  {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Ejecutar-Editor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Editor $argumentos $dir
}

function Ejecutar-Motor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Motor $argumentos $dir
}

function Preparar-Entrada([string]$dir, [string]$fixture, [string]$nombre) {
    Copy-Item (Join-Path $casosDir $fixture) (Join-Path $dir $nombre) -Force
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

function Comprobar-Copia([string]$entrada, [string]$copia, $problemas) {
    if (-not (Test-Path -LiteralPath $copia)) {
        [void]$problemas.Add("no se creo la copia de seguridad")
        return
    }
    if ((Get-Sha256 $entrada) -ne (Get-Sha256 $copia)) {
        [void]$problemas.Add("la copia no es byte a byte igual al original")
    }
}

# Sondas del arbol de Inicio a 1200x700: el fondo de los botones UTILIDADES,
# la etiqueta gris y el borde del lienzo, que no debe quedar tapado.
function Get-PixelesInicio([string]$bmp) {
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

function Comprobar-Script([string]$dir, [string]$relativa, [string]$nombre,
                          $problemas) {
    $ruta = Join-Path $dir $relativa
    if (-not (Test-Path -LiteralPath $ruta)) {
        [void]$problemas.Add("no se genero " + $relativa)
        return
    }
    Comprobar-Texto $nombre $ruta ".lobo" $problemas
    $traza = Join-Path $dir ($nombre + ".traza")
    $codigo = Ejecutar-Motor $dir @("--vm-test", $ruta, "--traza", $traza,
                                    "--fotogramas", "8")
    if ($codigo -ne 0) {
        [void]$problemas.Add("el script no compila: el motor devolvio $codigo")
        return
    }
    if (-not (Test-Path -LiteralPath $traza)) {
        [void]$problemas.Add("el script no genero traza")
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
        Write-Host ("  [{0,3}] {1,-22} OK" -f $script:indice, $nombre) -ForegroundColor Green
    } else {
        $script:fallos++
        Write-Host ("  [{0,3}] {1,-22} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
        foreach ($p in $problemas) {
            Write-Host ("        - {0}" -f $p) -ForegroundColor Red
        }
    }
}

Ejecutar-Caso "montaje_piezas" {
    param($dir, $problemas)
    Preparar-Entrada $dir "montaje.log" "entrada.log"
    $bmp = Join-Path $dir "captura.bmp"
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--migrar-visual",
        "entrada.log|salida.log|copia.bak|informe.txt",
        "--captura", $bmp, "--volcar-inicio", (Join-Path $dir "inicio.txt"))
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Copia (Join-Path $dir "entrada.log") `
                    (Join-Path $dir "copia.bak") $problemas
    Comprobar-Texto "montaje_salida" (Join-Path $dir "salida.log") `
                    ".log" $problemas
    Comprobar-Texto "montaje_informe" (Join-Path $dir "informe.txt") `
                    ".txt" $problemas
    Comprobar-Texto "montaje_inicio" (Join-Path $dir "inicio.txt") `
                    ".txt" $problemas
    Comprobar-Lineas (Join-Path $dir "informe.txt") @(
        "origen=LOGIC3_bloques_sistema",
        "resumen escenas=1 nodos=3 recursos=1 scripts=1 asociaciones=1 avisos=0"
    ) $problemas
    Comprobar-Lineas (Join-Path $dir "inicio.txt") @(
        "inicio_vista=1",
        "inicio_ruta=salida.log",
        "inicio_entidades=8 escenas=1 nodos=3 scripts=1 asociaciones=1 recursos=1",
        "inicio_nodo=5 escena=2 padre=3 nombre=Jugador recurso=4",
        "inicio_nodo=6 escena=2 padre=5 nombre=Teclado recurso=0"
    ) $problemas
    Comprobar-Script $dir "Scripts\accion_5.lobo" "montaje_script" $problemas
    if (-not (Test-Path -LiteralPath $bmp)) {
        [void]$problemas.Add("falta captura.bmp")
    } else {
        $pixeles = Get-PixelesInicio $bmp
        $patron = Join-Path $esperadoDir "montaje_panel.pixeles"
        if ($Actualizar) {
            Guardar-TextoNormalizado $patron $pixeles
        } elseif (-not (Test-Path -LiteralPath $patron)) {
            [void]$problemas.Add("falta montaje_panel.pixeles (usa -Actualizar)")
        } elseif ((Get-TextoNormalizado $patron) -ne $pixeles) {
            [void]$problemas.Add("los pixeles del panel no coinciden")
            [void]$problemas.Add("  esperado: " + (Get-TextoNormalizado $patron))
            [void]$problemas.Add("  obtenido: " + $pixeles)
        }
    }
}

Ejecutar-Caso "bloques_funciones" {
    param($dir, $problemas)
    Preparar-Entrada $dir "sistema.logic3" "entrada.logic3"
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--migrar-visual",
        "entrada.logic3|salida.log|copia.bak|informe.txt",
        "--captura", (Join-Path $dir "captura.bmp"),
        "--volcar-inicio", (Join-Path $dir "inicio.txt"))
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Copia (Join-Path $dir "entrada.logic3") `
                    (Join-Path $dir "copia.bak") $problemas
    Comprobar-Texto "bloques_salida" (Join-Path $dir "salida.log") `
                    ".log" $problemas
    Comprobar-Texto "bloques_informe" (Join-Path $dir "informe.txt") `
                    ".txt" $problemas
    Comprobar-Texto "bloques_inicio" (Join-Path $dir "inicio.txt") `
                    ".txt" $problemas
    Comprobar-Lineas (Join-Path $dir "informe.txt") @(
        "aviso: funcion saltar: cuerpo original no migrado",
        "origen=LOGIC3_bloques_sistema",
        "convertida accion=21 nombre=Accion_21 script=7 nodo=6 evento=tecla_pulsar",
        "resumen escenas=1 nodos=3 recursos=1 scripts=1 asociaciones=1 avisos=1"
    ) $problemas
    Comprobar-Lineas (Join-Path $dir "inicio.txt") @(
        "inicio_vista=1",
        "inicio_ruta=salida.log",
        "inicio_nodo=6 escena=2 padre=5 nombre=Control_1 recurso=0"
    ) $problemas
    Comprobar-Script $dir "Scripts\accion_21.lobo" "bloques_script" $problemas
}

Ejecutar-Caso "log2_jerarquico" {
    param($dir, $problemas)
    Preparar-Entrada $dir "jerarquico.log" "entrada.log"
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--migrar-visual",
        "entrada.log|salida.log||informe.txt",
        "--captura", (Join-Path $dir "captura.bmp"),
        "--volcar-inicio", (Join-Path $dir "inicio.txt"))
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Copia (Join-Path $dir "entrada.log") `
                    (Join-Path $dir "entrada.log.bak") $problemas
    Comprobar-Texto "jerarquico_salida" (Join-Path $dir "salida.log") `
                    ".log" $problemas
    Comprobar-Texto "jerarquico_informe" (Join-Path $dir "informe.txt") `
                    ".txt" $problemas
    Comprobar-Texto "jerarquico_inicio" (Join-Path $dir "inicio.txt") `
                    ".txt" $problemas
    Comprobar-Lineas (Join-Path $dir "informe.txt") @(
        "migracion entrada=entrada.log salida=salida.log copia=entrada.log.bak",
        "origen=LOG2_jerarquico conversion=ya_jerarquico",
        "resumen escenas=1 nodos=3 scripts=1 asociaciones=1 avisos=0"
    ) $problemas
    $informe = Get-TextoNormalizado (Join-Path $dir "informe.txt")
    if ($informe -match "(?m)^convertida escena=") {
        [void]$problemas.Add("la entrada jerarquica se convirtio dos veces")
    }
    Comprobar-Lineas (Join-Path $dir "inicio.txt") @(
        "inicio_vista=1",
        "inicio_ruta=salida.log",
        "inicio_entidades=8 escenas=1 nodos=3 scripts=1 asociaciones=1 recursos=1",
        "inicio_nodo=12 escena=10 padre=11 nombre=Player recurso=1"
    ) $problemas
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("MIGRACION: {0} casos, todo OK" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("MIGRACION: {0} casos, {1} fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
