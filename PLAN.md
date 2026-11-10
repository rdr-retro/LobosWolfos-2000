# Plan: LobosWolfos 2000, entorno de videojuegos 2D

LobosWolfos 2000 no es solo un editor de texturas: es el **entorno de
desarrollo de videojuegos 2D con motor propio**. Se monta el juego en el
editor y se compila a dos destinos: **`game.exe`** para Windows, o una
**imagen USB flasheable** que arranca desde la BIOS con el kernel propio
**loboswolfos**, sin Windows.

Este documento organiza la visión, las piezas, las fases y las decisiones.
Se actualiza con cada entrega.

## 1. Las cuatro piezas

| Pieza | Qué es | Dónde vive |
|---|---|---|
| **Editor** | El entorno de trabajo (este programa): pintar, tiles, mapas y el gestor de lógica donde se compone el juego. | Repositorio actual. C, Win32/GDI, 32 bits, Windows 2000/XP. |
| **Proyecto del juego** | Lo que monta el usuario: lógica (elementos del lienzo), mapas `.mav`, tiles e items. | Archivo `.logic` + carpetas `maps/` y `tiles/` junto al editor. |
| **Motor egine** | El corazón del juego: bucle, render, entrada y colisiones. Sus partes esenciales escritas en **ensamblador**. | Carpeta `motor/` (nueva): núcleo x86 de 32 bits, núcleo x86_64 e interfaz común. |
| **Kernel loboswolfos** | El sistema operativo propio que arranca el juego desde la BIOS del PC. | Copia adaptada de `rocky` (`C:\Users\rdr-ryzen\Desktop\kernel`), reducida a lo que el juego necesita. |

### 1.1 Editor (ya hecho)

- Pestañas de modo: Pintar, Tiles, Mapa, Logica... todas sobre un único
  documento compartido.
- Editor de mapas `.mav` (rejilla infinita) y modo tiles.
- **Gestor de lógica**: lienzo desplazable donde se compone el juego.
  - Los **items** (mapas `.mav` de `maps/`) son las pantallas o niveles del
    juego: se eligen en el botón del item y se arrastran al lienzo.
  - La sección **FUNCIONES** del panel derecho tiene los módulos
    arrastrables del juego. El primero es **egine**, el motor.
  - En el futuro habrá más botones arrastrables con **partes de código
    editable dentro del propio botón**: al escribir código en el botón, el
    botón cambia (ver fase 8).
- Menú **COMPILE** (entre COLORES e INFO): las dos salidas de compilación.
- Efecto imán entre elementos, lista de elementos plegable, guardar
  lógica `.logic`.

### 1.2 Proyecto del juego

- `.logic`: texto plano con el desplazamiento de la vista y cada elemento
  del lienzo (posición, tamaño, id, plegado, mapa asociado y función).
- `maps/*.mav`: pantallas/niveles (con sus tiles referenciados).
- El juego final = proyecto + motor + código de los módulos FUNCIONES.

### 1.3 Motor egine (ensamblador)

Interfaz común (cabecera C) y **dos variantes del núcleo**, una por destino:

| Variante | Destino | Ensamblador |
|---|---|---|
| x86, 32 bits | `game.exe` en Windows 2000/XP | MinGW i686, `.s` GNU (o inline asm), MMX/SSE |
| x86_64 | kernel loboswolfos | `.s` GNU estilo rocky, SSE2 |

La variante de 32 bits ya está implementada (`motor/nucleo32.s`): limpiar
con MMX, blit opaco con recorte, blit con máscara alpha, render del mapa
con el sello del tile igual que el editor y colisiones AABB. La x86_64
llega con la fase 6.

**Partes esenciales escritas a mano en ensamblador** (lo que debe correr
rápido, el resto va encima en C/Rust):

1. **Blit de sprites y tiles**: copia de rectángulos de píxeles al
   framebuffer, con recorte por bordes (MMX/SSE).
2. **Render del mapa de tiles**: bucle de las celdas visibles de un `.mav`
   y volcado de cada tile.
