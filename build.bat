@echo off
rem ==========================================================================
rem  LobosWolfos 2000 - compilacion con MinGW-w64 i686 (Windows XP)
rem  Uso: build.bat [ruta\a\mingw32\bin]
rem ==========================================================================
setlocal enabledelayedexpansion

set "AQUI=%~dp0"
set "SRC=%AQUI%src"
set "OBJ=%AQUI%obj"
set "SALIDA=%AQUI%LobosWolfos2000.exe"

rem Modo pruebas (fase 23): compila y ejecuta las suites.
if /i "%~1"=="prueba" goto pruebas

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

rem El editor lleva la plantilla del motor como recurso (menu COMPILE):
rem compilarla antes de leer los recursos, porque windres la incrusta.
echo Compilando el motor egine (plantilla del menu COMPILE)...
call "%AQUI%motor\build.bat" "%MINGW%"
if errorlevel 1 goto error

rem Lo mismo con la plantilla de arranque del kernel loboswolfos (menu
rem COMPILE, "Compilar para BIOS", fase 7): el xtask la escribe en
rem loboswolfos\build\plantilla-kernel.bin y windres la incrusta.
echo Compilando el kernel loboswolfos (plantilla del menu COMPILE)...
cargo run --manifest-path "%AQUI%loboswolfos\xtask\Cargo.toml" -- plantilla
if errorlevel 1 goto error

rem windres falla con rutas absolutas que contienen espacios: usar rutas relativas.
pushd "%AQUI%"
echo Compilando recursos...
"%WINDRES%" -I src -i src\recurso.rc -o obj\recurso.o --codepage=1252
if errorlevel 1 goto error_pop

echo Compilando C...
for %%F in (src\nucleo.c src\fuente.c src\documento.c src\imagen.c src\pintura.c src\texto.c src\ui.c src\tiles.c src\mapa.c src\tiles_modo.c src\pestanas.c src\app.c src\ajustes.c src\compilar.c src\juego_ventana.c src\ejemplo.c src\logic3.c src\estudio.c src\estudio_ui.c src\datos.c src\sonido.c src\productividad.c src\bloques.c src\bloques_ui.c src\compilador_bloques.c motor\lobo.c motor\activo.c motor\colisiones.c motor\nucleo32.s) do (
    echo   %%~nxF
    "%GCC%" %CFLAGS% -c "%%~F" -o "obj\%%~nF.o"
    if errorlevel 1 goto error_pop
)

echo Enlazando...
"%GCC%" -m32 -o "LobosWolfos2000.exe" obj\nucleo.o obj\fuente.o obj\documento.o obj\imagen.o obj\pintura.o obj\texto.o obj\ui.o obj\tiles.o obj\mapa.o obj\tiles_modo.o obj\pestanas.o obj\app.o obj\ajustes.o obj\compilar.o obj\juego_ventana.o obj\ejemplo.o obj\logic3.o obj\estudio.o obj\estudio_ui.o obj\datos.o obj\sonido.o obj\productividad.o obj\bloques.o obj\bloques_ui.o obj\compilador_bloques.o obj\lobo.o obj\activo.o obj\colisiones.o obj\nucleo32.o obj\recurso.o %LIBS% -mwindows -static-libgcc
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

rem ------------------------------------------------------------------
rem  Modo pruebas: build.bat prueba [conformidad^|logic3^|vm^|entidades^|eventos^|colisiones^|render^|estudio^|bloques^|compilador^|depurador^|datos^|regresion^|actualizar]
rem  Compila el editor con la configuracion normal y pasa las suites.
rem ------------------------------------------------------------------
:pruebas
echo Compilando antes de las pruebas...
call "%~f0"
if errorlevel 1 exit /b 1
set "PS=powershell -NoProfile -ExecutionPolicy Bypass -File"
if /i "%~2"=="actualizar" goto pruebas_actualizar
if /i "%~2"=="regresion" goto pruebas_regresion
if /i "%~2"=="conformidad" goto pruebas_conformidad
if /i "%~2"=="logic3" goto pruebas_logic3
if /i "%~2"=="vm" goto pruebas_vm
if /i "%~2"=="entidades" goto pruebas_entidades
if /i "%~2"=="eventos" goto pruebas_eventos
if /i "%~2"=="colisiones" goto pruebas_colisiones
if /i "%~2"=="render" goto pruebas_render
if /i "%~2"=="estudio" goto pruebas_estudio
if /i "%~2"=="bloques" goto pruebas_bloques
if /i "%~2"=="compilador" goto pruebas_compilador
if /i "%~2"=="depurador" goto pruebas_depurador
if /i "%~2"=="datos" goto pruebas_datos
if /i "%~2"=="productividad" goto pruebas_productividad

