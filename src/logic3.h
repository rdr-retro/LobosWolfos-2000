/* logic3.h - modelo del proyecto LOGIC3 (fase 24).
 *
 * LOGIC3 es el formato de proyecto de los juegos complejos: escenas,
 * objetos, eventos, bloques, variables, listas, trajes y funciones. Esta
 * fase solo guarda, abre, valida y migra el modelo; el runtime sigue
 * usando LOGIC2 hasta la fase 25.
 *
 * El texto es UTF-8 sin BOM con lineas terminadas en \n (el lector
 * acepta CRLF). Una linea es un registro; los campos van separados por
 * espacios y los textos entre comillas dobles con los escapes \" \\ y
 * \n. Las secciones desconocidas se ignoran para poder ampliar el
 * formato sin romper archivos viejos.
 *
 * Un archivo con extension .log puede anadir la seccion `montaje`: sus
 * registros `nodo ID tipo ...` y `conecta ID -> ID` describen las piezas
 * visuales del juego de forma editable por texto. El lector convierte esa
 * seccion a bloques `sistema.pieza` y trata sus enlaces como fuente de verdad.
 *
 * Los topes son los presupuestos de Windows de la tabla 3.2 del plan
 * maestro. Los objetos y los bloques se reservan en el monton para que
 * un proyecto vacio no pague la memoria de los topes.
 */
#ifndef LOGIC3_H
#define LOGIC3_H

#include "app.h"

#define L3_VERSION              3

#define L3_ESCENAS_MAX          64
#define L3_OBJETOS_MAX         128
#define L3_TRAJES_MAX           32
#define L3_VARIABLES_GLOBAL_MAX 128
#define L3_VARIABLES_OBJETO_MAX 64
#define L3_EVENTOS_OBJETO_MAX   64
#define L3_EVENTOS_FUNCION_MAX  64
#define L3_FUNCIONES_MAX        64
#define L3_LISTAS_MAX           32
#define L3_LISTA_ELEMENTOS_MAX 256 /* elementos por lista (como en lobo) */
#define L3_BLOQUES_MAX       16384
#define L3_CAMPOS_MAX            8
#define L3_RANURAS_MAX           4
#define L3_HIJOS_MAX             8

#define L3_NOMBRE_MAX           48
#define L3_RUTA_MAX            128
#define L3_TIPO_MAX             48
#define L3_TEXTO_MAX            64
#define L3_ERROR_MAX           192
#define L3_LINEA_MAX          8192

/* Tipos de valor de variables y listas. */
enum {
    L3_TIPO_ENTERO = 0,
    L3_TIPO_BOOLEANO = 1,
    L3_TIPO_TEXTO = 2
};

/* Clases de evento. `llamada` solo vale dentro de una funcion. */
enum {
    L3_EVENTO_INICIO = 1,
    L3_EVENTO_FOTOGRAMA,
    L3_EVENTO_TECLA,
    L3_EVENTO_RECIBIR,
    L3_EVENTO_TOCAR,
    L3_EVENTO_CADA,
    L3_EVENTO_CLONAR,
    L3_EVENTO_ESCENA,
    L3_EVENTO_LLAMADA
};

/* Flanco de un evento de tecla. */
enum {
    L3_FLANCO_MANTENER = 0,
    L3_FLANCO_PULSAR = 1,
    L3_FLANCO_SOLTAR = 2
};

typedef struct {
    char nombre[L3_NOMBRE_MAX];
    int tipo;
    char valor[L3_TEXTO_MAX];
    int linea;
} Variable3;

/* Lista global con tipo de elemento y valores iniciales. Los elementos
 * se reservan dinamicamente, igual que objetos y bloques, para no inflar
 * el modelo que el editor tiene en la pila. */
typedef struct {
    char nombre[L3_NOMBRE_MAX];
    int capacidad;
    int tipo;                        /* L3_TIPO_* del elemento */
    int num_elementos;
    char (*elementos)[L3_TEXTO_MAX]; /* valores en texto, ya decodificados */
    int linea;
} Lista3;

/* Traje de un objeto. Los seis primeros campos son el formato original;
 * el resto son opcionales (fase 30) y por defecto dibujan un solo cuadro,
 * sin flip, sin rotacion y a un fotograma por cuadro. */
typedef struct {
    int indice;
    char imagen[L3_RUTA_MAX]; /* "" = traje vacio, sin PNG */
    int ancho, alto;
    int pivote_x, pivote_y;
    int cuadros;
    int cuadro_ancho, cuadro_alto;
    int flip_x, flip_y;
    int rotacion;  /* 0, 90, 180 o 270 */
    int velocidad; /* fotogramas por cuadro */
    int linea;
} Traje3;

/* Camara opcional del proyecto (fase 30). objetivo 0 sigue al jugador;
 * limites 0,0,0,0 usa el tamano del proyecto. */
typedef struct {
    int objetivo;
    int x, y, ancho, alto;
    int zoom;
    int linea;
} Camara3;

typedef struct {
    int id;
    int tipo;
    char nombre[L3_TEXTO_MAX]; /* tecla, mensaje o etiqueta de tocar */
    int flanco;
    int objeto;                /* tocar: id de objeto, -1 = cualquiera */
    int cada;                  /* cada N fotogramas */
    int escena;                /* evento al entrar en una escena */
    int funcion;               /* llamada: id de funcion */
    int linea;
} Evento3;

typedef struct {
    int id;
    char nombre[L3_NOMBRE_MAX];
    int parametros;
    int retorno;
    Evento3 eventos[L3_EVENTOS_FUNCION_MAX];
    int num_eventos;
    int linea;
} Funcion3;

