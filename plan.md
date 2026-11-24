# Plan de rediseño del gestor de lógica

Estado: en curso — cuarta entrega avanzada
Alcance: exclusivamente el gestor de lógica y su integración con escenas,
objetos, scripts, recursos y runtime.
Regla: cada fase se implementa y verifica por separado; no se retira el gestor
visual antiguo hasta que la migración y las pruebas correspondientes pasen.

### Progreso registrado

- [x] Base ProjectModel y lector de montaje existente comprobados.
- [x] Puente CLI `.log -> ProjectModel -> LOGIC3` añadido como primera entrega.
- [x] Formato `LOG 2` mínimo para escena, jerarquía, recursos y scripts.
- [x] Validación de nodos padre-hijo y asociaciones de scripts.
- [x] Adaptador incluido en `build.bat` y `build.ps1`.
- [x] Prueba `modelo_logic3` añadida y superada.
- [x] Prueba `modelo_escena` añadida y superada.
- [x] LOGIC3 generado cargado y renderizado por `motor/game.exe`.
- [x] Modelo jerárquico mínimo de escenas y nodos padre-hijo en `LOG 2`.
- [x] Vista Inicio inicial con árbol, lienzo e inspector basada en IDs del modelo.
- [x] Prueba `inicio_modelo`: carga, selección de nodo e inspector verificados.
- [x] LOG 2 jerárquico convertido a escenas, objetos, eventos y bloques `codigo.lobo` reales.
- [x] Asociación de scripts externos a escenas y objetos verificada en el runtime.
- [x] Escena `inicial 1` ordenada como primera escena ejecutable.
- [x] Propagar `padre` al runtime para transformaciones globales y creación por niveles.
- [x] Convertir los nodos `camera2d` en objetivos de cámara del runtime.
- [x] Hacer editable el inspector de Inicio sobre el `ProjectModel` real.
- [x] Guardar el modelo editado como `LOG 2` determinista y recargarlo sin perder referencias.
- [x] Verificar el flujo completo con `inicio_editar`: clic, teclado, guardado, LOGIC3 y runtime.
- [x] Añadir comandos de creación, borrado y reparentado desde Inicio.
- [x] Añadir transacciones de deshacer/rehacer para cambios del inspector.
- [ ] Retirada gradual de la UI visual antigua.

---

## 1. Decisión principal

El gestor de lógica visual actual se retirará como sistema de programación.

No se va a mejorar el lienzo de piezas ni se va a sustituir por otro sistema de
bloques. El nuevo gestor será un espacio de trabajo basado en:

- escenas;
- objetos jerárquicos;
- propiedades;
- recursos;
- scripts de texto;
- eventos y asociaciones de scripts;
- validación;
- ejecución y depuración.

La programación del juego se escribirá en scripts. El editor visual servirá para
organizar el mundo del juego y asociar scripts a escenas u objetos, no para
construir programas arrastrando cuadrados.

La distinción fundamental será:

    Inicio = escena y jerarquía de objetos
    Scripts = código de texto asociado a elementos
    Utilidades = pintura, mapas, recursos y herramientas auxiliares
    Motor = runtime que ejecuta escenas y scripts

---

## 2. Objetivo final del gestor

Desde la sección Inicio, el usuario debe poder:

1. Crear o abrir un proyecto.
2. Crear escenas.
3. Abrir una escena.
4. Ver su árbol de objetos.
5. Crear objetos y objetos hijos.
6. Colocar los objetos en un lienzo.
7. Asignar imágenes, mapas, trajes, colisiones y cámaras.
8. Editar sus propiedades en el inspector.
9. Crear un script desde el árbol de archivos del proyecto.
10. Asociar el script a una escena u objeto.
11. Elegir el evento que dispara el script.
12. Ejecutar la escena en preview.
13. Ver errores de scripts, referencias y recursos.
14. Cambiar de escena desde un script.
15. Guardar el proyecto sin perder la jerarquía ni las asociaciones.

Ejemplo del resultado esperado:

    Inicio
    └── Nivel 1
        ├── Mapa
        │   ├── Árbol 1
        │   └── Casa
        ├── Player
        │   └── Cámara
        └── Enemigo

    Scripts/
    ├── player.lobo       -> Player / fotograma
    ├── player_input.lobo -> Player / tecla
    ├── puerta.lobo       -> Puerta / tocar
    └── niveles.lobo      -> Nivel 1 / escena

---

## 3. Análisis del sistema actual

### 3.1 Componentes reutilizables

El motor ya tiene una base importante que debe conservarse:

