# datos.ps1 - suite de la fase 35: variables, listas, trajes y sonidos.
#
# Congela el volcado propio de la fase, el hash del BMP y tres sondas de
# pixel. Ningun caso usa las rutas de las doce suites anteriores y ninguna
# salida nueva aparece sin una flag nueva.
#
# Casos:
#   lista10       crear 10 celdas, editar una, guardar y recargar intacta
#   variables     observaciones globales y de objeto en el sidecar
#   traje         importar PNG con recorte, pivote y dos cuadros
#   sonido        importar WAV valido y probar reproduccion
#   validacion    rechazar un WAV a 44100 Hz
#   borrar        confirmacion y sustitucion de referencias
#   menu_indices  ARCHIVO=1, MODO=2 y AYUDA=9 siguen en su sitio
#
# Uso:
#   pruebas\datos.ps1                       pasa la suite
#   pruebas\datos.ps1 -Filtro lista*        solo los casos que encajan
#   pruebas\datos.ps1 -Actualizar           reescribe los patrones

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
$esperadoDir = Join-Path $PSScriptRoot "datos\esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Editor) { $Editor = Join-Path $raiz "LobosWolfos2000.exe" }
if (-not (Test-Path -LiteralPath $Editor)) {
    Write-Host "[ERROR] no existe el editor: $Editor" -ForegroundColor Red
    exit 2
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path

$tmp = Join-Path $env:TEMP ("lw2000-datos-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

$clicsCrear = "52,85;148,85"

Write-Host ""
Write-Host "Datos, trajes y sonidos" -ForegroundColor Cyan
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
        $puntos = @("100,680", "500,401", "1100,680")
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
        [void]$problemas.Add("falta el volcado de datos")
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
                           ($texto -split "`n") 8
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
    }
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

function Ejecutar-Editor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Editor $argumentos $dir
}

