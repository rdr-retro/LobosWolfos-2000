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
- Barra superior compacta para pantallas CRT (cambio previo a la fase 31):
  la franja de pestañas mide 26 px y la de menús 26 px, 52 px en total en vez
  de 70 px; las etiquetas usan Segoe UI a 12 px en vez de 15 px. Los botones
  de ventana, los controles play/pausa/stop y los iconos de pestaña se han
  reducido para conservar el centrado.
- Gestor de lógica: paneles izquierdo, derecho e inferior (los laterales del
  mismo ancho), un botón de item arrastrable y un lienzo desplazable con
  rejilla. Los items son mapas `.mav` de la carpeta `maps` junto al ejecutable:
  se eligen con un clic y se muestran con miniatura y nombre. Los elementos se
  pueden mover y se acoplan entre sí por sus lados con efecto imán. El panel
  izquierdo lista los elementos del lienzo; hay una sola lógica y se guarda
  como archivo `.logic`.
- Proyectos `LOGIC3` (fases 24 y 25): el editor abre y guarda el formato nuevo
  con escenas, objetos, eventos, bloques, variables, listas y trajes, detecta
  la cabecera al abrir un `.logic`, convierte un LOGIC2 existente y exporta de
  emergencia a LOGIC2. El motor ejecuta los scripts `codigo.lobo` de los
  eventos de fotograma con una VM resumible (fase 25).
- Modelo de programa (fase 46): los proyectos nuevos guardan `sistema.opciones`
  con `programa=1`, `jugador_implicito=0` y `auto_compilar=1`. Insertar un mapa
  crea solo el escenario; el jugador es una pieza explícita (Actor + Control +
  Acciones + Funciones) sobre bloques `sistema.*`, sin cambiar LOGIC3 ni su CRC.
  Los proyectos sin esa pieza son legacy y conservan el lobito clásico tanto en
  `game.exe` como en la vista previa.
- Constructor de programa (fase 47): el gestor conmuta entre **Programa** y
  **Compatibilidad** desde su barra inferior, sin menús nuevos ni cambios en
  los índices de menú. La paleta ofrece 31 piezas en siete grupos plegables
  (Escenario, Actores, Control, Eventos, Acciones, Funciones y Recursos); las
  tarjetas se arrastran con imán, se enlazan entre sí y se editan en un
  inspector con deshacer/rehacer. El estado se guarda en bloques
  `sistema.pieza` con `ui=1`, sin tocar el formato ni el CRC de LOGIC3. Una
  pieza con salida sin destino muestra una insignia roja. El botón **BLOQUES**
  abre la pestaña correspondiente del Estudio. Los tipos y valores serializados
  siguen siendo ASCII; las etiquetas visibles llevan acentos.
- Entrada al constructor (fase 48): **MODO > Gestor de lógica**, `--gestor`,
  `--abrir-logic3` y `--abrir-logica` abren directamente la capa **Programa**
  con la paleta visible desde el primer momento. La vista LOGIC2 clásica sigue
  disponible con **Compatibilidad** o `--compatibilidad`. Abrir el gestor no
  convierte un proyecto legacy: `programa=1` solo se escribe al guardar piezas
  en el lienzo. El título **INSPECTOR** del panel derecho se dibuja dentro del
  panel, debajo de la barra de menús, y queda visible desde el primer momento.
  El plan de trabajo se amplía poco a poco en `PLAN_GESTOR_LOGICA.md`, con
  entregas E0 a E12.
- Vista Estudio (fase 31): entrada **MODO > Vista Estudio**, pestana «Estudio»
  y columnas de 200, 740 y 260 px a 1200 px de ancho. El panel izquierdo crea
  borra y selecciona escenas y objetos, el centro muestra el escenario con
  rejilla y el derecho inspecciona propiedades, trajes, variables y eventos.
  La seleccion y el scroll se recuerdan por proyecto sin cambiar el formato
  LOGIC3 ni su CRC.
- Lienzo de bloques (fase 32): nueva pestaña **Bloques** en la vista Estudio,
  con paleta por categorías, formas hat/pila/C/reportero/booleano/cap,
  arrastre con acople por imán, ranuras, anidamiento, duplicado, borrado,
  deshacer/rehacer y navegación por teclado. Cada mutación vuelca el modelo a
  los bloques del proyecto LOGIC3 con el escritor existente, sin tocar el
  formato ni el CRC.
- Compilador bloques -> lobo (fase 33): los bloques generan codigo lobo 3
  validado por la VM, con visor de solo lectura y resaltado del bloque con
  error. `Convertir a codigo` sustituye el lienzo por un unico bloque
  `codigo.lobo`, de forma irreversible, y el motor ejecuta la misma traza que
  el lobo escrito a mano.
- Depurador del motor (fase 34): pausa, paso por bloque y por instruccion,
  breakpoints por bloque y evento, variables observadas con su valor en vivo,
  consola de errores en runtime, panel visual en la vista Play y replay
  determinista con comparador de trazas. Todo detras de flags nuevos: sin
  `--depurar` ni `--panel-debug`, el motor y las capturas siguen igual.
- Entidades y escenas en el motor (fase 26): cada objeto de la primera escena
  se instancia con posición, tamaño, capa, visibilidad, traje y variables
  propias; `mi.x`, `mi.y`, `mi.vida`, `mi.visible`, `mi.traje` y
  `mi.variable("nombre")` leen y escriben esa instancia, y `crear`, `clonar`,
  `borrar` y `mover` gestionan su ciclo de vida con ids que no se reutilizan.
  El render provisional dibuja cada entidad como un rectángulo de color con su
  id, para validar el sistema sin assets.
