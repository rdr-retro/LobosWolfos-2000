# logic3.ps1 - suite del proyecto LOGIC3 (fase 24).
#
# Comprueba el lector, el escritor y el migrador de LOGIC3 sin tocar el
# runtime: el motor sigue leyendo LOGIC2 y el editor clasico sigue
# abriendo los .logic de siempre.
#
# Casos:
#   convertir   LOGIC2 -> LOGIC3 y comprobacion del archivo canonico
#   ida_vuelta  abrir el LOGIC3, guardarlo y comparar byte a byte
#   exportar    LOGIC3 -> LOGIC2 de emergencia y comprobacion del texto
#   clasico     el LOGIC2 exportado y el fixture LOGIC2 abren como antes
#   detectar    la cabecera LOGIC3 funciona desde el dialogo clasico
#   crc         un CRC32 que no coincide se rechaza
#   crc_texto   un CRC32 con formato invalido se rechaza
#   version     una version futura se rechaza
#   sin_fin     un archivo sin FIN se rechaza
#   extension   secciones desconocidas se ignoran y el CRC es opcional
#   tipos       variables tipadas y listas con elementos: ida y vuelta
#
# Uso:
#   pruebas\logic3.ps1                 pasa la suite
#   pruebas\logic3.ps1 -Filtro crc*    solo los casos que encajan
#   pruebas\logic3.ps1 -Actualizar     reescribe los patrones

[CmdletBinding()]
param(
    [string]$Editor = "",
    [string]$Filtro = "",
    [switch]$Actualizar
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "comun.ps1")

$raiz = Split-Path -Parent $PSScriptRoot
$carpeta = Join-Path $PSScriptRoot "logic3"
$patron3 = Join-Path $carpeta "esperado.logic3"
$patron2 = Join-Path $carpeta "esperado-export.log"

if (-not $Editor) { $Editor = Join-Path $raiz "LobosWolfos2000.exe" }
if (-not (Test-Path -LiteralPath $Editor)) {
    Write-Host "[ERROR] no existe el editor: $Editor" -ForegroundColor Red
    exit 2
}
if (-not (Test-Path -LiteralPath (Join-Path $carpeta "base.logic"))) {
    Write-Host "[ERROR] falta pruebas\logic3\base.logic" -ForegroundColor Red
    exit 2
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path

$tmp = Join-Path $env:TEMP ("lw2000-logic3-" + $PID)
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host ""
Write-Host "Proyecto LOGIC3" -ForegroundColor Cyan
Write-Host ("  editor: {0}" -f $Editor) -ForegroundColor DarkGray
Write-Host ("  casos:  {0}" -f $carpeta) -ForegroundColor DarkGray

function Get-Campo([string]$ruta, [string]$clave) {
    if (-not (Test-Path -LiteralPath $ruta)) { return "" }
    $texto = [IO.File]::ReadAllText($ruta)
    $m = [regex]::Match($texto, "(?:^|\s)" + [regex]::Escape($clave) + "=([^\s]+)")
    if ($m.Success) { return $m.Groups[1].Value }
    return ""
}

function Comprobar-Campos([string]$dump, $problemas, [hashtable]$esperados) {
    foreach ($clave in ($esperados.Keys | Sort-Object)) {
        $valor = Get-Campo $dump $clave
        $quiero = [string]$esperados[$clave]
        if ($valor -ne $quiero) {
            [void]$problemas.Add(("volcado: {0}={1} (esperado {2})" -f $clave, $valor, $quiero))
        }
    }
}

function Get-Lineas([string]$ruta) {
    return ((Get-TextoNormalizado $ruta) -split "`n")
}

function Guardar-Lineas([string]$ruta, [string[]]$lineas) {
    $texto = (($lineas -join "`n").TrimEnd("`n")) + "`n"
    $cod = New-Object System.Text.ASCIIEncoding
    [IO.File]::WriteAllText($ruta, $texto, $cod)
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
        Write-Host ("  [{0,3}] {1,-12} OK" -f $script:indice, $nombre) -ForegroundColor Green
    } else {
        $script:fallos++
        Write-Host ("  [{0,3}] {1,-12} FALLA" -f $script:indice, $nombre) -ForegroundColor Red
        foreach ($p in $problemas) {
            Write-Host ("        - {0}" -f $p) -ForegroundColor Red
        }
    }
}

