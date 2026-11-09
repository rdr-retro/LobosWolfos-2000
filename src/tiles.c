/* ==========================================================================
 * Tiles: guardar y cargar los PNG de la carpeta `tiles`.
 * ========================================================================== */
#include "app.h"

static void carpeta_tiles(char *dst, int tam) {
    char exe[MAX_PATH];
    obtener_ruta_exe(exe, sizeof(exe));
    unir_ruta(dst, tam, exe, CARPETA_TILES_TEXTO);
}

void tile_liberar(TileGuardado *t) {
    if (!t) return;
    if (t->pixeles) free(t->pixeles);
    t->pixeles = NULL;
    t->ancho = t->alto = 0;
}

void tiles_liberar(void) {
    int i;
    for (i = 0; i < g.num_tiles; i++) tile_liberar(&g.tiles[i]);
    g.num_tiles = 0;
}

/* Clave de orden de los tiles, como `orden_tile`: `tile_N.png` va por su
 * numero y el resto de imagenes quedan al final, por nombre en minusculas. */
static void orden_tile(const char *nombre, unsigned *numero, char *clave, int tam) {
    const char *resto;
    const char *fin;
    unsigned n = 0;
    int i = 0;
    int valido = 1;
    *numero = 0xFFFFFFFFu;
    if (strncmp(nombre, "tile_", 5) == 0) {
        resto = nombre + 5;
        fin = resto + strlen(resto);
        if (fin - resto > 4 && strcmp(fin - 4, ".png") == 0) {
            const char *p;
            fin -= 4;
            for (p = resto; p < fin; p++) {
                if (*p < '0' || *p > '9') { valido = 0; break; }
                n = n * 10u + (unsigned)(*p - '0');
            }
            if (valido && fin > resto) *numero = n;
        }
    }
    while (nombre[i] && i < tam - 1) {
        char c = nombre[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        clave[i] = c;
        i++;
    }
    clave[i] = '\0';
}

static int comparar_tiles(const TileGuardado *a, const TileGuardado *b) {
    unsigned na, nb;
    char ca[64], cb[64];
    orden_tile(a->nombre, &na, ca, sizeof(ca));
    orden_tile(b->nombre, &nb, cb, sizeof(cb));
    if (na < nb) return -1;
    if (na > nb) return 1;
    return strcmp(ca, cb);
}

void tiles_ordenar_por_fecha(void) {
    int i, j;
    for (i = 1; i < g.num_tiles; i++) {
        TileGuardado t = g.tiles[i];
        j = i - 1;
        while (j >= 0 && comparar_tiles(&g.tiles[j], &t) > 0) {
            g.tiles[j + 1] = g.tiles[j];
            j--;
        }
        g.tiles[j + 1] = t;
    }
}

void tiles_cargar(void) {
    char carpeta[MAX_PATH];
    char patron[MAX_PATH];
    WIN32_FIND_DATAA datos;
    HANDLE busqueda;
    int scroll_anterior = g.tiles_scroll;
    int max_scroll;
    tiles_liberar();
    carpeta_tiles(carpeta, sizeof(carpeta));
    crear_carpeta(carpeta);
    formatear(patron, sizeof(patron), "%s\\*.png", carpeta);
    busqueda = FindFirstFileA(patron, &datos);
    if (busqueda == INVALID_HANDLE_VALUE) {
        g.tiles_scroll = 0;
        return;
    }
    do {
        char ruta[MAX_PATH];
        int w = 0, h = 0;
        u32 *pix;
        if (datos.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (g.num_tiles >= MAX_TILES) break;
        unir_ruta(ruta, sizeof(ruta), carpeta, datos.cFileName);
        pix = imagen_cargar(ruta, &w, &h);
        if (!pix) continue;
        if (w < 1 || h < 1 || w > 4096 || h > 4096) { free(pix); continue; }
        {
            TileGuardado *t = &g.tiles[g.num_tiles];
            memset(t, 0, sizeof(*t));
            ruta_nombre(t->nombre, sizeof(t->nombre), datos.cFileName);
            recortar_texto(t->ruta, ruta, sizeof(t->ruta));
            t->ancho = w;
            t->alto = h;
            t->pixeles = pix;
            g.num_tiles++;
        }
    } while (FindNextFileA(busqueda, &datos));
    FindClose(busqueda);
    tiles_ordenar_por_fecha();
    max_scroll = g.num_tiles - TILES_SELECTOR_VISIBLES;
    if (max_scroll < 0) max_scroll = 0;
    g.tiles_scroll = scroll_anterior;
    if (g.tiles_scroll > max_scroll) g.tiles_scroll = max_scroll;
    if (g.tiles_scroll < 0) g.tiles_scroll = 0;
}

/* Primera ruta libre `tile_N.png` dentro de la carpeta. */
static void siguiente_tile(char *dst, int tam) {
    char carpeta[MAX_PATH];
    char ruta[MAX_PATH];
    int numero;
    carpeta_tiles(carpeta, sizeof(carpeta));
    for (numero = 1; numero < 1000000; numero++) {
        formatear(ruta, sizeof(ruta), "%s\\tile_%d.png", carpeta, numero);
        if (GetFileAttributesA(ruta) == INVALID_FILE_ATTRIBUTES) break;
    }
    recortar_texto(dst, ruta, tam);
}

int tiles_guardar_desde(const u32 *pix, int ancho, int alto, const char *nombre) {
    char carpeta[MAX_PATH];
    char ruta[MAX_PATH];
    int max_scroll;
    (void)nombre;
    if (!pix || ancho < 1 || alto < 1) return 0;
    carpeta_tiles(carpeta, sizeof(carpeta));
    crear_carpeta(carpeta);
    siguiente_tile(ruta, sizeof(ruta));
    if (!imagen_guardar_png(ruta, pix, ancho, alto)) {
        app_estado("No se pudo guardar el tile");
        return 0;
    }
    /* Se recarga la carpeta y se reeordena. */
    tiles_cargar();
    max_scroll = g.num_tiles - TILES_SELECTOR_VISIBLES;
    if (max_scroll < 0) max_scroll = 0;
    if (g.tiles_scroll > max_scroll) g.tiles_scroll = max_scroll;
    app_estado("Tile guardado: %s", ruta);
    return 1;
}

int tiles_guardar_actual(void) {
    doc_componer_todo(&g.doc);
    return tiles_guardar_desde(g.doc.compuesto, g.doc.ancho, g.doc.alto, NULL);
}

u32 *tile_miniatura(const TileGuardado *t, int lado) {
    u32 *mini;
    int x, y;
    float fw, fh, f;
    int dw, dh, ox, oy;
    if (!t || !t->pixeles || lado < 1) return NULL;
    mini = (u32 *)calloc((size_t)lado * lado, sizeof(u32));
    if (!mini) return NULL;
    fw = (float)lado / (float)t->ancho;
    fh = (float)lado / (float)t->alto;
    f = fw < fh ? fw : fh;
    dw = (int)((float)t->ancho * f);
    dh = (int)((float)t->alto * f);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    ox = (lado - dw) / 2;
    oy = (lado - dh) / 2;
    for (y = 0; y < dh; y++) {
        int sy = y * t->alto / dh;
        for (x = 0; x < dw; x++) {
            int sx = x * t->ancho / dw;
            mini[(size_t)(oy + y) * lado + (ox + x)] = t->pixeles[(size_t)sy * t->ancho + sx];
        }
    }
    return mini;
}
