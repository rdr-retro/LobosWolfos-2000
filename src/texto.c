/* ==========================================================================
 * Texto: herramienta de texto completa.
 * ========================================================================== */
#include "app.h"

static void mango_mueve_local(int mango, int *izq, int *der, int *arr, int *aba);

static const wchar_t *FUENTES_TEXTO_NOMBRES[TEXTO_FUENTES_TOTAL] = {
    L"Arial", L"Times New Roman", L"Courier New", L"Segoe UI", L"Georgia", L"Verdana"
};

static const int TAMANOS_TEXTO[TEXTO_TAMANOS_TOTAL] = {
    6, 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 40, 48, 64, 80, 96
};

static Documento texto_preview;
static int texto_preview_valida = 0;

static void texto_cache_liberar(TextoRaster *c) {
    int i;
    if (!c) return;
    if (c->cobertura) { free(c->cobertura); c->cobertura = NULL; }
    if (c->lineas) {
        for (i = 0; i < c->num_lineas; i++) {
            if (c->lineas[i].anchos) free(c->lineas[i].anchos);
            c->lineas[i].anchos = NULL;
            c->lineas[i].num_anchos = 0;
        }
        free(c->lineas);
        c->lineas = NULL;
    }
    c->num_lineas = 0;
    c->ancho = 0; c->alto = 0; c->alto_linea = 0;
}

static int rasterizar_texto(const u16 *contenido, int largo, int ancho, int fuente_idx, int tamano, TextoRaster *out) {
    HDC dc;
    HFONT font, font_prev;
    BITMAPINFO bmi;
    HBITMAP bmp, bmp_prev;
    void *bits;
    int fuente, alto_linea, alto, num_rangos = 0, cap_rangos = 0;
    int *rangos_ini = NULL, *rangos_fin = NULL;
    int i, li;

    memset(out, 0, sizeof(*out));
    if (tamano < 1) tamano = 1;
    if (tamano > 512) tamano = 512;
    if (ancho < 1) ancho = 1;
    if (ancho > 8192) ancho = 8192;
    fuente = fuente_idx;
    if (fuente < 0 || fuente >= TEXTO_FUENTES_TOTAL) fuente = TEXTO_FUENTE_POR_DEFECTO;

    dc = CreateCompatibleDC(NULL);
    if (!dc) return 0;
    font = CreateFontW(-tamano, 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 4, 0, FUENTES_TEXTO_NOMBRES[fuente]);
    font_prev = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, 1);
    SetTextColor(dc, RGB(255, 255, 255));
    {
        SIZE sz;
        GetTextExtentPoint32W(dc, L"Ag", 2, &sz);
        alto_linea = sz.cy;
        if (alto_linea < tamano) alto_linea = tamano;
        if (alto_linea < 1) alto_linea = 1;
    }

#define PUSH_RANGE(ii, ff) do { \
    if (num_rangos == cap_rangos) { \
        cap_rangos = cap_rangos ? cap_rangos * 2 : 16; \
        rangos_ini = (int *)realloc(rangos_ini, cap_rangos * sizeof(int)); \
        rangos_fin = (int *)realloc(rangos_fin, cap_rangos * sizeof(int)); \
    } \
    rangos_ini[num_rangos] = (ii); \
    rangos_fin[num_rangos] = (ff); \
    num_rangos++; \
} while (0)

    if (largo <= 0) {
        PUSH_RANGE(0, 0);
    } else {
        int ii = 0;
        int n = largo;
        while (ii < n) {
            if (contenido[ii] == '\n') {
                PUSH_RANGE(ii, ii);
                ii += 1;
                continue;
            }
            int inicio = ii;
            int fin = ii;
            int ultimo_espacio = -1;
            while (fin < n && contenido[fin] != '\n') {
                if (contenido[fin] == ' ') ultimo_espacio = fin;
                int len = fin - inicio + 1;
                SIZE msz;
                GetTextExtentPoint32W(dc, (const WCHAR *)(contenido + inicio), len, &msz);
                int ancho_actual = msz.cx;
                if (ancho_actual > ancho && fin > inicio) break;
                fin += 1;
            }
            if (fin < n && contenido[fin] != '\n') {
                if (ultimo_espacio > inicio) {
                    PUSH_RANGE(inicio, ultimo_espacio);
                    ii = ultimo_espacio + 1;
                } else {
                    PUSH_RANGE(inicio, fin);
                    ii = fin;
                }
            } else {
                PUSH_RANGE(inicio, fin);
                ii = (fin < n) ? (fin + 1) : fin;
            }
        }
        if (largo > 0 && contenido[largo - 1] == '\n') {
            PUSH_RANGE(largo, largo);
        }
    }

#undef PUSH_RANGE

    if (num_rangos <= 0) num_rangos = 0;

    alto = num_rangos * alto_linea;
    if (alto < 1) alto = 1;
    if ((long long)ancho * alto > 8000000) {
        SelectObject(dc, font_prev);
        DeleteObject(font);
        DeleteDC(dc);
        free(rangos_ini); free(rangos_fin);
        return 0;
    }

    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = ancho;
    bmi.bmiHeader.biHeight = -alto;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bits = NULL;
    bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) {
        SelectObject(dc, font_prev);
        DeleteObject(font);
        DeleteDC(dc);
        free(rangos_ini); free(rangos_fin);
        return 0;
    }
    bmp_prev = (HBITMAP)SelectObject(dc, bmp);
    memset(bits, 0, (size_t)ancho * alto * 4);

    for (li = 0; li < num_rangos; li++) {
        int ini = rangos_ini[li], fin = rangos_fin[li];
        if (ini == fin) continue;
        int len = fin - ini;
        TextOutW(dc, 0, li * alto_linea, (const WCHAR *)(contenido + ini), len);
    }

    out->cobertura = (u8 *)calloc((size_t)ancho * alto, 1);
    if (!out->cobertura) {
        SelectObject(dc, bmp_prev);
        SelectObject(dc, font_prev);
        DeleteObject(bmp);
        DeleteObject(font);
        DeleteDC(dc);
        free(rangos_ini); free(rangos_fin);
        return 0;
    }
    for (i = 0; i < ancho * alto; i++) {
        u32 px = ((u32 *)bits)[i];
        int b = (int)(px & 0xFF);
        int gch = (int)((px >> 8) & 0xFF);
        int r = (int)((px >> 16) & 0xFF);
        int cov = r;
        if (gch > cov) cov = gch;
        if (b > cov) cov = b;
        out->cobertura[i] = (u8)cov;
    }

    out->lineas = (LineaTexto *)calloc(num_rangos > 0 ? num_rangos : 1, sizeof(LineaTexto));
    out->num_lineas = num_rangos > 0 ? num_rangos : 1;
    for (li = 0; li < num_rangos; li++) {
        int ini = rangos_ini[li], fin = rangos_fin[li];
        out->lineas[li].inicio = ini;
        out->lineas[li].fin = fin;
        int len = fin - ini + 1;
        if (len < 1) len = 1;
        out->lineas[li].anchos = (int *)malloc(len * sizeof(int));
        out->lineas[li].num_anchos = len;
        for (i = 0; i < len; i++) {
            if (i == 0) { out->lineas[li].anchos[i] = 0; continue; }
            SIZE msz;
            GetTextExtentPoint32W(dc, (const WCHAR *)(contenido + ini), i, &msz);
            out->lineas[li].anchos[i] = msz.cx;
        }
    }
    if (num_rangos == 0) {
        out->lineas[0].inicio = 0;
        out->lineas[0].fin = 0;
        out->lineas[0].anchos = (int *)malloc(sizeof(int));
        out->lineas[0].anchos[0] = 0;
        out->lineas[0].num_anchos = 1;
        out->num_lineas = 1;
    }

    out->ancho = ancho;
    out->alto = alto;
    out->alto_linea = alto_linea;

    SelectObject(dc, bmp_prev);
    SelectObject(dc, font_prev);
    DeleteObject(bmp);
    DeleteObject(font);
    DeleteDC(dc);
    free(rangos_ini);
    free(rangos_fin);
    return 1;
}

