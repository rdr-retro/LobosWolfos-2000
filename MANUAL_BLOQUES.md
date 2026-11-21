# Manual de bloques

El lienzo de bloques vive en la vista Estudio, pestana **Bloques**. Se abre con
**MODO > Vista Estudio**, con la propia pestana o con `--bloques`. Cada mutacion
se vuelca al proyecto LOGIC3 con el escritor canonico: el formato y el CRC no
cambian. Los bloques no se guardan como nodos sueltos, sino como bloques
`Bloque3` con `padre`, `slot`, `orden`, `tipo` y campos.

## Paleta y formas

La paleta tiene seis categorias: **EVENTOS**, **CONTROL**, **MOVIMIENTO**,
**DATOS**, **OPERADORES** y **CODIGO**. Las formas son:

| Forma | Uso |
|---|---|
| hat | Inicia un script; solo tiene cuerpo |
| pila | Sentencia que se encadena dentro de un cuerpo |
| C | Sentencia con cuerpo y, si procede, una ranura de condicion |
| reportero | Devuelve un valor; se conecta a una ranura |
| booleano | Reportero de condicion |
| cap | Contiene codigo lobo escrito a mano |

El liquido de fondo distingue la categoria. Los bloques se arrastran con el
boton izquierdo; al soltarlos sobre una ranura o el cuerpo de un bloque C se
acoplan por iman. El boton derecho duplica. La rueda desplaza el lienzo.

## Atajos del lienzo

| Tecla | Accion |
|---|---|
| `Tab` | Alterna el foco entre paleta y lienzo |
| Flechas | Recorren categorias, definiciones o bloques |
| `Intro` | Anade la definicion activa |
| `Supr` | Borra la seleccion |
| `Ctrl+Z` / `Ctrl+Y` | Deshacer y rehacer |
| `Ctrl+C` / `Ctrl+V` | Copiar y pegar |
| Al escribir | Edita el primer campo del bloque seleccionado |
| `F3` | Abre la ayuda contextual del bloque seleccionado |
| `F2` | Abre **VER CODIGO** (visor del lobo generado) |
| `Esc` | Cierra el visor o cancela la accion |

## Catalogo

Los 27 bloques estan en este orden en `src/bloques.c`. El orden es parte del
contrato de las pruebas: no se desplaza ninguna fila al anadir otros nuevos.

| Id | Categoria | Forma | Campos | Codigo lobo que genera |
|---|---|---|---|---|
| `eventos.inicio` | EVENTOS | hat | - | Ninguno: el evento ya vive en LOGIC3 |
| `eventos.tecla` | EVENTOS | hat | tecla | Ninguno: el evento `tecla` ya vive en LOGIC3 |
| `eventos.recibir` | EVENTOS | hat | mensaje | Ninguno: el evento `recibir` ya vive en LOGIC3 |
| `control.repetir_siempre` | CONTROL | C | cuerpo | `mientras 1` ... `fin` |
| `control.si` | CONTROL | C | cuerpo, condicion | `si <cond>` ... `fin` |
| `movimiento.mover_x` | MOVIMIENTO | pila | dx | `x = x + N` o `x = x + <expresion>` |
| `datos.cambiar_variable` | DATOS | pila | variable, delta | `mi.variable("nombre") = mi.variable("nombre") + N` |
| `operadores.suma` | OPERADORES | reportero | a, b | `a + b` |
| `operadores.mayor` | OPERADORES | booleano | a, b | `a > b` |
| `codigo.lobo` | CODIGO | cap | codigo | El texto literal del campo |
| `hud.vidas` | CODIGO | pila | vidas | `hud_vidas(N)` |
| `hud.puntos` | CODIGO | pila | puntos | `hud_puntos(N)` |
| `hud.dialogo` | CODIGO | pila | linea, texto | `hud_dialogo(N, "texto")` |
| `hud.texto` | CODIGO | pila | texto, y | `hud_texto(8, N, "texto", 16777215, 1)` |
| `hud.plantilla` | CODIGO | pila | cual | `hud_plantilla(N)` |
| `sonido.sonar` | CODIGO | pila | sonido | `sonar("nombre")` |
| `sonido.musica` | CODIGO | pila | musica | `musica("nombre")` |
| `sonido.detener` | CODIGO | pila | - | `detener_sonido()` |
| `sonido.volumen` | CODIGO | pila | valor | `volumen(N)` |
| `sonido.volumen_actual` | CODIGO | reportero | - | `volumen_actual()` |
| `datos.guardar` | CODIGO | pila | clave, valor | `guardar("clave", N)` |
| `datos.cargar` | CODIGO | reportero | clave | `cargar("clave")` |
| `datos.borrar_save` | CODIGO | pila | - | `borrar_save()` |
| `datos.record` | CODIGO | pila | clave, valor | `record("clave", N)` |
| `datos.record_actual` | CODIGO | reportero | clave | `record_actual("clave")` |
| `datos.ajuste` | CODIGO | pila | nombre, valor | `ajuste("nombre", N)` |
| `datos.ajuste_actual` | CODIGO | reportero | nombre | `ajuste_actual("nombre")` |

Los reporteros se resuelven dentro de la sentencia que los usa; si cuelgan
solos no producen codigo. El generador recorre las raices en orden visual y
valida el resultado con `lobo_compilar(..., LOBO_MODO_VM, ...)`.

## Ver y convertir el codigo

**VER CODIGO (F2)** muestra el lobo generado en solo lectura, con numeros de
linea, colores para palabras clave y `mi.`, y resaltado en rojo de la linea con
error. **VOLVER AL LIENZO** o `Esc` cierran el visor. **CONVERTIR A CODIGO** es
irreversible: sustituye el lienzo por un unico bloque `codigo.lobo` con el
texto generado, para seguir editando en lobo.

El visor no reescribe el proyecto. Solo **CONVERTIR A CODIGO** y las mutaciones
del lienzo vuelcan al modelo LOGIC3. Los flags de prueba son
`--compilar-bloques`, `--visor-codigo`, `--convertir-bloques`,
`--volcar-codigo RUTA` y `--campo-bloque ID:TEXTO`.

## Volcado de pruebas

En la pestana Bloques, `--volcar` anade una linea propia:

```text
lb_bloques=N lb_sel=N lb_ranuras=N lb_cuerpo=N lb_paleta=N lb_categoria=N lb_foco=N lb_zoom=N lb_pan=X,Y
```

El campo `bloques=N` de la primera linea sigue siendo el conteo LOGIC3. Cuando
el compilador interviene se anade otra linea:

```text
cb_visor=N cb_lineas=N cb_error=TEXTO cb_bloque=N cb_linea=N cb_convertido=N
```

## Portabilidad BIOS

El subconjunto VM de la BIOS no puede ejecutar `codigo.lobo` arbitrario,
`hud.texto`, `hud.plantilla`, guardado, records ni ajustes en caliente. Si un
bloque de esos esta en el lienzo, `--bios-avisos` lo senala; la imagen se genera
de todos modos y el kernel cae al interprete clasico cuando no hay bytecode.
`hud.vidas`, `hud.puntos` y `hud.dialogo` usan llamadas del subconjunto y solo
necesitan que el texto del `codigo.lobo` sea portable.

Para la gramatica completa de las sentencias generadas, consulta
`REFERENCIA_LENGUAJE.md`.
