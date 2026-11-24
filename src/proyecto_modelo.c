/* proyecto_modelo.c - implementacion del ProjectModel minimo (plan 16.1).
 *
 * Servicio en memoria, sin dependencias de Win32 ni de LOGIC3. Las vistas y
 * los importadores futuros solo necesitan este contrato para crear entidades
 * y enlazarlas por ID.
 */
#include "proyecto_modelo.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copiar_seguro(char *dst, size_t tam, const char *src) {
    size_t n;
    if (!dst || tam == 0) return;
    if (!src) src = "";
    n = strlen(src);
    if (n >= tam) n = tam - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void limpiar_espacios(char *texto) {
    size_t i;
    if (!texto) return;
    for (i = 0; texto[i]; i++) if (texto[i] == ' ') texto[i] = '_';
}

static void error_fijar(ProjectModel *m, const char *texto) {
    if (m) copiar_seguro(m->error, sizeof(m->error), texto);
}

static void anotar_fallo(char *primera, size_t tam, int *fallos,
                         const char *texto) {
    (*fallos)++;
    if (*fallos == 1) copiar_seguro(primera, tam, texto);
}

static const PropiedadModelo *buscar_propiedad(const EntidadModelo *e,
                                               const char *clave) {
    int i;
    if (!e || !clave) return NULL;
    for (i = 0; i < e->num_propiedades; i++)
        if (strcmp(e->propiedades[i].clave, clave) == 0)
            return &e->propiedades[i];
    return NULL;
}

static const ReferenciaModelo *buscar_referencia(const EntidadModelo *e,
                                                 const char *nombre);

ProjectModel *pm_crear(void) {
    ProjectModel *m = (ProjectModel *)calloc(1, sizeof(ProjectModel));
    if (!m) return NULL;
    m->siguiente_id = 1;
    return m;
}

void pm_destruir(ProjectModel *m) {
    free(m);
}

const char *pm_error(const ProjectModel *m) {
    return m ? m->error : "";
}

EntidadModelo *pm_entidad(ProjectModel *m, int id) {
    int i;
    if (!m || id <= 0) return NULL;
    for (i = 0; i < m->num_entidades; i++)
        if (m->entidades[i].id == id) return &m->entidades[i];
    return NULL;
}

const EntidadModelo *pm_entidad_const(const ProjectModel *m, int id) {
    return pm_entidad((ProjectModel *)m, id);
}

int pm_anadir_con_id(ProjectModel *m, int id, int kind, const char *nombre) {
    EntidadModelo *e;
    if (!m) return 0;
    if (id <= 0) {
        error_fijar(m, "el ID debe ser positivo");
        return 0;
    }
    if (pm_entidad(m, id)) {
        error_fijar(m, "ID duplicado");
        return 0;
    }
    if (m->num_entidades >= PM_ENTIDADES_MAX) {
        error_fijar(m, "limite de entidades alcanzado");
        return 0;
    }
    e = &m->entidades[m->num_entidades++];
    memset(e, 0, sizeof(*e));
    e->id = id;
    e->kind = kind;
    copiar_seguro(e->nombre, sizeof(e->nombre), nombre);
    if (id >= m->siguiente_id) {
        if (id == INT_MAX) m->siguiente_id = INT_MAX;
        else m->siguiente_id = id + 1;
    }
    return id;
}

int pm_anadir(ProjectModel *m, int kind, const char *nombre) {
    int id;
    if (!m) return 0;
    id = m->siguiente_id > 0 ? m->siguiente_id : 1;
    while (pm_entidad(m, id)) {
        if (id == INT_MAX) {
            error_fijar(m, "no quedan IDs libres");
            return 0;
        }
        id++;
    }
    return pm_anadir_con_id(m, id, kind, nombre);
}

int pm_renombrar(EntidadModelo *e, const char *nombre) {
    if (!e) return 0;
    copiar_seguro(e->nombre, sizeof(e->nombre), nombre);
    return e->nombre[0] ? 1 : 0;
}

int pm_propiedad(EntidadModelo *e, const char *clave, const char *valor,
                 int linea) {
    PropiedadModelo *p;
    int i;
    if (!e || !clave || !clave[0]) return 0;
    for (i = 0; i < e->num_propiedades; i++) {
        p = &e->propiedades[i];
        if (strcmp(p->clave, clave) == 0) {
            copiar_seguro(p->valor, sizeof(p->valor), valor);
            p->linea = linea;
            return 1;
        }
    }
    if (e->num_propiedades >= PM_PROPIEDADES_MAX) return 0;
    p = &e->propiedades[e->num_propiedades++];
    copiar_seguro(p->clave, sizeof(p->clave), clave);
    copiar_seguro(p->valor, sizeof(p->valor), valor);
    p->linea = linea;
    return 1;
}