3. **Colisiones AABB**: rectángulo contra rectángulo y rectángulo contra
   rejilla de tiles.
4. **Bucle principal sincronizado**: paso de tiempo fijo por fotograma.
5. **Entrada**: lectura del estado de teclado y ratón empaquetado en bits.

Sobre ese núcleo, la capa de juego (qué hace cada pantalla, cómo pasa de
nivel) se define con los elementos del gestor y, en el futuro, el código de
los botones FUNCIONES.

### 1.4 Kernel loboswolfos

Base: **rocky** (`C:\Users\rdr-ryzen\Desktop\kernel`), un sistema x86_64 en
Rust + ensamblador que ya arranca por BIOS, tiene framebuffer, entrada
PS/2, disco y ejecutables propios. De rocky se reutiliza:

- `boot/` (`bootusb.s`, `bootusb2.s`): arranque BIOS por USB, dos tramos,
  carga del kernel y salto a modo largo (ya copiado a `loboswolfos/boot/`).
- `kernel/src/*.s` (`bios.s`, `interrupts.s`, `start.s`): el estilo de
  ensamblador del sistema.
- Drivers de framebuffer/VGA/VBE, PS/2 (teclado y ratón) y disco ATA.
- `flashusb/` + `flashear.bat`: la forma de flashear el pendrive desde
  Windows con la imagen.

loboswolfos es la **reducción** de rocky al juego: sin shell completo, sin
escritorio Kira; solo lo necesario para levantar framebuffer + entrada +
disco, cargar el proyecto y ejecutar el motor con él. El kernel rocky del
escritorio **no se toca**: se trabaja sobre una copia.

## 2. Las dos compilaciones (menú COMPILE)

- **Compilar para Windows**: el editor toma el proyecto activo y el núcleo
  de 32 bits del motor y produce `game.exe` (Win32, 32 bits). Los mapas y
  tiles del proyecto se incrustan en el ejecutable.
- **Compilar para BIOS**: el editor toma el proyecto, el núcleo x86_64 del
  motor y el kernel loboswolfos, y produce una **imagen de disco USB** con
  MBR + loader + kernel + juego. Flasheándola a un pendrive, el juego
  arranca directamente desde la BIOS del PC.

## 3. Fases

| Fase | Entregable | Estado |
|---|---|---|
| 0 | Editor con gestor de lógica, sección FUNCIONES y menú COMPILE | **Hecho** |
| 1 | **egine arrastrable al lienzo**: el botón FUNCIONES se arrastra como elemento del motor y el `.logic` lo guarda | **Hecho (10-11-2026)** |
| 2 | **Cargar `.logic` de vuelta al gestor**: la fila **Abrir lógica .logic** del menú ARCHIVO sustituye la lógica activa | **Hecho (10-11-2026)** |
| 3 | Motor mínimo: núcleo ASM 32 bits (blit + tiles) y `game.exe` de prueba que abre una ventana, dibuja un mapa del proyecto y mueve un sprite con el teclado | **Hecho (10-11-2026)** |
| 4 | **Compilar para Windows** desde el menú COMPILE: genera `game.exe` con el proyecto actual | **Hecho (10-11-2026)** |
| 5 | Kernel loboswolfos: copia reducida de rocky que arranca por USB con framebuffer y entrada | **Hecho (10-11-2026)** |
| 6 | Núcleo ASM x86_64 + el mismo juego corriendo sobre loboswolfos | Pendiente |
| 7 | **Compilar para BIOS** desde el menú COMPILE: genera la imagen USB flasheable (y flashearla con la herramienta de rocky como guía) | Pendiente |
| 8 | Más botones FUNCIONES arrastrables con **código editable dentro del botón**: al escribir el código, el botón cambia su aspecto; el código pasa a formar parte del juego compilado | Pendiente |

Orden de trabajo por defecto: 1 → 2 → 3 → 4 (primero Windows, que es
inmediato), y en paralelo cuando toque 5 → 6 → 7. La fase 8 llega cuando
el motor ya compila.

## 4. Decisiones tomadas