/* ---------------- Texto state helpers ---------------- */

static void texto_cache_actualizar(void) {
    Texto *t = &g.texto;
    int ancho_interior;
    if (!g.texto_activo) return;
    ancho_interior = (t->x1 - t->x0 + 1) - TEXTO_MARGEN * 2;
    if (ancho_interior < 8) ancho_interior = 8;
    if (t->cache_clave_valida &&
        t->cache_fuente == t->fuente &&
        t->cache_tamano == t->tamano &&
        t->cache_ancho == ancho_interior &&
        t->cache_revision == t->revision) {
        return;
    }
    texto_cache_liberar(&t->cache);
    t->cache_valida = 0;
    if (rasterizar_texto(t->contenido, t->largo, ancho_interior, t->fuente, t->tamano, &t->cache)) {
        t->cache_valida = 1;
    }
    t->cache_fuente = t->fuente;
    t->cache_tamano = t->tamano;
    t->cache_ancho = ancho_interior;
    t->cache_revision = t->revision;
    t->cache_clave_valida = 1;
}

static void texto_linea_de(int cursor, int *idx_out, int *col_out) {
    Texto *t = &g.texto;
    if (!t->cache_valida) { *idx_out = 0; *col_out = 0; return; }
    int i;
    for (i = 0; i < t->cache.num_lineas; i++) {
        LineaTexto *ln = &t->cache.lineas[i];
        if (cursor <= ln->fin) {
            *idx_out = i;
            *col_out = cursor - ln->inicio;
            return;
        }
    }
    if (t->cache.num_lineas > 0) {
        i = t->cache.num_lineas - 1;
        LineaTexto *ln = &t->cache.lineas[i];
        int colv = cursor - ln->inicio;
        if (colv > ln->fin - ln->inicio) colv = ln->fin - ln->inicio;
        if (colv < 0) colv = 0;
        *idx_out = i;
        *col_out = colv;
    } else {
        *idx_out = 0; *col_out = 0;
    }
}

static void texto_insertar_char(u16 c) {
    Texto *t = &g.texto;
    if (t->largo >= MAX_TEXTO - 1) return;
    if (c == '\n' || c >= 32) {
        memmove(t->contenido + t->cursor + 1, t->contenido + t->cursor,
                (size_t)(t->largo - t->cursor) * sizeof(u16));
        t->contenido[t->cursor] = c;
        t->cursor++;
        t->largo++;
        t->contenido[t->largo] = 0;
        t->revision++;
    }
}

static void texto_retroceso(void) {
    Texto *t = &g.texto;
    if (t->cursor > 0) {
        t->cursor--;
        memmove(t->contenido + t->cursor, t->contenido + t->cursor + 1,
                (size_t)(t->largo - t->cursor - 1) * sizeof(u16) + sizeof(u16));
        t->largo--;
        t->contenido[t->largo] = 0;
        t->revision++;
    }
}

static void texto_borrar(void) {
    Texto *t = &g.texto;
    if (t->cursor < t->largo) {
        memmove(t->contenido + t->cursor, t->contenido + t->cursor + 1,
                (size_t)(t->largo - t->cursor - 1) * sizeof(u16) + sizeof(u16));
        t->largo--;
        t->contenido[t->largo] = 0;
        t->revision++;
    }
}

static void texto_mover_vertical(int dir) {
    Texto *t = &g.texto;
    texto_cache_actualizar();
    if (!t->cache_valida) return;
    int actual, col;
    texto_linea_de(t->cursor, &actual, &col);
    int dest = actual + dir;
    if (dest < 0 || dest >= t->cache.num_lineas) return;
    int x;
    LineaTexto *la = &t->cache.lineas[actual];
    if (col >= 0 && col < la->num_anchos) x = la->anchos[col];
    else x = 0;
    LineaTexto *ld = &t->cache.lineas[dest];
    int mejor = 0, dist = 0x7fffffff;
    int i;
    for (i = 0; i < ld->num_anchos; i++) {
        int d = ld->anchos[i] - x;
        if (d < 0) d = -d;
        if (d < dist) { dist = d; mejor = i; }
    }
    t->cursor = ld->inicio + mejor;
    if (t->cursor > t->largo) t->cursor = t->largo;
}

static void texto_ir_inicio_linea(void) {
    Texto *t = &g.texto;
    texto_cache_actualizar();
    if (!t->cache_valida) return;
    int idx, col;
    texto_linea_de(t->cursor, &idx, &col);
    if (idx < t->cache.num_lineas) t->cursor = t->cache.lineas[idx].inicio;
}

static void texto_ir_fin_linea(void) {
    Texto *t = &g.texto;
    texto_cache_actualizar();
    if (!t->cache_valida) return;
    int idx, col;
    texto_linea_de(t->cursor, &idx, &col);
    if (idx < t->cache.num_lineas) t->cursor = t->cache.lineas[idx].fin;
}

static int texto_cursor_en(int x, int y) {
    Texto *t = &g.texto;
    if (!t->cache_valida || t->cache.num_lineas == 0) {
        return t->cursor > t->largo ? t->largo : t->cursor;
    }
    int rx = x - (t->x0 + TEXTO_MARGEN);
    int ry = y - (t->y0 + TEXTO_MARGEN);
    int alto_linea = t->cache.alto_linea > 0 ? t->cache.alto_linea : 1;
    int idx = ry / alto_linea;
    if (idx < 0) idx = 0;
    if (idx >= t->cache.num_lineas) idx = t->cache.num_lineas - 1;
    LineaTexto *ln = &t->cache.lineas[idx];
    int mejor = 0, dist = 0x7fffffff;
    int i;
    for (i = 0; i < ln->num_anchos; i++) {
        int d = ln->anchos[i] - rx;
        if (d < 0) d = -d;
        if (d < dist) { dist = d; mejor = i; }
    }
    int c = ln->inicio + mejor;
    if (c > t->largo) c = t->largo;
    return c;
}

