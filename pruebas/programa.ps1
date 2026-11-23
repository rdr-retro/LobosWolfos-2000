# programa.ps1 - suite del modelo y el constructor de programa (fases 46-47).
#
# Congela la separacion entre escenario y jugador. Un proyecto nuevo guarda
# la pieza sistema.opciones con jugador_implicito=0; un LOGIC3 sin esa pieza
# es legacy y mantiene el lobito clasico. La fase 47 anade la vista visible:
# paleta, tarjetas, enlaces e inspector dentro del gestor.
#
# Casos:
#   modelo_minimo   vuelca el ProjectModel con los siete tipos y dos enlaces
#   modelo_desde_log lee el .log de ejemplo y vuelca el grafo convertido
#   modelo_recursos valida el PNG del actor y su conexion
#   modelo_gestor   el .log se ve igual en el lienzo y en el inspector
#   opciones_nuevo  el editor crea sistema.opciones en un proyecto nuevo
#   mapa_solo       mapa sin opciones de jugador: lobo quieto y sin sprite
#   legacy          sin sistema.opciones: lobo se mueve y el BMP cambia
#   ui_programa     lienzo Programa con tres piezas, un enlace y sondas BMP
#   entrada_gestor  --gestor abre el constructor sin piezas y con paleta visible
#   elementos_logic2 al abrir un .logic clasico, el inspector lista sus elementos
#   menu_tras_piezas recargar un LOGIC2 por la ruta del menu tras un LOGIC3 con piezas
#   arrastre_gestor --gestor permite arrastrar una pieza sin --programa
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
Add-Type -AssemblyName System.Drawing

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