- **10-11-2026** — El motor tiene dos variantes de núcleo en ensamblador
  (32 bits para `game.exe`, x86_64 para el kernel) con la misma interfaz
  común definida en C.
- **10-11-2026** — El plan vive en el repositorio del editor (`PLAN.md`) y
  se versiona con el código.
- **10-11-2026** — egine se arrastra al lienzo como elemento del motor
  (fase 1); un clic sin arrastrar explica cómo usarlo.
- **10-11-2026** — El kernel loboswolfos se basa en rocky, en copia aparte; el original del
  escritorio queda intocado.
- **10-11-2026** — Ensamblador del núcleo en sintaxis AT&T (gnu as), convención
  cdecl y símbolos con guion bajo (`_motor_blit`) en PE de 32 bits. Bug
  corregido en la fase 3: las bases de punteros de blit y mapa se calculaban
  en píxeles sin escalar a bytes (faltaba el `*4`); detectado con gdb (el
  primer `movq` escribía en `fb+1950` bytes en vez de `fb+7800`) y corregido
  con `leal (base, offset, 4)`.
- **10-11-2026** — `motor/prueba.c` queda versionado como banco de pruebas
  de consola del núcleo; se compila aparte (ver su cabecera) y se lanza desde
  la raíz del repo para que resuelva `maps/` y `tiles/`.
- **10-11-2026** — `game.exe` se lanza desde la raíz del repo (cwd con `maps/`
  y `tiles/`), no desde `motor/`. El punto de aparición del sprite se busca
  en espiral con margen de 8 px (el mapa puede tener celda sólida en el
  origen) y la cámara se centra en el sprite.
- **10-11-2026** — Fila nueva **Abrir lógica .logic** en el menú ARCHIVO
  (cuarta sin editor de mapas, sexta con él; "Guardar logica" pasa a la
  última). Al abrir, la lógica del archivo sustituye la activa y la vista
  entra al gestor si no estaba ya en él.
- **10-11-2026** — El cargador acepta las líneas antiguas sin campo función
  como items de mapa, sanea los tamaños degenerados (vuelven a 96x64) y
  reasigna los ids menores o iguales que cero; el siguiente id queda por
  encima del mayor cargado.
- **10-11-2026** — `arg_valor` corta el valor sin comillas en el siguiente
  `--flag` en vez de devolver todo el resto de la línea, y copia el
  resultado a un buffer estático rotatorio (16 x MAX_PATH) en lugar de
  cortar la propia línea de comandos.
- **10-11-2026** — El proyecto compilado viaja **pegado al final del propio
  exe**: `[exe del motor][entradas del blob]["LGB1" u32 tam_blob u32]`. El
  motor lee el pie al arrancar (`activo_blob_cargar`), registra las entradas
  como archivos virtuales y la carga (`.logic`, `.mav`, PNG) los consulta
  antes que el disco, así el `game.exe` resultante es autónomo: funciona en
  cualquier carpeta sin `maps/` ni `tiles/`.
- **10-11-2026** — La plantilla `motor/game.exe` viaja incrustada en el
  editor como recurso RCDATA (`IDR_MOTOR`); `build.bat` compila el motor
  antes de los recursos, porque windres la lee. Compilar es copiar la
  plantilla y pegarle el blob del proyecto (sin recompilar nada).
- **10-11-2026** — El blob registra `proyecto.logic` (la lógica activa
  serializada en memoria), cada `maps\*.mav` referenciado por un elemento y
  los PNG que usan esos mapas, resueltos con el mismo orden de búsqueda del
  motor. Los PNG virtuales se decodifican desde memoria con
  `GdipCreateBitmapFromStream` sobre un IStream (`-lole32` en el enlace).
- El formato `.logic` sigue en texto plano; los elementos de función
  añaden su campo propio al final de la línea.
- **10-11-2026 (fase 5)** — El kernel loboswolfos vive **dentro del repo del
  editor**, en `loboswolfos/` (el mismo patrón que `motor/`), para que la
  fase 7 («Compilar para BIOS») pueda invocarlo por ruta relativa. Del build
  solo se versiona la fuente: `target/`, `loboswolfos/build/` y `*.img`
  quedan ignorados.