- Eventos, planificador y mensajes en el motor (fase 27): `motor/eventos.c`
  reparte los scripts de `inicio`, `fotograma`, `tecla` (pulsar, mantener y
  soltar), `recibir`, `tocar`, `cada`, `clonar` y `escena` en el orden del
  capítulo 5.2, con una cola de mensajes que no se pierde si el receptor está
  esperando. `enviar("mensaje")` y `enviar_a("objeto", "mensaje")` encolan
  mensajes para los eventos `recibir`; `detener_script`, `detener_todo` y
  `reiniciar_escena` controlan el despacho, y un presupuesto por fotograma
  evita que un bucle de mensajes cuelgue el juego.
- Variables y listas tipadas en `lobo` (fase 28): `entero`, `booleano`,
  `texto` y `lista` con ámbitos global (`$`), de entidad (`@`), local y de
  parámetro; funciones con `retornar`, `sino si`, `romper`, `continuar`,
  `repetir`, `para cada` y built-ins numéricos y de texto. El editor guarda
  los valores iniciales en LOGIC3 y el motor siembra la memoria de la VM con
  ellos.
- Apertura y exportación de PNG/BMP y mapas `.mav`, con un menú **EXPORTAR**
  propio para las dos exportaciones de imagen.
- Guardado del proyecto entero en un paquete `.wolfo` (lógica, mapas, tiles,
  trajes y, desde la fase 45, el LOGIC3 nativo) y apertura del mismo paquete
  para restaurarlo, o solo de la lógica en un `.logic`. El `game.exe` y la
  imagen BIOS siguen llevando el blob clásico para no alterar sus trazas.
- **Ajustes del proyecto** (menú de la barra): nombre, resolución y relación de
  aspecto, límite de cuadros, velocidad del jugador y color de fondo; viajan
  con el `.logic` y los aplican la vista previa y `game.exe`.
- Vista previa del juego con los botones **play/pausa/stop** de la barra:
  prueba el proyecto en una ventana sin compilar ni escribir ningún `.exe`.
- Menú **AYUDA** con un **juego de ejemplo** listo para jugar (crea los tiles,
  el mapa, la lógica y los ajustes) y una **guía paso a paso** de seis
  pantallas para empezar sin experiencia previa.
- Interfaz flotante de opciones, panel de capas y menús contextuales.
- Suite de pruebas en `pruebas/`: conformidad del lenguaje lobo (trazas y
  hashes de BMP), proyecto LOGIC3, VM resumible, paridad BIOS con QEMU,
  entidades y escenas, eventos y planificador, vista Estudio, lienzo de
  bloques, compilador, depurador, exportacion y regresion visual del editor,
  con `build.bat prueba` (24 suites).
- Manuales de la version 2.0: `MANUAL_BLOQUES.md` y
  `REFERENCIA_LENGUAJE.md`.
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

## Pruebas

`build.bat prueba` compila el editor y pasa las 24 suites:

```bat
build.bat prueba              todas las suites, en orden
build.bat prueba conformidad  solo el lenguaje lobo
build.bat prueba logic3       solo el proyecto LOGIC3
build.bat prueba vm           solo la VM y lobo 3
build.bat prueba bios         solo la paridad BIOS con QEMU
build.bat prueba entidades    solo entidades, escenas y ciclo de vida
build.bat prueba eventos      solo eventos, planificador y mensajes
build.bat prueba colisiones   solo colisiones, flags y fisica
build.bat prueba render       solo trajes, animacion, capas y camara
build.bat prueba estudio      solo la vista Estudio
build.bat prueba bloques      solo el lienzo de bloques
build.bat prueba compilador   solo el compilador bloques -> lobo
build.bat prueba depurador    solo el depurador y el replay
build.bat prueba datos        solo variables, listas, trajes y sonidos
build.bat prueba productividad solo atajos, paleta y productividad
build.bat prueba entrada      solo entrada extendida
build.bat prueba hud          solo HUD, texto, UI y dialogos
build.bat prueba audio        solo sonido y musica simple
build.bat prueba save         solo guardado, records y persistencia
build.bat prueba escenas      solo escenas multiples, camara y efectos
build.bat prueba perfil       solo perfilador, presupuestos y estres
build.bat prueba plantillas   solo plantillas, ejemplos y tutorial
build.bat prueba exportacion  solo paquetes, game.exe autonomo y BIOS/QEMU
build.bat prueba programa     solo el modelo y el lienzo de programa
build.bat prueba regresion    solo la interfaz del editor
build.bat prueba actualizar   reescribe los patrones (revisar el diff)
```

`pruebas/README.md` describe los casos, el formato de las trazas y cómo añadir
uno nuevo. Los patrones son texto (trazas, volcados y hashes SHA-256): no se
guardan imágenes en el repositorio. El caso `ui_programa` usa tres flags de
prueba propios: `--programa` entra en la capa nueva, `--arrastre-programa`
simula los gestos de paleta y enlace, y `--volcar-programa` describe las
piezas sin alterar los volcados antiguos.

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

Opción: `--logica ruta` (proyecto `.logic`), `--logic3 ruta` (proyecto
LOGIC3), `--mapa ruta` (un `.mav` directo), `--captura ruta`,
`--fotogramas N`, `--tecla nombre` (izquierda/derecha/arriba/abajo/saltar)
y `--traza ruta` (una línea por fotograma con el contador y la posición del
lobo; la cabecera `lobo: N scripts` confirma que los scripts lobo compilaron
y uno que falla sale como `lobo: error <id> <motivo>`). La resolución, el
límite de cuadros, la velocidad y el color de fondo salen de los ajustes del
proyecto guardados al final del `.logic` o de la cabecera del LOGIC3.

