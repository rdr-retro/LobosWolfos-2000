# ==========================================================================
#  LobosWolfos 2000 - compilacion con MinGW-w64 i686 (Windows XP)
#  Uso:  powershell -ExecutionPolicy Bypass -File build.ps1 [-Mingw <ruta\bin>]
# ==========================================================================
param(
    [string]$Mingw = ""
)

$ErrorActionPreference = "Stop"
$Aqui = Split-Path -Parent $MyInvocation.MyCommand.Path
$Src = Join-Path $Aqui "src"
$Obj = Join-Path $Aqui "obj"
$Salida = Join-Path $Aqui "LobosWolfos2000.exe"

if ($Mingw -eq "") {
    if ($env:MINGW32) { $Mingw = $env:MINGW32 }
    else { $Mingw = Join-Path $env:LOCALAPPDATA "Temp\opencode\mingw32\mingw32\bin" }
}

$Gcc = Join-Path $Mingw "gcc.exe"
$Windres = Join-Path $Mingw "windres.exe"

if (-not (Test-Path $Gcc)) {
    Write-Error "No se encuentra gcc.exe en '$Mingw'. Pasa la carpeta bin de MinGW-w64 i686 con -Mingw."
    exit 1
}
if (-not (Test-Path $Windres)) {
    Write-Error "No se encuentra windres.exe en '$Mingw'."
    exit 1
}

New-Item -ItemType Directory -Force -Path $Obj | Out-Null

$CFlags = @(
    "-m32", "-std=c99", "-O2", "-Wall", "-Wextra",
    "-Wno-unused-parameter",
    "-D_WIN32_WINNT=0x0501", "-DWINVER=0x0501",
    "-UUNICODE", "-U_UNICODE",
    "-fno-strict-aliasing"
)
$Libs = @(
    "-lcomctl32", "-lcomdlg32", "-lgdiplus",
    "-lgdi32", "-luser32", "-lkernel32", "-lmsimg32", "-lole32", "-luuid"
)

$Fuentes = @(
    "src\nucleo.c", "src\fuente.c", "src\documento.c", "src\imagen.c",
    "src\pintura.c", "src\texto.c", "src\ui.c", "src\tiles.c", "src\mapa.c",
    "src\tiles_modo.c", "src\pestanas.c", "src\app.c", "src\compilar.c",
    "src\juego_ventana.c",
    "motor\lobo.c", "motor\activo.c", "motor\nucleo32.s"
)

# El editor lleva la plantilla del motor como recurso (menu COMPILE):
# compilarla antes de leer los recursos, porque windres la incrusta.
# Mismas piezas y banderas que motor\build.bat.
Write-Host "Compilando el motor egine (plantilla del menu COMPILE)..." -ForegroundColor Cyan
Push-Location (Join-Path $Aqui "motor")
& $Gcc @CFlags nucleo32.s activo.c juego.c lobo.c -o game.exe -lgdiplus -lgdi32 -luser32 -lkernel32 -lole32 -mwindows -static-libgcc
if ($LASTEXITCODE -ne 0) { Pop-Location; Write-Error "fallo la compilacion del motor"; exit 1 }
Pop-Location

# Lo mismo con la plantilla de arranque del kernel loboswolfos (menu
# COMPILE, "Compilar para BIOS", fase 7): el xtask la escribe en
# loboswolfos\build\plantilla-kernel.bin y windres la incrusta.
Write-Host "Compilando el kernel loboswolfos (plantilla del menu COMPILE)..." -ForegroundColor Cyan
& cargo run --manifest-path (Join-Path $Aqui "loboswolfos\xtask\Cargo.toml") -- plantilla
if ($LASTEXITCODE -ne 0) { Write-Error "fallo la compilacion del kernel"; exit 1 }

Write-Host "Compilando recursos..." -ForegroundColor Cyan
Push-Location $Aqui
& $Windres -I src -i src\recurso.rc -o obj\recurso.o --codepage=1252
if ($LASTEXITCODE -ne 0) { Pop-Location; Write-Error "windres fallo"; exit 1 }

Write-Host "Compilando C..." -ForegroundColor Cyan
foreach ($f in $Fuentes) {
    Write-Host "  $f"
    & $Gcc @CFlags -c $f -o (Join-Path "obj" ([IO.Path]::GetFileNameWithoutExtension($f) + ".o"))
    if ($LASTEXITCODE -ne 0) { Pop-Location; Write-Error "gcc fallo con $f"; exit 1 }
}

Write-Host "Enlazando..." -ForegroundColor Cyan
$Objetos = @("recurso.o") + ($Fuentes | ForEach-Object { [IO.Path]::GetFileNameWithoutExtension($_) + ".o" })
$RutasObj = $Objetos | ForEach-Object { Join-Path "obj" $_ }
& $Gcc -m32 -o "LobosWolfos2000.exe" @RutasObj @Libs -mwindows -static-libgcc
if ($LASTEXITCODE -ne 0) { Pop-Location; Write-Error "el enlazado fallo"; exit 1 }
Pop-Location

Write-Host ""
Write-Host "Listo: $Salida" -ForegroundColor Green
exit 0
