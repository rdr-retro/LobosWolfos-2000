# estudio.ps1 - suite de la vista Estudio (fase 31).
#
# Congela la captura, el volcado y cinco sondas de pixel de cada caso.
# Las sondas comprueban los limites de las tres columnas:
#   interior izquierdo, separador, interior central, separador, interior derecho.
# Las rutas temporales se sustituyen por <CASO> para que los patrones sean
# estables entre maquinas.
#
# Casos:
#   entrada          layout inicial 200/740/260 sin proyecto LOGIC3
#   crear            dos escenas y tres objetos desde los botones
#   borrar           crea y borra hasta dejar una escena sin objetos
#   seleccion_id     selecciona un objeto por `--objeto`
#   seleccion_clic   cambia la seleccion al hacer clic en una fila
#   guardar          guarda el proyecto y lo reabre con los mismos contadores
#   menu_modo        desplegable MODO con la fila nueva
#   menu_indices     ARCHIVO=1, MODO=2 y AYUDA=9 siguen en su sitio
#
# Uso:
#   pruebas\estudio.ps1                      pasa la suite
#   pruebas\estudio.ps1 -Filtro crear*       solo los casos que encajan
#   pruebas\estudio.ps1 -Actualizar          reescribe los patrones

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
$esperadoDir = Join-Path $PSScriptRoot "estudio\esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Editor) { $Editor = Join-Path $raiz "LobosWolfos2000.exe" }
if (-not (Test-Path -LiteralPath $Editor)) {
    Write-Host "[ERROR] no existe el editor: $Editor" -ForegroundColor Red
    exit 2
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path

$tmp = Join-Path $env:TEMP ("lw2000-estudio-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

$clicsCrear = "52,85;52,85;148,85;148,85;148,85"

Write-Host ""
Write-Host "Vista Estudio" -ForegroundColor Cyan
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
        $puntos = @("100,680", "199,400", "500,401", "940,400", "1100,680")
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

function Nuevo-Proyecto([string]$dir) {
    $bmp = Join-Path $dir "preparar.bmp"
    $dump = Join-Path $dir "preparar.txt"
    $logic = Join-Path $dir "proyecto.logic3"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
                                     "--clics", $clicsCrear, `
                                     "--guardar-logic3", $logic, `
                                     "--captura", $bmp, "--volcar", $dump)
    return @($codigo, $logic)
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
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "entrada" $dir $bmp $dump @{
        vista = "4"; estudio = "1"; paneles = "200,740,260";
        logic3 = "0"; zoom = "10"; ruta_estudio = "-"
    } $problemas
}

Ejecutar-Caso "crear" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "crear.bmp"
    $dump = Join-Path $dir "crear.txt"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
                                     "--clics", $clicsCrear, `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "crear" $dir $bmp $dump @{
        vista = "4"; estudio = "1"; paneles = "200,740,260";
        logic3 = "1"; escenas = "2"; objetos = "3"; eventos = "6";
        sel_escena = "2"; sel_objeto = "3"; ruta_estudio = "-"
    } $problemas
}

Ejecutar-Caso "borrar" {
    param($dir, $problemas)
    $clics = $clicsCrear + ";148,111;148,111;148,111;148,111"
    $bmp = Join-Path $dir "borrar.bmp"
    $dump = Join-Path $dir "borrar.txt"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
                                     "--clics", $clics, `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "borrar" $dir $bmp $dump @{
        vista = "4"; estudio = "1"; logic3 = "1"; escenas = "1";
        objetos = "0"; eventos = "0"; sel_escena = "1"; sel_objeto = "0"
    } $problemas
}

Ejecutar-Caso "seleccion_id" {
    param($dir, $problemas)
    $preparado = Nuevo-Proyecto $dir
    if ($preparado[0] -ne 0) {
        [void]$problemas.Add("no se pudo preparar el proyecto")
        return
    }
    $bmp = Join-Path $dir "seleccion_id.bmp"
    $dump = Join-Path $dir "seleccion_id.txt"
    $codigo = Ejecutar-Editor $dir @("--abrir-logic3", $preparado[1], `
                                     "--estudio", "--objeto", "2", `
                                     "--sin-ventanas", `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "seleccion_id" $dir $bmp $dump @{
        vista = "4"; estudio = "1"; escenas = "2"; objetos = "3";
        sel_escena = "2"; sel_objeto = "2"; ruta_estudio = "<CASO>\proyecto.logic3"
    } $problemas
}

Ejecutar-Caso "seleccion_clic" {
    param($dir, $problemas)
    $preparado = Nuevo-Proyecto $dir
    if ($preparado[0] -ne 0) {
        [void]$problemas.Add("no se pudo preparar el proyecto")
        return
    }
    $bmp = Join-Path $dir "seleccion_clic.bmp"
    $dump = Join-Path $dir "seleccion_clic.txt"
    $codigo = Ejecutar-Editor $dir @("--abrir-logic3", $preparado[1], `
                                     "--estudio", "--objeto", "2", `
                                     "--clics", "52,134", "--sin-ventanas", `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "seleccion_clic" $dir $bmp $dump @{
        vista = "4"; estudio = "1"; escenas = "2"; objetos = "3";
        sel_escena = "1"; sel_objeto = "0"; ruta_estudio = "<CASO>\proyecto.logic3"
    } $problemas
}

Ejecutar-Caso "guardar" {
    param($dir, $problemas)
    $preparado = Nuevo-Proyecto $dir
    if ($preparado[0] -ne 0) {
        [void]$problemas.Add("no se pudo guardar el proyecto")
        return
    }
    $logic = $preparado[1]
    $bmp = Join-Path $dir "guardar.bmp"
    $dump = Join-Path $dir "guardar.txt"
    $codigo = Ejecutar-Editor $dir @("--abrir-logic3", $logic, "--estudio", `
                                     "--sin-ventanas", `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "guardar" $dir $bmp $dump @{
        vista = "4"; estudio = "1"; logic3 = "1"; escenas = "2";
        objetos = "3"; eventos = "6"; ruta_estudio = "<CASO>\proyecto.logic3"
    } $problemas

    $rutaLogic = Join-Path $esperadoDir "guardar.logic3"
    $contenido = Get-TextoNormalizado $logic
    if ($Actualizar) {
        Guardar-TextoNormalizado $rutaLogic $contenido
    } elseif (-not (Test-Path -LiteralPath $rutaLogic)) {
        [void]$problemas.Add("falta guardar.logic3 (usa -Actualizar)")
    } elseif ((Get-TextoNormalizado $rutaLogic) -ne $contenido) {
        [void]$problemas.Add("el LOGIC3 guardado no coincide")
        Mostrar-Diferencia ((Get-TextoNormalizado $rutaLogic) -split "`n") `
                           ($contenido -split "`n") 4
    }
}

Ejecutar-Caso "menu_modo" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "menu_modo.bmp"
    $dump = Join-Path $dir "menu_modo.txt"
    $codigo = Ejecutar-Editor $dir @("--menu", "2", "--sin-ventanas", `
                                     "--captura", $bmp, "--volcar", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "menu_modo" $dir $bmp $dump @{
        menu = "2"; vista = "0"; args = "2|-"
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
        if ($valor -ne ([string]$menu)) {
            [void]$problemas.Add(("menu={0} (esperado {1})" -f $valor, $menu))
        }
        if ($vista -ne "0") {
            [void]$problemas.Add(("vista={0} en el menu {1}" -f $vista, $menu))
        }
        $lineas += ("menu={0} vista={1}" -f $valor, $vista)
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
    Write-Host ("Estudio: {0} casos, todos correctos" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("Estudio: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