static int texto_caret_logico(int *cx, int *cy0, int *cy1) {
    Texto *t = &g.texto;
    if (!t->cache_valida) return 0;
    int idx, col;
    texto_linea_de(t->cursor, &idx, &col);
    if (idx >= t->cache.num_lineas) return 0;
    LineaTexto *ln = &t->cache.lineas[idx];
    int x = t->x0 + TEXTO_MARGEN + (col >= 0 && col < ln->num_anchos ? ln->anchos[col] : 0);
    int y0 = t->y0 + TEXTO_MARGEN + idx * t->cache.alto_linea;
    *cx = x; *cy0 = y0; *cy1 = y0 + t->cache.alto_linea;
    return 1;
}

static void texto_caja_logica(int *x0, int *y0, int *x1, int *y1) {
    Texto *t = &g.texto;
    int alto_texto = t->cache_valida ? t->cache.alto : (t->tamano > 1 ? t->tamano : 1);
    int alto = TEXTO_MARGEN * 2 + alto_texto;
    int base = t->y1 - t->y0 + 1;
    if (alto < base) alto = base;
    *x0 = t->x0; *y0 = t->y0; *x1 = t->x1; *y1 = t->y0 + alto - 1;
}

/* Limites de la caja en coordenadas del tile activo. En modo tiles la caja
 * puede salir del documento activo para ocupar los tiles vecinos. */
static void texto_limites_globales(int *x0, int *y0, int *x1, int *y1) {
    TileCoord activo = tiles_modo_activo();
    int w = g.doc.ancho > 0 ? g.doc.ancho : 1;
    int h = g.doc.alto > 0 ? g.doc.alto : 1;
    int i;
    *x0 = 0; *y0 = 0; *x1 = w - 1; *y1 = h - 1;
    if (!g.modo_tiles) return;
    for (i = 0; i < g.num_tiles_modo; i++) {
        TileDoc *td = &g.tiles_modo[i];
        int lx, ly;
        if (!td->ocupado) continue;
        lx = (td->cx - activo.cx) * w;
        ly = (td->cy - activo.cy) * h;
        if (lx < *x0) *x0 = lx;
        if (ly < *y0) *y0 = ly;
        if (lx + w - 1 > *x1) *x1 = lx + w - 1;
        if (ly + h - 1 > *y1) *y1 = ly + h - 1;
    }
}

/* Pixel local del tile activo bajo una posicion de pantalla. En modo tiles no
 * se recorta al documento: la caja puede continuar en el tile vecino y las
 * coordenadas siguen la misma extension lineal que la previsualizacion. */
static int texto_pixel_local(int x, int y, int *px, int *py) {
    int gx, gy;
    TileCoord activo;
    int w, h;
    if (!g.modo_tiles)
        return pintura_pixel_extendido(x, y, px, py);
    if (!g.tiles_layout_valido) tiles_modo_layout();
    if (!g.tiles_layout_valido) return 0;
    tiles_punto_global(x, y, &gx, &gy);
    activo = tiles_modo_activo();
    w = g.doc.ancho > 0 ? g.doc.ancho : 1;
    h = g.doc.alto > 0 ? g.doc.alto : 1;
    *px = gx - activo.cx * w;
    *py = gy - activo.cy * h;
    return 1;
}

/* Comprueba si el texto escribiria algun pixel en `doc`. `origen_x`/`origen_y`
 * es la posicion del documento dentro de las coordenadas del tile activo. */
static int texto_hay_escritura_en(Documento *doc, int origen_x, int origen_y) {
    Texto *t = &g.texto;
    texto_cache_actualizar();
    if (!t->cache_valida) return 0;
    TextoRaster *c = &t->cache;
    int ox = t->x0 + TEXTO_MARGEN - origen_x;
    int oy = t->y0 + TEXTO_MARGEN - origen_y;
    int w = doc->ancho, h = doc->alto;
    int y, x;
    for (y = 0; y < c->alto; y++) {
        for (x = 0; x < c->ancho; x++) {
            u8 a = c->cobertura[y * c->ancho + x];
            if (a == 0) continue;
            int px = ox + x, py = oy + y;
            if (px < 0 || py < 0 || px >= w || py >= h) continue;
            return 1;
        }
    }
    return 0;
}

static void texto_estampar_en(Documento *doc, int origen_x, int origen_y) {
    Texto *t = &g.texto;
    texto_cache_actualizar();
    if (!t->cache_valida) return;
    TextoRaster *c = &t->cache;
    int ox = t->x0 + TEXTO_MARGEN - origen_x;
    int oy = t->y0 + TEXTO_MARGEN - origen_y;
    int w = doc->ancho, h = doc->alto;
    Capa *capa = &doc->capas[doc->activa];
    int y, x;
    for (y = 0; y < c->alto; y++) {
        for (x = 0; x < c->ancho; x++) {
            u8 a = c->cobertura[y * c->ancho + x];
            if (a == 0) continue;
            int px = ox + x, py = oy + y;
            if (px < 0 || py < 0 || px >= w || py >= h) continue;
            u32 dst = capa->pixeles[(size_t)py * w + px];
            u32 color = t->color;
            int aa = a;
            int inv = 255 - aa;
            int r = ((((color >> 16) & 0xFF) * aa + ((dst >> 16) & 0xFF) * inv) / 255) & 0xFF;
            int gch = ((((color >> 8) & 0xFF) * aa + ((dst >> 8) & 0xFF) * inv) / 255) & 0xFF;
            int b = ((((color) & 0xFF) * aa + ((dst) & 0xFF) * inv) / 255) & 0xFF;
            capa->pixeles[(size_t)py * w + px] = 0xFF000000u | (r << 16) | (gch << 8) | b;
        }
    }
    doc->compuesto_sucio = 1;
    doc->modificado = 1;
    doc_componer_todo(doc);
}

static int paso_tamano(int actual, int dir) {
    int i;
    if (dir > 0) {
        for (i = 0; i < TEXTO_TAMANOS_TOTAL; i++)
            if (TAMANOS_TEXTO[i] > actual) return TAMANOS_TEXTO[i];
        return TAMANOS_TEXTO[TEXTO_TAMANOS_TOTAL - 1];
    } else {
        for (i = TEXTO_TAMANOS_TOTAL - 1; i >= 0; i--)
            if (TAMANOS_TEXTO[i] < actual) return TAMANOS_TEXTO[i];
        return TAMANOS_TEXTO[0];
    }
}

/* Menu texto helpers */
static int texto_menu_parte_en(MenuTexto *m, int mx, int my) {
    if (zona_contiene(m->fuente, mx, my)) return 0;
    if (zona_contiene(m->menos, mx, my)) return 1;
    if (zona_contiene(m->mas, mx, my)) return 2;
    if (zona_contiene(m->color, mx, my)) return 3;
    return -1;
}

static int texto_menu_fuente_en(MenuTexto *m, int mx, int my) {
    int i;
    for (i = 0; i < m->num_desplegable; i++) {
        if (zona_contiene(m->desplegable[i], mx, my)) return i;
    }
    return -1;
}