Con `--logic3` el motor compila cada bloque `codigo.lobo` de los eventos
`fotograma` como un script independiente y los ejecuta en la VM resumible:
un `esperar` solo detiene su script. La traza añade la cabecera
`vm: N scripts`, una línea `vm: script i bloque b objeto o evento e` y, por
fotograma, `vm i:estado:pc:espera`. `--vm-test ruta.lobo` ejecuta un
programa suelto en la VM sin ventana ni GDI+ y vuelca por fotograma
`vm <f> estado <e> pc <p> np <n> espera <w> x <x> y <y> msg <n> clones <n>
err <n>`; lo usa la suite `pruebas\vm.ps1`.

`--entidades` añade el detalle de las entidades a la traza y al volcado:
cabecera `entidades: N iniciales avisos A`, una línea `ent <id> <x> <y>` por
entidad y fotograma con `--traza`, y un resumen final
`entidades final: N avisos A creadas C`. Sin esta opción las trazas de las
fases anteriores no cambian. `--volcar ruta` escribe el estado final sin
ventana (`fotogramas`, `lobo`, `entidades N avisos A creadas C` y, con
`--entidades`, una línea `entidad ...` por cada una con sus variables).

El planificador de eventos (fase 27) se activa con `--logic3` y añade
`--evento N` (duración en fotogramas de la pulsación de `--tecla`; 0 la
mantiene) y `--mensaje "texto"` (un mensaje externo para los eventos
`recibir`). `--eventos` escribe el detalle en la traza: una línea
`evento <fotograma> <tipo> obj <id> ent <id> script <n> ...` por script
ejecutado, una línea por fotograma `eventos <f> total <n> cola <q>` (también
cuando `detener todo` congela el despacho o un mensaje queda en espera) y el
resumen `eventos final: N avisos A ejecutados E`. Con `--volcar` añade
`eventos N avisos A ejecutados E`. Sin `--eventos` la traza es igual que en
las fases anteriores.

### Depurador (fase 34)

`--depurar` activa el núcleo de `motor/depurador.c` sin cambiar el juego por
defecto. `--paso bloque` y `--paso instruccion:N` avanzan un bloque o N
instrucciones y vuelven a pausar. `--break B:id` y `--break E:tipo` rompen
por bloque o por evento. `--observar nombre[:indice]` añade una variable a
la lista observada (para una global usa `vidas:0`, porque el lector del
motor guarda el nombre `global`). `--ordenes ruta` ejecuta una orden por
línea mientras el juego está detenido: `pausar`, `continuar`, `paso`,
`paso_instruccion N`, `observar nombre`, `volcar`, `break_bloque N`,
`break_evento N` y `semilla N`. `--volcar-debug ruta` escribe cada parada
con `pausa f b e s pc err`, las observadas y la consola. Los errores en
runtime se anotan como `consola error en pc N`.

`--panel-debug` dibuja sobre la vista Play un panel con la fuente bitmap 5x7:
bloque, evento, script, PC, observados y las últimas líneas de la consola,
con el bloque en ejecución resaltado. Activa también `--depurar`; sin esta
opción las capturas de las fases anteriores son byte a byte idénticas.

`--grabar-replay ruta` guarda la semilla y una máscara de entrada por
fotograma; `--replay ruta` la reproduce sin reloj real. `--semilla N` fija la
semilla base y `--mensaje texto` inyecta un mensaje. `--comparar A B --traza
salida` escribe `sin divergencias` o `divergencia N lineaA lineaB` en una
sola línea; lo usa la suite `pruebas\depurador.ps1`. Ninguna salida nueva
aparece si no se pide con uno de estos flags, así que las once suites
anteriores conservan sus patrones.

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

### lobo 3 (modo VM)

Los bloques `codigo.lobo` de un proyecto LOGIC3 se compilan en modo VM. Los
comentarios empiezan por `#`. La sintaxis anterior sigue siendo válida y se
amplía con tipos, ámbitos, funciones y bucles.

#### Tipos y ámbitos

```lobo
entero vidas = 3           # local del script
booleano listo = 1         # 0 o 1
texto nombre = "Lobo"      # texto entre comillas
lista inventario           # lista local de enteros
lista texto nombres        # lista local de textos
entero $puntos = 0         # global compartida por todos los scripts
booleano @vivo = 1         # variable de la entidad actual
$puntos = $puntos + 1
@energia = @energia - 1
```

Los cuatro tipos son `entero`, `booleano`, `texto` y `lista`. Los prefijos
`$` y `@` convierten la variable en global o de entidad; sin prefijo es local
(o parámetro, dentro de una función). Una declaración puede omitir el valor:
los numéricos valen 0 y los textos valen `""`.

#### Funciones, condicionales y bucles

```lobo
funcion danar(cantidad)
    retornar cantidad * 2
fin

funcion suma(a, b)
    retornar a + b
fin

x = danar(5) + llamar("danar", 3)   # ambas formas son equivalentes
y = suma(x, 7)

si vidas > 0
    x = 1
sino si vidas == 0
    x = 2
sino
    x = 3
fin

repetir 10
    si x > 100
        romper
    fin
    si x == 7
        continuar
    fin
    x = x + 1
fin

para cada objeto en inventario
    x = x + objeto
fin
```

`funcion nombre(a, b)` ... `fin` declara hasta 8 parámetros y `retornar
<expr>` devuelve un valor. Las llamadas directas `nombre(args...)` y el alias
`llamar("nombre", args...)` son equivalentes. Una cadena `sino si` se cierra
con un solo `fin`. `romper` y `continuar` actúan sobre el bucle más interno;
`para cada variable en lista` también admite `de` en lugar de `en`.

