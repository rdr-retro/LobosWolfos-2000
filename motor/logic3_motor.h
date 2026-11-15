/* logic3_motor.h - lector minimo de LOGIC3 para el motor (fase 25).
 *
 * El editor tiene el lector completo en src/logic3.c (modelo, validacion,
 * escritor y conversiones). El motor solo necesita una parte para la fase
 * 25: la cabecera del proyecto, el mapa de la primera escena y los scripts
 * `codigo.lobo` de los eventos `fotograma`. Las escenas, entidades y el
 * resto del modelo llegan en la fase 26.
 *
 * El lector es tolerante: las secciones que no entiende las ignora y los
 * errores de sintaxis devuelven 0 con "linea N: motivo" en `error`. El
 * texto de los bloques acepta los mismos escapes que LOGIC3 (\" \\ \n) y
 * las lineas CRLF de Windows.
 */
#ifndef LOGIC3_MOTOR_H
#define LOGIC3_MOTOR_H

#define MOTOR_L3_SCRIPTS_MAX 64   /* scripts de fotograma por proyecto */
#define MOTOR_L3_CODIGO_MAX  2048 /* bytes de codigo por bloque */
#define MOTOR_L3_RUTA_MAX    260  /* ruta o nombre de mapa */
#define MOTOR_L3_NOMBRE_MAX  64   /* nombre del proyecto */
#define MOTOR_L3_ERROR_MAX   192  /* mensaje de error del lector */

/* Un script del proyecto: el codigo lobo de un bloque codigo.lobo que
 * cuelga de un evento fotograma. */
typedef struct MotorL3Script {
    int bloque;  /* id del bloque en el LOGIC3 (para la traza) */
    int objeto;  /* id del objeto dueno del evento */
    int evento;  /* id del evento */
    int orden;   /* campo orden del bloque */
    int capa;    /* capa del objeto, para el orden de dibujo futuro */
    char codigo[MOTOR_L3_CODIGO_MAX + 1];
} MotorL3Script;

/* Proyecto minimo que entiende el motor en la fase 25. */
typedef struct MotorL3 {
    char nombre[MOTOR_L3_NOMBRE_MAX];
    int ancho, alto;
    int limitar_cuadros;
    int fps;
    int velocidad;
    unsigned int fondo;
    char mapa[MOTOR_L3_RUTA_MAX]; /* mapa de la primera escena, "" si no hay */
    MotorL3Script scripts[MOTOR_L3_SCRIPTS_MAX];
    int num_scripts;
    int num_escenas;
    int num_objetos;
    int num_bloques; /* bloques vistos, informativo para --volcar */
} MotorL3;

/* Carga el LOGIC3. Devuelve 1 si compila; si no, 0 y el motivo en
 * `error`. `ruta` puede ser relativa al directorio actual. */
int motor_l3_cargar(const char *ruta, MotorL3 *p, char *error, int tam_error);

/* Ultimo error del lector (para depuracion). */
const char *motor_l3_error(void);

#endif /* LOGIC3_MOTOR_H */