typedef struct {
    int id;
    char nombre[L3_NOMBRE_MAX];
    char mapa[L3_RUTA_MAX];
    int ancho, alto;
    unsigned int fondo;
    int linea;
} Escena3;

typedef struct {
    int id;
    char nombre[L3_NOMBRE_MAX];
    int escena;
    int padre; /* 0 = raiz; ID del objeto padre en la misma escena */
    int x, y, w, h;
    int visible;
    int capa;
    Traje3 trajes[L3_TRAJES_MAX];
    int num_trajes;
    Variable3 variables[L3_VARIABLES_OBJETO_MAX];
    int num_variables;
    Evento3 eventos[L3_EVENTOS_OBJETO_MAX];
    int num_eventos;
    int linea;
} Objeto3;

typedef struct {
    char clave[L3_NOMBRE_MAX];
    char valor[L3_TEXTO_MAX];
    int linea;
} Campo3;

typedef struct {
    char clave[L3_NOMBRE_MAX];
    int bloque;
    int linea;
} Ranura3;

typedef struct {
    int id;
    int evento;
    int padre; /* 0 = bloque raiz del evento */
    int orden;
    char tipo[L3_TIPO_MAX];
    Campo3 campos[L3_CAMPOS_MAX];
    int num_campos;
    Ranura3 ranuras[L3_RANURAS_MAX];
    int num_ranuras;
    int hijos[L3_HIJOS_MAX];
    int num_hijos;
    /* El campo "codigo" de los bloques codigo.lobo es el unico valor que
     * puede pasar de L3_TEXTO_MAX: se guarda aparte para no engordar
     * todos los campos del proyecto. */
    char codigo[FUNCIONES_CODIGO_MAX];
    int tiene_codigo;
    int linea;
} Bloque3;

typedef struct {
    int version;
    char nombre[L3_NOMBRE_MAX];
    int ancho, alto;
    int limitar_cuadros;
    int fps;
    int velocidad;
    unsigned int fondo;
    char carpetas[4][L3_RUTA_MAX]; /* tiles, maps, trajes, sonidos */
    Camara3 camara;                 /* fase 30, opcional */
    Variable3 variables[L3_VARIABLES_GLOBAL_MAX];
    int num_variables;
    Lista3 listas[L3_LISTAS_MAX];
    int num_listas;
    Escena3 escenas[L3_ESCENAS_MAX];
    int num_escenas;
    Objeto3 *objetos;
    int num_objetos;
    int cap_objetos;
    Funcion3 funciones[L3_FUNCIONES_MAX];
    int num_funciones;
    Bloque3 *bloques;
    int num_bloques;
    int cap_bloques;
} Proyecto3;

/* Modelo en memoria (reserva y libera todo). */
void logic3_iniciar(Proyecto3 *p);
void logic3_liberar(Proyecto3 *p);

/* Anade un bloque al final del proyecto y devuelve un puntero a el en
 * `salida`. El id es el mayor existente mas uno; `evento`, `padre`, `orden`
 * y `tipo` se copian tal cual. Lo usan las piezas del modelo de programa. */
int logic3_bloque_nuevo(Proyecto3 *p, int evento, int padre, int orden,
                        const char *tipo, Bloque3 **salida);

/* Lectura y escritura del archivo LOGIC3. Si algo falla devuelve 0 y
 * deja en `error` un mensaje "linea N: motivo". */
int logic3_cargar(Proyecto3 *p, const char *ruta, char *error, int tam_error);
int logic3_guardar(const Proyecto3 *p, const char *ruta, char *error, int tam_error);
int logic3_validar(const Proyecto3 *p, char *error, int tam_error);

/* Modelo activo del editor, para la vista Estudio. Devuelve NULL si no hay
 * ningun proyecto LOGIC3 cargado. `logic3_nuevo_editor` crea uno vacio con
 * los ajustes actuales del editor. */
Proyecto3 *logic3_modelo_activo(void);
int logic3_nuevo_editor(void);

/* CRC32 del archivo (polinomio 0xEDB88320), el mismo que se escribe en
 * la ultima linea. */
unsigned int logic3_crc32(const void *datos, int largo);

/* Migracion LOGIC2 -> LOGIC3. `orden` son indices de `modulos` en orden
 * de ejecucion (como modulos_orden_ejecucion); si es NULL se usa el
 * orden del array de funciones. Devuelve 1. */
int logic3_desde_logic2(Proyecto3 *p, const AjustesProyecto *ajustes,
                        const ModuloElemento *modulos, int num_modulos,
                        const FuncionCodigo *funciones, int num_funciones,
                        const int *orden, int num_orden);

/* Exportacion de emergencia LOGIC3 -> LOGIC2: vuelca la cabecera y los
 * bloques `codigo.lobo` al modelo clasico. Devuelve 1; si no cabe algo
 * devuelve 0 con el motivo en `error` (los arrays no se tocan). */
int logic3_a_logic2(const Proyecto3 *p, AjustesProyecto *ajustes,
                    ModuloElemento *modulos, int *num_modulos, int max_modulos,
                    FuncionCodigo *funciones, int *num_funciones, int max_funciones,
                    char *error, int tam_error);

/* Nombres del formato de tipos y eventos (lector, escritor y editor). */
const char *logic3_tipo_texto(int tipo);
int logic3_tipo_desde_texto(const char *texto);
const char *logic3_evento_texto(int tipo);
int logic3_evento_desde_texto(const char *texto);
const char *logic3_flanco_texto(int flanco);

#endif /* LOGIC3_H */
