# perfil.ps1 - suite de la fase 42: perfilador, presupuestos, cache,
# culling, batching, estres de 200 entidades/5000 bloques y memoria.
#
# Congela instrucciones, blits, avisos y memoria. Los microsegundos se
# sustituyen por <us> porque dependen de la maquina; lo que se comprueba
# es que el perfilador los mide y que el resto es determinista.
#
# Casos:
#   basico      dos scripts con tecla: instrucciones y blits por script
#   sin_flag    sin --perfil no aparece ninguna salida nueva
#   presupuesto con --presupuesto 1 avisa, degrada y no bloquea
#   estres      200 entidades y 5000 bloques con trajes y cache
#
# Uso:
#   pruebas\perfil.ps1                     pasa la suite
#   pruebas\perfil.ps1 -Filtro estres*     solo los casos que encajan
#   pruebas\perfil.ps1 -Actualizar         reescribe los patrones

[CmdletBinding()]
param(
    [string]$Motor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$casosDir = Join-Path $PSScriptRoot "perfil"
$esperadoDir = Join-Path $casosDir "esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
if (-not (Test-Path -LiteralPath $Motor)) {
    Write-Host "[ERROR] no existe el motor: $Motor" -ForegroundColor Red
    exit 2
}
$Motor = (Resolve-Path -LiteralPath $Motor).Path

$tmp = Join-Path $env:TEMP ("lw2000-perfil-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Perfilador, presupuestos y estres (fase 42)" -ForegroundColor Cyan
Write-Host ("  motor:  {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Ejecutar-Motor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Motor $argumentos $dir
}

# Sustituye los tiempos por una marca estable. Las instrucciones, los
# blits y la memoria se dejan tal cual: son deterministas.
function Normalizar-Perfil([string]$ruta) {
    $t = Get-TextoNormalizado $ruta
    $t = [regex]::Replace($t, "us totales \d+", "us totales <us>")
    $t = [regex]::Replace($t, "us medios \d+", "us medios <us>")
    $t = [regex]::Replace($t, "us maximos \d+", "us maximos <us>")
    $t = [regex]::Replace($t, "microsegundos \d+", "microsegundos <us>")
    $t = [regex]::Replace($t, "instrucciones (\d+) us \d+", "instrucciones `$1 us <us>")
    $t = [regex]::Replace($t, "excedido en el fotograma (\d+) \d+ us",
                          "excedido en el fotograma `$1 <us> us")
    $t = [regex]::Replace($t, "(?m)^memoria: blob \d+", "memoria: blob <blob>")
    $t = [regex]::Replace($t, "runtime \d+", "runtime <runtime>")
    # El numero de avisos depende del tiempo real de cada fotograma.
    $t = [regex]::Replace($t, "(?m)^(perfil: presupuesto \d+ avisos) \d+",
                          "`$1 <avisos>")
    $t = [regex]::Replace($t, "imagenes \d+", "imagenes <bytes>")
    return $t
}

function Comprobar-Texto([string]$nombre, [string]$ruta, [string]$extension,
                         $problemas) {
    $obtenido = Normalizar-Perfil $ruta
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

function Comprobar-Bmp([string]$nombre, [string]$bmp, $problemas) {
    if (-not (Test-Path -LiteralPath $bmp)) {
        [void]$problemas.Add("falta el BMP")
        return
    }
    $hash = Get-Sha256 $bmp
    $patron = Join-Path $esperadoDir ($nombre + ".bmp.sha256")
    if ($Actualizar) {
        Guardar-TextoNormalizado $patron $hash
    } elseif (-not (Test-Path -LiteralPath $patron)) {
        [void]$problemas.Add("falta " + $nombre + ".bmp.sha256 (usa -Actualizar)")
    } elseif (((Get-TextoNormalizado $patron).Trim().ToLowerInvariant()) -ne $hash) {
        [void]$problemas.Add("el BMP no coincide")
    }
}

function Comprobar-Lineas([string]$ruta, [string[]]$lineas, $problemas) {
    $contenido = (Normalizar-Perfil $ruta) -split "`n"
    foreach ($linea in $lineas) {
        if ($contenido -notcontains $linea) {
            [void]$problemas.Add("falta la linea: $linea")
        }
    }
}

function Comprobar-Ausentes([string]$ruta, [string[]]$palabras, $problemas) {
    if (-not (Test-Path -LiteralPath $ruta)) { return }
    $contenido = Get-TextoNormalizado $ruta
    foreach ($palabra in $palabras) {
        if ($contenido.Contains($palabra)) {
            [void]$problemas.Add("no debia aparecer: $palabra")
        }
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
    $segundos = & $cuerpo $dir $problemas
    if ($problemas.Count -eq 0) {
        Write-Host ("  [{0,3}] {1,-12} OK ({2:0.0}s)" -f $script:indice, $nombre, $segundos) -ForegroundColor Green
    } else {
        $script:fallos++
        Write-Host ("  [{0,3}] {1,-12} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
        foreach ($p in $problemas) {
            Write-Host ("        - {0}" -f $p) -ForegroundColor Red
        }
    }
}

$teclas = Join-Path $raiz "pruebas\eventos\teclas.logic3"
$capas = Join-Path $raiz "pruebas\render\capas.logic3"

Ejecutar-Caso "basico" {
    param($dir, $problemas)
    $traza = Join-Path $dir "basico.traza"
    $volcado = Join-Path $dir "basico.perfil"
    $estado = Join-Path $dir "basico.estado"
    $cron = [Diagnostics.Stopwatch]::StartNew()
    $codigo = Ejecutar-Motor $dir @("--logic3", $teclas, "--fotogramas", "10",
        "--tecla", "derecha", "--perfil", "--traza", $traza,
        "--volcar", $estado, "--volcar-perfil", $volcado)
    $cron.Stop()
    if ($codigo -ne 0) { [void]$problemas.Add("basico devolvio $codigo") }
    Comprobar-Texto "basico" $volcado ".perfil" $problemas
    Comprobar-Texto "basico" $traza ".traza" $problemas
    Comprobar-Lineas $volcado @(
        "script 0 evento 1 tipo 3 instrucciones 4 microsegundos <us> blits 0 disparos 1",
        "script 2 evento 3 tipo 3 instrucciones 40 microsegundos <us> blits 0 disparos 10") $problemas
    return $cron.Elapsed.TotalSeconds
}

Ejecutar-Caso "sin_flag" {
    param($dir, $problemas)
    $traza = Join-Path $dir "sinflag.traza"
    $volcado = Join-Path $dir "sinflag.perfil"
    $estado = Join-Path $dir "sinflag.estado"
    $cron = [Diagnostics.Stopwatch]::StartNew()
    $codigo = Ejecutar-Motor $dir @("--logic3", $teclas, "--fotogramas", "6",
        "--tecla", "derecha", "--traza", $traza, "--volcar", $estado)
    $cron.Stop()
    if ($codigo -ne 0) { [void]$problemas.Add("sin_flag devolvio $codigo") }
    Comprobar-Ausentes $traza @("perfil ", "presupuesto:") $problemas
    if (Test-Path -LiteralPath $volcado) {
        [void]$problemas.Add("sin --perfil no se debia escribir el volcado")
    }
    return $cron.Elapsed.TotalSeconds
}

Ejecutar-Caso "presupuesto" {
    param($dir, $problemas)
    $traza = Join-Path $dir "presupuesto.traza"
    $volcado = Join-Path $dir "presupuesto.perfil"
    $estado = Join-Path $dir "presupuesto.estado"
    $cron = [Diagnostics.Stopwatch]::StartNew()
    $codigo = Ejecutar-Motor $dir @("--logic3", $teclas, "--fotogramas", "12",
        "--tecla", "derecha", "--presupuesto", "1", "--traza", $traza,
        "--volcar", $estado, "--volcar-perfil", $volcado)
    $cron.Stop()
    if ($codigo -ne 0) { [void]$problemas.Add("presupuesto devolvio $codigo") }
    Comprobar-Texto "presupuesto" $volcado ".perfil" $problemas
    $lineas = Get-TextoNormalizado $traza
    if (-not $lineas.Contains("presupuesto: excedido en el fotograma 1")) {
        [void]$problemas.Add("no aviso del presupuesto")
    }
    if ($lineas -notmatch "limite 1") {
        [void]$problemas.Add("no degradation los blits")
    }
    return $cron.Elapsed.TotalSeconds
}

Ejecutar-Caso "estres" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "estres.bmp"
    $traza = Join-Path $dir "estres.traza"
    $volcado = Join-Path $dir "estres.perfil"
    $cron = [Diagnostics.Stopwatch]::StartNew()
    $codigo = Ejecutar-Motor $dir @("--logic3", $capas, "--fotogramas", "30",
        "--estres", "200,5000", "--perfil", "--captura", $bmp,
        "--traza", $traza, "--volcar-perfil", $volcado)
    $cron.Stop()
    if ($codigo -ne 0) { [void]$problemas.Add("estres devolvio $codigo") }
    if ($cron.Elapsed.TotalSeconds -gt 20) {
        [void]$problemas.Add("la carga de estres tardo demasiado")
    }
    Comprobar-Bmp "estres" $bmp $problemas
    Comprobar-Texto "estres" $volcado ".perfil" $problemas
    Comprobar-Lineas $volcado @(
        "perfil: fotogramas 30 us totales <us> us medios <us> us maximos <us>",
        "memoria: mapa celdas 5000 entidades 203 tiles 1") $problemas
    $texto = Get-TextoNormalizado $volcado
    if ($texto -notmatch "runtime \d+") { [void]$problemas.Add("falta el informe de runtime") }
    $trazaTexto = Get-TextoNormalizado $traza
    if ($trazaTexto -notmatch "blits (\d+)") { [void]$problemas.Add("no hay blits en la traza") }
    if ($trazaTexto -notmatch "cache \d+/\d+") { [void]$problemas.Add("no hay cache en la traza") }
    return $cron.Elapsed.TotalSeconds
}

Write-Host ""
if ($fallos -eq 0) {
    Write-Host ("Perfil: {0} casos, todo OK" -f $total) -ForegroundColor Green
    exit 0
}
Write-Host ("Perfil: {0} casos, {1} con fallos" -f $total, $fallos) -ForegroundColor Red
exit 1
