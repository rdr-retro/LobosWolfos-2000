/* logic3_motor.h - lector minimo de LOGIC3 para el motor (fases 25 a 27).
 *
 * El editor tiene el lector completo en src/logic3.c (modelo, validacion,
 * escritor y conversiones). El motor necesita la cabecera del proyecto,
 * las escenas, los objetos con sus variables y trajes, y los scripts
 * `codigo.lobo` de todos los eventos (fase 27). Las escenas y los objetos
 * alimentan el modulo de entidades (fase 26) y el planificador de eventos.
 *
 * El lector es tolerante: las secciones que no entiende las ignora y los
 * errores de sintaxis devuelven 0 con "linea N: motivo" en `error`. El
 * texto de los bloques acepta los mismos escapes que LOGIC3 (\" \\ \n) y
 * las lineas CRLF de Windows.
 */
#ifndef LOGIC3_MOTOR_H
#define LOGIC3_MOTOR_H

#define MOTOR_L3_SCRIPTS_MAX 256  /* scripts de evento por proyecto */
#define MOTOR_L3_CODIGO_MAX  2048 /* bytes de codigo por bloque */
#define MOTOR_L3_RUTA_MAX    260  /* ruta o nombre de mapa */
#define MOTOR_L3_NOMBRE_MAX  64   /* nombre de proyecto, escena u objeto */
#define MOTOR_L3_ERROR_MAX   192  /* mensaje de error del lector */
#define MOTOR_L3_ESCENAS_MAX 64   /* escenas por proyecto */
#define MOTOR_L3_OBJETOS_MAX 128  /* objetos por proyecto */
#define MOTOR_L3_VARS_MAX    64   /* variables por objeto */
#define MOTOR_L3_GLOBALES_MAX 128  /* variables globales */
#define MOTOR_L3_LISTAS_MAX   32   /* listas globales */
#define MOTOR_L3_LISTA_ELEMS_MAX 256 /* elementos por lista */
#define MOTOR_L3_TEXTOS_MAX   32   /* textos globales compartidos */
#define MOTOR_L3_TEXTO_MAX    64   /* bytes por texto */
#define MOTOR_L3_TRAJES_MAX  32   /* trajes por objeto */

/* Un script del proyecto: el codigo lobo de un bloque codigo.lobo que
 * cuelga de un evento. `tipo` usa los valores de LOGIC3 (1 inicio,
 * 2 fotograma, 3 tecla, 4 recibir, 5 tocar, 6 cada, 7 clonar, 8 escena).
 * Los parametros del evento viajan con el script para que el planificador
 * no tenga que volver a mirar el proyecto. */
typedef struct MotorL3Script {
    int bloque;  /* id del bloque en el LOGIC3 (para la traza) */
    int objeto;  /* id del objeto dueno del evento */
    int evento;  /* id del evento */
    int orden;   /* campo orden del bloque */
    int capa;    /* capa del objeto, para el orden de dibujo futuro */
    int tipo;    /* tipo de evento (los valores de LOGIC3) */
    char nombre[MOTOR_L3_NOMBRE_MAX]; /* tecla, mensaje o tag de tocar */
    int flanco;      /* tecla: 0 mantener, 1 pulsar, 2 soltar */
    int objeto_tocar;/* tocar: id de objeto o -1 */
    int cada;        /* cada: fotogramas entre disparos */
    int escena;      /* escena: id de la escena que dispara */
    char codigo[MOTOR_L3_CODIGO_MAX + 1];
} MotorL3Script;

/* Una escena del proyecto. */
typedef struct MotorL3Escena {
    int id;
    char nombre[MOTOR_L3_NOMBRE_MAX];
    char mapa[MOTOR_L3_RUTA_MAX];
    int ancho, alto;
    unsigned int fondo;
} MotorL3Escena;

/* Una variable inicial, de objeto o global. `tipo` usa los mismos
 * codigos que LOGIC3 (0 entero, 1 booleano, 2 texto). En las variables de
 * texto `valor` es el indice en MotorL3.textos y la cadena tambien se
 * conserva en `texto` para depuracion. */
