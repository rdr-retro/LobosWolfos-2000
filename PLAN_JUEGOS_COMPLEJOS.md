# Plan maestro: juegos complejos tipo Scratch en LobosWolfos 2000

Estado: propuesta de ejecucion por otra IA
Version: 1.0 (15-11-2026)
Punto de partida: commit `baaa4ba` (fase 22)
Documentos que se deben leer antes: `README.md`, `PLAN.md` y este archivo.

---

## 0. Como debe usar este documento otra IA

Este plan esta pensado para que un agente lo ejecute **fase por fase**, sin
improvisar la arquitectura. Las reglas son obligatorias:

1. Leer primero `README.md`, `PLAN.md`, `PLAN_JUEGOS_COMPLEJOS.md` y los
   archivos que toque la fase.
2. Ejecutar **una sola fase por commit**. No mezclar fases.
3. Antes de tocar codigo, compilar y guardar la linea base:
   `build.bat`, `--captura`, `--volcar`. Anotar los hashes.
4. No romper `LOGIC1` ni `LOGIC2`. Todo proyecto antiguo debe seguir abriendo
   y ejecutandose igual.
5. No cambiar el formato `.mav` sin subir su version y mantener el lector
   anterior.
6. No anadir dependencias externas, frameworks, DLLs ni runtimes. Todo debe
   compilar con MinGW-w64 i686 y funcionar en Windows 2000/XP de 32 bits.
7. Codigo y comentarios en espanol **sin acentos**. Cadenas de interfaz con
   acentos. Fuente bitmap 5x7 para el editor.
8. `recursos/ascii.png` no se versiona y no se debe commitear.
9. Cada fase termina con: compilacion sin warnings, pruebas automaticas,
   verificacion por pixeles/clics, documentacion actualizada, commit en
   `main`, push y release `v1.0-beta` actualizada (hasta la fase 45, que crea
   `v2.0-beta`).
10. Si una fase no se puede terminar sin romper compatibilidad, se deja
    detras de un flag y se documenta el bloqueo. No se fuerza.

Formato del informe de cada fase:

```text
Fase NN: titulo
Archivos: ...
Formato: LOGIC3 vN (si cambia)
Pruebas: comandos y resultados
Pixeles/clics: coordenadas y resultados
Riesgos: ...
Commit: <hash>
Release: <hash exe> / <hash game.exe>
```

---

## 1. Vision del producto

LobosWolfos 2000 debe pasar de "editor de texturas con un gestor de logica de
un solo sprite" a un **estudio de videojuegos 2D completo**, con un editor
visual de bloques mas potente que Scratch y un runtime determinista que quepa
en un disquete de 32 MiB.

La experiencia objetivo:

- Una persona sin programar abre el editor, pulsa **Juego nuevo**, elige una
  plantilla y en minutos tiene un personaje que se mueve, recoge objetos y
  cambia de escena.
- Un usuario avanzado abre el **visor de codigo** del mismo proyecto, ve el
  lobo generado, lo edita y vuelve a los bloques.
- Un profesor puede grabar una partida, reproducirla y comparar resultados
  para corregir ejercicios.
- El mismo proyecto corre en Windows y en la imagen BIOS, con un subconjunto
  de funciones claramente indicado.

### 1.1 Pilares

1. **Visual primero, codigo siempre disponible.** Los bloques son la fuente
   de verdad, pero el codigo lobo generado se puede ver, copiar y (en modo
   avanzado) editar.
2. **Determinismo.** Misma entrada, mismo resultado, mismo BMP. Sin relojes
   ocultos, sin aleatoriedad no sembrada, sin hilos.
3. **Presupuestos visibles.** El editor muestra memoria, bloques, entidades y
   coste por fotograma antes de compilar.
4. **Compatibilidad hacia atras.** LOGIC1 y LOGIC2 siguen funcionando.
5. **Cero dependencias.** C, Win32/GDI, ASM y Rust `no_std` para el kernel.
6. **Depuracion de verdad.** Valores en vivo, pausa, paso a paso,
   breakpoints, traza y replay.
7. **Textos versionables.** `.logic` y `.mav` son texto diff-friendly.

### 1.2 Antejemplos (lo que NO se hara)

- No se implementara 3D.
- No habra multijugador en red ni nube.
- No se usara JavaScript, Lua embebido ni un motor externo.
- No se anadira un interprete de otro lenguaje.
- No se rompera la interfaz clasica de pintura, tiles, mapas y menus.
- No se usara coma flotante en el bucle principal; todo entero o punto fijo.

---

## 2. Estado actual (resumen tecnico)

### 2.1 Lo que ya existe

| Pieza | Archivos clave | Estado |
|---|---|---|
| Editor Win32 | `src/app.c`, `src/ui.c`, `src/nucleo.c` | Solido, 4600+ lineas en `ui.c` |
| Pintura, tiles, mapas | `src/pintura.c`, `src/tiles.c`, `src/mapa.c` | Completo para el alcance actual |
| Gestor de logica | `src/app.c` (modelo), `src/ui.c` (dibujo) | Lista plana de tarjetas, sin grafo |
| Editor de codigo lobo | `src/app.c` (`funciones_codigo_*`) | 10 lineas visibles, sin undo ni seleccion |
| Lenguaje lobo | `motor/lobo.c`, `loboswolfos/kernel/src/lobo.rs` | Enteros, `si`/`mientras`, 6 teclas; VM con `esperar`, funciones, listas, `mi` y entidades (fases 25-26) |
| Runtime Windows | `motor/juego.c`, `motor/activo.c`, `motor/nucleo32.s`, `motor/entidades.c` | Un lobo, un mapa, 256 entidades y 60 FPS |
| Depurador | `motor/depurador.c`, `motor/panel_debug.c` | Pausa, paso, breakpoints, observados, consola y replay (fase 34) |
| Runtime BIOS | `loboswolfos/kernel/src/*.rs` | Reimplementacion paralela en Rust |
| Vista previa | `src/juego_ventana.c` | Usa el mismo blob y motor que `game.exe` |
| Proyecto LOGIC3 | `src/logic3.c`, `src/logic3.h`, `motor/logic3_motor.c` | Modelo, lector, escritor y migrador (fase 24); el runtime lee escenas, objetos y scripts (fases 25-26) |
| Paquete | `src/compilar.c` | Blob `LGB1`, `.wolfo`, BIOS |

### 2.2 Modelo de datos actual

```c
typedef struct {
    int x, y, ancho, alto;
    int id;
    int plegado;
    char mapa[MAPS_NOMBRE_MAX];
    int funcion;      /* -1 item, 0 inicio, >=1 funcion lobo */
} ModuloElemento;

typedef struct {
    int id;
    char nombre[24];
    char codigo[2048];
    int largo, cursor, scroll;
    char error[LOBO_ERROR_MAX];
    int error_linea;
} FuncionCodigo;

typedef struct {
    char nombre[48];
    int ancho, alto;
    int limitar_cuadros, fps, velocidad;
    u32 fondo;
} AjustesProyecto;
```

El orden de ejecucion se deduce de la posicion (`y`, luego `x`, luego `id`).
La agrupacion con iman es puramente geometrica. El runtime **no conoce** los
ids, las posiciones ni los programas del editor: solo ejecuta en orden los
scripts con `funcion >= 1`.

### 2.3 Limites que hoy impiden juegos complejos

| Limite | Valor actual | Necesidad tipo Scratch |
|---|---:|---|
| Sprites/entidades | 1 lobo + 256 entidades | 64-256 |
| Mapas/escenas | 1 en pantalla | 64 |
| Objetos con script | 32 funciones clasicas; 64 en la VM | 128 objetos x 64 scripts |
| Instrucciones por script | 256 clasicas; 4096 en la VM | 4096 |
| Codigo por funcion | 2048 bytes | 64 KB por objeto |
| Variables locales | 16 por tarjeta; 64 por entidad en la VM | 64 por objeto |
| Variables globales | `$globales` (128 en la VM) | 128 |
| Listas | 32 listas x 256 elementos en la VM | 32 x 256 |
| Tipos | entero | entero, booleano, texto |
| Eventos | `fotograma` en la VM | 8 clases |
| Temporizadores/espera | `esperar` resumible en la VM | si |
| Funciones de usuario | `funcion`/`retornar` (32 frames) | si, con parametros y retorno |
| Colision objeto-objeto | no | si, por caja y por grupo |
| Tiles con flags | no (todo solido) | solido, hueco, trigger, one-way |
| Animacion | no | trajes, frames, tics |
| Capas Z | orden por capa en las entidades | 16 capas |
| Camara | centrada fija | objetivo, limites, zoom entero |
| Raton/teclas | 6 bits | 32 teclas + raton + flancos |
| HUD/texto | no | si |
| Audio | no | efectos y musica simple |
| Guardado | no | si |
| Depurador | solo error de parseo | valores, pausa, paso, breakpoints |

### 2.4 Activos que se deben reutilizar

- El **blob LGB1** y el empaquetado `.wolfo`.
- El **sistema de mapa `.mav`** y el render ASM de tiles.
- El **editor de texto** y la fuente bitmap.
- La **captura determinista** y los flags `--captura`, `--captura-juego`,
  `--clics`, `--arrastre`, `--volcar`.
- La **vista previa** con el mismo binario del motor.
- El **modelo de paneles** y la barra de menus existente.

---

## 3. Arquitectura objetivo

```text
+----------------------------- EDITOR ------------------------------+
| Vista Estudio (nueva)                                             |
|  - Escenario / Objetos / Datos / Sonidos / Play                   |
|  - Paleta de bloques  |  Lienzo de bloques  |  Inspector          |
|  - Consola de errores, valores en vivo, perfilador, replay        |
+------------------------------+------------------------------------+
                               | guarda LOGIC3 + assets
                               v
+--------------------------- PROYECTO ------------------------------+
| proyecto.logic3 (texto)                                           |
|   proyecto, escenas, objetos, trajes, eventos, bloques, variables |
| maps/*.mav, trajes/*.png, sonidos/*.wav, titles/*.png            |
+------------------------------+------------------------------------+
                               | compila a lobo 3 (texto -> bytecode)
                               v
+--------------------------- RUNTIME ------------------------------+
| VM resumible (C, paridad Rust)                                    |
| Planificador de eventos -> scripts -> entidades -> colisiones     |
| Render por capas -> HUD -> audio -> guardado                      |
+------------------------------+------------------------------------+
                               |
        +----------------------+----------------------+
        v                                             v
  game.exe / vista previa                     imagen BIOS (subset)
```

