/* ==========================================================================
 * Lienzo de bloques: modelo, historia y layout (fase 32).
 *
 * El modelo de bloques vive aparte del proyecto LOGIC3. Cada mutacion vuelca
 * los nodos a `p->bloques` con el escritor existente, asi que el formato y el
 * CRC de LOGIC3 no cambian. Los colores y la geometria estan aqui para que el
 * dibujo (bloques_ui.c) solo pinte cajas.
 * ========================================================================== */
#include "app.h"
#include "logic3.h"

/* Formas visuales del catalogo. */
enum {
    FORMA_HAT = 0,
    FORMA_PILA,
    FORMA_C,
    FORMA_REPORTERO,
    FORMA_BOOLEANO,
    FORMA_CAP
};

typedef struct {
    const char *tipo;
    const char *nombre;
    int categoria;
    int forma;
    u32 color;
    int campo[2];
    const char *campo_nombre[2];
    const char *slot[2];
} DefBloque;

typedef struct {
    int id;
    int def;
    int padre;
    int orden;
    char slot[BLOQUES_SLOT_MAX];
    int x, y;
    int abierto;
    char campo[2][BLOQUES_CAMPO_MAX];
} NodoBloque;

typedef struct {
    NodoBloque nodos[BLOQUES_MAX];
    int num;
    int siguiente;
    int sel;
} CopiaBloques;

static const char *CATEGORIAS[BLOQUES_CATEGORIAS] = {
    "EVENTOS", "CONTROL", "MOVIMIENTO", "DATOS", "OPERADORES", "CODIGO"
};

static const DefBloque DEFS[BLOQUES_DEFS] = {
    { "eventos.inicio", "al inicio", 0, FORMA_HAT, 0x0043A047u,
      { 0, 0 }, { "", "" }, { "", "" } },
    { "eventos.tecla", "al pulsar tecla", 0, FORMA_HAT, 0x0043A047u,
      { 1, 0 }, { "tecla", "" }, { "", "" } },
    { "eventos.recibir", "al recibir", 0, FORMA_HAT, 0x0043A047u,
      { 1, 0 }, { "mensaje", "" }, { "", "" } },
    { "control.repetir_siempre", "repetir siempre", 1, FORMA_C, 0x006A3FA0u,
      { 0, 0 }, { "", "" }, { "cuerpo", "" } },
    { "control.si", "si", 1, FORMA_C, 0x006A3FA0u,
      { 0, 0 }, { "", "" }, { "cuerpo", "condicion" } },
    { "movimiento.mover_x", "mover x", 2, FORMA_PILA, 0x003278E0u,
      { 1, 0 }, { "dx", "" }, { "dx", "" } },
    { "datos.cambiar_variable", "cambiar variable", 3, FORMA_PILA, 0x003278E0u,
      { 1, 1 }, { "variable", "delta" }, { "delta", "" } },
    { "operadores.suma", "suma", 4, FORMA_REPORTERO, 0x00E0A020u,
      { 1, 1 }, { "a", "b" }, { "", "" } },
    { "operadores.mayor", "mayor que", 4, FORMA_BOOLEANO, 0x00C05080u,
      { 1, 1 }, { "a", "b" }, { "", "" } },
    { "codigo.lobo", "codigo lobo", 5, FORMA_CAP, 0x00505058u,
      { 1, 0 }, { "codigo", "" }, { "", "" } }
};

static NodoBloque nodos[BLOQUES_MAX];
static int num_nodos = 0;
static int siguiente_id = BLOQUES_ID_INICIAL;
static int inicializado = 0;

static CopiaBloques historial[BLOQUES_HISTORIAL_MAX];
static int num_historial = 0;
static CopiaBloques futuro[BLOQUES_HISTORIAL_MAX];
static int num_futuro = 0;

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */
static void copiar_texto(char *dst, int tam, const char *src) {
    int n = 0;
    if (!src) src = "";
    while (src[n] && n < tam - 1) {
        dst[n] = src[n];
        n++;
    }
    dst[n] = 0;
}

