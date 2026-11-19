/* lobo.h - lenguaje de los botones FUNCIONES (fase 8) y lobo 3 (fase 25).
 *
 * Fase 37: se anaden teclas con flancos y estado de raton al final del
 * estado y al final de la numeracion de opcodes, para no romper los
 * programas ya compilados ni las trazas de las suites anteriores.
 *
 * lobo es el lenguaje propio de LobosWolfos 2000: el codigo que se
 * escribe dentro de los botones FUNCIONES del gestor de logica y que
 * viaja como texto en el .logic del proyecto (formato LOGIC2) o en los
 * bloques codigo.lobo de un proyecto LOGIC3. El mismo texto lo
 * interpretan los dos destinos: el motor de 32 bits (game.exe, este
 * archivo) y el kernel loboswolfos (puerto en kernel/src/lobo.rs), asi
 * que un boton escrito en el editor corre igual en Windows y en la BIOS.
 *
 * ------------------------------------------------------------------
 * Modo clasico (LOGIC2, fase 8)
 * ------------------------------------------------------------------
 * El lenguaje (palabras en espanol, sin acentos, una sentencia por linea):
 *
 *   si <expr>            ... fin        condicional (con sino opcional)
 *   mientras <expr>       ... fin        bucle (con tope de pasos)
 *   <nombre> = <expr>                    asignacion
 *   <expr>                               suelta (p. ej. cerrar())
 *
 * Expresiones: numeros enteros, variables, parentesis, + - * / %,
 * comparaciones (== != < > <= >=), logica (& | no), y las teclas y
 * funciones del juego. El # empieza un comentario hasta el final de la
 * linea. Las variables del juego son x, y (posicion del sprite), vel
 * (velocidad) y fotograma (contador). Las teclas: tecla_izquierda,
 * tecla_derecha, tecla_arriba, tecla_abajo, tecla_salto y tecla_cerrar.
 * Las funciones integradas: choca(x, y) (colision de la caja del sprite
 * contra el mapa) y cerrar() (pedir terminar el juego). Cualquier otro
 * nombre asignado en el programa es una variable local del script: nace
 * a 0 y conserva su valor entre fotogramas (hasta 16 por script).
 *
 * ------------------------------------------------------------------
 * Modo VM (lobo 3, fase 25)
 * ------------------------------------------------------------------
 * El modo VM amplia el lenguaje sin cambiar nada del modo clasico:
 *
 *   esperar <expr>                  deja el script esperando N fotogramas
 *   detener                         termina este script
 *   detener todo                    pide terminar la escena
 *   clonar                          pide un clon (fase 26)
 *   mensaje "texto"                 emite un mensaje (fase 27)
 *   funcion <nombre>(<p1>, ...)     ... fin       funcion de usuario
 *   retornar [<expr>]               vuelve de la funcion con un valor
 *   lista <nombre>                  declara una lista de enteros
 *   $nombre                         variable global (persiste entre scripts)
 *   @nombre                         variable de la entidad actual
 *   llamar("nombre", <args...>)     llama a una funcion de usuario
 *   largo(l) / elemento(l, i) / contiene(l, v)
 *   fijar(l, i, v) / anadir(l, v) / insertar(l, i, v)
 *   borrar(l, i) / limpiar(l) / ordenar(l)
 *   mi.x / mi.y / mi.vida / mi.visible / mi.traje       (fase 26)
 *   mi.id / mi.escena / mi.ancho / mi.alto / mi.capa
 *   mi.variable("nombre")                               (fase 26)
 *   crear("tag", x, y) / borrar() / borrar(id)          (fase 26)
 *   mover(id, x, y)                                     (fase 26)
 *
 * Un script puede estar listo, esperando o terminado. `esperar` guarda
 * el contador y el puntero de instruccion, asi que el resto de scripts
 * del fotograma siguen corriendo. `detener` marca el script terminado;
 * en el evento de fotograma vuelve a empezar en el siguiente fotograma.
 * Las funciones tienen su propia pila de llamadas (32 niveles) y sus
 * parametros se guardan como locales que se restauran al volver.
 *
 * `lobo_parsear` compila el texto a una lista de instrucciones en
 * memoria; `lobo_ejecutar` recorre esa lista de una vez (modo clasico,
 * con un presupuesto de pasos LOBO_PASOS_MAX) y `lobo_vm_fotograma`
 * ejecuta la VM resumible hasta un evento o hasta agotar el
 * presupuesto LOBO_PASOS_VM_MAX, conservando el estado para el
 * siguiente fotograma.
 *
 * El programa guarda sus instrucciones en un buffer que crece con
 * realloc: hay que dejarlo a cero (static/global) o llamar antes a
 * lobo_programa_iniciar. lobo_programa_liberar suelta la memoria.
 */