int pm_quitar_propiedad(EntidadModelo *e, const char *clave) {
    int i;
    if (!e || !clave) return 0;
    for (i = 0; i < e->num_propiedades; i++) {
        if (strcmp(e->propiedades[i].clave, clave) != 0) continue;
        for (; i + 1 < e->num_propiedades; i++)
            e->propiedades[i] = e->propiedades[i + 1];
        e->num_propiedades--;
        return 1;
    }
    return 0;
}

int pm_referencia(EntidadModelo *e, const char *nombre, int id,
                  const char *ruta, int linea) {
    ReferenciaModelo *r;
    int i;
    if (!e || !nombre || !nombre[0]) return 0;
    if (id <= 0 && (!ruta || !ruta[0])) return 0;
    for (i = 0; i < e->num_referencias; i++) {
        r = &e->referencias[i];
        if (strcmp(r->nombre, nombre) == 0) {
            r->id = id;
            copiar_seguro(r->ruta, sizeof(r->ruta), ruta);
            r->linea = linea;
            return 1;
        }
    }
    if (e->num_referencias >= PM_REFERENCIAS_MAX) return 0;
    r = &e->referencias[e->num_referencias++];
    copiar_seguro(r->nombre, sizeof(r->nombre), nombre);
    r->id = id;
    copiar_seguro(r->ruta, sizeof(r->ruta), ruta);
    r->linea = linea;
    return 1;
}

int pm_quitar_referencia(EntidadModelo *e, const char *nombre) {
    int i;
    if (!e || !nombre) return 0;
    for (i = 0; i < e->num_referencias; i++) {
        if (strcmp(e->referencias[i].nombre, nombre) != 0) continue;
        for (; i + 1 < e->num_referencias; i++)
            e->referencias[i] = e->referencias[i + 1];
        e->num_referencias--;
        return 1;
    }
    return 0;
}

void pm_origen(EntidadModelo *e, const char *archivo, int linea) {
    if (!e) return;
    copiar_seguro(e->origen.archivo, sizeof(e->origen.archivo), archivo);
    e->origen.linea = linea;
}

void pm_ui(EntidadModelo *e, int x, int y, int ancho, int alto, int grupo,
           unsigned int color, int plegado) {
    if (!e) return;
    e->ui.x = x;
    e->ui.y = y;
    e->ui.ancho = ancho;
    e->ui.alto = alto;
    e->ui.grupo = grupo;
    e->ui.color = color;
    e->ui.plegado = plegado;
}

int pm_conectar(ProjectModel *m, int origen, const char *puerto_origen,
                int destino, const char *puerto_destino, int linea) {
    char nombre[PM_NOMBRE_MAX];
    EntidadModelo *e;
    int id;
    if (!m) return 0;
    snprintf(nombre, sizeof(nombre), "conexion_%d", m->siguiente_id);
    id = pm_anadir(m, PM_KIND_CONEXION, nombre);
    if (!id) return 0;
    e = pm_entidad(m, id);
    if (!e) return 0;
    snprintf(nombre, sizeof(nombre), "conexion_%d", id);
    pm_renombrar(e, nombre);
    if (!pm_propiedad(e, "puerto_origen", puerto_origen, linea) ||
        !pm_propiedad(e, "puerto_destino", puerto_destino, linea) ||
        !pm_referencia(e, "origen", origen, NULL, linea) ||
        !pm_referencia(e, "destino", destino, NULL, linea)) {
        error_fijar(m, "no cabe la conexion en el modelo");
        return 0;
    }
    return id;
}

static void nombre_de_ruta(char *dst, size_t tam, const char *ruta) {
    const char *nombre = ruta;
    const char *p;
    if (!ruta || !ruta[0]) { copiar_seguro(dst, tam, "recurso"); return; }
    for (p = ruta; *p; p++)
        if (*p == '/' || *p == '\\') nombre = p + 1;
    copiar_seguro(dst, tam, nombre);
}