### 3.1 Decisiones estructurales

1. **Nuevo formato `LOGIC3`** con secciones explicitas. El editor guarda
   bloques y el compilador genera lobo 3. `LOGIC2` se importa y se convierte.
2. **El runtime sigue ejecutando lobo**, no un grafo directamente. Asi solo
   hay un lenguaje que mantener en C y Rust.
3. **La VM pasa a ser resumible** (PC, pila y frames persistentes) para
   soportar `esperar`, eventos y funciones de usuario sin romper el bucle.
4. **Entidades con identidad**. Cada objeto tiene un id de tipo y cada
   instancia un id de entidad. Los scripts reciben `mi` y pueden consultar
   otras entidades.
5. **Eventos de primera clase**. Un script pertenece a un evento, no a una
   posicion en el lienzo.
6. **La vista clasica de logica se conserva** como "Gestor de logica
   (clasico)" para abrir y convertir proyectos antiguos.
7. **Todo texto**. LOGIC3, `.mav`, listas de bloques y saves son texto para
   que otra IA pueda inspeccionarlos y para que git muestre diffs utiles.

### 3.2 Presupuestos de la version 2.0

Estos valores son el contrato de memoria. Se pueden ajustar solo subiendo la
version del formato y documentandolo.

| Recurso | Windows | BIOS |
|---|---:|---:|
| Escenas | 64 | 16 |
| Tipos de objeto | 128 | 64 |
| Entidades simultaneas | 256 | 128 |
| Trajes por objeto | 32 | 16 |
| Scripts por objeto | 64 | 32 |
| Bloques por script | 512 | 256 |
| Bloques totales | 16384 | 4096 |
| Variables globales | 128 | 64 |
| Variables por objeto | 64 | 32 |
| Listas | 32 | 16 |
| Elementos por lista | 256 | 128 |
| Texto por valor | 64 bytes | 32 bytes |
| Funciones de usuario | 64 | 32 |
| Parametros por funcion | 8 | 8 |
| Profundidad de llamadas | 32 | 16 |
| Pila de VM | 256 | 128 |
| Instrucciones por fotograma | 1.000.000 | 500.000 |
| Entidades creadas por fotograma | 32 | 16 |
| Sonidos | 32 | 8 |
| Tamano del blob | 16 MiB | 1 MiB |

---

## 4. Formato LOGIC3

### 4.1 Principios

- Texto UTF-8 sin BOM, lineas terminadas en `\n` (el editor puede leer CRLF).
- Una linea = un registro. Campos separados por espacios; textos entre
  comillas dobles; escapes `\"`, `\\`, `\n`.
- Los ids son enteros positivos y **nunca se reutilizan** dentro de un
  proyecto.
- El orden de las secciones es fijo. Las secciones desconocidas se ignoran
  para permitir extensiones futuras.
- Al final se escribe `FIN` y un `CRC32` opcional.

### 4.2 Gramatica

```text
LOGIC3
version 3
proyecto "<nombre>" <ancho> <alto> <limitar> <fps> <velocidad> <FONDO8>
carpetas "<tiles>" "<maps>" "<trajes>" "<sonidos>"
variable global "<nombre>" <tipo> <valor>
lista global "<nombre>" <capacidad>
escena <id> "<nombre>" "<mapa.mav>" <ancho> <alto> <fondo8>
objeto <id> "<nombre>" escena <id> x <x> y <y> w <w> h <h> visible <0|1> capa <n>
  traje <indice> "<png>" <ancho> <alto> <pivote_x> <pivote_y>
  variable "<nombre>" <tipo> <valor>
  evento <id> inicio
  evento <id> fotograma
  evento <id> tecla "<nombre>" <flanco>
  evento <id> recibir "<mensaje>"
  evento <id> tocar <objeto_id|-1> <tag>
  evento <id> cada <fotogramas>
  evento <id> clonar
  evento <id> escena <id>
bloque <id> evento <id> padre <id|0> orden <n> tipo "<categoria.nombre>"
  campo "<clave>" "<valor>"
  ranura "<clave>" <bloque_id>
  hijo <bloque_id>
funcion <id> "<nombre>" <parametros> <retorno>
  evento <id> llamada
  bloque ...
FIN
CRC32 <hex>
```

Reglas:

- `padre 0` significa bloque raiz del evento.
- `orden` ordena hermanos dentro del mismo padre.
- `hijo` enlaza el primer bloque de una ranura C (cuerpo de `si`, `repetir`).
- `ranura` enlaza un reportero a una entrada; el reportero no lleva `orden`.
- `campo` guarda literales o expresiones lobo. Si el campo empieza por `=`,
  el resto es una expresion que se compila; si no, es literal.
- Un `evento llamada` define una funcion de usuario reutilizable.
- Los bloques `codigo.lobo` permiten pegar lobo crudo dentro de un evento;
  son la via de escape y la base de la migracion.

### 4.3 Ejemplo minimo

```text
LOGIC3
version 3
proyecto "Primer juego" 640 480 1 60 3 001E5A2E
carpetas "tiles" "maps" "trajes" "sonidos"
variable global "vidas" entero 3
lista global "inventario" 16
escena 1 "Nivel 1" "primer_juego.mav" 640 480 001E5A2E
objeto 10 "Jugador" escena 1 x 0 y 0 w 24 h 24 visible 1 capa 5
  traje 0 "jugador.png" 24 24 12 12
  variable "energia" entero 100
  evento 100 inicio
  evento 101 tecla "derecha" mantener
  evento 102 recibir "golpe"
bloque 1000 evento 100 padre 0 orden 0 tipo "control.repetir_siempre"
  hijo 1001
bloque 1001 evento 100 padre 1000 orden 0 tipo "movimiento.mover_x"
  campo "dx" "=2"
bloque 1100 evento 101 padre 0 orden 0 tipo "movimiento.mover_x"
  campo "dx" "=3"
bloque 1200 evento 102 padre 0 orden 0 tipo "datos.cambiar_variable"
  campo "variable" "energia"
  campo "delta" "=-10"
FIN
CRC32 00000000
```

### 4.4 Versionado y migracion

- `LOGIC1`: solo items de mapa, sin funciones.
- `LOGIC2`: items + funciones de texto + `PROYECTO`. Sigue siendo el formato
  que entiende el motor actual.
- `LOGIC3`: proyecto completo con escenas, objetos, eventos, bloques,
  variables, listas y funciones.
- Al abrir un `LOGIC2`, el editor ofrece **Convertir a LOGIC3**:
  - crea una escena con el primer mapa,
  - crea un objeto `Escena` con un traje vacio,
  - convierte cada funcion en un bloque `codigo.lobo` dentro de un evento
    `fotograma` del objeto `Escena`, conservando el orden espacial,
  - conserva `PROYECTO` como cabecera.
- Al guardar un LOGIC3 no se escribe LOGIC2. El motor nuevo lee ambos.
- Un LOGIC3 con `version` mayor que la soportada se rechaza con un mensaje
  claro y no se toca el proyecto activo.

### 4.5 Validacion

Antes de compilar, el editor valida:

- ids unicos; referencias existentes; escenas con mapa existente;
- al menos un evento `inicio` o `fotograma` por objeto activo;
- bloques con campos obligatorios;
- expresiones lobo que compilen;
- limites de la tabla 3.2;
- que cada objeto tenga al menos un traje o sea invisible.
- El resultado se muestra en la consola con `linea N: motivo`.

---

## 5. Modelo de runtime

### 5.1 Entidades y escenas

```c
typedef struct {
    int id;
    int tipo;              /* indice de objeto */
    int escena;
    int x, y;              /* pixeles de mundo */
    int ancho, alto;
    int visible;
    int capa;              /* 0..15 */
    int traje;
    int frame;
    int activa;
    int es_clon;
    int vida;              /* bitset de eventos activos */
    i32 vars[64];          /* variables por objeto */
    char texto[4][64];     /* textos por objeto, opcional */
} Entidad;
```

- El tipo de objeto guarda los valores iniciales inmutables.
- Cada instancia tiene sus variables y su estado.
- `mi` en lobo apunta a la entidad actual.
- `crear("<objeto>", x, y)` devuelve un id de entidad o `-1`.
- `borrar()` marca la entidad para eliminacion al final del fotograma.
- Limite de entidades simultaneas segun tabla 3.2.

### 5.2 Eventos y planificador

Orden de despacho por fotograma:

1. Eventos de sistema: `inicio` solo al entrar en escena.
2. Entrada: actualizar teclado/raton y detectar flancos.
3. `fotograma` de todas las entidades activas, en orden de capa y luego id.
4. `tecla` mantenida y `tecla` pulsada/soltada.
5. `cada N` cuyo contador llegue a 0.
6. `tocar` de las entidades que colisionan, con cooldown configurable.
7. `recibir` de la cola de mensajes del fotograma.
8. `clonar` de las entidades creadas ese fotograma.
9. Eliminar entidades marcadas, limpiar mensajes y contadores.

Reglas:

- Cada script tiene su propia maquina (`pc`, pila, locales, estado).
- Un script puede estar `listo`, `esperando` o `terminado`.
- `esperar N` deja el script esperando N fotogramas sin bloquear a los demas.
- `detener este script` lo termina; `detener todo` detiene la escena.
- El planificador tiene un presupuesto por fotograma; si se agota, avisa en
  consola y pospone el resto al siguiente.

### 5.3 Variables, listas y tipos

- Tipos: entero de 32 bits, booleano (0/1) y texto de longitud fija.
- Ambitos: global, objeto, local de script y parametro.
- Listas de enteros y listas de texto con capacidad fija.
- Operaciones: fijar, cambiar, sumar, insertar, borrar, elemento, longitud,
  contiene, ordenar.
- Las variables se pueden marcar como "observadas" en el editor.
- No hay punteros ni memoria dinamica.

### 5.4 Colisiones

- Entidad vs entidad: AABB con cajas por traje.
- Entidad vs mapa: celdas con flags.
- Grupos/tags: `toca("<tag>")`.
- Capas de colision: bitmask por entidad.
- Tiles: `solido`, `hueco`, `trigger`, `one_way`, `dano`, `tag`.
- La colision no mueve por si sola; el bloque de fisica decide.