static void asegurar(void) {
    if (inicializado) return;
    inicializado = 1;
    bloques_iniciar();
}

static int indice_de_id(int id) {
    int i;
    if (id <= 0) return -1;
    for (i = 0; i < num_nodos; i++)
        if (nodos[i].id == id) return i;
    return -1;
}

static int def_por_tipo(const char *tipo) {
    int i;
    if (!tipo) return -1;
    for (i = 0; i < BLOQUES_DEFS; i++)
        if (strcmp(DEFS[i].tipo, tipo) == 0) return i;
    return -1;
}

static int es_antepasado(int candidato, int id, int profundo) {
    int i;
    if (candidato <= 0 || profundo > 16) return 0;
    i = indice_de_id(candidato);
    if (i < 0) return 0;
    if (nodos[i].padre == id) return 1;
    return es_antepasado(nodos[i].padre, id, profundo + 1);
}

static void tomar_nota(void) {
    CopiaBloques *c;
    if (num_historial >= BLOQUES_HISTORIAL_MAX) {
        memmove(&historial[0], &historial[1],
                sizeof(historial[0]) * (size_t)(BLOQUES_HISTORIAL_MAX - 1));
        num_historial = BLOQUES_HISTORIAL_MAX - 1;
    }
    c = &historial[num_historial++];
    memcpy(c->nodos, nodos, sizeof(nodos));
    c->num = num_nodos;
    c->siguiente = siguiente_id;
    c->sel = g.bloques_sel;
    num_futuro = 0;
}

static void restaurar(const CopiaBloques *c) {
    memcpy(nodos, c->nodos, sizeof(nodos));
    num_nodos = c->num;
    siguiente_id = c->siguiente;
    g.bloques_sel = c->sel;
    g.bloques_arrastre = -1;
    g.bloques_desde_paleta = -1;
}

static void valor_por_defecto(int def, int campo, char *dst, int tam) {
    if (def < 0 || def >= BLOQUES_DEFS) {
        copiar_texto(dst, tam, "0");
        return;
    }
    if (def == 5 && campo == 0) copiar_texto(dst, tam, "2");
    else if (def == 6 && campo == 0) copiar_texto(dst, tam, "vidas");
    else if (def == 6 && campo == 1) copiar_texto(dst, tam, "-10");
    else if (def == 7 || def == 8) copiar_texto(dst, tam, "0");
    else if (def == 9 && campo == 0) copiar_texto(dst, tam, "");
    else copiar_texto(dst, tam, "0");
}

/* ------------------------------------------------------------------ */
/* Volcado al proyecto LOGIC3 (sin tocar el escritor)                   */
/* ------------------------------------------------------------------ */
static int hay_eventos(const Proyecto3 *p) {
    int i;
    if (!p) return 0;
    if (p->objetos)
        for (i = 0; i < p->num_objetos; i++)
            if (p->objetos[i].num_eventos > 0) return 1;
    for (i = 0; i < p->num_funciones; i++)
        if (p->funciones[i].num_eventos > 0) return 1;
    return 0;
}

static int evento_para_bloques(const Proyecto3 *p) {
    int i;
    if (p && g.estudio_objeto > 0 && p->objetos) {
        for (i = 0; i < p->num_objetos; i++) {
            if (p->objetos[i].id == g.estudio_objeto &&
                p->objetos[i].num_eventos > 0)
                return p->objetos[i].eventos[0].id;
        }
    }
    if (p && p->objetos) {
        for (i = 0; i < p->num_objetos; i++)
            if (p->objetos[i].num_eventos > 0)
                return p->objetos[i].eventos[0].id;
    }
    return 100;
}