# Sondas del constructor cuando el .logic clasico se proyecta a piezas:
# titulo INSPECTOR, panel derecho, las dos piezas reales del lienzo (borde,
# color, aviso de enlace y tipo) y el hueco libre junto a ellas.
function Get-PixelesElementosLogic2([string]$bmp) {
    $imagen = [System.Drawing.Bitmap]::FromFile($bmp)
    try {
        $puntos = @("921,68", "930,94", "190,92", "200,110", "310,115",
                    "250,150", "340,92", "350,110", "460,115", "400,150",
                    "500,110")
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

# Sondas del gestor con un ProjectModel activo: titulo del inspector, panel
# derecho, resumen del grafo y una tarjeta por cada entidad no conexion.
function Get-PixelesModeloGestor([string]$bmp) {
    $imagen = [System.Drawing.Bitmap]::FromFile($bmp)
    try {
        $puntos = @("921,68", "930,94", "500,202", "336,150",
                    "536,150", "536,260", "536,370", "716,150")
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

function Get-PosicionLobo([string]$ruta) {
    if (-not (Test-Path -LiteralPath $ruta)) { return "" }
    $texto = [IO.File]::ReadAllText($ruta)
    $m = [regex]::Match($texto, "(?m)^lobo\s+(-?\d+)\s+(-?\d+)\s*$")
    if (-not $m.Success) { return "" }
    return ($m.Groups[1].Value + " " + $m.Groups[2].Value)
}

# Sondas fijas del lienzo Programa a 1200x700: paleta, tarjetas, lienzo,
# puertos de entrada/salida, seleccion, enlace, conmutador de la barra y
# titulo visible del inspector en el panel derecho.
function Get-PixelesPrograma([string]$bmp) {
    $imagen = [System.Drawing.Bitmap]::FromFile($bmp)
    try {
        $puntos = @("140,118", "140,214", "140,310", "600,240", "500,410",
                    "540,300", "586,226", "483,244", "706,247", "496,260",
                    "584,260", "716,260", "660,300", "180,682", "921,68")
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
        Write-Host ("  [{0,3}] {1,-15} OK" -f $script:indice, $nombre) -ForegroundColor Green
    } else {
        $script:fallos++
        Write-Host ("  [{0,3}] {1,-15} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
        foreach ($p in $problemas) {
            Write-Host ("        - {0}" -f $p) -ForegroundColor Red
        }
    }
}

Ejecutar-Caso "modelo_minimo" {
    param($dir, $problemas)
    $volcado = Join-Path $dir "modelo.txt"
    $bmp = Join-Path $dir "modelo.bmp"
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $bmp,
                                     "--volcar-modelo", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    if (-not (Test-Path -LiteralPath $volcado)) {
        [void]$problemas.Add("falta el volcado del ProjectModel")
        return
    }
    $texto = Get-TextoNormalizado $volcado
    Comprobar-Texto "modelo_minimo" ".volcar" $texto $problemas
    foreach ($linea in @(
        "proyecto_modelo entidades=9 conexiones=2 validacion=0 diagnostico=sin_errores",
        "entidad=1:proyecto:Primer_juego:origen=ejemplos/juego_ia.log:2:",
        "entidad=10:recurso:Jugador:origen=ejemplos/juego_ia.log:5:",
        "entidad=11:recurso:Pradera:origen=ejemplos/juego_ia.log:6:",
        "entidad=20:escena:Nivel_1:origen=ejemplos/juego_ia.log:8:",
        "entidad=21:actor:Jugador:origen=ejemplos/juego_ia.log:9:",
        "entidad=22:instancia:Jugador_en_Nivel_1:origen=ejemplos/juego_ia.log:10:",
        "entidad=23:control:Jugador_1:origen=ejemplos/juego_ia.log:11:",
        "entidad=24:conexion:conexion_24:origen=ejemplos/juego_ia.log:12:",
        "entidad=25:conexion:conexion_25:origen=ejemplos/juego_ia.log:13:",
        "referencia=21:recurso:10:tiles/jugador.png:9",
        "referencia=20:mapa:11:maps/pradera.mav:8",
        "conexion=24:10:salida:21:textura:12",
        "conexion=25:23:salida:22:movimiento:13")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta en el volcado: " + $linea)
        }
    }
}

Ejecutar-Caso "modelo_desde_log" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "entrada.log"
    $volcado = Join-Path $dir "convertido.txt"
    $diag = Join-Path $dir "diagnostico.txt"
    $bmp = Join-Path $dir "convertido.bmp"
    Copy-Item (Join-Path $raiz "pruebas\modelo\juego_ia.log") $entrada
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $bmp,
                                     "--leer-modelo",
                                     "entrada.log|convertido.txt|diagnostico.txt")
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    if (-not (Test-Path -LiteralPath $volcado)) {
        [void]$problemas.Add("falta el volcado del .log convertido")
        return
    }
    $texto = Get-TextoNormalizado $volcado
    Comprobar-Texto "modelo_desde_log" ".volcar" $texto $problemas
    Comprobar-Texto "modelo_desde_log" ".diag" `
                    (Get-TextoNormalizado $diag) $problemas
    foreach ($linea in @(
        "proyecto_modelo entidades=9 conexiones=3 validacion=0 diagnostico=sin_errores",
        "entidad=1:escena:Escenario:origen=entrada.log:7:ui=160,80,0,0,0,00000000,0",
        "entidad=2:actor:Jugador:origen=entrada.log:8:ui=360,80,0,0,0,00000000,0",
        "entidad=3:control:Teclado:origen=entrada.log:9:",
        "entidad=4:accion:Mover:origen=entrada.log:10:",
        "entidad=5:recurso:Textura_pintada:origen=entrada.log:11:",
        "entidad=6:proyecto:Juego_IA:origen=entrada.log:3:",
        "propiedad=5:ruta=tiles/jugador.png:11",
        "conexion=7:2:salida:3:entrada:12",
        "conexion=8:3:salida:4:entrada:13",
        "conexion=9:5:salida:2:entrada:14")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta en el volcado: " + $linea)
        }
    }

    $roto = Join-Path $dir "roto.log"
    $salida_rota = Join-Path $dir "roto.txt"
    $diag_roto = Join-Path $dir "roto.diag"
    (Get-Content -LiteralPath $entrada) -replace 'conecta 3 -> 4', 'conecta 3 -> 99' | Set-Content -LiteralPath $roto
    [void](Ejecutar-Editor $dir @("--sin-ventanas",
                                  "--captura", (Join-Path $dir "roto.bmp"),
                                  "--leer-modelo",
                                  "roto.log|roto.txt|roto.diag"))
    if (Test-Path -LiteralPath $salida_rota) {
        [void]$problemas.Add("un .log roto no debe generar volcado")
    }
    $texto_roto = Get-TextoNormalizado $diag_roto
    if (-not $texto_roto.Contains("linea 13: conecta referencia un nodo inexistente")) {
        [void]$problemas.Add("falta el diagnostico con numero de linea")
    }
}

Ejecutar-Caso "modelo_recursos" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "entrada.log"
    $bmp = Join-Path $dir "recursos.bmp"
    $tiles = Join-Path $dir "tiles"
    $png = Join-Path $tiles "jugador.png"
    Copy-Item (Join-Path $raiz "pruebas\modelo\juego_ia.log") $entrada
    if (-not (Test-Path -LiteralPath $tiles)) {
        [void](New-Item -ItemType Directory -Path $tiles)
    }
    [void](New-Item -ItemType File -Path $png -Force)
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $bmp,
                                     "--leer-modelo",
                                     "entrada.log|modelo.txt|estructura.txt|recursos.txt")
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    $ok = Join-Path $dir "recursos.txt"
    if (-not (Test-Path -LiteralPath $ok)) {
        [void]$problemas.Add("falta el diagnostico de recursos")
        return
    }
    Comprobar-Texto "modelo_recursos" ".recursos" `
                    (Get-TextoNormalizado $ok) $problemas

    Remove-Item -LiteralPath $png
    $falta = Join-Path $dir "falta.txt"
    [void](Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $bmp,
                                  "--leer-modelo",
                                  "entrada.log|modelo.txt|estructura.txt|falta.txt"))
    $texto_falta = Get-TextoNormalizado $falta
    if (-not $texto_falta.Contains("linea 11: recurso 5 sin archivo: tiles/jugador.png")) {
        [void]$problemas.Add("no se diagnostico el PNG ausente")
    }

    [void](New-Item -ItemType File -Path $png -Force)
    $sin_enlace = Join-Path $dir "sin_enlace.log"
    (Get-Content -LiteralPath $entrada) -replace 'conecta 5 -> 2', 'conecta 5 -> 3' | Set-Content -LiteralPath $sin_enlace
    $desconectado = Join-Path $dir "desconectado.txt"
    [void](Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $bmp,
                                  "--leer-modelo",
                                  "sin_enlace.log|modelo.txt|estructura.txt|desconectado.txt"))
    $texto_desconectado = Get-TextoNormalizado $desconectado
    if (-not $texto_desconectado.Contains("linea 8: actor 2 sin recurso conectado")) {
        [void]$problemas.Add("no se diagnostico el actor sin recurso")
    }
}

