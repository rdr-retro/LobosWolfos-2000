# Plan: gestor de logica como constructor de juegos tipo Scratch

Estado: propuesto  
Version: 1.0 (21-11-2026)  
Punto de partida: commit `07ea2e5`  
Alcance: siguiente salto del gestor de logica, no sustituye a la documentacion de usuario actual.

---

## 0. Idea central

El gestor de logica debe dejar de ser un lienzo donde solo se sueltan mapas y
funciones sueltas. Tiene que ser un **constructor de programas**:

- El **mapa insertado es solo el escenario**. No crea personajes.
- Un **actor** es una pieza independiente con traje, posicion, variables y
  scripts.
- Un **jugador no es un tipo de sprite**: es un **perfil de control** formado
  por un actor asignado, un dispositivo, un esquema de entradas y un conjunto
  de funciones de accion.
- Los **bloques** viven dentro de ese actor o de esa funcion, se encajan como
  en Scratch y se compilan a lobo 3 automaticamente al pulsar Play, al guardar
  y al empaquetar.
- Si no existe una pieza `Control` asignada a un actor, **no hay jugador**: no
  aparece el lobito azul, no hay movimiento por teclado y la camara no sigue a
  nadie.

La regla que resume todo el plan es:

> Insertar un mapa crea un escenario. Crear un jugador requiere insertar un
> actor y despues asignarle una pieza de control con sus acciones.

---

## 1. Por que pasa hoy el personaje fantasma

Estado actual, localizado en el codigo:

- `src/app.c:1736-1775` inserta un `ModuloElemento` con `funcion = -1` cuando
  se suelta un mapa. Ese elemento solo guarda `mapa`, `x`, `y`, `ancho`,
  `alto`, `id` y `plegado`.
- `src/logic3.c:1863-1994` migra el primer mapa a `Escena3`, pero no crea un
  jugador.
- `src/juego_ventana.c:60-84,415-422` crea siempre un sprite lobito de 24x24
  en `(16,16)` y lo dibuja cuando hay mapa.
- `motor/juego.c:252-275,1554-1588,2628-2634` hace exactamente lo mismo en el
  `game.exe` compilado.
- `motor/render.c:407-454` interpreta `camara objetivo 0` como “seguir al
  jugador”, y `motor/lobo.c:3253` hace que `mi` caiga en ese sprite cuando no
  hay entidad.

No hay ningun campo `jugador`, `rol` ni `controlado_por` en `ModuloElemento`
ni en `Objeto3` (`src/app.h:750-761`, `src/logic3.h:149-163`). El personaje
esta escrito a mano en los dos runtimes, no viene del proyecto.

Ademas, la vista previa carga solo `proyecto.logic` (`src/juego_ventana.c:402-404`),
mientras que `game.exe` prefiere `proyecto.logic3` cuando el blob lo trae
(`motor/juego.c:2638-2645`). Por eso el mapa del gestor y el mapa ejecutado
pueden divergir. Este plan tambien debe cerrar esa divergencia.

---

## 2. Principios de diseno

1. **Fuente unica:** el proyecto LOGIC3 canonico manda para los proyectos
   nuevos. LOGIC2 queda como proyeccion de compatibilidad.
2. **Sin magia:** ningun elemento del lienzo crea actores, jugadores ni
   camaras “por defecto”. Todo lo que exista en runtime debe tener una pieza
   visible en el lienzo.
3. **Pieza = dato + vista + regla:** cada pieza se guarda en el proyecto, se
   dibuja en el lienzo y se interpreta en el motor. No puede haber una de las
   tres sin las otras.
4. **Scratch por capas:** lienzo de programa para piezas grandes, lienzo de
   bloques para scripts dentro de un actor o funcion.
5. **Append-only:** los 27 bloques actuales no se reordenan. Las piezas nuevas
   se anaden al final de `DEFS`, `AYUDA_DEFS`, `DEFS_NO_BIOS` y del enum
   `DEF_*` de `src/compilador_bloques.c`.
6. **Formatos intactos:** no se cambia la gramatica LOGIC3, su CRC ni los
   lectores LOGIC1/LOGIC2. Las piezas nuevas se guardan como bloques
   `sistema.*` ya soportados por el escritor.
7. **Indices intactos:** `ARCHIVO=1`, `MODO=2`, `AYUDA=9`. No se anade un
   menu superior ni una septima pestana de Estudio.
