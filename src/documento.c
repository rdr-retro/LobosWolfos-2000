/* ==========================================================================
 * Documento: capas, composicion, deshacer/rehacer y seleccion.
 * ========================================================================== */
#include "app.h"

/* ------------------------------------------------------------------ */
/* Capas                                                               */
/* ------------------------------------------------------------------ */
static u32 *reservar_pixeles(int ancho, int alto) {
    size_t n;
    if (ancho < 1 || alto < 1) return NULL;
    n = (size_t)ancho * (size_t)alto;
    return (u32 *)calloc(n, sizeof(u32));
}

static void capa_iniciar(Capa *c, int ancho, int alto, u32 relleno, const char *nombre) {
    c->pixeles = reservar_pixeles(ancho, alto);
    c->visible = 1;
    c->alfa = (relleno == 0);
    c->opacidad = 255;
    recortar_texto(c->nombre, nombre ? nombre : "Capa", sizeof(c->nombre));
    if (c->pixeles && relleno) {
        size_t n = (size_t)ancho * alto;
        size_t i;
        for (i = 0; i < n; i++) c->pixeles[i] = relleno;
    }
}

Documento doc_nuevo(int ancho, int alto, u32 fondo) {
    Documento d;
    memset(&d, 0, sizeof(d));
    d.ancho = limitar(ancho, MIN_LADO, LIENZO_MAX);
    d.alto = limitar(alto, MIN_LADO, LIENZO_MAX);
    d.fondo = fondo;
    d.num_capas = 1;
    d.activa = 0;
    capa_iniciar(&d.capas[0], d.ancho, d.alto, 0xFF000000u | (fondo & 0x00FFFFFFu), "Fondo");
    d.compuesto = reservar_pixeles(d.ancho, d.alto);
    d.compuesto_sucio = 1;
    return d;
}

void doc_liberar(Documento *d) {
    int i;
    if (!d) return;
    for (i = 0; i < d->num_capas; i++) {
        if (d->capas[i].pixeles) free(d->capas[i].pixeles);
        d->capas[i].pixeles = NULL;
    }
    if (d->compuesto) free(d->compuesto);
    d->compuesto = NULL;
    d->num_capas = 0;
}

/* Copia profunda de un documento en otro. El destino se libera antes; se usa
 * para guardar tiles inactivos y para las instantaneas del modo tiles. */
void doc_copiar_en(Documento *dst, const Documento *src) {
    int i;
    size_t n;
    if (!dst || !src || dst == src) return;
    doc_liberar(dst);
    memset(dst, 0, sizeof(*dst));
    dst->ancho = src->ancho;
    dst->alto = src->alto;
    dst->fondo = src->fondo;
    dst->num_capas = src->num_capas;
    dst->activa = src->activa;
    dst->modificado = src->modificado;
    n = (size_t)src->ancho * src->alto;
    for (i = 0; i < src->num_capas && i < MAX_CAPAS; i++) {
        dst->capas[i] = src->capas[i];
        dst->capas[i].pixeles = reservar_pixeles(src->ancho, src->alto);
        if (dst->capas[i].pixeles && src->capas[i].pixeles)
            copiar_pixeles(dst->capas[i].pixeles, src->capas[i].pixeles, (int)n);
    }
    dst->compuesto = reservar_pixeles(src->ancho, src->alto);
    if (dst->compuesto && src->compuesto)
        copiar_pixeles(dst->compuesto, src->compuesto, (int)n);
    dst->compuesto_sucio = src->compuesto_sucio;
}

/* Intercambia dos documentos sin copiar sus pixeles. */
void doc_intercambiar(Documento *a, Documento *b) {
    Documento t;
    if (!a || !b || a == b) return;
    t = *a;
    *a = *b;
    *b = t;
}

