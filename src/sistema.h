/* sistema.h - contrato del modelo de programa (fase 46).
 *
 * Las piezas del constructor se guardan como bloques LOGIC3 de tipo
 * `sistema.*`. No se anade sintaxis nueva al formato: la informacion viaja
 * en los campos del bloque y las referencias en las ranuras.
 *
 * Un proyecto sin `sistema.opciones` es legacy y mantiene el sprite
 * clasico. Un proyecto nuevo lleva `jugador_implicito=0`, por lo que un
 * mapa insertado crea solo el escenario.
 */
#ifndef SISTEMA_H
#define SISTEMA_H

#include "logic3.h"

#define SISTEMA_TIPO_OPCIONES   "sistema.opciones"
#define SISTEMA_TIPO_CONTROL    "sistema.control"
#define SISTEMA_TIPO_ACCION     "sistema.accion"
#define SISTEMA_TIPO_REGLA      "sistema.regla"

#define SISTEMA_JUGADORES_MAX   4
#define SISTEMA_ACCIONES_MAX    32
#define SISTEMA_REGLAS_MAX      32

#define SISTEMA_DISPOSITIVO_MAX 24
#define SISTEMA_ESQUEMA_MAX     24
#define SISTEMA_ENTRADA_MAX     32
#define SISTEMA_FUNCION_MAX     L3_NOMBRE_MAX
#define SISTEMA_ARGUMENTOS_MAX  L3_TEXTO_MAX

/* Dispositivos de un perfil de jugador. */
enum {
    SISTEMA_DISPOSITIVO_TECLADO = 0,
    SISTEMA_DISPOSITIVO_RATON = 1,
    SISTEMA_DISPOSITIVO_MANDO = 2
};

/* Flancos compartidos con los eventos de tecla de LOGIC3. */
enum {
    SISTEMA_FLANCO_MANTENER = 0,
    SISTEMA_FLANCO_PULSAR = 1,
    SISTEMA_FLANCO_SOLTAR = 2
};

typedef struct {
    int presente;
    int programa;
    int jugador_implicito;
    int auto_compilar;
    int linea;
} SistemaOpciones;

typedef struct {
    int jugador;               /* 1..4, 0 si la pieza no es valida */
    int actor;                 /* id de Objeto3 */
    int dispositivo;
    int esquema_activo;        /* 1 si se reconocio el esquema */
    char esquema[SISTEMA_ESQUEMA_MAX];
    int activo;
    int linea;
} SistemaControl;

typedef struct {
    char entrada[SISTEMA_ENTRADA_MAX];
    int flanco;
    char funcion[SISTEMA_FUNCION_MAX];
    char argumentos[SISTEMA_ARGUMENTOS_MAX];
    int control;               /* indice en SistemaPrograma.controles */
    int linea;
} SistemaAccion;

typedef struct {
    int evento;
    int actor;
    int flanco;
    int cada;
    char funcion[SISTEMA_FUNCION_MAX];
    char etiqueta[L3_TEXTO_MAX];
    int linea;
} SistemaRegla;

typedef struct {
    SistemaOpciones opciones;
    SistemaControl controles[SISTEMA_JUGADORES_MAX];
    int num_controles;
    SistemaAccion acciones[SISTEMA_ACCIONES_MAX];
    int num_acciones;
    SistemaRegla reglas[SISTEMA_REGLAS_MAX];
    int num_reglas;
} SistemaPrograma;

/* Devuelve 1 si el sprite del lobito debe crearse. Sin opciones se mantiene
 * el comportamiento de los proyectos antiguos. */
int sistema_jugador_implicito(const SistemaPrograma *s);

/* Lee y valida las piezas del proyecto. Devuelve 1 si el modelo es usable;
 * si no, deja el motivo en `error`. */
int sistema_leer(const Proyecto3 *p, SistemaPrograma *s,
                 char *error, int tam_error);

/* Anade `sistema.opciones` a un proyecto si todavia no lo tiene. La usa
 * logic3_nuevo_editor para que los proyectos nuevos nazcan en modo
 * programa. Devuelve 1 si el proyecto tiene opciones al terminar. */
int sistema_opciones_asegurar(Proyecto3 *p);

/* 1 si el bloque pertenece al modelo de programa. */
int sistema_es_bloque(const Bloque3 *b);

#endif /* SISTEMA_H */
