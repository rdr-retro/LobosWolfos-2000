# Pruebas de LobosWolfos 2000

Seis suites: el lenguaje lobo y el motor (fase 23), el proyecto LOGIC3
(fase 24), la VM resumible y lobo 3 (fase 25), entidades, escenas y ciclo de
vida (fase 26), eventos, planificador y mensajes (fase 27) y la interfaz del
editor (fase 23). Congelan el comportamiento antes de cambiar el runtime en
las fases siguientes.

Todo es texto: los patrones de las capturas son hashes SHA-256 y
volcados de estado, no imagenes. `recursos/ascii.png` no se versiona y
las pruebas no lo necesitan.

## Comandos

```bat
build.bat prueba              compila y pasa las seis suites
build.bat prueba conformidad  solo la suite del lenguaje
build.bat prueba logic3       solo el proyecto LOGIC3
build.bat prueba vm           solo la VM y lobo 3
build.bat prueba entidades    solo entidades, escenas y ciclo de vida
build.bat prueba eventos      solo eventos, planificador y mensajes
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

La suite de la fase 25 congela la VM resumible y el compilador en modo VM.
Los casos viven en `pruebas\vm`:

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

## Regresion del editor (`regresion.ps1`)

Casos actuales, a 1200x700 y con `--sin-ventanas`:

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
