/* ==========================================================================
 * Vista Estudio: modelo editable de escenas y objetos (fase 31).
 *
 * Este modulo trabaja sobre el proyecto LOGIC3 activo. No escribe archivos
 * por si mismo: `logic3_guardar_actual` sigue siendo el unico escritor, asi
 * que el formato, el orden de campos y el CRC no cambian. La seleccion, el
 * desplazamiento y los bloqueos son estado de editor y no se serializan.
 * ========================================================================== */
#include "app.h"
#include "logic3.h"

typedef struct {
    char ruta[MAX_PATH];
    int escena;
    int objeto;
    int scroll;
    int pestana;
} RecuerdoEstudio;

static RecuerdoEstudio recuerdos[ESTUDIO_RECUERDOS_MAX];
static int num_recuerdos = 0;

/* ------------------------------------------------------------------ */
/* Utilidades del modelo                                               */
/* ------------------------------------------------------------------ */
static void copiar_ruta(char *dst, const char *src) {
    int n = 0;
    if (!src) src = "";
    while (src[n] && n < MAX_PATH - 1) {
        dst[n] = src[n];
        n++;
    }
    dst[n] = 0;
}

static int recurso_valido(const char *ruta) {
    return ruta && ruta[0];
}

static int indice_recuerdo(const char *ruta) {
    int i;
    if (!ruta) return -1;
    for (i = 0; i < num_recuerdos; i++)
        if (strcmp(recuerdos[i].ruta, ruta) == 0) return i;
    return -1;
}

static void recordar_actual(const char *ruta) {
    int i = indice_recuerdo(ruta);
    if (!ruta) return;
    if (i < 0) {
        if (num_recuerdos >= ESTUDIO_RECUERDOS_MAX) {
            i = num_recuerdos - 1;
            memmove(&recuerdos[1], &recuerdos[0],
                    sizeof(recuerdos[0]) * (size_t)i);
        } else {
            i = num_recuerdos++;
        }
    }
    copiar_ruta(recuerdos[i].ruta, ruta);
    recuerdos[i].escena = g.estudio_escena;
    recuerdos[i].objeto = g.estudio_objeto;
    recuerdos[i].scroll = g.estudio_scroll;
    recuerdos[i].pestana = g.estudio_pestana;
}

static const Escena3 *escena_por_id(const Proyecto3 *p, int id) {
    int i;
    if (!p || id <= 0) return NULL;
    for (i = 0; i < p->num_escenas; i++)
        if (p->escenas[i].id == id) return &p->escenas[i];
    return NULL;
}

static int objeto_indice_por_id(const Proyecto3 *p, int id) {
    int i;
    if (!p || id <= 0 || !p->objetos) return -1;
    for (i = 0; i < p->num_objetos; i++)
        if (p->objetos[i].id == id) return i;
    return -1;
}

static int max_escena_id(const Proyecto3 *p) {
    int maximo = 0, i;
    for (i = 0; i < p->num_escenas; i++)
        if (p->escenas[i].id > maximo) maximo = p->escenas[i].id;
    return maximo;
}

static int max_objeto_id(const Proyecto3 *p) {
    int maximo = 0, i;
    for (i = 0; i < p->num_objetos; i++)
        if (p->objetos[i].id > maximo) maximo = p->objetos[i].id;
    return maximo;
}

static int max_evento_id(const Proyecto3 *p) {
    int maximo = 0, i, j;
    for (i = 0; i < p->num_objetos; i++)
        for (j = 0; j < p->objetos[i].num_eventos; j++)
            if (p->objetos[i].eventos[j].id > maximo)
                maximo = p->objetos[i].eventos[j].id;
    for (i = 0; i < p->num_funciones; i++)
        for (j = 0; j < p->funciones[i].num_eventos; j++)
            if (p->funciones[i].eventos[j].id > maximo)
                maximo = p->funciones[i].eventos[j].id;
    return maximo;
}

static int bloques_que_usan(const Proyecto3 *p, const Objeto3 *objetos,
                            int num_objetos) {
    int i, j, k;
    for (i = 0; i < p->num_bloques; i++)
        for (j = 0; j < num_objetos; j++)
            for (k = 0; k < objetos[j].num_eventos; k++)
                if (p->bloques[i].evento == objetos[j].eventos[k].id) return 1;
    return 0;
}