8. **Determinismo:** entrada, eventos y compilacion deben poder simularse por
   CLI con `--traza`, `--volcar` y patrones de texto/BMP.
9. **Compatibilidad explicita:** los proyectos antiguos pueden seguir usando
   el sprite clasico solo si se marcan como `modo=clasico`; los proyectos
   nuevos nacen en `modo=programa`.

---

## 3. Modelo de datos propuesto

### 3.1 Como se guarda sin tocar el formato

El escritor de LOGIC3 ya acepta cualquier texto en `Bloque3.tipo`
(`src/logic3.h:177-195`, `src/logic3.c:1684-1712`) y el lector del motor ya
ignora tipos desconocidos (`motor/logic3_motor.c:194-236`). Por tanto no hace
falta una nueva palabra clave ni una nueva seccion.

Las piezas de programa se guardan como bloques del tipo:

```text
bloque <id> evento 1 padre <padre> orden <n> tipo sistema.<pieza>
  campo "<clave>" "<valor>"
  ranura "<puerto>" <id>
```

- `evento 1` (`inicio`) solo cumple la validacion actual
  (`src/logic3.c:1580-1587`); el motor no las ejecuta como scripts.
- Los valores van en `campo`, con `L3_CAMPOS_MAX`.
- Las referencias a otras piezas van en `ranura` o en campos con ids.
- El escritor y el CRC no cambian. Los archivos antiguos siguen byte a byte
  igual; solo los proyectos nuevos contienen lineas `sistema.*` nuevas.

### 3.2 Tipos de pieza

| Pieza | `tipo` | Campos principales | Produce |
|---|---|---|---|
| Opciones | `sistema.opciones` | `programa=1`, `jugador_implicito=0`, `auto_compilar=1` | Modo de ejecucion del proyecto |
| Escenario | `sistema.escena` | `id`, `mapa`, `ancho`, `alto`, `fondo` | `Escena3` |
| Actor | `sistema.actor` | `objeto`, `nombre`, `traje`, `x`, `y`, `capa`, `visible` | `Objeto3` |
| Control | `sistema.control` | `jugador`, `actor`, `dispositivo`, `esquema`, `activo` | Perfil de jugador |
| Accion | `sistema.accion` | `entrada`, `flanco`, `funcion`, `argumentos` | Enlace entrada -> funcion |
| Regla | `sistema.regla` | `evento`, `actor`, `funcion`, `flanco`, `tag`, `cada` | `Evento3` |
| Funcion | `sistema.funcion` | `nombre`, `parametros`, `retorno` | `Funcion3` + hat `llamada` |
| Dato | `sistema.dato` | `nombre`, `tipo`, `ambito`, `valor` | `Variable3` o `Lista3` |
| Recurso | `sistema.recurso` | `tipo`, `ruta` | Validacion de empaquetado |
| HUD | `sistema.hud` | `elemento`, `jugador`, `variable`, `x`, `y` | Plantilla de HUD ligada a un jugador |

Notas:

- `sistema.actor.objeto` apunta al `id` real de `Objeto3`, no al id del bloque
  de la pieza.
- `sistema.control.jugador` va de 1 a 4. El valor 0 significa “sin jugador”.
- `sistema.accion` cuelga de un `sistema.control` mediante `padre`.
- `sistema.regla` cuelga de un `sistema.actor` o de una `sistema.funcion`.
- `sistema.opciones.jugador_implicito=0` es la marca que distingue un proyecto
  nuevo de uno migrado. Si la pieza no existe, el motor usa `1` por
  compatibilidad.

### 3.3 Perfil de jugador

Un jugador queda descrito por este conjunto:

```text
sistema.control jugador=1 actor=100 dispositivo=teclado esquema=plataformas
  sistema.accion entrada=izquierda flanco=mantener funcion=mover_izq
  sistema.accion entrada=derecha   flanco=mantener funcion=mover_der
  sistema.accion entrada=espacio   flanco=pulsar   funcion=saltar
sistema.funcion nombre=mover_izq parametros=0 retorno=0
sistema.funcion nombre=mover_der parametros=0 retorno=0
sistema.funcion nombre=saltar    parametros=0 retorno=0
```

Cada `sistema.funcion` tiene debajo su pila de bloques normal. Esa es la
“lista de funciones” que menciona el usuario: el jugador no es un sprite
especial, es **control + acciones + funciones sobre un actor cualquiera**.