void bloques_volcar_modelo(void) {
    Proyecto3 *p;
    Bloque3 *tmp;
    int i, j, n, ev, cap;

    asegurar();
    if (!logic3_activo() && !estudio_asegurar_modelo()) return;
    p = logic3_modelo_activo();
    if (!p) return;
    /* LOGIC3 exige que todo bloque referencie un evento existente. Si el
     * usuario empieza a dibujar en un proyecto vacio, se crea el objeto
     * inicial con sus eventos por defecto. */
    if (!hay_eventos(p)) {
        estudio_objeto_nuevo();
        p = logic3_modelo_activo();
        if (!p) return;
    }

    n = num_nodos;
    if (n > L3_BLOQUES_MAX) n = L3_BLOQUES_MAX;
    if (n > p->cap_bloques) {
        cap = p->cap_bloques > 0 ? p->cap_bloques : 64;
        while (cap < n) cap *= 2;
        tmp = (Bloque3 *)realloc(p->bloques, sizeof(Bloque3) * (size_t)cap);
        if (!tmp) return;
        p->bloques = tmp;
        p->cap_bloques = cap;
    }

    ev = evento_para_bloques(p);
    for (i = 0; i < n; i++) {
        Bloque3 *b = &p->bloques[i];
        const DefBloque *d = &DEFS[nodos[i].def];
        memset(b, 0, sizeof(*b));
        b->id = nodos[i].id;
        b->evento = ev;
        b->padre = nodos[i].padre;
        b->orden = nodos[i].orden;
        copiar_texto(b->tipo, sizeof(b->tipo), d->tipo);
        for (j = 0; j < 2 && b->num_campos < L3_CAMPOS_MAX; j++) {
            if (!d->campo[j] || !nodos[i].campo[j][0]) continue;
            copiar_texto(b->campos[b->num_campos].clave,
                         sizeof(b->campos[b->num_campos].clave),
                         d->campo_nombre[j]);
            copiar_texto(b->campos[b->num_campos].valor,
                         sizeof(b->campos[b->num_campos].valor),
                         nodos[i].campo[j]);
            b->num_campos++;
        }
        /* El cuerpo se describe solo con `padre`; la lista `hijo` del
         * formato es redundante y su lector actual exige tres tokens, asi
         * que no se escribe. Las ranuras si guardan el id del reportero. */
        for (j = 0; j < num_nodos && b->num_ranuras < L3_RANURAS_MAX; j++) {
            int ya = 0, k;
            if (nodos[j].padre != nodos[i].id) continue;
            if (strcmp(nodos[j].slot, "cuerpo") == 0) continue;
            for (k = 0; k < b->num_ranuras; k++)
                if (strcmp(b->ranuras[k].clave, nodos[j].slot) == 0) ya = 1;
            if (ya) continue;
            copiar_texto(b->ranuras[b->num_ranuras].clave,
                         sizeof(b->ranuras[b->num_ranuras].clave),
                         nodos[j].slot);
            b->ranuras[b->num_ranuras].bloque = nodos[j].id;
            b->num_ranuras++;
        }
        b->linea = 0;
    }
    p->num_bloques = n;
}

void bloques_importar_modelo(void) {
    const Proyecto3 *p;
    int i, j, k;

    asegurar();
    if (g.bloques_modelo_cargado) return;
    if (!logic3_activo()) return;
    p = logic3_modelo_activo();
    if (!p || !p->bloques) return;

    num_nodos = 0;
    siguiente_id = BLOQUES_ID_INICIAL;
    num_historial = 0;
    num_futuro = 0;
    for (i = 0; i < p->num_bloques && num_nodos < BLOQUES_MAX; i++) {
        const Bloque3 *b = &p->bloques[i];
        NodoBloque *n = &nodos[num_nodos++];
        int def = def_por_tipo(b->tipo);
        memset(n, 0, sizeof(*n));
        if (def < 0) def = BLOQUES_DEFS - 1;
        n->id = b->id;
        n->def = def;
        n->padre = b->padre;
        n->orden = b->orden;
        n->abierto = 1;
        n->x = 0;
        n->y = 0;
        for (j = 0; j < 2 && j < b->num_campos; j++)
            copiar_texto(n->campo[j], sizeof(n->campo[j]), b->campos[j].valor);
        if (b->id >= siguiente_id) siguiente_id = b->id + 1;
    }
    for (i = 0; i < num_nodos; i++) {
        for (j = 0; j < p->num_bloques; j++) {
            const Bloque3 *b = &p->bloques[j];
            for (k = 0; k < b->num_ranuras; k++)
                if (b->ranuras[k].bloque == nodos[i].id)
                    copiar_texto(nodos[i].slot, sizeof(nodos[i].slot),
                                 b->ranuras[k].clave);
        }
        /* Un bloque con padre y sin ranura pertenece al cuerpo del padre. */
        if (!nodos[i].slot[0] && nodos[i].padre > 0) {
            int ip = indice_de_id(nodos[i].padre);
            if (ip >= 0 && DEFS[nodos[ip].def].forma == FORMA_C)
                copiar_texto(nodos[i].slot, sizeof(nodos[i].slot), "cuerpo");
        }
    }
    g.bloques_sel = num_nodos > 0 ? nodos[0].id : 0;
    g.bloques_modelo_cargado = 1;
}

