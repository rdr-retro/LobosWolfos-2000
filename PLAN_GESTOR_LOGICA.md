# Plan: gestor de logica como constructor de juegos tipo Scratch

Estado: en curso  
Version: 2.0 (21-11-2026)  
Punto de partida: commit `86eb58d`  
Alcance: crear el constructor de juegos por entregas pequenas, visibles y
verificables. No sustituye a la documentacion de usuario actual.

---

## 0. Idea central

El gestor de logica deja de ser un lienzo donde solo se sueltan mapas y
funciones sueltas. Pasa a ser un **constructor de programas**:

- El **mapa insertado es solo el escenario**. No crea personajes.
- Un **actor** es una pieza independiente con traje, posicion, variables y
  scripts.
- Un **jugador no es un tipo de sprite**: es un **perfil de control** formado
  por un actor asignado, un dispositivo, un esquema de entradas y un conjunto
  de funciones de accion.
- Los **bloques** viven dentro de ese actor o de esa funcion, se encajan como
  en Scratch y se compilan a lobo 3.
- Si no existe una pieza `Control` asignada a un actor, **no hay jugador**: no
  aparece el lobito azul, no hay movimiento por teclado y la camara no sigue a
  nadie.

Regla que resume el plan:

> Insertar un mapa crea un escenario. Crear un jugador requiere insertar un
> actor y despues asignarle una pieza de control con sus acciones.

---

## 1. Estado actual

Ya cerrado y verificado:

- **Fase 46** (`5dedbfc`): modelo de programa y fin del jugador implicito. Los
  proyectos nuevos guardan `sistema.opciones` con `programa=1`,
  `jugador_implicito=0` y `auto_compilar=1`. Sin esa pieza el proyecto es legacy
  y mantiene el sprite clasico. `game.exe` y la vista previa comparten modelo.
- **Fase 47** (`86eb58d`): lienzo visible. Paleta de 31 piezas en siete grupos,
  tarjetas arrastrables con iman, enlaces por campo `enlace`, inspector,
  deshacer/rehacer y barra conmutadora. Estado propio en `g.programa_piezas`,
  guardado como bloques `sistema.pieza` con `ui=1`. Sin opcodes nuevos.
- **Fase 48** (esta entrega): el gestor abre directamente en `Programa`. La
  vista del gestor deja de depender de `sistema.opciones.programa`; abrir un
  proyecto legacy y entrar al gestor ya muestra la paleta, pero el LOGIC3 no se
  convierte en programa hasta que se guarda una pieza. Anadidos `entrada_gestor`
  y `arrastre_gestor`, y flag de prueba `--compatibilidad`.

Patrones que cambiaron por la barra nueva o por la entrada visible:
`regresion/gestor`, `regresion/ejemplo` y `plantillas/canonico`. Sus volcados
siguien iguales.

---

## 2. Contrato inmutable

1. **Formatos intactos:** ni gramatica LOGIC3, ni CRC, ni lectores/escritores
   LOGIC1/LOGIC2. Las piezas nuevas se guardan como bloques `sistema.*`.
2. **Indices intactos:** `ARCHIVO=1`, `MODO=2`, `AYUDA=9`. No hay menus
   superiores nuevos ni pestanas nuevas en el Estudio.
3. **Sin dependencias:** Win32/GDI puro; winmm se carga dinamicamente.
4. **Sin jugador implicito:** ningun elemento del lienzo crea actores,
   jugadores ni camaras por defecto.
5. **Vista distinta de modelo:** `g.programa_modo` es la vista del gestor;
   `sistema.opciones.programa` es una opcion del proyecto. Abrir el gestor no
   escribe en el proyecto.
6. **Append-only:** los bloques actuales no se reordenan. Anadir siempre al
   final de `DEFS`, `AYUDA_DEFS`, `DEFS_NO_BIOS` y del enum `DEF_*`.
7. **Determinismo:** toda opcion nueva se puede simular por CLI con `--captura`,
   `--volcar`, `--volcar-programa` y `--traza`.
8. **Una entrega, un cambio visible:** cada entrega termina con build, suite
   filtrada, suite completa, commit, push y release `v2.0-beta`.

---

## 3. Regla de trabajo por entregas

Cada entrega siguiente sigue siempre este esqueleto:

1. **Una sola opcion nueva.** Si hace falta algo mas, se separa en otra
   entrega.
2. **Se ve sin abrir menus nuevos.** Boton, panel o estado dentro del gestor.
3. **No cambia el runtime todavia.** Solo interfaz, volcado y persistencia,
   salvo que la entrega diga lo contrario.