Ejecutar-Caso "modelo_gestor" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "entrada.log"
    $bmp = Join-Path $dir "grafo.bmp"
    $vpr = Join-Path $dir "grafo_ui.txt"
    Copy-Item (Join-Path $raiz "pruebas\modelo\juego_ia.log") $entrada
    $codigo = Ejecutar-Editor $dir @("--gestor-modelo", "entrada.log",
                                     "--sin-ventanas", "--captura", $bmp,
                                     "--volcar-programa", $vpr)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    foreach ($archivo in @($bmp, $vpr)) {
        if (-not (Test-Path -LiteralPath $archivo)) {
            [void]$problemas.Add("falta " + (Split-Path -Leaf $archivo))
        }
    }
    if ($problemas.Count -gt 0) { return }
    $ui = Get-TextoNormalizado $vpr
    Comprobar-Texto "modelo_gestor" ".volcar-programa" $ui $problemas
    Comprobar-Hash "modelo_gestor" (Get-Sha256 $bmp) $problemas
    Comprobar-Texto "modelo_gestor" ".pixeles" `
                    (Get-PixelesModeloGestor $bmp) $problemas
    if ($ui -notlike "programa_modo=1 programa_piezas=6*") {
        [void]$problemas.Add("el .log no projecto seis piezas reales")
    }
    foreach ($pieza in @(
        "programa_pieza=1:sistema.escena:160,80:0::",
        "programa_pieza=2:sistema.actor:360,80:3:Jugador:lobo",
        "programa_pieza=3:sistema.teclado:360,190:4:1:teclado",
        "programa_pieza=4:sistema.accion.movimiento:360,300:0:derecha:mantener",
        "programa_pieza=5:sistema.recurso.imagen:540,80:2:tiles/jugador.png:tiles",
        "programa_pieza=6:sistema.escena:324,132:0:640:360")) {
        if (-not $ui.Contains($pieza)) {
            [void]$problemas.Add("falta en el volcado: " + $pieza)
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

Ejecutar-Caso "ui_programa" {
    param($dir, $problemas)
    $logic = Join-Path $dir "programa.logic3"
    $bmp = Join-Path $dir "programa.bmp"
    $dump = Join-Path $dir "programa.txt"
    $vpr = Join-Path $dir "programa_ui.txt"
    $gestos = "0:20,118;430,260|0:20,214;650,260|0:20,310;540,430|0:716,260;474,430"
    $codigo = Ejecutar-Editor $dir @("--programa", "--sin-ventanas", `
                                     "--arrastre-programa", $gestos, `
                                     "--guardar-logic3", $logic, `
                                     "--captura", $bmp, "--volcar", $dump, `
                                     "--volcar-programa", $vpr)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    if (-not (Test-Path -LiteralPath $logic)) {
        [void]$problemas.Add("no se guardo el LOGIC3 del lienzo")
        return
    }
    if (-not (Test-Path -LiteralPath $bmp)) {
        [void]$problemas.Add("falta el BMP del lienzo")
        return
    }
    if (-not (Test-Path -LiteralPath $vpr)) {
        [void]$problemas.Add("falta el volcado del constructor")
        return
    }

    $contenido = Get-TextoNormalizado $logic
    $ui = Get-TextoNormalizado $vpr
    Comprobar-Texto "ui_programa" ".logic3" $contenido $problemas
    Comprobar-Texto "ui_programa" ".volcar-programa" $ui $problemas
    Comprobar-Texto "ui_programa" ".volcar" `
                    (Get-VolcarNormalizado $dump $dir) $problemas
    Comprobar-Hash "ui_programa" (Get-Sha256 $bmp) $problemas
    Comprobar-Texto "ui_programa" ".pixeles" `
                    (Get-PixelesPrograma $bmp) $problemas

    if ($ui -notlike "programa_modo=1 programa_piezas=3 programa_sel=1*") {
        [void]$problemas.Add("el volcado del constructor no describe el lienzo")
    }
    foreach ($pieza in @("programa_pieza=1:sistema.escena:",
                         "programa_pieza=2:sistema.actor:",
                         "programa_pieza=3:sistema.teclado:")) {
        if ($ui -notlike ("*" + $pieza + "*")) {
            [void]$problemas.Add("falta " + $pieza)
        }
    }
    foreach ($campo in @('tipo "sistema.pieza"',
                         'campo "ui" "1"',
                         'campo "pieza" "sistema.escena"',
                         'campo "enlace" "3"',
                         'campo "valor2" "teclado"')) {
        if ($contenido -notlike ("*" + $campo + "*")) {
            [void]$problemas.Add("falta " + $campo + " en el LOGIC3")
        }
    }
}

Ejecutar-Caso "entrada_gestor" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "entrada.bmp"
    $dump = Join-Path $dir "entrada.txt"
    $vpr = Join-Path $dir "entrada_ui.txt"
    $codigo = Ejecutar-Editor $dir @("--gestor", "--sin-ventanas", `
                                     "--captura", $bmp, "--volcar", $dump, `
                                     "--volcar-programa", $vpr)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    foreach ($archivo in @($bmp, $dump, $vpr)) {
        if (-not (Test-Path -LiteralPath $archivo)) {
            [void]$problemas.Add("falta " + (Split-Path -Leaf $archivo))
        }
    }
    if ($problemas.Count -gt 0) { return }
    $ui = Get-TextoNormalizado $vpr
    Comprobar-Texto "entrada_gestor" ".volcar-programa" $ui $problemas
    Comprobar-Texto "entrada_gestor" ".volcar" `
                    (Get-VolcarNormalizado $dump $dir) $problemas
    Comprobar-Hash "entrada_gestor" (Get-Sha256 $bmp) $problemas
    Comprobar-Texto "entrada_gestor" ".pixeles" `
                    (Get-PixelesPrograma $bmp) $problemas
    if ($ui -notlike "programa_modo=1 programa_piezas=0*") {
        [void]$problemas.Add("el gestor no abrio el constructor")
    }
}

Ejecutar-Caso "elementos_logic2" {
    param($dir, $problemas)
    $origen = Join-Path $raiz "ejemplos\minijuego.logic"
    Copy-Item -LiteralPath $origen -Destination (Join-Path $dir "minijuego.logic")
    $bmp = Join-Path $dir "elementos.bmp"
    $dump = Join-Path $dir "elementos.txt"
    $vpr = Join-Path $dir "elementos_ui.txt"
    $codigo = Ejecutar-Editor $dir @("--abrir-logica", "minijuego.logic", `
                                      "--sin-ventanas", "--captura", $bmp, `
                                      "--volcar", $dump, `
                                      "--volcar-programa", $vpr)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    foreach ($archivo in @($bmp, $dump, $vpr)) {
        if (-not (Test-Path -LiteralPath $archivo)) {
            [void]$problemas.Add("falta " + (Split-Path -Leaf $archivo))
        }
    }
    if ($problemas.Count -gt 0) { return }
    $ui = Get-TextoNormalizado $vpr
    $logica = Get-TextoNormalizado $dump
    Comprobar-Texto "elementos_logic2" ".volcar-programa" $ui $problemas
    Comprobar-Texto "elementos_logic2" ".volcar" `
                    (Get-VolcarNormalizado $dump $dir) $problemas
    Comprobar-Hash "elementos_logic2" (Get-Sha256 $bmp) $problemas
    Comprobar-Texto "elementos_logic2" ".pixeles" `
                    (Get-PixelesElementosLogic2 $bmp) $problemas
    if ($ui -notlike "programa_modo=1 programa_piezas=2*") {
        [void]$problemas.Add("el .logic no se proyecto a dos piezas reales")
    }
    if ($ui -notlike "*programa_pieza=1:sistema.recurso.mapa:24,40:0:mapa.mav:0*") {
        [void]$problemas.Add("falta la pieza real del mapa")
    }
    if ($ui -notlike "*programa_pieza=2:sistema.funcion:174,40:0:Laberinto_simple:0*") {
        [void]$problemas.Add("falta la pieza real de la funcion")
    }
    if ($logica -notlike "*modulos=2*") {
        [void]$problemas.Add("el .logic no trae dos elementos")
    }
}

