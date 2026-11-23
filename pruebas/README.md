# Pruebas de LobosWolfos 2000

Veinticuatro suites: el lenguaje lobo y el motor (fase 23), el proyecto LOGIC3
(fase 24), la VM resumible y lobo 3 (fase 25), entidades, escenas y ciclo de
vida (fase 26), eventos, planificador y mensajes (fase 27), variables, listas
y funciones (fase 28), colisiones, flags y fisica (fase 29), render de
trajes, animacion, capas y camara (fase 30), la vista Estudio (fase 31), el
lienzo de bloques (fase 32), el compilador bloques -> lobo (fase 33), el
depurador y el replay (fase 34), la interfaz del editor (fase 23), datos,
productividad, entrada, HUD, audio, guardado, escenas, perfil, plantillas y
exportacion 2.0 (fases 35 a 45), el modelo de programa (fase 46) y su lienzo
visible (fase 47). Congelan el comportamiento antes de cambiar el runtime en
las fases siguientes. La suite 23 anade cinco casos propios; no toca los
patrones de las 22 anteriores. La suite 24 anade cuatro casos:
`opciones_nuevo`, `mapa_solo`, `legacy` y `ui_programa`. El ultimo actualiza los
hashes de `regresion/gestor`, `regresion/ejemplo` y `plantillas/canonico` por
los botones nuevos de la barra, sin tocar sus volcados.

Todo es texto: los patrones de las capturas son hashes SHA-256 y
volcados de estado, no imagenes. La suite de exportacion lee blobs `LGB1`
y compara entradas, hashes y posiciones; sus patrones viven en
`pruebas\exportacion\esperado`. `recursos/ascii.png` no se versiona y
las pruebas no lo necesitan.

## Comandos

```bat
build.bat prueba              compila y pasa las 24 suites
build.bat prueba conformidad  solo la suite del lenguaje
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
build.bat prueba datos        solo datos, trajes y sonidos
build.bat prueba productividad solo atajos y productividad
build.bat prueba entrada      solo entrada extendida
build.bat prueba hud          solo HUD, texto, UI y dialogos
build.bat prueba audio        solo sonido y musica simple
build.bat prueba save         solo guardado y persistencia
build.bat prueba escenas      solo escenas multiples y camara
build.bat prueba perfil       solo perfilador y estres
build.bat prueba plantillas   solo plantillas, ejemplos y tutorial
build.bat prueba exportacion  solo paquetes, game.exe autonomo y BIOS/QEMU
build.bat prueba programa     solo el modelo y el lienzo de programa
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
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\compilador.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\depurador.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\exportacion.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\programa.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\regresion.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\linea_base.ps1
```

`-Filtro` acepta un patron como `bucle*` o `menu*`. `-Motor ruta.exe`
permite pasar otro binario (por ejemplo un futuro ejecutable del kernel)
a la suite de conformidad.

## Modelo de programa (`programa.ps1`)

`pruebas\programa` contiene dos LOGIC3 sin CRC: `mapa_solo.logic3` lleva
`sistema.opciones` con `jugador_implicito=0`, y `legacy.logic3` no lleva la
pieza. Ambos apuntan al mapa `pruebas/colisiones/fisica.mav` y no tienen
objetos. La suite comprueba quince cosas: el volcado del `ProjectModel` minimo,
la conversion del `.log` de ejemplo, la validacion de recursos del actor,
el diagnostico JSON de recurso faltante, que el mismo grafo aparece en el
lienzo y en el inspector, que el LOGIC3
generado conserva la traza del juego, que preview y motor capturan el mismo
fotograma, que un proyecto
nuevo guarda la
pieza de opciones, que `mapa_solo` mantiene `lobo 16 16` con `--tecla
derecha`, que `legacy` mueve el lobo y dibuja un BMP distinto, que
`ui_programa` construye un lienzo con Escenario, Actor y Teclado enlazados,
que `entrada_gestor` abre el constructor con `--gestor` sin `--programa`, que
`elementos_logic2` proyecta los dos elementos del minijuego en piezas reales
del lienzo, que `menu_tras_piezas` recarga ese LOGIC2 por la ruta del menú
después de un LOGIC3 con piezas, y que `arrastre_gestor` crea una pieza desde
esa entrada normal.

El caso `modelo_minimo` usa `--volcar-modelo` para construir en memoria un
grafo con proyecto, dos recursos, escena, actor, instancia, control y dos
conexiones con puertos. No lee `.log`, no dibuja y no escribe LOGIC3. Su unico
patron es `modelo_minimo.volcar`, que fija IDs, nombres, origen, estado de UI,
referencias, puertos y un diagnostico `sin_errores`.

