@echo off
rem ==========================================================================
rem  Texture Paint 2000 - compilacion con MinGW-w64 i686 (Windows XP)
rem  Uso: build.bat [ruta\a\mingw32\bin]
rem ==========================================================================
setlocal enabledelayedexpansion

set "AQUI=%~dp0"
set "SRC=%AQUI%src"
set "OBJ=%AQUI%obj"
set "SALIDA=%AQUI%TexturePaint2000.exe"

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
set "WINDRES=%MINGW%\windres.exe"

if not exist "%GCC%" (
    echo [ERROR] No se encuentra gcc.exe en "%MINGW%"
    echo         Pasa la carpeta bin de MinGW-w64 i686 como primer argumento.
    exit /b 1
)
if not exist "%WINDRES%" (
    echo [ERROR] No se encuentra windres.exe en "%MINGW%"
    exit /b 1
)

if not exist "%OBJ%" mkdir "%OBJ%"

rem Windows XP: subsistema 5.01 y sin funciones modernas.
set "CFLAGS=-m32 -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -UUNICODE -U_UNICODE -fno-strict-aliasing"
set "LIBS=-lcomctl32 -lcomdlg32 -lgdiplus -lgdi32 -luser32 -lkernel32 -lmsimg32 -lole32 -luuid"

rem windres falla con rutas absolutas que contienen espacios: usar rutas relativas.
pushd "%AQUI%"
echo Compilando recursos...
"%WINDRES%" -I src -i src\recurso.rc -o obj\recurso.o --codepage=1252
if errorlevel 1 goto error_pop

echo Compilando C...
for %%F in (src\nucleo.c src\fuente.c src\documento.c src\imagen.c src\pintura.c src\texto.c src\ui.c src\tiles.c src\mapa.c src\editor.c src\tiles_modo.c src\app.c) do (
    echo   %%~nxF
    "%GCC%" %CFLAGS% -c "%%~F" -o "obj\%%~nF.o"
    if errorlevel 1 goto error_pop
)

echo Enlazando...
"%GCC%" -m32 -o "TexturePaint2000.exe" obj\nucleo.o obj\fuente.o obj\documento.o obj\imagen.o obj\pintura.o obj\texto.o obj\ui.o obj\tiles.o obj\mapa.o obj\editor.o obj\tiles_modo.o obj\app.o obj\recurso.o %LIBS% -mwindows -static-libgcc
if errorlevel 1 goto error_pop
popd
goto exito

:error_pop
popd
goto error

:exito

echo.
echo Listo: "%SALIDA%"
exit /b 0

:error
echo.
echo [ERROR] La compilacion ha fallado.
exit /b 1