#ifndef LOBO_H
#define LOBO_H

/* Modos de compilacion: el clasico mantiene los topes de LOGIC2 y no
 * entiende la sintaxis nueva; el modo VM usa los topes de la tabla 3.2
 * del plan maestro. */
#define LOBO_MODO_CLASICO 0
#define LOBO_MODO_VM      1

/* Limites compartidos con el puerto del kernel (lobo.rs): el texto y
 * los programas caben iguales en los dos destinos. */
#define LOBO_INSTS_CLASICO 256  /* instrucciones por programa clasico */
#define LOBO_INSTS_MAX     4096 /* tope del programa en modo VM */
#define LOBO_PILA_MAX      32   /* valores apilados por expresion (clasico) */
#define LOBO_PILA_VM_MAX   256  /* pila de la VM (tabla 3.2) */
#define LOBO_LOCALES_CLASICO 16 /* variables locales por script clasico */
#define LOBO_LOCALES_MAX   64   /* locales por script en modo VM */
#define LOBO_NOMBRE_MAX    32   /* largo de un nombre de variable */
#define LOBO_ERROR_MAX     96   /* largo del mensaje de error de parsear */
#define LOBO_PASOS_MAX     200000  /* pasos por ejecucion clasica */
#define LOBO_PASOS_VM_MAX  1000000 /* instrucciones por fotograma (VM) */

/* Topes del modo VM (tabla 3.2 del plan maestro). */
#define LOBO_FUNCIONES_MAX 64   /* funciones de usuario por programa */
#define LOBO_PARAMS_MAX    8    /* parametros por funcion */
#define LOBO_FRAMES_MAX    32   /* profundidad de llamadas */
#define LOBO_TEXTOS_MAX    64   /* textos distintos por programa */
#define LOBO_TEXTO_MAX     64   /* largo de un texto (64 bytes) */
#define LOBO_GLOBALES_MAX  128  /* variables globales */
#define LOBO_ENTIDAD_VARS_MAX 64 /* variables por entidad */
#define LOBO_LISTAS_MAX    32   /* listas por programa */
#define LOBO_LISTA_ELEMENTOS_MAX 256 /* elementos por lista */
#define LOBO_ENTIDAD_NOMBRES_MAX 64 /* nombres de variable de entidad por programa */
#define LOBO_TEXTOS_RUNTIME_MAX 32 /* textos creados en ejecucion (unir) */

/* Un texto en la pila es un handle con este bit alto. Asi los textos
 * conviven con los enteros en la misma pila de 32 bits: el indice son los
 * 30 bits bajos (0..63 del programa, 64..95 del runtime). */
#define LOBO_TEXTO_MARCA 0x40000000

/* Codigos de tipo en las tablas del programa (0 = entero por defecto). */
#define LOBO_TIPO_ENTERO   0
#define LOBO_TIPO_BOOLEANO 1
#define LOBO_TIPO_TEXTO    2
#define LOBO_TIPO_LISTA    3

/* Campos de `mi` (fase 26). Las variables de la entidad empiezan en
 * LOBO_MI_VARS para compartir un solo espacio de indices. */
