# plantillas.ps1 - suite de plantillas, ejemplos y tutorial (fase 44).
#
# Congela el volcado propio y el hash del BMP de cada caso. El volcado usa
# --volcar-plantillas, un archivo nuevo: no toca los patrones de las 21
# suites anteriores. Ademas valida el CRC32 del proyecto generado y del
# ejemplo canonico calculandolo aqui, sin depender del editor.
#
# Casos:
#   catalogo      los cinco ids estan en el catalogo y no hay nada abierto
#   selector      --juego-nuevo abre el modal sin generar nada
#   plataformas   genera el proyecto jugable con nivel, jugador, monedas y meta
#   tutorial      abre el tutorial en un paso concreto
#   ayuda         abre la ayuda del bloque codigo.lobo por su tipo
#   canonico      valida ejemplos\primer_juego.logic3 y su CRC32
#
# Uso:
#   pruebas\plantillas.ps1                      pasa la suite
#   pruebas\plantillas.ps1 -Filtro plataformas  solo ese caso
#   pruebas\plantillas.ps1 -Actualizar          reescribe los patrones

[CmdletBinding()]
param(
    [string]$Editor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$esperadoDir = Join-Path $PSScriptRoot "plantillas\esperado"
New-Item -ItemType Directory -Force $esperadoDir | Out-Null

if (-not $Editor) { $Editor = Join-Path $raiz "LobosWolfos2000.exe" }
if (-not (Test-Path -LiteralPath $Editor)) {
    Write-Host "[ERROR] no existe el editor: $Editor" -ForegroundColor Red
    exit 2
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path

$tmp = Join-Path $env:TEMP ("lw2000-plantillas-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Plantillas, ejemplos y tutorial" -ForegroundColor Cyan
Write-Host ("  editor: {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $esperadoDir) -ForegroundColor DarkGray

function Get-CampoTexto([string]$texto, [string]$clave) {
    $m = [regex]::Match($texto, "(?:^|\s)" + [regex]::Escape($clave) + "=([^\s]+)")
    if ($m.Success) { return $m.Groups[1].Value }
    return ""
}

function Get-Crc32([byte[]]$datos) {
    # Los operadores bit a bit de PowerShell devuelven enteros con signo:
    # se trabaja con int64 enmascarado para no perder el bit alto.
    $tabla = New-Object 'int64[]' 256
    for ($i = 0; $i -lt 256; $i++) {
        $c = [int64]$i
        for ($k = 0; $k -lt 8; $k++) {
            if (($c -band [int64]1) -ne 0) {
                $c = ((($c -shr 1) -bxor [int64]3988292384) -band [int64]4294967295)
            } else {
                $c = $c -shr 1
            }
        }
        $tabla[$i] = $c
    }
    $crc = [int64]4294967295
    foreach ($b in $datos) {
        $idx = [int](($crc -bxor [int64]$b) -band [int64]0xFF)
        $crc = ((($crc -shr 8) -bxor $tabla[$idx]) -band [int64]4294967295)
    }
    return ('{0:X8}' -f ($crc -bxor [int64]4294967295))
}

function Test-Proyecto([string]$ruta, $problemas) {
    if (-not (Test-Path -LiteralPath $ruta)) {
        [void]$problemas.Add("falta el proyecto " + $ruta)
        return
    }
    $bytes = [IO.File]::ReadAllBytes($ruta)
    $texto = [Text.Encoding]::ASCII.GetString($bytes)
    $fin = $texto.IndexOf("FIN`n")
    if ($fin -lt 0) {
        [void]$problemas.Add("no aparece la linea FIN")
        return
    }
    $n = $fin + 4
    $parte = New-Object 'byte[]' $n
    [Array]::Copy($bytes, 0, $parte, 0, $n)
    $calculado = Get-Crc32 $parte
    $declarado = ""
    foreach ($linea in ($texto -split "`n")) {
        if ($linea.StartsWith("CRC32 ")) { $declarado = $linea.Substring(6).Trim() }
    }
    if ($declarado -ne $calculado) {
        [void]$problemas.Add(("CRC32 declarado {0}, calculado {1}" -f $declarado, $calculado))
    }
    $necesario = @(
        'LOGIC3',
        'escena 1 "Nivel 1"',
        'objeto 1 "Jugador"',
        'objeto 2 "Moneda 1"',
        'objeto 3 "Moneda 2"',
        'objeto 4 "Moneda 3"',
        'objeto 5 "Meta"',
        'variable "monedas"',
        'variable "ganado"',
        'evento 1 fotograma',
        'evento 2 tecla "derecha" mantener',
        'evento 3 tecla "izquierda" mantener',
        'tipo "codigo.lobo"',
        'toca(5)',
        'borrar(2)'
    )
    foreach ($frag in $necesario) {
        if (-not $texto.Contains($frag)) {
            [void]$problemas.Add("falta en el proyecto: " + $frag)
        }
    }
}

function Comprobar-Artefactos([string]$nombre, [string]$dir, [string]$bmp,
                              [string]$dump, [hashtable]$campos, $problemas) {
    if (-not (Test-Path -LiteralPath $dump)) {
        [void]$problemas.Add("falta el volcado")
        return
    }
    if (-not (Test-Path -LiteralPath $bmp)) {
        [void]$problemas.Add("falta el BMP")
        return
    }
    $texto = Get-TextoNormalizado $dump
    foreach ($clave in ($campos.Keys | Sort-Object)) {
        $valor = Get-CampoTexto $texto $clave
        $quiero = [string]$campos[$clave]
        if ($valor -ne $quiero) {
            [void]$problemas.Add(("volcado: {0}={1} (esperado {2})" -f `
                                  $clave, $valor, $quiero))
        }
    }
    $hash = Get-Sha256 $bmp
    $rutaVolcar = Join-Path $esperadoDir ($nombre + ".volcar")
    $rutaHash = Join-Path $esperadoDir ($nombre + ".bmp.sha256")
    if ($Actualizar) {
        Guardar-TextoNormalizado $rutaVolcar $texto
        Guardar-TextoNormalizado $rutaHash $hash
        return
    }
    if (-not (Test-Path -LiteralPath $rutaVolcar)) {
        [void]$problemas.Add("falta " + $nombre + ".volcar (usa -Actualizar)")
    } elseif ((Get-TextoNormalizado $rutaVolcar) -ne $texto) {
        [void]$problemas.Add("el volcado no coincide")
        Mostrar-Diferencia ((Get-TextoNormalizado $rutaVolcar) -split "`n") `
                           ($texto -split "`n") 6
    }
    if (-not (Test-Path -LiteralPath $rutaHash)) {
        [void]$problemas.Add("falta " + $nombre + ".bmp.sha256 (usa -Actualizar)")
    } elseif (((Get-TextoNormalizado $rutaHash).Trim().ToLowerInvariant()) -ne $hash) {
        [void]$problemas.Add("el BMP no coincide")
    }
}

function Ejecutar-Editor([string]$dir, [string[]]$argumentos) {
    return Ejecutar-Programa $Editor $argumentos $dir
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
        Write-Host ("  [{0,3}] {1,-15} OK" -f $script:indice, $nombre) -ForegroundColor Green
    } else {
        $script:fallos++
        Write-Host ("  [{0,3}] {1,-15} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
        foreach ($p in $problemas) {
            Write-Host ("        - {0}" -f $p) -ForegroundColor Red
        }
    }
}

Ejecutar-Caso "catalogo" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "catalogo.bmp"
    $dump = Join-Path $dir "catalogo.txt"
    $codigo = Ejecutar-Editor $dir @("--sin-ventanas", `
                                     "--volcar-plantillas", $dump, `
                                     "--captura", $bmp)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "catalogo" $dir $bmp $dump @{
        plantillas = "5"; seleccion = "plataformas";
        jn = "0"; tut = "0"; ayuda = "0"; creada = "-"
    } $problemas
    $texto = Get-TextoNormalizado $dump
    $catalogo = Get-CampoTexto $texto "catalogo"
    $esperado = "plataformas;shooter;puzzle;aventura;laberinto"
    if ($catalogo -ne $esperado) {
        [void]$problemas.Add(("catalogo={0} (esperado {1})" -f $catalogo, $esperado))
    }
}

Ejecutar-Caso "selector" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "selector.bmp"
    $dump = Join-Path $dir "selector.txt"
    $codigo = Ejecutar-Editor $dir @("--juego-nuevo", "--sin-ventanas", `
                                     "--volcar-plantillas", $dump, `
                                     "--captura", $bmp)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "selector" $dir $bmp $dump @{
        plantillas = "5"; jn = "1"; jn_sel = "0"; tut = "0";
        ayuda = "0"; creada = "-"
    } $problemas
}

Ejecutar-Caso "plataformas" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "plataformas.bmp"
    $dump = Join-Path $dir "plataformas.txt"
    $codigo = Ejecutar-Editor $dir @("--juego-nuevo", "--plantilla", "plataformas", `
                                     "--sin-ventanas", `
                                     "--volcar-plantillas", $dump, `
                                     "--captura", $bmp)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "plataformas" $dir $bmp $dump @{
        plantillas = "5"; seleccion = "plataformas"; jn = "0";
        creada = "logic\primer_juego.logic3"
    } $problemas
    $generado = Join-Path $dir "logic\primer_juego.logic3"
    Test-Proyecto $generado $problemas
    $ruta = Join-Path $esperadoDir "plataformas.logic3"
    if (Test-Path -LiteralPath $generado) {
        $contenido = Get-TextoNormalizado $generado
        if ($Actualizar) {
            Guardar-TextoNormalizado $ruta $contenido
        } elseif (-not (Test-Path -LiteralPath $ruta)) {
            [void]$problemas.Add("falta plataformas.logic3 (usa -Actualizar)")
        } elseif ((Get-TextoNormalizado $ruta) -ne $contenido) {
            [void]$problemas.Add("el LOGIC3 generado no coincide")
            Mostrar-Diferencia ((Get-TextoNormalizado $ruta) -split "`n") `
                               ($contenido -split "`n") 6
        }
    }
}

Ejecutar-Caso "tutorial" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "tutorial.bmp"
    $dump = Join-Path $dir "tutorial.txt"
    $codigo = Ejecutar-Editor $dir @("--tutorial", "--tutorial-paso", "2", `
                                     "--sin-ventanas", `
                                     "--volcar-plantillas", $dump, `
                                     "--captura", $bmp)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "tutorial" $dir $bmp $dump @{
        tut = "1"; tut_paso = "2"; jn = "0"; ayuda = "0"; creada = "-"
    } $problemas
}

Ejecutar-Caso "ayuda" {
    param($dir, $problemas)
    $bmp = Join-Path $dir "ayuda.bmp"
    $dump = Join-Path $dir "ayuda.txt"
    $codigo = Ejecutar-Editor $dir @("--ayuda-bloque", "codigo.lobo", `
                                     "--sin-ventanas", `
                                     "--volcar-plantillas", $dump, `
                                     "--captura", $bmp)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "ayuda" $dir $bmp $dump @{
        ayuda = "9"; tut = "0"; jn = "0"; creada = "-"
    } $problemas
}

Ejecutar-Caso "canonico" {
    param($dir, $problemas)
    $canonico = Join-Path $raiz "ejemplos\primer_juego.logic3"
    Test-Proyecto $canonico $problemas
    $bmp = Join-Path $dir "canonico.bmp"
    $dump = Join-Path $dir "canonico.txt"
    $codigo = Ejecutar-Editor $dir @("--abrir-logic3", $canonico, `
                                     "--sin-ventanas", `
                                     "--volcar-plantillas", $dump, `
                                     "--captura", $bmp)
    if ($codigo -ne 0) { [void]$problemas.Add("el editor devolvio $codigo") }
    Comprobar-Artefactos "canonico" $dir $bmp $dump @{
        jn = "0"; tut = "0"; ayuda = "0"; creada = "-"
    } $problemas
    $texto = Get-TextoNormalizado $dump
    if (-not (Get-CampoTexto $texto "plantillas")) {
        [void]$problemas.Add("no se escribio el volcado propio")
    }
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("Plantillas: {0} casos, todos correctos" -f $script:total) -ForegroundColor Green
} else {
    Write-Host ("Plantillas: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
}
exit $script:fallos
