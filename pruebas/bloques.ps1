# bloques.ps1 - suite del lienzo de bloques (fase 32).
#
# Congela la captura, el volcado y cinco sondas de pixel de cada caso.
# Las sondas cubren la paleta, el lienzo y el inspector. Las rutas temporales
# se sustituyen por <CASO> para que los patrones sean estables entre maquinas.
#
# Casos:
#   entrada        pestana Bloques con el lienzo vacio
#   arrastrar      un bloque va de la paleta al lienzo con el raton
#   seleccion_id   `--bloque N` elige un bloque por id LOGIC3
#   ranura         un reportero se acopla a la entrada dx
#   anidar         un C acoge dos hijos en su cuerpo
#   deshacer       `--deshacer-bloque` revierte la ultima creacion
#   teclado        Tab y Enter crean un bloque sin raton
#   guardar        el script del capitulo 4.3 se guarda y se reabre en LOGIC3
#   menu_indices   ARCHIVO=1, MODO=2 y AYUDA=9 siguen en su sitio
#
# Uso:
#   pruebas\bloques.ps1                      pasa la suite
#   pruebas\bloques.ps1 -Filtro anidar*      solo los casos que encajan
#   pruebas\bloques.ps1 -Actualizar          reescribe los patrones

[CmdletBinding()]
param(
    [string]$Editor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")
Add-Type -AssemblyName System.Drawing

$raiz = Split-Path -Parent $PSScriptRoot
$esperadoDir = Join-Path $PSScriptRoot "bloques\esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Editor) { $Editor = Join-Path $raiz "LobosWolfos2000.exe" }
if (-not (Test-Path -LiteralPath $Editor)) {
    Write-Host "[ERROR] no existe el editor: $Editor" -ForegroundColor Red
    exit 2
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path

$tmp = Join-Path $env:TEMP ("lw2000-bloques-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

# Categorias: 0 eventos, 1 control, 2 movimiento, 3 datos, 4 operadores.
# Fila 0 de la paleta esta en y=212 con la geometria nominal de 1200x700.
$arrastrarUno = "2:100,212;400,300"
$arrastrarRanura = "2:100,212;400,300|4:100,212;518,313"
$arrastrarAnidar = "1:100,212;400,300|2:100,212;430,345|2:100,212;430,355"
$script43 = "0:100,212;300,200|1:100,212;300,280|2:100,212;316,330|2:100,212;300,420|3:100,212;300,480"

Write-Host ""
Write-Host "Lienzo de bloques" -ForegroundColor Cyan
Write-Host ("  editor: {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Get-CampoTexto([string]$texto, [string]$clave) {
    $m = [regex]::Match($texto, "(?:^|\s)" + [regex]::Escape($clave) + "=([^\s]+)")
    if ($m.Success) { return $m.Groups[1].Value }
    return ""
}

function Get-VolcarNormalizado([string]$ruta, [string]$dir) {
    $texto = Get-TextoNormalizado $ruta
    if ($dir) { $texto = $texto.Replace($dir, "<CASO>") }
    return $texto
}

function Get-Pixeles([string]$bmp) {
    $imagen = [System.Drawing.Bitmap]::FromFile($bmp)
    try {
        $puntos = @("100,170", "100,212", "300,160", "500,300", "1100,400")
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

function Comprobar-Artefactos([string]$nombre, [string]$dir, [string]$bmp,
                              [string]$dump, [hashtable]$campos, $problemas) {
    if (-not (Test-Path -LiteralPath $dump)) {
        [void]$problemas.Add("falta el volcado")
        return
    }
    if (-not (Test-Path -LiteralPath $bmp)) {
        [void]$problemas.Add("falta el BMP")
        return
    }

    $texto = Get-VolcarNormalizado $dump $dir
    foreach ($clave in ($campos.Keys | Sort-Object)) {
        $valor = Get-CampoTexto $texto $clave
        $quiero = [string]$campos[$clave]
        if ($valor -ne $quiero) {
            [void]$problemas.Add(("volcado: {0}={1} (esperado {2})" -f `
                                  $clave, $valor, $quiero))
        }
    }

    $hash = Get-Sha256 $bmp
    $pixeles = Get-Pixeles $bmp
    $rutaVolcar = Join-Path $esperadoDir ($nombre + ".volcar")
    $rutaHash = Join-Path $esperadoDir ($nombre + ".bmp.sha256")
    $rutaPixeles = Join-Path $esperadoDir ($nombre + ".pixeles")

    if ($Actualizar) {
        Guardar-TextoNormalizado $rutaVolcar $texto
        Guardar-TextoNormalizado $rutaHash $hash
        Guardar-TextoNormalizado $rutaPixeles $pixeles
        return
    }

    if (-not (Test-Path -LiteralPath $rutaVolcar)) {
        [void]$problemas.Add("falta " + $nombre + ".volcar (usa -Actualizar)")
    } elseif ((Get-TextoNormalizado $rutaVolcar) -ne $texto) {
        [void]$problemas.Add("el volcado no coincide")
        Mostrar-Diferencia ((Get-TextoNormalizado $rutaVolcar) -split "`n") `
                           ($texto -split "`n") 4
    }
    if (-not (Test-Path -LiteralPath $rutaHash)) {
        [void]$problemas.Add("falta " + $nombre + ".bmp.sha256 (usa -Actualizar)")
    } elseif (((Get-TextoNormalizado $rutaHash).Trim().ToLowerInvariant()) -ne $hash) {
        [void]$problemas.Add("el BMP no coincide")
    }
    if (-not (Test-Path -LiteralPath $rutaPixeles)) {
        [void]$problemas.Add("falta " + $nombre + ".pixeles (usa -Actualizar)")
    } elseif ((Get-TextoNormalizado $rutaPixeles) -ne $pixeles) {
        [void]$problemas.Add("los pixeles no coinciden")
        [void]$problemas.Add("  obtenido: " + $pixeles)
    }
}

function Ejecutar-Editor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Editor $argumentos $dir
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

Ejecutar-Caso "entrada" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "entrada.bmp"
    $dump = Join-Path $dir "entrada.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "entrada" $dir $bmp $dump @{
        vista = "4"; estudio = "1"; pestana_estudio = "4";
        lb_bloques = "0"; lb_sel = "0"; lb_pan = "0,0"
    } $problemas
}

Ejecutar-Caso "arrastrar" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "arrastrar.bmp"
    $dump = Join-Path $dir "arrastrar.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--arrastre-bloque", $arrastrarUno, `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "arrastrar" $dir $bmp $dump @{
        vista = "4"; pestana_estudio = "4"; bloques = "1";
        lb_bloques = "1"; lb_sel = "1000"; lb_categoria = "2"
    } $problemas
}

Ejecutar-Caso "seleccion_id" {
    param($dir, $problemas)
    $gestos = $arrastrarUno + "|2:100,212;500,420"
    $bmp = Join-Path $dir "seleccion_id.bmp"
    $dump = Join-Path $dir "seleccion_id.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--arrastre-bloque", $gestos, `
                                     "--bloque", "1000", `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "seleccion_id" $dir $bmp $dump @{
        lb_bloques = "2"; lb_sel = "1000"
    } $problemas
}

Ejecutar-Caso "ranura" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "ranura.bmp"
    $dump = Join-Path $dir "ranura.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--arrastre-bloque", $arrastrarRanura, `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "ranura" $dir $bmp $dump @{
        lb_bloques = "2"; lb_sel = "1001"; lb_ranuras = "1"
    } $problemas
}

Ejecutar-Caso "anidar" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "anidar.bmp"
    $dump = Join-Path $dir "anidar.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--arrastre-bloque", $arrastrarAnidar, `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "anidar" $dir $bmp $dump @{
        lb_bloques = "3"; lb_cuerpo = "2"; lb_sel = "1002"
    } $problemas
}

Ejecutar-Caso "deshacer" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "deshacer.bmp"
    $dump = Join-Path $dir "deshacer.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--arrastre-bloque", $arrastrarUno, `
                                     "--deshacer-bloque", `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "deshacer" $dir $bmp $dump @{
        lb_bloques = "0"; lb_sel = "0"; bloques = "0"
    } $problemas
}

Ejecutar-Caso "teclado" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "teclado.bmp"
    $dump = Join-Path $dir "teclado.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--clics", "100,82", `
                                     "--teclas", "\n", `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "teclado" $dir $bmp $dump @{
        lb_bloques = "1"; lb_categoria = "1"; lb_foco = "0"
    } $problemas
}

Ejecutar-Caso "guardar" {
    param($dir, $problemas)
    $logic = Join-Path $dir "script.logic3"
    $bmp = Join-Path $dir "guardar.bmp"
    $dump = Join-Path $dir "guardar.txt"
    $codigo = Ejecutar-Editor $dir @("--bloques", "--sin-ventanas", `
                                     "--arrastre-bloque", $script43, `
                                     "--guardar-logic3", $logic, `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "guardar" $dir $bmp $dump @{
        bloques = "5"; lb_bloques = "5"; lb_cuerpo = "1"
    } $problemas

    $contenido = Get-TextoNormalizado $logic
    $numBloques = @($contenido -split "`n" | Where-Object { $_ -like "bloque *" }).Count
    if ($numBloques -ne 5) {
        [void]$problemas.Add("el LOGIC3 guardado tiene $numBloques bloques (esperado 5)")
    }
    $rutaLogic = Join-Path $esperadoDir "guardar.logic3"
    if ($Actualizar) {
        Guardar-TextoNormalizado $rutaLogic $contenido
    } elseif (-not (Test-Path -LiteralPath $rutaLogic)) {
        [void]$problemas.Add("falta guardar.logic3 (usa -Actualizar)")
    } elseif ((Get-TextoNormalizado $rutaLogic) -ne $contenido) {
        [void]$problemas.Add("el LOGIC3 guardado no coincide")
        Mostrar-Diferencia ((Get-TextoNormalizado $rutaLogic) -split "`n") `
                           ($contenido -split "`n") 4
    }

    $bmp2 = Join-Path $dir "reabrir.bmp"
    $dump2 = Join-Path $dir "reabrir.txt"
    $codigo2 = Ejecutar-Editor $dir @("--abrir-logic3", $logic, "--bloques", `
                                      "--sin-ventanas", `
                                      "--captura", $bmp2, "--volcar", $dump2)
    if ($codigo2 -ne 0) { [void]$problemas.Add("el editor devolvio $codigo2 al reabrir") }
    Comprobar-Artefactos "reabrir" $dir $bmp2 $dump2 @{
        logic3 = "1"; bloques = "5"; lb_bloques = "5"; lb_cuerpo = "1"
    } $problemas
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
        $texto = Get-VolcarNormalizado $dump $dir
        $valor = Get-CampoTexto $texto "menu"
        $vista = Get-CampoTexto $texto "vista"
        $bloques = Get-CampoTexto $texto "bloques"
        if ($valor -ne ([string]$menu)) {
            [void]$problemas.Add(("menu={0} (esperado {1})" -f $valor, $menu))
        }
        if ($vista -ne "0") {
            [void]$problemas.Add(("vista={0} en el menu {1}" -f $vista, $menu))
        }
        if ($texto -match "lb_bloques=") {
            [void]$problemas.Add("campos del lienzo fuera de la pestana Bloques")
        }
        $lineas += ("menu={0} vista={1} bloques={2}" -f $valor, $vista, $bloques)
    }
    $ruta = Join-Path $esperadoDir "menu_indices.volcar"
    $texto = ($lineas -join "`n")
    if ($Actualizar) {
        Guardar-TextoNormalizado $ruta $texto
    } elseif (-not (Test-Path -LiteralPath $ruta)) {
        [void]$problemas.Add("falta menu_indices.volcar (usa -Actualizar)")
    } elseif ((Get-TextoNormalizado $ruta) -ne $texto) {
        [void]$problemas.Add("los indices de menu no coinciden")
        Mostrar-Diferencia ((Get-TextoNormalizado $ruta) -split "`n") `
                           ($texto -split "`n") 4
    }
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("Bloques: {0} casos, todos correctos" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("Bloques: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