#### Built-ins

| Función | Qué hace |
|---|---|
| `abs(v)` | valor absoluto |
| `min(a, b)`, `max(a, b)` | menor y mayor |
| `limitar(v, bajo, alto)` | recorta `v` al intervalo |
| `redondear(v)` | identidad: el lenguaje solo tiene enteros |
| `aleatorio(a, b)` | entero en `[a, b]`, determinista (LCG con semilla 12345 por máquina) |
| `longitud(t)` | caracteres del texto, hasta 63 |
| `letra(t, i)` | código ASCII del carácter `i` |
| `unir(a, b)` | concatena y devuelve un texto de hasta 63 caracteres |

Los textos van entre comillas con los escapes `\"`, `\\`, `\n`, `\r` y
`\t`. Se comparan con `==` y `!=`. Cada texto ocupa 64 bytes: 63 caracteres
útiles y el terminador. Los creados por `unir()` viven en 32 huecos
compartidos por todas las máquinas del proyecto.

#### Listas

```lobo
lista inventario
agregar(inventario, 7)
insertar(inventario, 0, 3)
fijar(inventario, 1, 5)
x = elemento(inventario, 1) + largo(inventario)
si contiene(inventario, 7)
    vaciar(inventario)
fin
ordenar(inventario)
```

| Función | Alias | Qué hace |
|---|---|---|
| `agregar(l, v)` | `anadir` | añade al final |
| `insertar(l, i, v)` | | inserta en la posición |
| `borrar(l, i)` | | borra la posición |
| `fijar(l, i, v)` | | cambia la posición |
| `elemento(l, i)` | | lee la posición |
| `largo(l)` | | número de elementos |
| `contiene(l, v)` | `tiene` | busca un valor |
| `vaciar(l)` | `limpiar` | borra todos los elementos |
| `ordenar(l)` | | ordena de menor a mayor |

#### Variables y listas en LOGIC3

El editor guarda los valores iniciales como registros de una línea. Las
listas se escriben con su capacidad, el tipo de elemento y los valores; los
textos con espacios van entre comillas.

```text
variable global "vidas" entero 3
variable global "nombre" texto "Lobo \"feroz\""
lista global "inventario" 8 entero 3 1 2
lista global "nombres" 4 texto espada "pocion roja"
```

Los tipos admitidos son `entero`, `booleano` y `texto`; los números se
escriben en forma canónica (`007` se guarda como `7`) y los booleanos como
`0` o `1`. Al cargar un proyecto el motor siembra `g_memoria` con esos
valores en orden de declaración: la lista primera del archivo es la lista 0
que ve el script. Las secciones desconocidas se ignoran y `FIN`/`CRC32` siguen
siendo opcionales.

#### Otras sentencias y límites

`esperar <expr>` pausa N fotogramas y reanuda donde estaba, `detener`
termina el script, y `mensaje "texto"`, `clonar`, `enviar("...")` y
`enviar_a("objeto", "...")` interactúan con el planificador de la fase 27.

Límites del modo VM (tabla 3.2): 4096 instrucciones, 64 locales, 64
funciones, 8 parámetros por función, 32 frames de llamada (la recursión se
corta con error antes de colgar), 256 valores de pila, 128 globales, 64
variables de entidad, 32 listas de 256 elementos, 64 textos de programa y 32
textos runtime de 64 bytes (63 caracteres útiles), y 1000000 de pasos por
fotograma. El modo clásico
(LOGIC2) conserva sus topes antiguos.

#### Colisiones y física (fase 29)

Los mapas `.mav` pueden declarar `version = 2` y, en cada celda, los flags
`solido`, `hueco`, `trigger`, `one_way`, `dano` y `tag = "nombre"`. El lector
sigue aceptando `version = 1` (todo sólido); en el v2, una celda sin claves de
flags se considera sólida. `hueco` no colisiona ni se dibuja, `trigger` y
`dano` se anotan sin bloquear, y `one_way` solo bloquea al bajar desde arriba.
El editor conserva estos campos al abrir y guardar.

El motor añade las variables de física `vel_x`, `vel_y`, `gravedad`, `rebote`,
`friccion` y `en_suelo` (índices altos, para no desplazar las locales
clásicas). Con `rebote` en porcentaje y `friccion` restando unidades por
fotograma, un `codigo.lobo` de LOGIC3 puede mover al lobo con gravedad sin
tocar el movimiento por teclado de los proyectos antiguos:

```lobo
gravedad = 2
si tecla_derecha
vel_x = 3
fin
```

Los built-ins de colisiones para scripts de entidades son `toca(id)`,
`toca_tag("nombre")` (el tag es el nombre del objeto), `distancia(id)`
(distancia Manhattan) y `caja_de(id, i)` con `i` de 0 a 3 para x, y, ancho y
alto. Devuelven 0/-1 cuando no hay entidad. Los opcodes nuevos se añadieron
al final del enum y el puerto Rust del kernel aún no los implementa.

`game.exe --colisiones` añade a la traza una línea con `suelo`, `dano` y los
índices de celdas trigger tocadas, sin cambiar el formato de las trazas
antiguas.

#### Render de objetos y trajes (fase 30)