| Componente actual | Uso actual | Decisión |
|---|---|---|
| src/logic3.c | lector, escritor, CRC y migración LOGIC3 | conservar como adaptador y formato normalizado |
| src/logic3.h | escenas, objetos, eventos, bloques y funciones | ampliar con cuidado, sin romper lectura existente |
| src/sistema.c | controles, acciones y reglas sistema.* | mantener para compatibilidad y migrar gradualmente |
| motor/lobo.c | intérprete clásico de scripts | conservar para proyectos antiguos |
| VM de LOGIC3 | scripts con tipos, funciones, eventos y estado | convertirla en runtime principal |
| motor/juego.c | entidades, eventos, escenas, física, HUD y ciclo | reutilizar y separar de Win32 |
| motor/logic3_motor.c | carga de escenas, objetos, scripts y sistema | adaptar al modelo jerárquico |
| motor/activo.c | resolución de mapas, imágenes y archivos | centralizar como registro de recursos |
| motor/render.c | trajes, animación, cámara, capas y blits | conectar a nodos de escena |
| motor/entidades.c | instancias y ciclo de vida | usar como runtime de objetos |
| motor/eventos.c | despacho de eventos | reutilizar como despachador de scripts |
| src/documento.c | documento de pintura y capas | mover a Utilidades, no eliminar |
| src/pintura.c | edición de texturas | mover a Utilidades, no eliminar |
| src/mapa.c | edición de mapas .mav | mover a Utilidades, no eliminar |
| src/compilar.c | paquetes y exportación | adaptar al proyecto de escenas y scripts |
| pruebas/ | pruebas de VM, escenas, entidades, render y exportación | conservar y reorganizar |

### 3.2 Componentes del gestor visual antiguo

Estos archivos representan el flujo que se quiere retirar como autoría
principal:

- src/programa.c: paleta, tarjetas, enlaces, inspector y piezas sistema.pieza.
- src/bloques.c: modelo de bloques visuales.
- src/bloques_ui.c: dibujo e interacción del lienzo de bloques.
- src/compilador_bloques.c: generación de código desde bloques.
- La entrada de menú MODO > Gestor de lógica cuando abre el lienzo de piezas.
- El concepto de conectar acciones por puertos visuales para definir un juego.

No se deben borrar en la primera fase. Se deben aislar, marcar como legado y
dejar de utilizarse para proyectos nuevos. Durante la migración podrán servir
para abrir proyectos anteriores o convertirlos a scripts.

### 3.3 Riesgos actuales

1. src/programa.c mezcla la representación visual con el significado de
   ejecución.
2. Las piezas sistema.* representan escena, actor, control, acción, función y
   recurso como tarjetas independientes, pero no forman un árbol común.
3. Los bloques visuales y los scripts codigo.lobo son dos caminos diferentes.
4. El runtime conoce escenas, objetos LOGIC3, transformaciones padre-hijo y
   objetivos de cámara derivados de `camera2d`.
5. El gestor actual puede asociar movimiento y control sin que exista una
   instancia clara en una escena.
6. Mapas, imágenes, trajes y scripts se resuelven desde carpetas distintas sin
   un registro único de recursos.
7. Preview, game.exe y kernel pueden divergir si cada uno construye sus datos
   desde una representación diferente.
8. Retirar la UI visual sin crear primero el modelo de scripts dejaría el motor
   sin una forma usable de crear juegos.

---

## 4. Arquitectura propuesta

    Proyecto
    ├── Configuración
    ├── Escenas
    │   └── Escena
    │       └── Nodo raíz
    │           ├── Mapa
    │           ├── Player
    │           │   └── Cámara
    │           └── Enemigo
    ├── Scripts
    │   ├── player.lobo
    │   └── enemigo.lobo
    ├── Recursos
    │   ├── textures/
    │   ├── maps/
    │   ├── costumes/
    │   └── audio/
    └── Exportación

    Editor Inicio
          |
    ProjectModel
          |
    Parser .log + registro de recursos + validador
          |
    LOGIC3 normalizado / runtime común
          |
    Preview / game.exe / kernel

### 4.1 Modelo de proyecto

Crear una representación de proyecto separada de la interfaz. Como mínimo debe
contener:

    Project
      metadata
      settings
      scenes[]
      nodes[]
      scripts[]
      assets[]
      bindings[]
      diagnostics[]
      editor_state

Cada nodo tendrá:

- id estable;
- parent_id;
- scene_id;
- type;
- name;
- position;
- rotation;
- scale;
- visible;
- layer;
- properties específicas;
- script_bindings[];
- source con archivo y línea cuando provenga de .log.

El parent_id será la relación principal. No se representará la jerarquía
mediante tarjetas que tengan que conectarse manualmente.

### 4.2 Tipos iniciales de nodo

La primera versión no debe inventar decenas de tipos:

| Tipo | Función |
|---|---|
| root | raíz de una escena |
| map | mapa .mav dentro de la escena |
| node2d | contenedor transformable |
| sprite | imagen o animación visible |
| actor | objeto con entidad, colisión y scripts |
| camera2d | cámara hija de un nodo |
| collision | forma o área de colisión |
| audio | sonido o música asociado |
| hud | elementos de interfaz de la escena |
| instance | instancia de una escena reutilizable |

Todos los tipos deben compartir nombre, transformación, visibilidad, padre,
capa y scripts. Las propiedades especiales viven en su inspector.

### 4.3 Escena y objeto

- Una escena es un recurso compuesto que tiene un nodo raíz.
- Un mapa es un nodo dentro de una escena, no la escena completa por obligación.
- Un objeto puede contener otros objetos.
- Un objeto hijo hereda la transformación del padre según sus propiedades.
- Un Player puede tener una Camera2D como hija.
- Un objeto puede tener varios scripts y un script puede asociarse a varios
  objetos si el modelo lo permite.
- Una escena puede instanciar otra escena sin copiar sus datos.
- El runtime genera entidades a partir de los nodos instanciados de la escena.

---

## 5. Scripts como sistema de programación

### 5.1 Principio

El script será un archivo de texto independiente. El editor no generará el
programa a partir de bloques. Solo proporcionará creación, edición básica,
asociación, validación y depuración.

El lenguaje inicial será lobo, porque ya existen intérprete, VM, funciones,
eventos, variables, mensajes, escenas, HUD, audio, guardado y colisiones.

No se debe crear un lenguaje nuevo antes de aprovechar el actual.

### 5.2 Ubicación y asociación

Los scripts se verán en el árbol de archivos del proyecto y podrán ser:

- scripts de escena;
- scripts de objeto;
- scripts de evento;
- scripts reutilizables;
- scripts de biblioteca con funciones;
- scripts independientes llamados por otros scripts.

La asociación se expresará mediante datos, no mediante arrastre de bloques:

    script id 40 archivo "Scripts/player.lobo"
    asociar script 40 nodo 12 evento "inicio"
    asociar script 40 nodo 12 evento "tecla"

Eventos iniciales soportados:

    inicio
    fotograma
    tecla
    soltar_tecla
    tocar
    recibir
    crear
    destruir
    escena_entrar
    escena_salir

### 5.3 APIs que se reutilizarán

Se conservarán las APIs actuales para:

- mover y leer propiedades del nodo actual;
- consultar y modificar variables;
- detectar colisiones;
- crear, clonar, mover y borrar entidades;
- enviar mensajes;
- cambiar de escena;
- controlar cámara y transiciones;
- dibujar HUD;
- reproducir audio;
- guardar datos;
- llamar funciones reutilizables.

Si una API usa un nombre ambiguo, se añadirá un alias documentado y se
mantendrá el nombre antiguo durante la migración.

### 5.4 Cambio de escena desde script

El runtime debe permitir una operación equivalente a:

    si toca_tag("salida")
        cambiar_escena("Nivel 2")
    fin

El cambio no se aplicará a mitad de la iteración de entidades. Se pondrá en una
cola de transición y se ejecutará al final del fotograma, manteniendo la regla
que ya usa motor/juego.c para cambios pendientes.

---

## 6. Formato .log como montaje del proyecto

El nuevo .log no describirá conexiones de piezas visuales. Describirá el
proyecto, las escenas, la jerarquía, los recursos y las asociaciones de scripts.

### 6.1 Ejemplo de formato objetivo

    LOG 2

    proyecto nombre "Primer juego" ancho 640 alto 480 fps 60

    recurso id 1 tipo "texture" ruta "tiles/jugador.png"
    recurso id 2 tipo "map" ruta "maps/nivel1.mav"
    script id 10 ruta "Scripts/player.lobo"
    script id 11 ruta "Scripts/niveles.lobo"

    escena id 100 nombre "Nivel 1" inicial 1
    nodo id 101 escena 100 padre 0 tipo "root" nombre "Nivel 1"
    nodo id 102 escena 100 padre 101 tipo "map" nombre "Mapa" recurso 2
    nodo id 103 escena 100 padre 101 tipo "actor" nombre "Player" x 80 y 80
    nodo id 104 escena 100 padre 103 tipo "sprite" nombre "Sprite" recurso 1
    nodo id 105 escena 100 padre 103 tipo "camera2d" nombre "Camara"

    asociar script 10 nodo 103 evento "inicio"
    asociar script 10 nodo 103 evento "fotograma"
    asociar script 11 escena 100 evento "escena_entrar"

### 6.2 Reglas del .log