static int texto_menu_contiene(MenuTexto *m, int mx, int my) {
    if (zona_contiene(m->zona, mx, my)) return 1;
    int i;
    for (i = 0; i < m->num_desplegable; i++) {
        if (zona_contiene(m->desplegable[i], mx, my)) return 1;
    }
    return 0;
}

static int texto_menu_zona(MenuTexto *m) {
    Texto *t = &g.texto;
    if (!g.texto_activo || t->inicio_activo) return 0;
    Zona tr = zona_trabajo();
    int x0, y0, x1, y1;
    texto_caja_logica(&x0, &y0, &x1, &y1);
    int sx0, sy0, sx1, sy1;
    pintura_pantalla_caja(x0, y0, x1, y1, &sx0, &sy0, &sx1, &sy1);
    const char *fuente_nombre = "Arial";
    /* map index to name */
    static const char *nombres[TEXTO_FUENTES_TOTAL] = {"Arial","Times New Roman","Courier New","Segoe UI","Georgia","Verdana"};
    fuente_nombre = nombres[t->fuente >= 0 && t->fuente < TEXTO_FUENTES_TOTAL ? t->fuente : 0];
    int ancho_fuente = fuente_texto_ancho(fuente_nombre) + TEXTO_MENU_PADDING * 2 + TEXTO_MENU_FLECHA + TEXTO_MENU_HUECO;
    if (ancho_fuente < 72) ancho_fuente = 72;
    char etiqueta[32];
    formatear(etiqueta, sizeof(etiqueta), "%d px", t->tamano);
    int ancho_tamano = fuente_texto_ancho(etiqueta) + TEXTO_MENU_PADDING * 2;
    if (ancho_tamano < 40) ancho_tamano = 40;
    int total = TEXTO_MENU_PADDING * 2 + ancho_fuente + TEXTO_MENU_HUECO +
                TEXTO_MENU_BOTON + TEXTO_MENU_HUECO + ancho_tamano + TEXTO_MENU_HUECO +
                TEXTO_MENU_BOTON + TEXTO_MENU_HUECO + TEXTO_MENU_COLOR_LADO;
    int x_max = zona_derecha(tr) - total;
    if (x_max < tr.x) x_max = tr.x;
    int x = sx0 + ((sx1 - sx0) - total) / 2;
    if (x < tr.x) x = tr.x;
    if (x > x_max) x = x_max;
    int y_arriba = sy0 - TEXTO_MENU_SEPARACION_CAJA - TEXTO_MENU_ALTO;
    int y;
    if (y_arriba >= tr.y) y = y_arriba;
    else y = sy1 + TEXTO_MENU_SEPARACION_CAJA;
    int y_max = zona_abajo(tr) - TEXTO_MENU_ALTO;
    if (y_max < tr.y) y_max = tr.y;
    if (y < tr.y) y = tr.y;
    if (y > y_max) y = y_max;

    memset(m, 0, sizeof(*m));
    m->zona = zona_hacer(x, y, total, TEXTO_MENU_ALTO);
    int cursor = x + TEXTO_MENU_PADDING;
    m->fuente = zona_hacer(cursor, y, ancho_fuente, TEXTO_MENU_ALTO);
    cursor += ancho_fuente + TEXTO_MENU_HUECO;
    int y_boton = y + (TEXTO_MENU_ALTO - TEXTO_MENU_BOTON) / 2;
    m->menos = zona_hacer(cursor, y_boton, TEXTO_MENU_BOTON, TEXTO_MENU_BOTON);
    cursor += TEXTO_MENU_BOTON + TEXTO_MENU_HUECO;
    m->tamano = zona_hacer(cursor, y, ancho_tamano, TEXTO_MENU_ALTO);
    cursor += ancho_tamano + TEXTO_MENU_HUECO;
    m->mas = zona_hacer(cursor, y_boton, TEXTO_MENU_BOTON, TEXTO_MENU_BOTON);
    cursor += TEXTO_MENU_BOTON + TEXTO_MENU_HUECO;
    m->color = zona_hacer(cursor, y + (TEXTO_MENU_ALTO - TEXTO_MENU_COLOR_LADO) / 2, TEXTO_MENU_COLOR_LADO, TEXTO_MENU_COLOR_LADO);

    m->num_desplegable = 0;
    if (g.texto_fuentes_abierto) {
        int ancho_lista = 0;
        int i;
        for (i = 0; i < TEXTO_FUENTES_TOTAL; i++) {
            int w = fuente_texto_ancho(nombres[i]);
            if (w > ancho_lista) ancho_lista = w;
        }
        ancho_lista += TEXTO_MENU_PADDING * 2;
        int alto_lista = TEXTO_MENU_ITEM_ALTO * TEXTO_FUENTES_TOTAL + 2;
        int lista_x = m->fuente.x;
        if (lista_x < tr.x) lista_x = tr.x;
        if (lista_x > zona_derecha(tr) - ancho_lista) lista_x = zona_derecha(tr) - ancho_lista;
        if (lista_x < tr.x) lista_x = tr.x;
        int lista_y = zona_abajo(m->zona) + 2;
        if (lista_y + alto_lista > zona_abajo(tr)) {
            lista_y = m->zona.y - 2 - alto_lista;
            if (lista_y < tr.y) lista_y = tr.y;
        }
        for (i = 0; i < TEXTO_FUENTES_TOTAL; i++) {
            m->desplegable[i] = zona_hacer(lista_x + 1, lista_y + 1 + i * TEXTO_MENU_ITEM_ALTO, ancho_lista - 2, TEXTO_MENU_ITEM_ALTO);
            m->num_desplegable++;
        }
    }
    return 1;
}

/* Public API */

void texto_iniciar(void) {
    memset(&g.texto, 0, sizeof(g.texto));
    g.texto_activo = 0;
    g.texto_fuentes_abierto = 0;
    g.texto_hover_parte = -1;
    g.texto_hover_fuente = -1;
    g.texto_mango_hover = -1;
    texto_preview_valida = 0;
    memset(&texto_preview, 0, sizeof(texto_preview));
}

void texto_terminar(void) {
    if (texto_preview_valida) {
        doc_liberar(&texto_preview);
        texto_preview_valida = 0;
    }
    texto_cache_liberar(&g.texto.cache);
}

void texto_limpiar(void) {
    if (texto_preview_valida) {
        doc_liberar(&texto_preview);
        texto_preview_valida = 0;
    }
    texto_cache_liberar(&g.texto.cache);
    memset(&g.texto, 0, sizeof(g.texto));
    g.texto_activo = 0;
    g.texto_fuentes_abierto = 0;
    g.texto_hover_parte = -1;
    g.texto_hover_fuente = -1;
    g.texto_mango_hover = -1;
}

int texto_editing(void) {
    return g.texto_activo && !g.texto.inicio_activo;
}