static int insertar_capa(Documento *d, int indice, const char *nombre, u32 relleno) {
    int i;
    if (!d || d->num_capas >= MAX_CAPAS) return 0;
    if (indice < 0) indice = 0;
    if (indice > d->num_capas) indice = d->num_capas;
    for (i = d->num_capas; i > indice; i--) d->capas[i] = d->capas[i - 1];
    capa_iniciar(&d->capas[indice], d->ancho, d->alto, relleno, nombre);
    if (!d->capas[indice].pixeles) {
        for (i = indice; i < d->num_capas - 1; i++) d->capas[i] = d->capas[i + 1];
        return 0;
    }
    d->num_capas++;
    d->activa = indice;
    d->compuesto_sucio = 1;
    d->modificado = 1;
    return 1;
}

int doc_agregar_capa(Documento *d, const char *nombre) {
    char texto[64];
    if (!nombre) {
        formatear(texto, sizeof(texto), "Capa %d", d->num_capas + 1);
        nombre = texto;
    }
    return insertar_capa(d, d->activa + 1, nombre, 0);
}

int doc_duplicar_capa(Documento *d) {
    int i;
    Capa copia;
    if (!d || d->num_capas >= MAX_CAPAS) return 0;
    i = d->activa;
    copia = d->capas[i];
    copia.pixeles = reservar_pixeles(d->ancho, d->alto);
    if (!copia.pixeles) return 0;
    copiar_pixeles(copia.pixeles, d->capas[i].pixeles, d->ancho * d->alto);
    recortar_texto(copia.nombre, "Capa copia", sizeof(copia.nombre));
    {
        int j;
        for (j = d->num_capas; j > i + 1; j--) d->capas[j] = d->capas[j - 1];
        d->capas[i + 1] = copia;
    }
    d->num_capas++;
    d->activa = i + 1;
    d->compuesto_sucio = 1;
    d->modificado = 1;
    return 1;
}

int doc_eliminar_capa(Documento *d, int indice) {
    int i;
    if (!d || d->num_capas <= 1) return 0;
    if (indice < 0 || indice >= d->num_capas) return 0;
    free(d->capas[indice].pixeles);
    for (i = indice; i < d->num_capas - 1; i++) d->capas[i] = d->capas[i + 1];
    d->num_capas--;
    if (d->activa >= d->num_capas) d->activa = d->num_capas - 1;
    if (d->activa > indice) d->activa--;
    d->compuesto_sucio = 1;
    d->modificado = 1;
    return 1;
}

int doc_capa_tiene_alfa(Documento *d, int indice) {
    if (!d || indice < 0 || indice >= d->num_capas) return 0;
    return d->capas[indice].alfa;
}

/* Activa o quita el canal alfa de una capa. Al quitarlo, los pixeles
 * transparentes pasan a blanco para que la capa quede opaca, como en
 * `aplanar`. */
int doc_poner_alfa(Documento *d, int indice, int alfa) {
    Capa *c;
    size_t n, i;
    if (!d || indice < 0 || indice >= d->num_capas) return 0;
    c = &d->capas[indice];
    if (c->alfa == (alfa ? 1 : 0)) return 0;
    c->alfa = alfa ? 1 : 0;
    if (!c->alfa && c->pixeles) {
        n = (size_t)d->ancho * d->alto;
        for (i = 0; i < n; i++) {
            if (c->pixeles[i] == 0x00000000u) c->pixeles[i] = 0xFFFFFFFFu;
        }
    }
    d->compuesto_sucio = 1;
    d->modificado = 1;
    return 1;
}

int doc_mover_capa(Documento *d, int indice, int delta) {
    int destino;
    Capa t;
    if (!d) return 0;
    destino = indice + delta;
    if (indice < 0 || indice >= d->num_capas) return 0;
    if (destino < 0 || destino >= d->num_capas) return 0;
    t = d->capas[indice];
    d->capas[indice] = d->capas[destino];
    d->capas[destino] = t;
    d->activa = destino;
    d->compuesto_sucio = 1;
    d->modificado = 1;
    return 1;
}