# LOGIC2 -> LOGIC3: el archivo generado debe ser el patron.
Ejecutar-Caso "convertir" {
    param($dir, $problemas)
    $base = Join-Path $dir "base.logic"
    Copy-Item -LiteralPath (Join-Path $carpeta "base.logic") -Destination $base -Force
    $bmp = Join-Path $dir "convertir.bmp"
    $dump = Join-Path $dir "convertir.txt"
    $salida = $base + ".logic3"
    $cod = Ejecutar-Editor $dir @("--convertir-logic2", $base, "--captura", $bmp, "--volcar", $dump)
    if ($cod -ne 0) { [void]$problemas.Add("el editor devolvio el codigo $cod") }
    Comprobar-Campos $dump $problemas @{
        logic3 = "1"; escenas = "1"; objetos = "1"; eventos = "1";
        bloques = "1"; variables = "0"; ajustes = "320x240"
    }
    if (-not (Test-Path -LiteralPath $salida)) {
        [void]$problemas.Add("no se creo " + [IO.Path]::GetFileName($salida))
        return
    }
    $obtenido = Get-TextoNormalizado $salida
    if ($Actualizar) {
        $cod = New-Object System.Text.ASCIIEncoding
        [IO.File]::WriteAllText($patron3, $obtenido + "`n", $cod)
    } else {
        $esperado = Get-TextoNormalizado $patron3
        if ($obtenido -ne $esperado) {
            [void]$problemas.Add("el LOGIC3 generado no coincide con esperado.logic3")
            Mostrar-Diferencia ($esperado -split "`n") ($obtenido -split "`n")
        }
    }
}

# Abrir y guardar el LOGIC3 canonico debe devolver exactamente lo mismo.
Ejecutar-Caso "ida_vuelta" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "ida.logic3"
    Copy-Item -LiteralPath $patron3 -Destination $entrada -Force
    $salida = Join-Path $dir "vuelta.logic3"
    $bmp = Join-Path $dir "ida.bmp"
    $dump = Join-Path $dir "ida.txt"
    $cod = Ejecutar-Editor $dir @("--abrir-logic3", $entrada, "--guardar-logic3", $salida,
                                  "--captura", $bmp, "--volcar", $dump)
    if ($cod -ne 0) { [void]$problemas.Add("el editor devolvio el codigo $cod") }
    Comprobar-Campos $dump $problemas @{
        logic3 = "1"; escenas = "1"; objetos = "1"; eventos = "1";
        bloques = "1"; variables = "0"; ajustes = "320x240"; modulos = "2"
    }
    if (-not (Test-Path -LiteralPath $salida)) {
        [void]$problemas.Add("no se guardo el LOGIC3")
        return
    }
    if ((Get-TextoNormalizado $salida) -ne (Get-TextoNormalizado $entrada)) {
        [void]$problemas.Add("abrir y guardar cambia el proyecto")
    }
}