int pm_registrar_recurso(ProjectModel *m, const char *ruta, const char *tipo,
                         const char *nombre) {
    const PropiedadModelo *p;
    EntidadModelo *e;
    char nombre_auto[PM_NOMBRE_MAX];
    int i, id;

    if (!m || !ruta || !ruta[0] || !tipo || !tipo[0]) {
        error_fijar(m, "el registro necesita ruta y tipo");
        return 0;
    }
    for (i = 0; i < m->num_entidades; i++) {
        e = &m->entidades[i];
        if (e->kind != PM_KIND_RECURSO) continue;
        p = buscar_propiedad(e, "ruta");
        if (!p || strcmp(p->valor, ruta) != 0) continue;
        if (!pm_propiedad(e, "tipo", tipo, e->origen.linea)) {
            error_fijar(m, "no caben mas propiedades en el recurso");
            return 0;
        }
        if (nombre && nombre[0] && !pm_renombrar(e, nombre)) {
            error_fijar(m, "no se pudo renombrar el recurso");
            return 0;
        }
        return e->id;
    }
    if (!nombre || !nombre[0]) {
        nombre_de_ruta(nombre_auto, sizeof(nombre_auto), ruta);
        nombre = nombre_auto;
    }
    id = pm_anadir(m, PM_KIND_RECURSO, nombre);
    if (!id) return 0;
    e = pm_entidad(m, id);
    pm_origen(e, "utilidades", 0);
    if (!pm_propiedad(e, "tipo", tipo, 0) ||
        !pm_propiedad(e, "ruta", ruta, 0)) {
        error_fijar(m, "no caben mas propiedades en el recurso");
        return 0;
    }
    return id;
}

int pm_referencias_entrantes(const ProjectModel *m, int id) {
    const EntidadModelo *e;
    int i, j, n = 0;
    if (!m || id <= 0) return 0;
    for (i = 0; i < m->num_entidades; i++) {
        e = &m->entidades[i];
        for (j = 0; j < e->num_referencias; j++)
            if (e->referencias[j].id == id) n++;
    }
    return n;
}

const char *pm_kind_nombre(int kind) {
    switch (kind) {
    case PM_KIND_PROYECTO:  return "proyecto";
    case PM_KIND_RECURSO:   return "recurso";
    case PM_KIND_ESCENA:    return "escena";
    case PM_KIND_ACTOR:     return "actor";
    case PM_KIND_INSTANCIA: return "instancia";
    case PM_KIND_CONTROL:   return "control";
    case PM_KIND_CONEXION:  return "conexion";
    case PM_KIND_ACCION:    return "accion";
    case PM_KIND_NODO:      return "nodo";
    case PM_KIND_SCRIPT:    return "script";
    case PM_KIND_BINDING:   return "asociacion";
    default:                return "desconocido";
    }
}

int pm_padre(const ProjectModel *m, int id) {
    const EntidadModelo *e = pm_entidad_const(m, id);
    int i;
    if (!e) return 0;
    for (i = 0; i < e->num_referencias; i++)
        if (strcmp(e->referencias[i].nombre, "padre") == 0)
            return e->referencias[i].id;
    return 0;
}

int pm_borrar(ProjectModel *m, int id) {
    int borrar[PM_ENTIDADES_MAX];
    EntidadModelo *e;
    int padre, i, j, n, destino;

    if (!m) return 0;
    e = pm_entidad(m, id);
    if (!e) {
        error_fijar(m, "no existe la entidad");
        return 0;
    }
    if (e->kind == PM_KIND_PROYECTO) {
        error_fijar(m, "no se puede borrar el proyecto");
        return 0;
    }
    memset(borrar, 0, sizeof(borrar));
    borrar[e - m->entidades] = 1;
    n = 1;
    for (i = 0; i < m->num_entidades; i++) {
        if (borrar[i] ||
            m->entidades[i].kind != PM_KIND_CONEXION) continue;
        for (j = 0; j < m->entidades[i].num_referencias; j++) {
            if (m->entidades[i].referencias[j].id == id) {
                borrar[i] = 1;
                n++;
                break;
            }
        }
    }
    padre = pm_padre(m, id);
    /* Los hijos del nodo borrado suben un nivel para no quedar colgados. */
    for (i = 0; i < m->num_entidades; i++) {
        if (borrar[i] || pm_padre(m, m->entidades[i].id) != id) continue;
        if (padre)
            pm_referencia(&m->entidades[i], "padre", padre, NULL, 0);
        else
            pm_quitar_referencia(&m->entidades[i], "padre");
    }
    destino = 0;
    for (i = 0; i < m->num_entidades; i++) {
        if (borrar[i]) continue;
        if (destino != i) m->entidades[destino] = m->entidades[i];
        destino++;
    }
    m->num_entidades = destino;
    return n;
}