4. **Patrones propios o ampliacion controlada.** Nunca `build.bat prueba
   actualizar` global; se usa `-Filtro caso -Actualizar`.
5. **Criterio de salida verificable.** Una frase que se pueda comprobar con un
   case de prueba y una sonda de pixel o de texto.
6. **Documentacion y release.** README, pruebas/README y nota CRLF con los
   hashes nuevos.

---

## 4. Hoja de ruta incremental

### Entrega E0 - Entrada visible al constructor (hecha)

- **Anade:** `programa_vista_preparar()`, entrada por `--gestor` y por menu en
  `Programa`, `--compatibilidad` para la vista antigua, casos
  `entrada_gestor` y `arrastre_gestor`.
- **Se ve:** al abrir `MODO > Gestor de logica` aparece la paleta con las
  primeras piezas; se puede arrastrar una sin flags especiales.
- **Prueba:** `pruebas/programa.ps1 -Filtro *gestor`.
- **Criterio:** `--gestor` sin `--programa` produce
  `programa_modo=1 programa_piezas=0` y una tarjeta nueva al arrastrar.
- **Fuera de alcance:** ejecucion de las piezas.

### Entrega E1 - Buscador de la paleta

- **Anade:** campo de texto arriba de la paleta que filtra por nombre y tipo.
- **Se ve:** al escribir `tec`, solo aparecen Teclado y sus variantes.
- **Prueba:** caso `paleta_buscar`, con `--clics` sobre el campo y sondas.
- **Criterio:** el numero de filas visibles baja y el volcado incluye el texto
  filtrado.
- **Fuera de alcance:** grupos nuevos ni cambios en el inspector.

### Entrega E2 - Duplicar y borrar con confirmacion

- **Anade:** botones `DUPLICAR` y `BORRAR` en el inspector, ademas de las teclas
  ya existentes.
- **Se ve:** duplicar coloca la copia desplazada y seleccionada; borrar pide
  confirmacion corta en la barra.
- **Prueba:** caso `inspector_acciones`, con volcado y BMP.
- **Criterio:** `programa_piezas` aumenta en uno y luego vuelve al valor
  inicial sin dejar enlaces rotos.

### Entrega E3 - Enlaces mas claros

- **Anade:** resaltado del destino valido mientras se arrastra un enlace y
  cancelacion con Escape.
- **Se ve:** puerto destino en amarillo cuando puede soltarse, rojo cuando no.
- **Prueba:** ampliar `ui_programa` con dos sondas nuevas.
- **Criterio:** el enlace solo se crea si el destino acepta la entrada.
- **Fuera de alcance:** reglas ni compilacion.

### Entrega E4 - Panel de problemas

- **Anade:** lista pequeña con las piezas `sin destino` y click para saltar a
  ellas.
- **Se ve:** la insignia roja sigue en la tarjeta y el panel enumera el mismo
  aviso.
- **Prueba:** caso `problemas_pieza`.
- **Criterio:** una pieza con salida sin enlace aparece en los dos sitios y
  desaparece al enlazarla.

### Entrega E5 - Recordar la vista del lienzo

- **Anade:** guardar en el proyecto el scroll de la paleta y el estado de los
  grupos plegados, ademas de la posicion de las tarjetas.
- **Se ve:** al reabrir el proyecto, la paleta y los grupos quedan como se
  dejaron.
- **Prueba:** caso `vista_persistida`, con dos ejecuciones encadenadas.
- **Criterio:** `programa_grupos` y `programa_scroll` sobreviven a guardar y
  abrir.

### Entrega E6 - Plantilla de jugador explicita

- **Anade:** boton `JUGADOR` que crea Actor + Teclado + Acciones + Funciones
  enlazados, sin tocar el mapa.
- **Se ve:** cuatro tarjetas nuevas con tres enlaces y ningun aviso rojo.
- **Prueba:** caso `plantilla_jugador`.
- **Criterio:** el mapa sigue siendo solo Escenario y no aparece ningun sprite
  implicito al ejecutar.
- **Fuera de alcance:** movimiento real; eso llega en E9.

### Entrega E7 - Importar desde compatibilidad

- **Anade:** boton en la vista LOGIC2 que convierte los elementos antiguos en
  piezas, previa confirmacion.
- **Se ve:** cada mapa pasa a Escenario y cada objeto con control pasa a Actor.
- **Prueba:** caso `importar_legacy`.
- **Criterio:** el LOGIC2 original no se sobreescribe hasta confirmar.