# Recarga por la misma funcion que ARCHIVO > Abrir logica .logic. Primero
# abre un LOGIC3 con tres piezas y despues el minijuego: sin reproyectar el
# constructor, el lienzo seguiria mostrando las piezas viejas.
Ejecutar-Caso "menu_tras_piezas" {
    param($dir, $problemas)
    $origen = Join-Path $raiz "ejemplos\minijuego.logic"
    $piezas = Join-Path $esperadoDir "ui_programa.logic3"
    Copy-Item -LiteralPath $origen -Destination (Join-Path $dir "minijuego.logic")
    $bmp = Join-Path $dir "recarga.bmp"
    $dump = Join-Path $dir "recarga.txt"
    $vpr = Join-Path $dir "recarga_ui.txt"
    $codigo = Ejecutar-Editor $dir @("--abrir-logic3", $piezas, `
                                      "--abrir-logica-menu", "minijuego.logic", `
                                      "--sin-ventanas", "--captura", $bmp, `
                                      "--volcar", $dump, `
                                      "--volcar-programa", $vpr)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    foreach ($archivo in @($bmp, $dump, $vpr)) {
        if (-not (Test-Path -LiteralPath $archivo)) {
            [void]$problemas.Add("falta " + (Split-Path -Leaf $archivo))
        }
    }
    if ($problemas.Count -gt 0) { return }
    $ui = Get-TextoNormalizado $vpr
    $logica = Get-TextoNormalizado $dump
    Comprobar-Texto "menu_tras_piezas" ".volcar-programa" $ui $problemas
    Comprobar-Texto "menu_tras_piezas" ".volcar" `
                    (Get-VolcarNormalizado $dump $dir) $problemas
    Comprobar-Hash "menu_tras_piezas" (Get-Sha256 $bmp) $problemas
    Comprobar-Texto "menu_tras_piezas" ".pixeles" `
                    (Get-PixelesElementosLogic2 $bmp) $problemas
    if ($ui -notlike "programa_modo=1 programa_piezas=2*") {
        [void]$problemas.Add("el menu no proyecto las piezas del LOGIC2")
    }
    if ($ui -notlike "*programa_pieza=1:sistema.recurso.mapa:24,40:0:mapa.mav:0*") {
        [void]$problemas.Add("quedo la pieza del LOGIC3 anterior")
    }
    if ($logica -notlike "*modulos=2*logic3=0*") {
        [void]$problemas.Add("no se cargo el LOGIC2 del minijuego")
    }
}