---

## 4. Interfaz del gestor de logica

### 4.1 Lienzo de programa

La pestana `MODO > Gestor de logica` pasa a tener dos capas conmutables desde
la propia barra del gestor, sin anadir menus superiores:

- **Programa:** piezas grandes, rejilla, iman y lineas de enlace.
- **Compatibilidad:** vista actual de mapas y funciones LOGIC2.

El conmutador no cambia `ARCHIVO=1`, `MODO=2` ni `AYUDA=9`.

### 4.2 Paleta de piezas

Columna izquierda con grupos plegables:

1. Escenario: Escenario, Camara, Transicion.
2. Actores: Actor, Traje, Clon.
3. Control: Teclado, Raton, Mando, Jugador 1..4.
4. Eventos: Inicio, Tecla, Tocar, Cada, Recibir, Escena.
5. Acciones: Movimiento, Apariencia, Mensaje, Audio, HUD, Datos.
6. Funciones: Funcion, Parametro, Retorno.
7. Recursos: Mapa, Imagen, Sonido.

Cada pieza tiene un color y un puerto:

- `escena`: entradas de mapa y fondo.
- `actor`: salida hacia control, regla y camara.
- `entrada`: salida desde teclado/raton/mando hacia accion.
- `accion`: entrada desde control y salida hacia funcion.
- `funcion`: entrada desde accion o regla.

### 4.3 Inspector

Columna derecha-contextual con:

- Nombre, id, tipo y estado de validacion.
- Campos de la pieza con desplegables cuando el valor es enumerado.
- Selector de actor, dispositivo, esquema y jugador.
- Boton “Ir a scripts” que abre la pestaña `Bloques` del Estudio con el actor
  o la funcion seleccionados.
- Avisos: sin traje, sin funcion asignada, jugador duplicado, dispositivo no
  portable en BIOS.

### 4.4 Reglas de interaccion

- Arrastre con acoplamiento a rejilla ya existente en `src/app.c`.
- Conexion por arrastre desde un puerto a otro; alternativamente se puede
  elegir el destino en un desplegable del inspector.
- Doble clic en una pieza `Actor` o `Funcion` abre sus bloques.
- Suprimir una pieza pregunta si se borran sus hijos.
- Deshacer/rehacer cubre insercion, borrado, enlace y cambio de campo.
- El lienzo dibuja una insignia roja si la pieza no compila o no tiene
  destino.

---

## 5. Componentes que faltan y como se construyen

| Componente | Hoy | Se anade | Reutiliza |
|---|---|---|---|
| Modelo de programa | Mapas y funciones sueltas | Piezas `sistema.*` en bloques LOGIC3 | `Bloque3`, `campo`, `ranura` |
| Escenario | `ModuloElemento.mapa` | Pieza Escenario que crea `Escena3` | `logic3_desde_logic2` |
| Actor | `Objeto3` sin semantica de jugador | Pieza Actor con traje y scripts | `estudio_objeto_nuevo` |
| Control | No existe | Perfil jugador 1..4 y esquemas | ninguno, nuevo |
| Teclado | 32 ranuras + opcodes 2,64-66 | Piezas y mapeo de acciones | `motor/entrada.c` |
| Raton | Botones, x/y, rueda, dentro | Piezas y coordenadas de mundo | opcodes 67-71 |
| Mando | No existe | winmm dinamico, 2 mandos, flancos | `motor/audio.c` como patron de carga |
| Funciones | `Funcion3` y `L3_EVENTO_LLAMADA` sin UI completa | Pieza Funcion y editor de parametros | VM `llamar`/`retornar` |
| Compilacion | Conversion manual irreversible | Compilacion automatica en memoria | `compilador_bloques.c` |
| Depuracion | Visor de codigo | Valores sobre bloques, resaltado y traza de entrada | `depurador.ps1` |
| Runtime | Sprite fijo en dos motores | Cargador `sistema.c` y actores explicitos | `entidades_preparar` |

---

## 6. Fases de ejecucion

Cada fase es un commit. Se numeran a partir de la 46 porque el plan maestro
anterior cerro en la 45.

### Fase 46 - Contrato del modelo y fin del jugador implicito

**Objetivo:** dejar definido que significa “programa” y que un mapa no crea
un jugador.