#define LOBO_MI_X       0
#define LOBO_MI_Y       1
#define LOBO_MI_VIDA    2
#define LOBO_MI_VISIBLE 3
#define LOBO_MI_TRAJE   4
#define LOBO_MI_ID      5
#define LOBO_MI_ESCENA  6
#define LOBO_MI_ANCHO   7
#define LOBO_MI_ALTO    8
#define LOBO_MI_CAPA    9
#define LOBO_MI_VARS    1000 /* + slot de variable de la entidad */

/* Estados de un script en la VM. */
#define LOBO_VM_LISTO     0
#define LOBO_VM_ESPERANDO 1
#define LOBO_VM_TERMINADO 2

/* Resultado de lobo_paso. */
#define LOBO_PASO_SIGUE   0 /* instruccion ejecutada, se puede seguir */
#define LOBO_PASO_EVENTO  1 /* esperar/detener/fin: parar este fotograma */
#define LOBO_PASO_ERROR   2 /* pila mal, salto mal, sin frames... */

/* Indices de las variables del juego dentro del espacio comun
 * (0..3 globales del juego, 4..67 locales del script). */
#define LOBO_VAR_X         0
#define LOBO_VAR_Y         1
#define LOBO_VAR_VEL       2
#define LOBO_VAR_FOTOGRAMA 3
#define LOBO_VAR_LOCALES   4 /* primera local (4..67, como siempre) */
/* Fisica de la fase 29: indices altos para no desplazar las locales ni
 * cambiar el formato de los programas ya compilados. */
#define LOBO_VAR_VEL_X     1000
#define LOBO_VAR_VEL_Y     1001
#define LOBO_VAR_GRAVEDAD  1002
#define LOBO_VAR_REBOTE    1003
#define LOBO_VAR_FRICCION  1004
#define LOBO_VAR_EN_SUELO  1005

/* Bits de tecla de LOBO_TECLA: los mismos MOTOR_ENT_* de motor.h. */
#define LOBO_TECLA_IZQUIERDA 0x01
#define LOBO_TECLA_DERECHA   0x02
#define LOBO_TECLA_ARRIBA    0x04
#define LOBO_TECLA_ABAJO     0x08
#define LOBO_TECLA_SALTO     0x10
#define LOBO_TECLA_CERRAR    0x20

/* Codigo de operacion de cada instruccion. El numero de cada uno es
 * parte del formato: el kernel (lobo.rs) usa la misma numeracion y los
 * opcodes nuevos se anaden al final para no romperla. */
