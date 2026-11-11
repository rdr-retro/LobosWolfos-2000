# LobosWolfos 2000

Editor de texturas ligero para **Windows 2000 y Windows XP** de 32 bits. Está
escrito en C con Win32/GDI puro y un búfer propio de 32 bits; GDI+ solo se usa
para cargar y guardar PNG.

## Características

- Lienzo con zoom (hasta 1000 %), capas y deshacer/rehacer.
- Pincel, lápiz, aerógrafo, cubo de relleno y selector de color.
- Figuras: línea, rectángulo, rectángulo redondeado, elipse, polígono y curva
  de Bézier.
- Selección rectangular y de forma libre, con arrastre y redimensionado.
- Herramienta de texto con varias fuentes y tamaños.
- Patrón de textura repetido, visor de mosaico y guardado de tiles en PNG.
- Pestañas de modo: Pintar, Tiles, Mapa y patrón comparten el mismo documento;
  cada pestaña conserva su vista, zoom y desplazamiento.
- Gestor de lógica: paneles izquierdo, derecho e inferior (los laterales del
  mismo ancho), un botón de item arrastrable y un lienzo desplazable con
  rejilla. Los items son mapas `.mav` de la carpeta `maps` junto al ejecutable:
  se eligen con un clic y se muestran con miniatura y nombre. Los elementos se
  pueden mover y se acoplan entre sí por sus lados con efecto imán. El panel
  izquierdo lista los elementos del lienzo; hay una sola lógica y se guarda
  como archivo `.logic`.
- Apertura y exportación de PNG/BMP y mapas `.mav`.
- Interfaz flotante de opciones, panel de capas y menús contextuales.

## Compilar

Requiere MinGW-w64 para 32 bits (i686) con `gcc` y `windres`.

```bat
build.bat [ruta\a\mingw32\bin]
```

Si no se indica la ruta, se busca la variable `MINGW32` o la ruta usada durante
el desarrollo. El resultado es `LobosWolfos2000.exe`.

También se puede compilar desde PowerShell:

```powershell
.\build.ps1
```

## Motor egine (`motor/game.exe`)

El motor del juego tiene su propia carpeta `motor/` y se compila aparte, desde
ella:

```bat
cd motor
build.bat [ruta\a\mingw32\bin]
```

El resultado es `motor/game.exe` (Win32, 32 bits). Lánzalo **desde la raíz del
proyecto** (la carpeta con `maps/` y `tiles/`), no desde `motor/`:

```bat
motor\game.exe
```

Abre una ventana de 640x480, carga el mapa del proyecto (el del `.logic` de la
lógica o el primero que haya en `maps/`) y lo dibuja con el **núcleo en
ensamblador** (`motor/nucleo32.s`: limpiar y blit con MMX, render del mapa y
colisiones AABB). Mueve el sprite con los cursores o `WASD`; `Escape` cierra.

Si el ejecutable fue producido por **Compilar para Windows** del editor, el
proyecto va incrustado al final del propio `.exe` (pie `LGB1`) y el motor lo
carga desde ahí sin tocar el disco: funciona igual en cualquier carpeta.

Para pruebas automáticas sin ventana (captura determinista a BMP):

```bat
motor\game.exe --captura captura.bmp --fotogramas 60 --tecla derecha
```

Opciones: `--logica ruta` (proyecto `.logic`), `--mapa ruta` (un `.mav`
directo), `--captura ruta`, `--fotogramas N` y `--tecla nombre`
(izquierda/derecha/arriba/abajo/saltar).

`motor/prueba.c` es un banco de pruebas de consola del núcleo; se compila
aparte siguiendo las instrucciones de su cabecera y se ejecuta desde la raíz
del proyecto.

## Kernel loboswolfos (`loboswolfos/`)

El kernel propio del proyecto — la reducción de **rocky** a lo que el juego
necesita — vive en `loboswolfos/`, junto al motor. Arranca desde la BIOS de
un PC (MBR + segundo tramo en ensamblador), levanta un modo VESA VBE con
framebuffer lineal (640x480 a 32 bits o el primero disponible), activa la
entrada PS/2 (teclado y ratón) y el reloj de 100 Hz, y corre el juego del
proyecto **sobre el núcleo del motor en ensamblador x86_64** (`nucleo64.s`,
SSE2): el mismo juego que `game.exe`, con el mapa y sus tiles PNG cargados
del propio disco de arranque.