**Tareas:**

1. Crear `src/sistema.h` con `SistemaOpciones`, `SistemaControl`,
   `SistemaAccion`, `SistemaRegla` y constantes de dispositivo/flanco.
2. Crear `src/sistema.c` con lectura de bloques `sistema.*` desde `Proyecto3`
   y validacion: un actor por control, jugadores 1..4 unicos, funciones
   existentes, esquema conocido.
3. Anadir `sistema.opciones` a todo proyecto nuevo creado por
   `logic3_nuevo_editor()` con `jugador_implicito=0` y `auto_compilar=1`.
4. Definir que un proyecto sin `sistema.opciones` se trata como legacy y
   mantiene el sprite clasico. Un proyecto con `jugador_implicito=0` no lo
   mantiene.
5. Unificar la carga: la vista previa debe usar `motor_l3_cargar()` cuando
   exista `proyecto.logic3`, como hace `game.exe`.

**Criterio de salida:** insertar un mapa en un proyecto nuevo, guardar y
ejecutar no muestra ningun personaje; los proyectos legacy siguen igual.

**Estado:** completada. `src/sistema.c` valida las piezas `sistema.*`, los
proyectos nuevos nacen con `jugador_implicito=0` y tanto `game.exe` como la
vista previa usan el mismo modelo. Legacy mantiene el sprite clasico.

**Pruebas:** `logic3`, `regresion` caso `gestor`, `exportacion`, nueva suite
`programa`.

---

### Fase 47 - Lienzo de programa

**Objetivo:** insertar, mover, enlazar y borrar piezas grandes en el gestor.

**Tareas:**

1. Conmutador `Programa/Compatibilidad` en la barra del gestor sin tocar
   indices de menu.
2. Paleta plegable de piezas descrita en el apartado 4.2.
3. Dibujo de piezas con borde, color, nombre, icono de 16x16 y puertos.
4. Arrastre con iman reutilizando `modulos_insertar_en`.
5. Lineas de enlace derivadas de `campo`/`ranura`.
6. Inspector contextual y validacion en vivo.
7. Undo/redo de operaciones de programa.

**Criterio de salida:** se puede construir un programa vacio con un Escenario,
un Actor y un Control sin escribir codigo ni usar menus nuevos.

**Pruebas:** nueva suite `programa` con `--gestor --programa`, captura BMP,
`--volcar` y sondas de pixel de puertos y seleccion.

---

### Fase 48 - Escenario y actores

**Objetivo:** separar fondo de personajes.

**Tareas:**

1. La pieza Escenario escribe/actualiza `Escena3` y el mapa en `g.modulos`
   solo como proyeccion LOGIC2.
2. La pieza Actor crea un `Objeto3` con traje vacio por defecto; nunca asigna
   jugador.
3. Importar traje desde `recursos/` o `trajes/` y actualizar `Traje3`.
4. Sincronizar insertar/borrar mapa en el gestor con `g3.escenas`.
5. Proyeccion a LOGIC2 avisa de perdida de piezas `sistema.*`.

**Criterio de salida:** un proyecto con solo Escenario no tiene objetos en
`--volcar`; un Actor sin Control no se mueve con las flechas.

**Pruebas:** `programa`, `estudio`, `logic3`, `regresion`.

---

### Fase 49 - Control de jugador y perfil de acciones

**Objetivo:** sustituir el sprite magico por un perfil explicito.

**Tareas:**

1. Pieza Control con `jugador`, `actor`, `dispositivo` y `esquema`.
2. Esquemas iniciales: `plataformas`, `superior` y `personalizado`.
3. Piezas Accion hijas: entrada, flanco, funcion y argumentos.
4. `motor/sistema.c` resuelve el actor controlado y lo pasa a
   `entidades_preparar()`.
5. Camara: si `objetivo=0` y existe jugador, sigue a su actor; si no existe,
   camara fija centrada. Nunca usa coordenadas de un sprite inexistente.
6. Entrada de flechas/espacio: solo mueve al actor del control activo.
7. `mi` sin entidad deja de caer en el sprite; devuelve error de script o
   valor neutro segun modo de depuracion.

**Criterio de salida:** con dos actores y un solo Control asignado al actor 2,
solo se mueve el actor 2 y la camara lo sigue. Sin Control, no hay movimiento
ni sprite.

