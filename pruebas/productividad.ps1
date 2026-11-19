# productividad.ps1 - suite de la fase 36: productividad del editor.
#
# Congela el volcado propio de la fase, el hash del BMP y tres sondas de
# pixel. Ningun caso usa las rutas de las trece suites anteriores y ninguna
# salida nueva aparece sin una flag nueva.
#
# Casos:
#   duplicar      duplicar un objeto con variables y eventos, sin romper ids
#   portapapeles  copiar y pegar un objeto en otra sesion
#   deshacer      marcar, duplicar y deshacer deja el modelo original
#   buscar        buscar una variable por nombre
#   lote          duplicar tres objetos en una sola accion
#   plantillas    snippets, ayuda contextual y modo principiante
#   metricas      volcado de metricas con escenas, objetos y memoria
#
# Uso:
#   pruebas\productividad.ps1                       pasa la suite
#   pruebas\productividad.ps1 -Filtro duplicar*     solo los casos que encajan
#   pruebas\productividad.ps1 -Actualizar           reescribe los patrones

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
$esperadoDir = Join-Path $PSScriptRoot "productividad\esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Editor) { $Editor = Join-Path $raiz "LobosWolfos2000.exe" }
if (-not (Test-Path -LiteralPath $Editor)) {
    Write-Host "[ERROR] no existe el editor: $Editor" -ForegroundColor Red
    exit 2
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path

$tmp = Join-Path $env:TEMP ("lw2000-productividad-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

$clicsCrear = "52,85;148,85"

Write-Host ""
Write-Host "Productividad del editor" -ForegroundColor Cyan
Write-Host ("  editor: {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Get-CampoTexto([string]$texto, [string]$clave) {
    $m = [regex]::Match($texto, "(?:^|\s)" + [regex]::Escape($clave) + "\s+([^\s]+)")
    if ($m.Success) { return $m.Groups[1].Value }
    return ""
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

function Comprobar-Artefactos([string]$nombre, [string]$bmp,
                              [string]$dump, [hashtable]$campos, $problemas) {
    if (-not (Test-Path -LiteralPath $dump)) {
        [void]$problemas.Add("falta el volcado de productividad")
        return
    }
    if (-not (Test-Path -LiteralPath $bmp)) {
        [void]$problemas.Add("falta el BMP")
        return
    }
    $texto = Get-TextoNormalizado $dump
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

Ejecutar-Caso "duplicar" {
    param($dir, $problemas)
    $logic = Join-Path $dir "proyecto.logic3"
    $bmp = Join-Path $dir "duplicar.bmp"
    $dump = Join-Path $dir "duplicar.prod"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--datos", `
        "--datos-variable", "1|salud|entero|5|Vidas", `
        "--productividad-duplicar", "LoboCopia", `
        "--productividad-marcar", "listo", `
        "--guardar-logic3", $logic, "--captura", $bmp, `
        "--volcar-productividad", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("duplicar devolvio $codigo") }
    $texto = Get-TextoNormalizado $logic
    if ($texto -notmatch 'objeto') { [void]$problemas.Add("no hay objetos en el LOGIC3") }
    Comprobar-Artefactos "duplicar" $bmp $dump `
        @{ objetos = 2; portapapeles = 1; snippets = 4 } $problemas
}

Ejecutar-Caso "portapapeles" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "portapapeles.bmp"
    $dump = Join-Path $dir "portapapeles.prod"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--productividad-copiar", `
        "--productividad-pegar", "Pegado", "--captura", $bmp, `
        "--volcar-productividad", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("pegar devolvio $codigo") }
    Comprobar-Artefactos "portapapeles" $bmp $dump `
        @{ objetos = 2; portapapeles = 1 } $problemas
}

Ejecutar-Caso "deshacer" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "deshacer.bmp"
    $dump = Join-Path $dir "deshacer.prod"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--productividad-marcar", "inicial", `
        "--productividad-duplicar", "Copia", `
        "--productividad-deshacer", "--productividad-rehacer", `
        "--captura", $bmp, "--volcar-productividad", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("deshacer devolvio $codigo") }
    Comprobar-Artefactos "deshacer" $bmp $dump `
        @{ objetos = 2; historial = "3/3" } $problemas
}

Ejecutar-Caso "buscar" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "buscar.bmp"
    $dump = Join-Path $dir "buscar.prod"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--datos", `
        "--datos-variable", "1|salud|entero|5|Vidas", `
        "--productividad-buscar", "salud", `
        "--captura", $bmp, "--volcar-productividad", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("buscar devolvio $codigo") }
    Comprobar-Artefactos "buscar" $bmp $dump `
        @{ busqueda = "salud" } $problemas
}

Ejecutar-Caso "lote" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "lote.bmp"
    $dump = Join-Path $dir "lote.prod"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--productividad-lote", "3|Clon", `
        "--captura", $bmp, "--volcar-productividad", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("lote devolvio $codigo") }
    Comprobar-Artefactos "lote" $bmp $dump `
        @{ objetos = 4 } $problemas
}

Ejecutar-Caso "plantillas" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "plantillas.bmp"
    $dump = Join-Path $dir "plantillas.prod"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--productividad-snippet", "probar|decir hola", `
        "--productividad-ayuda", "mover", `
        "--productividad-principiante", "--captura", $bmp, `
        "--volcar-productividad", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("plantillas devolvio $codigo") }
    Comprobar-Artefactos "plantillas" $bmp $dump `
        @{ snippets = 5; modo = "principiante" } $problemas
}

Ejecutar-Caso "metricas" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "metricas.bmp"
    $dump = Join-Path $dir "metricas.prod"
    $codigo = Ejecutar-Editor $dir @("--estudio", "--sin-ventanas", `
        "--clics", $clicsCrear, "--productividad-marcar", "inicial", `
        "--productividad-avanzado", "--captura", $bmp, `
        "--volcar-productividad", $dump)
    if ($codigo -ne 0) { [void]$problemas.Add("metricas devolvio $codigo") }
    Comprobar-Artefactos "metricas" $bmp $dump `
        @{ escenas = 1; objetos = 1; modo = "avanzado" } $problemas
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("Fase 36: {0} casos, todos correctos" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("Fase 36: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
