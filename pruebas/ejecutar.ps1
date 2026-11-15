# ejecutar.ps1 - suite de conformidad del lenguaje lobo (fase 23).
#
# Cada caso vive en pruebas\conformidad:
#   <nombre>.lobo      programa de referencia (opcional)
#   <nombre>.args      argumentos extra de game.exe (opcional)
#   <nombre>.esperado  traza esperada (obligatoria al ejecutar)
#   <nombre>.bmp.sha256  hash del BMP esperado (opcional)
#
# Con .lobo el script genera un .logic temporal con un unico elemento de
# codigo y ejecuta el motor en modo captura. Sin .lobo (solo .args) usa
# los argumentos tal cual, para probar mapas y render.
#
# Uso:
#   pruebas\ejecutar.ps1                 pasa la suite
#   pruebas\ejecutar.ps1 -Filtro bucle*  solo los casos que encajan
#   pruebas\ejecutar.ps1 -Repetir        ademas comprueba determinismo
#   pruebas\ejecutar.ps1 -Actualizar     reescribe .esperado y .sha256
#   pruebas\ejecutar.ps1 -Motor ruta.exe usa otro motor (p. ej. kernel)

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
$casosDir = Join-Path $PSScriptRoot "conformidad"

if (-not $Motor) { $Motor = Join-Path $raiz "motor\game.exe" }
if (-not (Test-Path -LiteralPath $Motor)) {
    Write-Host "[ERROR] no existe el motor: $Motor" -ForegroundColor Red
    exit 2
}
$Motor = (Resolve-Path -LiteralPath $Motor).Path

