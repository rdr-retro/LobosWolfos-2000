/* entidades.c - entidades y escenas del runtime (fase 26).
 *
 * Ver entidades.h: array fijo, ids monotonos y borrado diferido al final
 * del fotograma. El modulo no sabe nada de LOGIC3 ni de lobo: juego.c le
 * pasa los tipos ya leidos y conecta los callbacks del lenguaje.
 */

#include <string.h>
#include "entidades.h"

static EntidadTipo g_tipos[ENTIDAD_TIPOS_MAX];
static int g_num_tipos;

static Entidad g_entidades[ENTIDADES_MAX];
static int g_siguiente_id = 1;
static int g_nuevas_foto;
static int g_avisos;
static int g_creadas;

/* Copia un nombre con tope y rellena con ceros. */
static void copiar_nombre(char *dst, const char *src, int tam) {
    int i;
    for (i = 0; i < tam - 1 && src && src[i]; i++) dst[i] = src[i];
    for (; i < tam; i++) dst[i] = 0;
}

static int mismo_nombre(const char *a, const char *b) {
    int i;
    for (i = 0; a[i] && a[i] == b[i]; i++) {}
    return a[i] == b[i];
}

static int tipo_valido(int tipo) {
    return tipo >= 0 && tipo < g_num_tipos;
}

void entidades_reiniciar(void) {
    memset(g_tipos, 0, sizeof(g_tipos));
    memset(g_entidades, 0, sizeof(g_entidades));
    g_num_tipos = 0;
    g_siguiente_id = 1;
    g_nuevas_foto = 0;
    g_avisos = 0;
    g_creadas = 0;
}

void entidades_vaciar(void) {
    int i;
    for (i = 0; i < ENTIDADES_MAX; i++) {
        g_entidades[i].usada = 0;
        g_entidades[i].borrar = 0;
        g_entidades[i].nueva = 0;
    }
    g_nuevas_foto = 0;
    g_creadas = 0;
}

int entidades_tipo_agregar(int id_objeto, const char *nombre, int escena,
                           int x, int y, int w, int h, int visible, int capa) {
    int i;
    EntidadTipo *t;

    if (!nombre) nombre = "";
    for (i = 0; i < g_num_tipos; i++)
        if (g_tipos[i].id == id_objeto) return i;
    if (g_num_tipos >= ENTIDAD_TIPOS_MAX) {
        g_avisos++;
        return -1;
    }
    t = &g_tipos[g_num_tipos];
    memset(t, 0, sizeof(*t));
    t->id = id_objeto;
    copiar_nombre(t->nombre, nombre, ENTIDAD_NOMBRE_MAX);
    t->escena = escena;
    t->x = x;
    t->y = y;
    t->w = w > 0 ? w : 16;
    t->h = h > 0 ? h : 16;
    t->visible = visible ? 1 : 0;
    t->capa = capa;
    t->num_vars = 0;
    return g_num_tipos++;
}

int entidades_tipo_var(int tipo, int slot, const char *nombre, int valor) {
    EntidadTipo *t;
    if (!tipo_valido(tipo) || slot < 0 || slot >= ENTIDAD_VARS_MAX) return 0;
    t = &g_tipos[tipo];
    t->var_valor[slot] = valor;
    copiar_nombre(t->var_nombre[slot], nombre ? nombre : "", ENTIDAD_NOMBRE_MAX);
    if (slot + 1 > t->num_vars) t->num_vars = slot + 1;
    return 1;
}

int entidades_tipo_por_objeto(int id_objeto) {
    int i;
    for (i = 0; i < g_num_tipos; i++)
        if (g_tipos[i].id == id_objeto) return i;
    return -1;
}

int entidades_tipo_por_tag(const char *nombre) {
    int i;
    if (!nombre) return -1;
    for (i = 0; i < g_num_tipos; i++)
        if (mismo_nombre(g_tipos[i].nombre, nombre)) return i;
    return -1;
}

int entidades_slot_var(int tipo, const char *nombre) {
    int i;
    if (!tipo_valido(tipo) || !nombre) return -1;
    for (i = 0; i < g_tipos[tipo].num_vars; i++)
        if (g_tipos[tipo].var_nombre[i][0] &&
            mismo_nombre(g_tipos[tipo].var_nombre[i], nombre))
            return i;
    return -1;
}

int entidades_tipo_vars(int tipo) {
    if (!tipo_valido(tipo)) return 0;
    return g_tipos[tipo].num_vars;
}

const char *entidades_tipo_var_nombre(int tipo, int slot) {
    if (!tipo_valido(tipo) || slot < 0 || slot >= ENTIDAD_VARS_MAX)
        return "";
    return g_tipos[tipo].var_nombre[slot];
}

int entidades_tipo_objeto(int tipo) {
    if (!tipo_valido(tipo)) return -1;
    return g_tipos[tipo].id;
}

const char *entidades_tipo_nombre(int tipo) {
    if (!tipo_valido(tipo)) return "";
    return g_tipos[tipo].nombre;
}