/* ------------------------------------------------------------------ */
/* Mutaciones                                                          */
/* ------------------------------------------------------------------ */
void bloques_iniciar(void) {
    inicializado = 1;
    bloques_reiniciar();
    g.bloques_zoom = 100;
    g.bloques_pan_x = 0;
    g.bloques_pan_y = 0;
}

void bloques_reiniciar(void) {
    num_nodos = 0;
    siguiente_id = BLOQUES_ID_INICIAL;
    num_historial = 0;
    num_futuro = 0;
    g.bloques_sel = 0;
    g.bloques_paleta_sel = 0;
    g.bloques_categoria = 0;
    g.bloques_foco = 0;
    g.bloques_hover = -1;
    g.bloques_arrastre = -1;
    g.bloques_desde_paleta = -1;
    g.bloques_modelo_cargado = 0;
}

int bloques_crear(int def, int x, int y) {
    NodoBloque *n;
    int i, id, raices = 0;
    asegurar();
    if (def < 0 || def >= BLOQUES_DEFS || num_nodos >= BLOQUES_MAX) return 0;
    for (i = 0; i < num_nodos; i++)
        if (nodos[i].padre == 0) raices++;
    tomar_nota();
    id = siguiente_id++;
    n = &nodos[num_nodos++];
    memset(n, 0, sizeof(*n));
    n->id = id;
    n->def = def;
    n->padre = 0;
    n->orden = raices;
    n->x = x;
    n->y = y;
    n->abierto = 1;
    for (i = 0; i < 2; i++)
        valor_por_defecto(def, i, n->campo[i], sizeof(n->campo[i]));
    g.bloques_sel = id;
    g.bloques_modelo_cargado = 1;
    bloques_volcar_modelo();
    return id;
}

int bloques_duplicar(int id) {
    int src, dest, nuevo;
    asegurar();
    src = indice_de_id(id);
    if (src < 0 || num_nodos >= BLOQUES_MAX) return 0;
    tomar_nota();
    dest = num_nodos++;
    memcpy(&nodos[dest], &nodos[src], sizeof(nodos[dest]));
    nuevo = siguiente_id++;
    nodos[dest].id = nuevo;
    nodos[dest].orden = nodos[src].orden + 1;
    g.bloques_sel = nuevo;
    bloques_volcar_modelo();
    return nuevo;
}

int bloques_acoplar(int id, int padre, const char *slot) {
    int ni, np;
    asegurar();
    ni = indice_de_id(id);
    if (ni < 0) return 0;
    np = indice_de_id(padre);
    if (padre != 0 && np < 0) return 0;
    if (padre == id || es_antepasado(padre, id, 0)) return 0;
    if (!slot || !slot[0]) slot = "";
    tomar_nota();
    nodos[ni].padre = padre;
    copiar_texto(nodos[ni].slot, sizeof(nodos[ni].slot), slot);
    nodos[ni].orden = 0;
    if (padre != 0) {
        int i;
        for (i = 0; i < num_nodos; i++)
            if (i != ni && nodos[i].padre == padre &&
                strcmp(nodos[i].slot, slot) == 0 && nodos[i].orden >= nodos[ni].orden)
                nodos[ni].orden = nodos[i].orden + 1;
    }
    g.bloques_sel = id;
    bloques_volcar_modelo();
    return 1;
}

