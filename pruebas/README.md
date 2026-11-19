# Pruebas de LobosWolfos 2000

Diez suites: el lenguaje lobo y el motor (fase 23), el proyecto LOGIC3
(fase 24), la VM resumible y lobo 3 (fase 25), entidades, escenas y ciclo de
vida (fase 26), eventos, planificador y mensajes (fase 27), variables, listas
y funciones (fase 28), colisiones, flags y fisica (fase 29), render de
trajes, animacion, capas y camara (fase 30), la vista Estudio (fase 31), el
lienzo de bloques (fase 32) y la interfaz del editor (fase 23). Congelan el
comportamiento antes de cambiar el runtime en las fases siguientes.

Todo es texto: los patrones de las capturas son hashes SHA-256 y
volcados de estado, no imagenes. `recursos/ascii.png` no se versiona y
las pruebas no lo necesitan.

## Comandos

```bat
build.bat prueba              compila y pasa las diez suites
build.bat prueba conformidad  solo la suite del lenguaje
build.bat prueba logic3       solo el proyecto LOGIC3
build.bat prueba vm           solo la VM y lobo 3
build.bat prueba entidades    solo entidades, escenas y ciclo de vida
build.bat prueba eventos      solo eventos, planificador y mensajes
build.bat prueba colisiones   solo colisiones, flags y fisica
build.bat prueba render       solo trajes, animacion, capas y camara
build.bat prueba estudio      solo la vista Estudio
build.bat prueba bloques      solo el lienzo de bloques
build.bat prueba regresion    solo la regresion del editor
build.bat prueba actualizar   reescribe los patrones con el estado actual
```