### 5.5 Render

- Mundo: mapa, entidades por capa, particulas simples.
- Pantalla: HUD, texto, barras, botones.
- Camara: objetivo, limites de escena, zoom entero 1x/2x/4x.
- Trajes: PNG con pivote, flip horizontal, rotacion 0/90/180/270.
- Animacion: tira de frames o lista de trajes, `animar(ini, fin, fps)`.
- Texto: fuente bitmap 5x7 y fuente grande 2x.

### 5.6 Audio

- Formato WAV PCM 8/16 bits, mono, 11025/22050 Hz.
- Windows: `waveOut` con mezclador simple de 8 canales.
- BIOS: altavoz del PC o Sound Blaster si se implementa despues.
- Bloques: `sonar(id)`, `detener_sonido`, `volumen`, `musica(id)`.
- Sin streaming; cada sonido se carga entero con tope de 256 KB.

### 5.7 Guardado

- `guardar("<clave>", valor)` y `cargar("<clave>")`.
- Windows: `saves/<proyecto>.sav`, texto con una linea por clave, escritura
  atomica (temporal + rename).
- BIOS: sector reservado o sistema de archivos simple (fase 43).
- Limite: 64 KB por proyecto.
- El editor puede borrar el save desde el menu.

---

## 6. Catalogo de bloques v1

Los identificadores son estables y se usan en LOGIC3 y en el compilador.
Cada bloque tiene forma: `hat` (evento), `pila` (accion), `c` (cuerpo),
`reporter` (valor), `booleano` (condicion) o `cap` (fin de pila).

### 6.1 Eventos

| id | Forma | Texto | Semantica |
|---|---|---|---|
| `evento.inicio` | hat | al empezar | una vez al entrar en escena |
| `evento.fotograma` | hat | cada fotograma | cada tick |
| `evento.tecla` | hat | al pulsar tecla | flanco de bajada |
| `evento.tecla_soltar` | hat | al soltar tecla | flanco de subida |
| `evento.recibir` | hat | al recibir mensaje | broadcast |
| `evento.tocar` | hat | al tocar | colision con objeto/tag |
| `evento.cada` | hat | cada N fotogramas | temporizador |
| `evento.clonar` | hat | al clonar | nueva instancia |
| `evento.escena` | hat | al cambiar de escena | transicion |

### 6.2 Control

| id | Forma | Texto |
|---|---|---|
| `control.esperar` | pila | esperar N fotogramas |
| `control.repetir` | c | repetir N |
| `control.repetir_siempre` | c | por siempre |
| `control.repetir_hasta` | c | repetir hasta condicion |
| `control.si` | c | si condicion |
| `control.si_sino` | c | si condicion / sino |
| `control.para_cada` | c | para cada elemento de lista |
| `control.romper` | cap | romper |
| `control.continuar` | cap | continuar |
| `control.detener_script` | cap | detener este script |
| `control.detener_todo` | cap | detener todo |
| `control.clonar` | pila | clonarme |
| `control.borrar_clon` | cap | borrar este clon |
| `control.crear` | pila | crear objeto en x,y |
| `control.llamar` | pila | llamar funcion |

### 6.3 Movimiento

| id | Forma | Texto |
|---|---|---|
| `movimiento.mover_x` | pila | cambiar x en dx |
| `movimiento.mover_y` | pila | cambiar y en dy |
| `movimiento.fijar_x` | pila | fijar x |
| `movimiento.fijar_y` | pila | fijar y |
| `movimiento.ir_a` | pila | ir a x,y |
| `movimiento.ir_a_objeto` | pila | ir a objeto |
| `movimiento.deslizar` | pila | deslizar a x,y en N |
| `movimiento.apuntar` | pila | apuntar en direccion |
| `movimiento.girar` | pila | girar N grados |
| `movimiento.rebotar` | pila | rebotar en borde |
| `movimiento.pos_x` | reporter | x |
| `movimiento.pos_y` | reporter | y |
| `movimiento.direccion` | reporter | direccion |

### 6.4 Apariencia

| id | Forma | Texto |
|---|---|---|
| `apariencia.mostrar` | pila | mostrar |
| `apariencia.ocultar` | pila | ocultar |
| `apariencia.traje` | pila | cambiar traje |
| `apariencia.siguiente_traje` | pila | siguiente traje |
| `apariencia.tamano` | pila | fijar tamano % |
| `apariencia.cambiar_tamano` | pila | cambiar tamano % |
| `apariencia.capa` | pila | ir a capa |
| `apariencia.efecto` | pila | fijar efecto |
| `apariencia.decir` | pila | decir texto |
| `apariencia.pensar` | pila | pensar texto |
| `apariencia.animar` | pila | animar frames |
| `apariencia.traje_actual` | reporter | traje actual |

### 6.5 Sonido

| id | Forma | Texto |
|---|---|---|
| `sonido.sonar` | pila | sonar sonido |
| `sonido.detener` | pila | detener sonidos |
| `sonido.volumen` | pila | fijar volumen |
| `sonido.musica` | pila | reproducir musica |
| `sonido.volumen_actual` | reporter | volumen |

### 6.6 Sensores

| id | Forma | Texto |
|---|---|---|
| `sensor.toca` | booleano | toca objeto/tag |
| `sensor.toca_color` | booleano | toca color |
| `sensor.tecla` | booleano | tecla pulsada |
| `sensor.raton_x` | reporter | raton x |
| `sensor.raton_y` | reporter | raton y |
| `sensor.raton_boton` | booleano | raton pulsado |
| `sensor.distancia` | reporter | distancia a objeto |
| `sensor.cronometro` | reporter | cronometro |
| `sensor.variable` | reporter | variable |
| `sensor.propiedad` | reporter | propiedad de objeto |

### 6.7 Operadores

| id | Forma | Texto |
|---|---|---|
| `operador.sumar` | reporter | a + b |
| `operador.restar` | reporter | a - b |
| `operador.multiplicar` | reporter | a * b |
| `operador.dividir` | reporter | a / b |
| `operador.modulo` | reporter | a % b |
| `operador.aleatorio` | reporter | aleatorio a..b |
| `operador.menor` | booleano | a < b |
| `operador.igual` | booleano | a = b |
| `operador.mayor` | booleano | a > b |
| `operador.y` | booleano | a y b |
| `operador.o` | booleano | a o b |
| `operador.no` | booleano | no a |
| `operador.unir` | reporter | unir texto |
| `operador.longitud` | reporter | longitud texto |
| `operador.letra` | reporter | letra N |
| `operador.redondear` | reporter | redondear |
| `operador.abs` | reporter | abs |
| `operador.min` | reporter | min |
| `operador.max` | reporter | max |
| `operador.limitar` | reporter | limitar a..b |

### 6.8 Datos

| id | Forma | Texto |
|---|---|---|
| `datos.fijar` | pila | fijar variable |
| `datos.cambiar` | pila | cambiar variable |
| `datos.mostrar` | pila | mostrar variable |
| `datos.lista_agregar` | pila | agregar a lista |
| `datos.lista_insertar` | pila | insertar en lista |
| `datos.lista_borrar` | pila | borrar de lista |
| `datos.lista_fijar` | pila | fijar elemento |
| `datos.lista_elemento` | reporter | elemento de lista |
| `datos.lista_largo` | reporter | longitud de lista |
| `datos.lista_contiene` | booleano | lista contiene |
| `datos.lista_vacia` | pila | vaciar lista |
| `datos.variable` | reporter | valor de variable |

### 6.9 Mis bloques y escena

| id | Forma | Texto |
|---|---|---|
| `funcion.definir` | hat | definir funcion |
| `funcion.parametro` | reporter | parametro |
| `funcion.retornar` | cap | retornar valor |
| `escena.cambiar` | pila | cambiar a escena |
| `escena.reiniciar` | pila | reiniciar escena |
| `escena.actual` | reporter | escena actual |
| `hud.texto` | pila | dibujar texto |
| `hud.rect` | pila | dibujar rectangulo |
| `hud.barra` | pila | dibujar barra |
| `hud.boton` | reporter | boton pulsado |

### 6.10 Bloques de escape

| id | Forma | Texto |
|---|---|---|
| `codigo.lobo` | pila | codigo lobo (texto multilinea) |
| `codigo.expresion` | reporter | expresion lobo |

Estos bloques son la valvula de escape para usuarios avanzados y la base de
la migracion desde LOGIC2. El compilador los inserta tal cual.

---

## 7. Editor: vista Estudio

### 7.1 Nueva vista

Se anade `VISTA_ESTUDIO = 4` sin eliminar `VISTA_MODULOS = 2`.

- Menu **MODO**:
  - `Estudio de juego` -> `VISTA_ESTUDIO`.
  - `Gestor de logica (clasico)` -> `VISTA_MODULOS`.
- La pestana se llama `Estudio`.
- El boton play/pausa/stop sigue funcionando y usa el proyecto LOGIC3.

### 7.2 Distribucion a 1200x700

```text
y=0..34    barra de titulo (pestanas y controles)
y=34..70   barra de menus
y=70..580  area de trabajo
  x=0..200     lista de objetos y escenas
  x=200..940   lienzo de bloques
  x=940..1200  inspector (propiedades, trajes, variables)
y=580..700  consola y depurador
```

- El lienzo de bloques tiene zoom 50..200 % y paneo con boton central o
  `Espacio` + arrastre.
- La paleta se abre como panel flotante sobre el lienzo o como columna
  izquierda temporal; se cierra con `Escape`.
- La consola muestra errores, warnings, valores observados y perfil.

### 7.3 Interacciones del lienzo de bloques

- Arrastrar un bloque de la paleta al lienzo crea una pila nueva.
- Arrastrar un bloque cerca de otro lo acopla por arriba/abajo.
- Los bloques C aceptan un cuerpo; se abren y cierran con chevron.
- Los reporteros se arrastran a ranuras; si se sueltan fuera, se borran.
- Papelera en la esquina inferior derecha.
- Clic derecho: ayuda, duplicar, deshabilitar, borrar.
- Seleccion multiple con marco; Ctrl+C/V/X; Ctrl+Z/Y; Supr.
- `Ctrl+F` abre busqueda de bloques por nombre y descripcion.
- `F5` reproduce; `Shift+F5` detiene; `F6` pausa; `F7` paso.
- Los bloques en ejecucion se resaltan y muestran valores en vivo.

