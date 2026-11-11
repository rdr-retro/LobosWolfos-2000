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
(`loboswolfos/kernel/src/nucleo64.s`, SSE2) llegó con la fase 6.

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
- El flasheo del pendrive: rocky ya no tiene `flashusb/` (solo queda el
  `flashear.bat` que lo llamaba), así que la referencia es su script
  `rocky-usb.ps1`; este repo lleva el porteo como `flashear.bat` +
  `flashear-usb.ps1` (fase 7).

loboswolfos es la **reducción** de rocky al juego: sin shell completo, sin
escritorio Kira; solo lo necesario para levantar framebuffer + entrada +
disco, cargar el proyecto y ejecutar el motor con él. El kernel rocky del
escritorio **no se toca**: se trabaja sobre una copia.

## 2. Las dos compilaciones (menú COMPILE)

- **Compilar para Windows**: el editor toma el proyecto activo y el núcleo
  de 32 bits del motor y produce `game.exe` (Win32, 32 bits). Los mapas y
  tiles del proyecto se incrustan en el ejecutable.
- **Compilar para BIOS**: el editor toma el proyecto activo y la plantilla
  de arranque del kernel loboswolfos (incrustada como recurso) y produce
  una **imagen de disco USB** de 32 MiB con MBR + loader + kernel + juego,
  con un clic del menú COMPILE. Flasheándola a un pendrive con
  `flashear.bat`, el juego arranca directamente desde la BIOS del PC.

## 3. Fases

| Fase | Entregable | Estado |
|---|---|---|
| 0 | Editor con gestor de lógica, sección FUNCIONES y menú COMPILE | **Hecho** |
| 1 | **egine arrastrable al lienzo**: el botón FUNCIONES se arrastra como elemento del motor y el `.logic` lo guarda | **Hecho (10-11-2026)** |
| 2 | **Cargar `.logic` de vuelta al gestor**: la fila **Abrir lógica .logic** del menú ARCHIVO sustituye la lógica activa | **Hecho (10-11-2026)** |
| 3 | Motor mínimo: núcleo ASM 32 bits (blit + tiles) y `game.exe` de prueba que abre una ventana, dibuja un mapa del proyecto y mueve un sprite con el teclado | **Hecho (10-11-2026)** |
| 4 | **Compilar para Windows** desde el menú COMPILE: genera `game.exe` con el proyecto actual | **Hecho (10-11-2026)** |
| 5 | Kernel loboswolfos: copia reducida de rocky que arranca por USB con framebuffer y entrada | **Hecho (10-11-2026)** |
| 6 | Núcleo ASM x86_64 + el mismo juego corriendo sobre loboswolfos | **Hecho (11-11-2026)** |
| 7 | **Compilar para BIOS** desde el menú COMPILE: genera la imagen USB flasheable (y flashearla con la herramienta de rocky como guía) | **Hecho (11-11-2026)** |
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
- **11-11-2026 (fase 6)** — Núcleo x86_64 en `loboswolfos/kernel/src/
  nucleo64.s`: SysV AMD64, **SSE2 en vez de MMX**, símbolos sin guion bajo
  y AT&T (incluido con `options(att_syntax)`, que `global_asm!` no usa por
  defecto). Misma interfaz y mismo recorte/sello/AABB que `nucleo32.s`;
  `MotorImagen` pasa a 16 bytes (puntero de 8).
- **11-11-2026 (fase 6)** — `start.s` activa `CR4.OSFXSR` y `OSXMMEXCPT`
  antes de entrar en `rust_main`: sin el bit OSFXSR, toda instruccion
  SSE2 que toque un XMM lanza #UD (detectado en QEMU: fallo grave nada mas
  entrar en el nucleo). El trampolin del BIOS solo borra `CR4.PAE`, asi
  que los bits sobreviven a las llamadas al firmware.
- **11-11-2026 (fase 6)** — El ejecutable del juego dentro del kernel es
  un **blob LGB1 en la zona de juego (LBA 2048)**, el mismo formato que
  viaja pegado al `game.exe` de la fase 4, con dos ajustes para el
  arranque por BIOS: los nombres de entrada llevan barra normal
  (`maps/mapa.mav`) y el pie queda al final del ultimo sector (relleno con
  ceros hasta multiplo de 512, con el largo total en el pie, que es lo
  que el kernel compara sector a sector). Sin FS y sin heap: el xtask lo
  monta con `proyecto.logic` (si existe) + `maps\*.mav` + `tiles\*.png`
  del repo.
