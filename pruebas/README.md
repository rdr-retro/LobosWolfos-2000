# Pruebas de LobosWolfos 2000

Tres suites: el lenguaje lobo y el motor (fase 23), el proyecto LOGIC3
(fase 24) y la interfaz del editor (fase 23). Congelan el comportamiento
antes de cambiar el runtime en las fases siguientes.

Todo es texto: los patrones de las capturas son hashes SHA-256 y
volcados de estado, no imagenes. `recursos/ascii.png` no se versiona y
las pruebas no lo necesitan.

## Comandos

```bat
build.bat prueba              compila y pasa las tres suites
build.bat prueba conformidad  solo la suite del lenguaje
build.bat prueba logic3       solo el proyecto LOGIC3
build.bat prueba regresion    solo la regresion del editor
build.bat prueba actualizar   reescribe los patrones con el estado actual
```

Tambien se pueden llamar por separado:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\ejecutar.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\ejecutar.ps1 -Repetir
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\logic3.ps1
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
