# LobosWolfos 2000

Editor de texturas ligero para **Windows 2000 y Windows XP** de 32 bits. Está
escrito en C con Win32/GDI puro y un búfer propio de 32 bits; GDI+ solo se usa
para cargar y guardar PNG.

## Características

- Lienzo con zoom (del 10 % al 4000 %; arranca al 500 %), capas y
  deshacer/rehacer.
- Pincel, lápiz, aerógrafo, cubo de relleno y selector de color.
- Figuras: línea, rectángulo, rectángulo redondeado, elipse, polígono y curva
  de Bézier.
- Selección rectangular y de forma libre, con arrastre y redimensionado.
- Herramienta de texto con varias fuentes y tamaños.
- Patrón de textura repetido, visor de mosaico y guardado de tiles en PNG.
- Pestañas de modo: Pintar, Tiles, Mapa y patrón comparten el mismo documento;
  cada pestaña conserva su vista, zoom y desplazamiento. Las pestañas viven en
  la barra de título, entre el título de la ventana y los controles de
  minimizar/maximizar/cerrar, como en Windows 2000 (fase 18).
- Gestor de lógica: paneles izquierdo, derecho e inferior (los laterales del
  mismo ancho), un botón de item arrastrable y un lienzo desplazable con
  rejilla. Los items son mapas `.mav` de la carpeta `maps` junto al ejecutable:
  se eligen con un clic y se muestran con miniatura y nombre. Los elementos se
  pueden mover y se acoplan entre sí por sus lados con efecto imán. El panel
  izquierdo lista los elementos del lienzo; hay una sola lógica y se guarda
  como archivo `.logic`.
- Apertura y exportación de PNG/BMP y mapas `.mav`, con un menú **EXPORTAR**
  propio para las dos exportaciones de imagen.
- Guardado del proyecto entero en un paquete `.wolfo` (lógica, mapas y tiles)
  y apertura del mismo paquete para restaurarlo, o solo de la lógica en un
  `.logic`.
- **Ajustes del proyecto** (menú de la barra): nombre, resolución y relación de
  aspecto, límite de cuadros, velocidad del jugador y color de fondo; viajan
  con el `.logic` y los aplican la vista previa y `game.exe`.
- Vista previa del juego con los botones **play/pausa/stop** de la barra:
  prueba el proyecto en una ventana sin compilar ni escribir ningún `.exe`.
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

