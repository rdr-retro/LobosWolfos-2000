# Ejemplos y plantillas (fase 44)

Este directorio guarda el proyecto canonico `primer_juego.logic3`. Es un
LOGIC3 real: se abre con `ARCHIVO > ABRIR PROYECTO` o con
`--abrir-logic3 ejemplos\primer_juego.logic3`, y su linea `CRC32` se valida
al cargar.

## Que contiene

- Una escena, `Nivel 1`, de 640x480.
- Un jugador con dos variables: `monedas` y `ganado`.
- Tres monedas y una meta, todas con bloques `codigo.lobo`.
- Movimiento con las flechas y comprobacion de colisiones en cada fotograma.

## Minijuego LOGIC2

`minijuego.logic` es un proyecto clásico jugable con el motor actual: usa
`maps\mapa.mav`, un item de mapa y un botón `FUNCIONES` con código lobo. No
necesita LOGIC3, bloques ni archivos nuevos. Las flechas mueven al lobo, el
mapa detiene los muros, `Espacio` vuelve al inicio y llegar al extremo derecho
del pasillo cierra la partida.

Desde la raíz del repositorio:

```
LobosWolfos2000.exe --abrir-logica ejemplos\minijuego.logic
motor\game.exe --logica ejemplos\minijuego.logic
```

Al abrirlo en el gestor, el inspector del constructor enumera los dos
elementos del proyecto (`mapa.mav` y la funcion `Laberinto simple`) aunque la
capa Programa no tenga piezas que arrastrar. El boton **COMPAT.** muestra los
mismos elementos en el lienzo clásico.

El caso `pruebas\conformidad\ejemplo_minijuego.args` congela la carga del
ejemplo y ocho fotogramas bajando; el hash del BMP comprueba que el mapa y el
sprite se dibujan.

El mismo proyecto LOGIC3 se puede generar desde el editor con:

- `Ctrl+Mayus+N` abre el selector `Juego nuevo` y pulsar `Plataformas`.
- `F2` abre el tutorial del estudio.
- `F3`, sobre un bloque seleccionado del lienzo, abre su ayuda contextual.

Desde la linea de comandos:

```
LobosWolfos2000.exe --juego-nuevo --plantilla plataformas
LobosWolfos2000.exe --juego-nuevo
LobosWolfos2000.exe --tutorial
LobosWolfos2000.exe --ayuda-bloque codigo.lobo
```

El generador escribe siempre `logic\primer_juego.logic3` y deja el proyecto
abierto en el gestor. `AYUDA > CREAR JUEGO DE EJEMPLO` y `--ejemplo` siguen
creando el proyecto clasico de LOGIC2; no se han tocado.

## Plantillas incluidas

| Id | Nombre | Punto de partida |
| --- | --- | --- |
| `plataformas` | Plataformas | Nivel, jugador, monedas y meta |
| `shooter` | Shooter | Nave, dianas y salida |
| `puzzle` | Puzzle | Cubo, llaves y puerta |
| `aventura` | Aventura | Explorador, pistas y salida |
| `laberinto` | Laberinto | Pasillos, pistas y salida |

Las cinco usan el mismo esqueleto LOGIC3. Plataformas es la que cumple el
criterio completo de juego jugable; las demas sirven para empezar y cambiar
nombres, posiciones y codigo.

## Assets libres

No hay dependencias externas. El ejemplo no usa imagenes, fuentes ni sonidos
de terceros:

- Los trajes se declaran vacios y el motor dibuja cajas de color.
- Los tiles y mapas se generan dentro del proyecto si se necesitan.
- La tipografia es la fuente bitmap incluida en el editor.

Cualquier asset que anadas tu mismo debe ser de tu propiedad o de licencia
libre (por ejemplo CC0 o dominio publico) y debe guardarse dentro de la
carpeta del proyecto para que el juego compilado lo lleve incluido.