enum {
    LOBO_NUM = 0,     /* valor: constante a apilar */
    LOBO_VAR,         /* valor: indice de variable */
    LOBO_TECLA,       /* valor: bit de tecla */
    LOBO_CHOCAR,      /* colision: apila 0/1 */
    LOBO_CERRAR,      /* pedir cerrar el juego */
    LOBO_SUMA,
    LOBO_RESTA,
    LOBO_MULT,
    LOBO_DIV,         /* division entera; entre 0 da 0 */
    LOBO_MOD,         /* resto; entre 0 da 0 */
    LOBO_IGUAL,
    LOBO_DISTINTO,
    LOBO_MENOR,
    LOBO_MAYOR,
    LOBO_MENOR_IG,
    LOBO_MAYOR_IG,
    LOBO_Y,           /* y logico (ambos lados se evaluan) */
    LOBO_O,           /* o logico */
    LOBO_NO,          /* no logico */
    LOBO_ASIGNA,      /* valor: indice de variable */
    LOBO_SALTO,       /* valor: instruccion destino */
    LOBO_SALTO_FALSO, /* valor: destino si el tope de la pila es 0 */
    LOBO_POP,         /* descarta el tope (sentencia-expr suelta) */
    /* --- fase 25: lobo 3 --- */
    LOBO_ESPERAR,        /* espera N fotogramas (N del tope de la pila) */
    LOBO_LLAMAR,         /* valor: indice de funcion */
    LOBO_RETORNAR,       /* vuelve con el tope de la pila */
    LOBO_CARGAR_GLOBAL,  /* valor: indice de variable global */
    LOBO_GUARDAR_GLOBAL, /* valor: indice de variable global */
    LOBO_CARGAR_ENTIDAD, /* valor: indice de variable de entidad */
    LOBO_GUARDAR_ENTIDAD,/* valor: indice de variable de entidad */
    LOBO_LISTA_LARGO,    /* valor: lista; apila la longitud */
    LOBO_LISTA_ELEMENTO, /* valor: lista; apila elemento[i] */
    LOBO_LISTA_FIJAR,    /* valor: lista; fija [i] = v */
    LOBO_LISTA_ANADIR,   /* valor: lista; anade v al final */
    LOBO_LISTA_INSERTAR, /* valor: lista; inserta v en i */
    LOBO_LISTA_BORRAR,   /* valor: lista; quita el elemento i */
    LOBO_LISTA_TIENE,    /* valor: lista; apila 0/1 si contiene v */
    LOBO_LISTA_ORDENAR,  /* valor: lista; ordena de menor a mayor */
    LOBO_LISTA_LIMPIAR,  /* valor: lista; vacia la lista */
    LOBO_MENSAJE,        /* valor: indice de texto */
    LOBO_DETENER,        /* valor: 0 este script, 1 todo */
    LOBO_CLONAR,         /* pedir un clon (apila 0) */
    /* --- fase 26: entidades y `mi` --- */
    LOBO_CARGAR_MI,      /* valor: LOBO_MI_* (campo o VARS + slot) */
    LOBO_GUARDAR_MI,     /* valor: LOBO_MI_* (campo o VARS + slot) */
    LOBO_CREAR,          /* valor: indice del tag; x e y del tope */
    LOBO_BORRAR_ENTIDAD, /* valor: 0 la actual, 1 la del tope de la pila */
    LOBO_MOVER_ENTIDAD,  /* mueve id, x, y (los tres del tope) */
    /* --- fase 27: mensajes y escena --- */
    LOBO_ENVIAR,         /* valor: indice del texto; a todos los objetos */
    LOBO_ENVIAR_A,       /* valor: indice del texto; destino en `extra` */
    LOBO_REINICIAR_ESCENA, /* reinicia la escena actual */
    /* --- fase 28: built-ins numericos y de texto --- */
    LOBO_ABS,           /* abs(x): valor absoluto de x */
    LOBO_MIN,           /* min(a, b): minimo de a y b */
    LOBO_MAX,           /* max(a, b): maximo de a y b */
    LOBO_LIMITAR,       /* limitar(v, lo, hi): v limitado a [lo, hi] */
    LOBO_REDONDEAR,     /* redondear(v): entero mas cercano a v */
    LOBO_ALEATORIO,     /* aleatorio(a, b): valor entre a y b (determinista) */
    LOBO_TEXTO,         /* valor: indice de texto en el programa */
    LOBO_LONGITUD,      /* longitud(texto): cantidad de caracteres */
    LOBO_LETRA,         /* letra(texto, i): codigo del caracter i */
    LOBO_UNIR,          /* unir(a, b): concatena dos textos */
    /* --- fase 29: colisiones y fisica --- */
    LOBO_TOCA,          /* toca(id): 1 si la entidad actual toca a id */
    LOBO_TOCA_TAG,      /* valor: indice de texto; toca_tag("tag") */
    LOBO_DISTANCIA,     /* distancia(id): distancia Manhattan al id */
    LOBO_CAJA_DE,       /* caja_de(id, i): x/y/w/h de otra entidad */
    /* --- fase 37: entrada extendida --- */
    LOBO_TECLA_PULSADA, /* valor: ranura; apila 1 en el flanco pulsar */
    LOBO_TECLA_MANTENIDA, /* valor: ranura; apila 1 si se mantiene */
    LOBO_TECLA_SOLTADA, /* valor: ranura; apila 1 en el flanco soltar */
    LOBO_RATON_BOTON,   /* valor: 1 izq, 2 der, 3 medio; apila 0/1 */
    LOBO_RATON_X,       /* posicion x del cursor */
    LOBO_RATON_Y,       /* posicion y del cursor */
    LOBO_RATON_RUEDA,   /* pasos de rueda acumulados */
    LOBO_RATON_DENTRO,  /* 1 si el cursor esta sobre la ventana */
    /* --- fase 38: HUD, texto, UI y dialogos --- */
    LOBO_HUD_VIDAS,     /* pila: valor; apila el valor anterior */
    LOBO_HUD_PUNTOS,    /* pila: valor; apila el valor anterior */
    LOBO_HUD_DIALOGO,   /* pila: texto, linea; apila 0 */
    LOBO_HUD_TEXTO,     /* pila: escala, color, texto, y, x; apila 1 */
    LOBO_HUD_RECT,      /* pila: color, h, w, y, x; apila 1 */
    LOBO_HUD_BARRA,     /* pila: color, max, valor, h, w, y, x; apila 1 */
    LOBO_HUD_IMAGEN,    /* pila: y, x, id; apila 1 */
    LOBO_HUD_BOTON,     /* pila: etiqueta, h, w, y, x, id; apila 1 si clic */
    LOBO_HUD_PLANTILLA  /* pila: cual (1 pausa, 2 fin); apila 0 */
};

