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
#   runtime_actor   Actor + Teclado visual se instancia y responde al teclado
#   montaje_log     un .log textual crea nodos, enlaces y un actor jugable
#   legacy          sin sistema.opciones: lobo se mueve y el BMP cambia
#   ui_programa     lienzo Programa con tres piezas, un enlace y sondas BMP
#   entrada_gestor  --gestor abre el constructor sin piezas y con paleta visible
#   elementos_logic2 al abrir un .logic clasico, el inspector lista sus elementos
#   menu_tras_piezas recargar un LOGIC2 por la ruta del menu tras un LOGIC3 con piezas
#   arrastre_gestor --gestor permite arrastrar una pieza sin --programa
#   inicio_modelo   Inicio dibuja el arbol, el lienzo y el inspector del LOG 2
#   inicio_editar   el inspector cambia una propiedad y la guarda como LOG 2
#   inicio_comandos botones visuales crean, borran, reparentan y deshacen
#   inicio_atajos   Supr, Ins, Ctrl+Z, Ctrl+Y y Ctrl+Shift+Z desde el teclado
#   modelo_runtime  LOG 2 genera escenas, objetos y scripts ejecutables
#   scripts_crear_asociar  crea y edita un .lobo, lo asocia y produce una traza
#   scripts_editar_compilar el editor mapea errores a archivo y linea
#   scripts_navegacion  salta entre asociacion, script y objeto
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

# Sondas de la barra de comandos de Inicio a 1200x700: interior de los siete
# botones (DESH y REH deben quedar activos) y zona vacia del lienzo.
function Get-PixelesInicioComandos([string]$bmp) {
    $imagen = [System.Drawing.Bitmap]::FromFile($bmp)
    try {
        $puntos = @("454,60", "516,60", "578,60", "640,60", "702,60",
                    "764,60", "826,60", "550,200")
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

# Sondas del editor de scripts de Inicio a 1200x700: titulo del inspector,
# cuatro acciones del inspector, cabecera del editor, fondo de codigo y la
# Linea resaltada por un error de compilacion.
function Get-PixelesInicioScripts([string]$bmp) {
    $imagen = [System.Drawing.Bitmap]::FromFile($bmp)
    try {
        $puntos = @("921,68", "903,474", "1001,474", "1099,474",
                    "903,502", "400,100", "600,120", "600,160")
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

Ejecutar-Caso "modelo_logic3" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "entrada.log"
    $salida = Join-Path $dir "salida.logic3"
    $diag = Join-Path $dir "salida.diag"
    $captura = Join-Path $dir "salida.bmp"
    Copy-Item (Join-Path $raiz "pruebas\modelo\juego_ia.log") $entrada
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $captura,
                                     "--modelo-logic3",
                                     "entrada.log|salida.logic3|salida.diag")
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    foreach ($archivo in @($salida, $diag)) {
        if (-not (Test-Path -LiteralPath $archivo)) {
            [void]$problemas.Add("falta " + (Split-Path -Leaf $archivo))
        }
    }
    if (-not (Test-Path -LiteralPath $salida)) { return }
    $contenido = Get-Content -LiteralPath $salida -Raw
    if (-not $contenido.StartsWith("LOGIC3")) {
        [void]$problemas.Add("la salida no es LOGIC3")
    }
    if (-not $contenido.Contains('tipo "sistema.pieza"')) {
        [void]$problemas.Add("la salida no conserva las piezas normalizadas")
    }
    if (-not ((Get-Content -LiteralPath $diag -Raw).Trim() -eq "sin errores")) {
        [void]$problemas.Add("la conversion no termino sin errores")
    }
}

Ejecutar-Caso "modelo_runtime" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "entrada.log"
    $salida = Join-Path $dir "salida.logic3"
    $diag = Join-Path $dir "salida.diag"
    $bmp = Join-Path $dir "juego.bmp"
    $traza = Join-Path $dir "juego.traza"
    New-Item -ItemType Directory -Force -Path (Join-Path $dir "Scripts") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $dir "tiles") | Out-Null
    Copy-Item (Join-Path $raiz "pruebas\modelo\escena_scripts.log") $entrada
    Copy-Item (Join-Path $raiz "pruebas\modelo\Scripts\player.lobo") `
              (Join-Path $dir "Scripts\player.lobo")
    Copy-Item (Join-Path $raiz "tiles\jugador.png") `
              (Join-Path $dir "tiles\jugador.png")
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $bmp,
                                     "--modelo-logic3",
                                     "entrada.log|salida.logic3|salida.diag")
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    foreach ($archivo in @($salida, $diag)) {
        if (-not (Test-Path -LiteralPath $archivo)) {
            [void]$problemas.Add("falta " + (Split-Path -Leaf $archivo))
        }
    }
    if ($problemas.Count -gt 0) { return }
    $logic3 = Get-TextoNormalizado $salida
    if ($logic3 -notlike "*escena 10*Nivel 1*") {
        [void]$problemas.Add("no se genero la escena real")
    }
    foreach ($linea in @(
        'objeto 12 "Player" escena 10 x 80 y 80',
        'camara objetivo 12',
        'evento 1 fotograma',
        'bloque 1 evento 1 padre 0 orden 0 tipo "codigo.lobo"',
        'mi.x = mi.x + 1')) {
        if (-not $logic3.Contains($linea)) {
            [void]$problemas.Add("falta en LOGIC3 ejecutable: " + $linea)
        }
    }
    $motorCodigo = Ejecutar-Motor $dir @("--logic3", "salida.logic3",
                                         "--captura", $bmp, "--traza", $traza,
                                         "--fotogramas", "3", "--entidades")
    if ($motorCodigo -ne 0) { [void]$problemas.Add("el motor devolvio $motorCodigo") }
    if (-not (Test-Path -LiteralPath $traza)) {
        [void]$problemas.Add("falta la traza del runtime")
    } else {
        $runtime = Get-TextoNormalizado $traza
        foreach ($linea in @("vm: 1 scripts", "objeto 12 evento 1",
                             "ent 2 83 80", "entidades final:")) {
            if ($runtime -notlike ("*" + $linea + "*")) {
                [void]$problemas.Add("falta en la traza: " + $linea)
            }
        }
    }
}