Compilarlo y probarlo requiere Rust (destino `x86_64-unknown-none`) y QEMU.
Desde la raíz del proyecto:

```bat
cargo run --manifest-path loboswolfos\xtask\Cargo.toml -- build
cargo run --manifest-path loboswolfos\xtask\Cargo.toml -- run
```

`run` abre QEMU con ventana: el juego carga el mapa del proyecto y se juega
con los cursores o `WASD` (Escape sale). Otros comandos del constructor:

- `run-serial` — QEMU sin ventana, con la serie por la consola (Ctrl+C para
  salir).
- `probar [segundos]` — prueba automática del arranque: QEMU sin ventana
  volcando la serie a `loboswolfos\build\serial.log`, que se imprime al
  terminar.
- `probar-juego` — prueba automática del juego: mueve el sprite con
  `sendkey` desde el monitor de QEMU (sin teclado) y comprueba en la serie
  que el mapa cargó, que los tiles decodificaron y que el sprite se movió.
- `clean` — borra los destinos de compilación y la imagen.

`build` genera `loboswolfos\loboswolfos.img`, la imagen USB de 32 MiB con el
trazado final: MBR en LBA 0, segundo tramo en 1..4, kernel en 5.. y la zona
del juego en LBA 2048 con el **blob LGB1 del proyecto** (el mismo formato
que viaja pegado al `game.exe` compilado: `proyecto.logic`, `maps\*.mav` y
`tiles\*.png` de la raíz del proyecto). El ensamblador de arranque es el de
rocky (`bootusb.s`, `bootusb2.s`) sin cambios de trazado.

## Uso

Ejecuta `LobosWolfos2000.exe`. Los tiles que guardes se almacenan en la carpeta
`tiles` junto al ejecutable.

Las pestañas representan modos de trabajo sobre el mismo documento. El botón
`+` (o `Ctrl+T`) duplica el modo actual en otra pestaña; después puedes cambiar
esa pestaña a Tiles, Mapa o patrón sin tocar el documento. `Nuevo` y `Abrir`
sustituyen el documento compartido, no crean pestañas nuevas.

El menú **MODO** incluye **Modo pintar**, que cierra el visor de patrón, sale del
modo tiles o del editor de mapas y vuelve al lienzo normal, y **Gestor de
lógica**, que abre un espacio de trabajo con los paneles izquierdo, derecho e
inferior (el derecho un poco más ancho), manteniendo las pestañas y la
barra de menús. Al elegir **Gestor de lógica** de nuevo se vuelve a Pintar.

El panel izquierdo muestra la lista de elementos del lienzo (`Elemento 1`,
`Elemento 2`…) y ocupa todo el panel. Los elementos unidos entre sí por el
efecto imán aparecen indentados bajo el elemento del grupo al que pertenecen;
la flecha del grupo permite plegar o desplegar sus hijos en la lista. Si hay
más elementos que filas visibles, la rueda sobre la lista los desplaza.

La opción **Guardar logica .logic** del menú **ARCHIVO** exporta la lógica a
un archivo de texto con su desplazamiento y cada elemento (posición, tamaño,
identificador, estado plegado, mapa asociado y función). La opción **Abrir
lógica .logic**, justo encima, carga uno de esos archivos de vuelta: la
lógica del archivo sustituye a la activa y la vista entra al gestor para
verla cargada.

Dentro del gestor, el panel derecho contiene el botón del **item**. Un clic
(sin arrastrar) abre el selector de items: la lista de mapas `.mav` de la
carpeta `maps` junto al ejecutable, cada uno con su miniatura y su nombre.
Elige uno con un clic y el botón pasa a mostrarlo; arrástralo hasta el lienzo
para insertarlo como elemento con su miniatura y su nombre. Sin item elegido,
el arrastre inserta un elemento vacío. Si hay más mapas que filas, la rueda
desplaza la lista y `Escape` la cierra.

Debajo del botón del item está la sección **FUNCIONES**, con rectángulos
arrastrables para las funciones lógicas del juego. De momento solo existe
**egine**, el motor del juego: se arrastra al lienzo para colocarlo como
elemento (con su banda MOTOR y el nombre en azul); un clic sin arrastrar
recuerda cómo se usa. La opción **Guardar logica .logic** también guarda la
función de cada elemento, y **Abrir lógica .logic** la recupera al cargar.