**Pruebas:** `programa`, `entrada`, `entidades`, `render`, `escenas`, `bios`.

---

### Fase 50 - Eventos, reglas y funciones

**Objetivo:** que las reglas del lienzo generen scripts reales.

**Tareas:**

1. Pieza Regla para los 9 eventos de `L3_EVENTO_*`.
2. Selector de flanco para tecla (`mantener`, `pulsar`, `soltar`).
3. Pieza Funcion con nombre, parametros y retorno.
4. Editor de parametros dentro del inspector; usa `Funcion3` ya existente.
5. Al crear una funcion se anade su hat `L3_EVENTO_LLAMADA`.
6. Una accion puede apuntar a una funcion o a una pila en linea.
7. Validacion de ciclos de llamada y de funciones sin cuerpo.

**Criterio de salida:** al pulsar una tecla se ejecuta la funcion enlazada, y
la traza muestra el evento y la funcion correctos.

**Pruebas:** `programa`, `compilador`, `depurador`, `vm`.

---

### Fase 51 - Compilacion automatica de bloques

**Objetivo:** ejecutar bloques sin “Convertir a codigo”.

**Tareas:**

1. `compilar_programa()` recorre actores, reglas y acciones y llama a
   `compilador_bloques` en memoria.
2. El resultado se guarda en los bloques `codigo.lobo` correspondientes como
   cache derivada, o se pasa directo al planificador sin escribir archivos.
3. Se ejecuta al pulsar Play, al guardar LOGIC3 y al empaquetar `.wolfo` y
   `game.exe`.
4. Los errores conservan `linea_bloque` y resaltan el bloque origen.
5. `VER CODIGO (F2)` pasa a ser diagnostico de solo lectura.
6. `CONVERTIR A CODIGO` se mantiene como exportacion opcional, ya no es
   obligatorio ni irreversible para ejecutar.

**Criterio de salida:** el mismo proyecto produce la misma traza antes y
despues de pulsar Play, sin conversion manual y sin archivos intermedios.

**Pruebas:** `compilador`, `bloques`, `depurador`, `programa`, `regresion`.

---

### Fase 52 - Catalogo de bloques base

**Objetivo:** cubrir las piezas de codigo mas usadas.

**Tareas:** anadir al final de `DEFS` (sin reordenar) los bloques:

- Eventos: `al fotograma`, `al tocar`, `cada N`, `al clonar`, `al entrar escena`.
- Control: `si...sino`, `repetir N`, `esperar`, `detener`, `llamar`, `retornar`.
- Movimiento: `mover y`, `ir a`, `fijar vel_x/vel_y`, `gravedad`, `en_suelo`.
- Operadores: resta, multiplicacion, division, modulo, igual, distinto,
  menor, menor o igual, mayor o igual, y, o, no, abs, min, max, limitar,
  aleatorio.
- Datos: fijar variable, variables globales y de entidad, longitud de lista,
  elemento, anadir, insertar, borrar, contiene, ordenar, vaciar.
- UI: campo 1 editable y reporteros anidados en `generar_expresion`.
- Paleta con busqueda por texto y desplegable de categoria para que 60-90
  bloques sigan siendo navegables.

**Criterio de salida:** se puede escribir un plataformas completo sin usar
`codigo.lobo` manual.

**Pruebas:** `bloques`, `compilador`, `vm`, `programa`.

---

### Fase 53 - Teclado y raton como piezas de sentido

**Objetivo:** usar la entrada existente desde el lienzo.

**Tareas:**

1. Piezas `tecla pulsada/mantenida/soltada` para las 32 ranuras.
2. Piezas de raton: boton 1/2/3, x, y, rueda, dentro.
3. Mapa de nombres amigables a ranuras en el inspector.
4. Opcodes nuevos al final para `raton_mundo_x` y `raton_mundo_y` cuando la
   camara este desplazada. Numeracion 104 y 105.
5. Flag `--entrada-simulada` para inyectar secuencias deterministas en pruebas.
6. Traza `--traza-entrada` con entrada, flanco, jugador y funcion destino.

**Criterio de salida:** un actor sigue al raton en coordenadas de mundo aunque
la camara este movida; no hay regresion en `entrada.ps1`.

**Pruebas:** `entrada`, `bloques`, `compilador`, `programa`, `render`.

---

### Fase 54 - Mando/gamepad

**Objetivo:** soporte opcional de mando sin dependencias de enlazado.