int pm_reparentar(ProjectModel *m, int id, int id_padre) {
    EntidadModelo *e;
    int actual, pasos;

    if (!m) return 0;
    e = pm_entidad(m, id);
    if (!e) {
        error_fijar(m, "no existe la entidad");
        return 0;
    }
    if (id_padre < 0) {
        error_fijar(m, "el padre debe ser un ID o 0");
        return 0;
    }
    if (id_padre == id) {
        error_fijar(m, "un nodo no puede ser su propio padre");
        return 0;
    }
    if (id_padre && !pm_entidad(m, id_padre)) {
        error_fijar(m, "no existe el padre");
        return 0;
    }
    if (id_padre && pm_entidad_const(m, id_padre)->kind == PM_KIND_CONEXION) {
        error_fijar(m, "el padre no puede ser una conexion");
        return 0;
    }
    actual = id_padre;
    pasos = 0;
    while (actual && pasos++ <= m->num_entidades) {
        if (actual == id) {
            error_fijar(m, "el reparentado formaria un ciclo");
            return 0;
        }
        actual = pm_padre(m, actual);
    }
    if (actual) {
        error_fijar(m, "el arbol de padres es demasiado largo");
        return 0;
    }
    if (id_padre) {
        if (!pm_referencia(e, "padre", id_padre, NULL, 0)) {
            error_fijar(m, "no cabe la referencia padre");
            return 0;
        }
    } else {
        pm_quitar_referencia(e, "padre");
    }
    return 1;
}

void pm_copiar(const ProjectModel *origen, ProjectModel *destino) {
    if (!origen || !destino) return;
    memcpy(destino, origen, sizeof(*destino));
}
int pm_validar(const ProjectModel *m, char *diagnostico, size_t longitud) {
    const EntidadModelo *e;
    const PropiedadModelo *p;
    char primera[PM_ERROR_MAX];
    char buf[PM_ERROR_MAX];
    int fallos = 0;
    int i, j;

    primera[0] = '\0';
    if (!m) {
        anotar_fallo(primera, sizeof(primera), &fallos, "sin modelo");
    } else if (m->num_entidades == 0) {
        anotar_fallo(primera, sizeof(primera), &fallos, "sin proyecto");
    } else {
        for (i = 0; i < m->num_entidades; i++) {
            e = &m->entidades[i];
            if (e->id <= 0 || !e->nombre[0]) {
                snprintf(buf, sizeof(buf), "entidad %d sin nombre", i + 1);
                anotar_fallo(primera, sizeof(primera), &fallos, buf);
            }
            if (e->kind == PM_KIND_RECURSO) {
                p = buscar_propiedad(e, "ruta");
                if (!p || !p->valor[0]) {
                    snprintf(buf, sizeof(buf), "recurso %d sin ruta", e->id);
                    anotar_fallo(primera, sizeof(primera), &fallos, buf);
                }
            }
            if (e->kind == PM_KIND_SCRIPT) {
                p = buscar_propiedad(e, "ruta");
                if (!p || !p->valor[0]) {
                    snprintf(buf, sizeof(buf), "script %d sin ruta", e->id);
                    anotar_fallo(primera, sizeof(primera), &fallos, buf);
                }
            }
            if (e->kind == PM_KIND_NODO) {
                const ReferenciaModelo *rs = buscar_referencia(e, "escena");
                const ReferenciaModelo *rp = buscar_referencia(e, "padre");
                const EntidadModelo *es = rs ? pm_entidad_const(m, rs->id) : NULL;
                const EntidadModelo *ep = rp ? pm_entidad_const(m, rp->id) : NULL;
                int escena_id;
                p = buscar_propiedad(e, "escena");
                escena_id = rs ? rs->id : (p ? atoi(p->valor) : 0);
                if (!p || atoi(p->valor) <= 0) {
                    snprintf(buf, sizeof(buf), "nodo %d sin escena", e->id);
                    anotar_fallo(primera, sizeof(primera), &fallos, buf);
                }
                if (!es || es->kind != PM_KIND_ESCENA ||
                    (p && atoi(p->valor) != es->id)) {
                    snprintf(buf, sizeof(buf), "nodo %d con escena invalida", e->id);
                    anotar_fallo(primera, sizeof(primera), &fallos, buf);
                }
                if (rp && (!ep || ep->kind != PM_KIND_NODO || ep->id == e->id)) {
                    snprintf(buf, sizeof(buf), "nodo %d con padre invalido", e->id);
                    anotar_fallo(primera, sizeof(primera), &fallos, buf);
                } else if (rp && ep) {
                    const ReferenciaModelo *padre_escena =
                        buscar_referencia(ep, "escena");
                    if (!padre_escena || padre_escena->id != escena_id) {
                        snprintf(buf, sizeof(buf),
                                 "nodo %d con padre de otra escena", e->id);
                        anotar_fallo(primera, sizeof(primera), &fallos, buf);
                    }
                }
            }
            if (e->kind == PM_KIND_BINDING) {
                const ReferenciaModelo *rs = buscar_referencia(e, "script");
                const ReferenciaModelo *rn = buscar_referencia(e, "nodo");
                const ReferenciaModelo *re = buscar_referencia(e, "escena");
                const EntidadModelo *es = rs ? pm_entidad_const(m, rs->id) : NULL;
                const EntidadModelo *en = rn ? pm_entidad_const(m, rn->id) : NULL;
                const EntidadModelo *ee = re ? pm_entidad_const(m, re->id) : NULL;
                p = buscar_propiedad(e, "evento");
                if (!p || !p->valor[0] || !rs || !es ||
                    es->kind != PM_KIND_SCRIPT ||
                    ((rn != NULL) == (re != NULL)) ||
                    (rn && (!en || en->kind != PM_KIND_NODO)) ||
                    (re && (!ee || ee->kind != PM_KIND_ESCENA))) {
                    snprintf(buf, sizeof(buf), "asociacion %d incompleta", e->id);
                    anotar_fallo(primera, sizeof(primera), &fallos, buf);
                }
            }
            if (e->kind == PM_KIND_CONEXION) {
                if (!buscar_propiedad(e, "puerto_origen") ||
                    !buscar_propiedad(e, "puerto_destino")) {
                    snprintf(buf, sizeof(buf), "conexion %d sin puertos", e->id);
                    anotar_fallo(primera, sizeof(primera), &fallos, buf);
                }
            }
            for (j = 0; j < e->num_referencias; j++) {
                const ReferenciaModelo *r = &e->referencias[j];
                if (r->id <= 0 && !r->ruta[0]) {
                    snprintf(buf, sizeof(buf), "referencia %d:%s vacia",
                             e->id, r->nombre);
                    anotar_fallo(primera, sizeof(primera), &fallos, buf);
                } else if (r->id > 0 && !pm_entidad_const(m, r->id)) {
                    snprintf(buf, sizeof(buf), "referencia %d:%s a ID %d inexistente",
                             e->id, r->nombre, r->id);
                    anotar_fallo(primera, sizeof(primera), &fallos, buf);
                }
            }
        }
    }
    if (diagnostico && longitud > 0)
        copiar_seguro(diagnostico, longitud, fallos ? primera : "sin errores");
    return fallos;
}