Los objetos LOGIC3 pueden declarar trajes con
`traje <índice> <png> <ancho> <alto> <pivote_x> <pivote_y>` y, tras esos seis
campos, pares opcionales `cuadros`, `cuadro_ancho`, `cuadro_alto`, `flip_x`,
`flip_y`, `rotacion` (0/90/180/270) y `velocidad` (fotogramas por cuadro). El
PNG se busca en `trajes/` junto al proyecto, junto al `.exe` y en el directorio
de trabajo, y se guarda en una caché indexada por traje. Un traje con varias
tiras o un objeto con varios trajes se anima por tiempo; `mi.traje` distinto
de 0 fija un traje concreto. Los objetos se dibujan por capa 0..15 y luego por
id, con culling de las cuatro esquinas y un máximo de 256 blits por fotograma.

La cámara se declara con
`camara objetivo <id> limites <x> <y> <ancho> <alto> zoom <1|2|4>`; sin
`limites` se usa el tamaño del proyecto y sin la línea se mantiene la cámara
clásica que sigue al sprite sin topes. Las opciones de captura `--render`,
`--camara N`, `--zoom 1|2|4`, `--limites x,y,w,h` y `--pixel x,y` permiten
congelar la traza de trajes/blits y sondear colores exactos.

El kernel (`loboswolfos/kernel`) recibe la primitiva `gfx::Canvas::blit_ex`
con el mismo flip, rotación y zoom, y calcula la cámara con el mismo criterio
sobre el sprite clásico. No hay entidades LOGIC3 en el kernel: el espejo se
limita a la primitiva y a la cámara.

#### Vista Estudio (fase 31)

La nueva vista se abre desde **MODO > Vista Estudio** (fila 6 del desplegable),
desde la pestana «Estudio» o con `--estudio`. Las tres columnas son fijas:
200 px de escenas y objetos, el resto para el escenario y 260 px de inspector.
A la resolución de referencia de 1200x700 el centro mide 740 px y el área de
trabajo es `200,52,740,648`, igual que la barra inferior clásica.

El panel izquierdo tiene los botones **Nueva esc.**, **Nuevo obj.**,
**Importar traje** y **Borrar**. Los objetos nuevos reciben los eventos
`inicio` y `fotograma`. Un clic en una fila selecciona la escena o el objeto y
actualiza el inspector; el clic derecho en un objeto conmuta su bloqueo. La
rueda y las flechas recorren la lista, y `Supr` borra la selección. El centro
dibuja la rejilla y los objetos de la escena activa; la barra del estudio
ofrece las vistas Escenario, Objetos, Datos, Sonidos, Bloques y Play.

La selección, el desplazamiento y la pestaña activa se guardan en memoria por
ruta de proyecto y se exponen al final de `--volcar` solo cuando el estudio
está activo: `estudio=1 paneles=200,740,260 sel_escena=... sel_objeto=...`.
Los flags `--escena N` y `--objeto N` seleccionan por id LOGIC3. Los archivos
LOGIC3 no cambian: el estudio reutiliza el escritor canonico y su CRC.

#### Lienzo de bloques (fase 32)

La barra del estudio gana una pestaña **Bloques** antes de **Play**. Se abre
desde esa pestaña, desde **MODO > Vista Estudio** o con `--bloques`; el índice
de la pestaña es 4 y **Play** pasa al 5. A 1200x700 la paleta ocupa la columna
izquierda, el lienzo con rejilla la central y el inspector la derecha.

La paleta agrupa diez definiciones en seis categorías: eventos, control,
movimiento, datos, operadores y código. Hay formas hat, pila, C, reportero,
booleano y cap. Con el botón izquierdo se arrastra una definición al lienzo o
se mueve un bloque raíz; al soltarlo sobre una ranura o sobre el cuerpo de un
bloque C se acopla. El botón derecho duplica el bloque bajo el cursor. La
rueda desplaza el lienzo. `Tab` alterna el foco entre paleta y lienzo, las
flechas recorren categorías, definiciones o bloques, `Intro` añade la
definición activa, `Supr` borra la selección y `Ctrl+Z`/`Ctrl+Y` deshacen y
rehacen. `Ctrl+C`/`Ctrl+V` copian y pegan. Al escribir se edita el primer
campo del bloque seleccionado.

El script del capítulo 4.3 se puede construir solo con gestos y guardar con
**ARCHIVO > Guardar LOGIC3**. El modelo de edición vive aparte del proyecto;
cada mutación vuelca los nodos a `p->bloques` con el escritor LOGIC3 de siempre.
El cuerpo se infiere al reabrir con `padre` y el tipo C, así que el vuelco no
emite líneas `hijo`: el lector actual de LOGIC3 exige tres tokens y el escritor
genera dos. La selección, el historial, el paneo y el zoom no se serializan.

El volcado añade una línea propia solo en la pestaña Bloques:
`lb_bloques=N lb_sel=N lb_ranuras=N lb_cuerpo=N lb_paleta=N lb_categoria=N
lb_foco=N lb_zoom=N lb_pan=X,Y`. El campo `bloques=N` de la primera línea
sigue siendo el conteo LOGIC3. Los flags de prueba son `--bloques`,
`--bloque N`, `--arrastre-bloque "cat:x0,y0;x1,y1|..."` (categoría opcional y
varios gestos separados por `|`) y `--deshacer-bloque`. `--bloque N` se aplica
después de los arrastres para que la selección por id sea la final.

#### Compilador bloques -> lobo (fase 33)

Dentro de la pestaña Bloques, **VER CODIGO (F2)** genera el lobo 3 del lienzo y
lo muestra en el area central en solo lectura, con numeros de linea y colores
para palabras clave, `mi.` y la linea con error. **VOLVER AL LIENZO** o `Esc`
cierran el visor. **CONVERTIR A CODIGO** es irreversible: vuelca el lobo a un
unico bloque `codigo.lobo` y el lienzo deja de ser editable. El codigo se
guarda en `Bloque3.codigo` y el escritor LOGIC3 emite `campo "codigo" ...` con
el mismo formato y CRC de siempre.