1. Los IDs son estables y únicos.
2. La jerarquía se expresa mediante padre.
3. La escena de cada nodo se expresa mediante escena.
4. Los recursos se referencian por ID y conservan una ruta relativa legible.
5. Los scripts se referencian por ID y ruta relativa.
6. La programación no depende de coordenadas del lienzo.
7. Las coordenadas de editor pertenecen a editor_state.
8. No se permiten rutas absolutas ni salir de la carpeta del proyecto.
9. El orden de las líneas no cambia el resultado.
10. Un archivo mal formado indica línea, propiedad e ID afectado.
11. El escritor produce una salida determinista para facilitar diffs.
12. Un recurso faltante es un error; nunca se sustituye silenciosamente por un
    rectángulo o una imagen inventada.

### 6.3 Compatibilidad con LOGIC3

Durante la transición:

    .log -> ProjectModel -> validar -> LOGIC3 normalizado -> runtime

src/logic3.c seguirá siendo el lector/escritor de LOGIC3. El adaptador deberá
transformar:

- escenas del modelo en escenas LOGIC3;
- nodos con entidad en objetos LOGIC3;
- scripts asociados en eventos y bloques codigo.lobo;
- recursos en trajes, mapas y activos resolubles;
- propiedades compatibles en campos existentes.

Si alguna propiedad jerárquica no cabe todavía en LOGIC3, la conversión debe
emitir un diagnóstico y no perderla silenciosamente. Primero se ampliará el
adaptador; solo después se considerará cambiar el formato normalizado.

En esta entrega ya se materializan escenas, objetos, trajes, eventos y bloques
`codigo.lobo`, y se comprueba su ejecución en `motor/game.exe`. El campo
`padre` se conserva en LOGIC3, se valida contra ciclos y escenas, y el runtime
calcula la posición global de cada hijo a partir de su transformación local.
El nodo `camera2d` usa su padre como objetivo de seguimiento y admite `zoom`
1, 2 o 4; los límites avanzados de cámara siguen siendo una propiedad futura
del inspector.

---

## 7. Nueva interfaz del gestor

### 7.1 Sección Inicio

La sección principal se llamará Inicio. No será un lienzo de bloques.

#### Panel izquierdo: árbol de escena y proyecto

Debe mostrar dos zonas relacionadas:

    Proyecto
    ├── Escenas
    │   ├── Nivel 1
    │   └── Nivel 2
    ├── Scripts
    │   ├── player.lobo
    │   └── niveles.lobo
    └── Recursos
        ├── Texturas
        ├── Mapas
        └── Audio

Al abrir una escena, el árbol puede enfocarse en su jerarquía:

    Nivel 1
    └── Player
        ├── Sprite
        ├── Collision
        └── Camera2D

No habrá árboles independientes de actores, cámaras y recursos que oculten la
relación real. El árbol de escena será la autoridad para padre e hijo. El árbol
del proyecto será la autoridad para archivos y recursos.

#### Zona central: lienzo de escena

Debe permitir:

- seleccionar nodos;
- mover, rotar y escalar;
- insertar recursos como nodos;
- crear hijos del objeto seleccionado;
- visualizar cámara, colisiones y capas;
- cambiar de escena;
- mostrar advertencias sobre recursos faltantes;
- hacer zoom y desplazamiento;
- respetar transformaciones padre-hijo.

No debe permitir crear lógica conectando tarjetas.

#### Panel derecho: inspector

El inspector dependerá del nodo seleccionado:

- nombre e ID;
- padre y escena;
- transformación;
- visibilidad y capa;
- recurso visual;
- colisión;
- variables;
- scripts asociados;
- eventos disponibles;
- propiedades específicas del tipo.

También debe mostrar el origen: archivo .log, línea e ID. Si una propiedad
viene de un script o de un recurso, debe indicarlo claramente.

#### Zona inferior: diagnóstico y consola

Debe mostrar:

- errores de .log;
- recursos faltantes;
- scripts que no compilan;
- asociaciones inválidas;
- nodos sin padre válido;
- límites alcanzados;
- mensajes de preview y runtime.

### 7.2 Sección Utilidades

Utilidades será independiente del sistema principal de escenas. Incluirá:

- Pintar;
- Tiles;
- Mapas;
- Importar/exportar imágenes;
- Trajes y animaciones;
- Audio;
- Fuentes;
- herramientas futuras.

Una utilidad puede crear o modificar recursos, pero no debe convertirse en una
segunda forma de organizar la escena. Al guardar un recurso, debe registrarlo
en el proyecto y actualizar las referencias mediante asset_registry.

### 7.3 Sección Scripts

Debe existir una vista clara para:

- árbol de archivos;
- editor de texto;
- lista de asociaciones;
- eventos del nodo seleccionado;
- errores de compilación;
- funciones y símbolos;
- botón de abrir la escena u objeto asociado.

No debe existir una paleta de bloques como sustituto.

---