static const ReferenciaModelo *buscar_referencia(const EntidadModelo *e,
                                                 const char *nombre) {
    int i;
    if (!e || !nombre) return NULL;
    for (i = 0; i < e->num_referencias; i++)
        if (strcmp(e->referencias[i].nombre, nombre) == 0)
            return &e->referencias[i];
    return NULL;
}

/* Un actor queda conectado cuando alguna conexion tiene como origen un
 * recurso y como destino ese actor. No se mira el nombre del recurso. */
static int actor_con_recurso(const ProjectModel *m, int id_actor) {
    const EntidadModelo *c;
    const ReferenciaModelo *origen, *destino;
    const EntidadModelo *recurso;
    int i;
    if (!m) return 0;
    for (i = 0; i < m->num_entidades; i++) {
        c = &m->entidades[i];
        if (c->kind != PM_KIND_CONEXION) continue;
        origen = buscar_referencia(c, "origen");
        destino = buscar_referencia(c, "destino");
        if (!origen || !destino || destino->id != id_actor) continue;
        recurso = pm_entidad_const(m, origen->id);
        if (recurso && recurso->kind == PM_KIND_RECURSO) return 1;
    }
    return 0;
}

int pm_validar_recursos(const ProjectModel *m, const char *raiz,
                        char *diagnostico, size_t longitud) {
    const EntidadModelo *e;
    const PropiedadModelo *p;
    char primera[PM_ERROR_MAX];
    char buf[PM_ERROR_MAX];
    char ruta[PM_RUTA_MAX * 2 + 4];
    FILE *f;
    int fallos = 0;
    int i;

    primera[0] = '\0';
    if (!m) {
        anotar_fallo(primera, sizeof(primera), &fallos, "sin modelo");
    } else {
        for (i = 0; i < m->num_entidades; i++) {
            e = &m->entidades[i];
            if (e->kind == PM_KIND_RECURSO) {
                p = buscar_propiedad(e, "ruta");
                if (!p || !p->valor[0]) {
                    snprintf(buf, sizeof(buf), "linea %d: recurso %d sin ruta",
                             e->origen.linea, e->id);
                    anotar_fallo(primera, sizeof(primera), &fallos, buf);
                    continue;
                }
                if (raiz && raiz[0])
                    snprintf(ruta, sizeof(ruta), "%s/%s", raiz, p->valor);
                else
                    copiar_seguro(ruta, sizeof(ruta), p->valor);
                f = fopen(ruta, "rb");
                if (f) fclose(f);
                else {
                    snprintf(buf, sizeof(buf),
                             "linea %d: recurso %d sin archivo: %s",
                             p->linea, e->id, p->valor);
                    anotar_fallo(primera, sizeof(primera), &fallos, buf);
                }
            } else if (e->kind == PM_KIND_ACTOR) {
                if (!actor_con_recurso(m, e->id)) {
                    snprintf(buf, sizeof(buf),
                             "linea %d: actor %d sin recurso conectado",
                             e->origen.linea, e->id);
                    anotar_fallo(primera, sizeof(primera), &fallos, buf);
                }
            }
        }
    }
    if (diagnostico && longitud > 0)
        copiar_seguro(diagnostico, longitud, fallos ? primera : "sin errores");
    return fallos;
}

