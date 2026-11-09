/* ==========================================================================
 * Modo tiles: cuadricula de lienzos independientes.
 *
 * Estructura de la cuadricula (`TilesLayout`, `tiles_coords`, `layout_tiles`,
 * `preparar_tiles`, `anadir_tile`) y de su visor flotante con la composicion
 * completa. El tile activo vive en `g.doc`; los demas se guardan en
 * `g.tiles_modo` sin copiar el documento activo.
 * ========================================================================== */
#include "app.h"

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */
static int div_euclidea(int a, int b) {
    int q, r;
    if (b <= 0) b = 1;
    q = a / b;
    r = a % b;
    if (r < 0) { r += b; q -= 1; }
    return q;
}

static int modulo_euclideo(int a, int b) {
    int r;
    if (b <= 0) b = 1;
    r = a % b;
    if (r < 0) r += b;
    return r;
}

static int coord_igual(TileCoord a, TileCoord b) {
    return a.cx == b.cx && a.cy == b.cy;
}

TileCoord tiles_modo_activo(void) {
    TileCoord c;
    c.cx = g.tile_cx;
    c.cy = g.tile_cy;
    return c;
}

/* Indice del documento guardado en una celda, o -1. La celda activa no esta
 * en el almacen: su documento es `g.doc`. */
static int indice_documento(TileCoord c) {
    int i;
    for (i = 0; i < g.num_tiles_modo; i++) {
        if (g.tiles_modo[i].ocupado &&
            g.tiles_modo[i].cx == c.cx && g.tiles_modo[i].cy == c.cy)
            return i;
    }
    return -1;
}

static Documento *documento_de_celda(TileCoord c) {
    int i = indice_documento(c);
    if (i >= 0) return &g.tiles_modo[i].doc;
    if (coord_igual(c, tiles_modo_activo())) return &g.doc;
    return NULL;
}

