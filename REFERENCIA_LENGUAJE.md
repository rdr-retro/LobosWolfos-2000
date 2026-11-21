# Referencia del lenguaje lobo

`lobo` es el lenguaje propio de LobosWolfos 2000. Es texto plano, con palabras
en espanol y una sentencia por linea. Hay dos modos:

- **Clasico (LOGIC1/LOGIC2)**: el interprete de `motor/lobo.c`. Es el de los
  proyectos antiguos y sigue intacto.
- **VM (lobo 3, LOGIC3)**: el que usan los bloques `codigo.lobo` y el ejemplo
  canonico. Anade tipos, ambitos, funciones, listas, textos y VM resumible.

El compilador de bloques genera codigo en modo VM. El `game.exe` y la vista
previa incrustan el LOGIC3 nativo cuando el modelo esta activo, porque el
ejemplo usa `mi.variable("monedas")`, que el interprete clasico no compila.

## Lexico

- `#` inicia un comentario hasta el final de la linea.
- Los nombres empiezan por letra y continuan con letras, digitos o `_`.
- Los numeros son enteros decimales. No hay coma flotante: `redondear(v)` es
  la identidad.
- Los textos van entre comillas con los escapes `\"`, `\\`, `\n`, `\r` y `\t`.
- Los operadores son `+ - * / %`, `== != < > <= >=`, `&`, `|`, `no` y `!`.
- La division entre cero da 0 y la aritmetica envuelve como unsigned.

## Sentencias

```lobo
si <expr>
    ...
sino si <expr>
    ...
sino
    ...
fin

mientras <expr> ... fin
repetir N ... fin
para cada objeto en inventario ... fin
funcion nombre(a, b) ... fin
retornar <expr>
romper
continuar
esperar N
detener
mensaje "texto"
```

`si` admite `sino si` encadenados y un solo `fin`. `para cada` tambien admite
`de` en lugar de `en`. `romper` y `continuar` actuan sobre el bucle mas interno.
`esperar N` pausa N fotogramas y reanuda donde estaba.

En el modo clasico hay ademas `x`, `y`, `vel` y `fotograma` como variables del
juego; cualquier otro nombre asignado es local del script y se preescanea, asi
que un error tipografico da error de sintaxis.

## Tipos y ambitos

```lobo
entero vidas = 3
booleano listo = 1
texto nombre = "Lobo"
lista inventario
lista texto nombres
entero $puntos = 0
booleano @vivo = 1
```

Los cuatro tipos son `entero`, `booleano`, `texto` y `lista`. El prefijo `$` es
global compartido por todos los scripts y `@` es variable de la entidad actual.
Sin prefijo es local, o parametro dentro de una funcion. Una declaracion sin
valor inicial vale 0 o `""`.

Las variables de entidad que ya conoce el motor son `mi.x`, `mi.y`,
`mi.vida`, `mi.visible`, `mi.traje` y `mi.variable("nombre")`. Las de fisica son
`vel_x`, `vel_y`, `gravedad`, `rebote`, `friccion` y `en_suelo`.

## Funciones

```lobo
funcion danar(cantidad)
    retornar cantidad * 2
fin

x = danar(5)
y = llamar("danar", 3)
```

Las llamadas directas y `llamar("nombre", args...)` son equivalentes. Se
admiten hasta 8 parametros y 64 funciones en modo VM.

## Built-ins

| Funcion | Que hace |
|---|---|
| `abs(v)` | Valor absoluto |
| `min(a, b)`, `max(a, b)` | Menor y mayor |
| `limitar(v, bajo, alto)` | Recorta `v` al intervalo |
| `redondear(v)` | Identidad: solo hay enteros |
| `aleatorio(a, b)` | Entero en `[a, b]`, determinista con semilla 12345 |
| `longitud(t)` | Caracteres del texto, hasta 63 |
| `letra(t, i)` | Codigo ASCII del caracter `i` |
| `unir(a, b)` | Concatena hasta 63 caracteres |

Cada texto ocupa 64 bytes: 63 utiles y el terminador. Los textos creados por
`unir()` usan 32 huecos compartidos por todas las maquinas del proyecto.

## Listas

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