El caso `modelo_desde_log` copia `pruebas/modelo/juego_ia.log` al directorio
del caso y usa `--leer-modelo` para convertirlo. Comprueba que los cinco nodos
conservan su ID y su linea, que la cabecera `proyecto` toma el primer ID libre,
que la ruta `tiles/jugador.png` se construye con la carpeta del nodo, que los
tres `conecta` se vuelven conexiones con puertos y que el diagnostico es
`sin errores`. Despues rompe un enlace y comprueba que no se escribe volcado y
que el diagnostico senala `linea 13`.

El caso `modelo_recursos` crea un `tiles/jugador.png` vacio en el directorio
del caso y comprueba que `pm_validar_recursos()` no informa fallos. Luego
borra el PNG y espera `linea 11: recurso 5 sin archivo: tiles/jugador.png`.
Por ultimo cambia `conecta 5 -> 2` por `conecta 5 -> 3` y espera
`linea 8: actor 2 sin recurso conectado`. La validacion es generica: no nombra
el proyecto de ejemplo y solo exige que todo recurso exista y que todo actor
tenga un recurso entrante.

El caso `diagnostico_json` usa `--diagnostico-json` sobre el mismo `.log` y
comprueba que, sin el PNG, el JSON informa `recurso_sin_archivo` con la linea
11, la entidad 5 y la ruta `tiles/jugador.png`. Despues crea el archivo y
exige `estado=ok`, `total=0` y la lista de errores vacia. Los dos patrones
son `diagnostico_json.json` y `diagnostico_json_ok.json`.

El caso `modelo_gestor` abre `pruebas/modelo/juego_ia.log` con
`--gestor-modelo` y comprueba que el lienzo y el inspector reciben el mismo
grafo: seis piezas reales (cinco nodos y la cabecera `proyecto`), los tres
`conecta` convertidos en `enlace` y los dos campos de cada entidad. El BMP
fija el color de cada tarjeta y el titulo `INSPECTOR`, que sigue visible; el
resumen del grafo se dibuja solo cuando hay un modelo activo, asi que los
casos antiguos no cambian. Sus patrones son `modelo_gestor.volcar-programa`,
`modelo_gestor.bmp.sha256` y `modelo_gestor.pixeles`.

El caso `modelo_logic3` usa `--generar-logic3` para convertir el mismo `.log`
en LOGIC3 canonico. El patron `modelo_logic3.logic3` fija la cabecera y los
cinco bloques `sistema.pieza` con sus campos y enlaces. Despues ejecuta el
motor dos veces con las mismas teclas y fotogramas, una sobre el `.log` y otra
sobre el archivo generado, y exige trazas identicas ademas del mismo hash
del BMP. Asi se comprueba que el adaptador no cambia el resultado del juego
ni el escritor canonico.

El caso `preview_captura` cierra el ciclo con el mismo LOGIC3: primero lo
genera con `--generar-logic3`, despues abre esa ruta en la vista previa con
`--abrir-logic3 --captura-juego --fotogramas 6` y ejecuta `game.exe` con
`--logic3 --captura --fotogramas 6`. El BMP de la interfaz que exige
`--captura` se descarta; se compara el BMP de juego de las dos vias y su hash
con `preview_captura.bmp.sha256`. Asi queda verificada la igualdad que ya
documentaba `README.md` sin depender de un ejecutable recompilado a mano.

El caso `entrada_gestor` fija el volcado `programa_modo=1 programa_piezas=0`,
el hash del BMP y 15 sondas de pixel; la paleta aparece sin resaltar la primera
pieza y la sonda `921,68=303030` comprueba que el título `INSPECTOR` no queda
tapado por la barra de menús. El caso `arrastre_gestor` abre `legacy.logic3`,
arrastra una pieza desde la paleta, guarda y comprueba que aparecen `sistema.opciones` con
`programa=1` y `sistema.pieza` con `sistema.escena`. Sus patrones son
`entrada_gestor.volcar`, `entrada_gestor.volcar-programa`,
`entrada_gestor.bmp.sha256`, `entrada_gestor.pixeles`,
`arrastre_gestor.logic3`, `arrastre_gestor.volcar-programa`,
`arrastre_gestor.bmp.sha256` y `arrastre_gestor.pixeles`.