Los elementos ya colocados también se pueden arrastrar, y un clic derecho
sobre uno de ellos lo elimina. Sus lados tienen
efecto imán: al acercarlos a otro elemento se acoplan y se alinean,
mostrando una guía azul. El lienzo no sirve para pintar; es un entorno
desplazable con rejilla. Arrastra sobre una zona vacía con el botón izquierdo
o central para mover la vista, o usa la rueda para desplazarte en vertical
(`Ctrl+rueda` en horizontal).

El **Editor de mapas** del menú **MODO** pide un archivo `.mav` y muestra una
rejilla infinita de celdas. El clic izquierdo coloca el tile elegido en el panel
derecho y el derecho lo borra; en ambos casos puedes arrastrar para pintar o
borrar varias celdas. El botón central (o `Espacio` + clic izquierdo) mueve la
vista, la rueda la desplaza 48 píxeles (`Shift` en horizontal) y `Ctrl+rueda`
cambia el tamaño de celda entre 8 y 128 píxeles manteniendo bajo el cursor el
mismo punto del mapa. Los ejes, las etiquetas de coordenadas y el panel de estado
indican la celda actual. Los tiles conservan su proporción dentro de la celda.
**ARCHIVO > Guardar mapa .mav** guarda el mapa en formato Lua para Love2D; al
volver a entrar se conserva el mapa que ya está en memoria. Los mapas se guardan
por defecto en la carpeta `maps` junto al ejecutable (la misma que usa el
selector de items del gestor de lógica). Las imágenes se buscan primero en la
carpeta `tiles_dir` junto al `.mav`, después junto al propio `.mav` y por
último en la carpeta `tiles` del ejecutable.

El menú **COMPILE** de la barra de menús, entre **COLORES** e **INFO**, ofrece
**Compilar para BIOS** y **Compilar para Windows**. **Compilar para Windows**
toma el proyecto activo — la lógica del gestor, los mapas que usan sus
elementos y los tiles de esos mapas — y produce un `game.exe` autónomo de 32
bits: el proyecto entero viaja incrustado dentro del propio ejecutable, así
el juego funciona en cualquier carpeta sin necesidad de `maps/` ni `tiles/`.
La plantilla del motor viaja dentro del propio editor, por lo que compilar
no requiere tener MinGW instalado. **Compilar para BIOS** todavía no está
hecha y avisa de ello al elegirla.

## Atajos

| Atajo | Acción |
|---|---|
| `Ctrl+Z` | Deshacer |
| `Ctrl+Y` / `Ctrl+Shift+Z` | Rehacer |
| `Ctrl+T` | Nueva pestaña con el modo actual |
| `Ctrl+W` | Cerrar la pestaña activa |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Pestaña siguiente / anterior |
| `Ctrl+1` … `Ctrl+9` | Ir a una pestaña |
| `Ctrl+rueda` | Cambiar el zoom |
| `Esc` | Cancelar la acción en curso |
| `Enter` | Confirmar la acción en curso |
| `Supr` | Borrar la selección |

## Estructura

- `src/` — código fuente C y recursos de la interfaz. `src/compilar.c` es el
  menú COMPILE (compilar el proyecto para Windows).
- `motor/` — motor egine: núcleo en ensamblador 32 bits (`nucleo32.s`),
  interfaz común (`motor.h`), carga del proyecto (`activo.c`) y `juego.c`
  (`game.exe`).
- `loboswolfos/` — kernel propio (reducción de rocky) para arrancar el juego
  desde la BIOS: arranque en ensamblador (`boot/`), kernel Rust `no_std`
  (`kernel/`) y el constructor de la imagen (`xtask/`).
- `recursos/` — iconos e imágenes de las herramientas.
- `tiles/` — carpeta donde se guardan los mosaicos.
- `maps/` — mapas `.mav` (pantallas del juego).
- `build.bat` / `build.ps1` — scripts de compilación. El del editor compila
  primero el motor, porque su plantilla viaja incrustada como recurso.

## Licencia

Uso interno de Green Blue Studios.