echo.
echo === Suite de conformidad del lenguaje ===
%PS% "%AQUI%pruebas\ejecutar.ps1"
if errorlevel 1 exit /b 1
echo.
echo === Proyecto LOGIC3 ===
%PS% "%AQUI%pruebas\logic3.ps1"
if errorlevel 1 exit /b 1
echo.
echo === VM resumible y lobo 3 ===
%PS% "%AQUI%pruebas\vm.ps1"
if errorlevel 1 exit /b 1
echo.
echo === Entidades, escenas y ciclo de vida ===
%PS% "%AQUI%pruebas\entidades.ps1"
if errorlevel 1 exit /b 1
echo.
echo === Eventos, planificador y mensajes ===
%PS% "%AQUI%pruebas\eventos.ps1"
if errorlevel 1 exit /b 1
echo.
echo === Colisiones, flags y fisica basica ===
%PS% "%AQUI%pruebas\colisiones.ps1"
if errorlevel 1 exit /b 1
echo.
echo === Render de trajes, animacion, capas y camara ===
%PS% "%AQUI%pruebas\render.ps1"
if errorlevel 1 exit /b 1
echo.
echo === Vista Estudio ===
%PS% "%AQUI%pruebas\estudio.ps1"
if errorlevel 1 exit /b 1
echo.
echo === Lienzo de bloques ===
%PS% "%AQUI%pruebas\bloques.ps1"
if errorlevel 1 exit /b 1
echo.
echo === Compilador bloques - lobo ===
%PS% "%AQUI%pruebas\compilador.ps1"
if errorlevel 1 exit /b 1
echo.
echo === Depurador y replay ===
%PS% "%AQUI%pruebas\depurador.ps1"
if errorlevel 1 exit /b 1
echo.
echo === Datos, trajes y sonidos ===
%PS% "%AQUI%pruebas\datos.ps1"
if errorlevel 1 exit /b 1
echo.
echo === Productividad del editor ===
%PS% "%AQUI%pruebas\productividad.ps1"
if errorlevel 1 exit /b 1
echo.
echo === Regresion visual del editor ===
%PS% "%AQUI%pruebas\regresion.ps1"
exit /b %ERRORLEVEL%

:pruebas_conformidad
%PS% "%AQUI%pruebas\ejecutar.ps1"
exit /b %ERRORLEVEL%

:pruebas_logic3
%PS% "%AQUI%pruebas\logic3.ps1"
exit /b %ERRORLEVEL%

:pruebas_vm
%PS% "%AQUI%pruebas\vm.ps1"
exit /b %ERRORLEVEL%

:pruebas_entidades
%PS% "%AQUI%pruebas\entidades.ps1"
exit /b %ERRORLEVEL%

:pruebas_eventos
%PS% "%AQUI%pruebas\eventos.ps1"
exit /b %ERRORLEVEL%

:pruebas_colisiones
%PS% "%AQUI%pruebas\colisiones.ps1"
exit /b %ERRORLEVEL%

:pruebas_render
%PS% "%AQUI%pruebas\render.ps1"
exit /b %ERRORLEVEL%

:pruebas_estudio
%PS% "%AQUI%pruebas\estudio.ps1"
exit /b %ERRORLEVEL%

:pruebas_bloques
%PS% "%AQUI%pruebas\bloques.ps1"
exit /b %ERRORLEVEL%

:pruebas_compilador
%PS% "%AQUI%pruebas\compilador.ps1"
exit /b %ERRORLEVEL%

:pruebas_depurador
%PS% "%AQUI%pruebas\depurador.ps1"
exit /b %ERRORLEVEL%

:pruebas_datos
%PS% "%AQUI%pruebas\datos.ps1"
exit /b %ERRORLEVEL%

:pruebas_productividad
%PS% "%AQUI%pruebas\productividad.ps1"
exit /b %ERRORLEVEL%

:pruebas_regresion
%PS% "%AQUI%pruebas\regresion.ps1"
exit /b %ERRORLEVEL%

:pruebas_actualizar
%PS% "%AQUI%pruebas\ejecutar.ps1" -Actualizar
if errorlevel 1 exit /b 1
%PS% "%AQUI%pruebas\logic3.ps1" -Actualizar
if errorlevel 1 exit /b 1
%PS% "%AQUI%pruebas\vm.ps1" -Actualizar
if errorlevel 1 exit /b 1
%PS% "%AQUI%pruebas\entidades.ps1" -Actualizar
if errorlevel 1 exit /b 1
%PS% "%AQUI%pruebas\eventos.ps1" -Actualizar
if errorlevel 1 exit /b 1
%PS% "%AQUI%pruebas\colisiones.ps1" -Actualizar
if errorlevel 1 exit /b 1
%PS% "%AQUI%pruebas\render.ps1" -Actualizar
if errorlevel 1 exit /b 1
%PS% "%AQUI%pruebas\estudio.ps1" -Actualizar
if errorlevel 1 exit /b 1
%PS% "%AQUI%pruebas\bloques.ps1" -Actualizar
if errorlevel 1 exit /b 1
%PS% "%AQUI%pruebas\compilador.ps1" -Actualizar
if errorlevel 1 exit /b 1
%PS% "%AQUI%pruebas\depurador.ps1" -Actualizar
if errorlevel 1 exit /b 1
%PS% "%AQUI%pruebas\datos.ps1" -Actualizar
if errorlevel 1 exit /b 1
%PS% "%AQUI%pruebas\productividad.ps1" -Actualizar
if errorlevel 1 exit /b 1
%PS% "%AQUI%pruebas\regresion.ps1" -Actualizar
exit /b %ERRORLEVEL%

:error
echo.
echo [ERROR] La compilacion ha fallado.
exit /b 1