int doc_fusionar_abajo(Documento *d) {
    int i;
    Capa *abajo, *arriba;
    size_t n, k;
    if (!d || d->activa <= 0) return 0;
    i = d->activa;
    abajo = &d->capas[i - 1];
    arriba = &d->capas[i];
    n = (size_t)d->ancho * d->alto;
    for (k = 0; k < n; k++) {
        u32 src = arriba->pixeles[k];
        int a = (int)((src >> 24) & 0xFF) * (arriba->visible ? arriba->opacidad : 0) / 255;
        if (a > 0) abajo->pixeles[k] = color_mezclar(abajo->pixeles[k], src, a);
    }
    free(arriba->pixeles);
    for (k = (size_t)i; k + 1 < (size_t)d->num_capas; k++) d->capas[k] = d->capas[k + 1];
    d->num_capas--;
    d->activa = i - 1;
    d->compuesto_sucio = 1;
    d->modificado = 1;
    return 1;
}

void doc_redimensionar(Documento *d, int ancho, int alto) {
    int i, x, y;
    ancho = limitar(ancho, MIN_LADO, LIENZO_MAX);
    alto = limitar(alto, MIN_LADO, LIENZO_MAX);
    if (!d || (ancho == d->ancho && alto == d->alto)) return;
    for (i = 0; i < d->num_capas; i++) {
        u32 *nuevo = reservar_pixeles(ancho, alto);
        u32 relleno;
        if (!nuevo) continue;
        /* Igual que `Dibujo::ajustar`: la capa base conserva el relleno del
         * documento; las superiores crecen transparentes si tienen alfa y en
         * blanco opaco si no. */
        if (i == 0)
            relleno = 0xFF000000u | (d->fondo & 0x00FFFFFFu);
        else if (d->capas[i].alfa)
            relleno = 0u;
        else
            relleno = 0xFFFFFFFFu;
        {
            size_t n = (size_t)ancho * alto, k;
            for (k = 0; k < n; k++) nuevo[k] = relleno;
        }
        for (y = 0; y < alto && y < d->alto; y++) {
            for (x = 0; x < ancho && x < d->ancho; x++) {
                nuevo[y * ancho + x] = d->capas[i].pixeles[y * d->ancho + x];
            }
        }
        free(d->capas[i].pixeles);
        d->capas[i].pixeles = nuevo;
    }
    d->ancho = ancho;
    d->alto = alto;
    if (d->compuesto) free(d->compuesto);
    d->compuesto = reservar_pixeles(ancho, alto);
    d->compuesto_sucio = 1;
    d->modificado = 1;
}

static __inline int alpha_de(u32 c) { return (int)((c >> 24) & 0xFF); }

void doc_componer_region(Documento *d, int x0, int y0, int x1, int y1) {
    int x, y, i;
    u32 fondo;
    if (!d || !d->compuesto) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > d->ancho) x1 = d->ancho;
    if (y1 > d->alto) y1 = d->alto;
    if (x0 >= x1 || y0 >= y1) return;
    fondo = 0xFF000000u | (d->fondo & 0x00FFFFFFu);
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            size_t p = (size_t)y * d->ancho + x;
            u32 color = fondo;
            for (i = 0; i < d->num_capas; i++) {
                Capa *c = &d->capas[i];
                int a;
                if (!c->visible || c->opacidad <= 0) continue;
                a = alpha_de(c->pixeles[p]) * c->opacidad / 255;
                if (a > 0) color = color_mezclar(color, c->pixeles[p], a);
            }
            d->compuesto[p] = color;
        }
    }
}

void doc_componer_todo(Documento *d) {
    if (!d) return;
    if (!d->compuesto) {
        d->compuesto = reservar_pixeles(d->ancho, d->alto);
        if (!d->compuesto) return;
    }
    doc_componer_region(d, 0, 0, d->ancho, d->alto);
    d->compuesto_sucio = 0;
}

int doc_asegurar_compuesto(Documento *d) {
    if (!d || !d->compuesto) return 0;
    return 1;
}

u32 doc_color_visible(Documento *d, int x, int y) {
    if (!d || x < 0 || y < 0 || x >= d->ancho || y >= d->alto) return 0xFF000000u;
    if (!d->compuesto) doc_componer_todo(d);
    return d->compuesto[(size_t)y * d->ancho + x];
}

void doc_borrar_capa(Documento *d) {
    size_t n, i;
    if (!d || d->activa < 0 || d->activa >= d->num_capas) return;
    n = (size_t)d->ancho * d->alto;
    for (i = 0; i < n; i++) {
        d->capas[d->activa].pixeles[i] = (d->activa == 0)
            ? (0xFF000000u | (d->fondo & 0x00FFFFFFu))
            : 0u;
    }
    d->compuesto_sucio = 1;
    d->modificado = 1;
}

