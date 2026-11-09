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
- Gestor de módulos: los paneles izquierdo, derecho e inferior se muestran
  vacíos sobre el fondo habitual de la aplicación.
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

Al pulsar **GESTOR DE MODULOS** en la barra de menús se abre un espacio de
trabajo con los paneles izquierdo, derecho e inferior vacíos, manteniendo las
pestañas y la barra de menús. Pulsa de nuevo la etiqueta para volver a Pintar.

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