int entidades_de_objeto(int id_objeto, int *salida, int maximo) {
    int tipo = entidades_tipo_por_objeto(id_objeto);
    int i, n = 0;
    if (tipo < 0 || !salida) return 0;
    for (i = 0; i < ENTIDADES_MAX && n < maximo; i++)
        if (g_entidades[i].usada && g_entidades[i].tipo == tipo)
            salida[n++] = g_entidades[i].id;
    return n;
}

/* Primer hueco libre del array; -1 si esta lleno. */
static int hueco_libre(void) {
    int i;
    for (i = 0; i < ENTIDADES_MAX; i++)
        if (!g_entidades[i].usada) return i;
    return -1;
}

int entidad_crear(int tipo, int x, int y) {
    EntidadTipo *t;
    Entidad *e;
    int i, slot;

    if (!tipo_valido(tipo)) return -1;
    if (g_nuevas_foto >= ENTIDADES_NUEVAS_FOTO) {
        /* El limite por fotograma es el primero que salta al crear en
         * bucle; se avisa una vez por intento rechazado. */
        g_avisos++;
        return -1;
    }
    slot = hueco_libre();
    if (slot < 0) {
        g_avisos++;
        return -1;
    }
    t = &g_tipos[tipo];
    e = &g_entidades[slot];
    memset(e, 0, sizeof(*e));
    e->usada = 1;
    e->id = g_siguiente_id++;
    e->tipo = tipo;
    e->escena = t->escena;
    e->x = x;
    e->y = y;
    e->w = t->w;
    e->h = t->h;
    e->visible = t->visible;
    e->capa = t->capa;
    e->traje = 0;
    e->vida = 1;
    e->es_clon = 0;
    e->nueva = 1;
    for (i = 0; i < ENTIDAD_VARS_MAX; i++) e->vars[i] = t->var_valor[i];
    g_nuevas_foto++;
    g_creadas++;
    return e->id;
}

int entidades_clonar(int id) {
    Entidad *origen = entidad_buscar(id);
    Entidad *copia;
    int nuevo, i;

    if (!origen) return -1;
    nuevo = entidad_crear(origen->tipo, origen->x, origen->y);
    if (nuevo < 0) return -1;
    copia = entidad_buscar(nuevo);
    if (!copia) return -1;
    for (i = 0; i < ENTIDAD_VARS_MAX; i++) copia->vars[i] = origen->vars[i];
    copia->escena = origen->escena;
    copia->w = origen->w;
    copia->h = origen->h;
    copia->visible = origen->visible;
    copia->capa = origen->capa;
    copia->traje = origen->traje;
    copia->vida = origen->vida;
    copia->es_clon = 1;
    return nuevo;
}

int entidad_borrar(int id) {
    Entidad *e = entidad_buscar(id);
    if (!e) return -1;
    e->borrar = 1;
    return 1;
}

void entidades_fin_fotograma(void) {
    int i;
    for (i = 0; i < ENTIDADES_MAX; i++) {
        if (g_entidades[i].usada && g_entidades[i].borrar)
            g_entidades[i].usada = 0;
        g_entidades[i].borrar = 0;
        g_entidades[i].nueva = 0;
    }
    g_nuevas_foto = 0;
}

Entidad *entidad_buscar(int id) {
    int i;
    if (id <= 0) return NULL;
    for (i = 0; i < ENTIDADES_MAX; i++)
        if (g_entidades[i].usada && g_entidades[i].id == id)
            return &g_entidades[i];
    return NULL;
}

int entidad_por_tag(const char *tag) {
    int i;
    int tipo;
    if (!tag) return -1;
    tipo = entidades_tipo_por_tag(tag);
    if (tipo < 0) return -1;
    for (i = 0; i < ENTIDADES_MAX; i++)
        if (g_entidades[i].usada && g_entidades[i].tipo == tipo)
            return g_entidades[i].id;
    return -1;
}

int entidad_mover(int id, int x, int y) {
    Entidad *e = entidad_buscar(id);
    if (!e) return -1;
    e->x = x;
    e->y = y;
    return 1;
}

int entidad_cambiar_escena(int id, int escena) {
    Entidad *e = entidad_buscar(id);
    if (!e) return -1;
    e->escena = escena;
    return 1;
}

Entidad *entidad_indice(int i) {
    if (i < 0 || i >= ENTIDADES_MAX) return NULL;
    return &g_entidades[i];
}

int entidades_max(void) { return ENTIDADES_MAX; }

int entidades_orden(int *salida, int maximo) {
    int n = 0, i, j;
    if (!salida) return 0;
    for (i = 0; i < ENTIDADES_MAX && n < maximo; i++)
        if (g_entidades[i].usada) salida[n++] = i;
    /* Insercion por capa y luego id: el mismo orden que el planificador
     * del plan maestro, para que el dibujo y la traza sean estables. */
    for (i = 1; i < n; i++) {
        int tmp = salida[i];
        const Entidad *e = &g_entidades[tmp];
        j = i - 1;
        while (j >= 0) {
            const Entidad *a = &g_entidades[salida[j]];
            int antes = a->capa < e->capa ||
                        (a->capa == e->capa && a->id < e->id);
            if (antes) break;
            salida[j + 1] = salida[j];
            j--;
        }
        salida[j + 1] = tmp;
    }
    return n;
}