/* Una instruccion del programa compilado. */
typedef struct LoboInst {
    unsigned char op; /* uno de los LOBO_* de arriba */
    int valor;       /* constante, indice o destino segun el op */
    int extra;       /* segundo operando (destino de enviar_a) */
} LoboInst;

/* Funcion de usuario compilada. */
typedef struct LoboFuncion {
    char nombre[LOBO_NOMBRE_MAX];
    int inicio;                  /* primera instruccion del cuerpo */
    int params;                  /* parametros declarados */
    int param_local[LOBO_PARAMS_MAX]; /* indice local de cada parametro */
} LoboFuncion;

/* Programa compilado: lista plana de instrucciones ampliable. */
typedef struct LoboPrograma {
    LoboInst *insts; /* buffer de instrucciones (NULL hasta compilar) */
    int largo;       /* instrucciones usadas */
    int capacidad;   /* instrucciones reservadas */
    LoboFuncion funciones[LOBO_FUNCIONES_MAX];
    int num_funciones;
    char textos[LOBO_TEXTOS_MAX][LOBO_TEXTO_MAX];
    int num_textos;
    /* Nombres de variable de entidad compartidos por @nombre y
     * mi.variable("nombre") (fase 26). */
    char entidad_nombres[LOBO_ENTIDAD_NOMBRES_MAX][LOBO_TEXTO_MAX];
    int num_ent_nombres;
    /* Tipos declarados (fase 28). 0 = entero, para no cambiar el valor
     * por defecto de los programas antiguos. */
    char tipos_locales[LOBO_LOCALES_MAX];
    char tipos_globales[LOBO_GLOBALES_MAX];
    char tipos_entidad[LOBO_ENTIDAD_VARS_MAX];
    char tipos_listas[LOBO_LISTAS_MAX];
} LoboPrograma;

/* Una lista de enteros con capacidad fija. */
typedef struct LoboLista {
    int n;
    int elem[LOBO_LISTA_ELEMENTOS_MAX];
} LoboLista;

/* Memoria compartida por los scripts de un proyecto (variables
 * globales, variables de entidad, listas y mensajes). */