## 8. Flujo de usuario objetivo

    Inicio
      -> Nueva escena
      -> Insertar mapa
      -> Crear Node2D o Actor
      -> Insertar Sprite como hijo
      -> Elegir textura desde Recursos
      -> Crear Camera2D como hija del Player
      -> Crear script desde Scripts
      -> Asociar script a Player
      -> Elegir eventos
      -> Escribir lobo
      -> Validar
      -> Preview
      -> Guardar .log y LOGIC3
      -> Exportar

El jugador no aparecerá por insertar un mapa. El usuario creará explícitamente
el nodo Player, su sprite, su colisión, sus scripts y su cámara.

---

## 9. Cambios por componente

### 9.1 src/programa.c

Tratamiento futuro:

1. Congelar nuevas funciones del lienzo de piezas.
2. Extraer la carga de proyectos antiguos a un migrador.
3. Retirar la paleta y tarjetas de la navegación principal.
4. Sustituir estado programa_piezas por selección de nodo y escena.
5. Conservar temporalmente lectura de sistema.pieza para importar.
6. Mostrar aviso de migración al abrir un proyecto antiguo.
7. Eliminarlo de la ruta de proyectos nuevos cuando el importador sea fiable.

No se debe reutilizar la estructura de tarjetas como modelo de escena. La escena
necesita parent_id, instancias, transformaciones y escena.

### 9.2 src/bloques.c, src/bloques_ui.c y src/compilador_bloques.c

- Dejar de mostrarlos en proyectos nuevos.
- Mantener lectura y conversión de proyectos antiguos durante la migración.
- Ofrecer exportación de bloques antiguos a codigo.lobo cuando sea posible.
- No añadir nuevos bloques ni nuevas categorías.
- Mover la compilación válida al pipeline de scripts.
- Retirarlos del menú cuando ejemplos y pruebas hayan terminado la migración.

El compilador de bloques será compatibilidad temporal, no autoría.

### 9.3 src/logic3.c y src/logic3.h

- Mantener el lector y escritor canónicos.
- Añadir el adaptador del modelo de escenas y nodos.
- Añadir serialización padre-hijo si el formato actual no basta.
- Mantener CRC y versiones existentes.
- Añadir migración explícita en vez de cambiar el significado de campos antiguos.

### 9.4 src/sistema.c y src/sistema.h

- Mantener la lectura de sistema.* para compatibilidad.
- Dejar de usar sistema.pieza como modelo primario.
- Convertir controles y acciones antiguas en asociaciones de scripts o eventos.
- Mantener jugador_implicito=0 en proyectos nuevos.
- No crear actores por cargar un mapa.

### 9.5 src/app.c y navegación

- Reorganizar menús alrededor de Inicio, Scripts y Utilidades.
- Hacer que las acciones llamen comandos del modelo.
- Sacar reglas de escena y scripts de callbacks de clic.
- Unificar selección, inspector, guardado y diagnósticos.
- Conservar pintura, mapas y exportación, cambiando su ubicación de navegación.

### 9.6 Motor

Reutilizar:

- motor/lobo.c y la VM;
- motor/eventos.c;
- motor/entidades.c;
- motor/render.c;
- motor/activo.c;
- motor/logic3_motor.c;
- motor/juego.c.

Adaptar:

- carga de nodos padre-hijo;
- cálculo de transformaciones globales;
- instanciación de escenas;
- asociaciones de scripts externos;
- resolución de recursos por registro;
- cámara hija del objeto objetivo;
- cambio de escena desde script;
- trazas y errores.

No duplicar reglas entre preview, game.exe y kernel. El runtime común debe
ejecutar el mismo modelo normalizado.

---

## 10. Plan de implementación por fases

No se debe modificar el código antes de aprobar este plan. Después de aprobarlo,
las fases se ejecutarán en este orden.

### Fase 0 — Inventario y línea base

Objetivo: entender y congelar el sistema antes de retirar nada.

- Documentar menús y flags relacionados con el gestor.
- Registrar qué archivos lee y escribe cada flujo.
- Crear fixtures de proyecto visual antiguo, LOGIC3 con scripts y .log actual.
- Ejecutar pruebas de LOGIC3, bloques, compilador, escenas, entidades, render y
  exportación.
- Añadir volcados de escenas, objetos, scripts y referencias.
- No cambiar el comportamiento.

Criterio de salida: se puede abrir un proyecto antiguo y reproducir su resultado
actual con una prueba automatizada.

### Fase 1 — Especificación del modelo

Objetivo: fijar contratos antes de construir la interfaz.

- Definir ProjectModel, Scene, Node, Script, Asset y Binding.
- Definir IDs, parent_id, scene_id y transformaciones.
- Definir tipos iniciales de nodo.
- Definir eventos y funciones de script.
- Definir errores y límites.
- Especificar la versión nueva de .log.
- Especificar la conversión a LOGIC3.