void texto_empezar(int x, int y) {
    if (g.texto_activo) texto_confirmar();
    memset(&g.texto, 0, sizeof(g.texto));
    g.texto.x0 = x; g.texto.y0 = y; g.texto.x1 = x; g.texto.y1 = y;
    g.texto.inicio_activo = 1;
    g.texto.inicio_x = x; g.texto.inicio_y = y;
    g.texto.fuente = TEXTO_FUENTE_POR_DEFECTO;
    g.texto.tamano = TEXTO_TAMANO_POR_DEFECTO;
    g.texto.color = g.frente;
    g.texto.cursor = 0;
    g.texto.largo = 0;
    g.texto.contenido[0] = 0;
    g.texto_activo = 1;
    g.texto_fuentes_abierto = 0;
    g.texto_hover_parte = -1;
    g.texto_hover_fuente = -1;
    g.texto_mango_hover = -1;
}

void texto_confirmar(void) {
    int writes, i;
    if (!g.texto_activo) return;
    if (g.texto.inicio_activo) {
        texto_limpiar();
        return;
    }
    if (g.modo_tiles) {
        /* La caja puede cubrir varios tiles: se comprueba la escritura en
         * todos antes de guardar una unica instantanea y se estampa en cada
         * uno con el desplazamiento de su celda. */
        TileCoord activo = tiles_modo_activo();
        int w = g.doc.ancho > 0 ? g.doc.ancho : 1;
        int h = g.doc.alto > 0 ? g.doc.alto : 1;
        writes = texto_hay_escritura_en(&g.doc, 0, 0);
        for (i = 0; i < g.num_tiles_modo && !writes; i++) {
            TileDoc *td = &g.tiles_modo[i];
            if (!td->ocupado) continue;
            if (texto_hay_escritura_en(&td->doc,
                                       (td->cx - activo.cx) * w,
                                       (td->cy - activo.cy) * h))
                writes = 1;
        }
        if (writes) {
            pila_guardar(&g.deshacer, &g.doc);
            pila_limpiar(&g.rehacer);
        }
        texto_estampar_en(&g.doc, 0, 0);
        for (i = 0; i < g.num_tiles_modo; i++) {
            TileDoc *td = &g.tiles_modo[i];
            if (!td->ocupado) continue;
            texto_estampar_en(&td->doc,
                              (td->cx - activo.cx) * w,
                              (td->cy - activo.cy) * h);
        }
        texto_limpiar();
        return;
    }
    writes = texto_hay_escritura_en(&g.doc, 0, 0);
    if (writes) {
        pila_guardar(&g.deshacer, &g.doc);
        pila_limpiar(&g.rehacer);
    }
    texto_estampar_en(&g.doc, 0, 0);
    texto_limpiar();
}

void texto_cancelar(void) {
    if (!g.texto_activo) return;
    texto_limpiar();
    app_invalidar();
}

void texto_invalidar(void) {
    if (!g.texto_activo) return;
    g.texto.revision++;
    texto_cache_liberar(&g.texto.cache);
    g.texto.cache_valida = 0;
    g.texto.cache_clave_valida = 0;
}

void texto_arrastrar(int x, int y) {
    Texto *t = &g.texto;
    int min_x, min_y, max_x, max_y;
    if (!t->inicio_activo) return;
    texto_limites_globales(&min_x, &min_y, &max_x, &max_y);
    if (x < min_x) x = min_x;
    if (x > max_x) x = max_x;
    if (y < min_y) y = min_y;
    if (y > max_y) y = max_y;
    int x0 = t->inicio_x, y0 = t->inicio_y;
    t->x0 = x0 < x ? x0 : x;
    t->y0 = y0 < y ? y0 : y;
    t->x1 = x0 > x ? x0 : x;
    t->y1 = y0 > y ? y0 : y;
}

void texto_soltar(int x, int y, int boton) {
    Texto *t = &g.texto;
    if (!g.texto_activo) return;
    if (t->inicio_activo) {
        int ix = t->inicio_x, iy = t->inicio_y;
        int min_x, min_y, max_x, max_y;
        texto_pixel_local(x, y, &ix, &iy);
        texto_limites_globales(&min_x, &min_y, &max_x, &max_y);
        if (ix < min_x) ix = min_x;
        if (ix > max_x) ix = max_x;
        if (iy < min_y) iy = min_y;
        if (iy > max_y) iy = max_y;
        /* normalize using last pointer */
        int x0 = t->inicio_x, y0 = t->inicio_y;
        int x1 = ix, y1 = iy;
        if (x1 < x0) { int tmp = x0; x0 = x1; x1 = tmp; }
        if (y1 < y0) { int tmp = y0; y0 = y1; y1 = tmp; }
        t->x0 = x0; t->y0 = y0; t->x1 = x1; t->y1 = y1;
        int caja_ancho = t->x1 - t->x0 + 1;
        int caja_alto = t->y1 - t->y0 + 1;
        if (caja_ancho < 8 || caja_alto < 8) {
            int alto_linea = t->cache_valida ? t->cache.alto_linea : (t->tamano > 1 ? t->tamano : 1);
            int ancho_deseado = TEXTO_ANCHO_MIN;
            if (ancho_deseado > g.doc.ancho - t->inicio_x) ancho_deseado = g.doc.ancho - t->inicio_x;
            if (ancho_deseado < 8) ancho_deseado = 8;
            int alto_deseado = (TEXTO_MARGEN * 2 + alto_linea);
            int min_alt = TEXTO_ALTO_MIN;
            if (min_alt > g.doc.alto - t->inicio_y) min_alt = g.doc.alto - t->inicio_y;
            if (min_alt < 8) min_alt = 8;
            if (alto_deseado < min_alt) alto_deseado = min_alt;
            t->x0 = t->inicio_x;
            t->y0 = t->inicio_y;
            t->x1 = t->inicio_x + ancho_deseado - 1;
            if (t->x1 > g.doc.ancho - 1) t->x1 = g.doc.ancho - 1;
            if (t->x1 < t->x0) t->x1 = t->x0;
            t->y1 = t->inicio_y + alto_deseado - 1;
            if (t->y1 > g.doc.alto - 1) t->y1 = g.doc.alto - 1;
            if (t->y1 < t->y0) t->y1 = t->y0;
        }
        t->inicio_activo = 0;
        t->inicio_x = t->inicio_y = 0;
        texto_cache_liberar(&t->cache);
        t->cache_valida = 0;
        t->cache_clave_valida = 0;
        app_invalidar();
        return;
    }
    if (t->redimensionando) {
        t->redimensionando = 0;
        app_invalidar();
        return;
    }
}

int texto_tecla(int vk, int ctrl, int shift) {
    (void)ctrl; (void)shift;
    if (!g.texto_activo || g.texto.inicio_activo) return 0;
    Texto *t = &g.texto;
    texto_cache_actualizar();
    if (vk == VK_ESCAPE) {
        if (g.texto_fuentes_abierto) { g.texto_fuentes_abierto = 0; app_invalidar(); return 1; }
        texto_cancelar();
        return 1;
    }
    if (vk == VK_RETURN) { texto_insertar_char('\n'); app_invalidar(); return 1; }
    if (vk == VK_BACK) { texto_retroceso(); app_invalidar(); return 1; }
    if (vk == VK_DELETE) { texto_borrar(); app_invalidar(); return 1; }
    if (vk == VK_LEFT) { if (t->cursor > 0) t->cursor--; app_invalidar(); return 1; }
    if (vk == VK_RIGHT) { if (t->cursor < t->largo) t->cursor++; app_invalidar(); return 1; }
    if (vk == VK_UP) { texto_mover_vertical(-1); app_invalidar(); return 1; }
    if (vk == VK_DOWN) { texto_mover_vertical(1); app_invalidar(); return 1; }
    if (vk == VK_HOME) { texto_ir_inicio_linea(); app_invalidar(); return 1; }
    if (vk == VK_END) { texto_ir_fin_linea(); app_invalidar(); return 1; }
    return 1;
}