static void validar_seleccion(const Proyecto3 *p) {
    int i;
    if (!p) {
        g.estudio_escena = 0;
        g.estudio_objeto = 0;
        return;
    }
    if (!escena_por_id(p, g.estudio_escena)) {
        g.estudio_escena = p->num_escenas > 0 ? p->escenas[0].id : 0;
        g.estudio_objeto = 0;
    }
    if (objeto_indice_por_id(p, g.estudio_objeto) < 0) {
        g.estudio_objeto = 0;
        for (i = 0; i < p->num_objetos; i++)
            if (p->objetos[i].escena == g.estudio_escena) {
                g.estudio_objeto = p->objetos[i].id;
                break;
            }
    }
}

/* ------------------------------------------------------------------ */
/* Persistencia ligera del editor                                      */
/* ------------------------------------------------------------------ */
void estudio_proyecto_cambiar(const char *ruta) {
    int i;
    copiar_ruta(g.estudio_ruta, ruta);
    i = indice_recuerdo(g.estudio_ruta);
    if (i >= 0) {
        g.estudio_escena = recuerdos[i].escena;
        g.estudio_objeto = recuerdos[i].objeto;
        g.estudio_scroll = recuerdos[i].scroll;
        g.estudio_pestana = recuerdos[i].pestana;
    } else {
        g.estudio_escena = 0;
        g.estudio_objeto = 0;
        g.estudio_scroll = 0;
        g.estudio_pestana = 1;
        memset(g.estudio_bloqueos, 0, sizeof(g.estudio_bloqueos));
    }
    validar_seleccion(logic3_modelo_activo());
    bloques_reiniciar();
    app_invalidar();
}

void estudio_proyecto_guardar(const char *ruta) {
    copiar_ruta(g.estudio_ruta, ruta);
}

void estudio_guardar_estado(void) {
    if (recurso_valido(g.estudio_ruta)) recordar_actual(g.estudio_ruta);
}

/* ------------------------------------------------------------------ */
/* Mutaciones del proyecto                                            */
/* ------------------------------------------------------------------ */
int estudio_asegurar_modelo(void) {
    return logic3_nuevo_editor();
}

int estudio_filas_totales(void) {
    const Proyecto3 *p = logic3_modelo_activo();
    if (!p) return 0;
    return p->num_escenas + p->num_objetos;
}

int estudio_filas_visibles(void) {
    int y0 = AREA_SUP_ALTO + 74;
    int alto = g.alto - y0 - 8;
    int filas = alto / ESTUDIO_FILA_ALTO;
    if (filas < 1) filas = 1;
    return filas;
}

int estudio_fila_info(int fila, int *tipo, int *indice) {
    const Proyecto3 *p = logic3_modelo_activo();
    if (!p || fila < 0 || fila >= p->num_escenas + p->num_objetos) return 0;
    if (fila < p->num_escenas) {
        if (tipo) *tipo = 0;
        if (indice) *indice = fila;
    } else {
        if (tipo) *tipo = 1;
        if (indice) *indice = fila - p->num_escenas;
    }
    return 1;
}

void estudio_scroll_ver(int fila) {
    int visibles = estudio_filas_visibles();
    if (fila < 0) fila = 0;
    if (g.estudio_scroll > fila) g.estudio_scroll = fila;
    if (fila >= g.estudio_scroll + visibles)
        g.estudio_scroll = fila - visibles + 1;
    if (g.estudio_scroll < 0) g.estudio_scroll = 0;
    estudio_guardar_estado();
}

int estudio_escena_nueva(void) {
    Proyecto3 *p = logic3_modelo_activo();
    Escena3 *e;
    if (!estudio_asegurar_modelo()) return 0;
    p = logic3_modelo_activo();
    if (!p || p->num_escenas >= L3_ESCENAS_MAX) {
        app_estado("No caben mas escenas");
        return 0;
    }
    e = &p->escenas[p->num_escenas++];
    memset(e, 0, sizeof(*e));
    e->id = max_escena_id(p) + 1;
    formatear(e->nombre, sizeof(e->nombre), "Escena %d", e->id);
    e->ancho = p->ancho;
    e->alto = p->alto;
    e->fondo = p->fondo;
    g.estudio_escena = e->id;
    g.estudio_objeto = 0;
    estudio_scroll_ver(p->num_escenas - 1);
    estudio_guardar_estado();
    app_invalidar();
    app_estado("Escena creada: %s", e->nombre);
    return 1;
}