/* ------------------------------------------------------------------ */
/* Deshacer / rehacer                                                  */
/* ------------------------------------------------------------------ */
static size_t foto_bytes(const Foto *f) {
    size_t total = 0;
    int i;
    for (i = 0; i < f->num_capas; i++) total += (size_t)f->ancho * f->alto * sizeof(u32);
    return total;
}

static size_t instantanea_bytes(const Instantanea *e) {
    size_t total = sizeof(Instantanea) + foto_bytes(&e->dibujo);
    int i;
    for (i = 0; i < e->num_tiles; i++) total += foto_bytes(&e->tiles[i].foto);
    return total;
}

static void foto_liberar(Foto *f) {
    int i;
    if (!f) return;
    for (i = 0; i < f->num_capas; i++) {
        if (f->capas[i].pixeles) free(f->capas[i].pixeles);
        f->capas[i].pixeles = NULL;
    }
    f->num_capas = 0;
}

static int foto_desde(Foto *f, const Documento *d) {
    int i;
    size_t n;
    memset(f, 0, sizeof(*f));
    f->ancho = d->ancho;
    f->alto = d->alto;
    f->num_capas = d->num_capas;
    f->activa = d->activa;
    n = (size_t)d->ancho * d->alto;
    for (i = 0; i < d->num_capas; i++) {
        f->capas[i] = d->capas[i];
        f->capas[i].pixeles = reservar_pixeles(d->ancho, d->alto);
        if (!f->capas[i].pixeles) {
            foto_liberar(f);
            return 0;
        }
        copiar_pixeles(f->capas[i].pixeles, d->capas[i].pixeles, (int)n);
    }
    return 1;
}

static void foto_a_documento(const Foto *f, Documento *d) {
    int i;
    size_t n;
    doc_liberar(d);
    memset(d, 0, sizeof(*d));
    d->ancho = f->ancho;
    d->alto = f->alto;
    d->fondo = LIENZO;
    d->num_capas = f->num_capas;
    d->activa = f->activa;
    n = (size_t)d->ancho * d->alto;
    for (i = 0; i < f->num_capas; i++) {
        d->capas[i] = f->capas[i];
        d->capas[i].pixeles = reservar_pixeles(d->ancho, d->alto);
        if (d->capas[i].pixeles) copiar_pixeles(d->capas[i].pixeles, f->capas[i].pixeles, (int)n);
    }
    d->compuesto = reservar_pixeles(d->ancho, d->alto);
    d->compuesto_sucio = 1;
    d->modificado = 1;
    doc_componer_todo(d);
}

static void instantanea_liberar(Instantanea *e) {
    int i;
    if (!e) return;
    foto_liberar(&e->dibujo);
    for (i = 0; i < e->num_tiles; i++) foto_liberar(&e->tiles[i].foto);
    e->num_tiles = 0;
}

/* Copia el estado completo: documento, tiles, celda activa, modo y zoom. */
static int instantanea_desde(Instantanea *e, const Documento *d) {
    int i;
    memset(e, 0, sizeof(*e));
    if (!foto_desde(&e->dibujo, d)) return 0;
    e->num_tiles = 0;
    for (i = 0; i < g.num_tiles_modo; i++) {
        TileDoc *t = &g.tiles_modo[i];
        FotoTile *ft;
        if (!t->ocupado) continue;
        ft = &e->tiles[e->num_tiles];
        ft->cx = t->cx;
        ft->cy = t->cy;
        if (!foto_desde(&ft->foto, &t->doc)) {
            instantanea_liberar(e);
            return 0;
        }
        e->num_tiles++;
    }
    e->tile_cx = g.tile_cx;
    e->tile_cy = g.tile_cy;
    e->modo_tiles = g.modo_tiles;
    e->zoom_idx = g.zoom_idx;
    return 1;
}