El generador recorre las raices en orden visual. Los sombreros de evento no
emiten codigo: el evento ya vive en el proyecto. `mover x` genera
`x = x + N`, `cambiar variable` genera `mi.variable("nombre") = ...`, `si` y
`repetir siempre` generan `si`/`mientras` con su `fin`, y los reporteros se
resuelven dentro de la sentencia que los usa. Todo se valida con
`lobo_compilar(..., LOBO_MODO_VM, ...)`; si falla, el numero de linea se mapea
al id del bloque que se resalta en rojo al volver al lienzo.

El volcado añade una linea `cb_*` solo cuando el compilador ha intervenido
(visor, conversion, codigo generado o error): `cb_visor=N cb_lineas=N
cb_error=TEXTO cb_bloque=N cb_linea=N cb_convertido=N`. El error se sanitiza
para no romper el formato de clave=valor. Los flags de prueba son
`--compilar-bloques`, `--visor-codigo`, `--convertir-bloques`,
`--volcar-codigo RUTA` y `--campo-bloque ID:TEXTO`. El codigo no se genera en
cada mutacion del lienzo: solo se produce al abrir el visor, al compilar
explicitamente o al convertir, para no alterar las capturas de la fase 32.

## Uso

Ejecuta `LobosWolfos2000.exe`. Al arrancar crea junto al ejecutable las
carpetas del proyecto: `tiles` (mosaicos e imágenes), `maps` (mapas `.mav`),
`trajes` (PNG de los objetos LOGIC3), `logic` (archivos `.logic`) y `titles`
(pantallas de título del editor de menús). Los diálogos de abrir y guardar
proponen esas carpetas: los tiles y las imágenes del editor van a `tiles`,
los trajes de los objetos a `trajes`, los mapas a `maps`, las lógicas a
`logic` y los PNG/BMP exportados desde el editor de menús a `titles`. Los tiles que guardes
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
barra de menús. Al abrirlo se muestra directamente la capa **Programa** del
constructor (fase 48): la paleta de piezas está a la izquierda y el lienzo a la
derecha. El botón **COMPAT.** de la barra inferior recupera la vista LOGIC2
clásica. Al elegir **Gestor de lógica** de nuevo se vuelve a Pintar.
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

El menú **AYUDA**, entre **COMPILE** e **INFO**, es la puerta de entrada para
quien empieza. **Crear juego de ejemplo** deja el editor con un primer juego
funcionando sin salir del programa: genera tres tiles de 32x32 (árbol, agua y
roca), un mapa `maps\primer_juego.mav` con un claro vacío rodeado de paredes
sólidas, la lógica con su item de mapa y los ajustes del proyecto (640x480,
60 FPS, velocidad 3 y fondo verde). Basta pulsar **play** para jugarlo.
**Guia paso a paso** abre una ventana con seis pasos que explican qué es cada
zona del editor y cómo llegar a ese primer juego; se avanza con
**SIGUIENTE**/`→` y se retrocede con **ANTERIOR**/`←`, `Escape` o la X la
cierran y en el último paso **SIGUIENTE** pasa a **CREAR EJEMPLO**.

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

El ejemplo `ejemplos\minijuego.logic` muestra ese formato clásico con un
minijuego jugable: baja por el pasillo del mapa y corre hasta la salida, con
colisiones y reinicio con `Espacio`. Se abre con **ARCHIVO > Abrir lógica
.logic** o con `--abrir-logica ejemplos\minijuego.logic`, y el motor lo corre
con `motor\game.exe --logica ejemplos\minijuego.logic`. Al cargarlo en el
gestor, el lienzo del constructor convierte sus dos elementos en piezas reales:
`Mapa` con `mapa.mav` y `Función` con `Laberinto simple`. Se pueden seleccionar,
arrastrar y enlazar como cualquier otra pieza de la paleta; no crean actor,
jugador ni movimiento automático y no se escriben en el `.logic`. Abrir otra
lógica desde **ARCHIVO** estando ya en el gestor sustituye esas piezas por las
del proyecto nuevo sin salir del constructor.

La opción **Guardar proyecto .wolfo** del menú **ARCHIVO** empaqueta el
proyecto entero en un solo archivo: la lógica con sus ajustes
(`proyecto.logic`), los mapas que usan sus elementos, los tiles de esos
mapas, los trajes `trajes\*.png`, los títulos y, desde la fase 45, el modelo
nativo `proyecto.logic3` cuando el proyecto LOGIC3 está activo. Usa el mismo
blob `LGB1` que viaja incrustado en el `game.exe` compilado y termina con el
pie `LGB1` y su longitud, así que un lector puede localizar el blob desde el
final y reconstruir el proyecto en otra carpeta. El nombre propuesto es el
del proyecto.

La opción **Abrir proyecto .wolfo**, justo encima, hace el recorrido inverso:
lee el pie, recorre las entradas del paquete y restaura la lógica (con sus
ajustes), escribe los mapas en la carpeta `maps`, los tiles en `tiles`, los
trajes en `trajes` y los títulos en `titles`, y abre el primer mapa que usa
la lógica. Si el paquete trae `proyecto.logic3` con cabecera `LOGIC3`, el
editor carga ese modelo y la vista previa usa el intérprete VM; si no, cae a
la lógica LOGIC1/LOGIC2 clásica de siempre. Si el paquete no lleva el pie o
una entrada está dañada muestra un aviso y no toca el proyecto activo.