/* Un fallo de recursos con los datos que necesitan tanto el diagnostico
 * de texto como el JSON. El codigo 4 representa la ausencia de modelo. */
typedef struct {
    int codigo; /* 1 sin ruta, 2 sin archivo, 3 actor sin recurso, 4 modelo */
    int linea;
    int id;
    char nombre[PM_NOMBRE_MAX];
    char ruta[PM_RUTA_MAX];
    char archivo[PM_RUTA_MAX];
} FalloRecurso;

static int validar_recursos_detallado(const ProjectModel *m, const char *raiz,
                                      FalloRecurso *fallos, int max) {
    const EntidadModelo *e;
    const PropiedadModelo *p;
    char ruta[PM_RUTA_MAX * 2 + 4];
    FILE *f;
    int n = 0;
    int i;

    if (!m) {
        if (n < max) {
            memset(&fallos[n], 0, sizeof(fallos[n]));
            fallos[n].codigo = 4;
            n++;
        }
        return n;
    }
    for (i = 0; i < m->num_entidades; i++) {
        e = &m->entidades[i];
        if (e->kind == PM_KIND_RECURSO) {
            p = buscar_propiedad(e, "ruta");
            if (!p || !p->valor[0]) {
                if (n < max) {
                    memset(&fallos[n], 0, sizeof(fallos[n]));
                    fallos[n].codigo = 1;
                    fallos[n].linea = e->origen.linea;
                    fallos[n].id = e->id;
                    copiar_seguro(fallos[n].nombre,
                                  sizeof(fallos[n].nombre), e->nombre);
                    copiar_seguro(fallos[n].archivo,
                                  sizeof(fallos[n].archivo),
                                  e->origen.archivo);
                    n++;
                }
                continue;
            }
            if (raiz && raiz[0])
                snprintf(ruta, sizeof(ruta), "%s/%s", raiz, p->valor);
            else
                copiar_seguro(ruta, sizeof(ruta), p->valor);
            f = fopen(ruta, "rb");
            if (f) fclose(f);
            else if (n < max) {
                memset(&fallos[n], 0, sizeof(fallos[n]));
                fallos[n].codigo = 2;
                fallos[n].linea = p->linea;
                fallos[n].id = e->id;
                copiar_seguro(fallos[n].nombre,
                              sizeof(fallos[n].nombre), e->nombre);
                copiar_seguro(fallos[n].ruta,
                              sizeof(fallos[n].ruta), p->valor);
                copiar_seguro(fallos[n].archivo,
                              sizeof(fallos[n].archivo),
                              e->origen.archivo);
                n++;
            }
        } else if (e->kind == PM_KIND_ACTOR) {
            if (!actor_con_recurso(m, e->id) && n < max) {
                memset(&fallos[n], 0, sizeof(fallos[n]));
                fallos[n].codigo = 3;
                fallos[n].linea = e->origen.linea;
                fallos[n].id = e->id;
                copiar_seguro(fallos[n].nombre,
                              sizeof(fallos[n].nombre), e->nombre);
                copiar_seguro(fallos[n].archivo,
                              sizeof(fallos[n].archivo),
                              e->origen.archivo);
                n++;
            }
        }
    }
    return n;
}