int texto_caracter(int ch) {
    if (!g.texto_activo || g.texto.inicio_activo) return 0;
    if (ch == 8) { texto_retroceso(); app_invalidar(); return 1; }
    if (ch == 13) { texto_insertar_char('\n'); app_invalidar(); return 1; }
    if (ch >= 32) {
        if (g.texto.largo < MAX_TEXTO - 1) {
            texto_insertar_char((u16)ch);
            app_invalidar();
        }
        return 1;
    }
    return 1;
}

void texto_mover(int x, int y, int botones) {
    if (!g.texto_activo) {
        g.texto_hover_parte = -1;
        g.texto_hover_fuente = -1;
        g.texto_mango_hover = -1;
        return;
    }
    Texto *t = &g.texto;
    if (t->inicio_activo) {
        if (botones & 1) {
            int ix, iy;
            if (texto_pixel_local(x, y, &ix, &iy)) texto_arrastrar(ix, iy);
        }
        app_invalidar();
        return;
    }
    if (t->redimensionando) {
        if (botones & 1) {
            int ix, iy;
            if (texto_pixel_local(x, y, &ix, &iy)) {
                /* redimensionar */
                RedimensionTexto *r = &t->redimension;
                int izq, der, arr, aba;
                int min_x, min_y, max_x, max_y;
                mango_mueve_local(r->mango, &izq, &der, &arr, &aba);
                texto_limites_globales(&min_x, &min_y, &max_x, &max_y);
                int ancho = max_x - min_x + 1;
                int alto = max_y - min_y + 1;
                int xx = ix - r->desfase_x;
                int yy = iy - r->desfase_y;
                int bx0 = r->x0, by0 = r->y0, bx1 = r->x1, by1 = r->y1;
                int ancho_min = TEXTO_MARGEN * 2 + 4;
                int alto_min = t->cache_valida ? (TEXTO_MARGEN * 2 + t->cache.alto) : (TEXTO_MARGEN * 2 + (t->tamano > 1 ? t->tamano : 1));
                if (ancho_min > ancho) ancho_min = ancho;
                if (alto_min > alto) alto_min = alto;
                int nx0 = bx0, ny0 = by0, nx1 = bx1, ny1 = by1;
                if (izq) {
                    int limite = bx1 - ancho_min + 1;
                    if (limite < min_x) limite = min_x;
                    if (limite > max_x) limite = max_x;
                    nx0 = xx;
                    if (nx0 < min_x) nx0 = min_x;
                    if (nx0 > limite) nx0 = limite;
                }
                if (der) {
                    int minimo = bx0 + ancho_min - 1;
                    if (minimo < min_x) minimo = min_x;
                    if (minimo > max_x) minimo = max_x;
                    nx1 = xx;
                    if (nx1 < minimo) nx1 = minimo;
                    if (nx1 > max_x) nx1 = max_x;
                }
                if (arr) {
                    int limite = by1 - alto_min + 1;
                    if (limite < min_y) limite = min_y;
                    if (limite > max_y) limite = max_y;
                    ny0 = yy;
                    if (ny0 < min_y) ny0 = min_y;
                    if (ny0 > limite) ny0 = limite;
                }
                if (aba) {
                    int minimo = by0 + alto_min - 1;
                    if (minimo < min_y) minimo = min_y;
                    if (minimo > max_y) minimo = max_y;
                    ny1 = yy;
                    if (ny1 < minimo) ny1 = minimo;
                    if (ny1 > max_y) ny1 = max_y;
                }
                t->x0 = nx0; t->y0 = ny0; t->x1 = nx1; t->y1 = ny1;
                t->cache_clave_valida = 0;
                texto_cache_actualizar();
            }
        } else {
            t->redimensionando = 0;
        }
        app_invalidar();
        return;
    }
    /* hover */
    MenuTexto m;
    int hp = -1, hf = -1, hm = -1;
    if (texto_menu_zona(&m)) {
        if (texto_menu_contiene(&m, x, y)) {
            hp = texto_menu_parte_en(&m, x, y);
            hf = texto_menu_fuente_en(&m, x, y);
        }
    }
    hm = texto_mango_en(x, y);
    g.texto_hover_parte = hp;
    g.texto_hover_fuente = hf;
    g.texto_mango_hover = hm;
}

int texto_pulsar(int x, int y, int boton) {
    if (!g.texto_activo) return 0;
    if (g.texto.inicio_activo) return 0;
    if (boton != 0) return 0;
    texto_cache_actualizar();
    MenuTexto m;
    int has_menu = texto_menu_zona(&m);
    int en_menu = has_menu && texto_menu_contiene(&m, x, y);
    int parte = has_menu ? texto_menu_parte_en(&m, x, y) : -1;
    int fuente_idx = has_menu ? texto_menu_fuente_en(&m, x, y) : -1;
    int mango = texto_mango_en(x, y);

    int consumed = 0;
    if (fuente_idx >= 0) {
        g.texto.fuente = fuente_idx;
        texto_invalidar();
        g.texto_fuentes_abierto = 0;
        consumed = 1;
    } else if (parte >= 0) {
        switch (parte) {
            case 0: g.texto_fuentes_abierto = !g.texto_fuentes_abierto; break;
            case 1: g.texto.tamano = paso_tamano(g.texto.tamano, -1); texto_invalidar(); g.texto_fuentes_abierto = 0; break;
            case 2: g.texto.tamano = paso_tamano(g.texto.tamano, 1); texto_invalidar(); g.texto_fuentes_abierto = 0; break;
            case 3: {
                u32 c = g.texto.color;
                if (dialogo_elegir_color_valor(c, &c)) g.texto.color = c;
                g.texto_fuentes_abierto = 0;
                break;
            }
        }
        consumed = 1;
    } else if (en_menu) {
        g.texto_fuentes_abierto = 0;
        consumed = 1;
    } else if (mango >= 0) {
        int ix, iy;
        if (texto_pixel_local(x, y, &ix, &iy)) {
            /* empezar redimension */
            Texto *t = &g.texto;
            int x0, y0, x1, y1;
            texto_caja_logica(&x0, &y0, &x1, &y1);
            int izq, der, arr, aba;
            mango_mueve_local(mango, &izq, &der, &arr, &aba);
            int desf_x = izq ? ix - x0 : (der ? ix - x1 : 0);
            int desf_y = arr ? iy - y0 : (aba ? iy - y1 : 0);
            t->redimension.mango = mango;
            t->redimension.desfase_x = desf_x;
            t->redimension.desfase_y = desf_y;
            t->redimension.x0 = x0; t->redimension.y0 = y0;
            t->redimension.x1 = x1; t->redimension.y1 = y1;
            t->redimensionando = 1;
            t->x0 = x0; t->y0 = y0; t->x1 = x1; t->y1 = y1;
            t->cache_clave_valida = 0;
            g.texto_fuentes_abierto = 0;
            consumed = 1;
        }
    } else {
        int ix, iy;
        if (texto_pixel_local(x, y, &ix, &iy)) {
            int x0, y0, x1, y1;
            texto_caja_logica(&x0, &y0, &x1, &y1);
            if (ix >= x0 && ix <= x1 && iy >= y0 && iy <= y1) {
                g.texto_fuentes_abierto = 0;
                texto_cache_actualizar();
                g.texto.cursor = texto_cursor_en(ix, iy);
                consumed = 1;
            }
        }
    }

    if (consumed) {
        app_invalidar();
        return 1;
    }
    /* confirm and let routing continue */
    g.texto_fuentes_abierto = 0;
    texto_confirmar();
    return 0;
}