Criterio de salida: existe un contrato y un ejemplo válido escrito a mano sin
depender del lienzo visual.

### Fase 2 — Parser, escritor y validador .log

Objetivo: que el montaje funcione sin abrir el editor.

- Leer proyecto, recursos, escenas, nodos y scripts.
- Validar IDs, padres, escenas, rutas y asociaciones.
- Guardar determinísticamente.
- Emitir errores con archivo, línea, ID y propiedad.
- Añadir modo estricto para exportación.
- Añadir modo de compatibilidad para proyectos antiguos.
- Añadir CLI de lectura y validación.

Criterio de salida: un .log válido se carga y se vuelve a guardar sin cambios
semánticos; uno inválido produce diagnósticos reparables.

### Fase 3 — Registro de recursos

Objetivo: unir texturas, mapas, trajes, audio y scripts.

- Crear registro único de assets.
- Normalizar rutas relativas.
- Registrar recursos creados desde Utilidades.
- Resolver miniaturas y archivos para editor y motor.
- Mostrar usos de un recurso.
- Bloquear exportación de recursos obligatorios faltantes.

Criterio de salida: una textura pintada aparece en Recursos, se asigna a un nodo
y se renderiza en preview y motor.

### Fase 4 — Árbol jerárquico y modelo de nodos

Objetivo: tener escenas editables sin programación visual.

- Crear escena y nodo raíz.
- Crear, duplicar, mover y borrar nodos.
- Cambiar padre con validación de ciclos.
- Heredar transformaciones.
- Crear instancias de escena.
- Mostrar árbol y selección por ID.
- Persistir la jerarquía en .log.

Criterio de salida: Player contiene Sprite, Collision y Camera2D, y el movimiento
del padre actualiza visualmente a los hijos.

### Fase 5 — Editor Inicio e inspector

Objetivo: reemplazar el lienzo del gestor como espacio principal.

- Crear layout Inicio: árbol, lienzo, inspector y diagnósticos.
- Insertar recursos como nodos.
- Editar transformaciones y propiedades.
- Mostrar referencias, origen y errores.
- Añadir deshacer/rehacer por comandos.
- Retirar la entrada visual de bloques de proyectos nuevos.

Criterio de salida: se puede construir una escena completa sin abrir el antiguo
gestor de piezas.

### Fase 6 — Editor y asociaciones de scripts

Objetivo: programar mediante archivos de texto.

- Crear scripts desde el árbol de proyecto.
- Abrir y editar código.
- Asociar script a escena u objeto.
- Elegir evento.
- Mostrar scripts en el inspector.
- Validar y compilar script.
- Mapear errores a archivo y línea.
- Abrir el objeto desde una asociación y viceversa.

Criterio de salida: player.lobo asociado a Player puede leer entrada, mover el
actor y producir una traza sin bloques visuales.

Estado: completada. Inicio crea scripts desde el arbol, abre un editor de texto
multilinea, guarda y compila con errores mapeados a archivo y linea, asocia por
arbol o lienzo en dos pasos, permite elegir el evento y navega entre la
asociacion, el script y el objeto. La suite `programa.ps1` cubre la creacion,
la edicion, la navegacion y la traza del runtime sin bloques visuales.

### Fase 7 — Runtime jerárquico y escenas

Objetivo: ejecutar el mismo árbol que se edita.

- Instanciar el árbol de la escena.
- Calcular transformaciones globales.
- Crear entidades desde nodos actor.
- Hacer que cámara, colisión y sprite respeten el árbol.
- Cargar scripts externos con sus asociaciones.
- Mantener cola de cambios de escena.
- Ejecutar escena_entrar y escena_salir.
- Compartir reglas entre preview y game.exe.

Criterio de salida: el juego carga una escena, ejecuta scripts, cambia a otra y
conserva la misma jerarquía en preview y ejecutable.

Estado: completada. Preview y `game.exe` comparten la instanciación del árbol,
las transformaciones globales y la cola de escenas. La suite
`runtime_jerarquico.ps1` congela la traza, el volcado y el BMP de dos escenas,
con `jerarquia_inicial` y `dos_escenas`, y comprueba que ambos ejecutables
producen exactamente el mismo resultado.

### Fase 8 — Utilidades y recursos conectados

Objetivo: separar herramientas auxiliares sin aislarlas del proyecto.

- Mover Pintar, Tiles y Mapas a Utilidades.
- Actualizar el registro al guardar cada recurso.
- Permitir arrastrar un recurso desde el árbol a Inicio.
- Mostrar referencias y advertir antes de borrar.
- Mantener .mav, PNG, BMP, WAV y paquetes compatibles.

Criterio de salida: todas las utilidades alimentan el mismo proyecto y no crean
copias invisibles de recursos.

