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
    "nucleo.c", "fuente.c", "documento.c", "imagen.c", "pintura.c", "texto.c", "ui.c",
    "tiles.c", "mapa.c", "tiles_modo.c", "pestanas.c", "app.c"
)

Write-Host "Compilando recursos..." -ForegroundColor Cyan
Push-Location $Aqui
& $Windres -I src -i src\recurso.rc -o obj\recurso.o --codepage=1252
if ($LASTEXITCODE -ne 0) { Pop-Location; Write-Error "windres fallo"; exit 1 }

Write-Host "Compilando C..." -ForegroundColor Cyan
foreach ($f in $Fuentes) {
    Write-Host "  $f"
    & $Gcc @CFlags -c (Join-Path "src" $f) -o (Join-Path "obj" ([IO.Path]::GetFileNameWithoutExtension($f) + ".o"))
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
