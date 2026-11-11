/* lobo.h - lenguaje de los botones FUNCIONES (fase 8).
 *
 * lobo es el lenguaje propio de LobosWolfos 2000: el codigo que se
 * escribe dentro de los botones FUNCIONES del gestor de logica y que
 * viaja como texto en el .logic del proyecto (formato LOGIC2). El mismo
 * texto lo interpretan los dos destinos: el motor de 32 bits (game.exe,
 * este archivo) y el kernel loboswolfos (puerto en kernel/src/lobo.rs),
 * asi que un boton escrito en el editor corre igual en Windows y en la
 * BIOS.
 *
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
 * `lobo_parsear` compila el texto a una lista de instrucciones en
 * memoria; `lobo_ejecutar` recorre esa lista con una pila de valores.
 * Cada ejecucion tiene un presupuesto de instrucciones (LOBO_PASOS_MAX)
 * para que un bucle sin salida no cuelgue el fotograma.
 */
#ifndef LOBO_H
#define LOBO_H

/* Limites compartidos con el puerto del kernel (lobo.rs): el texto y
 * los programas caben iguales en los dos destinos. */
#define LOBO_INSTS_MAX   256 /* instrucciones por programa */
#define LOBO_PILA_MAX    32  /* valores apilados por expresion */
#define LOBO_LOCALES_MAX 16  /* variables locales por script */
#define LOBO_NOMBRE_MAX   32 /* largo de un nombre de variable */
#define LOBO_ERROR_MAX    96 /* largo del mensaje de error de parsear */
#define LOBO_PASOS_MAX    200000 /* pasos por ejecucion (por fotograma) */

/* Indices de las variables del juego dentro del espacio comun
 * (0..3 globales, 4..19 locales del script). */
#define LOBO_VAR_X         0
#define LOBO_VAR_Y         1
#define LOBO_VAR_VEL       2
#define LOBO_VAR_FOTOGRAMA 3
#define LOBO_VAR_LOCALES   4 /* primera local */

/* Bits de tecla de LOBO_TECLA: los mismos MOTOR_ENT_* de motor.h. */
#define LOBO_TECLA_IZQUIERDA 0x01
#define LOBO_TECLA_DERECHA   0x02
#define LOBO_TECLA_ARRIBA    0x04
#define LOBO_TECLA_ABAJO     0x08
#define LOBO_TECLA_SALTO     0x10
#define LOBO_TECLA_CERRAR    0x20

/* Codigo de operacion de cada instruccion. El numero de cada uno es
 * parte del formato: el kernel (lobo.rs) usa la misma numeracion. */
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
    LOBO_POP          /* descarta el tope (sentencia-expr suelta) */
};

/* Una instruccion del programa compilado. */
typedef struct LoboInst {
    unsigned char op; /* uno de los LOBO_* de arriba */
    int valor;       /* constante, indice o destino segun el op */
} LoboInst;

/* Programa compilado: lista plana de instrucciones. */
typedef struct LoboPrograma {
    LoboInst insts[LOBO_INSTS_MAX];
    int largo;       /* instrucciones usadas */
} LoboPrograma;

/* Lo que el juego expone a los scripts: posicion del sprite, velocidad,
 * contador de fotogramas, bits de entrada y la prueba de colision de la
 * caja del sprite contra el mapa (la caja la aporta el juego). */
typedef struct LoboEstado {
    int x, y, vel, fotograma;
    unsigned entrada; /* bits LOBO_TECLA_* */
    int (*choca)(void *ctx, int x, int y);
    void *ctx;
} LoboEstado;

/* Estado persistente de un script entre fotogramas. */
typedef struct LoboMaquina {
    int locales[LOBO_LOCALES_MAX];
    int cerrar;  /* 1 si el script llamo a cerrar() */
    int agotado; /* 1 si se quedo sin presupuesto de pasos */
} LoboMaquina;

/* Compila el texto de un script. Devuelve 1 si compila; si no, 0 y
 * `error` lleva el mensaje "linea N: ...". `programa` queda vacio. */
int lobo_parsear(const char *texto, LoboPrograma *programa,
                char *error, int tam_error);

/* Ejecuta un programa ya compilado contra el estado del juego. La
 * maquina conserva las locales y las peticiones de cerrar. */
void lobo_ejecutar(const LoboPrograma *programa, LoboEstado *estado,
                   LoboMaquina *maquina);

/* Reinicia una maquina (locales a cero) para un uso nuevo. */
void lobo_maquina_reiniciar(LoboMaquina *maquina);

#endif /* LOBO_H */