El caso `elementos_logic2` abre `ejemplos\minijuego.logic` desde el gestor y
comprueba que el lienzo tiene dos piezas reales: `sistema.recurso.mapa` con
`mapa.mav` y `sistema.funcion` con `Laberinto simple`. Sus patrones son
`elementos_logic2.volcar`, `elementos_logic2.volcar-programa`,
`elementos_logic2.bmp.sha256` y `elementos_logic2.pixeles`; este ultimo fija
once sondas: dos del panel derecho, dos bordes, dos colores de pieza, dos
avisos de enlace, dos tipos y el lienzo vacío junto a las piezas.

El caso `menu_tras_piezas` usa `--abrir-logic3` con `ui_programa.logic3` y
después `--abrir-logica-menu minijuego.logic`, la misma función que
**ARCHIVO > Abrir lógica .logic** sin el cuadro de archivo. Comprueba que
las tres piezas previas se sustituyen por las dos piezas proyectadas, que el
LOGIC2 queda activo y que el mapa del LOGIC3 anterior no queda en el lienzo.
Sus patrones son `menu_tras_piezas.*` y reutilizan las once sondas del caso
anterior.

El caso `ui_programa` entra con `--programa`, arrastra tres piezas y un
enlace con `--arrastre-programa`, y guarda con `--guardar-logic3`. Sus patrones
son `ui_programa.logic3`, `ui_programa.volcar`, `ui_programa.volcar-programa`,
`ui_programa.bmp.sha256` y `ui_programa.pixeles`. Este ultimo fija 14 sondas
`x,y=RRGGBB` de paleta, tarjetas, lienzo vacio, seleccion, insignia roja,
puertos de entrada y salida, enlace y conmutador de la barra. Todos los
patrones viven en `pruebas\programa\esperado`.

## Conformidad del lenguaje (`ejecutar.ps1`)

Los casos viven en `pruebas\conformidad`:

| Archivo | Obligatorio | Que hace |
|---|---|---|
| `<nombre>.lobo` | no | programa que se incrusta en un `.logic` temporal con un solo elemento de codigo |
| `<nombre>.args` | no | argumentos extra de `game.exe` (`--fotogramas`, `--tecla`, `--mapa`...) |
| `<nombre>.esperado` | si | traza exacta que debe escribir `--traza` |
| `<nombre>.bmp.sha256` | no | hash del BMP final, cuando el dibujo importa |

