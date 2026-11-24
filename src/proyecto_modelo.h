/* proyecto_modelo.h - contrato minimo del ProjectModel (plan 16.1).
 *
 * Una sola fuente de verdad en memoria para proyecto, recurso, escena,
 * actor, instancia, control y conexiones. No lee archivos, no dibuja y no
 * escribe LOGIC3: esas capas se anadiran en los puntos siguientes del plan.
 *
 * Cada entidad tiene ID estable, kind semantico, nombre visible,
 * propiedades, referencias, estado de UI y origen. Las vistas guardan IDs,
 * nunca punteros. Los IDs no se reciclan. Un limite alcanzado deja un
 * diagnostico consultable con pm_error().
 */
#ifndef PROYECTO_MODELO_H
#define PROYECTO_MODELO_H

#include <stddef.h>

#define PM_ENTIDADES_MAX    128
#define PM_PROPIEDADES_MAX   16
#define PM_REFERENCIAS_MAX    4
#define PM_NOMBRE_MAX        48
#define PM_TEXTO_MAX         64
#define PM_RUTA_MAX         128
#define PM_ERROR_MAX        192

/* Tipos semanticos. Una conexion es tambien una entidad para que tenga
 * ID, nombre y origen como cualquier otra pieza del grafo. PM_KIND_ACCION
 * representa las acciones del .log actual sin renumerar los tipos minimos. */
enum {
    PM_KIND_PROYECTO = 1,
    PM_KIND_RECURSO,
    PM_KIND_ESCENA,
    PM_KIND_ACTOR,
    PM_KIND_INSTANCIA,
    PM_KIND_CONTROL,
    PM_KIND_CONEXION,
    PM_KIND_ACCION,
    PM_KIND_NODO,
    PM_KIND_SCRIPT,
    PM_KIND_BINDING
};

typedef struct {
    char clave[PM_NOMBRE_MAX];
    char valor[PM_TEXTO_MAX];
    int linea;
} PropiedadModelo;

typedef struct {
    char nombre[PM_NOMBRE_MAX];
    int id;
    char ruta[PM_RUTA_MAX];
    int linea;
} ReferenciaModelo;

typedef struct {
    int x, y, ancho, alto;
    int grupo;
    unsigned int color;
    int plegado;
} UiModelo;

typedef struct {
    char archivo[PM_RUTA_MAX];
    int linea;
} OrigenModelo;

typedef struct {
    int id;
    int kind;
    char nombre[PM_NOMBRE_MAX];
    PropiedadModelo propiedades[PM_PROPIEDADES_MAX];
    int num_propiedades;
    ReferenciaModelo referencias[PM_REFERENCIAS_MAX];
    int num_referencias;
    UiModelo ui;
    OrigenModelo origen;
} EntidadModelo;

typedef struct {
    int num_entidades;
    int siguiente_id;
    char error[PM_ERROR_MAX];
    EntidadModelo entidades[PM_ENTIDADES_MAX];
} ProjectModel;

ProjectModel *pm_crear(void);
void pm_destruir(ProjectModel *m);
const char *pm_error(const ProjectModel *m);
EntidadModelo *pm_entidad(ProjectModel *m, int id);
const EntidadModelo *pm_entidad_const(const ProjectModel *m, int id);
int pm_anadir(ProjectModel *m, int kind, const char *nombre);
int pm_anadir_con_id(ProjectModel *m, int id, int kind, const char *nombre);
int pm_renombrar(EntidadModelo *e, const char *nombre);
int pm_propiedad(EntidadModelo *e, const char *clave, const char *valor, int linea);
int pm_quitar_propiedad(EntidadModelo *e, const char *clave);
int pm_referencia(EntidadModelo *e, const char *nombre, int id,
                  const char *ruta, int linea);
int pm_quitar_referencia(EntidadModelo *e, const char *nombre);
void pm_origen(EntidadModelo *e, const char *archivo, int linea);
void pm_ui(EntidadModelo *e, int x, int y, int ancho, int alto, int grupo,
           unsigned int color, int plegado);
int pm_conectar(ProjectModel *m, int origen, const char *puerto_origen,
                int destino, const char *puerto_destino, int linea);
/* Registro de recursos compartido por Pintar, Tiles, Mapas, Audio y paquetes.
 * Si ya existe un recurso con la misma ruta, actualiza su tipo y nombre y
 * devuelve su ID; si no, anade una entidad PM_KIND_RECURSO. No toca archivos
 * ni cambia el formato de ningun .log. */
int pm_registrar_recurso(ProjectModel *m, const char *ruta, const char *tipo,
                         const char *nombre);
/* Numero de entidades que referencian a `id`. Sirve para avisar antes de
 * borrar una pieza usada por el arbol o por una asociacion. */
int pm_referencias_entrantes(const ProjectModel *m, int id);

/* Operaciones de la vista Inicio sobre el mismo grafo. Borrar quita la
 * entidad y las conexiones que la referencian; los hijos suben un nivel.
 * Reparentar usa la referencia "padre" y rechaza ciclos. Los IDs no se
 * reciclan. pm_copiar sirve para los snapshots del historial. */
int pm_borrar(ProjectModel *m, int id);
int pm_reparentar(ProjectModel *m, int id, int id_padre);
int pm_padre(const ProjectModel *m, int id);
void pm_copiar(const ProjectModel *origen, ProjectModel *destino);
int pm_validar(const ProjectModel *m, char *diagnostico, size_t longitud);
/* Comprueba en disco cada recurso y que cada actor recibe un recurso por
 * una conexion. `raiz` es la carpeta del proyecto; NULL o "" usa la actual.
 * Es una validacion distinta de pm_validar porque no depende de los archivos
 * presentes y puede ejecutarse antes de que existan los recursos. */
int pm_validar_recursos(const ProjectModel *m, const char *raiz,
                        char *diagnostico, size_t longitud);
/* Igual comprobacion, pero el resultado es un JSON de una sola linea con
 * todos los fallos: codigo, archivo, linea, id y nombre de la entidad, y
 * cuando aplica la ruta del recurso. Cada fallo incluye una sugerencia para
 * repararlo. No escribe archivos; devuelve el numero de fallos y deja el
 * texto en `json`. */
int pm_validar_recursos_json(const ProjectModel *m, const char *raiz,
                             char *json, size_t longitud);
void pm_volcar(const ProjectModel *m, const char *ruta);
const char *pm_kind_nombre(int kind);

#endif