void texto_dibujar_barra(void) {
    if (!g.texto_activo || g.texto.inicio_activo) return;
    MenuTexto m;
    if (!texto_menu_zona(&m)) return;
    /* hover values already in g */
    /* background */
    dibujar_rect(m.zona.x, m.zona.y, zona_derecha(m.zona), zona_abajo(m.zona), MENU_FONDO);
    dibujar_marco(m.zona.x, m.zona.y, zona_derecha(m.zona), zona_abajo(m.zona), MENU_BORDE);

    /* fuente hover */
    if (g.texto_hover_parte == 0) {
        dibujar_rect(m.fuente.x, m.fuente.y + 1, zona_derecha(m.fuente), zona_abajo(m.fuente) - 1, MENU_HOVER);
    }
    static const char *nombres[TEXTO_FUENTES_TOTAL] = {"Arial","Times New Roman","Courier New","Segoe UI","Georgia","Verdana"};
    const char *nombre = nombres[g.texto.fuente >= 0 && g.texto.fuente < TEXTO_FUENTES_TOTAL ? g.texto.fuente : 0];
    int texto_y = m.zona.y + (TEXTO_MENU_ALTO - 7) / 2;
    fuente_dibujar(m.fuente.x + TEXTO_MENU_PADDING, texto_y, nombre, MENU_TEXTO);
    /* arrow down */
    {
        int ax = zona_derecha(m.fuente) - TEXTO_MENU_PADDING - 5;
        int ay = m.zona.y + TEXTO_MENU_ALTO / 2 - 1;
        int fila;
        for (fila = 0; fila < 3; fila++)
            dibujar_rect(ax + fila, ay + fila, ax + 5 - fila, ay + fila + 1, MENU_TEXTO);
    }

    /* menos */
    if (g.texto_hover_parte == 1) {
        dibujar_rect(m.menos.x, m.menos.y, zona_derecha(m.menos), zona_abajo(m.menos), MENU_HOVER);
    }
    {
        int medio = m.menos.y + TEXTO_MENU_BOTON / 2;
        dibujar_rect(m.menos.x + 5, medio, zona_derecha(m.menos) - 5, medio + 1, MENU_TEXTO);
    }

    /* tamano label */
    char etiqueta[32];
    formatear(etiqueta, sizeof(etiqueta), "%d px", g.texto.tamano);
    fuente_dibujar(m.tamano.x + (m.tamano.ancho - fuente_texto_ancho(etiqueta)) / 2, texto_y, etiqueta, MENU_TEXTO);

    /* mas */
    if (g.texto_hover_parte == 2) {
        dibujar_rect(m.mas.x, m.mas.y, zona_derecha(m.mas), zona_abajo(m.mas), MENU_HOVER);
    }
    {
        int medio = m.mas.y + TEXTO_MENU_BOTON / 2;
        dibujar_rect(m.mas.x + 5, medio, zona_derecha(m.mas) - 5, medio + 1, MENU_TEXTO);
        int centro = m.mas.x + TEXTO_MENU_BOTON / 2;
        dibujar_rect(centro, m.mas.y + 5, centro + 1, zona_abajo(m.mas) - 5, MENU_TEXTO);
    }

    /* color */
    if (g.texto_hover_parte == 3) {
        dibujar_rect(m.color.x - 2, m.color.y - 2, zona_derecha(m.color) + 2, zona_abajo(m.color) + 2, MENU_HOVER);
    }
    dibujar_rect(m.color.x, m.color.y, zona_derecha(m.color), zona_abajo(m.color), g.texto.color);
    dibujar_marco(m.color.x, m.color.y, zona_derecha(m.color), zona_abajo(m.color), MENU_BORDE);

    /* dropdown */
    if (g.texto_fuentes_abierto && m.num_desplegable > 0) {
        int lista_x = m.desplegable[0].x - 1;
        int lista_y = m.desplegable[0].y - 1;
        int lista_w = m.desplegable[0].ancho + 2;
        int lista_h = TEXTO_MENU_ITEM_ALTO * m.num_desplegable + 2;
        dibujar_rect(lista_x, lista_y, lista_x + lista_w, lista_y + lista_h, MENU_FONDO);
        dibujar_marco(lista_x, lista_y, lista_x + lista_w, lista_y + lista_h, MENU_BORDE);
        int i;
        for (i = 0; i < m.num_desplegable; i++) {
            Zona fila = m.desplegable[i];
            int elegida = (i == g.texto.fuente);
            int hover = (g.texto_hover_fuente == i);
            if (hover) {
                dibujar_rect(fila.x, fila.y, zona_derecha(fila), zona_abajo(fila),
                             elegida ? BOTON_SELECCION : MENU_HOVER);
            } else if (elegida) {
                dibujar_rect(fila.x, fila.y, zona_derecha(fila), zona_abajo(fila), BOTON_SELECCION);
            }
            u32 color_texto = elegida ? 0x00FFFFFFu : MENU_TEXTO;
            fuente_dibujar(fila.x + TEXTO_MENU_PADDING, fila.y + (TEXTO_MENU_ITEM_ALTO - 7) / 2, nombres[i], color_texto);
        }
    }
}

void texto_dibujar_caja(void) {
    if (!g.texto_activo) return;
    Texto *t = &g.texto;
    int x0, y0, x1, y1;
    if (t->inicio_activo) {
        x0 = t->x0; y0 = t->y0; x1 = t->x1; y1 = t->y1;
    } else {
        texto_caja_logica(&x0, &y0, &x1, &y1);
    }
    int sx0, sy0, sx1, sy1;
    pintura_pantalla_caja(x0, y0, x1, y1, &sx0, &sy0, &sx1, &sy1);
    pintura_dibujar_seleccion_caja(sx0, sy0, sx1, sy1, (GetTickCount() / 90) % 8);
    if (!t->inicio_activo) {
        Lienzo cz = { sx0, sy0, sx1, sy1 };
        pintura_dibujar_mangos_caja(cz, g.texto_mango_hover);
    }
    if (!t->inicio_activo && ((GetTickCount() / 500) % 2 == 0)) {
        int cx, cy0, cy1;
        if (texto_caret_logico(&cx, &cy0, &cy1)) {
            int px0, py0, px1, py1;
            int yy1 = cy1 - 1;
            if (yy1 < cy0) yy1 = cy0;
            pintura_pantalla_caja(cx, cy0, cx, yy1, &px0, &py0, &px1, &py1);
            int grosor = px1 - px0;
            if (grosor < 1) grosor = 1;
            dibujar_rect(px0, py0, px0 + grosor, py1, 0x00000000u);
        }
    }
}

