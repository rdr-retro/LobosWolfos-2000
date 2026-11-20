# Subconjunto BIOS (fase 43)

Este documento fija que parte del lenguaje `lobo` puede arrancar desde la
imagen USB de loboswolfos. El editor avisa al compilar la BIOS cuando el
proyecto usa algo de fuera de la lista. Lo que no esta en la lista sigue
funcionando igual en `game.exe` (Windows); la imagen BIOS lo omite y el
resto del proyecto continua corriendo.

## Como se ejecuta

El editor compila cada funcion con `lobo_compilar(texto, programa,
LOBO_MODO_VM, ...)` y congela el resultado en una entrada nueva del blob
LGB1 llamada `vm.lbin`. El kernel no tiene parser: lee esa entrada y ejecuta
las instrucciones con la VM resumible de `lobovm.rs`.

`vm.lbin` lleva esta cabecera (todo little-endian):

```
"LVM1" u32 version=1 u32 num_programas u32 num_eventos
```

Cada programa:

```
i32 id_funcion u32 num_instrucciones u32 num_textos
[num_instrucciones x 12 bytes]: u8 op, 3 bytes de relleno, i32 valor, i32 extra
[num_textos x 64 bytes]: texto terminado en cero
```

Cada evento declarado son 4 x `i32`: `tipo`, `parametro`, `script`,
`objeto`. `script` es el id de la funcion LOGIC2; `-1` reparte el evento a
todos los scripts. `objeto` es la id de entidad; `0` significa el jugador.

El campo `parametro` de un evento `tecla` se empaqueta asi:

* bits 0..7: mascara `MOTOR_ENT_*` (por ejemplo `0x02` para derecha);
* bits 8..15: ranura extendida mas 1 (`0` si la tecla no tiene ranura);
* bits 16..23: flanco (`0` mantener, `1` pulsar, `2` soltar).

El planificador imita `motor/eventos.c`: `inicio` y `escena` se entregan una
vez, `fotograma` y la tecla mantenida se repiten cada fotograma con
`fotograma()` (que descuenta las esperas), `cada N` dispara en N, 2N...,
`tocar` solo al entrar en contacto, y `recibir`/`clonar` los emiten los
opcodes `mensaje`/`enviar` y `crear`. Los disparos que no se repiten dejan
sus esperas corriendo con `reanudar()` aunque no llegue un evento nuevo.

## Opcodes soportados

Los numeros son los de `motor/lobo.h`. No se han reordenado: las suites
anteriores y los programas ya compilados conservan su sentido.

| Numeros | Operaciones |
| --- | --- |
| 0..22 | `num`, `var`, `tecla`, `chocar`, `cerrar`, aritmetica, comparaciones, `y`, `o`, `no`, `asigna`, saltos y `pop` |
| 23 | `esperar` (VM resumible) |
| 26..27 | `global` y `guardar_global` |
| 39 | `mensaje` |
| 40 | `detener` |
| 42..44 | `mi`, `guardar_mi`, `crear` |
| 45..47 | `borrar_entidad`, `mover_entidad`, `enviar` |
| 49 | `reiniciar_escena` |
| 50..55 | `abs`, `min`, `max`, `limitar`, `redondear`, `aleatorio` |
| 56..59 | `texto`, `longitud`, `letra`, `unir` |
| 60..63 | `toca`, `toca_tag`, `distancia`, `caja_de` |
| 64..71 | teclas con flanco, estado de raton, rueda y dentro |
| 72..74 | `hud_vidas`, `hud_puntos`, `hud_dialogo` |
| 81..85 | `sonar`, `musica`, `detener_sonido`, `volumen`, `volumen_actual` |

## Entidades, eventos y colisiones

* Entidad 1 es el jugador; el juego sincroniza su posicion con el sprite.
* `crear("tag", x, y)` devuelve una id nueva; `borrar` nunca borra al jugador.
* `mi.x`, `mi.y`, `mi.vida`, `mi.visible`, `mi.traje`, `mi.id`,
  `mi.escena`, `mi.ancho`, `mi.alto`, `mi.capa` y `mi.vars[N]` caben en el
  subconjunto.
* Colisiones AABB entre entidades y `caja_de`, `distancia`, `toca` y
  `toca_tag`. La colision contra el mapa (`chocar`) es la misma caja del
  sprite que usa el motor de Windows.
* Eventos: `inicio` (solo el primer fotograma), `fotograma`, `tecla`,
  `recibir`, `tocar`, `cada`, `clonar` y `escena`. El planificador encola
  hasta 64 eventos por fotograma.
* Trajes: 8 colores fijos; las entidades se dibujan por capa entre el mapa
  y el HUD.

## Fuera del subconjunto

El editor avisa de estos bloques y construcciones al compilar la BIOS:

* listas (`lista`, `anadir`, `insertar`, `borrar`, `ordenar`...);
* llamadas a funciones de usuario y `retornar` (cada funcion LOGIC2 es un
  programa VM independiente);
* `clonar` y `enviar_a` con destino explicito;
* guardados, records y ajustes (fase 40);
* escenas avanzadas y efectos (fase 41) salvo `reiniciar_escena`;
* HUD avanzado (`hud_texto`, rectangulos, barras, imagenes, botones y
  plantillas) y efectos de sonido avanzados;
* cualquier opcode nuevo que no aparezca en la tabla anterior.

## Avisos del editor

`--bios-avisos <ruta>` escribe los avisos de la compilacion BIOS en un
archivo de texto, uno por linea, con el mismo orden en que aparecen los
bloques en el proyecto. Sin la flag no se escribe nada nuevo. Los avisos
tambien se muestran en el cuadro de compilacion cuando se elige
"Compilar para BIOS".

El marcador visual de los bloques no portables es una esquina rayada en
el lienzo; no cambia el id LOGIC3 ni el tamano del bloque.
