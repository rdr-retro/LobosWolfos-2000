# comun.ps1 - utilidades compartidas por las suites de pruebas.
# Sin acentos ni caracteres no ASCII: Windows PowerShell 5.1 lee los .ps1
# como ANSI si no llevan BOM.

function Get-Sha256([string]$ruta) {
    if (-not (Test-Path -LiteralPath $ruta)) { return "" }
    $h = Get-FileHash -LiteralPath $ruta -Algorithm SHA256
    return $h.Hash.ToLowerInvariant()
}

function Get-TextoNormalizado([string]$ruta) {
    if (-not (Test-Path -LiteralPath $ruta)) { return "" }
    $t = [IO.File]::ReadAllText($ruta)
    $t = $t.Replace("`r`n", "`n").Replace("`r", "`n")
    return $t.TrimEnd("`n")
}

function Guardar-TextoNormalizado([string]$ruta, [string]$texto) {
    $t = $texto.Replace("`r`n", "`n").Replace("`r", "`n")
    $t = $t.TrimEnd("`n") + "`n"
    $cod = New-Object System.Text.ASCIIEncoding
    [IO.File]::WriteAllText($ruta, $t, $cod)
}

function Leer-Argumentos([string]$ruta) {
    if (-not (Test-Path -LiteralPath $ruta)) { return @() }
    $texto = [IO.File]::ReadAllText($ruta)
    $lista = New-Object System.Collections.ArrayList
    foreach ($m in [regex]::Matches($texto, '"([^"]*)"|(\S+)')) {
        if ($m.Groups[1].Success) { [void]$lista.Add($m.Groups[1].Value) }
        else { [void]$lista.Add($m.Groups[2].Value) }
    }
    return $lista.ToArray()
}

# Lanza un ejecutable y espera. Devuelve el codigo de salida.
# Los argumentos se citan uno a uno para que las rutas con espacios
# lleguen enteras (Start-Process une la lista con espacios).
function Ejecutar-Programa([string]$exe, [string[]]$argumentos, [string]$carpeta) {
    $partes = @()
    foreach ($a in $argumentos) {
        if ($a -match '[\s"]') { $partes += '"' + ($a -replace '"', '\"') + '"' }
        else { $partes += $a }
    }
    $linea = $partes -join ' '
    $p = Start-Process -FilePath $exe -ArgumentList $linea `
                       -WorkingDirectory $carpeta -WindowStyle Hidden `
                       -Wait -PassThru
    return $p.ExitCode
}

# Imprime las primeras diferencias entre dos listas de lineas.
function Mostrar-Diferencia([string[]]$esperado, [string[]]$obtenido, [int]$maximo = 12) {
    $n = [Math]::Max($esperado.Count, $obtenido.Count)
    $vistos = 0
    for ($i = 0; $i -lt $n -and $vistos -lt $maximo; $i++) {
        $e = "<falta>"
        $o = "<falta>"
        if ($i -lt $esperado.Count) { $e = $esperado[$i] }
        if ($i -lt $obtenido.Count) { $o = $obtenido[$i] }
        if ($e -ne $o) {
            Write-Host ("    linea {0}: esperado [{1}] / obtenido [{2}]" -f ($i + 1), $e, $o) -ForegroundColor DarkYellow
            $vistos++
        }
    }
    if ($n -gt $maximo) {
        Write-Host ("    ... ({0} lineas en total)" -f $n) -ForegroundColor DarkGray
    }
}