# Fase 28: valores tipados y listas con elementos sobreviven al guardado.
# La entrada no canonica (007, variable antes que traje) debe producir el
# fixture canonico; abrir ese fixture y guardarlo debe ser estable.
Ejecutar-Caso "tipos" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "entrada.logic3"
    $canon = Join-Path $dir "canon.logic3"
    $salida = Join-Path $dir "salida.logic3"
    $bmp = Join-Path $dir "tipos.bmp"
    $dump = Join-Path $dir "tipos.txt"
    Copy-Item -LiteralPath (Join-Path $carpeta "tipos_entrada.logic3") -Destination $entrada -Force
    $cod = Ejecutar-Editor $dir @("--abrir-logic3", $entrada, "--guardar-logic3", $canon,
                                  "--captura", $bmp, "--volcar", $dump)
    if ($cod -ne 0) { [void]$problemas.Add("el editor devolvio el codigo $cod") }
    $patron = Get-TextoNormalizado (Join-Path $carpeta "tipos.logic3")
    if (-not (Test-Path -LiteralPath $canon)) {
        [void]$problemas.Add("no se guardo el LOGIC3 canonico")
        return
    }
    if ((Get-TextoNormalizado $canon) -ne $patron) {
        [void]$problemas.Add("la serializacion tipada no coincide con tipos.logic3")
        Mostrar-Diferencia ($patron -split "`n") ((Get-TextoNormalizado $canon) -split "`n")
    }
    $reabrir = Join-Path $dir "reabrir.logic3"
    Copy-Item -LiteralPath (Join-Path $carpeta "tipos.logic3") -Destination $reabrir -Force
    $cod = Ejecutar-Editor $dir @("--abrir-logic3", $reabrir,
                                  "--guardar-logic3", $salida,
                                  "--captura", $bmp, "--volcar", $dump)
    if ($cod -ne 0) { [void]$problemas.Add("el editor devolvio el codigo $cod al reabrir") }
    if ((Get-TextoNormalizado $salida) -ne $patron) {
        [void]$problemas.Add("abrir y guardar tipos.logic3 no es estable")
    }
}

# LOGIC3 -> LOGIC2 de emergencia: texto canonico y modelo aun activo.
Ejecutar-Caso "exportar" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "export.logic3"
    Copy-Item -LiteralPath $patron3 -Destination $entrada -Force
    $salida = Join-Path $dir "export.logic"
    $bmp = Join-Path $dir "export.bmp"
    $dump = Join-Path $dir "export.txt"
    $cod = Ejecutar-Editor $dir @("--abrir-logic3", $entrada, "--exportar-logic2", $salida,
                                  "--captura", $bmp, "--volcar", $dump)
    if ($cod -ne 0) { [void]$problemas.Add("el editor devolvio el codigo $cod") }
    Comprobar-Campos $dump $problemas @{ logic3 = "1"; escenas = "1"; bloques = "1" }
    if (-not (Test-Path -LiteralPath $salida)) {
        [void]$problemas.Add("no se creo el LOGIC2 exportado")
        return
    }
    $obtenido = Get-TextoNormalizado $salida
    if ($Actualizar) {
        $cod = New-Object System.Text.ASCIIEncoding
        [IO.File]::WriteAllText($patron2, $obtenido + "`n", $cod)
    } else {
        $esperado = Get-TextoNormalizado $patron2
        if ($obtenido -ne $esperado) {
            [void]$problemas.Add("el LOGIC2 exportado no coincide con esperado-export.log")
            Mostrar-Diferencia ($esperado -split "`n") ($obtenido -split "`n")
        }
    }

    # La vista del LOGIC3 y la del LOGIC2 exportado deben ser la misma.
    $bmp2 = Join-Path $dir "export2.bmp"
    $cod = Ejecutar-Editor $dir @("--abrir-logica", $salida,
                                  "--captura", $bmp2, "--volcar", (Join-Path $dir "export2.txt"))
    if ($cod -ne 0) { [void]$problemas.Add("la recarga clasica devolvio el codigo $cod") }
    if ((Get-Sha256 $bmp2) -ne (Get-Sha256 $bmp)) {
        [void]$problemas.Add("la vista del LOGIC3 y la del LOGIC2 exportado difieren")
    }
}

