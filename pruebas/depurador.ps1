# depurador.ps1 - suite del depurador del motor (fase 34).
#
# Congela el volcado de cada punto de parada, la traza del replay y el
# resultado del comparador. El caso `panel` comprueba por pixeles que el
# panel visual solo aparece con --panel-debug. Ningun caso usa las rutas
# de las once suites anteriores.
#
# Casos:
#   pausa             pausar y observar vidas=2 antes de avanzar
#   paso_instruccion  tres instrucciones dejan vidas=1
#   break_bloque      breakpoint por id de bloque
#   break_evento      breakpoint por tipo de evento
#   error_runtime     la VM informa del pc con error
#   replay            grabar y reproducir la misma entrada
#   comparar          sin divergencias y primera divergencia
#   panel             pixeles del panel en la vista Play
#   menu_indices      ARCHIVO=1, MODO=2 y AYUDA=9 siguen en su sitio
#
# Uso:
#   pruebas\depurador.ps1                      pasa la suite
#   pruebas\depurador.ps1 -Filtro break*       solo los casos que encajan
#   pruebas\depurador.ps1 -Actualizar          reescribe los patrones

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
$fixtures = Join-Path $PSScriptRoot "depurador"
$esperadoDir = Join-Path $fixtures "esperado"
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

$tmp = Join-Path $env:TEMP ("lw2000-depurador-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Depurador" -ForegroundColor Cyan
Write-Host ("  editor:   {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  motor:    {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  casos:    {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Get-CampoTexto([string]$texto, [string]$clave) {
    $m = [regex]::Match($texto, "(?:^|\s)" + [regex]::Escape($clave) + "=([^\s]+)")
    if ($m.Success) { return $m.Groups[1].Value }
    return ""
}

function Comprobar-Texto([string]$nombre, [string]$ruta, [string]$extension) {
    $obtenido = Get-TextoNormalizado $ruta
    $patron = Join-Path $esperadoDir ($nombre + $extension)
    if ($Actualizar) {
        Guardar-TextoNormalizado $patron $obtenido
    } elseif (-not (Test-Path -LiteralPath $patron)) {
        return ("falta " + $nombre + $extension + " (usa -Actualizar)")
    } elseif ((Get-TextoNormalizado $patron) -ne $obtenido) {
        Mostrar-Diferencia ((Get-TextoNormalizado $patron) -split "`n") `
                           ($obtenido -split "`n") 10
        return ("el patron de " + $nombre + " no coincide")
    }
    return ""
}

function Ejecutar-Motor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Motor $argumentos $dir
}

function Ejecutar-Editor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Editor $argumentos $dir
}