/* Restaura la instantanea completa sobre el estado global. */
static void instantanea_a_estado(const Instantanea *e, Documento *d) {
    int i;
    foto_a_documento(&e->dibujo, d);
    tiles_modo_limpiar();
    for (i = 0; i < e->num_tiles; i++) {
        TileDoc *t = &g.tiles_modo[i];
        memset(t, 0, sizeof(*t));
        t->ocupado = 1;
        t->cx = e->tiles[i].cx;
        t->cy = e->tiles[i].cy;
        foto_a_documento(&e->tiles[i].foto, &t->doc);
    }
    g.num_tiles_modo = e->num_tiles;
    g.tile_cx = e->tile_cx;
    g.tile_cy = e->tile_cy;
    g.modo_tiles = e->modo_tiles;
    g.zoom_idx = e->zoom_idx;
    g.tiles_layout_valido = 0;
    if (g.modo_tiles) {
        if (!g.tiles_visible) g.tiles_visible = 1;
        tiles_modo_layout();
    }
    lienzo_actualizar();
    app_invalidar();
}

void pila_iniciar(PilaUndo *p, int capacidad) {
    memset(p, 0, sizeof(*p));
    p->capacidad = capacidad > 0 ? capacidad : 1;
    p->estados = (Instantanea *)calloc((size_t)p->capacidad, sizeof(Instantanea));
}

void pila_limpiar(PilaUndo *p) {
    int i;
    if (!p) return;
    if (p->estados) {
        for (i = 0; i < p->num; i++) instantanea_liberar(&p->estados[i]);
    }
    p->num = 0;
    p->bytes = 0;
}

void pila_liberar(PilaUndo *p) {
    if (!p) return;
    pila_limpiar(p);
    if (p->estados) free(p->estados);
    p->estados = NULL;
    p->capacidad = 0;
}

static void pila_quitar_primera(PilaUndo *p) {
    int i;
    if (p->num <= 0) return;
    p->bytes -= instantanea_bytes(&p->estados[0]);
    instantanea_liberar(&p->estados[0]);
    for (i = 1; i < p->num; i++) p->estados[i - 1] = p->estados[i];
    p->num--;
}

void pila_guardar(PilaUndo *p, const Documento *d) {
    Instantanea *nueva;
    size_t necesario;
    if (!p || !p->estados || !d) return;
    nueva = (Instantanea *)malloc(sizeof(Instantanea));
    if (!nueva) return;
    if (!instantanea_desde(nueva, d)) {
        free(nueva);
        return;
    }
    necesario = instantanea_bytes(nueva);
    while (p->num > 0 &&
           (p->num >= p->capacidad ||
            p->bytes + necesario > 192u * 1024u * 1024u)) {
        pila_quitar_primera(p);
    }
    if (p->num >= p->capacidad) {
        instantanea_liberar(nueva);
        free(nueva);
        return;
    }
    p->estados[p->num++] = *nueva;
    p->bytes += necesario;
    free(nueva);
}

int pila_deshacer(PilaUndo *p, PilaUndo *rehacer, Documento *d) {
    Instantanea *copia;
    if (!p || !rehacer || !d || p->num <= 0) return 0;
    copia = (Instantanea *)malloc(sizeof(Instantanea));
    if (!copia) return 0;
    if (!instantanea_desde(copia, d)) {
        free(copia);
        return 0;
    }
    p->num--;
    p->bytes -= instantanea_bytes(&p->estados[p->num]);
    if (rehacer->num < rehacer->capacidad) {
        rehacer->estados[rehacer->num++] = *copia;
        rehacer->bytes += instantanea_bytes(copia);
    } else {
        instantanea_liberar(copia);
    }
    free(copia);
    instantanea_a_estado(&p->estados[p->num], d);
    instantanea_liberar(&p->estados[p->num]);
    return 1;
}

