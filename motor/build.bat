@echo off
rem ==========================================================================
rem  motor egine - game.exe con el nucleo en ensamblador x86 de 32 bits
rem  Uso: build.bat [ruta\a\mingw32\bin]
rem ==========================================================================
setlocal enabledelayedexpansion

set "AQUI=%~dp0"
set "SALIDA=%AQUI%game.exe"

if not "%~1"=="" (
    set "MINGW=%~1"
) else (
    if not "%MINGW32%"=="" (
        set "MINGW=%MINGW32%"
    ) else (
        set "MINGW=%LOCALAPPDATA%\Temp\opencode\mingw32\mingw32\bin"
    )
)

set "GCC=%MINGW%\gcc.exe"

if not exist "%GCC%" (
    echo [ERROR] No se encuentra gcc.exe en "%MINGW%"
    echo         Pasa la carpeta bin de MinGW-w64 i686 como primer argumento.
    exit /b 1
)

echo Compilando el motor egine (nucleo ASM 32 bits + juego)...

"%GCC%" -m32 -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter ^
    -D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -UUNICODE -U_UNICODE ^
    -fno-strict-aliasing ^
    nucleo32.s activo.c juego.c ^
    -o "%SALIDA%" ^
    -lgdiplus -lgdi32 -luser32 -lkernel32 ^
    -mwindows -static-libgcc
if errorlevel 1 (
    echo [ERROR] fallo la compilacion
    exit /b 1
)

echo Listo: "%SALIDA%"
echo   Prueba: game.exe --captura captura.bmp --fotogramas 60
