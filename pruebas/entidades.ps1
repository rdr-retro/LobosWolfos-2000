# entidades.ps1 - suite de entidades, escenas y ciclo de vida (fase 26).
#
# Casos en pruebas\entidades:
#   <nombre>.logic3   se abre con --logic3 --entidades --captura --traza
#                     --volcar; se compara <nombre>.esperado (traza) y
#                     <nombre>.volcado (estado final). Si existe
#                     <nombre>.bmp.sha256 tambien se compara el BMP.
#   <nombre>.args     argumentos extra (por ejemplo --fotogramas 12).
#
# Uso:
#   pruebas\entidades.ps1                 pasa la suite
#   pruebas\entidades.ps1 -Filtro crea*   solo los casos que encajan
#   pruebas\entidades.ps1 -Repetir        ademas comprueba determinismo
#   pruebas\entidades.ps1 -Actualizar     reescribe .esperado, .volcado y
#                                         .bmp.sha256
#
# Sin acentos: Windows PowerShell 5.1 lee los .ps1 como ANSI si no llevan
# BOM.

[CmdletBinding()]
param(
    [string]$Motor = "",
    [string]$Filtro = "",
    [switch]$Repetir,
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$casosDir = Join-Path $PSScriptRoot "entidades"

if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
if (-not (Test-Path -LiteralPath $Motor)) {
    Write-Host "[ERROR] no existe el motor: $Motor" -ForegroundColor Red
    exit 2
}
$Motor = (Resolve-Path -LiteralPath $Motor).Path

$tmp = Join-Path $env:TEMP ("lw2000-entidades-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Entidades, escenas y ciclo de vida" -ForegroundColor Cyan
Write-Host ("  motor: {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  casos: {0}" -f $casosDir) -ForegroundColor DarkGray

$entradas = New-Object System.Collections.ArrayList
foreach ($f in Get-ChildItem -LiteralPath $casosDir -Filter *.logic3 -File |
                 Sort-Object Name) {
    [void]$entradas.Add([pscustomobject]@{
        Nombre = [IO.Path]::GetFileNameWithoutExtension($f.Name)
        Ruta = $f.FullName
    })
}
if ($entradas.Count -eq 0) {
    Write-Host "[ERROR] no hay casos .logic3 en la carpeta" -ForegroundColor Red
    exit 2
}

$total = 0
$fallos = 0
$indice = 0

# Argumentos extra de un caso, sin las opciones que pone la propia suite.
function Get-ArgumentosCaso([string]$nombre) {
    $extra = Leer-Argumentos (Join-Path $casosDir ($nombre + ".args"))
    $lista = New-Object System.Collections.ArrayList
    $tieneFotogramas = $false
    for ($i = 0; $i -lt $extra.Count; $i++) {
        $a = $extra[$i]
        if ($a -eq "--fotogramas") {
            $tieneFotogramas = $true
        } elseif ($a -eq "--logic3" -or $a -eq "--captura" -or
                  $a -eq "--traza" -or $a -eq "--volcar" -or
                  $a -eq "--entidades") {
            continue
        }
        [void]$lista.Add($a)
    }
    if (-not $tieneFotogramas) { [void]$lista.Add("--fotogramas"); [void]$lista.Add("8") }
    return $lista.ToArray()
}

# Lanza un caso y devuelve las rutas de sus tres salidas.
function Invoke-Caso([string]$ruta, [string]$nombre, [string]$sufijo) {
    $traza = Join-Path $tmp ($nombre + $sufijo + ".traza")
    $bmp = Join-Path $tmp ($nombre + $sufijo + ".bmp")
    $volcado = Join-Path $tmp ($nombre + $sufijo + ".volcado")
    $argumentos = @("--logic3", $ruta, "--captura", $bmp,
                    "--traza", $traza, "--volcar", $volcado, "--entidades")
    $argumentos += Get-ArgumentosCaso $nombre
    $codigo = Ejecutar-Programa $Motor $argumentos $raiz
    return [pscustomobject]@{
        Codigo = $codigo
        Traza = $traza
        Bmp = $bmp
        Volcado = $volcado
    }
}

Push-Location $raiz
try {
    foreach ($caso in $entradas) {
        $nombre = $caso.Nombre
        if ($Filtro -and ($nombre -notlike $Filtro)) { continue }
        $indice++
        $total++

        $r = Invoke-Caso $caso.Ruta $nombre ""
        $problemas = New-Object System.Collections.ArrayList
        if ($r.Codigo -ne 0) {
            [void]$problemas.Add("el motor devolvio el codigo " + $r.Codigo)
        }

        # --- traza por fotograma --------------------------------------
        $esperadoTraza = Join-Path $casosDir ($nombre + ".esperado")
        if (-not (Test-Path -LiteralPath $r.Traza)) {
            [void]$problemas.Add("no se genero la traza")
            $obtenidoTraza = ""
        } else {
            $obtenidoTraza = Get-TextoNormalizado $r.Traza
            if ($Actualizar) {
                Guardar-TextoNormalizado $esperadoTraza $obtenidoTraza
            } elseif (-not (Test-Path -LiteralPath $esperadoTraza)) {
                [void]$problemas.Add("falta " + $nombre + ".esperado (usa -Actualizar)")
            } else {
                $esperado = Get-TextoNormalizado $esperadoTraza
                if ($esperado -ne $obtenidoTraza) {
                    [void]$problemas.Add("la traza no coincide con " + $nombre + ".esperado")
                    Mostrar-Diferencia ($esperado -split "`n") ($obtenidoTraza -split "`n")
                }
            }
        }

        # --- volcado del estado final ---------------------------------
        $esperadoVolcado = Join-Path $casosDir ($nombre + ".volcado")
        if (-not (Test-Path -LiteralPath $r.Volcado)) {
            [void]$problemas.Add("no se genero el volcado")
            $obtenidoVolcado = ""
        } else {
            $obtenidoVolcado = Get-TextoNormalizado $r.Volcado
            if ($Actualizar) {
                Guardar-TextoNormalizado $esperadoVolcado $obtenidoVolcado
            } elseif (-not (Test-Path -LiteralPath $esperadoVolcado)) {
                [void]$problemas.Add("falta " + $nombre + ".volcado (usa -Actualizar)")
            } else {
                $esperado = Get-TextoNormalizado $esperadoVolcado
                if ($esperado -ne $obtenidoVolcado) {
                    [void]$problemas.Add("el volcado no coincide con " + $nombre + ".volcado")
                    Mostrar-Diferencia ($esperado -split "`n") ($obtenidoVolcado -split "`n")
                }
            }
        }

        # --- BMP (solo si el caso lo congela) -------------------------
        $rutaHash = Join-Path $casosDir ($nombre + ".bmp.sha256")
        $hash = Get-Sha256 $r.Bmp
        if ($Actualizar) {
            if ($hash) { Guardar-TextoNormalizado $rutaHash $hash }
        } elseif (Test-Path -LiteralPath $rutaHash) {
            $esperadoHash = (Get-TextoNormalizado $rutaHash).Trim().ToLowerInvariant()
            if ($esperadoHash -ne $hash) {
                [void]$problemas.Add("el BMP no coincide con " + $nombre + ".bmp.sha256")
            }
        }

        # --- determinismo ---------------------------------------------
        if ($Repetir) {
            $r2 = Invoke-Caso $caso.Ruta $nombre ".2"
            if ((Get-TextoNormalizado $r2.Traza) -ne $obtenidoTraza) {
                [void]$problemas.Add("la traza cambia entre dos ejecuciones")
            }
            if ((Get-TextoNormalizado $r2.Volcado) -ne $obtenidoVolcado) {
                [void]$problemas.Add("el volcado cambia entre dos ejecuciones")
            }
            if ((Get-Sha256 $r2.Bmp) -ne $hash) {
                [void]$problemas.Add("el BMP cambia entre dos ejecuciones")
            }
        }

        if ($problemas.Count -eq 0) {
            Write-Host ("  [{0,3}] {1,-22} OK" -f $indice, $nombre) -ForegroundColor Green
        } else {
            $fallos++
            Write-Host ("  [{0,3}] {1,-22} FALLA" -f $indice, $nombre) -ForegroundColor Red
            foreach ($p in $problemas) {
                Write-Host ("        - {0}" -f $p) -ForegroundColor Red
            }
        }
    }
} finally {
    Pop-Location
}

Write-Host ""
if ($fallos -eq 0) {
    Write-Host ("ENTIDADES: {0} casos, todo OK" -f $total) -ForegroundColor Green
    exit 0
}
Write-Host ("ENTIDADES: {0} casos, {1} fallos" -f $total, $fallos) -ForegroundColor Red
exit 1