### 7.4 Inspector

- Pestanas: Propiedades, Trajes, Variables, Eventos.
- Propiedades: nombre, posicion inicial, tamano, capa, visible, tags.
- Trajes: lista con miniatura, importar PNG, pivote, previsualizacion.
- Variables: globales y del objeto, tipo, valor inicial, observar.
- Eventos: lista de scripts con su tipo y numero de bloques.

### 7.5 Accesibilidad y productividad

- Fuente 5x7 para todo el editor; fuente 2x para titulos.
- Todos los botones con atajo y descripcion en la barra de estado.
- Navegacion completa con teclado en paleta y lienzo.
- Mensajes de error con linea y bloque resaltado.
- Sin animaciones que consuman CPU en Windows 2000.

---

## 8. Fases de ejecucion

Cada fase es un commit. Las fases 23-30 son runtime y formato; 31-36 editor;
37-42 juego; 43-45 producto.

### Fase 23 - Congelar la base y crear la suite de conformidad

**Estado:** hecha (15-11-2026).

**Objetivo:** poder cambiar el motor sin miedo.

**Depende de:** nada.

**Tareas:**

1. Crear `pruebas/conformidad/` con programas lobo de referencia:
   aritmetica, precedencia, `si`, `mientras`, locales, errores, limites.
2. Crear `pruebas/ejecutar.ps1` que compile y ejecute `game.exe --captura`,
   compare BMP por hash y compare trazas `--traza`.
3. Documentar en `pruebas/README.md` como anadir un caso.
4. Crear `pruebas/regresion.ps1` con capturas del editor: pintar, tiles,
   mapa, menus, logica clasica, guia.
5. Anadir a `build.bat` un modo `build.bat prueba` que compile y ejecute la
   suite.
6. Medir la linea base: hashes de `LobosWolfos2000.exe`, `game.exe`, tiempos
   de arranque y memoria.

**Pruebas:** la suite pasa en C y, cuando exista, en Rust.

**Criterio de salida:** `pruebas/ejecutar.ps1` devuelve 0 y las capturas
coinciden con la linea base.

**Entregado:** `pruebas/conformidad` (12 casos), `pruebas/ejecutar.ps1`,
`pruebas/regresion.ps1` (8 casos), `pruebas/comun.ps1`,
`pruebas/linea_base.ps1`, `pruebas/linea_base.txt` y `pruebas/README.md`;
`build.bat prueba [conformidad|regresion|actualizar]`. La linea base se mide
sobre el commit `b99080f`: 12/12 y 8/8, con `-Repetir` determinista.

### Fase 24 - LOGIC3: modelo, lector, escritor y migrador

**Estado:** hecha (15-11-2026).

**Objetivo:** el editor guarda y abre el nuevo modelo sin cambiar aun el
runtime.

**Depende de:** 23.

**Tareas:**

1. Crear `src/logic3.h` y `src/logic3.c` con las estructuras del capitulo 4:
   `Proyecto3`, `Escena3`, `Objeto3`, `Evento3`, `Bloque3`, `Variable3`,
   `Lista3`, `Traje3`, `Funcion3`.
2. Implementar lector y escritor de texto con validacion y errores
   `linea N: motivo`.
3. Implementar `logic3_desde_logic2()` y `logic3_a_logic2()` para migracion
   y exportacion de emergencia.
4. Integrar en ARCHIVO: `Abrir proyecto .logic`, `Guardar proyecto .logic`,
   deteccion por cabecera.
5. Anadir flags `--abrir-logic3`, `--guardar-logic3`, `--convertir-logic2`.
6. Anadir campos a `--volcar`: `logic3=1 escenas=N objetos=N eventos=N
   bloques=N variables=N`.
7. Tests de ida y vuelta: guardar, cargar, volcar y comparar.

**Formato:** LOGIC3 v3.

**Criterio de salida:** un LOGIC2 de prueba se convierte, se guarda, se
recarga y conserva mapas, funciones y ajustes; el editor clasico sigue
abriendo LOGIC2 sin cambios.

**Entregado:** `src/logic3.h`, `src/logic3.c`, integracion en ARCHIVO y en
`--volcar`, los flags `--abrir-logic3`, `--guardar-logic3`,
`--convertir-logic2` y `--exportar-logic2`, y la suite `pruebas/logic3.ps1`
con `pruebas/logic3/base.logic`, `esperado.logic3` y `esperado-export.log`
(10 casos). Verificado con `build.bat prueba` (12/12, 10/10 y 8/8).

### Fase 25 - VM resumible y lobo 3 (Windows)

**Estado:** hecha (16-11-2026).

**Objetivo:** ejecutar scripts con estado persistente entre fotogramas.

**Depende de:** 24.

**Tareas:**

1. Redisenar `motor/lobo.h`: `LoboPrograma` con instrucciones ampliables,
   `LoboMaquina` con `pc`, `pila`, `pila_base`, `frames`, `espera`, `estado`.
2. Implementar `lobo_compilar()` y `lobo_paso()` (una instruccion o hasta
   evento) manteniendo `lobo_parsear()` para compatibilidad.
3. Anadir opcodes: `ESPERAR`, `LLAMAR`, `RETORNAR`, `CARGAR_GLOBAL`,
   `GUARDAR_GLOBAL`, `CARGAR_ENTIDAD`, `GUARDAR_ENTIDAD`, `LISTA_*`,
   `MENSAJE`, `DETENER`, `CLONAR`.
4. Implementar pila de llamadas y comprobacion de desbordamiento.
5. Mantener el modo actual "ejecutar programa entero" para LOGIC2.
6. Anadir `motor/vm_prueba.c` o flag `game.exe --vm-test` para ejecutar un
   programa paso a paso y volcar el estado.
7. Ampliar limites de instrucciones segun tabla 3.2 manteniendo LOGIC2 con
   sus limites antiguos.

**Pruebas:** los casos de conformidad pasan en modo clasico y en modo VM;
`esperar` no bloquea otros scripts; `detener` funciona.

**Criterio de salida:** `game.exe --logic3` ejecuta un proyecto con dos
scripts, uno con `esperar`, y la traza demuestra que no se bloquean.

**Entregado:** `motor/lobo.h` y `motor/lobo.c` rediseñados: `LoboPrograma`
con instrucciones ampliables (`insts`, `largo`, `capacidad`, `funciones`,
`textos`), `LoboMaquina` resumible (`pc`, `pila`, `np`, `frames`, `espera`,
`estado`) y dos modos de compilación, `LOBO_MODO_CLASICO` (topes antiguos y
sintaxis LOGIC2) y `LOBO_MODO_VM` (topes de la tabla 3.2 y sintaxis nueva).
`lobo_compilar()` y `lobo_paso()` son la base nueva; `lobo_parsear()` y
`lobo_ejecutar()` conservan el modo clásico de LOGIC2. Opcodes nuevos:
`ESPERAR`, `LLAMAR`, `RETORNAR`, `CARGAR_GLOBAL`, `GUARDAR_GLOBAL`,
`CARGAR_ENTIDAD`, `GUARDAR_ENTIDAD`, `LISTA_*`, `MENSAJE`, `DETENER` y
`CLONAR`, con pila de llamadas de 32 frames y comprobación de
desbordamiento. `motor/logic3_motor.c` y `motor/logic3_motor.h` son el lector
LOGIC3 mínimo del runtime (cabecera, mapa de la primera escena y bloques
`codigo.lobo` de los eventos `fotograma`). `motor/juego.c` añade `--logic3`
con memoria compartida (globales, listas, mensajes y clones), traza
`vm: N scripts` / `vm i:estado:pc:espera` y `--vm-test ruta.lobo` (con
`--tecla`), que ejecuta un programa suelto sin GDI+ ni ventanas. El editor
valida `codigo.lobo` y las expresiones `=...` con `lobo_compilar(...,
LOBO_MODO_VM, ...)` y arranca el `LoboPrograma` antes de compilar en
`funciones_codigo_validar()` (el buffer ya no es estático). La suite
`pruebas/vm.ps1` cubre 8 casos (`esperar`, `esperar_dos`, `funciones`,
`globales`, `listas`, `detener`, `mensajes`, `pila`) y la paridad
clásico/VM de los 6 scripts de conformidad con un solo elemento. El criterio
de salida se demuestra con `pruebas/vm/esperar_dos.logic3`: el script 0
espera tres fotogramas mientras el script 1 avanza cada fotograma.
`build.bat prueba` pasa las cuatro suites (12/12, 10/10, 9/9 y 8/8) y
`build.bat prueba vm` ejecuta solo la nueva.

### Fase 26 - Entidades, escenas y ciclo de vida

**Objetivo:** varios objetos con estado propio.

**Depende de:** 25.

**Tareas:**

1. Crear `motor/entidades.c` y `motor/entidades.h`.
2. Implementar `entidad_crear`, `entidad_borrar`, `entidad_buscar`,
   `entidad_por_tag`, `entidad_mover`, `entidad_cambiar_escena`.
3. Cargar escenas y objetos desde LOGIC3.
4. Implementar `mi` en lobo: `mi.x`, `mi.y`, `mi.vida`, `mi.visible`,
   `mi.traje`, `mi.variable("<nombre>")`.
5. Implementar `crear`, `borrar`, `clonar` y limite de entidades.
6. Render provisional: cada entidad se dibuja como rectangulo de color con
   su id (sin trajes aun), para validar sin assets.
7. Anadir `--entidades` a `--volcar` y `--traza` con id, x, y.

**Pruebas:** un proyecto con 50 entidades creadas por script; todas se
mueven y se borran sin fugas; el limite avisa.

**Criterio de salida:** dos entidades distintas tienen posiciones y variables
independientes en la misma captura.