static const char *codigo_fallo(int codigo) {
    switch (codigo) {
    case 1: return "recurso_sin_ruta";
    case 2: return "recurso_sin_archivo";
    case 3: return "actor_sin_recurso";
    default: return "sin_modelo";
    }
}

/* La sugerencia es una orden concreta, no una explicacion: quien corrige el
 * .log sabe que propiedad tocar sin leer el codigo del validador. */
static const char *sugerencia_fallo(int codigo) {
    switch (codigo) {
    case 1: return "anade la propiedad ruta con una ruta relativa";
    case 2: return "crea el archivo indicado o corrige la ruta del recurso";
    case 3: return "conecta un recurso de imagen al actor";
    default: return "revisa el archivo de entrada del proyecto";
    }
}

static void json_anadir(char *dst, size_t tam, size_t *usado,
                        const char *formato, ...) {
    va_list args;
    int n;
    if (!dst || tam == 0 || !usado || *usado >= tam) return;
    va_start(args, formato);
    n = vsnprintf(dst + *usado, tam - *usado, formato, args);
    va_end(args);
    if (n < 0) {
        dst[*usado] = '\0';
        return;
    }
    if ((size_t)n >= tam - *usado) *usado = tam - 1;
    else *usado += (size_t)n;
    dst[*usado] = '\0';
}

/* Escapa comillas, contrabarras y caracteres de control para que el texto
 * del .log siga siendo un string JSON valido sin convertir codificacion. */
static void json_texte(char *dst, size_t tam, const char *src) {
    size_t j = 0;
    const unsigned char *p;
    if (!dst || tam == 0) return;
    if (!src) src = "";
    for (p = (const unsigned char *)src; *p; p++) {
        char extra[8];
        size_t n = 1;
        if (*p == '"' || *p == '\\') {
            extra[0] = '\\';
            extra[1] = (char)*p;
            n = 2;
        } else if (*p < 32) {
            snprintf(extra, sizeof(extra), "\\u%04x", (unsigned int)*p);
            n = 6;
        } else {
            extra[0] = (char)*p;
            extra[1] = '\0';
        }
        if (j + n + 1 >= tam) break;
        memcpy(dst + j, extra, n);
        j += n;
    }
    dst[j] = '\0';
}

static const char *json_proyecto(const ProjectModel *m) {
    int i;
    if (!m) return "";
    for (i = 0; i < m->num_entidades; i++)
        if (m->entidades[i].kind == PM_KIND_PROYECTO)
            return m->entidades[i].nombre;
    return "";
}

int pm_validar_recursos_json(const ProjectModel *m, const char *raiz,
                             char *json, size_t longitud) {
    FalloRecurso fallos[PM_ENTIDADES_MAX];
    char nombre[PM_NOMBRE_MAX * 6];
    char ruta[PM_RUTA_MAX * 6];
    char archivo[PM_RUTA_MAX * 6];
    char sugerencia[PM_TEXTO_MAX * 6];
    size_t usado = 0;
    int n;
    int i;

    if (!json || longitud == 0) return 0;
    json[0] = '\0';
    n = validar_recursos_detallado(m, raiz, fallos, PM_ENTIDADES_MAX);
    json_anadir(json, longitud, &usado,
                "{\"version\":1,\"proyecto\":\"");
    json_texte(nombre, sizeof(nombre), json_proyecto(m));
    json_anadir(json, longitud, &usado, "%s", nombre);
    json_anadir(json, longitud, &usado,
                "\",\"validacion\":\"recursos\",\"estado\":\"%s\","
                "\"total\":%d,\"errores\":[",
                n ? "error" : "ok", n);
    for (i = 0; i < n; i++) {
        if (i) json_anadir(json, longitud, &usado, ",");
        json_anadir(json, longitud, &usado, "{\"codigo\":\"%s\",\"archivo\":\"",
                    codigo_fallo(fallos[i].codigo));
        json_texte(archivo, sizeof(archivo), fallos[i].archivo);
        json_anadir(json, longitud, &usado,
                    "%s\",\"linea\":%d,\"id_entidad\":%d,\"nombre\":\"",
                    archivo, fallos[i].linea, fallos[i].id);
        json_texte(nombre, sizeof(nombre), fallos[i].nombre);
        json_anadir(json, longitud, &usado, "%s\"", nombre);
        if (fallos[i].codigo == 2) {
            json_anadir(json, longitud, &usado, ",\"ruta\":\"");
            json_texte(ruta, sizeof(ruta), fallos[i].ruta);
            json_anadir(json, longitud, &usado, "%s\"", ruta);
        }
        json_anadir(json, longitud, &usado, ",\"sugerencia\":\"");
        json_texte(sugerencia, sizeof(sugerencia),
                   sugerencia_fallo(fallos[i].codigo));
        json_anadir(json, longitud, &usado, "%s\"}", sugerencia);
    }
    json_anadir(json, longitud, &usado, "]}");
    return n;
}