Desde la línea de comandos, `--guardar-wolfo RUTA` mantiene el paquete clásico
de la fase 15 por compatibilidad. Añade `--wolfo-logic3` para incluir la
entrada nativa `proyecto.logic3`; el menú **ARCHIVO > Guardar proyecto**
siempre exporta completa. `--abrir-wolfo RUTA` detecta la nueva entrada sin
necesidad de otra flag. `--guardar-logic3 RUTA` guarda solo el modelo activo
en formato LOGIC3, y `--abrir-logic3 RUTA` lo carga al arrancar.

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
último en la carpeta `tiles` del ejecutable. Los mapas guardados declaran
`version = 2` y conservan los flags de colisión de cada celda (`solido`,
`hueco`, `trigger`, `one_way`, `dano`, `tag`); los tiles `hueco` no se dibujan
en la rejilla ni en la miniatura del selector de items.

El menú **COMPILE** de la barra de menús, entre **AJUSTES DEL PROYECTO** y
**AYUDA**, ofrece **Compilar para BIOS** y **Compilar para Windows**. Ambos toman
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
funciona en cualquier carpeta sin necesidad de `maps/`, `tiles/`, `trajes/`
ni del `.logic` original. Si el modelo LOGIC3 está activo, el blob incluye la
entrada `proyecto.logic3` y el motor selecciona la VM para los bloques
`codigo.lobo`; si no, conserva la ruta clásica sin cambios. Equivale a la
flag `--compilar-windows RUTA`.

**Compilar para BIOS** produce la imagen USB de 32 MiB de loboswolfos
(`.img`): MBR + arranque + kernel + el proyecto en la zona de juego; escribida
en un pendrive con `flashear.bat`, el juego arranca desde la BIOS del PC sin
Windows. Las plantillas (el motor y el arranque del kernel) viajan dentro del
propio editor, por lo que compilar no requiere tener MinGW ni Rust
instalados. Equivale a `--compilar-bios RUTA`. `--bios-avisos RUTA` escribe
un informe de los bloques no portables al subconjunto VM de la BIOS; un
proyecto portable deja el archivo vacío. La BIOS mantiene su carga actual
con `vm.lbin` para no alterar las trazas de la fase 42.

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
que coincide píxel a píxel con `game.exe --captura` cuando ambos abren el
mismo LOGIC3 con los mismos fotogramas. El caso `preview_captura` verifica
esa igualdad con el LOGIC3 generado desde el `.log` de ejemplo.

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
`--bloques` (abre la pestana Bloques del estudio), `--bloque N` (selecciona un
bloque por id LOGIC3), `--arrastre-bloque "cat:x0,y0;x1,y1|..."` (gestos del
lienzo, con categoria opcional) y `--deshacer-bloque` (revierte el ultimo
cambio del modelo),
`--copiar`, `--cortar` y `--pegar` (portapapeles tras el arrastre),
`--vista menus` (abre directamente el editor de
menús, después de cargar la lógica si se indica) y `--volcar ruta`
(escribe una línea con la vista, los documentos, los ajustes del proyecto,
la geometría del lienzo, los módulos del gestor, el mapa abierto, el paso de
la guía, el estado de los menús y, en la pestana Bloques, los campos `lb_*`
del modelo, para comparar pruebas),
`--abrir-logic3 ruta` y `--guardar-logic3 ruta` (abren y guardan un proyecto
LOGIC3 sin diálogo), `--convertir-logic2 ruta` (convierte el LOGIC2 en
`ruta.logic3` y lo deja activo) y `--exportar-logic2 ruta` (exportación de
emergencia del modelo LOGIC3 a LOGIC2),
y, para la sección FUNCIONES (fase 8),
`--funcion-abrir <id>` (abre el editor de código de esa función),
`--codigo-teclas "texto"` (escribe en el editor abierto, con `\n`, `\t`,
`\b` y `\\` como escapes), `--texto-teclas "texto"` (escribe en la caja de
la herramienta de texto después de `--arrastre`, también con escapes),
`--clic-final x,y` (confirma el texto con un clic y descarta la caja vacía
que abre el clic), `--deshacer` (pulsa `Ctrl+Z` al final), `--sin-ventanas`
(oculta opciones, patrón, información y visor de tiles en las capturas),
`--maximizar` (captura con la ventana maximizada),
`--ejemplo` (crea el juego de ejemplo de AYUDA sin pasar por el menú),
`--guia` y `--guia-paso N` (abren la guía en el paso indicado),
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
| `Tab` (en Bloques) | Alternar entre paleta y lienzo |
| `Esc` | Cancelar la acción en curso |
| `Enter` | Confirmar la acción en curso |
| `Supr` | Borrar la selección |

## Contrato del ProjectModel

`pruebas/modelo/juego_ia.log` es el fixture de esta entrega: un `.log` de
ejemplo con cabecera LOGIC3 y una sección `montaje` de cinco nodos y tres
`conecta`. El contrato que fijan las pruebas es:

1. `src/proyecto_modelo.h` expone entidades con ID estable, tipo semántico,
   nombre, propiedades, referencias por ID, estado de UI, origen con archivo y
   línea, y conexiones con puertos. Los IDs no se reciclan y las vistas guardan
   IDs, nunca punteros.
2. `--leer-modelo "entrada.log|volcado.txt|diagnostico.txt|recursos.txt"`
   convierte ese fixture sin tocar los lectores de LOGIC3 ni crear actor,
   jugador o movimiento en el juego.
3. `pm_validar_recursos()` exige que cada recurso tenga archivo y que cada
   actor reciba uno por conexión. La orden
   `--diagnostico-json "entrada.log|salida.json"` emite lo mismo como JSON con
   `version`, `proyecto`, `estado`, `total` y `errores[]`; cada error trae
   `codigo`, `archivo`, `linea`, `id_entidad`, `nombre`, `sugerencia` y,
   cuando aplica, `ruta`. Así una IA sabe qué archivo abrir y qué corregir.