**Tareas:**

1. `motor/mando.c` carga `winmm.dll` en tiempo de ejecucion con
   `joyGetPosEx` y `joyGetDevCaps`, igual que `motor/audio.c` carga waveOut.
2. Hasta 2 mandos, 16 botones, ejes X/Y, POV, zona muerta configurable y
   flancos pulsar/soltar.
3. Opcodes nuevos al final: `mando_conectado`, `mando_boton`,
   `mando_boton_pulsado`, `mando_boton_soltado`, `mando_eje`.
4. Piezas de paleta para cada opcode y esquema `mando_1`/`mando_2`.
5. Marcas `DEFS_NO_BIOS` y avisos claros: el mando no existe en BIOS.
6. Simulacion de mando por CLI para pruebas headless.

**Criterio de salida:** el mismo perfil de jugador puede cambiar de
`dispositivo=teclado` a `dispositivo=mando` sin tocar las funciones de accion.

**Pruebas:** `entrada`, `programa`, `bios`, `compilador`, `exportacion`.

---

### Fase 55 - Acciones de mundo completas

**Objetivo:** que todas las acciones importantes tengan pieza.

**Tareas:**

1. Entidades: crear, clonar, borrar, mover, `mi.x`, `mi.y`, `mi.traje`,
   `mi.visible`, `mi.variable`.
2. Colisiones: `toca id`, `toca tag`, `distancia`, `caja de`, `choca`.
3. Mensajes: enviar, enviar a, recibir, reiniciar escena.
4. Camara: objetivo, zoom, limites, suavizar, transicion, parpadeo, temblor,
   flash y fade.
5. HUD: vidas, puntos, dialogo, rect, barra, imagen, boton y plantillas.
6. Cada bloque lleva ayuda y marca BIOS correcta.

**Criterio de salida:** un juego completo (plataformas + disparos + HUD + dos
escenas) se puede construir solo con piezas.

**Pruebas:** `bloques`, `colisiones`, `eventos`, `entidades`, `hud`, `render`,
`programa`.

---

### Fase 56 - Depuracion orientada a piezas

**Objetivo:** entender que hace cada pieza mientras se ejecuta.

**Tareas:**

1. Resaltar la pieza y el bloque activos en `--captura`.
2. Mostrar valores de variables sobre la pieza `Dato`.
3. Puntos de ruptura condicionales por bloque.
4. Paso a paso por fotograma y por evento.
5. Panel de traza filtrable por jugador, actor, evento y funcion.
6. Error de compilacion lleva al bloque y a la pieza que lo contiene.

**Criterio de salida:** un fallo de logica se localiza desde el lienzo sin
leer codigo lobo.

**Pruebas:** `depurador`, `programa`, `regresion`.

---

### Fase 57 - Migracion, plantillas y cierre

**Objetivo:** que los proyectos antiguos y las plantillas convivan con el
modelo nuevo.

**Tareas:**

1. Comando de migracion explicita que anade `sistema.opciones` con
   `jugador_implicito=0` y convierte el sprite clasico en un Actor + Control
   si el usuario quiere conservarlo.
2. Plantillas de `src/plantillas.c` pasan a crear Actor y Control reales, no
   un objeto llamado “Jugador” por convencion.
3. `ejemplos/primer_juego.logic3` y las plantillas se reescriben con piezas
   `sistema.*`.
4. Sincronizar `build.ps1` con `build.bat`, que hoy no compila la lista real
   de fuentes ni expone `prueba`.
5. Actualizar documentacion y release.

**Criterio de salida:** los patrones antiguos siguen pasando; los ejemplos
nuevos no dependen del sprite implicito.

**Pruebas:** todas las suites, especialmente `logic3`, `exportacion`, `bios`,
`regresion` y `programa`.

---

## 7. Suite nueva `programa.ps1`

Se anade como suite 24. Debe seguir el patron de `pruebas/comun.ps1`:

- Patrones de texto, no binarios opacos.
- `--captura` comparado por SHA-256.
- `--volcar` normalizado.
- Sondas `x,y=RRGGBB` para puertos, seleccion y actor resaltado.
- `--traza` para eventos y mapeo de entradas.

Casos minimos:

