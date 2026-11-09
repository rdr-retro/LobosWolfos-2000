/* ==========================================================================
 * Editor de mapas: rejilla infinita de celdas donde se colocan los tiles
 * guardados. El zoom y el desplazamiento son propios del mapa y no afectan
 * al zoom del documento.
 * ========================================================================== */
#include "app.h"

static int mapa_arrastrando = 0;       /* pintando con el boton izquierdo */
static int mapa_borrando = 0;          /* borrando con el boton derecho */
static int mapa_ult_valida = 0;
static int mapa_ult_cx = 0, mapa_ult_cy = 0;
static int mapa_pan_activo = 0;
static int mapa_pan_ult_x = 0, mapa_pan_ult_y = 0;
static int mapa_origen_iniciado = 0;

/* ------------------------------------------------------------------ */
/* Inicio / fin                                                        */
/* ------------------------------------------------------------------ */
void mapa_iniciar(void) {
    memset(&g.mapa, 0, sizeof(g.mapa));
    g.mapa.lado = MAPA_LADO_INICIAL;
    mapa_arrastrando = 0;
    mapa_borrando = 0;
    mapa_pan_activo = 0;
    mapa_ult_valida = 0;
    mapa_origen_iniciado = 0;
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
    g.mapa.tile_sel = -1;
    g.mapa_tile_sel = -1;
    mapa_arrastrando = 0;
    mapa_borrando = 0;
    mapa_pan_activo = 0;
    mapa_ult_valida = 0;
    mapa_origen_iniciado = 0;
}

/* ------------------------------------------------------------------ */
/* Coordenadas                                                         */
/* ------------------------------------------------------------------ */
static int mapa_lado(void) {
    int lado = g.mapa.lado;
    if (lado < MAPA_LADO_MIN) lado = MAPA_LADO_MIN;
    if (lado > MAPA_LADO_MAX) lado = MAPA_LADO_MAX;
    return lado;
}

/* Division entera hacia abajo, tambien con numeros negativos. */
static int div_piso(int a, int b) {
    int q;
    if (b < 0) b = -b;
    if (b <= 0) return 0;
    q = a / b;
    if ((a % b) != 0 && ((a < 0) != (b < 0))) q--;
    return q;
}

static int div_redondeado(int a, int b) {
    if (b <= 0) return 0;
    if (a >= 0) return (a + b / 2) / b;
    return -((-a + b / 2) / b);
}

/* Modulo no negativo: las etiquetas se repiten aunque la celda sea negativa. */
static int mod_pos(int a, int b) {
    int r;
    if (b <= 0) return 0;
    r = a % b;
    if (r < 0) r += b;
    return r;
}

/* La primera vez se centra la celda (0,0) en el area de trabajo. */
static void mapa_asegurar_origen(void) {
    Zona tr;
    if (mapa_origen_iniciado) return;
    tr = zona_trabajo();
    g.mapa.origen_x = tr.x + tr.ancho / 2;
    g.mapa.origen_y = tr.y + tr.alto / 2;
    mapa_origen_iniciado = 1;
}