int entidades_leer_campo(int id, int campo) {
    Entidad *e = entidad_buscar(id);
    if (!e) return 0;
    switch (campo) {
    case ENTIDAD_CAMPO_ID: return e->id;
    case ENTIDAD_CAMPO_X: return e->x;
    case ENTIDAD_CAMPO_Y: return e->y;
    case ENTIDAD_CAMPO_VIDA: return e->vida;
    case ENTIDAD_CAMPO_VISIBLE: return e->visible;
    case ENTIDAD_CAMPO_TRAJE: return e->traje;
    case ENTIDAD_CAMPO_ESCENA: return e->escena;
    case ENTIDAD_CAMPO_W: return e->w;
    case ENTIDAD_CAMPO_H: return e->h;
    case ENTIDAD_CAMPO_CAPA: return e->capa;
    case ENTIDAD_CAMPO_TIPO: return e->tipo;
    default: return 0;
    }
}

int entidades_escribir_campo(int id, int campo, int valor) {
    Entidad *e = entidad_buscar(id);
    if (!e) return 0;
    switch (campo) {
    case ENTIDAD_CAMPO_X: e->x = valor; return 1;
    case ENTIDAD_CAMPO_Y: e->y = valor; return 1;
    case ENTIDAD_CAMPO_VIDA: e->vida = valor; return 1;
    case ENTIDAD_CAMPO_VISIBLE: e->visible = valor ? 1 : 0; return 1;
    case ENTIDAD_CAMPO_TRAJE: e->traje = valor < 0 ? 0 : valor; return 1;
    case ENTIDAD_CAMPO_ESCENA: e->escena = valor; return 1;
    case ENTIDAD_CAMPO_W: e->w = valor > 0 ? valor : 1; return 1;
    case ENTIDAD_CAMPO_H: e->h = valor > 0 ? valor : 1; return 1;
    case ENTIDAD_CAMPO_CAPA: e->capa = valor; return 1;
    default: return 0;
    }
}

int entidades_leer_var(int id, int slot) {
    Entidad *e = entidad_buscar(id);
    if (!e || slot < 0 || slot >= ENTIDAD_VARS_MAX) return 0;
    return e->vars[slot];
}

int entidades_escribir_var(int id, int slot, int valor) {
    Entidad *e = entidad_buscar(id);
    if (!e || slot < 0 || slot >= ENTIDAD_VARS_MAX) return 0;
    e->vars[slot] = valor;
    return 1;
}

int entidades_num(void) {
    int i, n = 0;
    for (i = 0; i < ENTIDADES_MAX; i++)
        if (g_entidades[i].usada) n++;
    return n;
}

int entidades_avisos(void) { return g_avisos; }

int entidades_siguiente_id(void) { return g_siguiente_id; }

int entidades_creadas(void) { return g_creadas; }

void entidades_aviso(void) { g_avisos++; }

/* Caja util de una entidad, con tamano minimo 1 como en el render. */
static int caja_de_entidad(const Entidad *e, int *x, int *y, int *w, int *h) {
    if (!e || !e->usada) return 0;
    *x = e->x;
    *y = e->y;
    *w = e->w > 0 ? e->w : 1;
    *h = e->h > 0 ? e->h : 1;
    return 1;
}

int entidades_tocan(int a, int b) {
    int ax, ay, aw, ah, bx, by, bw, bh;
    if (!caja_de_entidad(entidad_buscar(a), &ax, &ay, &aw, &ah)) return 0;
    if (!caja_de_entidad(entidad_buscar(b), &bx, &by, &bw, &bh)) return 0;
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

int entidades_toca_tag(int id, const char *tag) {
    int tipo, i;
    if (!tag || !tag[0]) return 0;
    tipo = entidades_tipo_por_tag(tag);
    if (tipo < 0) return 0;
    for (i = 0; i < ENTIDADES_MAX; i++) {
        Entidad *e = &g_entidades[i];
        if (!e->usada || e->id == id || e->tipo != tipo) continue;
        if (entidades_tocan(id, e->id)) return 1;
    }
    return 0;
}

int entidades_distancia(int a, int b) {
    Entidad *ea = entidad_buscar(a);
    Entidad *eb = entidad_buscar(b);
    int dx, dy;
    if (!ea || !eb) return -1;
    dx = ea->x - eb->x;
    dy = ea->y - eb->y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx + dy; /* distancia Manhattan, sin coma flotante */
}

int entidades_caja(int id, int indice) {
    int x, y, w, h;
    if (!caja_de_entidad(entidad_buscar(id), &x, &y, &w, &h)) return 0;
    if (indice == 0) return x;
    if (indice == 1) return y;
    if (indice == 2) return w;
    if (indice == 3) return h;
    return 0;
}
