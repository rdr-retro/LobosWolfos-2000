# programa.ps1 - suite del modelo y el constructor de programa (fases 46-47).
#
# Congela la separacion entre escenario y jugador. Un proyecto nuevo guarda
# la pieza sistema.opciones con jugador_implicito=0; un LOGIC3 sin esa pieza
# es legacy y mantiene el lobito clasico. La fase 47 anade la vista visible:
# paleta, tarjetas, enlaces e inspector dentro del gestor.
#
# Casos:
#   opciones_nuevo  el editor crea sistema.opciones en un proyecto nuevo
#   mapa_solo       mapa sin opciones de jugador: lobo quieto y sin sprite
#   legacy          sin sistema.opciones: lobo se mueve y el BMP cambia
#   ui_programa     lienzo Programa con tres piezas, un enlace y sondas BMP
#   entrada_gestor  --gestor abre el constructor sin piezas y con paleta visible
#   elementos_logic2 al abrir un .logic clasico, el inspector lista sus elementos
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

# Sondas del inspector cuando el constructor muestra los elementos LOGIC2:
# titulo INSPECTOR, cabecera de la lista, las dos filas del minijuego y
# el contador final. Todas las filas nuevas viven en el panel derecho.
function Get-PixelesElementosLogic2([string]$bmp) {
    $imagen = [System.Drawing.Bitmap]::FromFile($bmp)
    try {
        $puntos = @("921,68", "930,94", "932,114", "931,132", "931,154")
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
    if ($ui -notlike "programa_modo=1 programa_piezas=0*") {
        [void]$problemas.Add("el gestor no abrio el constructor")
    }
    if ($logica -notlike "*modulos=2*") {
        [void]$problemas.Add("el .logic no trae dos elementos")
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