Estado: completada. El árbol de Inicio lleva un panel `UTILIDADES` con
`PINTAR`, `TILES` y `MAPAS`; abre cada herramienta sin desplazar los índices
`ARCHIVO=1`, `MODO=2` y `AYUDA=9`. `ProjectModel` reutiliza la entidad de un
recurso por su ruta y `inicio_registrar_recurso()` la conecta con el proyecto
abierto al guardar PNG, BMP, tiles, `.mav`, WAV o `.wolfo`. Un clic sobre un
recurso del árbol y un arrastre hasta un nodo escribe la referencia `recurso`
sin crear personaje, jugador ni movimiento. El inspector muestra las
referencias entrantes y el primer `BORRAR` avisa cuando existen: solo se
confirma con un segundo clic. La suite `utilidades.ps1` congela cuatro casos
(`utilidades_panel`, `registro_recursos`, `arrastrar_recurso` y
`borrar_con_referencias`), incluidas cinco sondas de píxel del panel. Los
formatos LOGIC1/LOGIC2/LOGIC3 y sus CRC no cambian.

### Fase 9 — Migración del gestor visual antiguo

Objetivo: retirar programación visual sin romper proyectos existentes.

- Convertir piezas sistema.* a escenas, nodos y asociaciones de scripts.
- Convertir sistema.accion.* a código lobo equivalente.
- Convertir funciones y parámetros a funciones de script.
- Avisar de conversiones no exactas.
- Mantener copia de seguridad del archivo original.
- Abrir el resultado migrado en Inicio.
- Mantener el lector antiguo solo para importar.

Criterio de salida: los ejemplos actuales se abren como escenas y scripts y su
comportamiento coincide con el original dentro de lo soportado.

### Fase 10 — Retirada de la UI visual

Objetivo: eliminar el flujo visual de proyectos nuevos.

- Quitar paleta, tarjetas y enlaces de la navegación principal.
- Quitar la creación de programas por bloques.
- Quitar la entrada de bloques de proyectos nuevos.
- Mantener importación/exportación de legado durante el periodo acordado.
- Eliminar compatibilidad solo cuando todos los fixtures pasen.
- Actualizar manuales, README y ayuda.

Criterio de salida: no existe una ruta normal que construya lógica mediante
arrastre de piezas o conexión de cuadrados.

### Fase 11 — Exportación, depuración y estabilización

Objetivo: cerrar el ciclo de desarrollo.

- Empaquetar .log, LOGIC3, scripts y recursos usados.
- Añadir trazas de escena, nodo, script, evento y línea.
- Añadir pausa, paso de script y variables observadas.
- Bloquear exportaciones con errores.
- Verificar Windows, preview y BIOS según sus capacidades.
- Limpiar compatibilidad histórica y documentar APIs públicas.

Criterio de salida: un proyecto puede abrirse en otra carpeta, validarse,
ejecutarse y exportarse sin depender del editor original.

---

## 11. Matriz de responsabilidades

| Función | Dueño nuevo | Reutiliza | No depende de |
|---|---|---|---|
| Crear escena | scene_service | logic3.c | paleta de piezas |
| Crear nodo | scene_service | estructuras LOGIC3 | callbacks aislados |
| Cambiar padre | scene_graph | undo/redo | conexiones visuales |
| Pintar textura | Utilidades/Pintar | documento.c, pintura.c | runtime |
| Crear mapa | Utilidades/Mapas | mapa.c | scripts |
| Registrar recurso | asset_registry | activo.c | ruta fija por menú |
| Crear script | script_service | editor de texto actual | bloques |
| Asociar script | script_service | eventos existentes | nombre ambiguo |
| Validar | validator | parser y límites | render |
| Ejecutar preview | runtime_adapter | motor | estado de UI |
| Cambiar escena | runtime | cola existente | clic del editor |
| Exportar | build_service | compilar.c | variables de una vista |
| Importar legado | legacy_importer | programa.c, sistema.c | modelo nuevo |

Cada botón nuevo debe poder señalar una fila de esta tabla. Si no puede, no
tiene una responsabilidad clara y no debe añadirse.

---

## 12. Validación y errores

### Sintaxis

- cabecera y versión;
- propiedades válidas;
- números y textos;
- comillas y rutas;
- scripts legibles.

### Estructura

- IDs únicos;
- padres existentes;
- sin ciclos en la jerarquía;
- nodos pertenecientes a una escena;
- scripts y recursos existentes;
- eventos válidos para el tipo de nodo;
- escenas iniciales válidas.

### Ejecución

- scripts compilables;
- recursos decodificables;
- mapas con tiles resolubles;
- cámara con objetivo válido;
- colisiones compatibles;
- APIs soportadas por el target;
- límites de entidades, scripts y memoria.

