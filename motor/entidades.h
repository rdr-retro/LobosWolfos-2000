/* entidades.h - entidades y escenas del runtime (fase 26).
 *
 * Una entidad es una instancia de un objeto LOGIC3: guarda su posicion, su
 * tamano, su capa, su visibilidad, su traje y sus variables. El tipo (el
 * objeto del proyecto) guarda los valores iniciales inmutables y la lista
 * de variables con sus valores de arranque.
 *
 * Las entidades viven en un array fijo de ENTIDADES_MAX (tabla 3.2 del
 * plan maestro) y nunca se mueven de sitio mientras estan vivas: los ids
 * son monotonos y no se reutilizan, para que un script pueda guardar un id
 * en una lista sin que acabe apuntando a otra entidad. Al final del
 * fotograma se eliminan las marcadas con entidad_borrar() y se reinicia
 * el contador de altas del fotograma.
 *
 * C puro: ni windows.h ni malloc. Lo enlazan game.exe y el editor.
 */
#ifndef ENTIDADES_H
#define ENTIDADES_H

#define ENTIDADES_MAX        256 /* entidades simultaneas (tabla 3.2) */
#define ENTIDAD_TIPOS_MAX    128 /* objetos distintos del proyecto */
#define ENTIDAD_VARS_MAX      64 /* variables por entidad */
#define ENTIDADES_NUEVAS_FOTO 32 /* altas por fotograma (tabla 3.2) */
#define ENTIDAD_NOMBRE_MAX    64 /* nombre o tag de un objeto */

/* Campos que se pueden leer o escribir desde lobo (mi.x, mi.vida...). */
enum {
    ENTIDAD_CAMPO_ID = 0,
    ENTIDAD_CAMPO_X,
    ENTIDAD_CAMPO_Y,
    ENTIDAD_CAMPO_VIDA,
    ENTIDAD_CAMPO_VISIBLE,
    ENTIDAD_CAMPO_TRAJE,
    ENTIDAD_CAMPO_ESCENA,
    ENTIDAD_CAMPO_W,
    ENTIDAD_CAMPO_H,
    ENTIDAD_CAMPO_CAPA,
    ENTIDAD_CAMPO_TIPO
};

/* Valores iniciales de un objeto LOGIC3. */
typedef struct EntidadTipo {
    int id;                           /* id del objeto en LOGIC3 */
    char nombre[ENTIDAD_NOMBRE_MAX];  /* tag que usa crear("tag", x, y) */
    int escena;
    int x, y, w, h;
    int visible;
    int capa;
    int num_vars;                     /* slots usados (maximo + 1) */
    int var_valor[ENTIDAD_VARS_MAX];
    char var_nombre[ENTIDAD_VARS_MAX][ENTIDAD_NOMBRE_MAX];
} EntidadTipo;

/* Una instancia viva. */
typedef struct Entidad {
    int usada;
    int id;        /* id monotono, nunca 0 */
    int tipo;      /* indice en la tabla de tipos */
    int escena;
    int x, y, w, h;
    int visible;
    int capa;
    int traje;
    int vida;
    int es_clon;
    int borrar;    /* marcada para eliminacion al final del fotograma */
    int vars[ENTIDAD_VARS_MAX];
} Entidad;

/* Borra todas las entidades y tipos (arranque de proyecto). */
void entidades_reiniciar(void);

/* Registra un objeto como tipo de entidad. Devuelve el indice del tipo o
 * -1 si no cabe. Si el id de objeto ya existe, devuelve el tipo actual. */
int entidades_tipo_agregar(int id_objeto, const char *nombre, int escena,
                           int x, int y, int w, int h, int visible, int capa);

/* Declara una variable del tipo en un slot global. */
int entidades_tipo_var(int tipo, int slot, const char *nombre, int valor);

/* Indices de tipo por id de objeto o por tag; -1 si no existe. */
int entidades_tipo_por_objeto(int id_objeto);
int entidades_tipo_por_tag(const char *nombre);

/* Indice del slot de una variable del tipo; -1 si no existe. */
int entidades_slot_var(int tipo, const char *nombre);

/* Numero de variables del tipo y nombre de una de ellas (para el volcado
 * de --volcar --entidades). */
int entidades_tipo_vars(int tipo);
const char *entidades_tipo_var_nombre(int tipo, int slot);

/* Crea una instancia en la escena del tipo. Devuelve el id o -1 (sin
 * hueco, limite de altas del fotograma o tipo invalido). */
int entidad_crear(int tipo, int x, int y);

/* Copia una entidad viva (posicion, traje, vida y variables). */
int entidades_clonar(int id);

/* Marca una entidad para borrar al final del fotograma. */
int entidad_borrar(int id);

/* Elimina las marcadas y reinicia el contador de altas del fotograma. */
void entidades_fin_fotograma(void);

/* Busca por id o por tag. Devuelve el puntero o NULL / el id o -1. */
Entidad *entidad_buscar(int id);
int entidad_por_tag(const char *tag);

/* Mueve una entidad y la cambia de escena. Devuelven 1 o -1. */
int entidad_mover(int id, int x, int y);
int entidad_cambiar_escena(int id, int escena);

/* Entrada por indice (0..ENTIDADES_MAX-1) para recorrer el array. */
Entidad *entidad_indice(int i);
int entidades_max(void);

/* Rellena `salida` con los indices de las entidades vivas ordenados por
 * capa y luego id. Devuelve cuantas hay. */
int entidades_orden(int *salida, int maximo);

/* Lectura y escritura de campos y variables de una entidad. */
int entidades_leer_campo(int id, int campo);
int entidades_escribir_campo(int id, int campo, int valor);
int entidades_leer_var(int id, int slot);
int entidades_escribir_var(int id, int slot, int valor);

/* Numero de entidades vivas, avisos acumulados y ultimo id repartido.
 * entidades_creadas() cuenta las altas correctas desde reiniciar (las del
 * arranque incluidas), para comprobar que un script creo lo que dice. */
int entidades_num(void);
int entidades_avisos(void);
int entidades_siguiente_id(void);
int entidades_creadas(void);

/* Anota un aviso (tag desconocido, limite tocado...). */
void entidades_aviso(void);

#endif /* ENTIDADES_H */