typedef struct LoboMemoria {
    int globales[LOBO_GLOBALES_MAX];
    int entidad_vars[LOBO_ENTIDAD_VARS_MAX];
    LoboLista listas[LOBO_LISTAS_MAX];
    char mensaje[LOBO_TEXTO_MAX];
    int mensajes; /* mensajes emitidos en total */
    int clones;   /* clones pedidos en total */
    /* Textos construidos por unir(). Se comparten entre scripts como
     * las listas; sus handles empiezan en LOBO_TEXTOS_MAX. */
    char textos[LOBO_TEXTOS_RUNTIME_MAX][LOBO_TEXTO_MAX];
    int num_textos;
} LoboMemoria;

/* Lo que el juego expone a los scripts: posicion del sprite, velocidad,
 * contador de fotogramas, bits de entrada y la prueba de colision de la
 * caja del sprite contra el mapa (la caja la aporta el juego). */
typedef struct LoboEstado {
    int x, y, vel, fotograma;
    unsigned entrada; /* bits LOBO_TECLA_* */
    LoboMemoria *memoria; /* compartida; puede ser NULL en modo clasico */
    int (*choca)(void *ctx, int x, int y);
    void *ctx;
    /* --- entidades (fase 26) ---
     * Callbacks opcionales: sin ellos (editor y --vm-test) `mi` cae en
     * x/y y en memoria->entidad_vars, y crear() devuelve -1. */
    int (*entidad_crear)(void *ctx, const char *tag, int x, int y);
    int (*entidad_borrar)(void *ctx, int id);
    int (*entidad_clonar)(void *ctx, int id);
    int (*entidad_mover)(void *ctx, int id, int x, int y);
    int (*entidad_leer)(void *ctx, int id, int campo);
    int (*entidad_escribir)(void *ctx, int id, int campo, int valor);
    void *ctx_entidades;
    /* --- eventos y mensajes (fase 27) ---
     * `destino` NULL o "" = a todos. Sin callback (editor y --vm-test),
     * enviar() solo deja el ultimo mensaje en memoria->mensaje. */
    void (*enviar)(void *ctx, int origen, const char *destino,
                   const char *texto);
    void (*reiniciar_escena)(void *ctx);
    void *ctx_eventos;
    /* --- colisiones y fisica (fase 29) --- */
    int (*toca)(void *ctx, int entidad, int id);
    int (*toca_tag)(void *ctx, int entidad, const char *tag);
    int (*distancia)(void *ctx, int entidad, int id);
    int (*caja_de)(void *ctx, int entidad, int id, int indice);
    void *ctx_colisiones;
    int vel_x, vel_y, gravedad, rebote, friccion, en_suelo;
    /* --- fase 37: entrada extendida, NULL en motores sin raton --- */
    struct MotorEntrada *entrada_ext;
    /* --- fase 38: HUD. Sin callbacks (editor y --vm-test) las llamadas no
     * hacen nada pero apilan un valor, asi la pila sigue cuadrando. Los
     * colores son u32 del framebuffer; se usa `unsigned` para no exigir
     * motor.h a quien solo incluye este archivo. */
    int (*hud_vidas)(void *ctx, int valor);
    int (*hud_puntos)(void *ctx, int valor);
    int (*hud_dialogo)(void *ctx, int linea, const char *texto);
    int (*hud_texto)(void *ctx, int x, int y, const char *texto,
                     unsigned color, int escala);
    int (*hud_rect)(void *ctx, int x, int y, int w, int h, unsigned color);
    int (*hud_barra)(void *ctx, int x, int y, int w, int h, int valor,
                     int max, unsigned color);
    int (*hud_imagen)(void *ctx, int id, int x, int y);
    int (*hud_boton)(void *ctx, int id, int x, int y, int w, int h,
                     const char *etiqueta);
    int (*hud_plantilla)(void *ctx, int cual);
    void *ctx_hud;
} LoboEstado;

/* Un frame de llamada: donde volver y el estado que hay que restaurar. */
typedef struct LoboFrame {
    int retorno;   /* pc al que volver */
    int pila_base; /* valores apilados al entrar (sin contar parametros) */
    int locales[LOBO_LOCALES_MAX];
} LoboFrame;

