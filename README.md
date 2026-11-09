# Texture Paint 2000

Editor de texturas ligero para **Windows 2000 y Windows XP** de 32 bits. Está
escrito en C con Win32/GDI puro y un búfer propio de 32 bits; GDI+ solo se usa
para cargar y guardar PNG.

## Características

- Lienzo con zoom (hasta 1000 %), capas y deshacer/rehacer.
- Pincel, lápiz, aerógrafo, cubo de relleno y selector de color.
- Figuras: línea, rectángulo, rectángulo redondeado, elipse, polígono y curva
  de Bézier.
- Selección rectangular y de forma libre, con arrastre y redimensionado.
- Herramienta de texto con varias fuentes y tamaños.
- Patrón de textura repetido, visor de mosaico y guardado de tiles en PNG.
- Pestañas de modo: Pintar, Tiles, Mapa y patrón comparten el mismo documento;
  cada pestaña conserva su vista, zoom y desplazamiento.
- Gestor de escenas: paneles izquierdo, derecho e inferior (los laterales del
  mismo ancho), un botón rectangular arrastrable y un lienzo desplazable con
  rejilla. Los elementos se pueden mover y se acoplan entre sí por sus lados con
  efecto imán. El panel izquierdo lista los elementos del lienzo y las escenas
  creadas; cada escena guarda su configuración de rectángulos.
- Apertura y exportación de PNG/BMP y mapas `.mav`.
- Interfaz flotante de opciones, panel de capas y menús contextuales.

## Compilar

Requiere MinGW-w64 para 32 bits (i686) con `gcc` y `windres`.

```bat
build.bat [ruta\a\mingw32\bin]
```

Si no se indica la ruta, se busca la variable `MINGW32` o la ruta usada durante
el desarrollo. El resultado es `TexturePaint2000.exe`.

También se puede compilar desde PowerShell:

```powershell
.\build.ps1
```

## Uso

Ejecuta `TexturePaint2000.exe`. Los tiles que guardes se almacenan en la carpeta
`tiles` junto al ejecutable.

Las pestañas representan modos de trabajo sobre el mismo documento. El botón
`+` (o `Ctrl+T`) duplica el modo actual en otra pestaña; después puedes cambiar
esa pestaña a Tiles, Mapa o patrón sin tocar el documento. `Nuevo` y `Abrir`
sustituyen el documento compartido, no crean pestañas nuevas.

El menú **MODO** incluye **Modo pintar**, que cierra el visor de patrón, sale del
modo tiles o del editor de mapas y vuelve al lienzo normal.

Al pulsar **GESTOR DE ESCENAS** en la barra de menús se abre un espacio de
trabajo con los paneles izquierdo, derecho e inferior (los dos laterales del
mismo ancho), manteniendo las pestañas y la barra de menús. Pulsa de nuevo la
etiqueta para volver a Pintar.

El panel izquierdo muestra, de arriba abajo, la lista de elementos del lienzo
(`Elemento 1`, `Elemento 2`…) y la lista de escenas creadas. Los elementos
unidos entre sí por el efecto imán aparecen indentados bajo el elemento del
grupo al que pertenecen; la flecha del grupo permite plegar o desplegar sus
hijos en la lista. Cada escena es una configuración de rectángulos del lienzo:
al pulsar una escena se carga en el lienzo, y el botón **Nueva escena** guarda
la actual y crea otra vacía. Si hay más de seis elementos, la rueda sobre la
lista los desplaza.

Dentro del gestor, el panel derecho contiene un botón rectangular sin nombre:
arrástralo hasta el lienzo para insertar un elemento. Los elementos ya colocados
también se pueden arrastrar. Sus lados tienen efecto imán: al acercarlos a otro
elemento se acoplan y se alinean, mostrando una guía azul. El lienzo no sirve
para pintar; es un entorno desplazable con rejilla. Arrastra sobre una zona vacía
con el botón izquierdo o central para mover la vista, o usa la rueda para
desplazarte en vertical (`Ctrl+rueda` en horizontal).

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

- `src/` — código fuente C y recursos de la interfaz.
- `recursos/` — iconos e imágenes de las herramientas.
- `tiles/` — carpeta donde se guardan los mosaicos.
- `build.bat` / `build.ps1` — scripts de compilación.

## Licencia

Uso interno de Green Blue Studios.
