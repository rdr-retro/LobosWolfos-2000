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
    default:                return "desconocido";
    }
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

/* Un fallo de recursos con los datos que necesitan tanto el diagnostico
 * de texto como el JSON. El codigo 4 representa la ausencia de modelo. */
typedef struct {
    int codigo; /* 1 sin ruta, 2 sin archivo, 3 actor sin recurso, 4 modelo */
    int linea;
    int id;
    char nombre[PM_NOMBRE_MAX];
    char ruta[PM_RUTA_MAX];
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

static void texto_fallo(const FalloRecurso *f, char *dst, size_t tam) {
    if (!f || !dst || tam == 0) return;
    switch (f->codigo) {
    case 1:
        snprintf(dst, tam, "linea %d: recurso %d sin ruta",
                 f->linea, f->id);
        break;
    case 2:
        snprintf(dst, tam, "linea %d: recurso %d sin archivo: %s",
                 f->linea, f->id, f->ruta);
        break;
    case 3:
        snprintf(dst, tam, "linea %d: actor %d sin recurso conectado",
                 f->linea, f->id);
        break;
    default:
        copiar_seguro(dst, tam, "sin modelo");
        break;
    }
}

int pm_validar_recursos(const ProjectModel *m, const char *raiz,
                        char *diagnostico, size_t longitud) {
    FalloRecurso fallos[PM_ENTIDADES_MAX];
    char buf[PM_ERROR_MAX];
    int fallos_n = validar_recursos_detallado(m, raiz, fallos,
                                              PM_ENTIDADES_MAX);

    if (diagnostico && longitud > 0) {
        if (fallos_n > 0) {
            texto_fallo(&fallos[0], buf, sizeof(buf));
            copiar_seguro(diagnostico, longitud, buf);
        } else {
            copiar_seguro(diagnostico, longitud, "sin errores");
        }
    }
    return fallos_n;
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
        json_anadir(json, longitud, &usado,
                    "{\"codigo\":\"%s\",\"linea\":%d,"
                    "\"id_entidad\":%d,\"nombre\":\"",
                    codigo_fallo(fallos[i].codigo),
                    fallos[i].linea, fallos[i].id);
        json_texte(nombre, sizeof(nombre), fallos[i].nombre);
        json_anadir(json, longitud, &usado, "%s", nombre);
        json_anadir(json, longitud, &usado, "\"");
        if (fallos[i].codigo == 2) {
            json_anadir(json, longitud, &usado, ",\"ruta\":\"");
            json_texte(ruta, sizeof(ruta), fallos[i].ruta);
            json_anadir(json, longitud, &usado, "%s\"", ruta);
        }
        json_anadir(json, longitud, &usado, "}");
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