**Entregado:** `motor/entidades.c` y `motor/entidades.h` con un array fijo de
256 entidades (`ENTIDADES_MAX`), ids monotonos que no se reutilizan, borrado
diferido al final del fotograma y un tope de 32 altas por fotograma con avisos
acumulados. La API cubre `entidad_crear`, `entidad_borrar`, `entidad_buscar`,
`entidad_por_tag`, `entidad_mover`, `entidad_cambiar_escena`, `entidad_clonar`,
lectura/escritura de campos y variables, orden por capa e id y contador de
altas. Cada entidad guarda posicion, tamano, capa, visibilidad, traje, vida y
hasta 64 variables. `motor/logic3_motor.c` lee escenas, objetos, variables y
trajes del LOGIC3, y `motor/juego.c` instancia los objetos de la primera escena
al arrancar (sin gastar el cupo de altas del fotograma 1). `motor/lobo.c` gana
`mi.x`, `mi.y`, `mi.vida`, `mi.visible`, `mi.traje`, `mi.variable("nombre")`,
`crear`, `clonar`, `borrar` y `mover`; las variables de entidad comparten tabla
con `@nombre` y se remapean a slots globales tras compilar. El render
provisional pinta cada entidad como un rectangulo de color con su id, tambien
sin mapa. `game.exe` añade `--entidades` (cabecera `entidades: N iniciales
avisos A`, lineas `ent <id> <x> <y>` y resumen con `creadas`) y `--volcar`
(estado final sin ventana, con las variables de cada entidad). La suite
`pruebas/entidades.ps1` cubre cuatro casos: `dos` (dos objetos con posicion y
variables independientes), `crear50` (51 altas, movimiento y borrado sin
fugas), `limite` (32 altas por fotograma y aviso al superarlas) y `clonar`. `build.bat prueba` pasa
las cinco suites y `build.bat prueba entidades` ejecuta solo la nueva; el
criterio de salida queda congelado en `pruebas/entidades/dos.bmp.sha256`.

### Fase 27 - Eventos, planificador y mensajes

**Objetivo:** que los scripts respondan a eventos, no solo al fotograma.

**Depende de:** 26.

**Tareas:**

1. Crear `motor/eventos.c` y `motor/eventos.h`.
2. Implementar la cola de eventos y el despacho del capitulo 5.2.
3. Implementar `inicio`, `fotograma`, `tecla`, `tecla_soltar`, `recibir`,
   `tocar`, `cada`, `clonar`.
4. Implementar `enviar("<mensaje>")`, `enviar_a("<objeto>", "<mensaje>")` y
   la cola de mensajes.
5. Implementar `detener_script`, `detener_todo` y reinicio de escena.
6. Anadir contador de eventos por fotograma y aviso de presupuesto.
7. Flags `--evento N`, `--mensaje "texto"` y volcado de eventos ejecutados.

**Pruebas:** un `al_recibir` no se ejecuta hasta que otro script envia el
mensaje; `al_pulsar` solo una vez por pulsacion; `cada 10` se ejecuta a los
10, 20, 30.

**Criterio de salida:** la traza muestra el orden exacto de eventos y el
juego no pierde mensajes.

**Estado:** hecha (16-11-2026).

**Entregado:** `motor/eventos.c` y `motor/eventos.h` con el planificador
completo: una `LoboMaquina` por script, despacho en el orden del capitulo 5.2,
cola de mensajes que respeta las esperas, presupuesto de 512 eventos por
fotograma con aviso y contadores para la traza. Cubre `inicio`, `fotograma`,
`tecla` (pulsar, mantener y soltar), `recibir`, `tocar` (AABB con espera de 15
fotogramas), `cada N` (dispara en N, 2N, 3N) y `clonar`; `escena` se dispara
al entrar en esa escena. `motor/logic3_motor.c` lee todos los tipos de evento y
sus parametros, y ordena los scripts por capa, objeto, evento y orden.
`motor/lobo.c` gana `enviar`, `enviar_a`, `detener_script`, `detener_todo` y
`reiniciar_escena` (con y sin parentesis) y expone `lobo_vm_evento` y
`lobo_vm_reanudar` para el planificador. `game.exe` anade `--evento N`,
`--mensaje "texto"` y `--eventos`; la traza escribe
`evento <fotograma> <tipo> obj <id> ent <id> script <n> ...`,
`eventos <f> total <n> cola <q>` por fotograma y el resumen
`eventos final: N avisos A ejecutados E`, y `--volcar` anade
`eventos N avisos A ejecutados E`. La suite `pruebas/eventos.ps1` cubre ocho
casos (`recibir`, `teclas`, `cada`, `orden`, `cola`, `tocar`, `reiniciar` y
`detener`) comparando traza y volcado (sin BMP congelado) y
`pruebas/vm/alias.*` cubre los alias en la VM. `build.bat prueba` pasa las seis
suites (12/12, 10/10, 10/10, 4/4, 8/8 y 8/8). El criterio de salida queda
congelado en `pruebas/eventos/`: `recibir` no corre hasta el mensaje (y el
mensaje externo llega en el fotograma 1), `teclas` cuenta una pulsacion por
flanco, `cada` dispara en 10, 20 y 30, `cola` conserva los mensajes
pospuestos y `detener` congela el despacho.

### Fase 28 - Variables, listas, funciones y expresiones

**Objetivo:** el lenguaje alcanza para logica de juego real.

**Depende de:** 27.

**Tareas:**

1. Anadir tipos `entero`, `booleano`, `texto` y `lista`.
2. Implementar ambitos: global, objeto, local, parametro.
3. Implementar funciones de usuario: `funcion nombre(a, b) ... retornar`.
4. Implementar `romper`, `continuar`, `repetir N`, `para cada`, `sino si`.
5. Implementar built-ins: `aleatorio`, `abs`, `min`, `max`, `limitar`,
   `redondear`, `longitud`, `unir`, `letra`.
6. Implementar listas: agregar, insertar, borrar, fijar, elemento, largo,
   contiene, vaciar, ordenar.
7. Serializar variables y listas en LOGIC3.
8. Documentar la gramatica en `README.md`.

**Pruebas:** una lista de 100 elementos se recorre y se ordena; una funcion
recursiva se corta a profundidad 32 sin colgar; los textos se comparan y se
cortan a 64 bytes.

**Criterio de salida:** un juego de prueba con inventario, vidas, puntuacion
y una funcion `danar(cantidad)` compila y corre.

**Estado:** hecha (18-11-2026).

**Entregado:** `motor/lobo.c` y `motor/lobo.h` con tipos `entero`, `booleano`,
`texto` y `lista`; ambitos global (`$`), de entidad (`@`), local y de parametro;
funciones de usuario con `retornar` y llamada directa `f(args)` ademas de
`llamar("f", args)`; `sino si`, `romper`, `continuar`, `repetir` y `para cada`;
y los built-ins `aleatorio`, `abs`, `min`, `max`, `limitar`, `redondear`,
`longitud`, `unir` y `letra`. Los textos son handles con marca alta
`0x40000000` y se cortan a 64 bytes; los runtime creados por `unir()` usan 32
huecos compartidos. `aleatorio` usa un LCG con semilla 12345 por maquina y
`redondear` es identidad porque no hay coma flotante. `src/logic3.c` y
`src/logic3.h` guardan variables tipadas y listas con elementos iniciales
(`variable global "n" entero v`, `lista global "n" 8 entero 3 1 2`), con
valores canonicos y un tokenizador de 288 tokens por linea. El lector del
runtime y `motor/juego.c` siembran la memoria de la VM con esos valores antes
de compilar los scripts, manteniendo el orden de declaracion. La recursión
infinita sigue cortandose a 32 frames sin colgar (`pruebas/vm/pila`). La suite
`pruebas/vm.ps1` anade ocho casos (`fase28_builtins`, `fase28_textos`,
`fase28_lista100`, `fase28_bucles`, `fase28_sino`, `fase28_funcion`,
`fase28_juego` y `fase28_datos`) y `pruebas/logic3.ps1` anade `tipos`.
`build.bat prueba` pasa las seis suites (12/12, 11/11, 18/18, 4/4, 8/8 y 8/8).
El criterio de salida queda congelado en `pruebas/vm/fase28_juego.esperado`
(x 19, y 203) y la gramatica completa esta en el README.

### Fase 29 - Colisiones, tiles con flags y fisica basica

**Objetivo:** que el mundo interactue.

**Depende de:** 28.

**Tareas:**

1. Versionar `.mav` a `version = 2` con flags por celda:
   `solido`, `hueco`, `trigger`, `one_way`, `tag`, `dano`.
2. Mantener lectura de `version = 1` (todas solidas).
3. Implementar colision entidad-entidad por AABB y por tag/grupo.
4. Implementar `toca`, `toca_tag`, `distancia`, `caja_de`.
5. Implementar fisica basica: `vel_x`, `vel_y`, `gravedad`, `rebote`,
   `friccion`, `en_suelo`.
6. Implementar tiles `one_way` y triggers sin bloquear.
7. Render de tiles huecos (no dibujar, no colisionar).
8. Tests de colision en mapa y entre entidades.

**Formato:** `.mav` v2, lector v1.

**Criterio de salida:** un nivel con suelo, plataformas one-way y una puerta
trigger funciona; el jugador no atraviesa paredes y si atraviesa triggers.

**Estado:** hecha (18-11-2026).

**Entregado:** `motor/colisiones.c` y `motor/colisiones.h` resuelven el barrido
de cajas por ejes, triggers, dano, plataformas one-way, gravedad, rebote y
friccion sin cambiar el layout de 12 bytes de `MotorCelda`. `motor/motor.h`
amplia `MotorMapa` con `version`, `flags`, `tags` y `dibujo_celdas`;
`motor/activo.c` lee el `.mav` v2 y mantiene el v1 como todo solido, con
valor por defecto solido si una celda v2 no trae claves de flag.
`motor/juego.c` conecta la fisica con los scripts, anade `--colisiones` a la
traza y usa la misma sonda en la vista previa del editor. `src/mapa.c`
guarda y lee los flags y oculta los huecos en rejilla y miniatura.
`motor/lobo.c` anade las variables de fisica en indices altos (1000..1005) y
los built-ins de VM `toca`, `toca_tag`, `distancia` y `caja_de`, con opcodes
al final del enum. `pruebas/colisiones.ps1` congela cuatro casos: mapa v2 con
pared, hueco, trigger y dano, aterrizaje en one-way, colisiones entre
entidades y fisica en VM. `build.bat prueba` pasa las siete suites (12/12,
11/11, 18/18, 4/4, 8/8, 4/4 y 8/8). El puerto Rust del kernel queda
pendiente para una fase posterior.

### Fase 30 - Render de objetos, trajes, animacion y capas

**Objetivo:** que el juego se vea como un juego.

**Depende de:** 29.

