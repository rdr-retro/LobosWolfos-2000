# generar.ps1 - crea el proyecto LOGIC3 de la suite de runtime jerarquico.
#
# El formato y el CRC son los mismos que escribe el editor (src/logic3.c):
# CRC32 sobre todo el texto hasta "FIN\n", inclusive, mas una linea final
# "CRC32 XXXXXXXX". Los campos "codigo" llevan \n literales.

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

function Escribir-Proyecto([string]$nombre, [string[]]$lineas) {
    $texto = ($lineas -join "`n") + "`n"
    $crc = CRC32-Texto $texto
    $texto += ("CRC32 {0:X8}`n" -f $crc)
    [System.IO.File]::WriteAllText((Join-Path $dir ($nombre + ".logic3")), $texto,
                                   (New-Object System.Text.UTF8Encoding($false)))
    Write-Host ("  {0}.logic3 CRC32 {1:X8}" -f $nombre, $crc)
}

$lineas = @(
    'LOGIC3'
    'version 3'
    'proyecto "Runtime jerarquico" 320 200 1 60 3 00112233'
    'carpetas "tiles" "maps" "trajes" "sonidos"'
    'escena 1 "Nivel 1" "" 320 200 00112233'
    'escena 20 "Nivel 2" "" 320 200 22334455'
    'objeto 21 "Hijo" escena 1 x 5 y 7 w 16 h 16 visible 1 capa 1 padre 20'
    '  traje 0 "" 0 0 0 0'
    'objeto 20 "Padre" escena 1 x 100 y 50 w 32 h 32 visible 1 capa 0 padre 100002'
    '  traje 0 "" 0 0 0 0'
    'objeto 100002 "Jugador" escena 1 x 16 y 16 w 24 h 24 visible 1 capa 0'
    '  traje 0 "" 0 0 0 0'
    '  evento 1 escena 1'
    '  evento 2 escena_salir 1'
    '  evento 3 inicio'
    'objeto 22 "EnNivel2" escena 20 x 16 y 16 w 16 h 16 visible 1 capa 0'
    '  traje 0 "" 0 0 0 0'
    '  evento 4 escena 20'
    'bloque 1 evento 1 padre 0 orden 0 tipo "codigo.lobo"'
    '  campo "codigo" "x = 111\ny = 112"'
    'bloque 2 evento 2 padre 0 orden 0 tipo "codigo.lobo"'
    '  campo "codigo" "x = 121\ny = 122"'
    'bloque 3 evento 3 padre 0 orden 0 tipo "codigo.lobo"'
    '  campo "codigo" "cambiar_escena(20)"'
    'bloque 4 evento 4 padre 0 orden 0 tipo "codigo.lobo"'
    '  campo "codigo" "x = 141\ny = 142"'
    'bloque 5 evento 1 padre 0 orden 0 tipo "sistema.opciones"'
    '  campo "programa" "1"'
    '  campo "jugador_implicito" "0"'
    '  campo "auto_compilar" "1"'
    'bloque 6 evento 1 padre 0 orden 1 tipo "sistema.pieza"'
    '  campo "ui" "1"'
    '  campo "pieza" "sistema.escena"'
    '  campo "id" "1"'
    '  campo "xy" "160,80"'
    '  campo "nombre" "Escenario"'
    '  campo "enlace" "0"'
    '  campo "valor" ""'
    '  campo "valor2" ""'
    'bloque 7 evento 1 padre 0 orden 2 tipo "sistema.pieza"'
    '  campo "ui" "1"'
    '  campo "pieza" "sistema.actor"'
    '  campo "id" "2"'
    '  campo "xy" "360,80"'
    '  campo "nombre" "Jugador"'
    '  campo "enlace" "3"'
    '  campo "valor" "Jugador"'
    '  campo "valor2" "lobo"'
    'bloque 8 evento 1 padre 0 orden 3 tipo "sistema.pieza"'
    '  campo "ui" "1"'
    '  campo "pieza" "sistema.teclado"'
    '  campo "id" "3"'
    '  campo "xy" "360,190"'
    '  campo "nombre" "Teclado"'
    '  campo "enlace" "4"'
    '  campo "valor" "1"'
    '  campo "valor2" "teclado"'
    'bloque 9 evento 1 padre 0 orden 4 tipo "sistema.pieza"'
    '  campo "ui" "1"'
    '  campo "pieza" "sistema.accion.movimiento"'
    '  campo "id" "4"'
    '  campo "xy" "360,300"'
    '  campo "nombre" "Movimiento"'
    '  campo "enlace" "3"'
    '  campo "valor" "mover"'
    '  campo "valor2" "mantener"'
    'FIN'
)

Escribir-Proyecto "dos_escenas" $lineas

# Variante sin cambio de escena: congela la jerarquia inicial, con el hijo
# declarado antes que el padre, para comprobar la instanciacion por niveles.
$inicial = @(
    $lineas | ForEach-Object {
        if ($_ -eq '  campo "codigo" "cambiar_escena(20)"') {
            '  campo "codigo" "x = 131\ny = 132"'
        } else {
            $_
        }
    }
)
Escribir-Proyecto "jerarquia_inicial" $inicial
