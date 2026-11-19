# compilador.ps1 - suite del compilador bloques -> lobo (fase 33).
#
# Congela el lobo generado, el volcado cb_* y la traza de la VM. El caso
# `paridad` comprueba el criterio de salida de la fase: el mismo proyecto
# produce la misma traza desde bloques y desde un lobo escrito a mano.
#
# Casos:
#   generar_mover    un bloque mover x genera `x = x + 2`
#   generar_control  repetir + mover anidado genera mientras/fin
#   generar_variable cambiar variable genera una variable de entidad
#   error_bloque     el error se mapea a linea y bloque
#   visor            --visor-codigo muestra el lobo de solo lectura
#   convertir        la conversion es irreversible y se guarda en LOGIC3
#   paridad          bloques y lobo manual producen la misma traza
#   menu_indices     ARCHIVO=1, MODO=2 y AYUDA=9 siguen en su sitio
#
# Uso:
#   pruebas\compilador.ps1                      pasa la suite
#   pruebas\compilador.ps1 -Filtro generar*     solo los casos que encajan
#   pruebas\compilador.ps1 -Actualizar          reescribe los patrones

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
$esperadoDir = Join-Path $PSScriptRoot "compilador\esperado"
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

$tmp = Join-Path $env:TEMP ("lw2000-compilador-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

# Categorias: 0 eventos, 1 control, 2 movimiento, 3 datos, 5 codigo.
# Fila 0 de la paleta esta en y=212 con la geometria nominal de 1200x700.
$mover = "2:100,212;400,300"
$anidar = "1:100,212;400,300|2:100,212;430,345"
$variable = "3:100,212;400,300"
$codigo = "5:100,212;400,300"

Write-Host ""
Write-Host "Compilador bloques - lobo" -ForegroundColor Cyan
Write-Host ("  editor: {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  motor:  {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Get-CampoTexto([string]$texto, [string]$clave) {
    $m = [regex]::Match($texto, "(?:^|\s)" + [regex]::Escape($clave) + "=([^\s]+)")
    if ($m.Success) { return $m.Groups[1].Value }
    return ""
}

function Get-Pixeles([string]$bmp) {
    $imagen = [System.Drawing.Bitmap]::FromFile($bmp)
    try {
        $puntos = @("300,170", "400,300", "1100,400", "1130,540")
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

# Comprueba un artefacto de texto contra su patron, o lo reescribe.
function Comprobar-Texto([string]$nombre, [string]$ruta, [string]$extension) {
    $obtenido = Get-TextoNormalizado $ruta
    $patron = Join-Path $esperadoDir ($nombre + $extension)
    if ($Actualizar) {
        Guardar-TextoNormalizado $patron $obtenido
    } elseif (-not (Test-Path -LiteralPath $patron)) {
        return ("falta " + $nombre + $extension + " (usa -Actualizar)")
    } elseif ((Get-TextoNormalizado $patron) -ne $obtenido) {
        Mostrar-Diferencia ((Get-TextoNormalizado $patron) -split "`n") `
                           ($obtenido -split "`n") 8
        return ("el patron de " + $nombre + " no coincide")
    }
    return ""
}

function Ejecutar-Editor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Editor $argumentos $dir
}

function Ejecutar-Motor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Motor $argumentos $dir
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
        Write-Host ("  [{0,3}] {1,-16} OK" -f $script:indice, $nombre) -ForegroundColor Green
    } else {
        $script:fallos++
        Write-Host ("  [{0,3}] {1,-16} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
        foreach ($p in $problemas) {
            Write-Host ("        - {0}" -f $p) -ForegroundColor Red
        }
    }
}

function Comprobar-Campos([string]$dir, [string]$dump, [hashtable]$campos, $problemas) {
    $ruta = Join-Path $dir $dump
    if (-not (Test-Path -LiteralPath $ruta)) {
        [void]$problemas.Add("falta el volcado")
        return
    }
    $texto = Get-TextoNormalizado $ruta
    foreach ($clave in ($campos.Keys | Sort-Object)) {
        $valor = Get-CampoTexto $texto $clave
        $quiero = [string]$campos[$clave]
        if ($valor -ne $quiero) {
            [void]$problemas.Add(("volcado: {0}={1} (esperado {2})" -f `
                                  $clave, $valor, $quiero))
        }
    }
}

Ejecutar-Caso "generar_mover" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "g.bmp"
    $dump = Join-Path $dir "g.txt"
    $lobo = Join-Path $dir "g.lobo"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--arrastre-bloque", $mover, `
                                     "--compilar-bloques", `
                                     "--captura", $bmp, "--volcar", $dump, `
                                     "--volcar-codigo", $lobo)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Campos $dir "g.txt" @{
        cb_visor = "0"; cb_lineas = "1"; cb_error = "-";
        cb_bloque = "0"; cb_linea = "0"; cb_convertido = "0"
    } $problemas
    $p = Comprobar-Texto "generar_mover" $lobo ".lobo"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "generar_control" {
    param($dir, $problemas)
    $lobo = Join-Path $dir "c.lobo"
    $dump = Join-Path $dir "c.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--arrastre-bloque", $anidar, `
                                     "--compilar-bloques", `
                                     "--captura", (Join-Path $dir "c.bmp"), `
                                     "--volcar", $dump, "--volcar-codigo", $lobo)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Campos $dir "c.txt" @{
        lb_cuerpo = "1"; cb_lineas = "3"; cb_error = "-"
    } $problemas
    $p = Comprobar-Texto "generar_control" $lobo ".lobo"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "generar_variable" {
    param($dir, $problemas)
    $lobo = Join-Path $dir "v.lobo"
    $dump = Join-Path $dir "v.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--arrastre-bloque", $variable, `
                                     "--campo-bloque", "1000:vidas", `
                                     "--compilar-bloques", `
                                     "--captura", (Join-Path $dir "v.bmp"), `
                                     "--volcar", $dump, "--volcar-codigo", $lobo)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Campos $dir "v.txt" @{
        cb_lineas = "1"; cb_error = "-"; cb_bloque = "0"
    } $problemas
    $p = Comprobar-Texto "generar_variable" $lobo ".lobo"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "error_bloque" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "e.bmp"
    $dump = Join-Path $dir "e.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--arrastre-bloque", $codigo, `
                                     "--campo-bloque", "1000:esto_no_es_lobo", `
                                     "--compilar-bloques", `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Campos $dir "e.txt" @{
        cb_visor = "0"; cb_lineas = "1";
        cb_error = "linea_1:_nombre_desconocido";
        cb_bloque = "1000"; cb_linea = "1"; cb_convertido = "0"
    } $problemas
    $pixeles = Get-Pixeles $bmp
    $rutaPix = Join-Path $dir "e.pixeles.tmp"
    Guardar-TextoNormalizado $rutaPix $pixeles
    $p = Comprobar-Texto "error_bloque" $rutaPix ".pixeles"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "visor" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "visor.bmp"
    $dump = Join-Path $dir "visor.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--arrastre-bloque", $mover, `
                                     "--visor-codigo", `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Campos $dir "visor.txt" @{
        cb_visor = "1"; cb_lineas = "1"; cb_error = "-"; cb_convertido = "0"
    } $problemas
    $pixeles = Get-Pixeles $bmp
    $rutaPix = Join-Path $dir "visor.pixeles.tmp"
    Guardar-TextoNormalizado $rutaPix $pixeles
    $p = Comprobar-Texto "visor" $rutaPix ".pixeles"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "convertir" {
    param($dir, $problemas)
    $logic = Join-Path $dir "c.logic3"
    $bmp = Join-Path $dir "c.bmp"
    $dump = Join-Path $dir "c.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--arrastre-bloque", $mover, `
                                     "--convertir-bloques", `
                                     "--guardar-logic3", $logic, `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Campos $dir "c.txt" @{
        lb_bloques = "1"; cb_visor = "1"; cb_lineas = "1";
        cb_error = "-"; cb_convertido = "1"
    } $problemas
    $p = Comprobar-Texto "convertir" $logic ".logic3"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "paridad" {
    param($dir, $problemas)
    $desdeBloques = Join-Path $dir "bloques.logic3"
    $manual = Join-Path $dir "manual.logic3"
    $bmp = Join-Path $dir "b.bmp"
    $dump = Join-Path $dir "b.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--arrastre-bloque", $mover, `
                                     "--convertir-bloques", `
                                     "--guardar-logic3", $desdeBloques, `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }

    # Lobo escrito a mano equivalente al que generan los bloques.
    $texto = @(
        'LOGIC3',
        'version 3',
        'proyecto "Mi proyecto" 640 480 1 60 3 00000000',
        'carpetas "tiles" "maps" "trajes" "sonidos"',
        'escena 1 "Escena 1" "" 640 480 001E5A2E',
        'objeto 1 "Objeto 1" escena 1 x 28 y 26 w 32 h 32 visible 1 capa 0',
        '  evento 1 inicio',
        '  evento 2 fotograma',
        'bloque 1000 evento 1 padre 0 orden 0 tipo "codigo.lobo"',
        '  campo "codigo" "x = x + 2\n"',
        'FIN'
    ) -join "`n"
    Guardar-TextoNormalizado $manual $texto

    $trazaB = Join-Path $dir "bloques.traza"
    $trazaM = Join-Path $dir "manual.traza"
    $c1 = Ejecutar-Motor $dir @("--logic3", $desdeBloques, `
                               "--traza", $trazaB, "--fotogramas", "8", `
                               "--captura", (Join-Path $dir "b2.bmp"))
    $c2 = Ejecutar-Motor $dir @("--logic3", $manual, `
                               "--traza", $trazaM, "--fotogramas", "8", `
                               "--captura", (Join-Path $dir "m.bmp"))
    if ($c1 -ne 0) { [void]$problemas.Add("el motor devolvio $c1 desde bloques") }
    if ($c2 -ne 0) { [void]$problemas.Add("el motor devolvio $c2 manual") }
    $tb = Get-TextoNormalizado $trazaB
    $tm = Get-TextoNormalizado $trazaM
    if ($tb -ne $tm) {
        [void]$problemas.Add("la traza desde bloques no coincide con la manual")
        Mostrar-Diferencia ($tm -split "`n") ($tb -split "`n") 8
    }
    $p = Comprobar-Texto "paridad" $trazaB ".traza"
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
        if ($texto -match "cb_visor=") {
            [void]$problemas.Add("campos del compilador fuera de la pestana Bloques")
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
    Write-Host ("Compilador: {0} casos, todos correctos" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("Compilador: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