int estudio_objeto_nuevo(void) {
    Proyecto3 *p;
    Objeto3 *o;
    Objeto3 *tmp;
    Evento3 *ev;
    int escena, id, sig, i;

    if (!estudio_asegurar_modelo()) return 0;
    p = logic3_modelo_activo();
    if (!p) return 0;
    if (p->num_escenas == 0 && !estudio_escena_nueva()) return 0;
    p = logic3_modelo_activo();
    if (p->num_objetos >= L3_OBJETOS_MAX) {
        app_estado("No caben mas objetos");
        return 0;
    }
    tmp = (Objeto3 *)realloc(p->objetos,
                             sizeof(Objeto3) * (size_t)(p->num_objetos + 1));
    if (!tmp) {
        app_estado("Sin memoria para el objeto");
        return 0;
    }
    p->objetos = tmp;
    p->cap_objetos = p->num_objetos + 1;
    o = &p->objetos[p->num_objetos++];
    memset(o, 0, sizeof(*o));
    id = max_objeto_id(p) + 1;
    if (!escena_por_id(p, g.estudio_escena))
        g.estudio_escena = p->escenas[0].id;
    escena = g.estudio_escena;
    o->id = id;
    formatear(o->nombre, sizeof(o->nombre), "Objeto %d", id);
    o->escena = escena;
    o->x = 16 + (p->num_objetos % 8) * 12;
    o->y = 16 + (p->num_objetos % 6) * 10;
    o->w = 32;
    o->h = 32;
    o->visible = 1;
    o->capa = 0;

    /* Eventos iniciales: dejan el objeto listo para recibir bloques sin
     * inventar datos nuevos en el archivo. */
    sig = max_evento_id(p) + 1;
    ev = &o->eventos[o->num_eventos++];
    memset(ev, 0, sizeof(*ev));
    ev->id = sig++;
    ev->tipo = L3_EVENTO_INICIO;
    ev = &o->eventos[o->num_eventos++];
    memset(ev, 0, sizeof(*ev));
    ev->id = sig;
    ev->tipo = L3_EVENTO_FOTOGRAMA;

    g.estudio_objeto = o->id;
    g.estudio_escena = escena;
    i = p->num_escenas + p->num_objetos - 1;
    estudio_scroll_ver(i);
    estudio_guardar_estado();
    app_invalidar();
    app_estado("Objeto creado: %s", o->nombre);
    return 1;
}

int estudio_traje_importar(void) {
    Proyecto3 *p = logic3_modelo_activo();
    int indice, i;
    Traje3 *t;
    if (!p) {
        app_estado("Abre o crea un proyecto antes de importar");
        return 0;
    }
    indice = objeto_indice_por_id(p, g.estudio_objeto);
    if (indice < 0) {
        app_estado("Selecciona un objeto para importar su traje");
        return 0;
    }
    if (estudio_esta_bloqueado(g.estudio_objeto)) {
        app_estado("El objeto esta bloqueado");
        return 0;
    }
    if (p->objetos[indice].num_trajes >= L3_TRAJES_MAX) {
        app_estado("No caben mas trajes");
        return 0;
    }
    i = p->objetos[indice].num_trajes++;
    t = &p->objetos[indice].trajes[i];
    memset(t, 0, sizeof(*t));
    t->indice = i;
    snprintf(t->imagen, sizeof(t->imagen), "%s", "trajes/lobo.png");
    t->ancho = 32;
    t->alto = 32;
    t->pivote_x = 16;
    t->pivote_y = 16;
    t->cuadros = 1;
    t->cuadro_ancho = 32;
    t->cuadro_alto = 32;
    t->velocidad = 1;
    app_invalidar();
    app_estado("Traje importado en %s", p->objetos[indice].nombre);
    return 1;
}