# Compatibilidad: el LOGIC2 exportado y el fixture clasico no activan LOGIC3.
Ejecutar-Caso "clasico" {
    param($dir, $problemas)
    $exportado = Join-Path $dir "clasico.logic"
    Copy-Item -LiteralPath $patron2 -Destination $exportado -Force
    $bmp = Join-Path $dir "clasico.bmp"
    $dump = Join-Path $dir "clasico.txt"
    $cod = Ejecutar-Editor $dir @("--abrir-logica", $exportado, "--captura", $bmp, "--volcar", $dump)
    if ($cod -ne 0) { [void]$problemas.Add("el editor devolvio el codigo $cod") }
    Comprobar-Campos $dump $problemas @{
        logic3 = "0"; escenas = "0"; objetos = "0"; eventos = "0";
        bloques = "0"; variables = "0"; modulos = "2"; ajustes = "320x240"
    }

    $base = Join-Path $dir "base.logic"
    Copy-Item -LiteralPath (Join-Path $carpeta "base.logic") -Destination $base -Force
    $dump2 = Join-Path $dir "base.txt"
    $cod = Ejecutar-Editor $dir @("--abrir-logica", $base,
                                  "--captura", (Join-Path $dir "base.bmp"),
                                  "--volcar", $dump2)
    if ($cod -ne 0) { [void]$problemas.Add("el fixture clasico devolvio el codigo $cod") }
    Comprobar-Campos $dump2 $problemas @{
        logic3 = "0"; escenas = "0"; modulos = "1"; ajustes = "320x240"
    }
}

# La cabecera LOGIC3 basta para que el dialogo clasico cargue el modelo, y
# --guardar-logica escribe LOGIC3 si el proyecto activo lo es.
Ejecutar-Caso "detectar" {
    param($dir, $problemas)
    $entrada = Join-Path $dir "detectar.logic3"
    Copy-Item -LiteralPath $patron3 -Destination $entrada -Force
    $salida = Join-Path $dir "detectar.logic"
    $dump = Join-Path $dir "detectar.txt"
    $cod = Ejecutar-Editor $dir @("--abrir-logica", $entrada, "--guardar-logica", $salida,
                                  "--captura", (Join-Path $dir "detectar.bmp"),
                                  "--volcar", $dump)
    if ($cod -ne 0) { [void]$problemas.Add("el editor devolvio el codigo $cod") }
    Comprobar-Campos $dump $problemas @{
        logic3 = "1"; escenas = "1"; objetos = "1"; eventos = "1"; bloques = "1"
    }
    if (-not (Test-Path -LiteralPath $salida)) {
        [void]$problemas.Add("no se guardo el proyecto desde el dialogo clasico")
        return
    }
    if ((Get-TextoNormalizado $salida) -ne (Get-TextoNormalizado $entrada)) {
        [void]$problemas.Add("guardar desde el dialogo clasico no conserva el LOGIC3")
    }
}

# Un CRC32 que no coincide con el contenido se rechaza.
Ejecutar-Caso "crc" {
    param($dir, $problemas)
    [string[]]$lineas = Get-Lineas $patron3
    for ($i = 0; $i -lt $lineas.Count; $i++) {
        if ($lineas[$i] -like "CRC32 *") { $lineas[$i] = "CRC32 00000000" }
    }
    $malo = Join-Path $dir "crc.logic3"
    Guardar-Lineas $malo $lineas
    $dump = Join-Path $dir "crc.txt"
    $cod = Ejecutar-Editor $dir @("--abrir-logic3", $malo,
                                  "--captura", (Join-Path $dir "crc.bmp"),
                                  "--volcar", $dump)
    if ($cod -ne 0) { [void]$problemas.Add("el editor devolvio el codigo $cod") }
    Comprobar-Campos $dump $problemas @{ logic3 = "0"; escenas = "0"; objetos = "0" }
}