| Funcion | Alias | Que hace |
|---|---|---|
| `agregar(l, v)` | `anadir` | Anade al final |
| `insertar(l, i, v)` | | Inserta en la posicion |
| `borrar(l, i)` | | Borra la posicion |
| `fijar(l, i, v)` | | Cambia la posicion |
| `elemento(l, i)` | | Lee la posicion |
| `largo(l)` | | Numero de elementos |
| `contiene(l, v)` | `tiene` | Busca un valor |
| `vaciar(l)` | `limpiar` | Borra todos los elementos |
| `ordenar(l)` | | Ordena de menor a mayor |

## Colisiones y entidades

| Funcion | Que hace |
|---|---|
| `toca(id)` | 1 si la entidad toca a la otra |
| `toca_tag("nombre")` | Como `toca`, por nombre del objeto |
| `distancia(id)` | Distancia Manhattan |
| `caja_de(id, i)` | `i` de 0 a 3 para x, y, ancho y alto |
| `crear`, `clonar`, `borrar`, `mover` | Ciclo de vida de entidades |
| `enviar("msg")`, `enviar_a("obj", "msg")` | Cola de mensajes |
| `detener_script`, `detener_todo`, `reiniciar_escena` | Control del planificador |

Devuelven 0 o -1 cuando no hay entidad. Los mapas `.mav` v2 pueden marcar
celdas con `solido`, `hueco`, `trigger`, `one_way`, `dano` y `tag = "nombre"`.
El v1 se lee como todo solido. `game.exe --colisiones` anade a la traza una
linea con `suelo`, `dano` y los triggers tocados.

## HUD, sonido, guardado y ajustes

| Funcion | Que hace |
|---|---|
| `hud_vidas(n)`, `hud_puntos(n)` | Muestra vidas o puntos |
| `hud_dialogo(linea, "texto")` | Linea de dialogo |
| `hud_texto(x, y, "texto", color, escala)` | Texto libre |
| `hud_plantilla(n)` | Plantilla preparada de HUD |
| `sonar("nombre")`, `musica("nombre")` | Efecto o musica en bucle |
| `detener_sonido()`, `volumen(n)`, `volumen_actual()` | Audio |
| `guardar("clave", v)`, `cargar("clave")`, `borrar_save()` | Partida guardada |
| `record("clave", v)`, `record_actual("clave")` | Mejor marca |
| `ajuste("nombre", v)`, `ajuste_actual("nombre")` | Ajustes en caliente |

## Valores iniciales en LOGIC3

```text
variable global "vidas" entero 3
variable global "nombre" texto "Lobo \"feroz\""
lista global "inventario" 8 entero 3 1 2
lista global "nombres" 4 texto espada "pocion roja"
```

Los numeros se escriben en forma canonica (`007` se guarda como `7`) y los
booleanos como `0` o `1`. Al cargar, el motor siembra la memoria con estos
valores en orden de declaracion. Las secciones desconocidas se ignoran y
`FIN`/`CRC32` siguen siendo opcionales en el lector.

## Limites

| Recurso | Tope en VM |
|---|---|
| Instrucciones | 4096 |
| Locales | 64 |
| Funciones | 64 |
| Parametros por funcion | 8 |
| Frames de llamada | 32 |
| Valores de pila | 256 |
| Globales | 128 |
| Variables de entidad | 64 |
| Listas | 32 de 256 elementos |
| Textos | 64 de programa y 32 de runtime |
| Pasos por fotograma | 1000000 |

El modo clasico conserva sus topes antiguos: 32 funciones, 2048 bytes por
boton, 256 instrucciones, 16 locales, 32 valores de pila y 200000 pasos.

## Ejemplo canonico

`ejemplos/primer_juego.logic3` usa eventos de fotograma y tecla con bloques
`codigo.lobo`:

```lobo
si toca(2)
borrar(2)
mi.variable("monedas") = mi.variable("monedas") + 1
fin
```

Se puede abrir con `--abrir-logic3`, jugar con play, probar con
`--captura-juego` y exportar con `--guardar-wolfo --wolfo-logic3` o
`--compilar-windows`. La suite `pruebas/exportacion.ps1` congela el paquete, la
restauracion, el `game.exe` autonomo y la paridad Windows/QEMU.