**Tareas:**

1. Carpeta `trajes/` junto al ejecutable; carga de PNG con GDI+ y cache.
2. Costume por objeto: frames, pivote, tamano, flip, rotacion 0/90/180/270.
3. Animacion por tira de frames o lista de trajes.
4. Orden por capa 0..15 y luego por id.
5. Camara: objetivo, limites de escena, zoom entero 1x/2x/4x.
6. Render de entidades en el kernel con el mismo criterio.
7. Culling por pantalla y presupuesto de blits.
8. Pruebas de pixeles: sprite en x,y exactos, flip, rotacion, capa.

**Criterio de salida:** una captura 640x480 muestra mapa, tres objetos con
trajes distintos, animacion y orden de capas correcto.

**Estado (18-11-2026): completada.** `motor/render.c` implementa la cache de
trajes, la animacion por tira y lista, pivote, flip, rotacion, zoom entero,
culling y presupuesto de blits. `motor/juego.c` pinta por capa e id y expone
camara y sondas `--pixel`. El editor escribe las extensiones sin alterar los
CRC antiguos y crea `trajes/` al arrancar. `pruebas/render.ps1` congela cuatro
casos (capas, animacion inicio/fin y zoom 2) con trazas y hashes BMP. El
kernel anade `Canvas::blit_ex` y camara equivalente para el sprite; no tiene
entidades LOGIC3, por lo que el espejo queda en la primitiva y la camara.
`build.bat prueba` pasa las ocho suites.

**Cambio visual previo a la fase 31:** barra superior compacta para CRT. La
franja de pestanas y la de menus pasan de 34+36 a 26+26 px, la fuente de
etiquetas de 15 a 12 px y los controles/iconos se redimensionan. El modo
`--captura` fuerza 1200x700 aunque el monitor este a 1024x768. Los ocho
patrones de regresion se regeneraron con `regresion.ps1 -Actualizar`.

### Fase 31 - Vista Estudio: escenario, objetos e inspector

**Objetivo:** la nueva interfaz de proyecto.

**Depende de:** 24, 30.

**Tareas:**

1. Anadir `VISTA_ESTUDIO` y la entrada de menu.
2. Crear `src/estudio.c` y `src/estudio_ui.c`.
3. Panel izquierdo: escenas y objetos con miniatura, visibilidad y bloqueo.
4. Panel derecho: propiedades, trajes, variables y eventos.
5. Botones: nuevo objeto, nuevo escenario, importar traje, borrar.
6. Barra superior del estudio: escenario, objetos, datos, sonidos, play.
7. Persistencia de seleccion y scroll por proyecto.
8. Flags `--estudio`, `--objeto N`, `--escena N`, volcado de seleccion.

**Pruebas por pixeles:** layout 200/740/260, clics en objetos cambian el
inspector, crear/borrar objeto actualiza el volcado.

**Criterio de salida:** se puede crear un proyecto con dos escenas y tres
objetos desde cero y guardarlo en LOGIC3.

**Estado:** cerrada. `VISTA_ESTUDIO` se anadio al final del enum para no
desplazar las vistas 0..3, y la entrada vive en la fila 6 de MODO sin tocar
`menu=1` ni `menu=9`. `src/estudio.c` muta el modelo LOGIC3 activo y
`src/estudio_ui.c` dibuja las tres columnas y las zonas clicables. Los eventos
por defecto de los objetos nuevos son `inicio` y `fotograma`. La seleccion, el
scroll y la pestana se recuerdan por ruta de proyecto en memoria y se volcan
solo con `vista=4`, de modo que los ocho patrones de regresion siguen intactos.
`pruebas/estudio.ps1` congela ocho casos con BMP, volcado y cinco sondas de
pixel de los limites 200/740/260; el caso `guardar` ademas compara el proyecto
LOGIC3 canonico con dos escenas y tres objetos. `build.bat prueba` pasa las
nueve suites.

### Fase 32 - Lienzo de bloques: paleta, arrastre y formas

**Objetivo:** editar scripts visualmente.

**Depende de:** 31.

**Tareas:**

1. Crear `src/bloques.c` (modelo y layout) y `src/bloques_ui.c` (dibujo).
2. Implementar formas: hat, pila, C, reporter, booleano, cap.
3. Paleta por categorias con busqueda y descripcion.
4. Arrastre desde paleta, acople por iman, insercion en ranuras.
5. Bloques C con cuerpo, chevron y anidamiento.
6. Papelera, clic derecho, duplicar, deshabilitar.
7. Zoom, paneo, seleccion multiple, copiar/pegar, deshacer/rehacer.
8. Atajos y navegacion por teclado.
9. Flags `--bloque N`, `--arrastre-bloque`, `--volcar bloques=N`.

**Pruebas por pixeles:** un bloque se arrastra de la paleta al lienzo; un
reportero se acopla a una ranura; un C anida dos hijos; deshacer revierte.

**Criterio de salida:** se puede construir el script del ejemplo del
capitulo 4.3 solo con el raton y el teclado, y guardarlo.

**Estado:** cerrada. `BLOQUES_TAB=4` coloca la pestana antes de Play y
`ESTUDIO_PESTANAS=6` deja `ARCHIVO=1` y `AYUDA=9` intactos. `src/bloques.c`
guarda el modelo, el historial, el catalogo y el layout; `src/bloques_ui.c`
dibuja la paleta, el lienzo y el inspector y gestiona arrastre, acople,
ranuras, anidamiento, teclado, rueda, duplicado y borrado. Cada mutacion
vuelca a `p->bloques` con el escritor LOGIC3 existente: no cambian el formato
ni el CRC. El vuelco no escribe `hijo` porque el lector actual exige tres
tokens y el escritor emite dos; el cuerpo se infiere al importar con `padre`
y el tipo C. La seleccion, el historial, el paneo y el zoom no se serializan.
El volcado anade campos `lb_` solo en la pestana Bloques, asi que los volcados
de regresion siguen intactos; sus hashes BMP se regeneraron por la barra de
seis pestanas. `pruebas/bloques.ps1` congela nueve casos con BMP, volcado,
cinco sondas de pixel y el LOGIC3 canonico del capitulo 4.3. `build.bat prueba`
pasa las once suites y 94 casos. Quedan para fases posteriores la busqueda en
la paleta, deshabilitar bloques y la seleccion multiple; el criterio de salida
de esta fase queda cubierto.

### Fase 33 - Compilador bloques -> lobo y visor de codigo

**Objetivo:** que los bloques se ejecuten.

**Depende de:** 32, 28.

**Tareas:**

1. Crear `src/compilador_bloques.c`.
2. Generar lobo 3 por evento, con etiquetas, saltos y llamadas.
3. Resolver expresiones de campos y reporteros.
4. Implementar `codigo.lobo` y `codigo.expresion`.
5. Visor de codigo de solo lectura por objeto/evento, con resaltado basico.
6. Errores del compilador con bloque y linea; resaltar el bloque.
7. Boton `Convertir a codigo` irreversible con aviso.
8. Tests: bloques -> lobo -> VM -> traza esperada.

**Criterio de salida:** el mismo proyecto produce la misma traza desde
bloques y desde el lobo generado escrito a mano.

**Estado (19-11-2026): completada.** `src/compilador_bloques.c` recorre las
raices del lienzo y genera un script lobo 3 por proyecto: `mover x` emite
`x = x + N`, `cambiar variable` emite `mi.variable("nombre") = ...`, `si` y
`repetir siempre` emiten `si`/`mientras` con su `fin`, y los reporteros se
resuelven dentro de la sentencia que los usa. Todo se valida con
`lobo_compilar(..., LOBO_MODO_VM, ...)`. Los sombreros de evento no emiten
codigo porque el evento ya vive en el LOGIC3; `codigo.expresion` queda para una
fase posterior. `NodoBloque.codigo` guarda el lobo y `bloques_volcar_modelo()`
lo vuelca a `Bloque3.codigo`, asi que el escritor emite `campo "codigo"` con
el mismo formato y CRC. El visor de solo lectura muestra numeros de linea y
resalta la linea con error; al volver al lienzo el bloque implicado aparece con
marco rojo. `Convertir a codigo` es irreversible y deja un unico bloque
`codigo.lobo`. Los campos `cb_*` y los botones del inspector solo aparecen
cuando el compilador interviene, para no alterar los volcados ni los hashes BMP
de la fase 32. `pruebas/compilador.ps1` congela ocho casos, incluido `paridad`,
que comprueba que bloques y lobo manual producen la misma traza (`18 16`).
`build.bat prueba` pasa las once suites y 94 casos.

### Fase 34 - Depurador: valores en vivo, pausa y breakpoints

**Objetivo:** encontrar fallos sin adivinar.

**Depende de:** 33.

**Tareas:**

1. Panel de variables observadas con valor actual.
2. Resaltado del bloque en ejecucion.
3. Pausa, continuar, paso a paso por bloque y por instruccion.
4. Breakpoints por bloque y por evento.
5. Consola de mensajes y errores en runtime.
6. Replay determinista: grabar entrada y reproducirla.
7. Comparar dos replays y marcar la primera divergencia.
8. Flags `--depurar`, `--paso`, `--break`, `--replay`.

**Criterio de salida:** se puede pausar un juego, ver `vidas=2`, avanzar un
bloque y comprobar que cambia a `1`.

**Estado (19-11-2026): completada.** `motor/depurador.c` guarda pausa, paso por
bloque o instruccion, breakpoints de bloque y evento, observadores, consola
circular, grabacion y reproduccion de replay y el comparador de trazas. Los
hooks de `motor/lobo.c` y `motor/eventos.c` solo cortan la ejecucion cuando
el depurador esta activo, y `motor/juego.c` anade los flags nuevos sin tocar
las opciones de las fases anteriores. `motor/panel_debug.c` dibuja sobre la vista
Play un panel con fuente bitmap 5x7, bloque resaltado, observados y consola,
solo con `--panel-debug`. El criterio se verifica en
`pruebas/depurador/esperado/paso_instruccion.debug`: `vidas=2` en la primera
parada y `vidas=1` despues de tres instrucciones. `pruebas/depurador.ps1`
congela nueve casos sin reutilizar las rutas de las once suites anteriores;
`build.bat prueba` pasa las doce suites y 103 casos.

### Fase 35 - Editor de variables, listas, trajes y sonidos