int tiles_modo_celda_ocupada(TileCoord c, int *indice) {
    int i;
    if (coord_igual(c, tiles_modo_activo())) {
        if (indice) *indice = -1;
        return 1;
    }
    i = indice_documento(c);
    if (i >= 0) {
        if (indice) *indice = i;
        return 1;
    }
    if (indice) *indice = -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Cuadricula                                                          */
/* ------------------------------------------------------------------ */
/* Coordenadas de todos los tiles, incluido el activo, ordenadas igual que
 * `sort_unstable` sobre la tupla (cx, cy). */
static int construir_coords(TileCoord *coords) {
    int n = 0, i, j;
    for (i = 0; i < g.num_tiles_modo; i++) {
        if (!g.tiles_modo[i].ocupado) continue;
        coords[n].cx = g.tiles_modo[i].cx;
        coords[n].cy = g.tiles_modo[i].cy;
        n++;
    }
    {
        TileCoord activo = tiles_modo_activo();
        int existe = 0;
        for (i = 0; i < n; i++)
            if (coord_igual(coords[i], activo)) { existe = 1; break; }
        if (!existe && n < TILES_MODO_MAX + 1) coords[n++] = activo;
    }
    for (i = 1; i < n; i++) {
        TileCoord t = coords[i];
        j = i - 1;
        while (j >= 0 && (coords[j].cx > t.cx ||
                          (coords[j].cx == t.cx && coords[j].cy > t.cy))) {
            coords[j + 1] = coords[j];
            j--;
        }
        coords[j + 1] = t;
    }
    return n;
}

static void layout_calcular(Zona zona, const TileCoord *coords, int ncoords,
                            TileCoord activo, int nominal_w, int nominal_h,
                            int hueco, int ajustar, TilesLayout *out) {
    int min_cx = activo.cx, max_cx = activo.cx;
    int min_cy = activo.cy, max_cy = activo.cy;
    int cols, rows, ancho_huecos, alto_huecos, disponible_w, disponible_h;
    float escala_w, escala_h, escala;
    int i;

    for (i = 0; i < ncoords; i++) {
        if (coords[i].cx < min_cx) min_cx = coords[i].cx;
        if (coords[i].cx > max_cx) max_cx = coords[i].cx;
        if (coords[i].cy < min_cy) min_cy = coords[i].cy;
        if (coords[i].cy > max_cy) max_cy = coords[i].cy;
    }
    cols = max_cx - min_cx + 1;
    rows = max_cy - min_cy + 1;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    if (hueco < 0) hueco = 0;
    if (nominal_w < 1) nominal_w = 1;
    if (nominal_h < 1) nominal_h = 1;

    /* Los huecos exteriores tambien cuentan: en ellos viven los botones "+". */
    ancho_huecos = (cols + 1) * hueco;
    alto_huecos = (rows + 1) * hueco;
    disponible_w = zona.ancho - ancho_huecos;
    disponible_h = zona.alto - alto_huecos;
    if (disponible_w < 1) disponible_w = 1;
    if (disponible_h < 1) disponible_h = 1;
    escala_w = (float)disponible_w / (float)(nominal_w * cols);
    escala_h = (float)disponible_h / (float)(nominal_h * rows);
    escala = escala_w < escala_h ? escala_w : escala_h;
    if (!ajustar && escala > 1.0f) escala = 1.0f;

    out->min_cx = min_cx;
    out->min_cy = min_cy;
    out->tile_w = (int)floorf((float)nominal_w * escala);
    out->tile_h = (int)floorf((float)nominal_h * escala);
    if (out->tile_w < 1) out->tile_w = 1;
    if (out->tile_h < 1) out->tile_h = 1;
    out->hueco = hueco;
    out->paso_x = out->tile_w + hueco;
    out->paso_y = out->tile_h + hueco;
    out->origen_x = zona.x +
        (zona.ancho - (cols * out->tile_w + ancho_huecos)) / 2 + hueco;
    out->origen_y = zona.y +
        (zona.alto - (rows * out->tile_h + alto_huecos)) / 2 + hueco;
}

Zona tiles_modo_zona_celda(TileCoord c) {
    TilesLayout *l = &g.tiles_layout;
    return zona_hacer(l->origen_x + (c.cx - l->min_cx) * l->paso_x,
                      l->origen_y + (c.cy - l->min_cy) * l->paso_y,
                      l->tile_w, l->tile_h);
}

Zona tiles_modo_boton_zona(TileCoord c, int dir, int *existe) {
    TilesLayout *l = &g.tiles_layout;
    TileCoord vecina = c;
    Zona t;
    int lado, x, y;

    if (dir == TILES_DIR_ARRIBA) vecina.cy -= 1;
    else if (dir == TILES_DIR_DERECHA) vecina.cx += 1;
    else if (dir == TILES_DIR_ABAJO) vecina.cy += 1;
    else vecina.cx -= 1;

    if (tiles_modo_celda_ocupada(vecina, NULL)) {
        if (existe) *existe = 0;
        return zona_vacia();
    }
    t = tiles_modo_zona_celda(c);
    lado = TILES_BOTON < l->hueco ? TILES_BOTON : l->hueco;
    if (lado < 4) lado = 4;
    switch (dir) {
        case TILES_DIR_ARRIBA:
            x = t.x + (t.ancho - lado) / 2;
            y = t.y - l->hueco + (l->hueco - lado) / 2;
            break;
        case TILES_DIR_DERECHA:
            x = zona_derecha(t) + (l->hueco - lado) / 2;
            y = t.y + (t.alto - lado) / 2;
            break;
        case TILES_DIR_ABAJO:
            x = t.x + (t.ancho - lado) / 2;
            y = zona_abajo(t) + (l->hueco - lado) / 2;
            break;
        default:
            x = t.x - l->hueco + (l->hueco - lado) / 2;
            y = t.y + (t.alto - lado) / 2;
            break;
    }
    if (existe) *existe = 1;
    return zona_hacer(x, y, lado, lado);
}

void tiles_modo_layout(void) {
    TileCoord coords[TILES_MODO_MAX + 1];
    TileCoord activo = tiles_modo_activo();
    int n, pct = zoom_porcentaje(g.zoom_idx);
    int nominal_w = lado_con_zoom(g.doc.ancho, pct);
    int nominal_h = lado_con_zoom(g.doc.alto, pct);
    Zona zona = zona_trabajo();
    Zona activa;

    if (nominal_w < 1) nominal_w = 1;
    if (nominal_h < 1) nominal_h = 1;
    n = construir_coords(coords);
    layout_calcular(zona, coords, n, activo, nominal_w, nominal_h,
                    TILES_HUECO, 0, &g.tiles_layout);
    g.tiles_layout_valido = 1;
    g.num_tiles_visibles = n;
    memcpy(g.tiles_visibles, coords, (size_t)n * sizeof(TileCoord));

    activa = tiles_modo_zona_celda(activo);
    g.lienzo.x0 = activa.x;
    g.lienzo.y0 = activa.y;
    g.lienzo.x1 = zona_derecha(activa);
    g.lienzo.y1 = zona_abajo(activa);
}

int tiles_modo_celda_en(int x, int y, TileCoord *out) {
    int i;
    if (!g.tiles_layout_valido) tiles_modo_layout();
    for (i = 0; i < g.num_tiles_visibles; i++) {
        if (zona_contiene(tiles_modo_zona_celda(g.tiles_visibles[i]), x, y)) {
            if (out) *out = g.tiles_visibles[i];
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Puntos globales y locales                                           */
/* ------------------------------------------------------------------ */
static void eje_global(int origen, int paso, int lado, int min_celda,
                       int pos, int tam, int *celda, int *local) {
    int rel, dentro;
    if (paso < 1) paso = 1;
    if (lado < 1) lado = 1;
    rel = pos - origen;
    *celda = min_celda + div_euclidea(rel, paso);
    dentro = modulo_euclideo(rel, paso);
    *local = (dentro < lado) ? (int)(((long long)dentro * tam) / lado) : tam;
}

int tiles_punto_global(int x, int y, int *gx, int *gy) {
    int tam_w = g.doc.ancho > 0 ? g.doc.ancho : 1;
    int tam_h = g.doc.alto > 0 ? g.doc.alto : 1;
    int cx, lx, cy, ly;
    TilesLayout *l = &g.tiles_layout;
    if (!g.tiles_layout_valido) return 0;
    eje_global(l->origen_x, l->paso_x, l->tile_w, l->min_cx, x, tam_w, &cx, &lx);
    eje_global(l->origen_y, l->paso_y, l->tile_h, l->min_cy, y, tam_h, &cy, &ly);
    *gx = cx * tam_w + lx;
    *gy = cy * tam_h + ly;
    return 1;
}

void tiles_punto_local(TileCoord c, int gx, int gy, int *lx, int *ly) {
    int tam_w = g.doc.ancho > 0 ? g.doc.ancho : 1;
    int tam_h = g.doc.alto > 0 ? g.doc.alto : 1;
    int x0 = c.cx * tam_w;
    int y0 = c.cy * tam_h;
    int x = gx;
    int y = gy;
    if (x < x0) x = x0;
    if (x > x0 + tam_w) x = x0 + tam_w;
    if (y < y0) y = y0;
    if (y > y0 + tam_h) y = y0 + tam_h;
    *lx = x - x0;
    *ly = y - y0;
}

/* ------------------------------------------------------------------ */
/* Entrada y salida del modo                                           */
/* ------------------------------------------------------------------ */
void tiles_modo_limpiar(void) {
    int i;
    for (i = 0; i < g.num_tiles_modo; i++) {
        if (g.tiles_modo[i].ocupado) doc_liberar(&g.tiles_modo[i].doc);
        g.tiles_modo[i].ocupado = 0;
        g.tiles_modo[i].cx = 0;
        g.tiles_modo[i].cy = 0;
    }
    g.num_tiles_modo = 0;
}

void tiles_modo_iniciar(void) {
    g.modo_tiles = 0;
    g.tile_cx = 0;
    g.tile_cy = 0;
    g.num_tiles_modo = 0;
    memset(&g.tiles_layout, 0, sizeof(g.tiles_layout));
    g.tiles_layout_valido = 0;
    g.num_tiles_visibles = 0;
    g.tiles_visible = 0;
    g.tiles_plegado = 0;
    g.tiles_x = -1;
    g.tiles_y = -1;
    g.arrastre_tiles = 0;
    g.arrastre_tiles_x = 0;
    g.arrastre_tiles_y = 0;
}

void tiles_modo_liberar(void) {
    tiles_modo_limpiar();
}

void tiles_modo_entrar(void) {
    int i;
    /* Al entrar en tiles se sale del editor de mapas o del gestor. */
    if (g.vista != VISTA_PINTAR) g.vista = VISTA_PINTAR;

    if (g.sel.activa) pintura_confirmar_seleccion();
    seleccion_limpiar(&g.sel);
    if (g.texto_activo) texto_cancelar();
    g.pintando = 0;
    if (g.preview_activa) pintura_figura_cancelar();
    g.preview_activa = 0;
    g.poligono_activo = 0;
    g.lienzo_tirador = -1;
    g.sel_tirador = -1;

    /* Si el lienzo activo cambio de tamano fuera del modo tiles, los demas
     * ya no encajan: se descartan. */
    for (i = 0; i < g.num_tiles_modo; i++) {
        if (g.tiles_modo[i].ocupado &&
            (g.tiles_modo[i].doc.ancho != g.doc.ancho ||
             g.tiles_modo[i].doc.alto != g.doc.alto)) {
            tiles_modo_limpiar();
            break;
        }
    }
    /* La celda activa la ocupa `g.doc`, no el almacen. */
    {
        int idx = indice_documento(tiles_modo_activo());
        if (idx >= 0) {
            doc_liberar(&g.tiles_modo[idx].doc);
            g.tiles_modo[idx].ocupado = 0;
            for (i = idx; i + 1 < g.num_tiles_modo; i++)
                g.tiles_modo[i] = g.tiles_modo[i + 1];
            g.num_tiles_modo--;
        }
    }

    g.modo_tiles = 1;
    g.tiles_visible = 1;
    g.tiles_plegado = 0;
    g.arrastre_tiles = 0;
    tiles_modo_layout();
    app_invalidar();
}

void tiles_modo_salir(void) {
    g.modo_tiles = 0;
    g.tiles_visible = 0;
    g.tiles_plegado = 0;
    g.arrastre_tiles = 0;
    g.pintando = 0;
    lienzo_actualizar();
    app_invalidar();
}

/* ------------------------------------------------------------------ */
/* Operaciones sobre la cuadricula                                     */
/* ------------------------------------------------------------------ */
int tiles_modo_agregar(TileCoord desde, int dir) {
    TileCoord destino = desde;
    TileDoc *slot;
    Documento nuevo;

    if (dir == TILES_DIR_ARRIBA) destino.cy -= 1;
    else if (dir == TILES_DIR_DERECHA) destino.cx += 1;
    else if (dir == TILES_DIR_ABAJO) destino.cy += 1;
    else destino.cx -= 1;

    if (tiles_modo_celda_ocupada(destino, NULL)) return 0;
    if (g.num_tiles_modo >= TILES_MODO_MAX) return 0;

    nuevo = doc_nuevo(g.doc.ancho, g.doc.alto, LIENZO);
    if (!nuevo.capas[0].pixeles) {
        doc_liberar(&nuevo);
        return 0;
    }
    doc_componer_todo(&nuevo);

    slot = &g.tiles_modo[g.num_tiles_modo];
    memset(slot, 0, sizeof(*slot));
    slot->ocupado = 1;
    slot->cx = destino.cx;
    slot->cy = destino.cy;
    slot->doc = nuevo;
    g.num_tiles_modo++;
    tiles_modo_layout();
    return 1;
}

int tiles_modo_intercambiar(TileCoord destino) {
    int indice = indice_documento(destino);
    if (indice < 0) return 0;

    if (g.sel.activa) pintura_confirmar_seleccion();
    seleccion_limpiar(&g.sel);
    if (g.preview_activa) pintura_figura_cancelar();
    g.pintando = 0;
    if (g.texto_activo) texto_cancelar();
    g.texto_activo = 0;

    doc_intercambiar(&g.doc, &g.tiles_modo[indice].doc);
    g.tile_cx = destino.cx;
    g.tile_cy = destino.cy;
    tiles_modo_layout();
    return 1;
}

/* ------------------------------------------------------------------ */
/* Visor flotante                                                      */
/* ------------------------------------------------------------------ */
static Zona patron_ui_zona(void) {
    /* La ventana del patron y la del visor comparten tamano y recorte. */
    return patron_zona();
}

Zona tiles_visor_zona(void) {
    Zona trabajo = zona_trabajo();
    Zona patron;
    int ancho = TILES_VISTA_ANCHO;
    int alto = TILES_VISTA_ALTO;
    int x, y;

    if (ancho > trabajo.ancho - TILES_VISTA_MARGEN * 2)
        ancho = trabajo.ancho - TILES_VISTA_MARGEN * 2;
    if (ancho < 0) ancho = 0;
    if (alto > trabajo.alto) alto = trabajo.alto;
    if (alto < 0) alto = 0;

    if (g.tiles_x >= 0 && g.tiles_y >= 0) {
        int max_x = zona_derecha(trabajo) - ancho;
        int max_y = zona_abajo(trabajo) - alto;
        if (max_x < trabajo.x) max_x = trabajo.x;
        if (max_y < trabajo.y) max_y = trabajo.y;
        x = g.tiles_x;
        y = g.tiles_y;
        if (x < trabajo.x) x = trabajo.x;
        if (y < trabajo.y) y = trabajo.y;
        if (x > max_x) x = max_x;
        if (y > max_y) y = max_y;
    } else {
        /* Por defecto, a la derecha de la ventana del patron, recortado al
         * area de trabajo igual que `zona_ventana_patron(.., Some(..))`. */
        int max_x, max_y;
        patron = patron_ui_zona();
        x = zona_derecha(patron) + TILES_VISTA_MARGEN;
        y = patron.y;
        max_x = zona_derecha(trabajo) - ancho;
        max_y = zona_abajo(trabajo) - alto;
        if (max_x < trabajo.x) max_x = trabajo.x;
        if (max_y < trabajo.y) max_y = trabajo.y;
        if (x < trabajo.x) x = trabajo.x;
        if (y < trabajo.y) y = trabajo.y;
        if (x > max_x) x = max_x;
        if (y > max_y) y = max_y;
    }
    return zona_hacer(x, y, ancho, alto);
}

Zona tiles_visor_titulo_zona(void) {
    Zona p = tiles_visor_zona();
    int alto = TILES_VISTA_TITULO < p.alto ? TILES_VISTA_TITULO : p.alto;
    return zona_hacer(p.x, p.y, p.ancho, alto);
}

Zona tiles_visor_minimizar_zona(void) {
    Zona p = tiles_visor_zona();
    int lado = 16 < p.ancho ? 16 : p.ancho;
    int alto = 16 < p.alto ? 16 : p.alto;
    return zona_hacer(zona_derecha(p) - TILES_VISTA_PADDING - 16,
                      p.y + (TILES_VISTA_TITULO - 16) / 2, lado, alto);
}

Zona tiles_visor_contenido_zona(void) {
    Zona p = tiles_visor_zona();
    return zona_hacer(p.x + TILES_VISTA_PADDING,
                      p.y + TILES_VISTA_TITULO + TILES_VISTA_PADDING,
                      p.ancho - TILES_VISTA_PADDING * 2,
                      p.alto - TILES_VISTA_TITULO - TILES_VISTA_PADDING * 2);
}

Zona tiles_visor_plegado_zona(void) {
    Zona opciones = opciones_minimizada_zona();
    int y = zona_abajo(opciones) + OPCIONES_VENTANA_MIN_HUECO;
    /* Se apila bajo la tira del patron, que queda entre las dos ventanas. */
    y += OPCIONES_VENTANA_MIN_ALTO + OPCIONES_VENTANA_MIN_HUECO;
    return zona_hacer(SIDEBAR_MARGEN, y, AJUSTES_ANCHO,
                      OPCIONES_VENTANA_MIN_ALTO);
}

/* ------------------------------------------------------------------ */
/* Dibujo                                                              */
/* ------------------------------------------------------------------ */
static void dibujar_boton_mas(Zona z, int hover) {
    int cx, cy, mitad;
    u32 color;
    if (z.ancho <= 0 || z.alto <= 0) return;
    dibujar_rect_z(z, hover ? TILES_RESALTE : TILES_PANEL_FONDO);
    dibujar_marco_z(z, TILES_PANEL_BORDE);
    cx = z.x + z.ancho / 2;
    cy = z.y + z.alto / 2;
    mitad = (z.ancho - 8) > 4 ? (z.ancho - 8) / 2 : 2;
    if (mitad < 2) mitad = 2;
    color = TILES_TEXTO;
    dibujar_rect(cx - mitad, cy - 1, cx + mitad + 1, cy + 1, color);
    dibujar_rect(cx - 1, cy - mitad, cx + 1, cy + mitad + 1, color);
}

static void dibujar_boton_minimizar(Zona z, int hover) {
    int cx, cy;
    if (z.ancho <= 0 || z.alto <= 0) return;
    if (hover) dibujar_rect_z(z, TILES_RESALTE);
    dibujar_marco_z(z, TILES_PANEL_BORDE);
    cx = z.x + 2;
    cy = z.y + z.alto / 2;
    dibujar_rect(cx, cy, cx + z.ancho - 4, cy + 1, TILES_TEXTO);
}

/* Dibuja una cuadricula de tiles dentro de una zona ya calculada. */
static void dibujar_cuadricula(TilesLayout *layout, const TileCoord *coords,
                               int ncoords, TileCoord activo, Documento *activo_doc,
                               int botones) {
    int i, d;

    for (i = 0; i < ncoords; i++) {
        TileCoord c = coords[i];
        Zona z = zona_hacer(layout->origen_x + (c.cx - layout->min_cx) * layout->paso_x,
                            layout->origen_y + (c.cy - layout->min_cy) * layout->paso_y,
                            layout->tile_w, layout->tile_h);
        Documento *d_doc;
        if (zona_derecha(z) < 0 || zona_abajo(z) < 0 ||
            z.x >= g.ancho || z.y >= g.alto)
            continue;
        dibujar_rect(z.x - 1, z.y - 1, zona_derecha(z) + 1, zona_abajo(z) + 1,
                     coord_igual(c, activo) ? TILES_BORDE_ACTIVO : TILES_BORDE);

        d_doc = coord_igual(c, activo) ? activo_doc : documento_de_celda(c);
        if (d_doc) {
            if (!d_doc->compuesto || d_doc->compuesto_sucio)
                doc_componer_todo(d_doc);
            {
                Lienzo l;
                l.x0 = z.x; l.y0 = z.y;
                l.x1 = zona_derecha(z); l.y1 = zona_abajo(z);
                pintura_dibujar_documento(d_doc, l);
            }
        } else {
            dibujar_rect_z(z, LIENZO);
        }
    }

    if (!botones) return;
    for (i = 0; i < ncoords; i++) {
        for (d = 0; d < 4; d++) {
            int existe = 0;
            Zona b = tiles_modo_boton_zona(coords[i], d, &existe);
            int hover = g.raton_dentro && zona_contiene(b, g.raton_x, g.raton_y);
            if (existe) dibujar_boton_mas(b, hover);
        }
    }
}

static TilesLayout layout_visor;
static TileCoord coords_visor[TILES_MODO_MAX + 1];
static int num_coords_visor;

static void preparar_layout_visor(Zona interior) {
    Documento *activo_doc = &g.doc;
    TileCoord activo = tiles_modo_activo();
    int nominal_w = activo_doc->ancho > 0 ? activo_doc->ancho : 1;
    int nominal_h = activo_doc->alto > 0 ? activo_doc->alto : 1;
    num_coords_visor = construir_coords(coords_visor);
    layout_calcular(interior, coords_visor, num_coords_visor, activo,
                    nominal_w, nominal_h, TILES_HUECO_VISTA, 1, &layout_visor);
}

void ui_dibujar_tiles(void) {
    if (!g.modo_tiles) return;
    if (!g.tiles_layout_valido) tiles_modo_layout();
    dibujar_cuadricula(&g.tiles_layout, g.tiles_visibles,
                       g.num_tiles_visibles, tiles_modo_activo(), texto_documento_visible(), 1);
}

void ui_dibujar_tiles_visor(void) {
    Zona panel, minimizar, contenido, interior;
    int ty;
    int minimizar_hover;

    if (!g.modo_tiles || g.vista != VISTA_PINTAR) return;
    if (!g.tiles_visible) return;
    if (g.tiles_plegado) {
        Zona tira = tiles_visor_plegado_zona();
        int hover = g.raton_dentro && zona_contiene(tira, g.raton_x, g.raton_y);
        int ax, ay;
        if (tira.ancho <= 0 || tira.y >= g.alto) return;
        dibujar_rect_z(tira, hover ? TILES_RESALTE : TILES_PANEL_FONDO);
        dibujar_marco_z(tira, TILES_PANEL_BORDE);
        fuente_dibujar(tira.x + 6, tira.y + (tira.alto - 7) / 2,
                       "TILES", TILES_TEXTO);
        ax = zona_derecha(tira) - 10;
        ay = tira.y + tira.alto / 2;
        dibujar_rect(ax, ay, ax + 5, ay + 1, TILES_TEXTO);
        dibujar_rect(ax + 2, ay - 1, ax + 3, ay + 1, TILES_TEXTO);
        return;
    }

    panel = tiles_visor_zona();
    if (panel.ancho <= 0 || panel.alto <= 0 || panel.y >= g.alto) return;
    minimizar = tiles_visor_minimizar_zona();
    contenido = tiles_visor_contenido_zona();
    minimizar_hover = g.raton_dentro && zona_contiene(minimizar, g.raton_x, g.raton_y);

    dibujar_rect(panel.x + 3, panel.y + 3,
                 zona_derecha(panel) + 3, zona_abajo(panel) + 3, TILES_SOMBRA);
    dibujar_rect_z(panel, TILES_PANEL_FONDO);
    dibujar_marco_z(panel, TILES_PANEL_BORDE);

    ty = panel.y + (TILES_VISTA_TITULO - 7) / 2;
    fuente_dibujar(panel.x + TILES_VISTA_PADDING, ty, "VISTA DE TILES", TILES_TEXTO);
    dibujar_boton_minimizar(minimizar, minimizar_hover);
    dibujar_rect(panel.x, panel.y + TILES_VISTA_TITULO - 1,
                 zona_derecha(panel), panel.y + TILES_VISTA_TITULO, TILES_PANEL_BORDE);

    if (contenido.ancho <= 0 || contenido.alto <= 0) return;
    dibujar_rect_z(contenido, TILES_FONDO);
    dibujar_marco_z(contenido, TILES_PANEL_BORDE);
    interior = zona_hacer(contenido.x + 1, contenido.y + 1,
                          contenido.ancho - 2, contenido.alto - 2);
    if (interior.ancho < 0) interior.ancho = 0;
    if (interior.alto < 0) interior.alto = 0;
    preparar_layout_visor(interior);
    dibujar_cuadricula(&layout_visor, coords_visor, num_coords_visor,
                       tiles_modo_activo(), texto_documento_visible(), 0);
}

void ui_dibujar_tiles_minimizado(void) {
    ui_dibujar_tiles_visor();
}

/* ------------------------------------------------------------------ */
/* Interaccion                                                         */
/* ------------------------------------------------------------------ */
int clic_tiles_visor(int x, int y) {
    Zona panel;
    if (g.vista != VISTA_PINTAR || !g.modo_tiles || !g.tiles_visible) return 0;

    if (g.tiles_plegado) {
        if (zona_contiene(tiles_visor_plegado_zona(), x, y)) {
            g.tiles_plegado = 0;
            app_invalidar();
            return 1;
        }
        return 0;
    }

    panel = tiles_visor_zona();
    if (!zona_contiene(panel, x, y)) return 0;
    if (zona_contiene(tiles_visor_minimizar_zona(), x, y)) {
        g.tiles_plegado = 1;
        g.arrastre_tiles = 0;
        app_invalidar();
        return 1;
    }
    if (zona_contiene(tiles_visor_titulo_zona(), x, y)) {
        g.arrastre_tiles = 1;
        g.arrastre_tiles_x = x - panel.x;
        g.arrastre_tiles_y = y - panel.y;
        return 1;
    }
    /* El contenido no pinta: solo muestra la composicion. */
    return 1;
}

void tiles_modo_mover_visor(int x, int y) {
    if (!g.arrastre_tiles) return;
    g.tiles_x = x - g.arrastre_tiles_x;
    g.tiles_y = y - g.arrastre_tiles_y;
    {
        Zona p = tiles_visor_zona();
        g.tiles_x = p.x;
        g.tiles_y = p.y;
    }
    app_invalidar();
}

void tiles_modo_soltar(void) {
    g.arrastre_tiles = 0;
}

int tiles_modo_pulsar(int x, int y, int boton) {
    TileCoord c;
    int i, d;

    if (!g.modo_tiles || g.vista != VISTA_PINTAR) return 0;
    if (boton != 0) return 0;
    if (!g.tiles_layout_valido) tiles_modo_layout();

    /* Las ventanas flotantes ya han tenido prioridad en app_pulsar; aun asi,
     * el visor nunca toca la cuadricula. */
    if (g.tiles_visible && !g.tiles_plegado &&
        zona_contiene(tiles_visor_zona(), x, y))
        return 0;
    if (g.tiles_plegado && zona_contiene(tiles_visor_plegado_zona(), x, y))
        return 0;

    /* Boton "+": anade un tile vacio en la celda vecina. */
    for (i = 0; i < g.num_tiles_visibles; i++) {
        for (d = 0; d < 4; d++) {
            int existe = 0;
            Zona b = tiles_modo_boton_zona(g.tiles_visibles[i], d, &existe);
            if (existe && zona_contiene(b, x, y)) {
                tiles_modo_agregar(g.tiles_visibles[i], d);
                app_invalidar();
                return 1;
            }
        }
    }

    /* Un clic sobre otro tile lo convierte en el activo y permite pintar en el
     * mismo clic. */
    if (tiles_modo_celda_en(x, y, &c) && !coord_igual(c, tiles_modo_activo())) {
        if (tiles_modo_intercambiar(c)) app_invalidar();
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Cruce de trazos entre tiles                                         */
/* ------------------------------------------------------------------ */
int tiles_modo_cruzar_trazo(int x, int y) {
    TileCoord destino, activo;
    int gx, gy, bx, by;
    int tam_w, tam_h;
    int anterior_gx, anterior_gy;
    int salida_x, salida_y;
    int hay_destino;

    if (!g.modo_tiles || !g.pintando || !g.tiles_layout_valido) return 0;
    if (g.herramienta != H_LAPIZ && g.herramienta != H_PINCEL &&
        g.herramienta != H_AEROGRAFO && g.herramienta != H_BORRADOR)
        return 0;

    activo = tiles_modo_activo();
    hay_destino = tiles_modo_celda_en(x, y, &destino);
    if (hay_destino && coord_igual(destino, activo)) return 0;

    tiles_punto_global(x, y, &gx, &gy);
    tiles_punto_local(activo, gx, gy, &bx, &by);
    tam_w = g.doc.ancho > 0 ? g.doc.ancho : 1;
    tam_h = g.doc.alto > 0 ? g.doc.alto : 1;

    if (!hay_destino) {
        /* El cursor esta en el hueco: el trazo llega al borde y espera. */
        if (bx != g.ult_x || by != g.ult_y)
            pintura_trazo_linea(g.ult_x, g.ult_y, bx, by, g.boton_pintura);
        return 1;
    }

    anterior_gx = activo.cx * tam_w + g.ult_x;
    anterior_gy = activo.cy * tam_h + g.ult_y;
    tiles_punto_local(activo, gx, gy, &salida_x, &salida_y);
    if (salida_x != g.ult_x || salida_y != g.ult_y)
        pintura_trazo_linea(g.ult_x, g.ult_y, salida_x, salida_y, g.boton_pintura);

    if (!tiles_modo_intercambiar(destino)) return 1;

    /* Entrada en el tile nuevo: se continua desde el punto equivalente. */
    tiles_punto_local(destino, anterior_gx, anterior_gy, &bx, &by);
    g.ult_x = bx;
    g.ult_y = by;
    return 0;
}