- **11-11-2026 (fase 6)** — Memoria del juego (la RAM que hay sobre la
  pila del kernel, identidad-mapeada por el arranque): copia del blob en
  4 MiB (1 MiB de tope), rascaderos del decodificador PNG en 5 MiB (IDAT,
  crudo y paleta) y pixeles de los tiles en 6 MiB (2 MiB a repartir); las
  celdas, imagenes y nombres viven en `.bss`. La DAP y el bufer de rebote
  del INT 13h van en 0x9000/0x9100 (gfx.rs usa 0x8000/0x8300).
- **11-11-2026 (fase 6)** — `inflate` y `png` son puertos sin `alloc` de
  las librerias de rocky: la salida y los rascaderos los presta el
  llamante (slices), y el desfiltrado PNG se hace en el sitio sobre el
  bufer crudo (cada fila conserva su byte de filtro delante).
- **11-11-2026 (fase 6)** — `juego.rs` es el puerto de `motor/juego.c`:
  framebuffer de 640x480 en `.bss`, spawn en espiral con margen de 8 px,
  camara centrada, colision por eje, 60 FPS con acumulador entero sobre
  el PIT de 100 Hz y volcado centrado al modo VBE. La entrada sondea las
  flechas E0 (con WASD, espacio y Escape) como bits `MOTOR_ENT_*`; los
  toques soltados entre sondeos se recuerdan (`PULSOS`) para que cuenten
  un fotograma, y las letras `a/d/w/s`/espacio/Esc por la serie mueven
  igual (pruebas sin teclado).
- **11-11-2026 (fase 6)** — Comando `probar-juego` del xtask: QEMU sin
  ventana con la serie a `build/serial.log` y el monitor por la entrada
  estandar; mueve el sprite con `sendkey right 300` (y las demas flechas;
  QEMU 11 no acepta `spk-*`, y el tercer parametro es el tiempo de
  retencion en ms) y comprueba en el registro que el mapa cargo, que
  decodifico los tiles, que el juego arranco y que el rastro
  `egine: sprite en (x, y)` cambia.
- **11-11-2026 (fase 7)** — La plantilla de arranque viaja incrustada en
  el editor como recurso RCDATA (`IDR_KERNEL`), igual que
  `motor/game.exe`: el xtask la escribe con el comando `plantilla` en
  `loboswolfos/build/plantilla-kernel.bin` (los bytes de la imagen antes
  de la zona del juego: MBR con su tabla, tramo 2 con el RKYCFG
  parcheado y kernel), y `build.bat`/`build.ps1` la generan antes de los
  recursos porque windres la lee. Compilar es copiar la plantilla,
  ceros hasta el LBA 2048, el blob con su pie y ceros hasta los 32 MiB.
- **11-11-2026 (fase 7)** — La reunion del proyecto (`compilar_reunir`)
  se comparte entre los dos destinos: mismo blob LGB1 con
  `proyecto.logic`, los `.mav` referenciados y sus tiles; solo cambia el
  separador de la carpeta de mapas (`\` para el motor de Windows, `/`
  para el kernel). El pie BIOS queda al final del ultimo sector con el
  largo total (relleno incluido), como el kernel lo compara.
- **11-11-2026 (fase 7)** — Flasheo portado de rocky: `flashear.bat` +
  `flashear-usb.ps1` en la raiz del repo (el `flashusb/` de rocky ya no
  existe; el porteo sigue su `rocky-usb.ps1`). Se eleva solo, nunca toca
  el disco 0, pide BORRAR en mayusculas, desmonta todos los volumenes
  del disco, escribe en crudo con reintentos y verifica releyendo.
- **11-11-2026 (fase 7)** — Flags de prueba: `--compilar-bios <ruta>`
  compila la imagen sin dialogo (como `--compilar-windows`) y el xtask
  tiene `probar-imagen <ruta>` para arrancar en QEMU una imagen ya
  montada (la del editor) sin reconstruirla.

## 5. Pendiente de decidir

- Sistema de archivos de la imagen USB para el juego (Estrato de rocky,
  FAT32 u otro). Hoy no hay ninguno: la zona del juego lleva el blob LGB1
  crudo apuntado por el bloque RKYCFG, que de momento basta.
- Lenguaje del código de los botones de la fase 8 (texto plano, algo tipo
  `.rka` de rocky, u otro propio). La restricción que fija la fase 6:
  ese código tiene que correr en **los dos destinos** (el `game.exe` de
  32 bits con el motor en C y el kernel loboswolfos `no_std`), asi que o
  se interpreta en ambos nucleos o se compila a cada uno; el proyecto viaja
  en el mismo blob LGB1 en los dos casos.

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