/* Estado persistente de un script entre fotogramas. */
typedef struct LoboMaquina {
    int locales[LOBO_LOCALES_MAX];
    int cerrar;  /* 1 si el script llamo a cerrar() */
    int agotado; /* 1 si se quedo sin presupuesto o de pila */
    /* --- VM resumible (fase 25) --- */
    int pc;          /* instruccion actual */
    int pila[LOBO_PILA_VM_MAX];
    int np;          /* valores apilados */
    LoboFrame frames[LOBO_FRAMES_MAX];
    int num_frames;
    int espera;      /* fotogramas que faltan; 0 = listo */
    int estado;      /* LOBO_VM_* */
    int detener_todo;/* 1 si el script pidio detener la escena */
    int clones;      /* clones pedidos por este script */
    int entidad;     /* id de la entidad actual (0 si no es de una) */
    int semilla;     /* semilla de aleatorio(), propia de cada script */
} LoboMaquina;

/* Prepara un programa para compilar (buffer a cero). */
void lobo_programa_iniciar(LoboPrograma *programa);

/* Suelta el buffer de instrucciones de un programa. */
void lobo_programa_liberar(LoboPrograma *programa);

/* Compila el texto de un script en el modo indicado. Devuelve 1 si
 * compila; si no, 0 y `error` lleva el mensaje "linea N: ...". El
 * programa debe estar a cero o reiniciado; al recompilar aprovecha el
 * buffer que ya tuviera. */
int lobo_compilar(const char *texto, LoboPrograma *programa, int modo,
                  char *error, int tam_error);

/* Compatibilidad: compila en modo clasico (LOGIC2). */
int lobo_parsear(const char *texto, LoboPrograma *programa,
                 char *error, int tam_error);

/* Ejecuta un programa ya compilado contra el estado del juego, de una
 * vez (modo clasico). La maquina conserva las locales y las peticiones
 * de cerrar. */
void lobo_ejecutar(const LoboPrograma *programa, LoboEstado *estado,
                   LoboMaquina *maquina);

/* Ejecuta una instruccion de la VM y devuelve LOBO_PASO_*. */
int lobo_paso(const LoboPrograma *programa, LoboEstado *estado,
              LoboMaquina *maquina);

/* Hooks opcionales del depurador (fase 34). Registrar NULL los desactiva. */
void lobo_depurador_instruccion(void (*fn)(int pc, int error));
void lobo_depurador_parada(int (*fn)(void));

/* Ejecuta el fotograma de un script en la VM: descuenta la espera,
 * reengancha el evento de fotograma si termino y corre hasta un evento
 * o hasta agotar LOBO_PASOS_VM_MAX. Devuelve 1 si llego a un evento,
 * 0 si agoto el presupuesto. Es la variante de los eventos que se repiten
 * cada fotograma (fotograma y tecla mantenida). */
int lobo_vm_fotograma(const LoboPrograma *programa, LoboEstado *estado,
                      LoboMaquina *maquina);

/* Lanza un evento de un disparo (inicio, recibir, tecla pulsada/soltada,
 * cada, tocar, clonar). Si el script termino, se reinicia para el nuevo
 * disparo; si esta esperando, no se toca y devuelve 0 (el planificador
 * puede posponer el mensaje). Devuelve 1 si llego a un evento. */
int lobo_vm_evento(const LoboPrograma *programa, LoboEstado *estado,
                   LoboMaquina *maquina);

/* Continua un script que estaba esperando, sin reiniciarlo si termino.
 * Lo usa el planificador para los eventos de un disparo: `esperar N`
 * sigue contando aunque no llegue otro disparo. Devuelve 1 si ejecuto
 * alguna instruccion. */
int lobo_vm_reanudar(const LoboPrograma *programa, LoboEstado *estado,
                     LoboMaquina *maquina);

/* Reinicia una maquina (locales a cero y VM lista) para un uso nuevo. */
void lobo_maquina_reiniciar(LoboMaquina *maquina);

#endif /* LOBO_H */