int estudio_borrar_seleccion(void) {
    Proyecto3 *p = logic3_modelo_activo();
    int indice, escena, i, j;
    if (!p) return 0;

    indice = objeto_indice_por_id(p, g.estudio_objeto);
    if (indice >= 0) {
        if (!bloques_que_usan(p, &p->objetos[indice], 1)) {
            escena = p->objetos[indice].escena;
            memmove(&p->objetos[indice], &p->objetos[indice + 1],
                    sizeof(Objeto3) * (size_t)(p->num_objetos - indice - 1));
            p->num_objetos--;
            g.estudio_objeto = 0;
            g.estudio_escena = escena;
            validar_seleccion(p);
            app_estado("Objeto borrado");
        } else {
            app_estado("El objeto tiene bloques: no se puede borrar");
            return 0;
        }
    } else if (escena_por_id(p, g.estudio_escena)) {
        Objeto3 *temporales;
        int num_temporales = 0;
        temporales = (Objeto3 *)malloc(sizeof(Objeto3) *
                                       (size_t)(p->num_objetos + 1));
        if (!temporales) {
            app_estado("Sin memoria para borrar la escena");
            return 0;
        }
        for (i = 0; i < p->num_objetos; i++)
            if (p->objetos[i].escena == g.estudio_escena)
                temporales[num_temporales++] = p->objetos[i];
        if (bloques_que_usan(p, temporales, num_temporales)) {
            free(temporales);
            app_estado("La escena tiene bloques: no se puede borrar");
            return 0;
        }
        free(temporales);
        j = 0;
        for (i = 0; i < p->num_objetos; i++) {
            if (p->objetos[i].escena == g.estudio_escena) continue;
            if (j != i) p->objetos[j] = p->objetos[i];
            j++;
        }
        p->num_objetos = j;
        for (i = 0; i < p->num_escenas; i++)
            if (p->escenas[i].id == g.estudio_escena) {
                memmove(&p->escenas[i], &p->escenas[i + 1],
                        sizeof(Escena3) * (size_t)(p->num_escenas - i - 1));
                p->num_escenas--;
                break;
            }
        g.estudio_escena = 0;
        g.estudio_objeto = 0;
        validar_seleccion(p);
        app_estado("Escena borrada");
    } else {
        return 0;
    }
    if (g.estudio_scroll >= estudio_filas_totales())
        g.estudio_scroll = estudio_filas_totales() ? estudio_filas_totales() - 1 : 0;
    if (g.estudio_scroll < 0) g.estudio_scroll = 0;
    estudio_guardar_estado();
    app_invalidar();
    return 1;
}

/* ------------------------------------------------------------------ */
/* Seleccion y bloqueo                                                 */
/* ------------------------------------------------------------------ */
void estudio_seleccionar_escena(int id) {
    Proyecto3 *p = logic3_modelo_activo();
    int i;
    if (!p || !escena_por_id(p, id)) return;
    g.estudio_escena = id;
    g.estudio_objeto = 0;
    for (i = 0; i < p->num_objetos; i++)
        if (p->objetos[i].escena == id) {
            g.estudio_objeto = p->objetos[i].id;
            break;
        }
    for (i = 0; i < p->num_escenas; i++)
        if (p->escenas[i].id == id) {
            estudio_scroll_ver(i);
            break;
        }
    estudio_guardar_estado();
    app_invalidar();
}

void estudio_seleccionar_objeto(int id) {
    Proyecto3 *p = logic3_modelo_activo();
    int indice = objeto_indice_por_id(p, id);
    if (!p || indice < 0) return;
    g.estudio_objeto = p->objetos[indice].id;
    g.estudio_escena = p->objetos[indice].escena;
    estudio_scroll_ver(p->num_escenas + indice);
    estudio_guardar_estado();
    app_invalidar();
}

void estudio_fila_seleccionar(int fila) {
    Proyecto3 *p = logic3_modelo_activo();
    int tipo = -1, indice = -1;
    if (!p || !estudio_fila_info(fila, &tipo, &indice)) return;
    if (tipo == 0) estudio_seleccionar_escena(p->escenas[indice].id);
    else estudio_seleccionar_objeto(p->objetos[indice].id);
}

void estudio_alternar_bloqueo(int id) {
    if (id <= 0 || id > ESTUDIO_BLOQUEOS_MAX) return;
    g.estudio_bloqueos[id] = g.estudio_bloqueos[id] ? 0 : 1;
    app_invalidar();
}

int estudio_esta_bloqueado(int id) {
    if (id <= 0 || id > ESTUDIO_BLOQUEOS_MAX) return 0;
    return g.estudio_bloqueos[id] ? 1 : 0;
}