Sin `.lobo` el caso solo usa sus `.args` (asi se prueba `render_mapa`,
que corre un `.mav` sin scripts, y `ejemplo_minijuego`, que abre
`ejemplos\minijuego.logic` y comprueba ocho fotogramas bajando). Cada caso se
ejecuta en el raiz del repositorio porque el motor busca `maps\` y `tiles\`
desde el directorio de trabajo.

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

## Compilador bloques -> lobo (`compilador.ps1`)

Suite de la fase 33. Congela el lobo generado, los campos `cb_*` del volcado,
cuatro sondas de pixel y la traza de la VM. Los patrones viven en
`pruebas\compilador\esperado`:

| Archivo | Contenido |
|---|---|
| `<nombre>.lobo` | codigo generado por `--volcar-codigo` |
| `<nombre>.pixeles` | cuatro sondas `x,y=RRGGBB` del visor o del error |
| `<nombre>.logic3` | proyecto convertido con `campo "codigo"` |
| `<nombre>.traza` | traza de `game.exe --logic3` |
| `menu_indices.volcar` | indices de ARCHIVO, MODO y AYUDA |

| Caso | Flags | Cubre |
|---|---|---|
| `generar_mover` | arrastre `2:` y `--compilar-bloques` | `x = x + 2` y `cb_lineas=1` |
| `generar_control` | `1:` + `2:` anidados | `mientras 1` / `fin` y `lb_cuerpo=1` |
| `generar_variable` | arrastre `3:` y `--campo-bloque` | `mi.variable("...")` |
| `error_bloque` | `5:` con texto invalido | error mapeado a `cb_bloque` y `cb_linea` |
| `visor` | `--visor-codigo` | `cb_visor=1` y pixeles del visor |
| `convertir` | `--convertir-bloques` y guardado | `cb_convertido=1` y LOGIC3 |
| `paridad` | bloques y lobo manual | misma traza desde ambos proyectos |
| `menu_indices` | `--menu 1/2/9` | indices antiguos y sin campos `cb_` |

`--campo-bloque ID:TEXTO` reemplaza el valor por defecto del campo 0, para
poder escribir texto sin depender del orden de clics y arrastres. Los campos
`cb_*` solo aparecen en el volcado cuando hay visor, conversion, codigo
generado o error, de modo que los volcados de la fase 32 no cambian. El codigo
no se genera en cada mutacion del lienzo: solo al compilar, abrir el visor o
convertir, para preservar los hashes BMP de la fase 32.

Para regenerar o revisar:

```bat
build.bat prueba compilador
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\compilador.ps1 -Actualizar
git diff pruebas\compilador\esperado
```

## Depurador y replay (`depurador.ps1`)

Suite de la fase 34. Congela los puntos de parada, la consola, el replay y
el resultado del comparador. Sus fixtures y patrones viven en
`pruebas\depurador\` y `pruebas\depurador\esperado\`:

| Archivo | Contenido |
|---|---|
| `pausa.logic3` | dos bloques (`inicio` y `fotograma`) con `vidas` |
| `paso.logic3` | un solo bloque de `fotograma` con tres instrucciones |
| `teclas.logic3` | proyecto determinista usado al grabar y reproducir |
| `error.logic3` | programa con un error en runtime |
| `ordenes_pausa.txt` / `ordenes_error.txt` | guiones de ordenes del depurador |
| `<nombre>.debug` | `pausa ...`, `obs ...` y `consola ...` de cada parada |
| `<nombre>.replay` / `<nombre>.traza` | entrada grabada y traza reproducida |
| `<nombre>.txt` | `sin divergencias` o `divergencia N lineaA lineaB` |
| `panel.pixeles` | seis sondas `x,y=RRGGBB` del panel visual |
| `menu_indices.volcar` | indices de ARCHIVO, MODO y AYUDA |

| Caso | Cubre |
|---|---|
| `pausa` | `vidas=2` en la primera parada y `vidas=1` tras dos pasos |
| `paso_instruccion` | tres instrucciones llevan de `vidas=2` a `vidas=1` |
| `break_bloque` | `--break B:2` detiene en el bloque 2 |
| `break_evento` | `--break E:2` detiene en el evento 2 |
| `error_runtime` | `consola error en pc 1` en la VM |
| `replay` | la traza reproducida es identica a la grabada |
| `comparar` | sin divergencias y primera divergencia en una sola linea |
| `panel` | pixeles del panel solo con `--panel-debug` |
| `menu_indices` | ARCHIVO=1, MODO=2 y AYUDA=9 siguen en su sitio |

Toda salida nueva va detras de los flags del depurador, asi que los patrones
de las once suites anteriores no cambian. Para regenerar o revisar:

```bat
build.bat prueba depurador
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\depurador.ps1 -Actualizar
git diff pruebas\depurador\esperado
```

## Exportacion 2.0 (`exportacion.ps1`)

Suite de la fase 45. Congela el paquete `.wolfo` del ejemplo canonico, su
restauracion, el paquete legado sin `--wolfo-logic3`, el `game.exe`
autonomo y la paridad Windows/QEMU de la imagen BIOS. Sus patrones viven en
`pruebas\exportacion\esperado` y no se comparten con ninguna suite anterior.

| Archivo | Que es |
|---|---|
| `paquete.entradas.esperado` | entradas `proyecto.logic` y `proyecto.logic3` con sus largos |
| `legado.entradas.esperado` | entrada unica `proyecto.logic` del modo heredado |
| `jugable.posiciones.esperado` | ocho posiciones `N 16 16` de Windows y BIOS |

| Caso | Cubre |
|---|---|
| `paquete` | pie `LGB1`, LOGIC3 nativo con CRC valido y LOGIC2 intacto |
| `restauracion` | abrir el paquete, guardar el LOGIC3 y comparar su hash con la entrada |
| `legado` | sin `--wolfo-logic3` el paquete sigue siendo el de la fase 15 |
| `game` | LOGIC3 incrustado, 4 scripts VM, sin carpetas externas y posiciones fijas |
| `bios` | imagen de 32 MiB, entrada `vm.lbin`, avisos vacios y paridad con QEMU |

El escritor canonicaliza `variable "monedas" 0 0` a
`variable "monedas" entero 0`, asi que se compara la entrada exportada
contra el archivo restaurado, no contra los bytes del ejemplo. Para
regenerar o revisar:

```bat
build.bat prueba exportacion
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\exportacion.ps1 -Actualizar
git diff pruebas\exportacion\esperado
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
