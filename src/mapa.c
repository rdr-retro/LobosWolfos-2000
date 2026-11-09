/* ==========================================================================
 * Editor de mapas: rejilla de celdas con tiles colocados y formato .mav.
 * ========================================================================== */
#include "app.h"

static int mapa_arrastrando = 0;
static int mapa_ult_x = 0, mapa_ult_y = 0;
static int mapa_pan = 0;
static int mapa_celda_pantalla = 0;

/* ------------------------------------------------------------------ */
/* Inicio / fin                                                        */
/* ------------------------------------------------------------------ */
void mapa_iniciar(void) {
    memset(&g.mapa, 0, sizeof(g.mapa));
    g.mapa.lado = MAPA_LADO_INICIAL;
    mapa_arrastrando = 0;
    mapa_pan = 0;
}

static void mapa_vaciar(void) {
    int i;
    for (i = 0; i < g.mapa.num; i++) {
        if (g.mapa.celdas[i].pixeles) free(g.mapa.celdas[i].pixeles);
    }
    g.mapa.num = 0;
}

void mapa_liberar(void) {
    mapa_vaciar();
}

void mapa_reiniciar(void) {
    mapa_vaciar();
    g.mapa.lado = MAPA_LADO_INICIAL;
    g.mapa.scroll_x = 0;
    g.mapa.scroll_y = 0;
    g.mapa.tile_sel = -1;
    g.mapa_tile_sel = -1;
}

/* ------------------------------------------------------------------ */
/* Coordenadas                                                         */
/* ------------------------------------------------------------------ */
static int mapa_celda(void) {
    int c = lado_con_zoom(g.mapa.lado, zoom_porcentaje(g.zoom_idx));
    if (c < 4) c = 4;
    if (c > 200) c = 200;
    return c;
}

static void mapa_origen(int *ox, int *oy) {
    Zona tr = zona_trabajo();
    int lado = mapa_celda();
    int columnas = tr.ancho / lado + 2;
    int filas = tr.alto / lado + 2;
    *ox = tr.x + tr.ancho / 2 - columnas * lado / 2 + g.mapa.scroll_x * lado;
    *oy = tr.y + tr.alto / 2 - filas * lado / 2 + g.mapa.scroll_y * lado;
}

void mapa_coord(int x, int y, int *cx, int *cy) {
    int ox, oy, lado;
    mapa_origen(&ox, &oy);
    lado = mapa_celda();
    *cx = (x - ox) / lado;
    *cy = (y - oy) / lado;
}

/* ------------------------------------------------------------------ */
/* Colocar / borrar                                                    */
/* ------------------------------------------------------------------ */
static TileMapa *mapa_buscar(int cx, int cy) {
    int i;
    for (i = 0; i < g.mapa.num; i++) {
        if (g.mapa.celdas[i].x == cx && g.mapa.celdas[i].y == cy) return &g.mapa.celdas[i];
    }
    return NULL;
}

void mapa_colocar(int celda_x, int celda_y, int tile) {
    TileMapa *destino;
    TileGuardado *origen;
    if (tile < 0 || tile >= g.num_tiles) return;
    origen = &g.tiles[tile];
    destino = mapa_buscar(celda_x, celda_y);
    if (!destino) {
        if (g.mapa.num >= MAX_CELDAS) return;
        destino = &g.mapa.celdas[g.mapa.num++];
        memset(destino, 0, sizeof(*destino));
        destino->x = celda_x;
        destino->y = celda_y;
    } else if (destino->pixeles) {
        free(destino->pixeles);
        destino->pixeles = NULL;
    }
    destino->ancho = origen->ancho;
    destino->alto = origen->alto;
    destino->pixeles = (u32 *)malloc((size_t)origen->ancho * origen->alto * sizeof(u32));
    if (destino->pixeles) copiar_pixeles(destino->pixeles, origen->pixeles, origen->ancho * origen->alto);
    recortar_texto(destino->imagen, origen->nombre, sizeof(destino->imagen));
}