1. `mapa_solo`: escenario sin actor; volcado sin objetos; BMP sin lobito.
2. `actor_sin_control`: actor presente; no se mueve con flechas.
3. `control_teclado`: actor 2 se mueve, actor 1 quieto.
4. `control_raton`: coordenadas de mundo con camara desplazada.
5. `control_mando`: boton simulado llama a la funcion prevista.
6. `funcion_llamada`: tecla -> accion -> funcion -> traza.
7. `auto_compilar`: Play sin conversion manual produce la misma traza.
8. `error_pieza`: accion sin funcion marca error y resalta bloque.
9. `legacy`: proyecto sin `sistema.opciones` mantiene sprite clasico.
10. `dos_jugadores`: jugadores 1 y 2 con actores distintos.

Integracion en `build.bat`: cabecera de suites, dispatch, bloque de
ejecucion, bloque `:pruebas_programa` y lista `-Actualizar`. Tambien corregir
el comentario de `build.bat:88`, que omite `exportacion`.

---

## 8. Compatibilidad y riesgos

| Riesgo | Mitigacion |
|---|---|
| Cambio de CRC en patrones LOGIC3 | No cambiar el escritor; piezas nuevas solo en proyectos nuevos |
| Lectores antiguos | `tipo` arbitrario ya se preserva; `codigo.lobo` sigue intacto |
| `game.exe` viejo ignorando piezas nuevas | Documentar que los proyectos `programa` requieren el runtime nuevo |
| Regresion del sprite en pruebas antiguas | Proyectos sin `sistema.opciones` siguen en modo legacy |
| Desfase gestor/LOGIC3 | `Escena3` pasa a ser fuente; LOGIC2 es proyeccion |
| Desfase Play/game.exe | Ambos cargan `proyecto.logic3` cuando existe |
| Reordenacion accidental de bloques | Anadir siempre al final de los cuatro arrays paralelos |
|Indices de menu | No anadir menus superiores; acciones nuevas al final de submenus |
| BIOS | Mando y guardado marcados no portables con aviso |
| Tamano del blob `LGB1` | Medir en cada fase; revisar `pruebas/exportacion/esperado` |
| Paleta inusable | Buscador y categorias plegables antes de pasar de ~40 piezas |
| Dependencia externa | winmm se carga dinamicamente, como ya hace el audio |

---

## 9. Documentacion a actualizar

- `README.md`: nueva seccion “Constructor de programas” y suite 24.
- `MANUAL_BLOQUES.md`: catalogo ampliado, campo 1, buscador y ejecucion
  automatica.
- `REFERENCIA_LENGUAJE.md`: mapeo pieza -> lobo y opcodes 104 en adelante.
- `BIOS_SUBCONJUNTO.md`: fin del jugador implicito, rol explicito y avisos de
  mando.
- `pruebas/README.md`: 24 suites y caso de uso de `programa.ps1`.
- `ejemplos/LEEME.md`: ejemplo con Actor + Control + Funciones.

---

## 10. Comandos de verificacion

```powershell
$env:MINGW32 = 'C:\Users\rdr-ryzen\AppData\Local\Programs\LW2000-MinGW32\mingw32\bin'
cmd /c build.bat
cmd /c build.bat prueba
cmd /c build.bat prueba programa
```

Reglas:

- `build.bat prueba actualizar` solo en rama de trabajo y revisando
  `git diff` antes de aceptar patrones.
- Cada fase termina con commit en `main`, push y actualizacion de
  `v2.0-beta` con nota CRLF, binarios y SHA-256.
- `recursos/ascii.png` sigue sin rastrear y fuera de commits/releases.

---

## 11. Definicion de terminado

El plan se da por terminado cuando:

1. Insertar un mapa no crea ningun personaje.
2. Un actor sin control asignado no responde a teclado, raton ni mando.
3. Un jugador es visible como Control + Acciones + Funciones sobre un Actor.
4. Se pueden crear hasta 4 jugadores con dispositivos distintos.
5. Los bloques se ejecutan automaticamente, sin conversion manual.
6. El mismo proyecto da la misma traza en Play, `game.exe` y BIOS salvo
   funciones marcadas no portables.
7. LOGIC1, LOGIC2, LOGIC3, indices de menu y CRC de patrones antiguos siguen
   intactos.
8. `build.bat prueba` pasa las 24 suites.
9. La documentacion explica el modelo con ejemplos.
10. No se han anadido dependencias externas de ejecucion.

Fin del plan propuesto.