$tmp = Join-Path $env:TEMP ("lw2000-conformidad-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Conformidad del lenguaje lobo" -ForegroundColor Cyan
Write-Host ("  motor: {0}" -f $Motor) -ForegroundColor DarkGray
Write-Host ("  casos: {0}" -f $casosDir) -ForegroundColor DarkGray

# Lista de casos: todo .lobo y, ademas, los .args que no tengan .lobo
# (casos de render sin script).
$entradas = New-Object System.Collections.ArrayList
foreach ($f in Get-ChildItem -LiteralPath $casosDir -Filter *.lobo | Sort-Object Name) {
    [void]$entradas.Add([pscustomobject]@{ Nombre = [IO.Path]::GetFileNameWithoutExtension($f.Name) })
}
foreach ($f in Get-ChildItem -LiteralPath $casosDir -Filter *.args | Sort-Object Name) {
    $n = [IO.Path]::GetFileNameWithoutExtension($f.Name)
    if (-not (Test-Path -LiteralPath (Join-Path $casosDir ($n + ".lobo")))) {
        [void]$entradas.Add([pscustomobject]@{ Nombre = $n })
    }
}
if ($entradas.Count -eq 0) {
    Write-Host "[ERROR] no hay casos en la carpeta" -ForegroundColor Red
    exit 2
}

$total = 0
$fallos = 0
$indice = 0

Push-Location $raiz
try {
    foreach ($caso in $entradas) {
        $nombre = $caso.Nombre
        if ($Filtro -and ($nombre -notlike $Filtro)) { continue }
        $indice++
        $total++
        $lobo = Join-Path $casosDir ($nombre + ".lobo")
        $argsExtra = Leer-Argumentos (Join-Path $casosDir ($nombre + ".args"))
        if ($argsExtra.Count -eq 0) { $argsExtra = @("--fotogramas", "8") }

        $logic = Join-Path $tmp ($nombre + ".logic")
        $bmp = Join-Path $tmp ($nombre + ".bmp")
        $traza = Join-Path $tmp ($nombre + ".traza")

        $argumentos = @()
        $tieneScript = Test-Path -LiteralPath $lobo
        if ($tieneScript) {
            $bytes = [IO.File]::ReadAllBytes($lobo)
            $cabecera = "LOGIC2`n0 0`n1`n0 0 100 100 1 0 `"`" 1`n1`n1 `"prueba`" " +
                        $bytes.Length.ToString() + "`n"
            $pie = "`nPROYECTO `"conformidad`" 320 200 1 60 3 00112233`n"
            $ascii = New-Object System.Text.ASCIIEncoding
            $todo = $ascii.GetBytes($cabecera) + $bytes + $ascii.GetBytes($pie)
            [IO.File]::WriteAllBytes($logic, $todo)
            $argumentos = @("--logica", $logic)
        }
        $argumentos += $argsExtra
        $argumentos += @("--captura", $bmp, "--traza", $traza)

        $codigo = Ejecutar-Programa $Motor $argumentos $raiz
        $problemas = New-Object System.Collections.ArrayList

        if ($codigo -ne 0) {
            [void]$problemas.Add("el motor devolvio el codigo $codigo")
        }
        if (-not (Test-Path -LiteralPath $traza)) {
            [void]$problemas.Add("no se genero la traza")
        } else {
            $obtenido = Get-TextoNormalizado $traza
            $rutaEsperado = Join-Path $casosDir ($nombre + ".esperado")
            if ($Actualizar) {
                Guardar-TextoNormalizado $rutaEsperado $obtenido
            } elseif (-not (Test-Path -LiteralPath $rutaEsperado)) {
                [void]$problemas.Add("falta " + $nombre + ".esperado (usa -Actualizar)")
            } else {
                $esperado = Get-TextoNormalizado $rutaEsperado
                if ($esperado -ne $obtenido) {
                    [void]$problemas.Add("la traza no coincide con " + $nombre + ".esperado")
                }
            }

            if ($Repetir) {
                $bmp2 = Join-Path $tmp ($nombre + ".2.bmp")
                $traza2 = Join-Path $tmp ($nombre + ".2.traza")
                $args2 = @()
                foreach ($a in $argumentos) { $args2 += $a }
                # reemplazar las rutas de --captura y --traza
                $args2[$args2.Count - 3] = $bmp2
                $args2[$args2.Count - 1] = $traza2
                [void](Ejecutar-Programa $Motor $args2 $raiz)
                if ((Get-TextoNormalizado $traza2) -ne $obtenido) {
                    [void]$problemas.Add("la traza cambia entre dos ejecuciones")
                }
                if ((Get-Sha256 $bmp2) -ne (Get-Sha256 $bmp)) {
                    [void]$problemas.Add("el BMP cambia entre dos ejecuciones")
                }
            }
        }

        $rutaHash = Join-Path $casosDir ($nombre + ".bmp.sha256")
        $hash = Get-Sha256 $bmp
        if ($Actualizar) {
            if ($hash) { Guardar-TextoNormalizado $rutaHash $hash }
        } elseif (Test-Path -LiteralPath $rutaHash) {
            $esperadoHash = (Get-TextoNormalizado $rutaHash).Trim().ToLowerInvariant()
            if ($esperadoHash -ne $hash) {
                [void]$problemas.Add("el BMP no coincide con " + $nombre + ".bmp.sha256")
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
            if (Test-Path -LiteralPath $traza) {
                $esperadoTexto = Get-TextoNormalizado (Join-Path $casosDir ($nombre + ".esperado"))
                $obtenidoTexto = Get-TextoNormalizado $traza
                if ($esperadoTexto -ne $obtenidoTexto) {
                    Mostrar-Diferencia ($esperadoTexto -split "`n") ($obtenidoTexto -split "`n")
                }
            }
        }
    }
} finally {
    Pop-Location
}

Write-Host ""
if ($fallos -eq 0) {
    Write-Host ("Conformidad: {0} casos, todo OK" -f $total) -ForegroundColor Green
    exit 0
}
Write-Host ("Conformidad: {0} casos, {1} con fallos" -f $total, $fallos) -ForegroundColor Red
exit 1