void mapa_coord(int x, int y, int *cx, int *cy) {
    int lado;
    mapa_asegurar_origen();
    lado = mapa_lado();
    *cx = div_piso(x - g.mapa.origen_x, lado);
    *cy = div_piso(y - g.mapa.origen_y, lado);
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
    if (origen->ancho > 0 && origen->alto > 0 && origen->pixeles) {
        destino->pixeles = (u32 *)malloc((size_t)origen->ancho * origen->alto * sizeof(u32));
        if (destino->pixeles) {
            memcpy(destino->pixeles, origen->pixeles,
                   (size_t)origen->ancho * origen->alto * sizeof(u32));
        }
    }
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
static void mapa_etiqueta(Zona z, int x, int y, const char *texto) {
    int w = fuente_texto_ancho(texto) + 2;
    if (x < z.x || y < z.y || x + w > zona_derecha(z) || y + 9 > zona_abajo(z)) return;
    dibujar_rect(x, y, x + w, y + 9, MAPA_PANEL);
    fuente_dibujar(x + 1, y + 1, texto, MAPA_TEXTO);
}

/* Estampa un tile dentro de la celda conservando su proporcion. */
static void mapa_dibujar_tile(Zona z, int x, int y, int lado, const TileMapa *t) {
    int tw, th, dw, dh, ox, oy, dx, dy;
    double escala, otra;
    if (lado <= 0 || !t || !t->pixeles) return;
    tw = t->ancho;
    th = t->alto;
    if (tw <= 0 || th <= 0) return;

    escala = (double)lado / (double)tw;
    otra = (double)lado / (double)th;
    if (otra < escala) escala = otra;
    dw = (int)((double)tw * escala + 0.5);
    dh = (int)((double)th * escala + 0.5);
    if (dw < 1) dw = 1;
    if (dw > lado) dw = lado;
    if (dh < 1) dh = 1;
    if (dh > lado) dh = lado;
    ox = x + (lado - dw) / 2;
    oy = y + (lado - dh) / 2;

    for (dy = 0; dy < dh; dy++) {
        int py = oy + dy;
        int sy;
        if (py < z.y || py >= zona_abajo(z)) continue;
        sy = (dy * th) / dh;
        if (sy >= th) sy = th - 1;
        for (dx = 0; dx < dw; dx++) {
            int px = ox + dx;
            int sx;
            u32 c;
            if (px < z.x || px >= zona_derecha(z)) continue;
            sx = (dx * tw) / dw;
            if (sx >= tw) sx = tw - 1;
            c = t->pixeles[(size_t)sy * tw + sx];
            if ((c >> 24) == 0) continue;
            dibujar_pixel(px, py, 0xFF000000u | (c & 0x00FFFFFFu));
        }
    }
}

void mapa_dibujar(void) {
    Zona tr = zona_trabajo();
    int lado, ox, oy;
    int col0, col1, fil0, fil1;
    int i, cx, cy;
    int derecha = zona_derecha(tr), abajo = zona_abajo(tr);
    int hover = 0, hcx = 0, hcy = 0;

    dibujar_rect_z(tr, MAPA_FONDO);
    mapa_asegurar_origen();
    lado = mapa_lado();
    ox = g.mapa.origen_x;
    oy = g.mapa.origen_y;

    col0 = div_piso(tr.x - ox, lado) - 1;
    col1 = div_piso(derecha - ox, lado) + 1;
    fil0 = div_piso(tr.y - oy, lado) - 1;
    fil1 = div_piso(abajo - oy, lado) + 1;

    /* Lineas de la rejilla. Los ejes X=0 e Y=0 se resaltan. */
    for (cx = col0; cx <= col1; cx++) {
        int x = ox + cx * lado;
        int grosor = (cx == 0) ? 2 : 1;
        int x2;
        if (x < tr.x || x >= derecha) continue;
        x2 = x + grosor;
        if (x2 > derecha) x2 = derecha;
        dibujar_rect(x, tr.y, x2, abajo, cx == 0 ? MAPA_EJE : MAPA_REJILLA);
    }
    for (cy = fil0; cy <= fil1; cy++) {
        int y = oy + cy * lado;
        int grosor = (cy == 0) ? 2 : 1;
        int y2;
        if (y < tr.y || y >= abajo) continue;
        y2 = y + grosor;
        if (y2 > abajo) y2 = abajo;
        dibujar_rect(tr.x, y, derecha, y2, cy == 0 ? MAPA_EJE : MAPA_REJILLA);
    }

    /* Tiles colocados. La celda ocupada se pinta antes de estampar el tile. */
    for (i = 0; i < g.mapa.num; i++) {
        TileMapa *t = &g.mapa.celdas[i];
        int x = ox + t->x * lado;
        int y = oy + t->y * lado;
        int x0, y0, x1, y1;
        if (x >= derecha || y >= abajo || x + lado <= tr.x || y + lado <= tr.y) continue;
        x0 = x > tr.x ? x : tr.x;
        y0 = y > tr.y ? y : tr.y;
        x1 = (x + lado) < derecha ? (x + lado) : derecha;
        y1 = (y + lado) < abajo ? (y + lado) : abajo;
        dibujar_rect(x0, y0, x1, y1, MAPA_CELDA);
        mapa_dibujar_tile(tr, x, y, lado, t);
    }

    /* Etiquetas de coordenadas encima de los tiles para que siempre se lean. */
    {
        int paso = (lado >= 24) ? 1 : (lado >= 16 ? 5 : 10);
        char texto[32];
        for (cx = col0; cx <= col1; cx++) {
            int x;
            if (mod_pos(cx, paso) != 0) continue;
            x = ox + cx * lado;
            if (x < tr.x || x + 10 >= derecha) continue;
            formatear(texto, sizeof(texto), "%d", cx);
            mapa_etiqueta(tr, x + 1, tr.y + 1, texto);
        }
        for (cy = fil0; cy <= fil1; cy++) {
            int y;
            if (mod_pos(cy, paso) != 0) continue;
            y = oy + cy * lado;
            if (y < tr.y || y + 10 >= abajo) continue;
            formatear(texto, sizeof(texto), "%d", cy);
            mapa_etiqueta(tr, tr.x + 1, y + 1, texto);
        }
    }

    /* Celda bajo el cursor: marco doble recortado al area de trabajo. */
    if (g.raton_dentro && !mapa_pan_activo &&
        zona_contiene(tr, g.raton_x, g.raton_y)) {
        hover = 1;
        mapa_coord(g.raton_x, g.raton_y, &hcx, &hcy);
    }
    if (hover) {
        int x = ox + hcx * lado;
        int y = oy + hcy * lado;
        if (x + lado > tr.x && y + lado > tr.y && x < derecha && y < abajo) {
            int x0 = x > tr.x ? x : tr.x;
            int y0 = y > tr.y ? y : tr.y;
            int x1 = (x + lado) < derecha ? (x + lado) : derecha;
            int y1 = (y + lado) < abajo ? (y + lado) : abajo;
            dibujar_rect(x0, y0, x1, y0 + 2, MAPA_HOVER);
            dibujar_rect(x0, y1 - 2, x1, y1, MAPA_HOVER);
            dibujar_rect(x0, y0, x0 + 2, y1, MAPA_HOVER);
            dibujar_rect(x1 - 2, y0, x1, y1, MAPA_HOVER);
        }
    }

    /* Panel de estado del editor. */
    {
        Zona panel;
        int panel_ancho = tr.ancho - 16;
        char texto[96];
        if (panel_ancho > 232) panel_ancho = 232;
        if (panel_ancho < 90) panel_ancho = 90;
        panel = zona_hacer(tr.x + 8, tr.y + 8, panel_ancho, 62);
        dibujar_rect_z(panel, MAPA_PANEL);
        dibujar_marco_z(panel, MAPA_REJILLA);
        fuente_dibujar(panel.x + 8, panel.y + 7, "EDITOR DE MAPAS", MAPA_HOVER);
        if (hover) formatear(texto, sizeof(texto), "X %d  Y %d", hcx, hcy);
        else recortar_texto(texto, "X -  Y -", sizeof(texto));
        fuente_dibujar(panel.x + 8, panel.y + 25, texto, MAPA_TEXTO);
        if (g.mapa.tile_sel >= 0 && g.mapa.tile_sel < g.num_tiles)
            formatear(texto, sizeof(texto), "TILES %d  TILE %d",
                      g.mapa.num, g.mapa.tile_sel + 1);
        else
            formatear(texto, sizeof(texto), "TILES %d  SIN TILE", g.mapa.num);
        fuente_dibujar(panel.x + 8, panel.y + 43, texto, MAPA_TEXTO);
    }

    /* Ayuda de controles al pie del area de trabajo. */
    {
        const char *ayuda1 = "CLIC: PONER   DERECHO: BORRAR";
        const char *ayuda2 = "CENTRAL/ESPACIO: MOVER   RUEDA: MOVER   CTRL RUEDA: ZOOM";
        int ancho1 = fuente_texto_ancho(ayuda1);
        int ancho2 = fuente_texto_ancho(ayuda2);
        int ayuda_ancho = (ancho1 > ancho2 ? ancho1 : ancho2) + 10;
        if (tr.alto > 120 && tr.ancho > ayuda_ancho + 16) {
            int x = tr.x + 8;
            int y = abajo - 30;
            dibujar_rect(x, y, x + ayuda_ancho, abajo - 4, MAPA_PANEL);
            fuente_dibujar(x + 4, y + 4, ayuda1, MAPA_TEXTO);
            fuente_dibujar(x + 4, y + 16, ayuda2, MAPA_TEXTO);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Entrada                                                             */
/* ------------------------------------------------------------------ */
static int mapa_tile_valido(void) {
    return g.mapa.tile_sel >= 0 && g.mapa.tile_sel < g.num_tiles;
}

void mapa_pulsar(int x, int y, int boton) {
    Zona tr = zona_trabajo();
    int cx, cy;
    int espacio;
    if (!zona_contiene(tr, x, y)) return;
    mapa_asegurar_origen();
    espacio = (GetKeyState(VK_SPACE) & 0x8000) != 0;

    /* Boton central o Espacio + izquierdo: recorrer el mapa infinito. */
    if (boton == 2 || (boton == 0 && espacio)) {
        mapa_pan_activo = 1;
        mapa_pan_ult_x = x;
        mapa_pan_ult_y = y;
        mapa_arrastrando = 0;
        mapa_borrando = 0;
        app_invalidar();
        return;
    }

    mapa_coord(x, y, &cx, &cy);
    if (boton == 0) {
        if (!mapa_tile_valido() && g.num_tiles > 0) {
            int indice = g.num_tiles - 1;
            g.mapa.tile_sel = indice;
            g.mapa_tile_sel = indice;
        }
        if (!mapa_tile_valido()) {
            app_estado("Elige un tile en el panel derecho");
            return;
        }
        mapa_colocar(cx, cy, g.mapa.tile_sel);
        mapa_arrastrando = 1;
        mapa_ult_valida = 1;
        mapa_ult_cx = cx;
        mapa_ult_cy = cy;
        app_invalidar();
    } else if (boton == 1) {
        mapa_borrar(cx, cy);
        mapa_borrando = 1;
        mapa_ult_valida = 1;
        mapa_ult_cx = cx;
        mapa_ult_cy = cy;
        app_invalidar();
    }
}

void mapa_mover(int x, int y, int botones) {
    Zona tr = zona_trabajo();
    int espacio = (GetKeyState(VK_SPACE) & 0x8000) != 0;
    int medio = g.boton_medio || (botones & MK_MBUTTON) != 0;
    int izquierdo = g.boton_izq || (botones & MK_LBUTTON) != 0;
    int derecho = g.boton_der || (botones & MK_RBUTTON) != 0;
    int quiere_pan = medio || (izquierdo && espacio);
    int dentro = zona_contiene(tr, x, y);
    int cambio = 0;

    mapa_asegurar_origen();

    if (dentro && quiere_pan && !mapa_pan_activo) {
        mapa_pan_activo = 1;
        mapa_pan_ult_x = x;
        mapa_pan_ult_y = y;
    }
    if (mapa_pan_activo) {
        if (quiere_pan) {
            g.mapa.origen_x += x - mapa_pan_ult_x;
            g.mapa.origen_y += y - mapa_pan_ult_y;
            mapa_pan_ult_x = x;
            mapa_pan_ult_y = y;
            cambio = 1;
        } else {
            mapa_pan_activo = 0;
        }
    }

    if (mapa_arrastrando) {
        if (!izquierdo || espacio) {
            mapa_arrastrando = 0;
        } else if (dentro && !mapa_pan_activo) {
            int cx, cy;
            mapa_coord(x, y, &cx, &cy);
            if (!mapa_ult_valida || cx != mapa_ult_cx || cy != mapa_ult_cy) {
                if (mapa_tile_valido()) mapa_colocar(cx, cy, g.mapa.tile_sel);
                mapa_ult_valida = 1;
                mapa_ult_cx = cx;
                mapa_ult_cy = cy;
                cambio = 1;
            }
        }
    }

    if (mapa_borrando) {
        if (!derecho) {
            mapa_borrando = 0;
        } else if (dentro && !mapa_pan_activo) {
            int cx, cy;
            mapa_coord(x, y, &cx, &cy);
            if (!mapa_ult_valida || cx != mapa_ult_cx || cy != mapa_ult_cy) {
                mapa_borrar(cx, cy);
                mapa_ult_valida = 1;
                mapa_ult_cx = cx;
                mapa_ult_cy = cy;
                cambio = 1;
            }
        }
    }

    if (cambio) app_invalidar();
}

void mapa_soltar(int x, int y, int boton) {
    (void)x;
    (void)y;
    if (boton == 0) mapa_arrastrando = 0;
    if (boton == 1) mapa_borrando = 0;
    if (boton == 2 || (boton == 0 && !g.boton_medio)) mapa_pan_activo = 0;
}

void mapa_rueda(int x, int y, int delta, int ctrl) {
    int lado, signo, nuevo;
    if (delta == 0) return;
    mapa_asegurar_origen();
    signo = delta > 0 ? 1 : -1;
    lado = mapa_lado();

    if (ctrl) {
        /* Zoom propio del mapa, anclado al punto bajo el cursor. */
        nuevo = lado + signo * MAPA_LADO_PASO;
        if (nuevo < MAPA_LADO_MIN) nuevo = MAPA_LADO_MIN;
        if (nuevo > MAPA_LADO_MAX) nuevo = MAPA_LADO_MAX;
        if (nuevo != lado) {
            int dx = x - g.mapa.origen_x;
            int dy = y - g.mapa.origen_y;
            g.mapa.lado = nuevo;
            g.mapa.origen_x = x - div_redondeado(dx * nuevo, lado);
            g.mapa.origen_y = y - div_redondeado(dy * nuevo, lado);
        }
    } else {
        int paso = signo * MAPA_RUEDA_PASO;
        if (GetKeyState(VK_SHIFT) & 0x8000) g.mapa.origen_x += paso;
        else g.mapa.origen_y += paso;
    }
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
        "-- Mapa guardado con LobosWolfos 2000\n"
        "-- Love2D: local mapa = dofile(\"mapa.mav\")\n"
        "return {\n"
        "    version = 1,\n"
        "    tile_size = %d,\n"
        "    tiles_dir = \"tiles\",\n"
        "    tiles = {\n", mapa_lado());

    copia = (TileMapa *)malloc((size_t)g.mapa.num * sizeof(TileMapa));
    if (!copia && g.mapa.num > 0) { free(texto); return 0; }
    if (g.mapa.num > 0)
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

static int archivo_existe(const char *ruta) {
    DWORD atributos;
    if (!ruta || !ruta[0]) return 0;
    atributos = GetFileAttributesA(ruta);
    return atributos != INVALID_FILE_ATTRIBUTES &&
           (atributos & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

/* Busca el PNG de un tile como en la version Rust: primero en la carpeta
 * `tiles_dir` junto al .mav, luego junto al .mav y por ultimo en la carpeta
 * `tiles` del ejecutable. */
static int resolver_imagen_mapa(const char *mapa_ruta, const char *tiles_dir,
                                const char *imagen, char *dst, int tam) {
    char base[MAX_PATH];
    char carpeta[MAX_PATH];
    char exe[MAX_PATH];

    ruta_carpeta(base, sizeof(base), mapa_ruta);

    /* <carpeta del .mav>/<tiles_dir>/<imagen> */
    unir_ruta(carpeta, sizeof(carpeta), base, tiles_dir);
    unir_ruta(dst, tam, carpeta, imagen);
    if (archivo_existe(dst)) return 1;

    /* <carpeta del .mav>/<imagen> */
    unir_ruta(dst, tam, base, imagen);
    if (archivo_existe(dst)) return 1;

    /* <carpeta del exe>/tiles/<imagen> */
    obtener_ruta_exe(exe, sizeof(exe));
    unir_ruta(carpeta, sizeof(carpeta), exe, CARPETA_TILES_TEXTO);
    unir_ruta(dst, tam, carpeta, imagen);
    return archivo_existe(dst);
}

int mapa_cargar(const char *ruta) {
    char *contenido;
    int largo = 0;
    const char *p;
    int lado = MAPA_LADO_INICIAL;
    char tiles_dir[128];
    if (!ruta) return 0;
    if (!leer_archivo(ruta, &contenido, &largo)) return 0;
    if (!contenido) return 0;

    mapa_vaciar();
    extraer_entero_lua(contenido, "tile_size", &lado);
    g.mapa.lado = limitar(lado, MAPA_LADO_MIN, MAPA_LADO_MAX);
    if (!extraer_cadena_lua(contenido, "tiles_dir", tiles_dir, sizeof(tiles_dir)))
        recortar_texto(tiles_dir, "tiles", sizeof(tiles_dir));
    if (!tiles_dir[0]) recortar_texto(tiles_dir, "tiles", sizeof(tiles_dir));

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
                        if (!extraer_entero_lua(bloque, "w", &w)) w = 1;
                        if (!extraer_entero_lua(bloque, "h", &h)) h = 1;
                        if (g.mapa.num < MAX_CELDAS) {
                            char ruta_img[MAX_PATH];
                            TileMapa *t = &g.mapa.celdas[g.mapa.num++];
                            memset(t, 0, sizeof(*t));
                            t->x = x;
                            t->y = y;
                            recortar_texto(t->imagen, imagen, sizeof(t->imagen));
                            t->ancho = limitar(w, 1, 4096);
                            t->alto = limitar(h, 1, 4096);
                            if (resolver_imagen_mapa(ruta, tiles_dir, imagen,
                                                     ruta_img, sizeof(ruta_img))) {
                                t->pixeles = imagen_cargar(ruta_img, &t->ancho, &t->alto);
                            }
                            if (!t->pixeles) {
                                t->ancho = limitar(w, 1, 4096);
                                t->alto = limitar(h, 1, 4096);
                                t->pixeles = (u32 *)calloc((size_t)t->ancho * t->alto,
                                                           sizeof(u32));
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