Abre una ventana con la resolución del proyecto (640x480 por defecto), carga el
mapa del proyecto (el del `.logic` de la
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

Opción: `--logica ruta` (proyecto `.logic`), `--mapa ruta` (un `.mav`
directo), `--captura ruta`, `--fotogramas N`, `--tecla nombre`
(izquierda/derecha/arriba/abajo/saltar) y `--traza ruta` (una línea por
fotograma con el contador y la posición del lobo; la cabecera
`lobo: N scripts` confirma que los scripts lobo compilaron y uno que
falla sale como `lobo: error <id> <motivo>`). La resolución, el límite de
cuadros, la velocidad y el color de fondo salen de los ajustes del proyecto
guardados al final del `.logic`.

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
- `probar-imagen <ruta>` — lo mismo que `probar-juego` pero con una imagen
  ya montada (por ejemplo, la que genera el editor con **Compilar para
  BIOS**), sin reconstruirla.
- `plantilla` — escribe `loboswolfos\build\plantilla-kernel.bin`: los bytes
  de la imagen antes de la zona del juego, que el editor incrusta como
  recurso para **Compilar para BIOS**.
- `clean` — borra los destinos de compilación y la imagen.

`build` genera `loboswolfos\loboswolfos.img`, la imagen USB de 32 MiB con el
trazado final: MBR en LBA 0, segundo tramo en 1..4, kernel en 5.. y la zona
del juego en LBA 2048 con el **blob LGB1 del proyecto** (el mismo formato
que viaja pegado al `game.exe` compilado: `proyecto.logic`, `maps\*.mav` y
`tiles\*.png` de la raíz del proyecto). El ensamblador de arranque es el de
rocky (`bootusb.s`, `bootusb2.s`) sin cambios de trazado.

## El código lobo

Los botones FUNCIONES del gestor de lógica se programan en **lobo**, el
lenguaje propio de LobosWolfos 2000: texto plano, palabras en español y una
sentencia por línea. El mismo código corre en los dos destinos (`game.exe`
y el kernel loboswolfos), porque el proyecto entero viaja en el blob del
juego compilado:

```lobo
# paseo autonomo del lobo
vel = 3
si tecla_derecha
x = x + vel
fin
si paso < 90
x = x + 1
sino
paso = 0
fin
paso = paso + 1
```

Sentencias:

- `si <expr>` ... `sino` ... `fin` — condicional (el `sino` es opcional).
- `mientras <expr>` ... `fin` — bucle, con tope de pasos por fotograma (un
  bucle sin salida no cuelga el juego).
- `<nombre> = <expr>` — asignación.
- `<expr>` suelta (por ejemplo `cerrar()`) — se evalúa y se descarta.

Expresiones: números enteros, variables, paréntesis, aritmética
(`+ - * / %`), comparaciones (`== != < > <= >=`), lógica (`&`, `|`, `no`,
`!`), las teclas y las funciones del juego. El `#` empieza un comentario
hasta el final de la línea.

Variables del juego: `x` e `y` (posición del sprite), `vel` (velocidad) y
`fotograma` (contador de actualizaciones). Los demás nombres que se asignan
en el código son **variables locales del script**: se preescanean al
compilar, así que un error tipográfico da error de sintaxis en vez de leer
0 en silencio.

Teclas: `tecla_izquierda`, `tecla_derecha`, `tecla_arriba`,
`tecla_abajo`, `tecla_salto` y `tecla_cerrar`.

Funciones: `choca(x, y)` — 1 si la caja del sprite choca contra el mapa en
esa posición — y `cerrar()` — cierra el juego.

Límites (los mismos en los dos destinos): 32 funciones por proyecto, 2048
bytes de código por botón, 256 instrucciones compiladas, 16 variables
locales, 32 valores de pila y 200000 pasos por fotograma. La división entre
cero da 0 y la aritmética envuelve como unsigned.

**Cómo corre**: cuando algún elemento del lienzo usa una función, el motor
entra en **modo scripts**: el código lobo manda cada fotograma (un script
por elemento, con sus locales persistentes entre fotogramas) y el
movimiento por defecto queda fuera; al final del fotograma solo vuelven
`x` e `y` (vel y fotograma son de entrada). Sin funciones usadas, el juego
corre con el movimiento por teclado de siempre.

## Uso

Ejecuta `LobosWolfos2000.exe`. Al arrancar crea junto al ejecutable las
carpetas del proyecto: `tiles` (mosaicos e imágenes), `maps` (mapas `.mav`),
`logic` (archivos `.logic`) y `titles` (pantallas de título del editor de
menús). Los diálogos de abrir y guardar proponen esas carpetas: los tiles y
las imágenes van a `tiles`, los mapas a `maps`, las lógicas a `logic` y los
PNG/BMP exportados desde el editor de menús a `titles`. Los tiles que guardes
se almacenan en la carpeta `tiles` junto al ejecutable. En **modo tiles** los
lienzos quedan totalmente
pegados: las imagenes se tocan y no hay ninguna separacion entre ellas. Solo se
dibuja el contorno exterior de la cuadricula (el tile activo en azul), asi que
la union entre dos tiles es invisible. Si el zoom agranda un tile mas alla del
area de trabajo, el marco de ese tile no se pinta: asi no aparecen lineas
azules cruzando el lienzo. Los botones `+` siguen en la celda
vecina libre de cada cara. No hay tiradores para redimensionar: el tamano del
tile es el de **Ajustes del proyecto** y se cambia desde ahi. Se puede pintar
arrastrando de un tile a otro: el trazo cruza el borde y continua en el tile
de destino sin cortarse. La herramienta de texto tambien cruza: la caja de
seleccion puede extenderse al tile vecino y el texto se estampa en cada tile
ocupado que cubre, con el desplazamiento de su celda; las celdas libres no
crean tiles nuevos. Al confirmar se guarda una sola instantanea, asi que
`Ctrl+Z` deshace el texto de todos los tiles en un paso. El zoom usa los mismos niveles que el modo pintar
(hasta 4000 %): al cambiarlo con `Ctrl+rueda` o con los botones de la barra
inferior se recalcula toda la cuadricula y el lienzo de la celda activa, de
modo que los tiles siguen pegados y se puede pintar en ellos sin saltos. Si la
cuadricula no cabe en el area de trabajo, se recorta y se mantiene centrada la
celda activa; las barras y los paneles laterales no se pintan encima.

Las pestañas representan modos de trabajo sobre el mismo documento. El botón
`+` (o `Ctrl+T`) duplica el modo actual en otra pestaña; después puedes cambiar
esa pestaña a Tiles, Mapa o patrón sin tocar el documento. `Nuevo` y `Abrir`
sustituyen el documento compartido, no crean pestañas nuevas.

El menú **MODO** incluye **Modo pintar**, que cierra el visor de patrón, sale del
modo tiles o del editor de mapas y vuelve al lienzo normal, y **Gestor de
lógica**, que abre un espacio de trabajo con los paneles izquierdo, derecho e
inferior (el derecho un poco más ancho), manteniendo las pestañas y la
barra de menús. Al elegir **Gestor de lógica** de nuevo se vuelve a Pintar.
También incluye **Editor de menus**: abre esos mismos paneles pero vacíos, con
un lienzo propio en el hueco central cuya resolución la dictan las propiedades
del proyecto (**AJUSTES DEL PROYECTO**), sin cambiar el zoom guardado del modo
pintar. El lienzo del editor de menús se guarda aparte del documento de pintar
(con su propio historial de deshacer/rehacer), de modo que entrar y salir no
altera el dibujo. Si cambias la resolución del proyecto o abres otro `.logic`
estando dentro, el lienzo se redimensiona al momento y se limpia su historial.
El botón **TEXTO** del panel izquierdo activa la misma herramienta de texto que
el modo pintar (arrastra una caja y escribe). Al elegir **Editor de menus** de
nuevo se vuelve a Pintar; la pestaña de esta vista se llama `Menus`.

El panel izquierdo muestra la lista de elementos del lienzo (`Elemento 1`,
`Elemento 2`…) y ocupa todo el panel. Los elementos unidos entre sí por el
efecto imán aparecen indentados bajo el elemento del grupo al que pertenecen;
la flecha del grupo permite plegar o desplegar sus hijos en la lista. Si hay
más elementos que filas visibles, la rueda sobre la lista los desplaza.

El menú **EXPORTAR**, entre **ARCHIVO** y **MODO**, reúne las dos salidas de
imagen del documento activo: **Exportar a PNG** y **Exportar a BMP**. Guardan
en la carpeta `tiles` junto al ejecutable (o en `titles` si se lanzan desde el
editor de menús), igual que **Abrir imagen**.

La opción **Guardar logica .logic** del menú **ARCHIVO** exporta la lógica a
un archivo de texto con su desplazamiento y cada elemento (posición, tamaño,
identificador, estado plegado, mapa asociado y función). La opción **Abrir
lógica .logic**, justo encima, carga uno de esos archivos de vuelta: la
lógica del archivo sustituye a la activa y la vista entra al gestor para
verla cargada.

Al final del `.logic` se escribe una línea opcional `PROYECTO` con los
ajustes del proyecto: nombre, resolución, límite de cuadros, FPS, velocidad
del jugador y color de fondo. Los lectores antiguos la ignoran y los nuevos
la aplican a la vista previa y al `game.exe`; un `.logic` sin esa línea usa
los valores por defecto (640x480, 60 FPS, velocidad 3 y fondo negro).

La opción **Guardar proyecto .wolfo** del menú **ARCHIVO** empaqueta el
proyecto entero en un solo archivo: la lógica con sus ajustes
(`proyecto.logic`), los mapas que usan sus elementos y los tiles de esos
mapas. Usa el mismo blob `LGB1` que viaja incrustado en el `game.exe`
compilado y termina con el pie `LGB1` y su longitud, así que un lector puede
localizar el blob desde el final y reconstruir el proyecto en otra carpeta.
El nombre propuesto es el del proyecto.

La opción **Abrir proyecto .wolfo**, justo encima, hace el recorrido inverso:
lee el pie, recorre las entradas del paquete y restaura la lógica (con sus
ajustes), escribe los mapas en la carpeta `maps`, los tiles en `tiles` y los
títulos en `titles`, y abre el primer mapa que usa la lógica. Si el paquete no
lleva el pie o una entrada está dañada muestra un aviso y no toca el proyecto
activo.

Dentro del gestor, el panel derecho contiene el botón del **item**. Un clic
(sin arrastrar) abre el selector de items: la lista de mapas `.mav` de la
carpeta `maps` junto al ejecutable, cada uno con su miniatura y su nombre.
Elige uno con un clic y el botón pasa a mostrarlo; arrástralo hasta el lienzo
para insertarlo como elemento con su miniatura y su nombre. Sin item elegido,
el arrastre inserta un elemento vacío. Si hay más mapas que filas, la rueda
desplaza la lista y `Escape` la cierra.

Debajo del botón del item está la sección **FUNCIONES**, con rectángulos
arrastrables para las funciones lógicas del juego. El primero es **inicio
de programa**: se arrastra al lienzo para colocar el arranque de un
programa (con su banda INICIO, su número y la palabra PROGRAMA); un clic
sin arrastrar recuerda cómo se usa. Puedes colocar varios inicios: cada
uno abre un programa nuevo y los rectángulos que quedan debajo, hasta el
siguiente inicio, son sus módulos. Los demás son botones de **código
lobo**: el botón **+ NUEVA FUNCION**
del final crea uno (con nombre automático) y abre su editor, donde se
escribe el código del juego (ver la sección «El código lobo» más arriba). Cada botón muestra
su nombre, un cuadro de estado (rojo = error, verde = listo, gris = vacío)
y un resumen; un clic abre y cierra su editor, y un clic derecho lo borra
(con confirmación) junto con los elementos del lienzo que lo usen. El
código se valida al escribir con el mismo parser del motor: los errores
(«linea N: motivo») se pintan en la franja bajo el editor. Arrastrado al
lienzo, el botón inserta un elemento que ejecuta su código cada fotograma.
La opción **Guardar logica .logic** también guarda la función de cada
elemento, y **Abrir lógica .logic** la recupera al cargar.

Los rectángulos del lienzo que ejecutan código se ordenan **de arriba a
abajo**: el primero por altura corre primero, como las líneas de un
programa. Los items de mapa van siempre delante para elegir el mapa del
proyecto. Cada tarjeta muestra su puesto con un badge `1..N`, la lista de
ELEMENTOS lo antepone como `#n` y la cabecera de FUNCIONES recuerda
`ORDEN: 1 ARRIBA`. Al guardar el `.logic`, ese orden queda escrito en el
archivo, así que `game.exe` y el kernel lo respetan sin cambios.

El **inicio de programa no es obligatorio**: los rectángulos de código
corren aunque no haya ninguno. Cada inicio colocado abre un programa
nuevo en el mismo lienzo y los rectángulos de debajo (hasta el siguiente
inicio) son sus módulos, que se ejecutan en orden de arriba a abajo. Con
varios inicios se construyen varios programas independientes en el mismo
lienzo: la tarjeta del inicio muestra su número, los módulos llevan el
badge `Pn.k` (programa y puesto) y la lista de ELEMENTOS los antepone
como `Pn.k`. Si no hay ningún inicio, los módulos sueltos conservan la
numeración de siempre (`#n`).

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

El menú **COMPILE** de la barra de menús, entre **AJUSTES DEL PROYECTO** e
**INFO**, ofrece **Compilar para BIOS** y **Compilar para Windows**. Ambos toman
el proyecto activo — la lógica del gestor, los mapas que usan sus elementos y
los tiles de esos mapas — y lo incrustan entero como archivos virtuales (blob
`LGB1`). El mapa es opcional: si el lienzo no tiene ningún elemento de mapa, el
proyecto se compila igual y el juego arranca con la ventana en el color de
fondo del proyecto (negro por defecto).

Entre **COLORES** y **COMPILE** está el botón **AJUSTES DEL PROYECTO**, que
abre las propiedades del proyecto en una ventana flotante (como CAPA y
COLORES no tiene desplegable). Se puede editar el **nombre del proyecto**,
elegir la **resolución** entre las relaciones de aspecto 4:3 y 16:9 (640x480,
800x600, 1024x768, 1280x720, 1600x900 y 1920x1080), activar o desactivar
**limitar cuadros** y elegir los **FPS** (30, 60, 75, 120, 144 o 240), ajustar
la **velocidad del jugador** (1 a 10) y escoger el **color de fondo**. La
ventana se arrastra por la barra de título, `Escape` cancela, `Enter` acepta y
los botones **RESTABLECER**, **CANCELAR** y **ACEPTAR** hacen lo que indican.
Los ajustes se aplican a la vista previa y al `game.exe` compilado; la imagen
BIOS mantiene siempre su pantalla de 640x480.

**Compilar para Windows** produce un `game.exe` autónomo de 32 bits: el
proyecto viaja incrustado dentro del propio ejecutable, así el juego
funciona en cualquier carpeta sin necesidad de `maps/` ni `tiles/`.
**Compilar para BIOS** produce la imagen USB de 32 MiB de loboswolfos
(`.img`): MBR + arranque + kernel + el proyecto en la zona de juego; escribida
en un pendrive con `flashear.bat`, el juego arranca desde la BIOS del PC sin
Windows. Las plantillas (el motor y el arranque del kernel) viajan dentro del
propio editor, por lo que compilar no requiere tener MinGW ni Rust
instalados.

## Probar el juego sin compilar

En el extremo derecho de la barra de menús hay tres botones: **play**,
**pausa** y **stop**. Sirven para jugar el proyecto activo en una ventana
aparte sin pasar por COMPILE ni escribir ningún `.exe`, cómodo para probar
cambios sin llenar el disco.

- **play** abre la vista previa con el mapa, los tiles y el código
  lobo del proyecto, con la resolución de los ajustes del proyecto (640x480
  por defecto). Si el proyecto no tiene mapa, la ventana se abre con el color
  de fondo elegido (negro por defecto) y los programas corren igual. Si está en
  pausa, la reanuda; si ya
  está jugando, reinicia el proyecto para recoger los últimos cambios.
- **pausa** alterna pausa y marcha. Hasta que se abre la vista previa, está
  opacado y no responde.
- **stop** cierra la vista previa (también `Escape` y la X de la ventana).
  Hasta que se abre la vista previa, está opacado y no responde.

La vista previa usa el mismo proyecto incrustado (blob `LGB1`) que
**Compilar para Windows**, pero lo monta en memoria: no crea archivos ni
necesita las plantillas del motor. Los controles son los mismos que en
`game.exe` (cursores o `WASD`, `Espacio` para saltar y `Escape` para salir) y
el editor sigue respondiendo mientras la ventana está abierta.

Para comparar la vista previa con el ejecutable compilado hay una captura
determinista sin ventana: `--captura-juego ruta --fotogramas N` guarda un BMP
que coincide píxel a píxel con `game.exe --captura` para el mismo proyecto.

Para flashear la imagen a un pendrive (desde una consola con permisos de
administrador):

```bat
flashear.bat            (o: flashear.bat <num disco> <ruta\imagen.img>)
```

El script nunca toca el disco 0, pide confirmación escrita y verifica
releyendo el pendrive tras la escritura.

El editor acepta además flags de línea de comandos para pruebas
automáticas sin tocar la ventana: `--captura ruta` (dibuja un fotograma
y guarda el BMP), `--gestor` (abre el gestor de lógica), `--abrir-logica
ruta` y `--guardar-logica ruta` (abrir y guardar el `.logic` sin
diálogo), `--abrir-wolfo ruta` y `--guardar-wolfo ruta` (restaurar el
paquete `.wolfo` con lógica, mapas y tiles, o generarlo, sin diálogo),
`--clics "x,y;x,y"` y `--arrastre "x0,y0;x1,y1"`
(gestos simulados; `--arrastre` admite varios gestos separados por `|` y un
prefijo de herramienta, como `"12:100,200;300,400|15:150,250;150,350"`),
`--copiar`, `--cortar` y `--pegar` (portapapeles tras el arrastre),
`--vista menus` (abre directamente el editor de
menús, después de cargar la lógica si se indica) y `--volcar ruta`
(escribe una línea con la vista, los documentos, los ajustes del proyecto,
la geometría del lienzo, los módulos del gestor, el mapa abierto y el
estado de los menús para comparar pruebas),
y, para la sección FUNCIONES (fase 8),
`--funcion-abrir <id>` (abre el editor de código de esa función),
`--codigo-teclas "texto"` (escribe en el editor abierto, con `\n`, `\t`,
`\b` y `\\` como escapes), `--texto-teclas "texto"` (escribe en la caja de
la herramienta de texto después de `--arrastre`, también con escapes),
`--clic-final x,y` (confirma el texto con un clic y descarta la caja vacía
que abre el clic), `--deshacer` (pulsa `Ctrl+Z` al final), `--sin-ventanas`
(oculta opciones, patrón, información y visor de tiles en las capturas),
`--maximizar` (captura con la ventana maximizada),
`--funcion-borrar x,y` (clic derecho simulado
que pide confirmación de borrado) y `--modulos-rueda x,y,delta[,ctrl]`
(la rueda del gestor, que desplaza la sección FUNCIONES). Para la vista
previa (fase 9), `--captura-juego ruta` con `--fotogramas N` guarda un BMP
determinista sin abrir ventana. Cada flag está documentada junto a su
procesado en `src\nucleo.c`.

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
  menú COMPILE (compilar el proyecto para Windows y para BIOS) y el paquete
  `.wolfo`,
  `src/juego_ventana.c` la vista previa de los botones play/pausa/stop y
  `src/ajustes.c` las propiedades del proyecto.
- `motor/` — motor egine: núcleo en ensamblador 32 bits (`nucleo32.s`),
  interfaz común (`motor.h`), carga del proyecto (`activo.c`) y `juego.c`
  (`game.exe`).
- `loboswolfos/` — kernel propio (reducción de rocky) para arrancar el juego
  desde la BIOS: arranque en ensamblador (`boot/`), kernel Rust `no_std`
  (`kernel/`) y el constructor de la imagen (`xtask/`).
- `flashear.bat` / `flashear-usb.ps1` — flashear la imagen compilada a un
  pendrive (porteo del `rocky-usb.ps1` de rocky).
- `recursos/` — iconos e imágenes de las herramientas.
- `tiles/` — carpeta donde se guardan los mosaicos e imágenes.
- `maps/` — mapas `.mav` (pantallas del juego).
- `logic/` — archivos `.logic` de la lógica del proyecto.
- `titles/` — pantallas de título exportadas desde el editor de menús.
- `build.bat` / `build.ps1` — scripts de compilación. El del editor compila
  primero el motor y el kernel, porque sus plantillas viajan incrustadas
  como recursos.

## Licencia

Uso interno de Green Blue Studios.