Ejecutar-Caso "arrastre_gestor" {
    param($dir, $problemas)
    $logic = Join-Path $dir "arrastre.logic3"
    $bmp = Join-Path $dir "arrastre.bmp"
    $vpr = Join-Path $dir "arrastre_ui.txt"
    $gestos = "0:20,118;430,260"
    $base = Join-Path $carpeta "legacy.logic3"
    $codigo = Ejecutar-Editor $dir @("--abrir-logic3", $base, `
                                     "--gestor", "--sin-ventanas", `
                                     "--arrastre-programa", $gestos, `
                                     "--guardar-logic3", $logic, `
                                     "--captura", $bmp, `
                                     "--volcar-programa", $vpr)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    foreach ($archivo in @($logic, $bmp, $vpr)) {
        if (-not (Test-Path -LiteralPath $archivo)) {
            [void]$problemas.Add("falta " + (Split-Path -Leaf $archivo))
        }
    }
    if ($problemas.Count -gt 0) { return }
    $contenido = Get-TextoNormalizado $logic
    $ui = Get-TextoNormalizado $vpr
    Comprobar-Texto "arrastre_gestor" ".logic3" $contenido $problemas
    Comprobar-Texto "arrastre_gestor" ".volcar-programa" $ui $problemas
    Comprobar-Hash "arrastre_gestor" (Get-Sha256 $bmp) $problemas
    Comprobar-Texto "arrastre_gestor" ".pixeles" `
                    (Get-PixelesPrograma $bmp) $problemas
    if ($ui -notlike "programa_modo=1 programa_piezas=1*") {
        [void]$problemas.Add("el arrastre no anadio una pieza")
    }
    foreach ($campo in @('tipo "sistema.opciones"',
                         'campo "programa" "1"',
                         'tipo "sistema.pieza"',
                         'campo "pieza" "sistema.escena"')) {
        if ($contenido -notlike ("*" + $campo + "*")) {
            [void]$problemas.Add("falta " + $campo + " en el LOGIC3")
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
