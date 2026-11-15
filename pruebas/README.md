# Pruebas de LobosWolfos 2000

Suite de la fase 23: congela el lenguaje lobo y la interfaz del editor
antes de cambiar el motor (fases 24 y siguientes).

Todo es texto: los patrones de las capturas son hashes SHA-256 y
volcados de estado, no imagenes. `recursos/ascii.png` no se versiona y
las pruebas no lo necesitan.

## Comandos

```bat
build.bat prueba              compila y pasa las dos suites
build.bat prueba conformidad  solo la suite del lenguaje
build.bat prueba regresion    solo la regresion del editor
build.bat prueba actualizar   reescribe los patrones con el estado actual
```

Tambien se pueden llamar por separado:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\ejecutar.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File pruebas\ejecutar.ps1 -Repetir
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