Ejecutar-Caso "modelo_escena_inicial" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "escenas.log"
    $salida = Join-Path $dir "escenas.logic3"
    $diag = Join-Path $dir "escenas.diag"
    $captura = Join-Path $dir "escenas.bmp"
    Copy-Item (Join-Path $raiz "pruebas\modelo\escenas_inicial.log") $entrada
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $captura,
                                     "--modelo-logic3",
                                     "escenas.log|escenas.logic3|escenas.diag")
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    if (-not (Test-Path -LiteralPath $salida)) {
        [void]$problemas.Add("falta el LOGIC3 de escenas")
        return
    }
    $logic3 = Get-Content -LiteralPath $salida -Raw
    $nivel1 = $logic3.IndexOf('escena 10 "Nivel 1"')
    $nivel2 = $logic3.IndexOf('escena 20 "Nivel 2"')
    if ($nivel1 -lt 0 -or $nivel2 -lt 0 -or $nivel1 -ge $nivel2) {
        [void]$problemas.Add("la escena inicial no quedo en primera posicion")
    }
    if (-not (Test-Path -LiteralPath $diag) -or
        (Get-Content -LiteralPath $diag -Raw).Trim() -ne "sin errores") {
        [void]$problemas.Add("el diagnostico de escenas no esta limpio")
    }
}

Ejecutar-Caso "jerarquia_runtime" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "jerarquia.log"
    $salida = Join-Path $dir "jerarquia.logic3"
    $diag = Join-Path $dir "jerarquia.diag"
    $traza = Join-Path $dir "jerarquia.traza"
    New-Item -ItemType Directory -Force -Path (Join-Path $dir "Scripts") | Out-Null
    Copy-Item (Join-Path $raiz "pruebas\modelo\jerarquia_runtime.log") $entrada
    Copy-Item (Join-Path $raiz "pruebas\modelo\Scripts\parent.lobo") `
              (Join-Path $dir "Scripts\parent.lobo")
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--captura",
                                     (Join-Path $dir "jerarquia.bmp"),
                                     "--modelo-logic3",
                                     "jerarquia.log|jerarquia.logic3|jerarquia.diag")
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    if (-not (Test-Path -LiteralPath $salida)) {
        [void]$problemas.Add("falta el LOGIC3 jerarquico")
        return
    }
    $logic3 = Get-Content -LiteralPath $salida -Raw
    if (-not $logic3.Contains('objeto 21 "Hijo" escena 10 x 5 y 7') -or
        -not $logic3.Contains('padre 20')) {
        [void]$problemas.Add("no se serializo el padre del objeto")
    }
    $motorCodigo = Ejecutar-Motor $dir @("--logic3", "jerarquia.logic3",
                                         "--captura", (Join-Path $dir "jerarquia.bmp"),
                                         "--traza", $traza,
                                         "--fotogramas", "3", "--entidades")
    if ($motorCodigo -ne 0) {
        [void]$problemas.Add("el motor jerarquico devolvio $motorCodigo")
        return
    }
    if (-not (Test-Path -LiteralPath $traza)) {
        [void]$problemas.Add("falta la traza jerarquica")
        return
    }
    $runtime = Get-Content -LiteralPath $traza -Raw
    if ($runtime -notlike "*ent 1 103 50*" -or
        $runtime -notlike "*ent 2 108 57*") {
        [void]$problemas.Add("el hijo no siguio al padre: " + $runtime)
    }
    if (-not (Test-Path -LiteralPath $diag) -or
        (Get-Content -LiteralPath $diag -Raw).Trim() -ne "sin errores") {
        [void]$problemas.Add("el diagnostico jerarquico no esta limpio")
    }
}

Ejecutar-Caso "modelo_escena" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "escena.log"
    $volcado = Join-Path $dir "escena.txt"
    $diag = Join-Path $dir "escena.diag"
    $captura = Join-Path $dir "escena.bmp"
    Copy-Item (Join-Path $raiz "pruebas\modelo\escena_scripts.log") $entrada
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $captura,
                                     "--leer-modelo",
                                     "escena.log|escena.txt|escena.diag")
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    if (-not (Test-Path -LiteralPath $volcado)) {
        [void]$problemas.Add("falta el volcado de la escena")
        return
    }
    $texto = Get-TextoNormalizado $volcado
    if (-not $texto.Contains("validacion=0 diagnostico=sin_errores")) {
        [void]$problemas.Add("la escena no valida sin errores")
    }
    foreach ($linea in @(
        "entidad=1:recurso:Jugador",
        "entidad=2:script:Player",
        "entidad=10:escena:Nivel_1",
        "entidad=12:nodo:Player",
        "entidad=13:nodo:Camera2D",
        "entidad=14:asociacion:asociacion",
        "referencia=12:escena:10:-",
        "referencia=12:padre:11:-",
        "referencia=12:recurso:1:-",
        "referencia=13:padre:12:-",
        "referencia=14:script:2:-",
        "referencia=14:nodo:12:-")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta en el arbol: " + $linea)
        }
    }
    if (-not ((Get-Content -LiteralPath $diag -Raw).Trim() -eq "sin errores")) {
        [void]$problemas.Add("el diagnostico de escena no esta limpio")
    }
}

Ejecutar-Caso "inicio_modelo" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "escena.log"
    $volcado = Join-Path $dir "inicio.txt"
    $captura = Join-Path $dir "inicio.bmp"
    Copy-Item (Join-Path $raiz "pruebas\modelo\escena_scripts.log") $entrada
    $codigo = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
                                     "--sin-ventanas", "--clics", "80,136",
                                     "--captura", $captura,
                                     "--volcar-inicio", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    foreach ($archivo in @($captura, $volcado)) {
        if (-not (Test-Path -LiteralPath $archivo)) {
            [void]$problemas.Add("falta " + (Split-Path -Leaf $archivo))
        }
    }
    if ($problemas.Count -gt 0) { return }
    $texto = Get-TextoNormalizado $volcado
    foreach ($linea in @(
        "inicio_vista=1",
        "inicio_sel=12",
        "inicio_entidades=8 escenas=1 nodos=3 scripts=1 asociaciones=1",
        "inicio_seleccion=nodo #12",
        "inicio_nodo=12 escena=10 padre=11 nombre=Player",
        "inicio_nodo=13 escena=10 padre=12 nombre=Camera2D")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta en Inicio: " + $linea)
        }
    }
}

Ejecutar-Caso "inicio_editar" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "entrada.log"
    $editado = Join-Path $dir "editado.log"
    $logic3 = Join-Path $dir "editado.logic3"
    $diag = Join-Path $dir "editado.diag"
    $traza = Join-Path $dir "editado.traza"
    $captura = Join-Path $dir "editado.bmp"
    New-Item -ItemType Directory -Force -Path (Join-Path $dir "Scripts") | Out-Null
    Copy-Item (Join-Path $raiz "pruebas\modelo\escena_scripts.log") $entrada
    Copy-Item (Join-Path $raiz "pruebas\modelo\Scripts\player.lobo") `
              (Join-Path $dir "Scripts\player.lobo")
    $codigo = Ejecutar-Editor $dir @("--inicio-modelo", "entrada.log",
                                     "--clics", "80,136;920,257",
                                     "--inicio-teclas", "96\n",
                                     "--guardar-modelo", "editado.log",
                                     "--sin-ventanas", "--captura", $captura)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    if (-not (Test-Path -LiteralPath $editado)) {
        [void]$problemas.Add("no se guardo el LOG 2 editado")
        return
    }
    $texto = Get-TextoNormalizado $editado
    if (-not $texto.Contains('nodo id 12 escena 10 padre 11 tipo "actor" nombre "Player" x "96"')) {
        [void]$problemas.Add("el inspector no guardo x=96 en el nodo 12")
    }
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $captura,
                                     "--modelo-logic3",
                                     "editado.log|editado.logic3|editado.diag")
    if ($codigo -ne 0) { [void]$problemas.Add("la recarga editada devolvio $codigo") }
    if (-not (Test-Path -LiteralPath $logic3) -or
        -not (Test-Path -LiteralPath $diag)) {
        [void]$problemas.Add("no se pudo recargar el LOG 2 editado")
        return
    }
    $salida = Get-TextoNormalizado $logic3
    if ((Get-Content -LiteralPath $diag -Raw).Trim() -ne "sin errores") {
        [void]$problemas.Add("el LOG 2 editado no valida")
    }
    foreach ($linea in @(
        'objeto 12 "Player" escena 10 x 96 y 80',
        'objeto 12 "Player" escena 10 x 96 y 80 w 16 h 16 visible 1 capa 0 padre 11',
        'camara objetivo 12')) {
        if (-not $salida.Contains($linea)) {
            [void]$problemas.Add("falta tras guardar el modelo: " + $linea)
        }
    }
    $motorCodigo = Ejecutar-Motor $dir @("--logic3", "editado.logic3",
                                         "--captura", $captura,
                                         "--traza", $traza,
                                         "--fotogramas", "3", "--entidades")
    if ($motorCodigo -ne 0) { [void]$problemas.Add("el motor no cargo el LOGIC3 editado") }
    if ((Get-Content -LiteralPath $traza -Raw) -notlike "*ent 2 99 80*") {
        [void]$problemas.Add("el runtime no partio de la posicion editada")
    }
}