int pila_rehacer(PilaUndo *p, PilaUndo *deshacer, Documento *d) {
    Instantanea *copia;
    if (!p || !deshacer || !d || p->num <= 0) return 0;
    copia = (Instantanea *)malloc(sizeof(Instantanea));
    if (!copia) return 0;
    if (!instantanea_desde(copia, d)) {
        free(copia);
        return 0;
    }
    p->num--;
    p->bytes -= instantanea_bytes(&p->estados[p->num]);
    if (deshacer->num < deshacer->capacidad) {
        deshacer->estados[deshacer->num++] = *copia;
        deshacer->bytes += instantanea_bytes(copia);
    } else {
        instantanea_liberar(copia);
    }
    free(copia);
    instantanea_a_estado(&p->estados[p->num], d);
    instantanea_liberar(&p->estados[p->num]);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Seleccion                                                           */
/* ------------------------------------------------------------------ */
static int sel_ancho(Seleccion *s) { return s->x1 - s->x0; }
static int sel_alto(Seleccion *s) { return s->y1 - s->y0; }

void seleccion_liberar(Seleccion *s) {
    if (!s) return;
    if (s->mascara) free(s->mascara);
    if (s->flotante) free(s->flotante);
    memset(s, 0, sizeof(*s));
    s->redimensionando = -1;
}

void seleccion_limpiar(Seleccion *s) {
    seleccion_liberar(s);
}

void seleccion_todo(Seleccion *s, int ancho, int alto) {
    int i, n;
    seleccion_limpiar(s);
    s->activa = 1;
    s->tipo = 0;
    s->x0 = 0;
    s->y0 = 0;
    s->x1 = ancho;
    s->y1 = alto;
    n = ancho * alto;
    s->mascara = (u8 *)malloc((size_t)n);
    if (s->mascara) {
        for (i = 0; i < n; i++) s->mascara[i] = 1;
    }
}

void seleccion_invertir(Seleccion *s, int ancho, int alto) {
    int x, y;
    if (!s->activa || !s->mascara) {
        seleccion_todo(s, ancho, alto);
        return;
    }
    for (y = 0; y < sel_alto(s); y++) {
        for (x = 0; x < sel_ancho(s); x++) {
            s->mascara[y * sel_ancho(s) + x] = !s->mascara[y * sel_ancho(s) + x];
        }
    }
    /* Recalcular la caja a partir de la mascara. */
    {
        int minx = ancho, miny = alto, maxx = -1, maxy = -1;
        for (y = 0; y < sel_alto(s) + 0; y++) {
            for (x = 0; x < sel_ancho(s); x++) {
                if (s->mascara[y * sel_ancho(s) + x]) {
                    int px = s->x0 + x, py = s->y0 + y;
                    if (px < minx) minx = px;
                    if (py < miny) miny = py;
                    if (px > maxx) maxx = px;
                    if (py > maxy) maxy = py;
                }
            }
        }
        if (maxx < minx || maxy < miny) {
            seleccion_limpiar(s);
            return;
        }
        /* Trasladar la mascara a la nueva caja. */
        {
            int nw = maxx - minx + 1, nh = maxy - miny + 1;
            u8 *nueva = (u8 *)calloc((size_t)nw * nh, 1);
            if (nueva) {
                int ox = s->x0, oy = s->y0;
                for (y = 0; y < nh; y++) {
                    for (x = 0; x < nw; x++) {
                        int sx = minx + x - ox, sy = miny + y - oy;
                        if (sx >= 0 && sy >= 0 && sx < sel_ancho(s) && sy < sel_alto(s)) {
                            nueva[y * nw + x] = s->mascara[sy * sel_ancho(s) + sx];
                        }
                    }
                }
                free(s->mascara);
                s->mascara = nueva;
                s->x0 = minx; s->y0 = miny; s->x1 = maxx + 1; s->y1 = maxy + 1;
            }
        }
    }
}

int seleccion_contiene(Seleccion *s, int x, int y) {
    int lx, ly;
    if (!s->activa) return 1;
    lx = x - s->x0;
    ly = y - s->y0;
    if (lx < 0 || ly < 0 || lx >= sel_ancho(s) || ly >= sel_alto(s)) return 0;
    if (!s->mascara) return 1;
    return s->mascara[ly * sel_ancho(s) + lx] != 0;
}

Zona seleccion_caja(Seleccion *s) {
    if (!s->activa) return zona_vacia();
    return zona_hacer(s->x0, s->y0, sel_ancho(s), sel_alto(s));
}

void seleccion_levantar(Seleccion *s, Documento *d) {
    int x, y, w, h, i;
    Capa *c;
    if (!s->activa || !d || s->flotante) return;
    w = sel_ancho(s);
    h = sel_alto(s);
    if (w <= 0 || h <= 0) return;
    if (d->activa < 0 || d->activa >= d->num_capas) return;
    c = &d->capas[d->activa];
    s->flotante = (u32 *)calloc((size_t)w * h, sizeof(u32));
    if (!s->flotante) return;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int px = s->x0 + x, py = s->y0 + y;
            if (px < 0 || py < 0 || px >= d->ancho || py >= d->alto) continue;
            if (!s->mascara || s->mascara[y * w + x]) {
                size_t p = (size_t)py * d->ancho + px;
                s->flotante[y * w + x] = c->pixeles[p];
                c->pixeles[p] = 0;
            }
        }
    }
    d->compuesto_sucio = 1;
    d->modificado = 1;
    (void)i;
}

