# escenas.ps1 - suite de la fase 41: escenas multiples, transiciones,
# camara y efectos.
#
# Congela la traza de los builtins, las trazas y volcados de dos proyectos
# LOGIC3 y los hashes de sus BMP. Comprueba que un script puede cambiar de
# escena a mitad de una transicion, que los efectos y la camara se aplican
# en la escena nueva y que sin --escenas no aparece ninguna salida nueva.
#
# Casos:
#   builtins      los doce builtins en --vm-test, sin callbacks
#   dos_niveles   cambio de escena con transicion y efectos de la escena 2
#   camara        camara con suavizado, parpadeo, temblor, flash y fade
#   neutro        sin --escenas la VM devuelve valores neutros
#
# Uso:
#   pruebas\escenas.ps1                           pasa la suite
#   pruebas\escenas.ps1 -Filtro dos*              solo los casos que encajan
#   pruebas\escenas.ps1 -Actualizar               reescribe los patrones

[CmdletBinding()]
param(
    [string]$Motor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$casosDir = Join-Path $PSScriptRoot "escenas"
$esperadoDir = Join-Path $casosDir "esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
if (-not (Test-Path -LiteralPath $Motor)) {
    Write-Host "[ERROR] no existe el motor: $Motor" -ForegroundColor Red
    exit 2
}
$Motor = (Resolve-Path -LiteralPath $Motor).Path

$tmp = Join-Path $env:TEMP ("lw2000-escenas-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Escenas multiples, transiciones, camara y efectos (fase 41)" -ForegroundColor Cyan
Write-Host ("  motor:  {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Ejecutar-Motor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Motor $argumentos $dir
}

# La ruta del mapa cargado depende de TEMP y del usuario: se sustituye por
# <mapa> antes de congelar el volcado.
function Normalizar-Escenas([string]$ruta, [string]$destino) {
    $t = Get-TextoNormalizado $ruta
    $t = [regex]::Replace($t, "(?m)^escenas: mapa .+$", "escenas: mapa <mapa>")
    Guardar-TextoNormalizado $destino $t
    return $t
}

function Comprobar-Texto([string]$nombre, [string]$ruta, [string]$extension,
                         $problemas) {
    $obtenido = Get-TextoNormalizado $ruta
    $patron = Join-Path $esperadoDir ($nombre + $extension)
    if ($Actualizar) {
        Guardar-TextoNormalizado $patron $obtenido
    } elseif (-not (Test-Path -LiteralPath $patron)) {
        [void]$problemas.Add("falta " + $nombre + $extension + " (usa -Actualizar)")
    } elseif ((Get-TextoNormalizado $patron) -ne $obtenido) {
        [void]$problemas.Add("el patron de " + $nombre + $extension + " no coincide")
        Mostrar-Diferencia ((Get-TextoNormalizado $patron) -split "`n") `
                           ($obtenido -split "`n") 10
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
    $contenido = (Get-TextoNormalizado $ruta) -split "`n"
    foreach ($linea in $lineas) {
        if ($contenido -notcontains $linea) {
            [void]$problemas.Add("falta la linea: $linea")
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
    & $cuerpo $dir $problemas
    if ($problemas.Count -eq 0) {
        Write-Host ("  [{0,3}] {1,-12} OK" -f $script:indice, $nombre) -ForegroundColor Green
    } else {
        $script:fallos++
        Write-Host ("  [{0,3}] {1,-12} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
        foreach ($p in $problemas) {
            Write-Host ("        - {0}" -f $p) -ForegroundColor Red
        }
    }
}

Ejecutar-Caso "builtins" {
    param($dir, $problemas)
    $lobo = Join-Path $casosDir "builtins.lobo"
    $traza = Join-Path $dir "builtins.traza"
    $codigo = Ejecutar-Motor $dir @("--vm-test", $lobo,
        "--traza", $traza, "--fotogramas", "4")
    if ($codigo -ne 0) { [void]$problemas.Add("builtins devolvio $codigo") }
    Comprobar-Texto "builtins" $traza ".traza" $problemas
    Comprobar-Lineas $traza @("vm: 71 instrucciones 0 funciones",
        "vm 4 estado 2 pc 71 np 0 espera 0 x 1 y 16 msg 0 clones 0 err 0") `
        $problemas
}

Ejecutar-Caso "dos_niveles" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "dos_niveles.logic3"
    $bmp = Join-Path $dir "dos_niveles.bmp"
    $traza = Join-Path $dir "dos_niveles.traza"
    $volcado = Join-Path $dir "dos_niveles.volcado"
    $norm = Join-Path $dir "dos_niveles.norm"
    $codigo = Ejecutar-Motor $dir @("--logic3", $logic3, "--escenas",
        "--captura", $bmp, "--traza", $traza, "--fotogramas", "8",
        "--volcar-escenas", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("dos_niveles devolvio $codigo") }
    [void](Normalizar-Escenas $volcado $norm)
    Comprobar-Texto "dos_niveles" $traza ".traza" $problemas
    Comprobar-Texto "dos_niveles" $norm ".volcado" $problemas
    Comprobar-Bmp "dos_niveles" $bmp $problemas
    Comprobar-Lineas $traza @("vm: 3 scripts",
        "escena 2 camara 0 0 zoom 2 suave 0 trans 1 4 2 parpadeo 0 0 temblor 0 0 flash 0 0 fade 0 0 0 cambios 1 reinicios 0",
        "escena 2 camara 0 0 zoom 2 suave 3 trans 1 4 3 parpadeo 1 6 temblor 2 4 flash 16711680 2 fade 255 4 1 cambios 1 reinicios 0") `
        $problemas
    Comprobar-Lineas $norm @("escenas: activo 1", "escenas: actual 2",
        "escenas: numero 2", "escenas: mapa <mapa>",
        "escenas: cambios 1 reinicios 0",
        "escena 1 `"Nivel 1`" mapa nivel1.mav fondo 00112233",
        "escena 2 `"Nivel 2`" mapa nivel2.mav fondo 22334455") $problemas
}

Ejecutar-Caso "camara" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "camara_efectos.logic3"
    $bmp = Join-Path $dir "camara.bmp"
    $traza = Join-Path $dir "camara.traza"
    $volcado = Join-Path $dir "camara.volcado"
    $norm = Join-Path $dir "camara.norm"
    $codigo = Ejecutar-Motor $dir @("--logic3", $logic3, "--escenas",
        "--captura", $bmp, "--traza", $traza, "--fotogramas", "8",
        "--volcar-escenas", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("camara devolvio $codigo") }
    [void](Normalizar-Escenas $volcado $norm)
    Comprobar-Texto "camara" $traza ".traza" $problemas
    Comprobar-Texto "camara" $norm ".volcado" $problemas
    Comprobar-Bmp "camara" $bmp $problemas
    Comprobar-Lineas $traza @("vm: 1 scripts",
        "escena 1 camara 0 0 zoom 2 suave 3 trans 0 0 0 parpadeo 1 6 temblor 2 4 flash 16711680 2 fade 255 4 1 cambios 0 reinicios 0") `
        $problemas
    Comprobar-Lineas $norm @("escenas: activo 1", "escenas: actual 1",
        "escenas: camara objetivo 1 zoom 2 limites 0,0,320,200 suavizar 3",
        "escenas: transicion tipo 0 duracion 0 transcurrido 0 fase 0",
        "escenas: parpadeo id 1 periodo 6", "escenas: temblor intensidad 2 restantes 0",
        "escenas: flash color 16711680 restantes 0",
        "escenas: fade color 255 alpha 4 restantes 0") $problemas
}

Ejecutar-Caso "neutro" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "dos_niveles.logic3"
    $bmp = Join-Path $dir "neutro.bmp"
    $traza = Join-Path $dir "neutro.traza"
    $volcado = Join-Path $dir "neutro.volcado"
    $norm = Join-Path $dir "neutro.norm"
    $codigo = Ejecutar-Motor $dir @("--logic3", $logic3,
        "--captura", $bmp, "--traza", $traza, "--fotogramas", "8",
        "--volcar-escenas", $volcado)
    if ($codigo -ne 0) { [void]$problemas.Add("neutro devolvio $codigo") }
    [void](Normalizar-Escenas $volcado $norm)
    Comprobar-Texto "neutro" $traza ".traza" $problemas
    Comprobar-Texto "neutro" $norm ".volcado" $problemas
    Comprobar-Bmp "neutro" $bmp $problemas
    Comprobar-Lineas $norm @("escenas: activo 0", "escenas: actual 1",
        "escenas: camara objetivo 1 zoom 1 limites 0,0,320,200 suavizar 0",
        "escenas: cambios 0 reinicios 0") $problemas
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("Fase 41: {0} casos, todos correctos" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("Fase 41: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