function New-Png([string]$ruta) {
    $bmp = New-Object System.Drawing.Bitmap 64, 32
    $gfx = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        $gfx.Clear([System.Drawing.Color]::FromArgb(255, 30, 60, 90))
        $pincel = New-Object System.Drawing.SolidBrush `
                         ([System.Drawing.Color]::FromArgb(255, 220, 170, 60))
        $gfx.FillRectangle($pincel, 4, 4, 24, 24)
        $pincel.Dispose()
        $bmp.Save($ruta, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $gfx.Dispose()
        $bmp.Dispose()
    }
}

function New-Wav([string]$ruta, [int]$frecuencia) {
    $nums = [byte[]](0, 64, 128, 192, 255, 192, 128, 64)
    $muestra = 512
    $datos = New-Object byte[] $muestra
    for ($i = 0; $i -lt $muestra; $i++) { $datos[$i] = $nums[$i % 8] }
    $ascii = New-Object System.Text.ASCIIEncoding
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    $bw.Write($ascii.GetBytes("RIFF"))
    $bw.Write([int](36 + $muestra))
    $bw.Write($ascii.GetBytes("WAVE"))
    $bw.Write($ascii.GetBytes("fmt "))
    $bw.Write([int]16)
    $bw.Write([System.Int16]1)
    $bw.Write([System.Int16]1)
    $bw.Write([int]$frecuencia)
    $bw.Write([int]($frecuencia * 2))
    $bw.Write([System.Int16]2)
    $bw.Write([System.Int16]16)
    $bw.Write($ascii.GetBytes("data"))
    $bw.Write([int]$muestra)
    $bw.Write($datos)
    $bw.Flush()
    [System.IO.File]::WriteAllBytes($ruta, $ms.ToArray())
    $bw.Dispose()
    $ms.Dispose()
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

Ejecutar-Caso "lista10" {
    param($dir, $problemas)
    $logic = Join-Path $dir "proyecto.logic3"
    $bmp = Join-Path $dir "lista10.bmp"
    $dump = Join-Path $dir "lista10.txt"
    $datos = Join-Path $dir "lista10.datos"
    $elementos = "c0;c1;c2;c3;c4;c5;c6;c7;c8;c9"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--datos", `
        "--datos-lista", "inventario|10|texto|$elementos", `
        "--datos-editar", "0,3,EDITADO", `
        "--datos-guardar", $logic, "--captura", $bmp, `
        "--volcar-datos", $datos)
    if ($codigo -ne 0) { [void]$problemas.Add("guardar devolvio $codigo") }
    $bmp2 = Join-Path $dir "recargada.bmp"
    $dump2 = Join-Path $dir "recargada.txt"
    $datos2 = Join-Path $dir "recargada.datos"
    $codigo = Ejecutar-Editor $dir @("--abrir-logic3", $logic, "--sin-ventanas", `
        "--datos", "--captura", $bmp2, "--volcar", $dump2, `
        "--volcar-datos", $datos2)
    if ($codigo -ne 0) { [void]$problemas.Add("recargar devolvio $codigo") }
    $texto = Get-TextoNormalizado $logic
    if ($texto -notmatch 'lista global "?inventario"? 10 texto') {
        [void]$problemas.Add("el LOGIC3 no conserva la lista")
    }
    if ($texto -notmatch 'EDITADO') {
        [void]$problemas.Add("el LOGIC3 no conserva la celda editada")
    }
    if (-not (Test-Path -LiteralPath ($logic -replace '\.logic3$', '.fase35'))) {
        [void]$problemas.Add("falta el archivo lateral")
    }
    Comprobar-Artefactos "lista10" $dir $bmp $datos `
        @{ pestana = 2; seccion = 2; seleccion = 0 } $problemas
    $p = Comprobar-Texto "lista10_recargada" $datos2 ".volcar"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "variables" {
    param($dir, $problemas)
    $logic = Join-Path $dir "proyecto.logic3"
    $bmp = Join-Path $dir "variables.bmp"
    $datos = Join-Path $dir "variables.datos"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--datos", `
        "--datos-variable", "0|salud|entero|5|Vidas del lobo", `
        "--datos-variable", "1|energia|entero|3|Energia", `
        "--datos-guardar", $logic, "--captura", $bmp, `
        "--volcar-datos", $datos)
    if ($codigo -ne 0) { [void]$problemas.Add("guardar devolvio $codigo") }
    $bmp2 = Join-Path $dir "variables_recargadas.bmp"
    $datos2 = Join-Path $dir "variables_recargadas.datos"
    $codigo = Ejecutar-Editor $dir @("--abrir-logic3", $logic, "--sin-ventanas", `
        "--datos", "--captura", $bmp2, "--volcar-datos", $datos2)
    if ($codigo -ne 0) { [void]$problemas.Add("recargar devolvio $codigo") }
    Comprobar-Artefactos "variables" $dir $bmp $datos `
        @{ pestana = 2; seccion = 1; error = "-" } $problemas
    $p = Comprobar-Texto "variables_recargadas" $datos2 ".volcar"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "traje" {
    param($dir, $problemas)
    $png = Join-Path $dir "lobo.png"
    New-Png $png
    $logic = Join-Path $dir "proyecto.logic3"
    $bmp = Join-Path $dir "traje.bmp"
    $datos = Join-Path $dir "traje.datos"
    $importar = "$png|0,0,64,32|16,16|2,32,32"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--datos", `
        "--traje-importar", $importar, `
        "--datos-guardar", $logic, "--captura", $bmp, `
        "--volcar-datos", $datos)
    if ($codigo -ne 0) { [void]$problemas.Add("importar devolvio $codigo") }
    if (-not (Test-Path -LiteralPath (Join-Path $dir "trajes\lobo.png"))) {
        [void]$problemas.Add("no se copio el PNG en trajes")
    }
    $bmp2 = Join-Path $dir "traje_recargada.bmp"
    $datos2 = Join-Path $dir "traje_recargado.datos"
    $codigo = Ejecutar-Editor $dir @("--abrir-logic3", $logic, "--sin-ventanas", `
        "--datos", "--captura", $bmp2, "--volcar-datos", $datos2)
    if ($codigo -ne 0) { [void]$problemas.Add("recargar devolvio $codigo") }
    Comprobar-Artefactos "traje" $dir $bmp $datos `
        @{ pestana = 2; seccion = 1; error = "-" } $problemas
    $p = Comprobar-Texto "traje_recargado" $datos2 ".volcar"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "sonido" {
    param($dir, $problemas)
    $wav = Join-Path $dir "paso.wav"
    New-Wav $wav 22050
    $logic = Join-Path $dir "proyecto.logic3"
    $bmp = Join-Path $dir "sonido.bmp"
    $datos = Join-Path $dir "sonido.datos"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--sonidos", `
        "--sonido-importar", "paso|$wav", "--sonido-probar", "0", `
        "--datos-guardar", $logic, "--captura", $bmp, `
        "--volcar-datos", $datos)
    if ($codigo -ne 0) { [void]$problemas.Add("importar devolvio $codigo") }
    if (-not (Test-Path -LiteralPath (Join-Path $dir "sonidos\paso.wav"))) {
        [void]$problemas.Add("no se copio el WAV en sonidos")
    }
    $bmp2 = Join-Path $dir "sonido_recargada.bmp"
    $datos2 = Join-Path $dir "sonido_recargado.datos"
    $codigo = Ejecutar-Editor $dir @("--abrir-logic3", $logic, "--sin-ventanas", `
        "--sonidos", "--captura", $bmp2, "--volcar-datos", $datos2)
    if ($codigo -ne 0) { [void]$problemas.Add("recargar devolvio $codigo") }
    Comprobar-Artefactos "sonido" $dir $bmp $datos `
        @{ pestana = 3; seccion = 3; error = "-" } $problemas
    $p = Comprobar-Texto "sonido_recargado" $datos2 ".volcar"
    if ($p) { [void]$problemas.Add($p) }
}

Ejecutar-Caso "validacion" {
    param($dir, $problemas)
    $wav = Join-Path $dir "malo.wav"
    New-Wav $wav 44100
    $bmp = Join-Path $dir "validacion.bmp"
    $datos = Join-Path $dir "validacion.datos"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--sonidos", `
        "--sonido-importar", "malo|$wav", `
        "--captura", $bmp, "--volcar-datos", $datos)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "validacion" $dir $bmp $datos `
        @{ pestana = 3; seccion = 0 } $problemas
}

Ejecutar-Caso "borrar" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "borrar.bmp"
    $datos = Join-Path $dir "borrar.datos"
    $pendiente = Join-Path $dir "pendiente.datos"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--datos", `
        "--datos-variable", "0|salud|entero|5|Vidas", `
        "--datos-variable", "0|energia|entero|3|Energia", `
        "--datos-borrar", "0,0,salud_nueva", `
        "--captura", $bmp, "--volcar-datos", $pendiente)
    if ($codigo -ne 0) { [void]$problemas.Add("borrado pendiente devolvio $codigo") }
    $textoPendiente = Get-TextoNormalizado $pendiente
    if ($textoPendiente -notmatch "confirmar=1000") {
        [void]$problemas.Add("no se pidio confirmacion")
    }
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--datos", `
        "--datos-variable", "0|salud|entero|5|Vidas", `
        "--datos-variable", "0|energia|entero|3|Energia", `
        "--datos-borrar", "0,0,salud_nueva", "--confirmar", `
        "--captura", $bmp, "--volcar-datos", $datos)
    if ($codigo -ne 0) { [void]$problemas.Add("borrado confirmado devolvio $codigo") }
    Comprobar-Artefactos "borrar" $dir $bmp $datos `
        @{ pestana = 2; seccion = 0; confirmar = -1 } $problemas
}

Ejecutar-Caso "menu_indices" {
    param($dir, $problemas)
    $lineas = @()
    foreach ($menu in @(1, 2, 9)) {
        $bmp = Join-Path $dir ("menu" + $menu + ".bmp")
        $dump = Join-Path $dir ("menu" + $menu + ".txt")
        $volcarDatos = Join-Path $dir ("menu" + $menu + ".datos")
        $codigo = Ejecutar-Editor $dir @("--menu", ([string]$menu), `
                                         "--volcar-datos", $volcarDatos, `
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
    Write-Host ("Fase 35: {0} casos, todos correctos" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("Fase 35: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