typedef struct MotorL3Variable {
    char nombre[MOTOR_L3_NOMBRE_MAX];
    int tipo;
    int valor;
    char texto[MOTOR_L3_TEXTO_MAX];
} MotorL3Variable;

/* Lista global con sus valores iniciales ya convertidos. */
typedef struct MotorL3Lista {
    char nombre[MOTOR_L3_NOMBRE_MAX];
    int capacidad;
    int tipo;
    int num_elementos;
    int elementos[MOTOR_L3_LISTA_ELEMS_MAX];
} MotorL3Lista;

/* Un traje del objeto. Los seis primeros campos son el formato original;
 * el resto son extensiones opcionales de la fase 30 (valores por defecto:
 * un cuadro, sin flip, sin rotacion y un fotograma por cuadro). */
typedef struct MotorL3Traje {
    int indice;
    char imagen[MOTOR_L3_RUTA_MAX];
    int ancho, alto;
    int pivote_x, pivote_y;
    int cuadros;        /* cuadros de una tira horizontal */
    int cuadro_ancho;   /* 0 = deducir de ancho / cuadros */
    int cuadro_alto;    /* 0 = deducir de alto */
    int flip_x, flip_y;
    int rotacion;       /* 0, 90, 180 o 270 grados */
    int velocidad;      /* fotogramas por cuadro */
} MotorL3Traje;

/* Camara del proyecto (linea opcional `camara`). objetivo 0 sigue al
 * sprite del jugador; si no hay limites se usa el tamano del proyecto. */
typedef struct MotorL3Camara {
    int objetivo;       /* id de objeto LOGIC3, 0 = jugador */
    int x, y, ancho, alto; /* limites del mundo; 0 = sin limites */
    int zoom;           /* 1, 2 o 4 */
    int definida;       /* 1 si el LOGIC3 trae la linea */
} MotorL3Camara;

/* Un objeto LOGIC3: el tipo de las entidades que se instancian. */
typedef struct MotorL3Objeto {
    int id;
    char nombre[MOTOR_L3_NOMBRE_MAX];
    int escena;
    int x, y, w, h;
    int visible;
    int capa;
    MotorL3Variable variables[MOTOR_L3_VARS_MAX];
    int num_variables;
    MotorL3Traje trajes[MOTOR_L3_TRAJES_MAX];
    int num_trajes;
} MotorL3Objeto;

/* Proyecto minimo que entiende el motor. */
typedef struct MotorL3 {
    char nombre[MOTOR_L3_NOMBRE_MAX];
    int ancho, alto;
    int limitar_cuadros;
    int fps;
    int velocidad;
    unsigned int fondo;
    MotorL3Camara camara;      /* fase 30, valores por defecto si falta */
    char mapa[MOTOR_L3_RUTA_MAX]; /* mapa de la primera escena, "" si no hay */
    int escena_inicial;           /* id de la escena que arranca (0 si no hay) */
    MotorL3Escena escenas[MOTOR_L3_ESCENAS_MAX];
    int num_escenas;
    MotorL3Objeto objetos[MOTOR_L3_OBJETOS_MAX];
    int num_objetos;
    /* Fase 28: variables y listas globales del LOGIC3. */
    MotorL3Variable variables[MOTOR_L3_GLOBALES_MAX];
    int num_variables;
    MotorL3Lista listas[MOTOR_L3_LISTAS_MAX];
    int num_listas;
    char textos[MOTOR_L3_TEXTOS_MAX][MOTOR_L3_TEXTO_MAX];
    int num_textos;
    MotorL3Script scripts[MOTOR_L3_SCRIPTS_MAX];
    int num_scripts;
    int num_bloques; /* bloques vistos, informativo para --volcar */
} MotorL3;

/* Carga el LOGIC3. Devuelve 1 si compila; si no, 0 y el motivo en
 * `error`. `ruta` puede ser relativa al directorio actual. */
int motor_l3_cargar(const char *ruta, MotorL3 *p, char *error, int tam_error);

/* Ultimo error del lector (para depuracion). */
const char *motor_l3_error(void);

#endif /* LOGIC3_MOTOR_H */