/* Completa la previsualizacion del texto en los tiles vecinos. El tile activo
 * ya la recibe estampada en `texto_documento_visible`; aqui se pintan las
 * celdas cubiertas por la caja, cada pixel recortado al area de su tile. */
void texto_dibujar_preview_tiles(TilesLayout *layout, const TileCoord *coords,
                                 int ncoords, Zona clip) {
    Texto *t = &g.texto;
    TextoRaster *r;
    TileCoord activo;
    int ox, oy, doc_w, doc_h, i;
    if (!g.modo_tiles || !g.texto_activo || t->inicio_activo || t->largo == 0)
        return;
    if (!layout || layout->tile_w <= 0 || layout->tile_h <= 0) return;
    texto_cache_actualizar();
    if (!t->cache_valida) return;
    r = &t->cache;
    ox = t->x0 + TEXTO_MARGEN;
    oy = t->y0 + TEXTO_MARGEN;
    activo = tiles_modo_activo();
    doc_w = g.doc.ancho > 0 ? g.doc.ancho : 1;
    doc_h = g.doc.alto > 0 ? g.doc.alto : 1;

    for (i = 0; i < ncoords; i++) {
        TileCoord c = coords[i];
        Zona z;
        int local_x, local_y;
        int sx0, sy0, sx1, sy1, x, y;
        if (c.cx == activo.cx && c.cy == activo.cy) continue;
        local_x = ox - (c.cx - activo.cx) * doc_w;
        local_y = oy - (c.cy - activo.cy) * doc_h;
        if (local_x >= doc_w || local_y >= doc_h) continue;
        if (local_x + r->ancho <= 0 || local_y + r->alto <= 0) continue;

        z = zona_hacer(layout->origen_x + (c.cx - layout->min_cx) * layout->paso_x,
                       layout->origen_y + (c.cy - layout->min_cy) * layout->paso_y,
                       layout->tile_w, layout->tile_h);
        sx0 = z.x > clip.x ? z.x : clip.x;
        sy0 = z.y > clip.y ? z.y : clip.y;
        sx1 = zona_derecha(z) < zona_derecha(clip) ? zona_derecha(z) : zona_derecha(clip);
        sy1 = zona_abajo(z) < zona_abajo(clip) ? zona_abajo(z) : zona_abajo(clip);
        if (sx0 >= sx1 || sy0 >= sy1) continue;

        /* Solo se recorre la parte de la caja que cae en este tile. */
        {
            int bx0 = z.x + (int)(((long long)local_x * z.ancho) / doc_w) - 2;
            int by0 = z.y + (int)(((long long)local_y * z.alto) / doc_h) - 2;
            int bx1 = z.x + (int)(((long long)(local_x + r->ancho) * z.ancho) / doc_w) + 2;
            int by1 = z.y + (int)(((long long)(local_y + r->alto) * z.alto) / doc_h) + 2;
            if (bx0 > sx0) sx0 = bx0;
            if (by0 > sy0) sy0 = by0;
            if (bx1 < sx1) sx1 = bx1;
            if (by1 < sy1) sy1 = by1;
            if (sx0 >= sx1 || sy0 >= sy1) continue;
        }

        for (y = sy0; y < sy1; y++) {
            int ly = (int)(((long long)(y - z.y) * doc_h) / z.alto);
            int ry = ly - local_y;
            if (ry < 0 || ry >= r->alto) continue;
            for (x = sx0; x < sx1; x++) {
                int lx = (int)(((long long)(x - z.x) * doc_w) / z.ancho);
                int rx = lx - local_x;
                u8 a;
                u32 src;
                if (rx < 0 || rx >= r->ancho) continue;
                a = r->cobertura[ry * r->ancho + rx];
                if (a == 0) continue;
                src = (t->color & 0x00FFFFFFu) | ((u32)a << 24);
                dibujar_pixel(x, y, color_mezclar(dibujar_leer(x, y), src, 255));
            }
        }
    }
}

Documento *texto_documento_visible(void) {
    if (!g.texto_activo || g.texto.inicio_activo || g.texto.largo == 0) {
        if (texto_preview_valida) {
            doc_liberar(&texto_preview);
            texto_preview_valida = 0;
        }
        return &g.doc;
    }
    if (texto_preview_valida) {
        doc_liberar(&texto_preview);
        texto_preview_valida = 0;
    }
    doc_copiar_en(&texto_preview, &g.doc);
    texto_cache_actualizar();
    texto_estampar_en(&texto_preview, 0, 0);
    texto_preview_valida = 1;
    return &texto_preview;
}

int texto_en_menu(int x, int y) {
    if (!g.texto_activo || g.texto.inicio_activo) return 0;
    MenuTexto m;
    if (!texto_menu_zona(&m)) return 0;
    return texto_menu_contiene(&m, x, y);
}

int texto_en_caja(int x, int y) {
    if (!g.texto_activo) return 0;
    int x0, y0, x1, y1;
    if (g.texto.inicio_activo) {
        x0 = g.texto.x0; y0 = g.texto.y0; x1 = g.texto.x1; y1 = g.texto.y1;
    } else {
        texto_caja_logica(&x0, &y0, &x1, &y1);
    }
    int ix, iy;
    if (!texto_pixel_local(x, y, &ix, &iy)) return 0;
    return (ix >= x0 && ix <= x1 && iy >= y0 && iy <= y1);
}

int texto_mango_en(int x, int y) {
    if (!g.texto_activo || g.texto.inicio_activo) return -1;
    int x0, y0, x1, y1;
    texto_caja_logica(&x0, &y0, &x1, &y1);
    int sx0, sy0, sx1, sy1;
    pintura_pantalla_caja(x0, y0, x1, y1, &sx0, &sy0, &sx1, &sy1);
    Lienzo cz = { sx0, sy0, sx1, sy1 };
    return pintura_mango_en_caja(cz, x, y);
}

/* provide mango_mueve access (already static in pintura.c, not accessible) */
/* We re-implement locally */
static void mango_mueve_local(int mango, int *izq, int *der, int *arr, int *aba) {
    *izq = *der = *arr = *aba = 0;
    switch (mango) {
        case 0: *izq = 1; *arr = 1; break;
        case 1: *arr = 1; break;
        case 2: *der = 1; *arr = 1; break;
        case 3: *der = 1; break;
        case 4: *der = 1; *aba = 1; break;
        case 5: *aba = 1; break;
        case 6: *izq = 1; *aba = 1; break;
        case 7: *izq = 1; break;
        default: break;
    }
}
