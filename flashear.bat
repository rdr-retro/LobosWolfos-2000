@echo off
rem ==========================================================================
rem  LobosWolfos 2000 - flashear el pendrive con la imagen del juego
rem  Uso: flashear.bat [disco] [imagen]   (por defecto loboswolfos.img)
rem
rem  La imagen la genera el editor con el menu COMPILE, "Compilar para
rem  BIOS" (fase 7), o el xtask de loboswolfos con `build`. Basado en el
rem  flashear.bat de rocky.
rem ==========================================================================
setlocal EnableDelayedExpansion
title loboswolfos-usb - flashear pendrive

set "BASE=%~dp0"
set "DISCO=%~1"
set "IMAGEN=%~2"
if "%IMAGEN%"=="" set "IMAGEN=%BASE%loboswolfos.img"

echo ============================================================
echo   loboswolfos-usb: flashear el pendrive con el juego
echo ============================================================
echo.

if not exist "%IMAGEN%" (
    echo  ERROR: falta la imagen
    echo    %IMAGEN%
    echo.
    echo  Compila el juego desde el editor ^(menu COMPILE,
    echo  "Compilar para BIOS"^) o genera la imagen con:
    echo    cargo run --manifest-path loboswolfos\xtask\Cargo.toml -- build
    echo.
    pause
    exit /b 1
)

echo   Imagen: %IMAGEN%
echo.

rem El script se eleva solo (hace falta administrador para escribir en
rem \\.\PhysicalDriveN) y espera a su ventana elevada, asi que aqui vemos
rem el resultado cuando vuelve.
if "%DISCO%"=="" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%BASE%flashear-usb.ps1" -Imagen "%IMAGEN%"
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%BASE%flashear-usb.ps1" -Imagen "%IMAGEN%" -Disco %DISCO%
)
set "EC=!errorlevel!"

echo.
if %EC% equ 0 (
    echo  OK: imagen flasheada. Arranca el PC desde el USB.
) else if %EC% equ 1 (
    echo  Cancelado o fallo de argumentos ^(codigo 1^).
) else (
    echo  ALGO HA FALLADO. Revisa los mensajes de arriba ^(codigo %EC%^).
)
echo.
pause
exit /b %EC%