Ejecutar-Caso "inicio_comandos" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "escena.log"
    $volcado = Join-Path $dir "comandos.txt"
    $captura = Join-Path $dir "comandos.bmp"
    Copy-Item (Join-Path $raiz "pruebas\modelo\escena_scripts.log") $entrada

    # NODO, ESCENA, DESH, REH, seleccionar la escena nueva, BORRAR,
    # seleccionar Camera2D, PADRE, clic en lienzo vacio, DESH, REH,
    # RAIZ y DESH. Los centros son los de la barra del lienzo a 1200x700.
    $clics = "543,68;481,68;791,68;853,68;80,190;605,68;" +
             "80,168;667,68;550,200;791,68;853,68;729,68;791,68"
    $codigo = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
                                     "--sin-ventanas", "--clics", $clics,
                                     "--captura", $captura,
                                     "--volcar-inicio", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    foreach ($archivo in @($captura, $volcado)) {
        if (-not (Test-Path -LiteralPath $archivo)) {
            [void]$problemas.Add("falta " + (Split-Path -Leaf $archivo))
        }
    }
    if ($problemas.Count -gt 0) { return }

    $texto = Get-TextoNormalizado $volcado
    foreach ($linea in @(
        "inicio_sel=13",
        "inicio_undo=4 redo=1 ultimo=deshecho reparentar=-1",
        "inicio_entidades=9 escenas=1 nodos=4 scripts=1 asociaciones=1",
        "inicio_seleccion=nodo #13",
        "inicio_nodo=13 escena=10 padre=12 nombre=Camera2D",
        "inicio_nodo=16 escena=0 padre=0 nombre=Nodo 1")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta en Inicio: " + $linea)
        }
    }
    if ((Get-Content -LiteralPath $volcado -Raw) -match "padre=0 nombre=Camera2D") {
        [void]$problemas.Add("el ultimo desh no restauro el padre de Camera2D")
    }
    Comprobar-Texto "inicio_comandos" ".pixeles" `
                    (Get-PixelesInicioComandos $captura) $problemas
}

Ejecutar-Caso "inicio_atajos" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "escena.log"
    $volcado = Join-Path $dir "atajos.txt"
    $captura = Join-Path $dir "atajos.bmp"
    Copy-Item (Join-Path $raiz "pruebas\modelo\escena_scripts.log") $entrada

    # Supr borra Camera2D; las tres combinaciones de deshacer y rehacer
    # recorren el historial; Ins crea un nodo, Mayus+Ins una escena y Esc
    # deja la seleccion vacia.
    $teclas = '\d\z\y\z\Z\z\i\I\e'
    $codigo = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
                                     "--sin-ventanas", "--clics", "80,168",
                                     "--inicio-teclas", $teclas,
                                     "--captura", $captura,
                                     "--volcar-inicio", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    if (-not (Test-Path -LiteralPath $volcado)) {
        [void]$problemas.Add("falta atajos.txt")
        return
    }
    if (-not (Test-Path -LiteralPath $captura)) {
        [void]$problemas.Add("falta atajos.bmp")
    }
    $texto = Get-TextoNormalizado $volcado
    foreach ($linea in @(
        "inicio_sel=-1",
        "inicio_filas=7",
        "inicio_undo=2 redo=0 ultimo=creado reparentar=-1",
        "inicio_entidades=10 escenas=2 nodos=4 scripts=1 asociaciones=1",
        "inicio_nodo=13 escena=10 padre=12 nombre=Camera2D",
        "inicio_nodo=16 escena=0 padre=0 nombre=Nodo 1")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta en Inicio: " + $linea)
        }
    }
    if ($texto.Contains("inicio_seleccion=")) {
        [void]$problemas.Add("Escape no limpio la seleccion")
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

Ejecutar-Caso "runtime_actor" {
    param($dir, $problemas)

    $logic = Join-Path $esperadoDir "ui_programa.logic3"
    $traza = Join-Path $dir "runtime_actor.traza.txt"
    $dump = Join-Path $dir "runtime_actor.volcado.txt"
    $codigo = Ejecutar-Motor $dir @(
        "--logic3", $logic,
        "--entrada-teclas", "derecha",
        "--fotogramas", "6",
        "--traza", $traza,
        "--volcar", $dump,
        "--entidades"
    )

    if ($codigo -ne 0) {
        [void]$problemas.Add("el runtime de actor devolvio $codigo")
        return
    }
    if (-not (Test-Path -LiteralPath $traza)) {
        [void]$problemas.Add("no se genero la traza del actor")
        return
    }

    $movimientos = @(
        Get-Content -LiteralPath $traza |
            Where-Object { $_ -match '^ent 1 -?\d+ -?\d+$' }
    )
    if ($movimientos.Count -lt 6) {
        [void]$problemas.Add("no se encontro la traza del actor visual")
        return
    }

    $ultimo = $movimientos[-1].Split(' ')
    if ([int]$ultimo[2] -le 16) {
        [void]$problemas.Add("el actor no avanzo hacia la derecha: $($movimientos[-1])")
    }
}

Ejecutar-Caso "montaje_log" {
    param($dir, $problemas)

    $logic = Join-Path $raiz "ejemplos\juego_ia.log"
    $traza = Join-Path $dir "montaje_log.traza.txt"
    $bmp = Join-Path $dir "montaje_log.bmp"
    $codigo = Ejecutar-Motor $dir @(
        "--captura", $bmp,
        "--logic3", $logic,
        "--entrada-teclas", "derecha",
        "--fotogramas", "6",
        "--traza", $traza,
        "--entidades"
    )

    if ($codigo -ne 0) {
        [void]$problemas.Add("el montaje .log devolvio $codigo")
        return
    }
    if (-not (Test-Path -LiteralPath $traza)) {
        [void]$problemas.Add("no se genero la traza del montaje .log")
        return
    }
    $movimientos = @(
        Get-Content -LiteralPath $traza |
            Where-Object { $_ -match '^ent \d+ -?\d+ -?\d+$' }
    )
    if ($movimientos.Count -lt 6) {
        [void]$problemas.Add("el .log no creo la entidad del Actor")
        return
    }
    $ultimo = $movimientos[-1].Split(' ')
    if ([int]$ultimo[2] -le 16) {
        [void]$problemas.Add("el Actor del .log no avanzo: $($movimientos[-1])")
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

Ejecutar-Caso "diagnostico_json" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "entrada.log"
    $bmp = Join-Path $dir "json.bmp"
    $tiles = Join-Path $dir "tiles"
    $png = Join-Path $tiles "jugador.png"
    $salida = Join-Path $dir "salida.json"
    Copy-Item (Join-Path $raiz "pruebas\modelo\juego_ia.log") $entrada
    if (-not (Test-Path -LiteralPath $tiles)) {
        [void](New-Item -ItemType Directory -Path $tiles)
    }

    # Sin PNG: el JSON debe contener el recurso ausente con su linea y ruta.
    if (Test-Path -LiteralPath $png) { Remove-Item -LiteralPath $png }
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $bmp,
                                     "--diagnostico-json",
                                     "entrada.log|salida.json")
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    if (-not (Test-Path -LiteralPath $salida)) {
        [void]$problemas.Add("no se emitio el diagnostico JSON")
        return
    }
    Comprobar-Texto "diagnostico_json" ".json" `
                    (Get-TextoNormalizado $salida) $problemas

    # Con el PNG presente el estado cambia a ok y la lista queda vacia.
    [void](New-Item -ItemType File -Path $png -Force)
    Remove-Item -LiteralPath $salida
    [void](Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $bmp,
                                  "--diagnostico-json",
                                  "entrada.log|salida.json"))
    Comprobar-Texto "diagnostico_json_ok" ".json" `
                    (Get-TextoNormalizado $salida) $problemas
}


Ejecutar-Caso "preview_captura" {
    param($dir, $problemas)

    $entrada = Join-Path $dir "entrada.log"
    $salida = Join-Path $dir "generado.logic3"
    $ui = Join-Path $dir "ui.bmp"
    $preview = Join-Path $dir "preview.bmp"
    $motorBmp = Join-Path $dir "motor.bmp"
    Copy-Item (Join-Path $raiz "pruebas\modelo\juego_ia.log") $entrada

    # Un solo archivo para las dos vias: el generado por el ProjectModel.
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $ui,
                                     "--modelo-logic3",
                                     "entrada.log|generado.logic3")
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    if (-not (Test-Path -LiteralPath $salida)) {
        [void]$problemas.Add("no se genero el LOGIC3 compartido")
        return
    }

    # --captura-juego requiere --captura: el BMP de la UI se descarta.
    $e = Ejecutar-Editor $dir @("--sin-ventanas", "--abrir-logic3", $salida,
                                "--captura", $ui, "--captura-juego", $preview,
                                "--fotogramas", "6")
    $m = Ejecutar-Motor $dir @("--logic3", $salida, "--captura", $motorBmp,
                               "--fotogramas", "6")
    if ($e -ne 0 -or $m -ne 0) {
        [void]$problemas.Add("preview o motor devolvieron $e / $m")
        return
    }
    if (-not (Test-Path -LiteralPath $preview) -or
        -not (Test-Path -LiteralPath $motorBmp)) {
        [void]$problemas.Add("falta la captura de preview o de motor")
        return
    }
    $hPreview = Get-Sha256 $preview
    $hMotor = Get-Sha256 $motorBmp
    if ($hPreview -ne $hMotor) {
        [void]$problemas.Add(
            "preview y motor dibujan distinto: $hPreview / $hMotor")
    }
    Comprobar-Hash "preview_captura" $hPreview $problemas
}

Ejecutar-Caso "recorrido_autoria" {
    param($dir, $problemas)

    $entrada = Join-Path $dir "entrada.log"
    $volcado = Join-Path $dir "modelo.txt"
    $diagnostico = Join-Path $dir "estructura.txt"
    $json = Join-Path $dir "salida.json"
    $salida = Join-Path $dir "generado.logic3"
    $ui = Join-Path $dir "ui.bmp"
    $preview = Join-Path $dir "preview.bmp"
    $motorBmp = Join-Path $dir "motor.bmp"
    $tiles = Join-Path $dir "tiles"
    Copy-Item (Join-Path $raiz "pruebas\modelo\juego_ia.log") $entrada
    if (-not (Test-Path -LiteralPath $tiles)) {
        [void](New-Item -ItemType Directory -Path $tiles)
    }
    [void](New-Item -ItemType File -Path (Join-Path $tiles "jugador.png") -Force)

    # Un solo recorrido: el .log se lee, se valida y se guarda como LOGIC3.
    $leer = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $ui,
                                   "--leer-modelo",
                                   "entrada.log|modelo.txt|estructura.txt")
    $validar = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $ui,
                                      "--diagnostico-json",
                                      "entrada.log|salida.json")
    $guardar = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $ui,
                                      "--modelo-logic3",
                                      "entrada.log|generado.logic3")
    if ($leer -ne 0 -or $validar -ne 0 -or $guardar -ne 0) {
        [void]$problemas.Add("el recorrido devolvio $leer / $validar / $guardar")
        return
    }
    if (-not (Test-Path -LiteralPath $volcado) -or
        -not (Test-Path -LiteralPath $diagnostico) -or
        -not (Test-Path -LiteralPath $json) -or
        -not (Test-Path -LiteralPath $salida)) {
        [void]$problemas.Add("falta algun paso del recorrido")
        return
    }
    $texto = Get-TextoNormalizado $volcado
    if (-not $texto.Contains("diagnostico=sin_errores")) {
        [void]$problemas.Add("el .log no paso la validacion estructural")
    }
    Comprobar-Texto "modelo_desde_log" ".volcar" $texto $problemas
    Comprobar-Texto "diagnostico_json_ok" ".json" `
                    (Get-TextoNormalizado $json) $problemas
    Comprobar-Texto "modelo_logic3" ".logic3" `
                    (Get-TextoNormalizado $salida) $problemas

    # El mismo LOGIC3 se ejecuta en las dos vias y produce la misma captura.
    $e = Ejecutar-Editor $dir @("--sin-ventanas", "--abrir-logic3", $salida,
                                "--captura", $ui, "--captura-juego", $preview,
                                "--fotogramas", "6")
    $m = Ejecutar-Motor $dir @("--logic3", $salida, "--captura", $motorBmp,
                               "--fotogramas", "6")
    if ($e -ne 0 -or $m -ne 0) {
        [void]$problemas.Add("preview o motor devolvieron $e / $m")
        return
    }
    if (-not (Test-Path -LiteralPath $preview) -or
        -not (Test-Path -LiteralPath $motorBmp)) {
        [void]$problemas.Add("falta la captura de preview o de motor")
        return
    }
    $hPreview = Get-Sha256 $preview
    $hMotor = Get-Sha256 $motorBmp
    if ($hPreview -ne $hMotor) {
        [void]$problemas.Add(
            "preview y motor dibujan distinto: $hPreview / $hMotor")
    }
    Comprobar-Hash "preview_captura" $hPreview $problemas
}

Ejecutar-Caso "inicio_nodos" {
    param($dir, $problemas)

    $entrada = Join-Path $dir "entrada.log"
    $paso = Join-Path $dir "paso.txt"
    $deshecho = Join-Path $dir "deshecho.txt"
    $rehecho = Join-Path $dir "rehecho.txt"
    $borrado = Join-Path $dir "borrado.txt"
    $bmp = Join-Path $dir "inicio.bmp"
    Copy-Item (Join-Path $raiz "pruebas\modelo\juego_ia.log") $entrada

    $ordenes = "cargar|entrada.log;crear|escena|Bosque;crear|actor|Lobo;" +
               "reparentar|11|10;volcar|paso.txt;borrar|10;deshacer;" +
               "volcar|deshecho.txt;rehacer;volcar|rehecho.txt"
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $bmp,
                                     "--inicio", $ordenes)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    if (Test-Path -LiteralPath (Join-Path $dir "inicio.diagnostico.txt")) {
        [void]$problemas.Add("Inicio escribio un diagnostico en una secuencia valida")
    }
    foreach ($ruta in @($paso, $deshecho, $rehecho)) {
        if (-not (Test-Path -LiteralPath $ruta)) {
            [void]$problemas.Add("falta el volcado " + (Split-Path -Leaf $ruta))
            return
        }
    }
    $pasoTexto = Get-TextoNormalizado $paso
    $deshechoTexto = Get-TextoNormalizado $deshecho
    $rehechoTexto = Get-TextoNormalizado $rehecho
    Comprobar-Texto "inicio_nodos" ".paso.txt" $pasoTexto $problemas
    Comprobar-Texto "inicio_nodos" ".deshecho.txt" $deshechoTexto $problemas
    Comprobar-Texto "inicio_nodos" ".rehecho.txt" $rehechoTexto $problemas

    foreach ($linea in @(
        "proyecto_modelo entidades=11 conexiones=3 validacion=0 diagnostico=sin_errores",
        "entidad=10:escena:Bosque:origen=-:0:ui=0,0,0,0,0,00000000,0",
        "entidad=11:actor:Lobo:origen=-:0:ui=0,0,0,0,0,00000000,0",
        "referencia=11:padre:10:-:0",
        "inicio undo=3 redo=0 ultimo=reparentado")) {
        if (-not $pasoTexto.Contains($linea)) {
            [void]$problemas.Add("falta en el paso: " + $linea)
        }
    }
    foreach ($linea in @(
        "entidad=10:escena:Bosque:origen=-:0:ui=0,0,0,0,0,00000000,0",
        "referencia=11:padre:10:-:0",
        "inicio undo=3 redo=1 ultimo=deshecho")) {
        if (-not $deshechoTexto.Contains($linea)) {
            [void]$problemas.Add("falta al deshacer: " + $linea)
        }
    }
    if ($rehechoTexto.Contains("entidad=10:escena:Bosque")) {
        [void]$problemas.Add("rehacer no aplico el borrado")
    }
    if ($rehechoTexto.Contains("referencia=11:padre")) {
        [void]$problemas.Add("rehacer dejo una referencia al nodo borrado")
    }
    if (-not $rehechoTexto.Contains(
            "inicio undo=4 redo=0 ultimo=rehecho")) {
        [void]$problemas.Add("rehacer no actualizo los contadores")
    }

    # Borrar un actor con dos conexiones deja el grafo sin enlaces colgados.
    $ordenes2 = "cargar|entrada.log;borrar|2;volcar|borrado.txt"
    $codigo2 = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $bmp,
                                      "--inicio", $ordenes2)
    if ($codigo2 -ne 0) { [void]$problemas.Add("el editor devolvio $codigo2") }
    if (-not (Test-Path -LiteralPath $borrado)) {
        [void]$problemas.Add("falta el volcado del borrado de conexiones")
        return
    }
    $borradoTexto = Get-TextoNormalizado $borrado
    Comprobar-Texto "inicio_nodos" ".borrado.txt" $borradoTexto $problemas
    if (-not $borradoTexto.Contains(
            "proyecto_modelo entidades=6 conexiones=1")) {
        [void]$problemas.Add("borrar no quito las dos conexiones del actor")
    }
    if ($borradoTexto.Contains("conexion=7:") -or
        $borradoTexto.Contains("conexion=9:")) {
        [void]$problemas.Add("quedaron conexiones que referenciaban al actor")
    }
}

Ejecutar-Caso "inicio_ciclos" {
    param($dir, $problemas)

    $entrada = Join-Path $dir "entrada.log"
    $volcado = Join-Path $dir "ciclo.txt"
    $bmp = Join-Path $dir "inicio.bmp"
    Copy-Item (Join-Path $raiz "pruebas\modelo\juego_ia.log") $entrada

    $ordenes = "cargar|entrada.log;crear|escena|A;crear|escena|B;" +
               "reparentar|10|11;reparentar|11|10;volcar|ciclo.txt"
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $bmp,
                                     "--inicio", $ordenes)
    if ($codigo -eq 0) {
        [void]$problemas.Add("el ciclo de padres deberia devolver error")
    }
    if (Test-Path -LiteralPath $volcado) {
        [void]$problemas.Add("no se debio volcar un estado con ciclo")
    }
    $diag = Join-Path $dir "inicio.diagnostico.txt"
    if (-not (Test-Path -LiteralPath $diag)) {
        [void]$problemas.Add("falta el diagnostico del ciclo")
        return
    }
    $texto = Get-TextoNormalizado $diag
    Comprobar-Texto "inicio_ciclos" ".diag.txt" $texto $problemas
    if (-not $texto.Contains("comando 5 (reparentar): el reparentado formaria un ciclo")) {
        [void]$problemas.Add("el diagnostico no senala el comando 5")
    }
}


Ejecutar-Caso "scripts_crear_asociar" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "escena.log"
    $creado = Join-Path $dir "creado.log"
    $final = Join-Path $dir "final.log"
    $codigo = Join-Path $dir "Scripts\script1.lobo"
    $creadoTxt = Join-Path $dir "creado.txt"
    $asocTxt = Join-Path $dir "asoc.txt"
    $scriptsTxt = Join-Path $dir "scripts.txt"
    $logic3 = Join-Path $dir "final.logic3"
    $diag = Join-Path $dir "final.diag"
    $traza = Join-Path $dir "final.traza"
    $creadoBmp = Join-Path $dir "creado.bmp"
    $juegoBmp = Join-Path $dir "juego.bmp"
    New-Item -ItemType Directory -Force -Path (Join-Path $dir "Scripts") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $dir "tiles") | Out-Null
    Copy-Item (Join-Path $raiz "pruebas\modelo\escena_scripts.log") $entrada
    Copy-Item (Join-Path $raiz "pruebas\modelo\Scripts\player.lobo") `
              (Join-Path $dir "Scripts\player.lobo")
    Copy-Item (Join-Path $raiz "tiles\jugador.png") `
              (Join-Path $dir "tiles\jugador.png")

    # Nodo Player -> +SCRIPT; el editor abre el archivo nuevo y Ctrl+G lo guarda.
    $salida = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
                                     "--sin-ventanas",
                                     "--clics", "80,147;1045,483",
                                     "--inicio-teclas", 'mi.x = mi.x + 2\n\s',
                                     "--captura", $creadoBmp,
                                     "--guardar-modelo", "creado.log",
                                     "--volcar-inicio", $creadoTxt)
    if ($salida -ne 0) { [void]$problemas.Add("el editor devolvio $salida al crear") }
    foreach ($archivo in @($creado, $codigo, $creadoTxt, $creadoBmp)) {
        if (-not (Test-Path -LiteralPath $archivo)) {
            [void]$problemas.Add("falta " + (Split-Path -Leaf $archivo))
        }
    }
    if ($problemas.Count -gt 0) { return }
    $texto = Get-TextoNormalizado $creado
    foreach ($linea in @('script id 16 nombre "Script 1" ruta "Scripts/script1.lobo"')) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta en creado.log: " + $linea)
        }
    }
    if (-not ((Get-Content -LiteralPath $codigo -Raw).Contains("mi.x = mi.x + 2"))) {
        [void]$problemas.Add("el editor no guardo el codigo del script nuevo")
    }
    $texto = Get-TextoNormalizado $creadoTxt
    foreach ($linea in @("inicio_editor=1 id=16 cursor=16 modificado=0",
                         "inicio_script=16 ruta=Scripts/script1.lobo",
                         "inicio_entidades=9 escenas=1 nodos=3 scripts=2 asociaciones=1")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta en el editor: " + $linea)
        }
    }
    Comprobar-Texto "scripts_crear_asociar" ".pixeles" `
                    (Get-PixelesInicioScripts $creadoBmp) $problemas

    # Script 1 -> ASOCIAR -> nodo Player. Queda fijado el evento fotograma.
    $salida = Ejecutar-Editor $dir @("--inicio-modelo", "creado.log",
                                     "--sin-ventanas",
                                     "--clics", "80,235;947,511;80,147",
                                     "--inicio-scripts", "volcar_scripts|scripts.txt",
                                     "--captura", (Join-Path $dir "asoc.bmp"),
                                     "--volcar-inicio", $asocTxt,
                                     "--guardar-modelo", "final.log")
    if ($salida -ne 0) { [void]$problemas.Add("el editor devolvio $salida al asociar") }
    foreach ($archivo in @($final, $asocTxt, $scriptsTxt)) {
        if (-not (Test-Path -LiteralPath $archivo)) {
            [void]$problemas.Add("falta " + (Split-Path -Leaf $archivo))
        }
    }
    if ($problemas.Count -gt 0) { return }
    $texto = Get-TextoNormalizado $asocTxt
    foreach ($linea in @("inicio_sel=19",
                         "inicio_seleccion=asociacion #19",
                         "inicio_binding=19 script=16 destino=12 evento=fotograma",
                         "inicio_entidades=10 escenas=1 nodos=3 scripts=2 asociaciones=2")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta tras asociar: " + $linea)
        }
    }
    $texto = Get-TextoNormalizado $scriptsTxt
    foreach ($linea in @("inicio_scripts=2",
                         "inicio_script=16 nombre=Script 1 ruta=Scripts/script1.lobo asociaciones=1",
                         "inicio_asociacion=19 script=16 nodo=12 evento=fotograma")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta en volcar_scripts: " + $linea)
        }
    }
    $texto = Get-TextoNormalizado $final
    if (-not $texto.Contains('asociar script 16 nodo 12 evento "fotograma"')) {
        [void]$problemas.Add("el LOG 2 guardado no conserva la asociacion nueva")
    }

    $salida = Ejecutar-Editor $dir @("--sin-ventanas", "--captura", $juegoBmp,
                                     "--modelo-logic3",
                                     "final.log|final.logic3|final.diag")
    if ($salida -ne 0) { [void]$problemas.Add("la conversion devolvio $salida") }
    if (-not (Test-Path -LiteralPath $logic3) -or
        -not (Test-Path -LiteralPath $diag)) {
        [void]$problemas.Add("no se pudo convertir el LOG 2 final")
        return
    }
    if ((Get-Content -LiteralPath $diag -Raw).Trim() -ne "sin errores") {
        [void]$problemas.Add("el LOG 2 final no valida")
    }
    $texto = Get-TextoNormalizado $logic3
    foreach ($linea in @('campo "codigo" "mi.x = mi.x + 2\n"', 'evento 2 fotograma')) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta en LOGIC3: " + $linea)
        }
    }
    $salida = Ejecutar-Motor $dir @("--logic3", "final.logic3",
                                    "--captura", $juegoBmp, "--traza", $traza,
                                    "--fotogramas", "3", "--entidades")
    if ($salida -ne 0) { [void]$problemas.Add("el motor devolvio $salida") }
    if (-not (Test-Path -LiteralPath $traza)) {
        [void]$problemas.Add("falta la traza del script creado desde Inicio")
        return
    }
    $texto = Get-TextoNormalizado $traza
    foreach ($linea in @("vm: 2 scripts", "ent 2 89 80")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta en la traza: " + $linea)
        }
    }
    Comprobar-Texto "scripts_crear_asociar" ".traza.txt" $texto $problemas
}