# Un CRC32 con formato invalido se rechaza aunque FIN exista.
Ejecutar-Caso "crc_texto" {
    param($dir, $problemas)
    [string[]]$lineas = Get-Lineas $patron3
    for ($i = 0; $i -lt $lineas.Count; $i++) {
        if ($lineas[$i] -like "CRC32 *") { $lineas[$i] = "CRC32 ZZ" }
    }
    $malo = Join-Path $dir "crc_texto.logic3"
    Guardar-Lineas $malo $lineas
    $dump = Join-Path $dir "crc_texto.txt"
    $cod = Ejecutar-Editor $dir @("--abrir-logic3", $malo,
                                  "--captura", (Join-Path $dir "crc_texto.bmp"),
                                  "--volcar", $dump)
    if ($cod -ne 0) { [void]$problemas.Add("el editor devolvio el codigo $cod") }
    Comprobar-Campos $dump $problemas @{ logic3 = "0"; escenas = "0" }
}

# Una version futura se rechaza con el modelo a cero.
Ejecutar-Caso "version" {
    param($dir, $problemas)
    [string[]]$lineas = Get-Lineas $patron3
    for ($i = 0; $i -lt $lineas.Count; $i++) {
        if ($lineas[$i] -like "version *") { $lineas[$i] = "version 99" }
    }
    $malo = Join-Path $dir "version.logic3"
    Guardar-Lineas $malo $lineas
    $dump = Join-Path $dir "version.txt"
    $cod = Ejecutar-Editor $dir @("--abrir-logic3", $malo,
                                  "--captura", (Join-Path $dir "version.bmp"),
                                  "--volcar", $dump)
    if ($cod -ne 0) { [void]$problemas.Add("el editor devolvio el codigo $cod") }
    Comprobar-Campos $dump $problemas @{ logic3 = "0"; escenas = "0" }
}

# Falta FIN: el archivo esta truncado y no se acepta.
Ejecutar-Caso "sin_fin" {
    param($dir, $problemas)
    [string[]]$lineas = Get-Lineas $patron3
    $corte = New-Object System.Collections.ArrayList
    foreach ($l in $lineas) {
        if ($l -eq "FIN") { break }
        [void]$corte.Add($l)
    }
    $malo = Join-Path $dir "sin_fin.logic3"
    Guardar-Lineas $malo ([string[]]$corte.ToArray())
    $dump = Join-Path $dir "sin_fin.txt"
    $cod = Ejecutar-Editor $dir @("--abrir-logic3", $malo,
                                  "--captura", (Join-Path $dir "sin_fin.bmp"),
                                  "--volcar", $dump)
    if ($cod -ne 0) { [void]$problemas.Add("el editor devolvio el codigo $cod") }
    Comprobar-Campos $dump $problemas @{ logic3 = "0"; escenas = "0" }
}

# Secciones desconocidas: se ignoran y el CRC32 es opcional.
Ejecutar-Caso "extension" {
    param($dir, $problemas)
    [string[]]$lineas = Get-Lineas $patron3
    $nuevas = New-Object System.Collections.ArrayList
    foreach ($l in $lineas) {
        if ($l -like "CRC32 *") { continue }
        if ($l -eq "FIN") {
            [void]$nuevas.Add("futuro ""campo"" 1 2 3")
            [void]$nuevas.Add("sonido 1 ""paso.wav"" 22050")
        }
        [void]$nuevas.Add($l)
    }
    $bueno = Join-Path $dir "extension.logic3"
    Guardar-Lineas $bueno ([string[]]$nuevas.ToArray())
    $dump = Join-Path $dir "extension.txt"
    $cod = Ejecutar-Editor $dir @("--abrir-logic3", $bueno,
                                  "--captura", (Join-Path $dir "extension.bmp"),
                                  "--volcar", $dump)
    if ($cod -ne 0) { [void]$problemas.Add("el editor devolvio el codigo $cod") }
    Comprobar-Campos $dump $problemas @{
        logic3 = "1"; escenas = "1"; objetos = "1"; eventos = "1"; bloques = "1"
    }
}

Write-Host ""
if ($script:fallos -eq 0) {
    Write-Host ("LOGIC3: {0} casos, todo OK" -f $script:total) -ForegroundColor Green
    exit 0
}
Write-Host ("LOGIC3: {0} casos, {1} con fallos" -f $script:total, $script:fallos) -ForegroundColor Red
exit 1
