/* runtime_comun.c - reglas del runtime compartidas por preview y game.exe.
 *
 * Ver runtime_comun.h. Todo lo que aqui se hace debe dar el mismo resultado
 * en la ventana de prueba del editor y en el ejecutable, por eso no depende
 * de Win32 y solo usa los modulos C puros del motor.
 */
#include <stdio.h>
#include <string.h>

#include "runtime_comun.h"
#include "entidades.h"
#include "programa.h"

/* ------------------------------------------------------------------ */
/* Cola FIFO de cambios de escena                                      */
/* ------------------------------------------------------------------ */

void runtime_cola_iniciar(RuntimeColaEscenas *cola) {
    if (cola) memset(cola, 0, sizeof(*cola));
}

int runtime_cola_num(const RuntimeColaEscenas *cola) {
    return cola ? cola->num : 0;
}

int runtime_cola_meter(RuntimeColaEscenas *cola, int id) {
    int pos, ultimo;
    if (!cola || id <= 0) return 0;
    /* Un mismo id repetido al final de la cola se coalesce: mientras una
     * transicion retrasa el drenaje, un evento fotograma puede pedir la
     * escena de destino varias veces. Solo la primera peticion cuenta. */
    if (cola->num > 0) {
        ultimo = (cola->ini + cola->num - 1) % RUNTIME_COLA_ESCENAS_MAX;
        if (cola->id[ultimo] == id) return 1;
    }
    if (cola->num >= RUNTIME_COLA_ESCENAS_MAX) {
        cola->descartados++;
        return 0;
    }
    pos = (cola->ini + cola->num) % RUNTIME_COLA_ESCENAS_MAX;
    cola->id[pos] = id;
    cola->num++;
    return 1;
}

int runtime_cola_sacar(RuntimeColaEscenas *cola) {
    int id;
    if (!cola || cola->num <= 0) return 0;
    id = cola->id[cola->ini];
    cola->ini = (cola->ini + 1) % RUNTIME_COLA_ESCENAS_MAX;
    cola->num--;
    return id;
}

/* ------------------------------------------------------------------ */
/* Variables de entidad y memoria de la VM                             */
/* ------------------------------------------------------------------ */

static char g_ent_nombres[LOBO_ENTIDAD_NOMBRES_MAX][LOBO_TEXTO_MAX];
static int g_num_ent_nombres;

void runtime_var_slots_reiniciar(void) {
    g_num_ent_nombres = 0;
    memset(g_ent_nombres, 0, sizeof(g_ent_nombres));
}

int runtime_var_slot(const char *nombre) {
    int i;
    if (!nombre) return -1;
    for (i = 0; i < g_num_ent_nombres; i++)
        if (strcmp(g_ent_nombres[i], nombre) == 0) return i;
    if (g_num_ent_nombres >= LOBO_ENTIDAD_NOMBRES_MAX) {
        entidades_aviso();
        return -1;
    }
    strncpy(g_ent_nombres[g_num_ent_nombres], nombre, LOBO_TEXTO_MAX - 1);
    g_ent_nombres[g_num_ent_nombres][LOBO_TEXTO_MAX - 1] = 0;
    return g_num_ent_nombres++;
}

void runtime_remap_programa_cb(void *ctx, LoboPrograma *p) {
    int mapa[LOBO_ENTIDAD_NOMBRES_MAX];
    int i;
    (void)ctx;
    if (!p) return;
    for (i = 0; i < p->num_ent_nombres; i++)
        mapa[i] = runtime_var_slot(p->entidad_nombres[i]);
    for (i = 0; i < p->largo; i++) {
        LoboInst *in = &p->insts[i];
        if (in->op == LOBO_CARGAR_ENTIDAD || in->op == LOBO_GUARDAR_ENTIDAD) {
            if (in->valor >= 0 && in->valor < p->num_ent_nombres &&
                mapa[in->valor] >= 0)
                in->valor = mapa[in->valor];
        } else if (in->op == LOBO_CARGAR_MI || in->op == LOBO_GUARDAR_MI) {
            if (in->valor >= LOBO_MI_VARS) {
                int slot = in->valor - LOBO_MI_VARS;
                if (slot >= 0 && slot < p->num_ent_nombres &&
                    mapa[slot] >= 0)
                    in->valor = LOBO_MI_VARS + mapa[slot];
            }
        }
    }
}