4. `--gestor-modelo "entrada.log"` proyecta el grafo en piezas reales del
   lienzo y en el inspector sin escribir LOGIC3.
5. `--generar-logic3 "entrada.log|salida.logic3"` delega en
   `logic3_guardar()`, así que el formato y el CRC no cambian. El caso
   `modelo_logic3` exige la misma traza y el mismo BMP que el `.log`.
6. La orden `--captura-juego ruta --fotogramas N` de la vista previa y la
   orden `--captura ruta --fotogramas N` de `game.exe` producen el mismo BMP
   para el LOGIC3 generado; el caso `preview_captura` lo comprueba.
7. `src/inicio.c` expone el servicio de la futura vista Inicio. `pm_borrar()`
   quita una entidad y sus conexiones, `pm_reparentar()` guarda el padre en la
   referencia `padre` y rechaza ciclos, y cada cambio guarda un snapshot antes
   de aplicarse, así que `deshacer` y `rehacer` cubren crear, borrar y
   reparentar con el mismo historial. La flag
   `--inicio "cargar|ruta;crear|kind|nombre|padre;borrar|id;reparentar|id|padre;deshacer;rehacer;volcar|ruta"`
   encadena órdenes y permite volcar estados intermedios. No dibuja, no toca
   menús y no escribe LOGIC3.

Las comprobaciones están en `pruebas/programa.ps1` y sus patrones en
`pruebas/programa/esperado/`. El caso `recorrido_autoria` encadena la lectura,
la validación, el guardado y la ejecución para detectar cambios que los casos
aislados no verían. Ninguno de estos flags modifica el archivo de entrada ni
desplaza los índices de menú (`ARCHIVO=1`, `MODO=2`, `AYUDA=9`).

## Estructura

- `src/` — código fuente C y recursos de la interfaz. `src/compilar.c` es el
  menú COMPILE (compilar el proyecto para Windows y para BIOS) y el paquete
  `.wolfo`,
  `src/juego_ventana.c` la vista previa de los botones play/pausa/stop,
  `src/ajustes.c` las propiedades del proyecto, `src/logic3.c` el modelo, el
  lector, el escritor y el migrador del formato LOGIC3, `src/ejemplo.c` el
  juego de ejemplo y su guía (menú AYUDA), `src/estudio.c` y
  `src/estudio_ui.c` la vista Estudio y `src/bloques.c` y
  `src/bloques_ui.c` el modelo y el dibujo del lienzo de bloques.
  `src/proyecto_modelo.c` y `src/proyecto_modelo.h` definen el contrato minimo
  del `ProjectModel` del plan: entidades con ID estable, propiedades,
  referencias, estado de UI, origen y conexiones con puertos. No dibuja ni
  escribe LOGIC3. `src/modelo_log.c` y `src/modelo_log.h` leen el `.log` de
  ejemplo con seccion `montaje` y lo convierten a ese modelo sin tocar los
  lectores de LOGIC3. `pm_validar_recursos()` comprueba en disco cada recurso
  y que cada actor recibe un recurso por una conexion, sin tratar el ejemplo
  como un caso especial. El flag
  `--leer-modelo "entrada.log|volcado.txt|diagnostico.txt|recursos.txt"`
  expone la conversion y la validacion en las pruebas; el ultimo campo es
  opcional, no abre ventanas ni modifica archivos de juego.
  `pm_validar_recursos_json()` reutiliza esa misma comprobacion y devuelve un
  JSON de una linea con `estado`, `total` y un `errores` con codigo, archivo,
  linea, id, nombre y sugerencia de cada fallo; `--diagnostico-json
  "entrada.log|salida.json"` lo escribe para que una IA pueda leerlo sin
  interpretar texto libre.
  `programa_proyectar_modelo()` muestra ese mismo grafo en el Gestor de
  logica: cada entidad se convierte en una pieza real del lienzo, cada
  `conecta` pasa a ser el `enlace` de la pieza de origen y el inspector
  añade el resumen `grafo: N entidades, C conexiones` con su origen. El flag
  `--gestor-modelo "entrada.log"` abre esa vista sin escribir LOGIC3 y sin
  crear actor, jugador ni movimiento.
  `src/inicio.c` y `src/inicio.h` añaden `pm_borrar()`, `pm_reparentar()`,
  `pm_padre()` y `pm_copiar()` al modelo, y mantienen el servicio Inicio con
  16 snapshots para deshacer y rehacer. La vista visual se enganchara después
  a `inicio_servicio()`; por ahora solo existe la via CLI.
  `src/modelo_logic3.c` y `src/modelo_logic3.h` cierran el ciclo: convierten
  el modelo en bloques `sistema.pieza` y delegan en `logic3_guardar()`, asi
  que el formato y el CRC siguen siendo los del escritor canonico. La
  cabecera sale de la entidad de proyecto y las conexiones sobrescriben el
  campo `enlace` de la pieza de origen. El flag
  `--generar-logic3 "entrada.log|salida.logic3"` expone esa conversion sin
  abrir ventanas; el caso `modelo_logic3` comprueba que la traza del motor
  con el archivo generado es identica a la del `.log` original.
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
- `MANUAL_BLOQUES.md` — paleta, formas, atajos y los 27 bloques del lienzo.
- `REFERENCIA_LENGUAJE.md` — gramatica lobo, tipos, built-ins y limites.
- `pruebas/exportacion/esperado/` — patrones propios de la suite 23.

## Licencia

Uso interno de Green Blue Studios.