- **10-11-2026 (fase 5)** — Copias de rocky: del **rocky actual**, el
  arranque completo (`bootusb.s`, `bootusb2.s`, sus linkers, `kernel.ld`,
  `start.s`) y el par `bios.rs`/`bios.s`/`pit.rs` endurecido del reloj; de la
  snapshot `20261015-entrada-ps2-ok`, los drivers `cpu`, `serial`, `sync`,
  `vga`, `gfx`, `interrupts.s`, `idt`, `pic`, `cmos`, `ps2`, `input` y
  `mouse`. El original de rocky no se toca.
- **10-11-2026 (fase 5)** — Layout de la imagen USB, idéntico trazado que
  rocky: LBA 0 = MBR, 1..4 = tramo 2 (el MBR lee 4 sectores a 0x8000),
  5.. = kernel (copiado a 0x100000) y **2048.. = zona del juego** (la
  usará la fase 6). Imagen de 32 MiB con una sola particion tipo 0x83
  que cubre la zona del juego.
- **10-11-2026 (fase 5)** — El bloque `RKYCFG` se reutiliza tal cual
  (u16 kernel_sectores + u32 juego_lba + u32 juego_sectores detras de la
  marca): los campos que en rocky apuntaban al volumen Estrato apuntan
  ahora a la zona del juego, y el tramo 2 los deja heredados en el boot
  info de 0x7E00. **Sin FS de momento**: la zona del juego es una particion
  cruda.
- **10-11-2026 (fase 5)** — El kernel va **sin `alloc`**: `Canvas::new`
  dibuja directo sobre el LFB, el informe PS/2 imprime con `match` directo
  (sin `format!`), la consola no tiene terminal virtual y
  `bios::set_text_mode` no restaura pantalla (no hay escritorio que
  restaurar).
- **10-11-2026 (fase 5)** — Las flags de compilacion del kernel y el
  arranque viajan por `CARGO_ENCODED_RUSTFLAGS` (separadas por `\x1f`): la
  ruta de este repo lleva espacios («GreenBlue kit») y con `RUSTFLAGS`
  normal cargo las parte por espacios y rompe el `-Clink-arg` del linker.
- **10-11-2026 (fase 5)** — Constructor propio (`loboswolfos/xtask`, estilo
  rocky) con comandos `build | run | run-serial | probar [segundos] |
  clean`. `probar` es la verificacion del kernel: QEMU sin ventana con la
  serie a `loboswolfos/build/serial.log`. Verificada la fase 5 en QEMU:
  arranque completo, VBE 1024x768 a 32 bpp, reloj a 100 Hz y entrada en
  vivo (eco de teclado y movimiento + botones del raton inyectados por el
  monitor).

## 5. Pendiente de decidir

- Formato del ejecutable del juego dentro del kernel (binario propio o
  adaptar el `.rok` de rocky). Se decide en la fase 6, junto con el contenido
  de la zona del juego en LBA 2048.
- Sistema de archivos de la imagen USB para el juego (Estrato de rocky,
  FAT32 u otro). Hoy no hay ninguno: la zona del juego es una particion cruda
  apuntada por el bloque RKYCFG.
- Lenguaje del código de los botones de la fase 8 (texto plano, algo tipo
  `.rka` de rocky, u otro propio).

## 6. Reglas de trabajo

- Cada fase se entrega como siempre: cambios en `src/` del editor,
  compilación con `build.bat` (MinGW i686), verificación por píxeles con
  las flags de prueba (`--gestor`, `--captura`, `--clics`, `--arrastre`...),
  commit en `main`, push y release `v1.0-beta` actualizada (exe + SHA-256 +
  nota).
- Código y comentarios en español sin acentos; las cadenas de la interfaz
  sí llevan acentos. Compatible con Windows 2000/XP, 32 bits, Win32/GDI.
- Las partes nuevas que no afecten al editor (motor, kernel) siguen las
  convenciones de su carpeta.