void pm_volcar(const ProjectModel *m, const char *ruta) {
    FILE *f;
    char nombre[PM_NOMBRE_MAX];
    char valor[PM_TEXTO_MAX];
    char ruta_ref[PM_RUTA_MAX];
    char diagnostico[PM_ERROR_MAX];
    int conexiones = 0;
    int fallos;
    int i, j;

    if (!ruta || !ruta[0]) return;
    f = fopen(ruta, "w");
    if (!f) return;

    if (m) {
        for (i = 0; i < m->num_entidades; i++)
            if (m->entidades[i].kind == PM_KIND_CONEXION) conexiones++;
    }
    fallos = pm_validar(m, diagnostico, sizeof(diagnostico));
    limpiar_espacios(diagnostico);
    fprintf(f, "proyecto_modelo entidades=%d conexiones=%d validacion=%d diagnostico=%s\n",
            m ? m->num_entidades : 0, conexiones, fallos, diagnostico);

    if (!m) {
        fclose(f);
        return;
    }
    for (i = 0; i < m->num_entidades; i++) {
        const EntidadModelo *e = &m->entidades[i];
        copiar_seguro(nombre, sizeof(nombre), e->nombre);
        limpiar_espacios(nombre);
        copiar_seguro(ruta_ref, sizeof(ruta_ref), e->origen.archivo);
        limpiar_espacios(ruta_ref);
        if (!ruta_ref[0]) copiar_seguro(ruta_ref, sizeof(ruta_ref), "-");
        fprintf(f,
                "entidad=%d:%s:%s:origen=%s:%d:ui=%d,%d,%d,%d,%d,%08X,%d\n",
                e->id, pm_kind_nombre(e->kind), nombre, ruta_ref,
                e->origen.linea, e->ui.x, e->ui.y, e->ui.ancho, e->ui.alto,
                e->ui.grupo, e->ui.color, e->ui.plegado);
        for (j = 0; j < e->num_propiedades; j++) {
            const PropiedadModelo *p = &e->propiedades[j];
            copiar_seguro(nombre, sizeof(nombre), p->clave);
            copiar_seguro(valor, sizeof(valor), p->valor);
            limpiar_espacios(nombre);
            limpiar_espacios(valor);
            fprintf(f, "propiedad=%d:%s=%s:%d\n", e->id, nombre, valor, p->linea);
        }
        for (j = 0; j < e->num_referencias; j++) {
            const ReferenciaModelo *r = &e->referencias[j];
            copiar_seguro(nombre, sizeof(nombre), r->nombre);
            copiar_seguro(ruta_ref, sizeof(ruta_ref), r->ruta);
            limpiar_espacios(nombre);
            limpiar_espacios(ruta_ref);
            if (!ruta_ref[0]) copiar_seguro(ruta_ref, sizeof(ruta_ref), "-");
            fprintf(f, "referencia=%d:%s:%d:%s:%d\n",
                    e->id, nombre, r->id, ruta_ref, r->linea);
        }
        if (e->kind == PM_KIND_CONEXION) {
            const PropiedadModelo *po = buscar_propiedad(e, "puerto_origen");
            const PropiedadModelo *pd = buscar_propiedad(e, "puerto_destino");
            const ReferenciaModelo *ro = NULL;
            const ReferenciaModelo *rd = NULL;
            for (j = 0; j < e->num_referencias; j++) {
                if (strcmp(e->referencias[j].nombre, "origen") == 0)
                    ro = &e->referencias[j];
                else if (strcmp(e->referencias[j].nombre, "destino") == 0)
                    rd = &e->referencias[j];
            }
            fprintf(f, "conexion=%d:%d:%s:%d:%s:%d\n", e->id,
                    ro ? ro->id : 0, po ? po->valor : "",
                    rd ? rd->id : 0, pd ? pd->valor : "",
                    po ? po->linea : e->origen.linea);
        }
    }
    fclose(f);
}
