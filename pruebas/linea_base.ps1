# linea_base.ps1 - mide y escribe pruebas\linea_base.txt (fase 23).
#
# La linea base es informativa: los hashes de los ejecutables cambian en
# cada compilacion (marca de tiempo del PE), asi que no se usan como
# patron de fallo. Lo que si se congela son las trazas y los BMP de las
# suites ejecutar.ps1 y regresion.ps1.
#
# Uso: powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\linea_base.ps1

[CmdletBinding()]
param([int]$Muestras = 5)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$editor = Join-Path $raiz "LobosWolfos2000.exe"
$motor = Join-Path $raiz "motor\game.exe"
$tmp = Join-Path $env:TEMP ("lw2000-linea-base-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

if (-not (Test-Path -LiteralPath $editor)) { throw "no existe $editor" }
if (-not (Test-Path -LiteralPath $motor)) { throw "no existe $motor" }

function Medir-Tiempos([string]$exe, [string[]]$argumentos) {
    $tiempos = New-Object System.Collections.ArrayList
    for ($i = 0; $i -lt $Muestras; $i++) {
        $reloj = [Diagnostics.Stopwatch]::StartNew()
        $codigo = Ejecutar-Programa $exe $argumentos $raiz
        $reloj.Stop()
        if ($codigo -ne 0) { throw "codigo $codigo" }
        [void]$tiempos.Add($reloj.Elapsed.TotalMilliseconds)
    }
    $m = $tiempos | Measure-Object -Average -Minimum -Maximum
    return ("{0:N0} ms (min {1:N0}, max {2:N0}, {3} muestras)" -f `
            $m.Average, $m.Minimum, $m.Maximum, $Muestras)
}

function Medir-Memoria([string]$exe, [string[]]$argumentos) {
    $partes = @()
    foreach ($a in $argumentos) { $partes += $a }
    $linea = $partes -join ' '
    $p = Start-Process -FilePath $exe -ArgumentList $linea `
                       -WorkingDirectory $raiz -PassThru
    Start-Sleep -Milliseconds 1200
    $memoria = 0
    try {
        $proc = Get-Process -Id $p.Id -ErrorAction Stop
        $memoria = $proc.PeakWorkingSet64
    } catch { }
    try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch { }
    return ("{0:N0} KB" -f ($memoria / 1024))
}

$capturaEditor = Join-Path $tmp "base_editor.bmp"
$volcarEditor = Join-Path $tmp "base_editor.txt"
$capturaMotor = Join-Path $tmp "base_motor.bmp"
$trazaMotor = Join-Path $tmp "base_motor.traza"

$tiempoEditor = Medir-Tiempos $editor @("--captura", $capturaEditor,
    "--sin-ventanas", "--volcar", $volcarEditor)
$tiempoMotor = Medir-Tiempos $motor @("--mapa", "maps\mapa.mav",
    "--fotogramas", "30", "--captura", $capturaMotor, "--traza", $trazaMotor)
$memoriaEditor = Medir-Memoria $editor @("--sin-ventanas")
$memoriaMotor = Medir-Memoria $motor @("--mapa", "maps\mapa.mav")

$commit = ""
Push-Location $raiz
try { $commit = (git rev-parse --short HEAD 2>$null) } finally { Pop-Location }
if (-not $commit) { $commit = "desconocido" }

$editorDatos = Get-Item -LiteralPath $editor
$motorDatos = Get-Item -LiteralPath $motor

$lineas = @()
$lineas += "LobosWolfos 2000 - linea base de pruebas"
$lineas += "Fecha: " + (Get-Date -Format "yyyy-MM-dd HH:mm")
$lineas += "Commit: " + $commit
$lineas += ""
$lineas += "Esta linea base es informativa. Los hashes de los ejecutables"
$lineas += "cambian en cada compilacion (marca de tiempo del PE); las"
$lineas += "suites congelan trazas y BMP, no el binario."
$lineas += ""
$lineas += "LobosWolfos2000.exe"
$lineas += ("  bytes:  {0}" -f $editorDatos.Length)
$lineas += ("  sha256: {0}" -f (Get-Sha256 $editor))
$lineas += ""
$lineas += "motor\game.exe"
$lineas += ("  bytes:  {0}" -f $motorDatos.Length)
$lineas += ("  sha256: {0}" -f (Get-Sha256 $motor))
$lineas += ""
$lineas += "Arranque de una captura del editor (1200x700)"
$lineas += ("  tiempo: {0}" -f $tiempoEditor)
$lineas += ("  memoria pico en ventana: {0}" -f $memoriaEditor)
$lineas += ""
$lineas += "Arranque de una captura del motor (mapa.mav, 30 fotogramas)"
$lineas += ("  tiempo: {0}" -f $tiempoMotor)
$lineas += ("  memoria pico en ventana: {0}" -f $memoriaMotor)
$lineas += ""
$lineas += "Comandos"
$lineas += "  build.bat prueba"
$lineas += "  pruebas\ejecutar.ps1 -Repetir"
$lineas += "  pruebas\regresion.ps1"

$texto = ($lineas -join "`n") + "`n"
$ascii = New-Object System.Text.ASCIIEncoding
[IO.File]::WriteAllText((Join-Path $PSScriptRoot "linea_base.txt"), $texto, $ascii)
Write-Host "Escrito pruebas\linea_base.txt" -ForegroundColor Green