Ejecutar-Caso "scripts_editar_compilar" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "escena.log"
    $codigo = Join-Path $dir "Scripts\player.lobo"
    $invalidoTxt = Join-Path $dir "invalido.txt"
    $validoTxt = Join-Path $dir "valido.txt"
    $invalidoBmp = Join-Path $dir "invalido.bmp"
    New-Item -ItemType Directory -Force -Path (Join-Path $dir "Scripts") | Out-Null
    Copy-Item (Join-Path $raiz "pruebas\modelo\escena_scripts.log") $entrada
    Copy-Item (Join-Path $raiz "pruebas\modelo\Scripts\player.lobo") $codigo

    $salida = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
                                     "--sin-ventanas",
                                     "--clics", "80,213;947,483",
                                     "--inicio-teclas", '\nzzz\f',
                                     "--captura", $invalidoBmp,
                                     "--volcar-inicio", $invalidoTxt)
    if ($salida -ne 0) { [void]$problemas.Add("el editor devolvio $salida al compilar") }
    if (-not (Test-Path -LiteralPath $invalidoTxt)) {
        [void]$problemas.Add("falta el volcado del error")
        return
    }
    $texto = Get-TextoNormalizado $invalidoTxt
    foreach ($linea in @("inicio_editor=1 id=2",
                         "codigo_error=3:linea 3: nombre desconocido",
                         "ultimo=Scripts/player.lobo:3: linea 3: nombre desconocido")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta el mapeo del error: " + $linea)
        }
    }
    if (-not ((Get-Content -LiteralPath $codigo -Raw).Contains("zzz"))) {
        [void]$problemas.Add("F7 no guardo el codigo con errores")
    }
    Comprobar-Texto "scripts_editar_compilar" ".pixeles" `
                    (Get-PixelesInicioScripts $invalidoBmp) $problemas

    # Cuatro Retrocesos quitan "zzz" y el salto; luego se escribe una linea
    # valida, se guarda con Ctrl+G y se compila de nuevo.
    $salida = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
                                     "--sin-ventanas",
                                     "--clics", "80,213;947,483",
                                     "--inicio-teclas", '\b\b\b\b\nmi.y = mi.y + 1\s\f',
                                     "--captura", (Join-Path $dir "valido.bmp"),
                                     "--volcar-inicio", $validoTxt)
    if ($salida -ne 0) { [void]$problemas.Add("el editor devolvio $salida al corregir") }
    if (-not (Test-Path -LiteralPath $validoTxt)) {
        [void]$problemas.Add("falta el volcado corregido")
        return
    }
    $texto = Get-TextoNormalizado $validoTxt
    foreach ($linea in @("codigo_error=0:",
                         "ultimo=Scripts/player.lobo:OK:8")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta la compilacion correcta: " + $linea)
        }
    }
    $codigoTexto = Get-Content -LiteralPath $codigo -Raw
    foreach ($linea in @("mi.x = mi.x + 1", "mi.y = mi.y + 1")) {
        if (-not $codigoTexto.Contains($linea)) {
            [void]$problemas.Add("falta la linea guardada: " + $linea)
        }
    }
}

Ejecutar-Caso "scripts_navegacion" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "escena.log"
    $asocTxt = Join-Path $dir "asoc.txt"
    $desdeAsoc = Join-Path $dir "desde_asoc.txt"
    $desdeObjeto = Join-Path $dir "desde_objeto.txt"
    $inversoTxt = Join-Path $dir "inverso.txt"
    $scriptsTxt = Join-Path $dir "scripts.txt"
    $bmp = Join-Path $dir "navegacion.bmp"
    New-Item -ItemType Directory -Force -Path (Join-Path $dir "Scripts") | Out-Null
    Copy-Item (Join-Path $raiz "pruebas\modelo\escena_scripts.log") $entrada
    Copy-Item (Join-Path $raiz "pruebas\modelo\Scripts\player.lobo") `
              (Join-Path $dir "Scripts\player.lobo")

    $salida = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
                                     "--sin-ventanas",
                                     "--clics", "80,213;920,405",
                                     "--captura", (Join-Path $dir "asoc.bmp"),
                                     "--volcar-inicio", $asocTxt)
    if ($salida -ne 0) { [void]$problemas.Add("el editor devolvio $salida al seleccionar") }
    if (-not (Test-Path -LiteralPath $asocTxt)) { return }
    $texto = Get-TextoNormalizado $asocTxt
    foreach ($linea in @("inicio_sel=14",
                         "inicio_seleccion=asociacion #14",
                         "inicio_binding=14 script=2 destino=12 evento=fotograma")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta la asociacion seleccionada: " + $linea)
        }
    }

    $salida = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
                                     "--sin-ventanas",
                                     "--clics", "80,213;920,405;947,483",
                                     "--captura", $bmp,
                                     "--volcar-inicio", $desdeAsoc)
    if ($salida -ne 0) { [void]$problemas.Add("el editor devolvio $salida al abrir el script") }
    if (-not (Test-Path -LiteralPath $desdeAsoc)) { return }
    $texto = Get-TextoNormalizado $desdeAsoc
    foreach ($linea in @("inicio_sel=2", "inicio_editor=1 id=2",
                         "inicio_seleccion=script #2")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("la asociacion no abrio el script: " + $linea)
        }
    }
    Comprobar-Texto "scripts_navegacion" ".pixeles" `
                    (Get-PixelesInicioScripts $bmp) $problemas

    $salida = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
                                     "--sin-ventanas",
                                     "--clics", "80,213;920,405;1045,483",
                                     "--captura", (Join-Path $dir "objeto.bmp"),
                                     "--volcar-inicio", $desdeObjeto)
    if ($salida -ne 0) { [void]$problemas.Add("el editor devolvio $salida al abrir el objeto") }
    if (-not (Test-Path -LiteralPath $desdeObjeto)) { return }
    $texto = Get-TextoNormalizado $desdeObjeto
    foreach ($linea in @("inicio_sel=12", "inicio_editor=0 id=0",
                         "inicio_seleccion=nodo #12")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("la asociacion no abrio el objeto: " + $linea)
        }
    }

    # Camino inverso: objeto -> ASOCIAR -> script; despues se cambia el evento.
    $salida = Ejecutar-Editor $dir @("--inicio-modelo", "escena.log",
                                     "--sin-ventanas",
                                     "--clics", "80,147;947,483;80,213;1143,483",
                                     "--inicio-scripts", "volcar_scripts|scripts.txt",
                                     "--captura", (Join-Path $dir "inverso.bmp"),
                                     "--volcar-inicio", $inversoTxt)
    if ($salida -ne 0) { [void]$problemas.Add("el editor devolvio $salida al asociar al reves") }
    foreach ($archivo in @($inversoTxt, $scriptsTxt)) {
        if (-not (Test-Path -LiteralPath $archivo)) {
            [void]$problemas.Add("falta " + (Split-Path -Leaf $archivo))
        }
    }
    if ($problemas.Count -gt 0) { return }
    $texto = Get-TextoNormalizado $inversoTxt
    foreach ($linea in @("inicio_sel=16",
                         "inicio_binding=16 script=2 destino=12 evento=inicio",
                         "inicio_entidades=9 escenas=1 nodos=3 scripts=1 asociaciones=2")) {
        if (-not $texto.Contains($linea)) {
            [void]$problemas.Add("falta la asociacion inversa: " + $linea)
        }
    }
    $texto = Get-TextoNormalizado $scriptsTxt
    if (-not $texto.Contains("inicio_asociacion=16 script=2 nodo=12 evento=inicio")) {
        [void]$problemas.Add("el evento elegido no llego al volcado")
    }
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("PROGRAMA: {0} casos, todo OK" -f $script:total) -ForegroundColor Green
    exit 0
}
Write-Host ("PROGRAMA: {0} casos, {1} fallos" -f $script:total, $script:fallos) -ForegroundColor Red
exit 1