int bloques_mover_a(int id, int x, int y) {
    int i;
    asegurar();
    i = indice_de_id(id);
    if (i < 0) return 0;
    if (nodos[i].x == x && nodos[i].y == y) return 1;
    tomar_nota();
    nodos[i].x = x;
    nodos[i].y = y;
    bloques_volcar_modelo();
    return 1;
}

int bloques_borrar(int id) {
    int i, j, destino = 0;
    int borrar[BLOQUES_MAX];
    int nb = 0;
    asegurar();
    if (indice_de_id(id) < 0) return 0;
    borrar[nb++] = id;
    for (i = 0; i < nb && i < BLOQUES_MAX; i++)
        for (j = 0; j < num_nodos; j++)
            if (nodos[j].padre == borrar[i] && nb < BLOQUES_MAX) {
                int k, existe = 0;
                for (k = 0; k < nb; k++)
                    if (borrar[k] == nodos[j].id) existe = 1;
                if (!existe) borrar[nb++] = nodos[j].id;
            }
    tomar_nota();
    for (i = 0; i < num_nodos; i++) {
        int borra = 0;
        for (j = 0; j < nb; j++)
            if (nodos[i].id == borrar[j]) borra = 1;
        if (!borra) nodos[destino++] = nodos[i];
    }
    num_nodos = destino;
    if (g.bloques_sel == id) g.bloques_sel = 0;
    bloques_volcar_modelo();
    return 1;
}

void bloques_seleccionar(int id) {
    asegurar();
    if (id == 0 || indice_de_id(id) >= 0) g.bloques_sel = id;
}

void bloques_deshacer(void) {
    asegurar();
    if (num_historial <= 0) return;
    if (num_futuro < BLOQUES_HISTORIAL_MAX) {
        CopiaBloques *f = &futuro[num_futuro++];
        memcpy(f->nodos, nodos, sizeof(f->nodos));
        f->num = num_nodos;
        f->siguiente = siguiente_id;
        f->sel = g.bloques_sel;
    }
    num_historial--;
    restaurar(&historial[num_historial]);
    bloques_volcar_modelo();
}

void bloques_rehacer(void) {
    asegurar();
    if (num_futuro <= 0) return;
    if (num_historial < BLOQUES_HISTORIAL_MAX) {
        CopiaBloques *h = &historial[num_historial++];
        memcpy(h->nodos, nodos, sizeof(h->nodos));
        h->num = num_nodos;
        h->siguiente = siguiente_id;
        h->sel = g.bloques_sel;
    }
    num_futuro--;
    restaurar(&futuro[num_futuro]);
    bloques_volcar_modelo();
}

/* ------------------------------------------------------------------ */
/* Consultas del catalogo                                              */
/* ------------------------------------------------------------------ */
int bloques_cantidad(void) { asegurar(); return num_nodos; }
int bloques_num_ranuras(void) {
    int i, n = 0;
    asegurar();
    for (i = 0; i < num_nodos; i++) n += nodos[i].slot[0] && strcmp(nodos[i].slot, "cuerpo") != 0;
    return n;
}
int bloques_num_hijos(void) {
    int i, n = 0;
    asegurar();
    for (i = 0; i < num_nodos; i++) n += strcmp(nodos[i].slot, "cuerpo") == 0;
    return n;
}
int bloques_id_seleccionado(void) { asegurar(); return g.bloques_sel; }
const char *bloques_nombre_def(int def) {
    asegurar();
    return (def >= 0 && def < BLOQUES_DEFS) ? DEFS[def].nombre : "";
}
int bloques_def_cantidad(void) { asegurar(); return BLOQUES_DEFS; }
int bloques_def_categoria(int def) {
    asegurar();
    return (def >= 0 && def < BLOQUES_DEFS) ? DEFS[def].categoria : 0;
}
int bloques_def_forma(int def) {
    asegurar();
    return (def >= 0 && def < BLOQUES_DEFS) ? DEFS[def].forma : FORMA_PILA;
}
unsigned int bloques_def_color(int def) {
    asegurar();
    return (def >= 0 && def < BLOQUES_DEFS) ? (unsigned int)DEFS[def].color : 0x003278E0u;
}
const char *bloques_categoria_nombre(int cat) {
    asegurar();
    return (cat >= 0 && cat < BLOQUES_CATEGORIAS) ? CATEGORIAS[cat] : "";
}
const char *bloques_def_slot(int def, int indice) {
    asegurar();
    if (def < 0 || def >= BLOQUES_DEFS || indice < 0 || indice > 1) return "";
    return DEFS[def].slot[indice];
}
const char *bloques_def_campo(int def, int indice) {
    asegurar();
    if (def < 0 || def >= BLOQUES_DEFS || indice < 0 || indice > 1) return "";
    return DEFS[def].campo_nombre[indice];
}

