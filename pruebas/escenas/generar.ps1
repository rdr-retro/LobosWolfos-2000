# generar.ps1 - crea los proyectos LOGIC3 de la suite de la fase 41.
#
# El formato y el CRC son los mismos que escribe el editor (src/logic3.c):
# se calcula el CRC32 sobre todo el texto hasta "FIN\n", inclusive, y se
# anade una linea "CRC32 XXXXXXXX". No se toca el lector ni el escritor.
# Los campos "codigo" se escriben en una sola linea con \n literales: el
# lector LOGIC3 lee el valor de campo de una sola linea.

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

function Escribir-Proyecto([string]$nombre, [string]$lineas) {
    $texto = $lineas + "`n"
    $crc = CRC32-Texto $texto
    $texto += ("CRC32 {0:X8}`n" -f $crc)
    [System.IO.File]::WriteAllText((Join-Path $dir ($nombre + ".logic3")), $texto,
                                   (New-Object System.Text.UTF8Encoding($false)))
    Write-Host ("  {0}.logic3 CRC32 {1:X8}" -f $nombre, $crc)
}

$nivel1 = @'
-- Mapa propio de la fase 41: nivel 1.
return {
    version = 2,
    tile_size = 32,
    tiles_dir = "tiles",
    tiles = {
        { x = 0, y = 0, image = "tile_1.png", w = 32, h = 32, solido = false },
        { x = 1, y = 0, image = "tile_1.png", w = 32, h = 32, solido = false },
        { x = 2, y = 0, image = "tile_1.png", w = 32, h = 32, solido = false },
        { x = 0, y = 1, image = "tile_1.png", w = 32, h = 32, solido = false },
        { x = 1, y = 1, image = "tile_1.png", w = 32, h = 32, solido = false },
        { x = 2, y = 1, image = "tile_1.png", w = 32, h = 32, solido = false }
    }
}
'@
$nivel2 = @'
-- Mapa propio de la fase 41: nivel 2.
return {
    version = 2,
    tile_size = 32,
    tiles_dir = "tiles",
    tiles = {
        { x = 0, y = 0, image = "tile_2.png", w = 32, h = 32, solido = false },
        { x = 1, y = 0, image = "tile_2.png", w = 32, h = 32, solido = false },
        { x = 2, y = 0, image = "tile_2.png", w = 32, h = 32, solido = false },
        { x = 0, y = 1, image = "tile_2.png", w = 32, h = 32, solido = false },
        { x = 1, y = 1, image = "tile_2.png", w = 32, h = 32, solido = false },
        { x = 2, y = 1, image = "tile_2.png", w = 32, h = 32, solido = false }
    }
}
'@
[System.IO.File]::WriteAllText((Join-Path $dir "nivel1.mav"), $nivel1 + "`n",
                               (New-Object System.Text.UTF8Encoding($false)))
[System.IO.File]::WriteAllText((Join-Path $dir "nivel2.mav"), $nivel2 + "`n",
                               (New-Object System.Text.UTF8Encoding($false)))

$builtins = @'
# Fase 41: builtins de escenas sin callbacks (banco --vm-test).
entero a = cambiar_escena(2)
entero b = escena_actual()
entero c = transicion(1, 4)
entero d = camara_objetivo(1)
entero e = camara_zoom(2)
entero f = camara_limites(0, 0, 320, 200)
entero g = camara_suavizar(3)
entero h = parpadeo(1, 6)
entero i = temblor(2, 5)
entero j = flash(16711680, 3)
entero k = fade(255, 4, 2)
entero r = reiniciar_escena()
reiniciar_escena()
x = a + b + c + d + e + f + g + h + i + j + k + r
y = 16
'@
[System.IO.File]::WriteAllText((Join-Path $dir "builtins.lobo"), $builtins + "`n",
                               (New-Object System.Text.UTF8Encoding($false)))

# Cada bloque y su campo se escriben como elementos separados del array.
# El codigo va entre comillas simples para que \n sea literal (barra + n).
$dosNiveles = @(
    'LOGIC3'
    'version 3'
    'proyecto "Fase 41" 320 200 1 60 3 00112233'
    'carpetas "tiles" "maps" "trajes" "sonidos"'
    'camara objetivo 1 limites 0 0 320 200 zoom 1'
    'variable global progreso entero 0'
    'escena 1 "Nivel 1" "nivel1.mav" 320 200 00112233'
    'escena 2 "Nivel 2" "nivel2.mav" 320 200 22334455'
    'objeto 1 "Lobo" escena 1 x 16 y 16 w 24 h 24 visible 1 capa 0'
    '  traje 0 "" 0 0 0 0'
    '  evento 1 inicio'
    '  evento 2 fotograma'
    'objeto 2 "Lobo2" escena 2 x 32 y 32 w 24 h 24 visible 1 capa 0'
    '  traje 0 "" 0 0 0 0'
    '  evento 3 inicio'
    'bloque 1 evento 1 padre 0 orden 0 tipo "codigo.lobo"'
    '  campo "codigo" "progreso = progreso + 10\ntransicion(1, 4)\ncamara_limites(0, 0, 320, 200)\ncamara_zoom(2)\nx = 16\ny = 16"'
    'bloque 2 evento 2 padre 0 orden 0 tipo "codigo.lobo"'
    '  campo "codigo" "si escena_actual() == 1\n  cambiar_escena(2)\nfin"'
    'bloque 3 evento 3 padre 0 orden 0 tipo "codigo.lobo"'
    '  campo "codigo" "parpadeo(1, 6)\ntemblor(2, 5)\nflash(16711680, 3)\nfade(255, 4, 2)\ncamara_objetivo(1)\ncamara_suavizar(3)\nx = 32\ny = 32"'
    'FIN'
)
Escribir-Proyecto "dos_niveles" ($dosNiveles -join "`n")

$camaraEfectos = @(
    'LOGIC3'
    'version 3'
    'proyecto "Fase 41 camara" 320 200 1 60 3 00112233'
    'carpetas "tiles" "maps" "trajes" "sonidos"'
    'camara objetivo 0 limites 0 0 320 200 zoom 1'
    'escena 1 "Nivel 1" "nivel1.mav" 320 200 00112233'
    'objeto 1 "Lobo" escena 1 x 16 y 16 w 24 h 24 visible 1 capa 0'
    '  traje 0 "" 0 0 0 0'
    '  evento 1 inicio'
    'bloque 1 evento 1 padre 0 orden 0 tipo "codigo.lobo"'
    '  campo "codigo" "camara_objetivo(1)\ncamara_zoom(2)\ncamara_limites(0, 0, 320, 200)\ncamara_suavizar(3)\nparpadeo(1, 6)\ntemblor(2, 5)\nflash(16711680, 3)\nfade(255, 4, 2)\nx = 16\ny = 16"'
    'FIN'
)
Escribir-Proyecto "camara_efectos" ($camaraEfectos -join "`n")

Write-Host "Listo: fixtures de la fase 41 en $dir"