void runtime_memoria_preparar(LoboMemoria *memoria, const MotorL3 *proy) {
    int i, j;
    if (!memoria || !proy) return;
    memset(memoria, 0, sizeof(*memoria));
    for (i = 0; i < proy->num_textos && i < LOBO_TEXTOS_RUNTIME_MAX; i++) {
        strncpy(memoria->textos[i], proy->textos[i], LOBO_TEXTO_MAX - 1);
        memoria->textos[i][LOBO_TEXTO_MAX - 1] = 0;
    }
    memoria->num_textos = proy->num_textos;
    for (i = 0; i < proy->num_variables && i < LOBO_GLOBALES_MAX; i++) {
        const MotorL3Variable *v = &proy->variables[i];
        int valor = v->valor;
        if (v->tipo == 2 && valor >= 0 && valor < proy->num_textos)
            valor = (int)(LOBO_TEXTO_MARCA |
                          (unsigned)(LOBO_TEXTOS_MAX + valor));
        memoria->globales[i] = valor;
    }
    for (i = 0; i < proy->num_listas && i < LOBO_LISTAS_MAX; i++) {
        const MotorL3Lista *li = &proy->listas[i];
        LoboLista *dst = &memoria->listas[i];
        int n = li->num_elementos;
        if (n > li->capacidad) n = li->capacidad;
        if (n > LOBO_LISTA_ELEMENTOS_MAX) n = LOBO_LISTA_ELEMENTOS_MAX;
        for (j = 0; j < n; j++) {
            int valor = li->elementos[j];
            if (li->tipo == 2 && valor >= 0 && valor < proy->num_textos)
                valor = (int)(LOBO_TEXTO_MARCA |
                              (unsigned)(LOBO_TEXTOS_MAX + valor));
            dst->elem[j] = valor;
        }
        dst->n = n;
    }
}

/* ------------------------------------------------------------------ */
/* Callbacks de entidades y colisiones expuestos a lobo                 */
/* ------------------------------------------------------------------ */

static int campo_entidad_lobo(int campo) {
    switch (campo) {
    case LOBO_MI_X: return ENTIDAD_CAMPO_X;
    case LOBO_MI_Y: return ENTIDAD_CAMPO_Y;
    case LOBO_MI_VIDA: return ENTIDAD_CAMPO_VIDA;
    case LOBO_MI_VISIBLE: return ENTIDAD_CAMPO_VISIBLE;
    case LOBO_MI_TRAJE: return ENTIDAD_CAMPO_TRAJE;
    case LOBO_MI_ID: return ENTIDAD_CAMPO_ID;
    case LOBO_MI_ESCENA: return ENTIDAD_CAMPO_ESCENA;
    case LOBO_MI_ANCHO: return ENTIDAD_CAMPO_W;
    case LOBO_MI_ALTO: return ENTIDAD_CAMPO_H;
    case LOBO_MI_CAPA: return ENTIDAD_CAMPO_CAPA;
    default: return -1;
    }
}

static int ent_leer(void *ctx, int id, int campo) {
    (void)ctx;
    if (campo >= LOBO_MI_VARS)
        return entidades_leer_var(id, campo - LOBO_MI_VARS);
    campo = campo_entidad_lobo(campo);
    return campo < 0 ? 0 : entidades_leer_campo(id, campo);
}

static int ent_escribir(void *ctx, int id, int campo, int valor) {
    (void)ctx;
    if (campo >= LOBO_MI_VARS)
        return entidades_escribir_var(id, campo - LOBO_MI_VARS, valor);
    campo = campo_entidad_lobo(campo);
    if (campo < 0 || campo == ENTIDAD_CAMPO_ID ||
        campo == ENTIDAD_CAMPO_TIPO)
        return 0;
    return entidades_escribir_campo(id, campo, valor);
}

static int ent_crear(void *ctx, const char *tag, int x, int y) {
    int tipo;
    (void)ctx;
    tipo = entidades_tipo_por_tag(tag);
    if (tipo < 0) {
        entidades_aviso();
        return -1;
    }
    return entidad_crear(tipo, x, y);
}

static int ent_borrar(void *ctx, int id) {
    (void)ctx;
    return entidad_borrar(id);
}

static int ent_clonar(void *ctx, int id) {
    (void)ctx;
    return entidades_clonar(id);
}

static int ent_mover(void *ctx, int id, int x, int y) {
    (void)ctx;
    return entidad_mover(id, x, y);
}

static int col_toca(void *ctx, int entidad, int id) {
    (void)ctx;
    return entidades_tocan(entidad, id);
}

static int col_toca_tag(void *ctx, int entidad, const char *tag) {
    (void)ctx;
    return entidades_toca_tag(entidad, tag);
}

static int col_distancia(void *ctx, int entidad, int id) {
    (void)ctx;
    return entidades_distancia(entidad, id);
}

static int col_caja_de(void *ctx, int entidad, int id, int indice) {
    (void)ctx;
    (void)entidad;
    return entidades_caja(id, indice);
}