function Get-PixelesPanel([string]$bmp) {
    $imagen = [System.Drawing.Bitmap]::FromFile($bmp)
    try {
        $puntos = @("4,4", "10,10", "20,12", "100,30", "4,115", "240,60")
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
        Write-Host ("  [{0,3}] {1,-17} OK" -f $script:indice, $nombre) -ForegroundColor Green
    } else {
        $script:fallos++
        Write-Host ("  [{0,3}] {1,-17} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
        foreach ($p in $problemas) {
            Write-Host ("        - {0}" -f $p) -ForegroundColor Red
        }
    }
}

$pausaLogic = Join-Path $fixtures "pausa.logic3"
$pasoLogic = Join-Path $fixtures "paso.logic3"
$teclasLogic = Join-Path $fixtures "teclas.logic3"
$errorLogic = Join-Path $fixtures "error.logic3"
$ordenesPausa = Join-Path $fixtures "ordenes_pausa.txt"
$ordenesError = Join-Path $fixtures "ordenes_error.txt"

Ejecutar-Caso "pausa" {
    param($dir, $problemas)
    $debug = Join-Path $dir "p.debug"
    $codigo = Ejecutar-Motor $dir @("--logic3", $pausaLogic, `
                                     "--captura", (Join-Path $dir "p.bmp"), `
                                     "--fotogramas", "4", `
                                     "--traza", (Join-Path $dir "p.traza"), `
                                     "--depurar", "--ordenes", $ordenesPausa, `
                                     "--volcar-debug", $debug)
    if ($codigo -ne 0) { [void]$problemas.Add("el motor devolvio $codigo") }
    $p = Comprobar-Texto "pausa" $debug ".debug"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "paso_instruccion" {
    param($dir, $problemas)
    $debug = Join-Path $dir "pi.debug"
    $codigo = Ejecutar-Motor $dir @("--logic3", $pasoLogic, `
                                     "--captura", (Join-Path $dir "pi.bmp"), `
                                     "--fotogramas", "3", `
                                     "--depurar", "--paso", "instruccion:3", `
                                     "--observar", "vidas:0", `
                                     "--volcar-debug", $debug)
    if ($codigo -ne 0) { [void]$problemas.Add("el motor devolvio $codigo") }
    $p = Comprobar-Texto "paso_instruccion" $debug ".debug"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "break_bloque" {
    param($dir, $problemas)
    $debug = Join-Path $dir "bb.debug"
    $codigo = Ejecutar-Motor $dir @("--logic3", $pausaLogic, `
                                     "--captura", (Join-Path $dir "bb.bmp"), `
                                     "--fotogramas", "3", `
                                     "--depurar", "--break", "B:2", `
                                     "--volcar-debug", $debug)
    if ($codigo -ne 0) { [void]$problemas.Add("el motor devolvio $codigo") }
    $p = Comprobar-Texto "break_bloque" $debug ".debug"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "break_evento" {
    param($dir, $problemas)
    $debug = Join-Path $dir "be.debug"
    $codigo = Ejecutar-Motor $dir @("--logic3", $pausaLogic, `
                                     "--captura", (Join-Path $dir "be.bmp"), `
                                     "--fotogramas", "3", `
                                     "--depurar", "--break", "E:2", `
                                     "--volcar-debug", $debug)
    if ($codigo -ne 0) { [void]$problemas.Add("el motor devolvio $codigo") }
    $p = Comprobar-Texto "break_evento" $debug ".debug"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "error_runtime" {
    param($dir, $problemas)
    $debug = Join-Path $dir "e.debug"
    $codigo = Ejecutar-Motor $dir @("--logic3", $errorLogic, `
                                     "--captura", (Join-Path $dir "e.bmp"), `
                                     "--fotogramas", "2", `
                                     "--traza", (Join-Path $dir "e.traza"), `
                                     "--depurar", "--ordenes", $ordenesError, `
                                     "--volcar-debug", $debug)
    if ($codigo -ne 0) { [void]$problemas.Add("el motor devolvio $codigo") }
    $p = Comprobar-Texto "error_runtime" $debug ".debug"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "replay" {
    param($dir, $problemas)
    $grabar = Join-Path $dir "grabar.traza"
    $reproducir = Join-Path $dir "reproducir.traza"
    $replay = Join-Path $dir "partida.replay"
    $c1 = Ejecutar-Motor $dir @("--logic3", $teclasLogic, `
                                 "--captura", (Join-Path $dir "g.bmp"), `
                                 "--fotogramas", "4", "--traza", $grabar, `
                                 "--tecla", "derecha", `
                                 "--grabar-replay", $replay)
    $c2 = Ejecutar-Motor $dir @("--logic3", $teclasLogic, `
                                 "--captura", (Join-Path $dir "r.bmp"), `
                                 "--fotogramas", "4", "--traza", $reproducir, `
                                 "--replay", $replay)
    if ($c1 -ne 0) { [void]$problemas.Add("al grabar devolvio $c1") }
    if ($c2 -ne 0) { [void]$problemas.Add("al reproducir devolvio $c2") }
    if ((Get-TextoNormalizado $grabar) -ne (Get-TextoNormalizado $reproducir)) {
        [void]$problemas.Add("la traza reproducida no coincide con la grabada")
        Mostrar-Diferencia ((Get-TextoNormalizado $grabar) -split "`n") `
                           ((Get-TextoNormalizado $reproducir) -split "`n") 8
    }
    $p = Comprobar-Texto "replay" $replay ".replay"
    if ($p) { [void]$problemas.Add($p) }
    $p = Comprobar-Texto "replay_traza" $reproducir ".traza"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "comparar" {
    param($dir, $problemas)
    $a = Join-Path $dir "a.txt"
    $b = Join-Path $dir "b.txt"
    Guardar-TextoNormalizado $a "uno`ndos`ntres"
    Guardar-TextoNormalizado $b "uno`ndos`ntres"
    $igual = Join-Path $dir "igual.txt"
    $c1 = Ejecutar-Motor $dir @("--comparar", $a, $b, "--traza", $igual)
    if ($c1 -ne 0) { [void]$problemas.Add("al comparar iguales devolvio $c1") }
    $p = Comprobar-Texto "comparar_igual" $igual ".txt"
    if ($p) { [void]$problemas.Add($p) }

    Guardar-TextoNormalizado $b "uno`ndos cambios`ntres"
    $distinto = Join-Path $dir "distinto.txt"
    $c2 = Ejecutar-Motor $dir @("--comparar", $a, $b, "--traza", $distinto)
    if ($c2 -ne 0) { [void]$problemas.Add("al comparar distintos devolvio $c2") }
    $p = Comprobar-Texto "comparar_divergente" $distinto ".txt"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "panel" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "panel.bmp"
    $codigo = Ejecutar-Motor $dir @("--logic3", $pausaLogic, `
                                     "--captura", $bmp, "--fotogramas", "2", `
                                     "--depurar", "--panel-debug", `
                                     "--ordenes", $ordenesPausa, `
                                     "--volcar-debug", (Join-Path $dir "panel.debug"))
    if ($codigo -ne 0) { [void]$problemas.Add("el motor devolvio $codigo") }
    $pixeles = Get-PixelesPanel $bmp
    $rutaPix = Join-Path $dir "panel.pixeles.tmp"
    Guardar-TextoNormalizado $rutaPix $pixeles
    $p = Comprobar-Texto "panel" $rutaPix ".pixeles"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "menu_indices" {
    param($dir, $problemas)
    $lineas = @()
    foreach ($menu in @(1, 2, 9)) {
        $bmp = Join-Path $dir ("menu" + $menu + ".bmp")
        $dump = Join-Path $dir ("menu" + $menu + ".txt")
        $codigo = Ejecutar-Editor $dir @("--menu", ([string]$menu), `
                                         "--sin-ventanas", `
                                         "--captura", $bmp, "--volcar", $dump)
        if ($codigo -ne 0) {
            [void]$problemas.Add("el menu $menu devolvio $codigo")
            continue
        }
        $texto = Get-TextoNormalizado $dump
        $valor = Get-CampoTexto $texto "menu"
        $vista = Get-CampoTexto $texto "vista"
        if ($valor -ne ([string]$menu)) {
            [void]$problemas.Add(("menu={0} (esperado {1})" -f $valor, $menu))
        }
        if ($vista -ne "0") {
            [void]$problemas.Add(("vista={0} en el menu {1}" -f $vista, $menu))
        }
        $lineas += ("menu={0} vista={1}" -f $valor, $vista)
    }
    $ruta = Join-Path $dir "menu_indices.tmp"
    Guardar-TextoNormalizado $ruta ($lineas -join "`n")
    $p = Comprobar-Texto "menu_indices" $ruta ".volcar"
    if ($p) { [void]$problemas.Add($p) }
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("Depurador: {0} casos, todos correctos" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("Depurador: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
