# runtime_jerarquico.ps1 - suite de la fase 7: runtime jerarquico compartido.
#
# Congela la traza, el volcado y el BMP de un proyecto con dos escenas, y
# comprueba que la vista previa del editor y game.exe producen exactamente
# lo mismo: la misma jerarquia, los mismos eventos y la misma imagen.
#
# Casos:
#   jerarquia_inicial  hijo declarado antes que padre, instanciado por niveles
#   dos_escenas        escena_salir, vaciado, nueva escena y cambio de fondo
#
# Uso:
#   pruebas\runtime_jerarquico.ps1                    pasa la suite
#   pruebas\runtime_jerarquico.ps1 -Filtro dos*       solo los casos que encajan
#   pruebas\runtime_jerarquico.ps1 -Actualizar        reescribe los patrones

[CmdletBinding()]
param(
    [string]$Editor = "",
    [string]$Motor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$casosDir = Join-Path $PSScriptRoot "runtime_jerarquico"
$esperadoDir = Join-Path $casosDir "esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Editor) { $Editor = Join-Path $raiz "LobosWolfos2000.exe" }
if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
foreach ($exe in @($Editor, $Motor)) {
    if (-not (Test-Path -LiteralPath $exe)) {
        Write-Host "[ERROR] no existe el ejecutable: $exe" -ForegroundColor Red
        exit 2
    }
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path
$Motor = (Resolve-Path -LiteralPath $Motor).Path

$tmp = Join-Path $env:TEMP ("lw2000-rt-jerarquico-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Runtime jerarquico compartido editor / game.exe (fase 7)" -ForegroundColor Cyan
Write-Host ("  editor: {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  motor:  {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Ejecutar-Editor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Editor $argumentos $dir
}

function Ejecutar-Motor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Motor $argumentos $dir
}

function Comprobar-Texto([string]$nombre, [string]$ruta, [string]$extension,
                         $problemas) {
    $obtenido = Get-TextoNormalizado $ruta
    if (-not $obtenido) {
        [void]$problemas.Add("no se genero " + $nombre + $extension)
        return
    }
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

function Comprobar-Iguales([string]$nombre, [string]$rutaEditor,
                           [string]$rutaMotor, $problemas) {
    $a = Get-TextoNormalizado $rutaEditor
    $b = Get-TextoNormalizado $rutaMotor
    if (-not $a -or -not $b) {
        [void]$problemas.Add("falta una de las dos salidas de $nombre")
        return
    }
    if ($a -ne $b) {
        [void]$problemas.Add("editor y motor difieren en $nombre")
        Mostrar-Diferencia ($a -split "`n") ($b -split "`n") 12
    }
}

function Comprobar-Bmp([string]$nombre, [string]$editorBmp, [string]$motorBmp,
                       $problemas) {
    if (-not (Test-Path -LiteralPath $editorBmp) -or
        -not (Test-Path -LiteralPath $motorBmp)) {
        [void]$problemas.Add("falta el BMP del editor o del motor")
        return
    }
    $editorHash = Get-Sha256 $editorBmp
    $motorHash = Get-Sha256 $motorBmp
    if ($editorHash -ne $motorHash) {
        [void]$problemas.Add(
            "preview y motor dibujan distinto: $editorHash / $motorHash")
    }
    $patron = Join-Path $esperadoDir ($nombre + ".bmp.sha256")
    if ($Actualizar) {
        Guardar-TextoNormalizado $patron $editorHash
    } elseif (-not (Test-Path -LiteralPath $patron)) {
        [void]$problemas.Add("falta " + $nombre + ".bmp.sha256 (usa -Actualizar)")
    } elseif (((Get-TextoNormalizado $patron).Trim().ToLowerInvariant()) -ne $editorHash) {
        [void]$problemas.Add("el BMP de $nombre no coincide con el patron")
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

function Comprobar-OrdenEscena([string]$ruta, $problemas) {
    $contenido = (Get-TextoNormalizado $ruta) -split "`n"
    $salir = [Array]::IndexOf($contenido, "runtime: salir 1")
    $vaciar = [Array]::IndexOf($contenido, "runtime: vaciar 1")
    $escena = [Array]::IndexOf($contenido, "runtime: escena 20")
    if ($salir -lt 0 -or $vaciar -lt 0 -or $escena -lt 0) {
        [void]$problemas.Add("falta alguna linea de cambio de escena")
        return
    }
    if (-not ($salir -lt $vaciar -and $vaciar -lt $escena)) {
        [void]$problemas.Add(
            "el orden debe ser salir, vaciar, escena ($salir,$vaciar,$escena)")
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
        Write-Host ("  [{0,3}] {1,-18} OK" -f $script:indice, $nombre) -ForegroundColor Green
    } else {
        $script:fallos++
        Write-Host ("  [{0,3}] {1,-18} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
        foreach ($p in $problemas) {
            Write-Host ("        - {0}" -f $p) -ForegroundColor Red
        }
    }
}

Ejecutar-Caso "jerarquia_inicial" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "jerarquia_inicial.logic3"
    $ui = Join-Path $dir "ui.bmp"
    $editorBmp = Join-Path $dir "editor.bmp"
    $motorBmp = Join-Path $dir "motor.bmp"
    $editorVolcado = Join-Path $dir "editor.volcado"
    $motorVolcado = Join-Path $dir "motor.volcado"

    $e = Ejecutar-Editor $dir @("--sin-ventanas", "--abrir-logic3", $logic3,
        "--captura", $ui, "--captura-juego", $editorBmp,
        "--runtime-jerarquico", "--captura-juego-volcar", $editorVolcado,
        "--fotogramas", "2")
    $m = Ejecutar-Motor $dir @("--logic3", $logic3, "--escenas",
        "--runtime-jerarquico", "--captura", $motorBmp,
        "--volcar-runtime", $motorVolcado, "--fotogramas", "2")
    if ($e -ne 0 -or $m -ne 0) {
        [void]$problemas.Add("editor o motor devolvieron $e / $m")
        return
    }

    Comprobar-Texto "jerarquia_inicial" $editorVolcado ".volcado" $problemas
    Comprobar-Iguales "el volcado" $editorVolcado $motorVolcado $problemas
    Comprobar-Bmp "jerarquia_inicial" $editorBmp $motorBmp $problemas
    Comprobar-Lineas $editorVolcado @(
        "runtime: jerarquia escena 1 entidades 3 descartados 0",
        "runtime: entidad 1 tipo 100002 nombre Jugador padre 0 local 16 16 global 16 16 capa 0 visible 1",
        "runtime: entidad 2 tipo 20 nombre Padre padre 1 local 100 50 global 116 66 capa 0 visible 1",
        "runtime: entidad 3 tipo 21 nombre Hijo padre 2 local 5 7 global 121 73 capa 1 visible 1"
    ) $problemas
}

Ejecutar-Caso "dos_escenas" {
    param($dir, $problemas)
    $logic3 = Join-Path $casosDir "dos_escenas.logic3"
    $ui = Join-Path $dir "ui.bmp"
    $editorBmp = Join-Path $dir "editor.bmp"
    $motorBmp = Join-Path $dir "motor.bmp"
    $editorTraza = Join-Path $dir "editor.traza"
    $motorTraza = Join-Path $dir "motor.traza"
    $editorVolcado = Join-Path $dir "editor.volcado"
    $motorVolcado = Join-Path $dir "motor.volcado"

    $e = Ejecutar-Editor $dir @("--sin-ventanas", "--abrir-logic3", $logic3,
        "--captura", $ui, "--captura-juego", $editorBmp,
        "--runtime-jerarquico", "--captura-juego-traza", $editorTraza,
        "--captura-juego-volcar", $editorVolcado, "--fotogramas", "6")
    $m = Ejecutar-Motor $dir @("--logic3", $logic3, "--escenas", "--eventos",
        "--runtime-jerarquico", "--captura", $motorBmp,
        "--traza-runtime", $motorTraza, "--volcar-runtime", $motorVolcado,
        "--fotogramas", "6")
    if ($e -ne 0 -or $m -ne 0) {
        [void]$problemas.Add("editor o motor devolvieron $e / $m")
        return
    }

    Comprobar-Texto "dos_escenas" $editorTraza ".traza" $problemas
    Comprobar-Texto "dos_escenas" $editorVolcado ".volcado" $problemas
    Comprobar-Iguales "la traza" $editorTraza $motorTraza $problemas
    Comprobar-Iguales "el volcado" $editorVolcado $motorVolcado $problemas
    Comprobar-Bmp "dos_escenas" $editorBmp $motorBmp $problemas
    Comprobar-Lineas $editorTraza @(
        "runtime: activo 1",
        "evento 1 escena obj 100002 ent 1 script 1",
        "runtime: salir 1",
        "evento 1 escena_salir obj 100002 ent 1 script 2",
        "runtime: vaciar 1",
        "runtime: escena 20",
        "evento 2 escena obj 22 ent 4 script 0"
    ) $problemas
    Comprobar-OrdenEscena $editorTraza $problemas
    Comprobar-Lineas $editorVolcado @(
        "runtime: jerarquia escena 20 entidades 1 descartados 0",
        "runtime: entidad 4 tipo 22 nombre EnNivel2 padre 0 local 16 16 global 16 16 capa 0 visible 1"
    ) $problemas
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("Fase 7: {0} casos, todos correctos" -f $script:total) -ForegroundColor Green
    exit 0
}
Write-Host ("Fase 7: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
exit 1