### Entrega E8 - Campos editables en el inspector

- **Anade:** editar valor y valor2 con texto libre validado (longitud y
  caracteres permitidos).
- **Se ve:** el campo cambia de color mientras se edita y vuelve al guardar.
- **Prueba:** caso `inspector_campos`, con `--clics` y `--volcar-programa`.
- **Criterio:** los valores viajan al bloque `sistema.pieza` sin romper el CRC.

### Entrega E9 - Primer runtime explicito

- **Anade:** el motor interpreta Actor + Control + Accion de teclado.
- **Se ve:** solo se mueve el actor con control asignado.
- **Prueba:** casos `control_teclado` y `actor_sin_control`.
- **Criterio:** dos actores, solo el enlazado al teclado cambia de posicion.
- **Fuera de alcance:** raton avanzado, mando y reglas.

### Entrega E10 - Ratón y mando

- **Anade:** dispositivos raton y mando con mapeo de entradas.
- **Se ve:** campos nuevos en el inspector y traza de pulsaciones.
- **Prueba:** casos `control_raton` y `control_mando`.
- **Criterio:** las coordenadas de raton respetan la camara; el mando avisa si
  la plataforma no lo soporta.

### Entrega E11 - Funciones y reglas

- **Anade:** piezas Regla y Funcion con parametros y retorno.
- **Se ve:** una tecla dispara una accion que llama a una funcion.
- **Prueba:** caso `funcion_llamada`, con `--traza`.
- **Criterio:** la traza muestra tecla, accion, funcion y retorno en orden.

### Entrega E12 - Compilacion automatica

- **Anade:** compilar a lobo 3 al pulsar Play, al guardar y al empaquetar.
- **Se ve:** aviso de compilacion en la barra del gestor.
- **Prueba:** caso `auto_compilar` y comparacion con la compilacion manual.
- **Criterio:** la traza del editor, `game.exe` y BIOS coincide salvo avisos de
  no portabilidad.

---

## 5. Compatibilidad y riesgos

| Riesgo | Mitigacion |
|---|---|
| Cambio de CRC en patrones LOGIC3 | No tocar el escritor; piezas nuevas solo al guardar desde el lienzo |
| Lectores antiguos | `tipo` arbitrario ya se preserva; `codigo.lobo` sigue intacto |
| Proyecto legacy convertido por accidente | Solo se marca `programa=1` cuando hay piezas guardadas |
| `game.exe` viejo ignorando piezas | Documentar que el runtime nuevo se requiere desde E9 |
| Desfase gestor/LOGIC3 | `g.programa_piezas` es la fuente de la vista; LOGIC2 es proyeccion |
| Desfase Play/game.exe | Ambos cargan `proyecto.logic3` cuando existe |
| Reordenacion accidental de bloques | Anadir siempre al final de los arrays paralelos |
| Paleta inusable | Buscador en E1 antes de pasar de ~40 piezas |
| Disco lleno en pruebas BIOS | Limpiar solo `lw2000-*` de `%TEMP%` entre pasadas |
| Dependencia externa | winmm se carga dinamicamente, como ya hace el audio |

---

## 6. Verificacion

```powershell
$env:MINGW32 = 'C:\Users\rdr-ryzen\AppData\Local\Programs\LW2000-MinGW32\mingw32\bin'
cmd /c build.bat
cmd /c build.bat prueba programa
cmd /c build.bat prueba regresion
cmd /c build.bat prueba
```

Reglas:

- `build.bat prueba actualizar` solo en rama de trabajo y revisando
  `git diff`; mejor usar `-Filtro caso -Actualizar`.
- Cada entrega termina con commit en `main`, push y actualizacion de
  `v2.0-beta` con nota CRLF, binarios y SHA-256.
- `recursos/ascii.png` sigue sin rastrear y fuera de commits/releases.

---

## 7. Documentacion de cada entrega

- `README.md`: una frase visible por entrega, sin abrir un capitulo nuevo.
- `pruebas/README.md`: caso o casos anadidos y patrones que generan.
- `PLAN_GESTOR_LOGICA.md`: marcar la entrega como hecha y anadir la siguiente.
- Nota de release: fase, commit, cambio visible, hashes de los dos binarios.

---

## 8. Definicion de terminado

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
8. `build.bat prueba` pasa todas las suites.
9. La documentacion explica el modelo con ejemplos.
10. No se han anadido dependencias externas de ejecucion.

Fin del plan.