**Objetivo:** gestionar datos y assets sin tocar archivos.

**Depende de:** 31.

**Tareas:**

1. Editor de variables globales y por objeto con tipo, valor y observacion.
2. Editor de listas con celdas editables y previsualizacion.
3. Importador de trajes PNG con recorte, pivote y tira de frames.
4. Importador de sonidos WAV con reproduccion de prueba.
5. Validacion de tamanos y formatos.
6. Borrado con confirmacion y sustitucion de referencias.
7. Serializacion y migracion.

**Criterio de salida:** se crea una lista de 10 elementos, se edita, se
guarda y se recarga intacta.

**Estado (19-11-2026): completada.** La vista Estudio estrena las pestanas
reservadas **Datos** (indice 2) y **Sonidos** (indice 3), sin anadir una
septima pestana ni desplazar ARCHIVO=1, MODO=2 y AYUDA=9. `src/datos.c`
gestiona variables globales y de objeto con observacion, listas de hasta 256
elementos editables, importacion de trajes PNG con recorte, pivote y tira de
cuadros, y borrado con confirmacion y sustitucion de referencias. Las
observaciones y la tabla de sonidos se guardan en `<proyecto>.fase35`; las
variables, listas y trajes siguen usando los campos de LOGIC3, con el
escritor canonico y el CRC intactos. `src/sonido.c` valida WAV PCM mono de
8 o 16 bits a 11025 o 22050 Hz con tope de 256 KB y reproduce con
`PlaySoundA` cargada de `winmm.dll` sin anadir `-lwinmm`. Las entradas y
salidas nuevas van detras de flags nuevos (`--datos`, `--sonidos`,
`--datos-variable`, `--datos-lista`, `--datos-editar`, `--traje-importar`,
`--sonido-importar`, `--sonido-probar`, `--datos-migrar`, `--datos-borrar`,
`--confirmar`, `--datos-guardar` y `--volcar-datos`). El criterio se verifica
en `pruebas/datos/esperado/lista10.volcar`: la lista `inventario` conserva
sus 10 celdas y la celda 3 editada tras guardar y recargar. Nueva suite
`pruebas/datos.ps1` con siete casos que congelan volcado, hash BMP y tres
sondas de pixel; `build.bat prueba` pasa las trece suites y 110 casos.

### Fase 36 - Productividad del editor

**Objetivo:** que construir un juego grande sea rapido.

**Depende de:** 32-35.

**Tareas:**

1. Deshacer/rehacer global del estudio (no solo pintura).
2. Portapapeles de bloques y objetos.
3. Busqueda global de bloques, variables y eventos.
4. Multi-seleccion y edicion en lote.
5. Plantillas de script y snippets.
6. Ayuda contextual por bloque con ejemplo.
7. Modo principiante/avanzado.
8. Metricas del proyecto: bloques, memoria, entidades, coste.

**Criterio de salida:** duplicar un objeto con sus scripts y variables
cuesta menos de 10 segundos y no rompe ids.

**Estado (19-11-2026): completada.** `src/productividad.c` concentra todo el
estado nuevo fuera de `App` y del archivo LOGIC3. El deshacer/rehacer global
guarda copias profundas del modelo del estudio, con historial de 12 estados
que se descarta al cambiar de proyecto. El portapapeles copia un objeto con
sus trajes, variables y eventos; al pegar o duplicar se reasignan los ids de
evento y de los bloques que apuntan a ellos, y el objeto recibe un id libre.
La busqueda recorre variables, listas, eventos, bloques y trajes; la
duplicacion en lote admite hasta 32 copias. Hay cuatro snippets iniciales,
ayuda contextual por bloque, modo principiante/avanzado y metricas de
escenas, objetos, eventos, bloques, variables, listas, elementos y memoria.
Las entradas y salidas nuevas van detras de flags nuevos (`--productividad-marcar`,
`--productividad-deshacer`, `--productividad-rehacer`, `--productividad-copiar`,
`--productividad-pegar`, `--productividad-duplicar`, `--productividad-buscar`,
`--productividad-lote`, `--productividad-snippet`, `--productividad-ayuda`,
`--productividad-avanzado`, `--productividad-principiante` y
`--volcar-productividad`). El criterio se verifica guardando un proyecto con
un objeto y su variable `salud`: el duplicado conserva la variable y recibe
el id 2, sin colisiones. Nueva suite `pruebas/productividad.ps1` con siete
casos que congelan volcado, hash BMP y tres sondas de pixel; `build.bat
prueba` pasa las catorce suites y 117 casos.

### Fase 37 - Entrada extendida

**Objetivo:** teclado completo, raton y flancos.

**Depende de:** 30.

**Tareas:**

1. Mapa de 32 teclas configurables + flechas + WASD + espacio + enter.
2. Raton: x, y, botones, rueda, dentro/fuera.
3. Flancos: pulsada, soltada, mantenida.
4. `tecla_pulsada("a")` y `raton_boton(1)`.
5. Remapeo basico y multiples jugadores (fase posterior).
6. Paridad BIOS para teclado PS/2; raton PS/2 opcional.

**Criterio de salida:** un script responde a `a`, clic izquierdo y rueda de
forma determinista en captura.

### Fase 38 - HUD, texto, UI y dialogos

**Objetivo:** interfaz dentro del juego.

**Depende de:** 30, 28.

**Tareas:**

1. Fuente bitmap 5x7 y 2x para runtime.
2. Bloques `hud.texto`, `hud.rect`, `hud.barra`, `hud.imagen`, `hud.boton`.
3. Coordenadas de pantalla separadas de mundo.
4. Dialogos con cola de texto y avance con tecla.
5. Menu de pausa y game over de plantilla.
6. Render de HUD en Windows y BIOS.

**Criterio de salida:** una captura muestra vidas, puntuacion y un dialogo
de tres lineas sin tapar el mundo.

### Fase 39 - Audio

**Objetivo:** sonido y musica simple.

**Depende de:** 30.

**Tareas:**

1. Cargar WAV PCM 8/16 bits mono 11025/22050 Hz.
2. Mezclador de 8 canales en Windows con `waveOut`.
3. Bloques de sonido y volumen.
4. Musica en bucle con un canal reservado.
5. BIOS: pitidos por PIT como minimo; SB si se decide.
6. Limites de memoria y carga.
7. Pruebas de que el juego no se bloquea sin tarjeta de sonido.

**Criterio de salida:** `sonar("moneda")` suena una vez y `musica("tema")`
se repite; sin audio el juego sigue.

### Fase 40 - Guardado, records y persistencia

**Objetivo:** que el progreso sobreviva.

**Depende de:** 28.

**Tareas:**

1. Formato `saves/<proyecto>.sav` con claves y valores.
2. Escritura atomica y recuperacion ante corte.
3. Bloques `guardar`/`cargar`/`borrar_save`.
4. Records por proyecto.
5. Ajustes de usuario (volumen, pantalla completa, teclas).
6. BIOS: sector reservado o FS simple.
7. Herramienta del editor para ver/borrar saves.

**Criterio de salida:** cerrar y abrir el juego conserva vidas, puntos y
records; un save corrupto no impide arrancar.

### Fase 41 - Escenas multiples, transiciones, camara y efectos

**Objetivo:** juegos con varios niveles.

**Depende de:** 30, 27.

**Tareas:**

1. Carga de escena por demanda desde el blob.
2. Transiciones: fundido, barrido, inmediata.
3. `cambiar_escena`, `reiniciar_escena`, `escena_actual`.
4. Camara con objetivo, limites, zoom y suavizado entero.
5. Efectos: parpadeo, temblor, flash, fade.
6. Persistencia de variables entre escenas.
7. Paridad Windows/BIOS.

**Criterio de salida:** un juego de dos niveles cambia de escena sin recargar
el ejecutable y conserva la puntuacion.

### Fase 42 - Optimizacion, presupuestos y perfilador

**Objetivo:** que un juego grande corra a 60 FPS en hardware de 2000.

**Depende de:** 30-41.

**Tareas:**

1. Perfilador por evento/script: instrucciones, microsegundos, blits.
2. Presupuesto por fotograma y aviso en consola.
3. Cache de trajes y tiles; evitar recargas.
4. Culling y batching de blits.
5. Reducir copias de framebuffer.
6. Tests de estres: 200 entidades, 5000 bloques.
7. Informe de memoria del blob y del runtime.

**Criterio de salida:** un proyecto de estres mantiene 60 FPS o degrada con
aviso claro, sin bloquear la interfaz.

### Fase 43 - Paridad BIOS del subconjunto

**Objetivo:** que el mismo proyecto arranque desde la BIOS.

**Depende de:** 25-42.

**Tareas:**

1. Definir el subconjunto BIOS: que bloques y tipos soporta.
2. Portar la VM resumible a Rust `no_std` con los mismos opcodes.
3. Portar entidades, eventos y colisiones.
4. Portar render de trajes y HUD; audio minimo.
5. Ejecutar la suite de conformidad en QEMU.
6. Marcar en el editor los bloques no soportados en BIOS.
7. Avisar al compilar la imagen si el proyecto usa funciones fuera del
   subconjunto.

**Criterio de salida:** el proyecto de ejemplo corre igual en Windows y en
QEMU (posiciones y eventos), y el editor avisa de lo que no es portable.

### Fase 44 - Plantillas, ejemplos y tutoriales

**Objetivo:** que alguien sin experiencia tenga un juego en minutos.

**Depende de:** 31-42.

**Tareas:**

1. Plantillas: plataformas, shooter, puzzle, aventura, laberinto.
2. Ejemplos completos con assets libres y documentacion.
3. Tutorial interactivo dentro del estudio (extiende la guia de la fase 22).
4. Boton `Juego nuevo` con eleccion de plantilla.
5. Sistema de ayuda contextual por bloque.
6. Proyecto `primer_juego` migrado a LOGIC3 como ejemplo canonico.

**Criterio de salida:** `Juego nuevo > Plataformas` crea un proyecto jugable
con un nivel, un jugador, monedas y meta.

### Fase 45 - Exportacion, documentacion y release 2.0

**Objetivo:** cerrar el producto.

**Depende de:** todas.

**Tareas:**