void runtime_estado_entidades(LoboEstado *st) {
    if (!st) return;
    st->entidad_crear = ent_crear;
    st->entidad_borrar = ent_borrar;
    st->entidad_clonar = ent_clonar;
    st->entidad_mover = ent_mover;
    st->entidad_leer = ent_leer;
    st->entidad_escribir = ent_escribir;
    st->ctx_entidades = NULL;
    st->toca = col_toca;
    st->toca_tag = col_toca_tag;
    st->distancia = col_distancia;
    st->caja_de = col_caja_de;
    st->ctx_colisiones = NULL;
}

/* ------------------------------------------------------------------ */
/* Instanciacion del arbol                                             */
/* ------------------------------------------------------------------ */

int runtime_instanciar_escena(const MotorL3 *proy, int escena) {
    int pasada, i, creadas = 0;
    if (!proy) return 0;
    /* Los objetos pueden aparecer en cualquier orden en LOGIC3. Se crean
     * por niveles para que un hijo reciba la transformacion global de su
     * padre aunque el archivo declare primero al hijo. */
    for (pasada = 0; pasada <= proy->num_objetos; pasada++) {
        int progreso = 0;
        for (i = 0; i < proy->num_objetos; i++) {
            const MotorL3Objeto *o = &proy->objetos[i];
            int tipo, padre = 0;
            if (o->escena != escena ||
                entidad_de_objeto_escena(o->id, escena) > 0)
                continue;
            if (o->padre > 0) {
                padre = entidad_de_objeto_escena(o->padre, escena);
                if (padre <= 0) continue;
            }
            tipo = entidades_tipo_por_objeto(o->id);
            if (tipo >= 0 && entidad_crear_hijo(tipo, o->x, o->y, padre) > 0) {
                progreso = 1;
                creadas++;
            }
        }
        if (!progreso) break;
    }
    entidades_recalcular_jerarquia();
    return creadas;
}

int runtime_preparar_entidades(const MotorL3 *proy, int escena) {
    int i, j;
    if (!proy) return 0;
    entidades_reiniciar();
    runtime_var_slots_reiniciar();

    for (i = 0; i < proy->num_objetos; i++) {
        const MotorL3Objeto *o = &proy->objetos[i];
        int tipo = entidades_tipo_agregar(o->id, o->nombre, o->escena,
                                          o->padre, o->x, o->y, o->w, o->h,
                                          o->visible, o->capa);
        if (tipo < 0) continue;
        for (j = 0; j < o->num_variables; j++) {
            int slot = runtime_var_slot(o->variables[j].nombre);
            if (slot >= 0)
                entidades_tipo_var(tipo, slot, o->variables[j].nombre,
                                   o->variables[j].valor);
        }
    }
    runtime_instanciar_escena(proy, escena);
    /* El cupo de altas por fotograma empieza limpio en el fotograma 1. */
    entidades_fin_fotograma();
    return entidades_num();
}

/* ------------------------------------------------------------------ */
/* Camara y volcado                                                    */
/* ------------------------------------------------------------------ */

void runtime_camara_centro(const MotorL3 *proy, int ancho, int alto,
                           int *x, int *y) {
    const MotorL3Control *c;
    int ids[4];
    int n;
    Entidad *e;
    if (!x || !y) return;
    *x = ancho / 2;
    *y = alto / 2;
    if (!proy) return;
    c = motor_programa_control_principal(proy);
    if (!c || c->actor <= 0) return;
    n = entidades_de_objeto(c->actor, ids, 4);
    if (n < 1) return;
    e = entidad_buscar(ids[0]);
    if (!e) return;
    *x = e->x + (e->w > 0 ? e->w / 2 : 8);
    *y = e->y + (e->h > 0 ? e->h / 2 : 8);
}

/* Volcado determinista de la jerarquia viva. Lo usan las pruebas para
 * comparar preview y ejecutable con el mismo texto. */
void runtime_volcar_jerarquia(FILE *f, const MotorL3 *proy) {
    int i, num = 0, escena = proy ? proy->escena_inicial : 0;
    if (!f) return;
    for (i = 0; i < entidades_max(); i++) {
        Entidad *e = entidad_indice(i);
        if (!e || !e->usada || e->borrar) continue;
        escena = e->escena;
        num++;
    }
    fprintf(f, "runtime: jerarquia escena %d entidades %d descartados %d\n",
            escena, num, 0);
    for (i = 0; i < entidades_max(); i++) {
        Entidad *e = entidad_indice(i);
        const char *nombre;
        if (!e || !e->usada || e->borrar) continue;
        nombre = entidades_tipo_nombre(e->tipo);
        fprintf(f,
                "runtime: entidad %d tipo %d nombre %s padre %d local %d %d "
                "global %d %d capa %d visible %d\n",
                e->id, entidades_tipo_objeto(e->tipo),
                nombre ? nombre : "", e->padre, e->local_x, e->local_y,
                e->x, e->y, e->capa, e->visible);
    }
}