void mapa_borrar(int celda_x, int celda_y) {
    int i;
    for (i = 0; i < g.mapa.num; i++) {
        if (g.mapa.celdas[i].x == celda_x && g.mapa.celdas[i].y == celda_y) {
            if (g.mapa.celdas[i].pixeles) free(g.mapa.celdas[i].pixeles);
            g.mapa.celdas[i] = g.mapa.celdas[g.mapa.num - 1];
            g.mapa.num--;
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Dibujo                                                              */
/* ------------------------------------------------------------------ */
void mapa_dibujar(void) {
    Zona tr = zona_trabajo();
    int ox, oy, lado;
    int cx, cy;
    int minx, maxx, miny, maxy;
    int x, y;

    dibujar_rect_z(tr, MAPA_FONDO);

    mapa_origen(&ox, &oy);
    lado = mapa_celda();

    minx = (tr.x - ox) / lado - 1;
    maxx = (zona_derecha(tr) - ox) / lado + 1;
    miny = (tr.y - oy) / lado - 1;
    maxy = (zona_abajo(tr) - oy) / lado + 1;

    /* Celdas. */
    for (cy = miny; cy <= maxy; cy++) {
        for (cx = minx; cx <= maxx; cx++) {
            int px = ox + cx * lado;
            int py = oy + cy * lado;
            dibujar_rect(px + 1, py + 1, px + lado - 1, py + lado - 1, MAPA_CELDA);
        }
    }

    /* Tiles colocados. */
    for (cx = 0; cx < g.mapa.num; cx++) {
        TileMapa *t = &g.mapa.celdas[cx];
        int px = ox + t->x * lado;
        int py = oy + t->y * lado;
        if (t->pixeles) {
            int ix, iy;
            for (iy = 0; iy < lado; iy++) {
                int sy = iy * t->alto / lado;
                for (ix = 0; ix < lado; ix++) {
                    int sx = ix * t->ancho / lado;
                    u32 c = t->pixeles[(size_t)sy * t->ancho + sx];
                    if ((c >> 24) == 0) continue;
                    dibujar_pixel(px + ix, py + iy, 0xFF000000u | (c & 0x00FFFFFFu));
                }
            }
        }
        dibujar_marco(px, py, px + lado, py + lado, MAPA_REJILLA);
    }

    /* Ejes. */
    dibujar_rect(ox - 1, tr.y, ox + 1, zona_abajo(tr), MAPA_EJE);
    dibujar_rect(tr.x, oy - 1, zona_derecha(tr), oy + 1, MAPA_EJE);

    /* Celda bajo el cursor. */
    if (zona_contiene(tr, g.raton_x, g.raton_y)) {
        int hx = (g.raton_x - ox) / lado;
        int hy = (g.raton_y - oy) / lado;
        dibujar_marco(ox + hx * lado, oy + hy * lado, ox + hx * lado + lado, oy + hy * lado + lado, MAPA_HOVER);
    }

    /* Panel de estado del mapa, arriba a la izquierda. */
    dibujar_rect(tr.x + 8, tr.y + 8, tr.x + 250, tr.y + 74, MAPA_PANEL);
    dibujar_marco(tr.x + 8, tr.y + 8, tr.x + 250, tr.y + 74, MAPA_REJILLA);
    {
        char texto[128];
        formatear(texto, sizeof(texto), "EDITOR DE MAPAS  %d x %d", g.mapa.lado, g.mapa.lado);
        dibujar_texto(tr.x + 16, tr.y + 14, texto, MAPA_TEXTO);
        formatear(texto, sizeof(texto), "Celdas: %d", g.mapa.num);
        dibujar_texto(tr.x + 16, tr.y + 32, texto, MAPA_TEXTO);
        if (g.mapa.tile_sel >= 0 && g.mapa.tile_sel < g.num_tiles)
            formatear(texto, sizeof(texto), "Tile: %s", g.tiles[g.mapa.tile_sel].nombre);
        else
            recortar_texto(texto, "Tile: ninguno (elige en el panel)", sizeof(texto));
        dibujar_texto(tr.x + 16, tr.y + 50, texto, MAPA_TEXTO);
    }

    (void)x; (void)y;
}

/* ------------------------------------------------------------------ */
/* Entrada                                                             */
/* ------------------------------------------------------------------ */
void mapa_pulsar(int x, int y, int boton) {
    int cx, cy;
    Zona tr = zona_trabajo();
    if (!zona_contiene(tr, x, y)) return;
    if (boton == 2) {
        mapa_pan = 1;
        mapa_ult_x = x;
        mapa_ult_y = y;
        return;
    }
    mapa_coord(x, y, &cx, &cy);
    if (boton == 0) {
        if (g.mapa.tile_sel >= 0) mapa_colocar(cx, cy, g.mapa.tile_sel);
        else app_estado("Elige un tile en el panel derecho");
    } else {
        mapa_borrar(cx, cy);
    }
}

void mapa_mover(int x, int y, int botones) {
    if (mapa_pan) {
        int lado = mapa_celda();
        int dx = x - mapa_ult_x;
        int dy = y - mapa_ult_y;
        g.mapa.scroll_x += dx / lado;
        g.mapa.scroll_y += dy / lado;
        mapa_ult_x += (dx / lado) * lado;
        mapa_ult_y += (dy / lado) * lado;
        app_invalidar();
        return;
    }
    (void)botones;
}

void mapa_soltar(int x, int y, int boton) {
    (void)x; (void)y; (void)boton;
    mapa_pan = 0;
}

void mapa_rueda(int x, int y, int delta, int ctrl) {
    if (ctrl) {
        int paso = delta > 0 ? 1 : -1;
        zoom_aplicar(g.zoom_idx + paso);
        mapa_celda_pantalla = mapa_celda();
        return;
    }
    g.mapa.scroll_y += delta > 0 ? -2 : 2;
    if (GetKeyState(VK_SHIFT) & 0x8000) {
        g.mapa.scroll_x += delta > 0 ? -2 : 2;
        g.mapa.scroll_y -= delta > 0 ? -2 : 2;
    }
    (void)x; (void)y;
    app_invalidar();
}

/* ------------------------------------------------------------------ */
/* Guardar / cargar .mav                                               */
/* ------------------------------------------------------------------ */
static void escapar_lua(const char *texto, char *dst, int tam) {
    int i = 0, j = 0;
    if (!dst || tam <= 0) return;
    while (texto[i] && j < tam - 2) {
        char c = texto[i++];
        if (c == '\\' || c == '"') {
            dst[j++] = '\\';
            dst[j++] = c;
        } else if (c == '\n') {
            dst[j++] = '\\';
            dst[j++] = 'n';
        } else {
            dst[j++] = c;
        }
    }
    dst[j] = '\0';
}

static int comparar_celdas(const void *a, const void *b) {
    const TileMapa *ta = (const TileMapa *)a;
    const TileMapa *tb = (const TileMapa *)b;
    if (ta->y != tb->y) return ta->y < tb->y ? -1 : 1;
    if (ta->x != tb->x) return ta->x < tb->x ? -1 : 1;
    return 0;
}

int mapa_guardar(const char *ruta) {
    char *texto;
    size_t capacidad = (size_t)g.mapa.num * 160 + 512;
    size_t usado = 0;
    int i;
    TileMapa *copia;
    if (!ruta) return 0;
    texto = (char *)malloc(capacidad);
    if (!texto) return 0;

    usado += (size_t)sprintf(texto + usado,
        "-- Mapa guardado con Texture Paint 2000\n"
        "-- Love2D: local mapa = dofile(\"mapa.mav\")\n"
        "return {\n"
        "    version = 1,\n"
        "    tile_size = %d,\n"
        "    tiles_dir = \"tiles\",\n"
        "    tiles = {\n", g.mapa.lado);

    copia = (TileMapa *)malloc((size_t)g.mapa.num * sizeof(TileMapa));
    if (!copia) { free(texto); return 0; }
    memcpy(copia, g.mapa.celdas, (size_t)g.mapa.num * sizeof(TileMapa));
    qsort(copia, (size_t)g.mapa.num, sizeof(TileMapa), comparar_celdas);

    for (i = 0; i < g.mapa.num; i++) {
        char imagen[256];
        escapar_lua(copia[i].imagen, imagen, sizeof(imagen));
        usado += (size_t)sprintf(texto + usado,
            "        { x = %d, y = %d, image = \"%s\", w = %d, h = %d },\n",
            copia[i].x, copia[i].y, imagen,
            copia[i].ancho > 0 ? copia[i].ancho : 0,
            copia[i].alto > 0 ? copia[i].alto : 0);
        if (usado + 256 > capacidad) break;
    }
    free(copia);
    usado += (size_t)sprintf(texto + usado, "    },\n}\n");

    if (!escribir_archivo(ruta, texto, (int)usado)) {
        free(texto);
        return 0;
    }
    free(texto);
    return 1;
}

static int extraer_entero_lua(const char *texto, const char *clave, int *salida) {
    const char *pos = strstr(texto, clave);
    int negativo = 0, valor = 0, digitos = 0;
    if (!pos) return 0;
    pos += strlen(clave);
    while (*pos == ' ' || *pos == '=' || *pos == '\t' || *pos == ':') pos++;
    if (*pos == '-') { negativo = 1; pos++; }
    while (*pos >= '0' && *pos <= '9') {
        valor = valor * 10 + (*pos - '0');
        pos++;
        digitos++;
    }
    if (!digitos) return 0;
    *salida = negativo ? -valor : valor;
    return 1;
}

static int extraer_cadena_lua(const char *texto, const char *clave, char *dst, int tam) {
    const char *pos = strstr(texto, clave);
    const char *q1, *q2;
    if (!pos) return 0;
    q1 = strchr(pos + strlen(clave), '"');
    if (!q1) return 0;
    q2 = strchr(q1 + 1, '"');
    if (!q2) return 0;
    {
        int n = (int)(q2 - q1 - 1);
        if (n >= tam) n = tam - 1;
        memcpy(dst, q1 + 1, (size_t)n);
        dst[n] = '\0';
    }
    return 1;
}

int mapa_cargar(const char *ruta) {
    char *contenido;
    int largo = 0;
    const char *p;
    int lado = MAPA_LADO_INICIAL;
    if (!ruta) return 0;
    if (!leer_archivo(ruta, &contenido, &largo)) return 0;
    if (!contenido) return 0;

    mapa_vaciar();
    extraer_entero_lua(contenido, "tile_size", &lado);
    g.mapa.lado = limitar(lado, MAPA_LADO_MIN, MAPA_LADO_MAX);

    p = strstr(contenido, "tiles");
    if (p) {
        p = strstr(p, "{");
        while (p && *p) {
            const char *fin = strchr(p, '}');
            if (!fin) break;
            {
                char bloque[512];
                int n = (int)(fin - p + 1);
                if (n >= (int)sizeof(bloque)) n = (int)sizeof(bloque) - 1;
                memcpy(bloque, p, (size_t)n);
                bloque[n] = '\0';
                if (strstr(bloque, "image")) {
                    int x, y, w = 0, h = 0;
                    char imagen[64];
                    if (extraer_entero_lua(bloque, "x", &x) &&
                        extraer_entero_lua(bloque, "y", &y) &&
                        extraer_cadena_lua(bloque, "image", imagen, sizeof(imagen))) {
                        int i;
                        int colocado = 0;
                        if (!extraer_entero_lua(bloque, "w", &w)) w = 1;
                        if (!extraer_entero_lua(bloque, "h", &h)) h = 1;
                        for (i = 0; i < g.num_tiles; i++) {
                            if (_stricmp(g.tiles[i].nombre, imagen) == 0) {
                                mapa_colocar(x, y, i);
                                colocado = 1;
                                break;
                            }
                        }
                        if (!colocado) {
                            char carpeta[MAX_PATH], ruta_img[MAX_PATH];
                            obtener_ruta_exe(carpeta, sizeof(carpeta));
                            unir_ruta(carpeta, sizeof(carpeta), carpeta, CARPETA_TILES_TEXTO);
                            unir_ruta(ruta_img, sizeof(ruta_img), carpeta, imagen);
                            if (g.mapa.num < MAX_CELDAS) {
                                TileMapa *t = &g.mapa.celdas[g.mapa.num++];
                                memset(t, 0, sizeof(*t));
                                t->x = x;
                                t->y = y;
                                recortar_texto(t->imagen, imagen, sizeof(t->imagen));
                                t->ancho = w > 0 ? w : 1;
                                t->alto = h > 0 ? h : 1;
                                t->pixeles = imagen_cargar(ruta_img, &t->ancho, &t->alto);
                                if (!t->pixeles) {
                                    t->pixeles = (u32 *)calloc((size_t)t->ancho * t->alto, sizeof(u32));
                                }
                            }
                        }
                    }
                }
            }
            p = fin + 1;
            p = strstr(p, "{");
        }
    }
    free(contenido);
    return 1;
}