void seleccion_soltar(Seleccion *s, Documento *d, PilaUndo *deshacer) {
    int x, y, w, h;
    Capa *c;
    if (!s->flotante || !d) return;
    if (deshacer) pila_guardar(deshacer, d);
    if (d->activa < 0 || d->activa >= d->num_capas) return;
    c = &d->capas[d->activa];
    w = sel_ancho(s);
    h = sel_alto(s);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int px = s->x0 + x, py = s->y0 + y;
            u32 src;
            if (px < 0 || py < 0 || px >= d->ancho || py >= d->alto) continue;
            if (s->mascara && !s->mascara[y * w + x]) continue;
            src = s->flotante[y * w + x];
            if ((src >> 24) == 0) continue;
            {
                size_t p = (size_t)py * d->ancho + px;
                c->pixeles[p] = color_mezclar(c->pixeles[p], src, (int)((src >> 24) & 0xFF));
            }
        }
    }
    free(s->flotante);
    s->flotante = NULL;
    d->compuesto_sucio = 1;
    d->modificado = 1;
}

void seleccion_mover(Seleccion *s, Documento *d, int dx, int dy) {
    (void)d;
    if (!s->activa) return;
    s->x0 += dx;
    s->y0 += dy;
    s->x1 += dx;
    s->y1 += dy;
}

void seleccion_borrar_contenido(Seleccion *s, Documento *d) {
    int x, y, w, h;
    Capa *c;
    if (!s->activa || !d) return;
    if (d->activa < 0 || d->activa >= d->num_capas) return;
    c = &d->capas[d->activa];
    w = sel_ancho(s);
    h = sel_alto(s);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int px = s->x0 + x, py = s->y0 + y;
            if (px < 0 || py < 0 || px >= d->ancho || py >= d->alto) continue;
            if (s->mascara && !s->mascara[y * w + x]) continue;
            c->pixeles[(size_t)py * d->ancho + px] = 0;
        }
    }
    d->compuesto_sucio = 1;
    d->modificado = 1;
}

void seleccion_reescalar(Seleccion *s, int ancho, int alto) {
    int x, y, ow, oh;
    u32 *nuevo;
    u8 *nueva;
    if (!s->flotante || ancho < 1 || alto < 1) return;
    ow = sel_ancho(s);
    oh = sel_alto(s);
    if (ow == ancho && oh == alto) return;
    nuevo = (u32 *)calloc((size_t)ancho * alto, sizeof(u32));
    nueva = (u8 *)calloc((size_t)ancho * alto, 1);
    if (!nuevo || !nueva) { free(nuevo); free(nueva); return; }
    for (y = 0; y < alto; y++) {
        int sy = y * oh / alto;
        for (x = 0; x < ancho; x++) {
            int sx = x * ow / ancho;
            nuevo[y * ancho + x] = s->flotante[sy * ow + sx];
            nueva[y * ancho + x] = s->mascara ? s->mascara[sy * ow + sx] : 1;
        }
    }
    free(s->flotante);
    free(s->mascara);
    s->flotante = nuevo;
    s->mascara = nueva;
    s->x1 = s->x0 + ancho;
    s->y1 = s->y0 + alto;
}