int bloques_nodo_id(int indice) {
    asegurar();
    return (indice >= 0 && indice < num_nodos) ? nodos[indice].id : 0;
}
int bloques_nodo_def(int id) {
    int i = indice_de_id(id);
    asegurar();
    return i >= 0 ? nodos[i].def : -1;
}
int bloques_nodo_padre(int id) {
    int i = indice_de_id(id);
    asegurar();
    return i >= 0 ? nodos[i].padre : 0;
}
const char *bloques_nodo_slot(int id) {
    int i = indice_de_id(id);
    asegurar();
    return i >= 0 ? nodos[i].slot : "";
}
const char *bloques_nodo_campo(int id, int campo) {
    int i = indice_de_id(id);
    asegurar();
    if (i < 0 || campo < 0 || campo > 1) return "";
    return nodos[i].campo[campo];
}
void bloques_nodo_posicion(int id, int *x, int *y) {
    int i = indice_de_id(id);
    asegurar();
    if (x) *x = i >= 0 ? nodos[i].x : 0;
    if (y) *y = i >= 0 ? nodos[i].y : 0;
}
void bloques_campo_anadir(int id, int campo, int ch) {
    int i;
    asegurar();
    i = indice_de_id(id);
    if (i < 0 || campo < 0 || campo > 1) return;
    if (ch < 32 || ch > 126) return;
    if (strlen(nodos[i].campo[campo]) >= BLOQUES_CAMPO_MAX - 1) return;
    nodos[i].campo[campo][strlen(nodos[i].campo[campo])] = (char)ch;
    bloques_volcar_modelo();
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */
static int altura_de(int id, int profundo) {
    int i, idx, def, h;
    if (profundo > 12) return BLOQUES_ALTO;
    idx = indice_de_id(id);
    if (idx < 0) return 0;
    def = nodos[idx].def;
    if (def < 0 || def >= BLOQUES_DEFS) return BLOQUES_ALTO;
    if (DEFS[def].forma == FORMA_REPORTERO ||
        DEFS[def].forma == FORMA_BOOLEANO) return 20;
    if (DEFS[def].forma == FORMA_CAP) return 22;
    h = BLOQUES_ALTO;
    if (DEFS[def].forma == FORMA_C) {
        for (i = 0; i < num_nodos; i++)
            if (nodos[i].padre == id && strcmp(nodos[i].slot, "cuerpo") == 0)
                h += altura_de(nodos[i].id, profundo + 1) + 2;
        h += 10;
        /* Un C vacio necesita un cuerpo visible donde soltar el primer hijo. */
        if (h < BLOQUES_ALTO + 40) h = BLOQUES_ALTO + 40;
    }
    return h;
}

static int anchura_de(int def) {
    if (def >= 0 && def < BLOQUES_DEFS) {
        if (DEFS[def].forma == FORMA_REPORTERO ||
            DEFS[def].forma == FORMA_BOOLEANO) return 96;
        if (DEFS[def].forma == FORMA_CAP) return BLOQUES_ANCHO + 30;
    }
    return BLOQUES_ANCHO;
}

static void ordenar_hijos(int padre, const char *slot, int *ids, int *num, int max) {
    int i, j;
    *num = 0;
    for (i = 0; i < num_nodos && *num < max; i++)
        if (nodos[i].padre == padre && strcmp(nodos[i].slot, slot) == 0)
            ids[(*num)++] = nodos[i].id;
    for (i = 1; i < *num; i++) {
        int actual = ids[i];
        j = i - 1;
        while (j >= 0 && nodos[indice_de_id(ids[j])].orden >
                         nodos[indice_de_id(actual)].orden) {
            ids[j + 1] = ids[j];
            j--;
        }
        ids[j + 1] = actual;
    }
}

static int colocar(int id, int px, int py, BloqueCaja *cajas, int max, int *num) {
    int idx, def, ancho, alto, i, j, n;
    int hijos[BLOQUES_MAX];
    NodoBloque *nodo;
    BloqueCaja *caja;

    idx = indice_de_id(id);
    if (idx < 0 || *num >= max) return 0;
    nodo = &nodos[idx];
    def = nodo->def;
    ancho = anchura_de(def);
    alto = altura_de(id, 0);
    caja = &cajas[(*num)++];
    memset(caja, 0, sizeof(*caja));
    caja->id = id;
    caja->z = zona_hacer(px, py, ancho, alto);

    if (def < 0 || def >= BLOQUES_DEFS) return alto;
    for (i = 0; i < 2 && DEFS[def].slot[i][0]; i++) {
        const char *s = DEFS[def].slot[i];
        Zona z;
        if (strcmp(s, "cuerpo") == 0)
            z = zona_hacer(px + 16, py + BLOQUES_ALTO, ancho - 32, alto - BLOQUES_ALTO - 8);
        else
            z = zona_hacer(px + ancho - 58, py + 6, 52, 14);
        if (caja->num_slots < BLOQUES_RANURAS_MAX) {
            caja->slots[caja->num_slots] = z;
            copiar_texto(caja->nombres[caja->num_slots], BLOQUES_SLOT_MAX, s);
            caja->num_slots++;
        }
    }

    ordenar_hijos(id, "cuerpo", hijos, &n, BLOQUES_MAX);
    {
        int y = py + BLOQUES_ALTO + 2;
        for (i = 0; i < n; i++) {
            int h = colocar(hijos[i], px + 16, y, cajas, max, num);
            y += h + 2;
        }
    }
    for (i = 0; i < 2; i++) {
        const char *s = DEFS[def].slot[i];
        if (!s[0] || strcmp(s, "cuerpo") == 0) continue;
        ordenar_hijos(id, s, hijos, &n, BLOQUES_MAX);
        for (j = 0; j < n; j++) {
            int sx = px + ancho - 58;
            int sy = py + 6;
            colocar(hijos[j], sx, sy, cajas, max, num);
        }
    }
    (void)hijos;
    return alto;
}

int bloques_layout(int x, int y, int ancho, int alto,
                   BloqueCaja *cajas, int max) {
    int ids[BLOQUES_MAX], n = 0, i, num = 0;
    int px, py;
    asegurar();
    if (!cajas || max <= 0) return 0;
    (void)ancho;
    (void)alto;
    ordenar_hijos(0, "", ids, &n, BLOQUES_MAX);
    px = x + 18 + g.bloques_pan_x;
    py = y + 16 + g.bloques_pan_y;
    for (i = 0; i < n; i++) {
        int nx = 0, ny = 0, h;
        bloques_nodo_posicion(ids[i], &nx, &ny);
        if (nx > 0 || ny > 0) {
            h = colocar(ids[i], nx + g.bloques_pan_x,
                        ny + g.bloques_pan_y, cajas, max, &num);
        } else {
            h = colocar(ids[i], px, py, cajas, max, &num);
        }
        py += h + 12;
    }
    return num;
}