Tambien se pueden llamar por separado:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\ejecutar.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\ejecutar.ps1 -Repetir
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\logic3.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\vm.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\entidades.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\eventos.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\colisiones.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\render.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\render.ps1 -Repetir
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\estudio.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\bloques.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\regresion.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\linea_base.ps1
```

`-Filtro` acepta un patron como `bucle*` o `menu*`. `-Motor ruta.exe`
permite pasar otro binario (por ejemplo un futuro ejecutable del kernel)
a la suite de conformidad.

## Conformidad del lenguaje (`ejecutar.ps1`)

Los casos viven en `pruebas\conformidad`:

| Archivo | Obligatorio | Que hace |
|---|---|---|
| `<nombre>.lobo` | no | programa que se incrusta en un `.logic` temporal con un solo elemento de codigo |
| `<nombre>.args` | no | argumentos extra de `game.exe` (`--fotogramas`, `--tecla`, `--mapa`...) |
| `<nombre>.esperado` | si | traza exacta que debe escribir `--traza` |
| `<nombre>.bmp.sha256` | no | hash del BMP final, cuando el dibujo importa |

Sin `.lobo` el caso solo usa sus `.args` (asi se prueba `render_mapa`,
que corre un `.mav` sin scripts). Cada caso se ejecuta en el raiz del
repositorio porque el motor busca `maps\` y `tiles\` desde el directorio
de trabajo.

Anadir un caso:

1. Crear `<nombre>.lobo` con un programa pequeno y determinista.
2. Crear `<nombre>.args` con el numero exacto de fotogramas.
3. Ejecutar `pruebas\ejecutar.ps1 -Filtro <nombre>* -Actualizar`.
4. Revisar a mano `<nombre>.esperado`: un patron solo vale si se entiende.
5. Volver a ejecutar sin `-Actualizar` y con `-Repetir`.

La traza del motor tiene este formato:

```text
lobo: N scripts
<fotograma> <x> <y>
```

Si un script no compila, en su lugar aparecen las lineas de error y
`lobo: 0 scripts`. `x` e `y` son el canal de salida de los casos: el
programa escribe sus resultados ahi y la traza los congela.

`-Repetir` ejecuta cada caso dos veces y comprueba que la traza y el BMP
no cambian: es la prueba de determinismo del runtime.

## Proyecto LOGIC3 (`logic3.ps1`)

La suite de la fase 24 comprueba el modelo nuevo sin tocar el runtime:
`game.exe` sigue leyendo LOGIC2.

| Archivo | Que es |
|---|---|
| `logic3\base.logic` | fixture LOGIC2 con un mapa, un codigo y `PROYECTO` |
| `logic3\esperado.logic3` | LOGIC3 canonico que debe producir la conversion |
| `logic3\tipos_entrada.logic3` | entrada no canonica con variables tipadas y listas con valores |
| `logic3\tipos.logic3` | LOGIC3 canonico con `007` convertido a `7` y textos escapados |
| `logic3\esperado-export.log` | LOGIC2 canonico de la exportacion de emergencia |

Casos:

| Caso | Cubre |
|---|---|
| `convertir` | LOGIC2 -> LOGIC3, volcado (`logic3=1`, contadores) y archivo canonico |
| `ida_vuelta` | abrir, guardar y comparar byte a byte con `esperado.logic3` |
| `exportar` | LOGIC3 -> LOGIC2 de emergencia y archivo canonico |
| `clasico` | el LOGIC2 exportado y el fixture abren sin activar LOGIC3 |
| `detectar` | `--abrir-logica` detecta la cabecera LOGIC3 y `--guardar-logica` lo conserva |
| `crc` | un CRC32 que no coincide se rechaza |
| `crc_texto` | un CRC32 con formato invalido se rechaza |
| `version` | una version futura se rechaza |
| `sin_fin` | un archivo sin `FIN` se rechaza |
| `extension` | secciones desconocidas se ignoran y el CRC es opcional |
| `tipos` | ida y vuelta tipada: valores canonizados y listas con elementos (fase 28) |

Cada caso ejecuta el editor en una carpeta temporal con `--captura` y
`--volcar`, y comprueba los campos `logic3=`, `escenas=`, `objetos=`,
`eventos=`, `bloques=` y `variables=` del volcado. Para regenerar los
patrones tras un cambio intencionado:

```bat
build.bat prueba logic3
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\logic3.ps1 -Actualizar
git diff pruebas\logic3
```

## VM resumible y lobo 3 (`vm.ps1`)

La suite de las fases 25 y 28 congela la VM resumible, el compilador en modo
VM y las variables, listas, funciones y textos. Los casos viven en `pruebas\vm`:

| Archivo | Que es |
|---|---|
| `<nombre>.lobo` | programa suelto; se ejecuta con `game.exe --vm-test` durante 8 fotogramas |
| `<nombre>.esperado` | traza exacta del caso |
| `<nombre>.logic3` | proyecto LOGIC3; se ejecuta con `--logic3 --captura` durante 8 fotogramas |
| `<nombre>.bmp.sha256` | hash del BMP del caso `.logic3` |

Casos:

| Caso | Cubre |
|---|---|
| `esperar` | `esperar 3` deja la VM en espera y reanuda en el fotograma 4 |
| `esperar_dos` | criterio de salida: dos scripts, uno espera y el otro avanza cada fotograma |
| `funciones` | `llamar`/`retornar`, parametros y restauracion de locales |
| `globales` | `$global` persiste entre fotogramas |
| `listas` | `lista`, `anadir`, `ordenar`, `largo`, `elemento` |
| `detener` | `detener` corta el script y el evento vuelve a empezar |
| `mensajes` | `mensaje` y `clonar` anotan en la memoria compartida |
| `alias` | `enviar`, `enviar_a`, `reiniciar_escena` y `detener_*` con y sin parentesis (fase 27) |
| `pila` | recursion sin fin: el tope de 32 frames corta con error sin colgar el fotograma |
| `fase28_builtins` | `abs`, `min`, `max`, `limitar`, `redondear` y `aleatorio` determinista |
| `fase28_textos` | textos por valor, comparacion `==`/`!=` y corte a 64 bytes |
| `fase28_lista100` | lista de 100 elementos: se ordena y se recorre con `para cada` |
| `fase28_bucles` | `romper`, `continuar`, `repetir` y `para cada` |
| `fase28_sino` | cadena de `sino si` cerrada con un solo `fin` |
| `fase28_funcion` | llamada directa y alias `llamar`, con dos parametros |
| `fase28_juego` | criterio de salida: inventario, vidas, puntuacion y `danar(cantidad)` |
| `fase28_datos` | un `.logic3` con semillas de variables y lista alimenta la VM |

La traza de `--vm-test` es una linea por fotograma:

```text
vm <fotograma> estado <estado> pc <pc> np <pila> espera <espera> x <x> y <y> msg <n> clones <n> err <n>
```

`estado` 1 es esperando, 2 terminado. La traza de `--logic3` anade la
cabecera `vm: N scripts`, una linea `vm: script i bloque b objeto o evento e`
por script y, por fotograma, `vm i:estado:pc:espera` y la salida `x y`.

Ademas, la suite comprueba la **paridad clasico/VM**: los scripts de
conformidad que en clasico dan `lobo: 1 scripts` se ejecutan tambien con
`--vm-test` y deben producir la misma secuencia de `x` e `y`. Se excluye
`limites_pasos`, cuyo presupuesto por fotograma es distinto a proposito.

Para anadir un caso: crear el `.lobo` o `.logic3`, ejecutar
`pruebas\vm.ps1 -Filtro <nombre>* -Actualizar`, revisar el `.esperado` a
mano y volver a pasarla sin `-Actualizar` y con `-Repetir`.

## Entidades, escenas y ciclo de vida (`entidades.ps1`)

La suite de la fase 26 congela las entidades y sus limites. Los casos viven
en `pruebas\entidades` y se ejecutan con
`--logic3 --entidades --captura --traza --volcar`:

| Archivo | Que es |
|---|---|
| `<nombre>.logic3` | proyecto con escenas, objetos y scripts de evento |
| `<nombre>.args` | argumentos extra; si no hay `--fotogramas` se usan 8 |
| `<nombre>.esperado` | traza exacta (incluye `ent <id> <x> <y>` por fotograma) |
| `<nombre>.volcado` | estado final con las variables de cada entidad |
| `<nombre>.bmp.sha256` | hash del BMP final del caso |

Casos:

| Caso | Cubre |
|---|---|
| `dos` | criterio de salida: dos entidades con posicion y variables independientes |
| `crear50` | 51 altas, movimiento por lista y borrado sin fugas ni avisos |
| `limite` | el tope de 32 altas por fotograma avisa y no rompe el bucle |
| `clonar` | `clonar` copia la instancia actual y el original se puede borrar |

Los scripts de evento `fotograma` se reinician al terminar, asi que los
fixtures usan una local `hecho` para ejecutar su cuerpo una sola vez.

La traza con `--entidades` tiene este formato:

```text
entidades: N iniciales avisos A
<fotograma> <x> <y>
ent <id> <x> <y>
...
entidades final: N avisos A creadas C
```

El volcado (`--volcar`) escribe `fotogramas`, `lobo`, `entidades N avisos A
creadas C` y, con `--entidades`, una linea `entidad <id> ...` seguida de
`  var "<nombre>" <valor>` por variable. `creadas` cuenta todas las altas
desde el arranque, incluidas las instancias iniciales de la escena.

Para anadir un caso: crear el `.logic3` y su `.args`, ejecutar
`pruebas\entidades.ps1 -Filtro <nombre>* -Actualizar`, revisar el
`.esperado`, el `.volcado` y el BMP a mano, y volver a pasarla sin
`-Actualizar` y con `-Repetir`.

## Eventos, planificador y mensajes (`eventos.ps1`)

La suite de la fase 27 congela el orden de despacho y la cola de mensajes.
Los casos viven en `pruebas\eventos` y se ejecutan con
`--logic3 --eventos --entidades --captura --traza --volcar`:

| Archivo | Que es |
|---|---|
| `<nombre>.logic3` | proyecto con varios objetos y sus eventos |
| `<nombre>.args` | argumentos extra (`--fotogramas`, `--tecla`, `--evento`, `--mensaje`) |
| `<nombre>.esperado` | traza exacta, con las lineas `evento` y `eventos` |
| `<nombre>.volcado` | estado final con las variables de cada entidad |

El BMP no se congela: `--captura` solo evita que el motor abra ventana.

Casos:

| Caso | Cubre |
|---|---|
| `recibir` | criterio de salida: `recibir` no corre hasta que llega el mensaje; `--mensaje externo` se entrega en el fotograma 1 |
| `teclas` | `pulsar`, `mantener` y `soltar`: una pulsacion por flanco con la tecla mantenida |
| `cada` | `cada 10` dispara exactamente en 10, 20 y 30 |
| `orden` | traza del orden inicio, fotograma, tecla y cada en los mismos fotogramas |
| `cola` | varios mensajes encolados: el receptor que espera los recibe despues, sin perder ninguno |
| `tocar` | AABB entre dos entidades con espera de 15 fotogramas |
| `reiniciar` | `reiniciar_escena()` reinstancia las entidades y repite `inicio` |
| `detener` | `detener_script` corta un script; `detener_todo` congela el despacho |

Los ids de evento son unicos en todo el proyecto (el editor los valida
globalmente), y las variables de entidad se remapean a slots globales: los
fixtures declaran las mismas variables en el mismo orden.

La traza con `--eventos` tiene este formato:

```text
evento <fotograma> <tipo> obj <id> ent <id> script <n> ["<mensaje>"]
...
eventos <fotograma> total <n> cola <q>
...
eventos final: N avisos A ejecutados E
```

La linea `eventos <fotograma> total <n> cola <q>` sale en todos los
fotogramas, tambien cuando `detener todo` congela el despacho o un mensaje
queda en espera: es la prueba de que la cola no se pierde. El volcado anade
`eventos N avisos A ejecutados E`.

Para anadir un caso: crear el `.logic3` y su `.args`, ejecutar
`pruebas\eventos.ps1 -Filtro <nombre>* -Actualizar`, revisar la traza y el
volcado a mano, y volver a pasarla sin `-Actualizar` y con `-Repetir`.

## Colisiones, flags y fisica (`colisiones.ps1`)

Los casos viven en `pruebas\colisiones`. Un `.args` manda los argumentos de
`game.exe` (normalmente un `.mav` v2 o un `.logic3`) y la suite anade
`--captura` y `--traza`; si ademas existe un `.lobo`, se ejecuta con
`--vm-test`. Se compara `<nombre>.esperado` y, cuando existe,
`<nombre>.bmp.sha256`.

| Caso | Que comprueba |
|---|---|
| `mapa_v2` | pared que no se atraviesa, hueco que no bloquea, trigger y dano senalados en la traza |
| `fisica_oneway` | caida con gravedad y aterrizaje estable sobre una plataforma one-way |
| `entidades_toca` | `toca`, `toca_tag`, `distancia` y `caja_de` entre dos entidades |
| `fisica_vm` | vel_x, gravedad, rebote y friccion en la VM de `--vm-test` |

`game.exe --colisiones` anade a la traza una linea `colision suelo N dano N
triggers ...`; es la unica suite que usa esa flag, asi que las demas trazas
quedan intactas.

## Render de trajes, animacion, capas y camara (`render.ps1`)

Los casos viven en `pruebas\render`. Cada `.args` abre un `.logic3` con
`--render` y sonda pixeles con `--pixel`; la suite anade `--captura` y
`--traza`, y compara `<nombre>.esperado` y `<nombre>.bmp.sha256`. El mapa
`render.mav`, los tiles y los PNG de `trajes/` son parte del patron.

| Caso | Que comprueba |
|---|---|
| `capas` | tres objetos en capas 0, 5 y 15; el de arriba tapa al de abajo en el mismo punto |
| `animacion_inicio` | tres trajes distintos en el primer cuadro de la tira |
| `animacion_fin` | avance de cuadros por tiempo y color del cuadro 3 despues de ocho fotogramas |
| `zoom2` | camara con zoom 2, mapa ampliado y sprite con pivote en x,y |

La traza congela el orden por capa e id, el cuadro elegido, la camara y el
presupuesto de blits. Las sondas comprueban colores exactos: `FFE0C040` en la
capa 0, `FF3060D0` en la capa 5, `FFC040C0` en el cuadro 3 y `FFE04040` al
zoom 2. `-Repetir` ejecuta cada caso dos veces para detectar render no
determinista. Para anadir un caso: crear el `.logic3`, sus PNG y su `.args`,
ejecutar `pruebas\render.ps1 -Filtro <nombre>* -Actualizar`, revisar la traza
y el hash a mano, y volver a pasarla sin `-Actualizar` y con `-Repetir`.

## Vista Estudio (`estudio.ps1`)

Suite de la fase 31. Congela el BMP, el volcado y cinco sondas de pixel por
caso. Las sondas caen en `100,680` (panel izquierdo), `199,400` (separador),
`500,401` (centro), `940,400` (separador) y `1100,680` (panel derecho), para
verificar el layout 200/740/260. Las rutas temporales del volcado se
sustituyen por `<CASO>`, asi que los patrones no dependen del equipo.

Los patrones viven en `pruebas\estudio\esperado`:

| Archivo | Contenido |
|---|---|
| `<nombre>.volcar` | volcado de `--volcar` normalizado |
| `<nombre>.bmp.sha256` | hash SHA-256 de la captura |
| `<nombre>.pixeles` | las cinco sondas `x,y=RRGGBB` |
| `guardar.logic3` | proyecto canonico creado desde el estudio |
| `menu_indices.volcar` | indices de ARCHIVO, MODO y AYUDA |

| Caso | Flags | Cubre |
|---|---|---|
| `entrada` | `--estudio` | paneles, zoom 10 y proyecto LOGIC3 vacio |
| `crear` | `--clics 52,85;52,85;148,85;148,85;148,85` | dos escenas, tres objetos y seis eventos |
| `borrar` | crear y cuatro clics en `148,111` | vuelta a una escena sin objetos |
| `seleccion_id` | `--abrir-logic3 ... --objeto 2` | seleccion por id y su inspector |
| `seleccion_clic` | `--objeto 2 --clics 52,134` | el clic cambia la fila seleccionada |
| `guardar` | crear, guardar y reabrir | proyecto LOGIC3 con 2 escenas y 3 objetos |
| `menu_modo` | `--menu 2` | desplegable MODO con la fila nueva |
| `menu_indices` | `--menu 1/2/9` | indices usados por las pruebas antiguas |

Para regenerar o revisar:

```bat
build.bat prueba estudio
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\estudio.ps1 -Actualizar
git diff pruebas\estudio\esperado
```

El volcado anade campos de estudio solo cuando `vista=4`, por lo que los ocho
volcados de `regresion.ps1` siguen sin cambios. Sus hashes BMP se regeneraron
al insertar la pestana Bloques en la barra del estudio (fase 32).

## Lienzo de bloques (`bloques.ps1`)

Suite de la fase 32. Congela el BMP, el volcado y cinco sondas de pixel por
caso. Las sondas caen en `100,170` (categoria), `100,212` (paleta), `300,160`
(barra del estudio), `500,300` (lienzo) y `1100,400` (inspector). Las rutas
temporales se sustituyen por `<CASO>`, asi que los patrones no dependen del
equipo. La captura fuerza 1200x700 con `--captura`.

Los patrones viven en `pruebas\bloques\esperado`:

| Archivo | Contenido |
|---|---|
| `<nombre>.volcar` | volcado de `--volcar` normalizado |
| `<nombre>.bmp.sha256` | hash SHA-256 de la captura |
| `<nombre>.pixeles` | las cinco sondas `x,y=RRGGBB` |
| `guardar.logic3` | proyecto canonico del script del capitulo 4.3 |
| `menu_indices.volcar` | indices de ARCHIVO, MODO y AYUDA |

| Caso | Flags | Cubre |
|---|---|---|
| `entrada` | `--bloques` | pestana 4, lienzo vacio y `lb_bloques=0` |
| `arrastrar` | `--arrastre-bloque 2:100,212;400,300` | creacion por arrastre y seleccion |
| `seleccion_id` | dos bloques y `--bloque 1000` | seleccion por id al final de los gestos |
| `ranura` | dos gestos con `4:` | reportero acoplado a la entrada `dx` |
| `anidar` | un C y dos hijos | `lb_cuerpo=2` dentro del mismo padre |
| `deshacer` | crear y `--deshacer-bloque` | vuelta a cero bloques |
| `teclado` | clic en categoria y `Intro` | creacion sin arrastre |
| `guardar` | cinco bloques y `--guardar-logic3` | guardado, relectura y `lb_cuerpo=1` |
| `menu_indices` | `--menu 1/2/9` | indices antiguos y ausencia de campos `lb_` |

El vuelco no emite lineas `hijo`: el lector actual de LOGIC3 exige tres tokens
y el escritor genera dos. El cuerpo se infiere al reabrir con `padre` y el
tipo C. El campo `bloques=N` de la primera linea sigue siendo el conteo
LOGIC3; los campos nuevos llevan prefijo `lb_` y solo aparecen en la pestana
Bloques.

Para regenerar o revisar:

```bat
build.bat prueba bloques
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\bloques.ps1 -Actualizar
git diff pruebas\bloques\esperado
```

## Regresion del editor (`regresion.ps1`)

Casos actuales, a 1200x700 y con `--sin-ventanas`. El modo `--captura` fuerza
ese tamano aunque el monitor este a 1024x768, para que los patrones no
dependan de la resolucion del CRT. Tras el cambio visual previo a la fase 31
la franja superior ocupa `y=0..51` (pestanas `0..25`, menus `26..51`):

| Caso | Flags | Cubre |
|---|---|---|
| `pintar` | `--sin-ventanas` | vista de pintura, barra, lienzo y zoom inicial |
| `tiles` | `--tiles` | modo tiles y su botonera |
| `mapa` | `--vista mapa --mapa-ruta maps\mapa.mav` | editor de mapas y miniatura |
| `menu_archivo` | `--menu 1` | desplegable ARCHIVO |
| `menu_ayuda` | `--menu 9` | desplegable AYUDA (fase 22) |
| `gestor` | `--gestor --modulos 500,250;500,400` | gestor de logica clasico con dos elementos |
| `guia` | `--guia` | guia modal, paso 0 |
| `ejemplo` | `--ejemplo` | juego de ejemplo y sus cinco archivos |

`ejemplo` se ejecuta sobre una copia del editor en una carpeta temporal:
las carpetas del proyecto viven junto al ejecutable y no se deben tocar
las del repositorio. En el volcado, la ruta de esa carpeta se normaliza
a `<carpeta>` para que el patron no dependa del equipo.

Cada caso compara el codigo de salida, el volcado de `--volcar` y el
hash del BMP. Para actualizar un cambio de interfaz intencionado:

```bat
build.bat prueba actualizar
git diff pruebas\regresion\esperado
```

Hay que revisar el diff: si el cambio no era intencionado, es una
regresion.

## Linea base

`linea_base.txt` guarda fecha, commit, tamanos, hashes, tiempos de
arranque y memoria pico de los dos ejecutables. Se regenera con
`pruebas\linea_base.ps1`. Los hashes son informativos: cambian en cada
compilacion por la marca de tiempo del PE.
