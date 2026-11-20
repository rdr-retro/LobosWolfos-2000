# generar.ps1 - crea los proyectos LOGIC3 de la suite de la fase 40.
#
# El formato y el CRC son los mismos que escribe el editor (src/logic3.c):
# se calcula el CRC32 sobre todo el texto hasta "FIN\n", inclusive, y se
# anade una linea "CRC32 XXXXXXXX". No se toca el lector ni el escritor.

$ErrorActionPreference = "Stop"
$dir = Split-Path -Parent $MyInvocation.MyCommand.Path

function CRC32-Texto([string]$texto) {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($texto)
    $tabla = New-Object uint32[] 256
    for ($i = 0; $i -lt 256; $i++) {
        $c = [uint32]$i
        for ($k = 0; $k -lt 8; $k++) {
            if (($c -band 1) -ne 0) { $c = [uint32]3988292384 -bxor ($c -shr 1) }
            else { $c = $c -shr 1 }
        }
        $tabla[$i] = $c
    }
    $crc = [uint32]4294967295
    foreach ($b in $bytes) {
        $crc = $tabla[[int](($crc -bxor [uint32]$b) -band 0xFF)] -bxor ($crc -shr 8)
    }
    return (([uint32]4294967295) -bxor $crc)
}

function Escribir-Proyecto([string]$nombre, [string]$codigo) {
    $lineas = @(
        "LOGIC3",
        "version 3",
        'proyecto "Fase 40" 320 200 1 60 3 00112233',
        'carpetas "tiles" "maps" "trajes" "sonidos"',
        'escena 1 "Escena" "" 320 200 00112233',
        'objeto 1 "Lobo" escena 1 x 16 y 16 w 24 h 24 visible 1 capa 0',
        '  traje 0 "" 0 0 0 0',
        '  evento 1 inicio',
        'bloque 1 evento 1 padre 0 orden 0 tipo "codigo.lobo"'
    )
    $codigo = $codigo.Replace("`r`n", "`n")
    $campo = $codigo.Replace("\", "\\").Replace('"', '\"').Replace("`n", "\n")
    $lineas += '  campo "codigo" "' + $campo + '"'
    $lineas += "FIN"
    $texto = ($lineas -join "`n") + "`n"
    $crc = CRC32-Texto $texto
    $texto += ("CRC32 {0:X8}`n" -f $crc)
    [System.IO.File]::WriteAllText((Join-Path $dir ($nombre + ".logic3")), $texto,
                                   (New-Object System.Text.UTF8Encoding($false)))
    Write-Host ("  {0}.logic3 CRC32 {1:X8}" -f $nombre, $crc)
}

Escribir-Proyecto "builtins" @'
guardar("vidas", 3)
x = cargar("vidas")
borrar_save()
record("puntos", 500)
a = record_actual("puntos")
ajuste("volumen", 70)
b = ajuste_actual("volumen")
'@

Escribir-Proyecto "guardar" @'
guardar("vidas", 3)
guardar("puntos", 120)
record("puntos", 500)
ajuste("volumen", 70)
x = 16
y = 16
'@

Escribir-Proyecto "cargar" @'
a = cargar("vidas")
guardar("vidas", a + 1)
b = record("puntos", 120)
x = a * 10
y = b / 10
'@

Escribir-Proyecto "corrupto" @'
x = cargar("vidas") * 10
y = record_actual("puntos")
'@

Escribir-Proyecto "borrar" @'
guardar("vidas", 3)
borrar_save()
'@

Escribir-Proyecto "neutro" @'
guardar("vidas", 3)
x = 16
y = 16
'@