Cada diagnóstico incluirá:

    severity
    code
    message
    file
    line
    node_id
    script_id
    property
    suggestion

Códigos iniciales:

    LOG-NODE-001    padre inexistente
    LOG-NODE-002    ciclo de jerarquía
    LOG-ASSET-001   recurso no encontrado
    LOG-SCRIPT-001  script no compila
    LOG-BIND-001    evento no válido para el nodo
    LOG-SCENE-001   escena inicial inexistente

---

## 13. Compatibilidad y seguridad de la migración

1. No borrar archivos de proyectos antiguos.
2. Migrar a una copia o a un archivo nuevo.
3. Guardar un informe de conversiones exactas y aproximadas.
4. Mantener intactos mapas, PNG, BMP, WAV y scripts que ya funcionen.
5. No cambiar el significado de LOGIC3 sin versión y migrador.
6. No retirar el runtime antiguo hasta que los casos de compatibilidad pasen.
7. No esconder errores usando sprites, actores o scripts implícitos.
8. Mantener jugador_implicito=0 en proyectos nuevos.
9. No mezclar estado de UI con estado del juego.
10. No duplicar una regla de ejecución en editor, preview y motor.

---

## 14. Pruebas obligatorias

### Modelo y formato

- crear escena y nodo raíz;
- crear árbol padre-hijo;
- impedir ciclos;
- guardar y recargar IDs;
- reordenar líneas sin cambiar resultado;
- detectar recursos faltantes;
- detectar scripts no compilables;
- migrar un .log antiguo;
- migrar un proyecto con sistema.pieza.

### Editor

- crear, seleccionar, mover y borrar nodos;
- reparentar un nodo;
- insertar textura en el lienzo;
- editar propiedades;
- asociar y desasociar scripts;
- deshacer y rehacer una transacción;
- ver un error en árbol, inspector y panel inferior.

### Runtime

- cargar escena con mapa;
- cargar actor con sprite pintado;
- heredar transformación del padre;
- seguir al Player con una cámara hija;
- ejecutar inicio y fotograma;
- responder a tecla, tocar y recibir mensaje;
- cambiar de escena al final del fotograma;
- mantener la misma captura en preview y game.exe.

### Retirada visual

- ningún proyecto nuevo crea sistema.pieza;
- no se abre paleta de bloques desde Inicio;
- los proyectos antiguos muestran migración;
- los bloques antiguos no se pierden al importar;
- la documentación ya no presenta bloques como flujo principal.

---

## 15. Criterios de terminado

El rediseño estará terminado cuando:

- Inicio sea el editor de escenas y objetos, no un lienzo de programación visual;
- exista un árbol único de objetos con relaciones padre-hijo;
- lienzo, árbol e inspector editen el mismo modelo;
- los scripts sean archivos de texto independientes;
- los scripts puedan asociarse a escenas y objetos;
- un script pueda cambiar de escena;
- texturas y mapas se usen como recursos registrados;
- preview y motor ejecuten la misma representación;
- los errores indiquen archivo, línea, nodo y solución sugerida;
- los proyectos antiguos puedan migrarse sin perder funcionalidades;
- el gestor visual de piezas quede fuera del flujo normal;
- no se creen actores o imágenes implícitos para ocultar referencias faltantes;
- Utilidades esté separada de Inicio pero conectada al registro de recursos;
- el juego se pueda crear, validar, probar y exportar editando .log y scripts.

La prueba final será:

    crear proyecto
    -> crear Nivel 1
    -> introducir mapa
    -> crear Player
    -> introducir Sprite, Collision y Camera2D como hijos
    -> pintar y registrar jugador.png desde Utilidades
    -> crear player.lobo
    -> asociarlo a Player
    -> mover al jugador mediante teclado
    -> cambiar de escena desde un script
    -> validar
    -> ejecutar preview
    -> exportar y abrir fuera del editor

Si para completar ese flujo hay que abrir el antiguo lienzo de piezas o editar
datos internos a mano, el rediseño todavía no está terminado.

---

## 16. Primera implementación después de la aprobación

La primera entrega de código debe ser solo una base verificable:

1. Crear estructuras mínimas de ProjectModel, Scene, Node, Script y Binding.
2. Implementar parser y escritor de un .log mínimo.
3. Cargar una escena con Player, Sprite y Camera2D jerárquicos.
4. Mostrar árbol e inspector sin retirar todavía el gestor antiguo.
5. Validar rutas y scripts.
6. Ejecutar una captura de preview con el modelo nuevo.
7. Añadir pruebas antes de migrar controles o retirar menús.

No se deben cambiar colores, eliminar módulos ni borrar archivos de legado en
esa primera entrega. Primero debe demostrarse que escenas, jerarquía, scripts y
runtime están conectados correctamente.