1. Exportar proyecto `.wolfo` con LOGIC3 y assets.
2. Exportar `game.exe` autonomo y comprobar que no depende de carpetas.
3. Actualizar `README.md`, `PLAN.md`, `PLAN_JUEGOS_COMPLEJOS.md`.
4. Manual de bloques y referencia del lenguaje.
5. Release `v2.0-beta` con binarios, hashes y nota.
6. Verificacion final de regresion de pintura, tiles, mapas y menus.
7. Limpiar artefactos de prueba y confirmar que `recursos/ascii.png` sigue
   sin seguimiento.

**Criterio de salida:** release publicada, suite en verde y proyecto de
ejemplo jugable desde Windows y desde BIOS.

---

## 9. Plan de pruebas

### 9.1 Conformidad de lenguaje

- Un archivo `.lobo` + un `.txt` con la traza esperada por fotograma.
- Se ejecuta en `game.exe --captura --traza` y en el kernel/QEMU.
- Se comparan enteros, no imagenes, para aislar el lenguaje del render.
- Casos obligatorios: aritmetica, precedencia, comparaciones, logicos,
  `si`, `sino`, `mientras`, anidamiento 16, locales, funciones, listas,
  textos, errores de sintaxis y limites.

### 9.2 Determinismo

- `--replay entrada.txt` reproduce una partida sin reloj real.
- La misma entrada produce el mismo BMP y la misma traza.
- La aleatoriedad usa una semilla fijada por proyecto y se guarda en el
  replay.

### 9.3 Pixeles y clics

- Capturas a 1200x700 y 1920x1080.
- Coordenadas estables para menus, estudio, paleta y lienzo.
- Cada fase de UI define sus zonas y sus clics en la documentacion.
- Se comparan bounding boxes y colores clave, no la imagen completa.

### 9.4 Regresion del editor clasico

- Pintar: trazo, figuras, texto, seleccion, capas, deshacer.
- Tiles: pegado, trazo entre tiles, texto entre tiles.
- Mapa: pintar, borrar, zoom, guardar/cargar.
- Menus: ARCHIVO, EXPORTAR, MODO, AYUDA, INFO, COMPILE.
- Paquetes: `.wolfo` guardar/abrir, LOGIC2 cargar/guardar.

### 9.5 Rendimiento

- 200 entidades, 5000 bloques, 60 FPS en Windows XP virtual.
- Memoria del editor < 64 MB.
- Memoria del runtime < 32 MB.
- Blob BIOS < 1 MiB.

### 9.6 Matriz minima antes de cada commit

| Prueba | Comando |
|---|---|
| Build | `build.bat` |
| Conformidad | `pruebas\ejecutar.ps1` |
| Regresion editor | `pruebas\regresion.ps1` |
| Captura base | `LobosWolfos2000.exe --captura base.bmp --sin-ventanas` |
| Volcado | `--volcar base.txt` |
| Juego ejemplo | `game.exe --logica logic\primer_juego.logic --captura j.bmp` |

---

## 10. Protocolo de trabajo para la IA

### 10.1 Bucle por fase

1. Leer la fase y los archivos implicados.
2. Compilar y ejecutar la matriz minima; anotar hashes.
3. Implementar en pasos pequenos, compilando tras cada paso.
4. Anadir flags de prueba antes de la UI, para poder verificar sin raton.
5. Verificar por pixeles/clics y por volcado.
6. Actualizar documentacion y este plan (marcar la fase).
7. Commit con mensaje `Fase NN: titulo`.
8. Push y release.

### 10.2 Convenciones de codigo

- C sin acentos en identificadores ni comentarios.
- Funciones nuevas en archivos nuevos cuando sea posible; no engordar
  `ui.c` ni `app.c` mas alla de lo necesario.
- Prototipos en `src/app.h` o en el `.h` del modulo nuevo.
- Nada de variables globales nuevas sin prefijo del modulo.
- Nada de `malloc` en el runtime del juego; buffers fijos.
- Nada de coma flotante en el bucle; punto fijo si hace falta.
- Comentarios de fase y de intencion, no de linea obvia.
- Errores de usuario en espanol con acentos; errores internos sin acentos.

### 10.3 Que no debe hacer la IA

- No reescribir el editor de pintura para "limpiarlo".
- No cambiar el formato `.mav` sin fase y version.
- No eliminar `VISTA_MODULOS` ni el editor de codigo clasico.
- No romper LOGIC1/LOGIC2.
- No anadir dependencias.
- No commitear `recursos/ascii.png`.
- No hacer commits gigantes con varias fases.
- No publicar release sin binarios y hashes.

### 10.4 Cuando una fase es demasiado grande

Si una fase no cabe en un commit razonable, se parte en `NNa`, `NNb`, `NNc`
con el mismo criterio de salida. El plan permite subdivision, no saltarse
pruebas.

---

## 11. Riesgos y mitigaciones

| Riesgo | Impacto | Mitigacion |
|---|---|---|
| Explosion de alcance | No terminar nunca | Fases verticales, plantillas, subfases |
| Divergencia C/Rust | Mismo proyecto distinto en BIOS | Suite de conformidad compartida desde fase 23 |
| Complejidad del editor de bloques | Meses de UI | Modelo y layout separados de dibujo; flags de prueba |
| Romper proyectos antiguos | Perdida de confianza | LOGIC2 intacto, migrador, pruebas de ida y vuelta |
| Memoria fija insuficiente | Juegos grandes no caben | Presupuestos visibles y avisos antes de compilar |
| Save corrupto | Perdida de progreso | Escritura atomica, copia de seguridad, validacion |
| Formato LOGIC3 fragil | Errores dificiles | Lector estricto, `linea N`, CRC opcional, tests |
| Paridad BIOS incompleta | Promesa rota | Subconjunto explicito y aviso en editor |
| Audio bloqueante | Tirones | Hilo/mezclador simple, limites, sin streaming |

---

## 12. Backlog "mejor que Scratch"

Ideas para despues de la version 2.0, ordenadas por valor:

1. **Depuracion visual** con valores sobre los bloques y breakpoints
   condicionales.
2. **Replay y tests de juego** integrados en el editor.
3. **Perfilador** que senala el bloque mas caro.
4. **Tipos explicitos** y avisos de conversion.
5. **Funciones de usuario** con parametros y retorno (fase 28).
6. **Listas y textos** de primera clase.
7. **Multiples escenas** y guardado (fases 40-41).
8. **Tiles con flags** y one-way (fase 29).
9. **Camara configurable** (fase 41).
10. **HUD y dialogos** (fase 38).
11. **Audio simple** (fase 39).
12. **Exportacion a codigo** y visor de lobo (fase 33).
13. **Paleta buscable** y atajos (fase 36).
14. **Plantillas** y tutorial interactivo (fase 44).
15. **Versionado de proyecto** y diffs legibles (LOGIC3).
16. **Modo colaborativo offline** por archivos, sin nube.
17. **Accesibilidad**: fuente grande, contraste, navegacion por teclado.
18. **Internacionalizacion** de la interfaz (espanol/ingles) sin cambiar
    el lenguaje lobo.

---

## 13. Arranque rapido para la siguiente IA

Si solo hay tiempo para tres acciones, hacer en este orden:

1. Crear la suite de conformidad de la fase 23 y dejar la base verde.
2. Implementar `LOGIC3` con lector/escritor y migrador (fase 24).
3. Implementar la VM resumible con `esperar` y dos scripts (fase 25).

Sin esos tres cimientos, cualquier editor de bloques sera una capa bonita
sobre un motor que sigue sin poder hacer juegos complejos.

---

## Apendice A - Mapa de archivos previsto

Nuevos:

```text
src/logic3.c / logic3.h          modelo y serializacion LOGIC3
src/estudio.c / estudio_ui.c     vista Estudio
src/bloques.c / bloques.h        modelo y layout de bloques
src/bloques_ui.c                 dibujo e interaccion de bloques
src/compilador_bloques.c         bloques -> lobo 3
src/objetos.c                    objetos, escenas y trajes
src/datos_editor.c               variables y listas
src/depurador.c                  pausa, paso, breakpoints
src/replay.c                     grabacion y reproduccion
motor/vm.c / vm.h                VM resumible
motor/entidades.c / .h           entidades y escenas
motor/eventos.c / .h             planificador
motor/render_objetos.c           trajes, capas, camara
motor/hud.c                      texto y UI de juego
motor/audio.c                    mezclador Windows
motor/guardado.c                 saves
pruebas/conformidad/*            casos de lenguaje
pruebas/ejecutar.ps1             suite
pruebas/regresion.ps1            suite del editor
```

Modificados:

```text
src/app.h / app.c                integracion y flags
src/ui.c                         menus y vistas nuevas
src/nucleo.c                     arranque y captura
src/compilar.c                   blob con LOGIC3 y assets
src/juego_ventana.c              preview con LOGIC3
motor/lobo.c / lobo.h            lobo 3 y VM
motor/juego.c                    runtime nuevo
motor/activo.c                   carga LOGIC3
loboswolfos/kernel/src/*.rs      paridad BIOS
build.bat / build.ps1            fuentes nuevas
README.md / PLAN.md              documentacion
```

## Apendice B - Comandos de referencia

```bat
build.bat
LobosWolfos2000.exe --captura out.bmp --sin-ventanas --volcar out.txt
LobosWolfos2000.exe --estudio --objeto 1 --volcar out.txt
LobosWolfos2000.exe --abrir-logic3 proyecto.logic --captura out.bmp
game.exe --logica logic\primer_juego.logic --captura j.bmp --fotogramas 60
game.exe --logic3 proyecto.logic --escena 1 --traza t.txt --replay r.txt
pruebas\ejecutar.ps1
pruebas\regresion.ps1
```

## Apendice C - Criterios de aceptacion globales

Un proyecto se considera "juego complejo" cuando puede demostrar, en una
captura y una traza:

1. Dos escenas con mapas distintos y transicion.
2. Tres tipos de objeto con trajes, animacion y variables propias.
3. Eventos de inicio, teclado, colision, mensaje y temporizador.
4. Variables globales, lista de inventario y funcion de usuario.
5. Colision con tiles solidos, one-way y trigger.
6. HUD con vidas, puntos y dialogo.
7. Guardado y carga de progreso.
8. Sonido de efecto y musica.
9. Replay determinista que reproduce la misma traza.
10. Compilacion a `game.exe` autonomo y a imagen BIOS dentro de 1 MiB.

---

Fin del plan maestro.
