/* ==========================================================================
 * Interfaz: barra de menus, sidebar, barras, paneles y ventanas flotantes.
 * Todo se dibuja a mano sobre el buffer de 32 bits.
 * ========================================================================== */
#include "app.h"

static const char *MENU_ETIQUETAS[8] = {
    "ARCHIVO", "MODO", "EDITAR", "GESTOR DE MODULOS",
    "SELECCIONAR", "CAPA", "COLORES", "INFO"
};

#define MENU_MARGEN 6
#define MENU_PADDING 10

#define SEPARADOR_ALTO 8
#define SEPARADOR_MARGEN 4
#define ICONO_CAJA 24
#define DIVISION_HERRAMIENTAS 8
#define HERRAMIENTAS_ESPECIALES 2

static const char *NOMBRES_HERRAMIENTA[H_TOTAL] = {
    "Pencil", "Brush", "Cube", "Spray", "Draft", "Text",
    "Dropper", "Zoom", "Line", "Curve", "Circle", "Polygon",
    "Rectangle", "Rounded Rectangle", "Free Form Select", "Rectangular Sellection"
};

const char *ui_nombre_herramienta(int indice) {
    if (indice < 0 || indice >= H_TOTAL) return "";
    return NOMBRES_HERRAMIENTA[indice];
}

static const u32 ARCOIRIS[7] = {
    0x00E53935u, 0x00FB8C00u, 0x00FDD835u, 0x0043A047u,
    0x001E88E5u, 0x003949ABu, 0x008E24AAu
};

static const char *TEXTURAS_NOMBRE[TEXTURA_TOTAL] = {
    "Cerda", "Acuarela", "Tinta", "Seco", "Aerografo"
};

static const char *ROCIO_NOMBRE[ROCIO_TOTAL] = {
    "Fina", "Media", "Gruesa", "Dispersa", "Goteo"
};

/* ------------------------------------------------------------------ */
/* Ayudas de dibujo                                                    */
/* ------------------------------------------------------------------ */
static u32 color_casilla(u32 c) {
    return 0xFF000000u | (c & 0x00FFFFFFu);
}

/* ------------------------------------------------------------------ */
/* Zonas                                                               */
/* ------------------------------------------------------------------ */
int sidebar_derecho_ancho(void) {
    int a = SIDEBAR_MARGEN * 2 + TILES_REJILLA_ANCHO + TILES_SCROLL_HUECO + TILES_SCROLL_ANCHO;
    int b = SIDEBAR_ANCHO * 2;
    return a > b ? a : b;
}

static int ancho_util(void) {
    int w = g.ancho - SIDEBAR_ANCHO - sidebar_derecho_ancho();
    return w < MIN_LADO ? MIN_LADO : w;
}

Zona menu_barra_zona(void) {
    return zona_hacer(0, TAB_BARRA_ALTO, g.ancho, MENU_BARRA_ALTO);
}

Zona barra_zona(void) {
    return zona_hacer(SIDEBAR_ANCHO, AREA_SUP_ALTO, ancho_util(), BARRA_ALTO);
}

Zona barra_abajo_zona(void) {
    int alto = BARRA_ABAJO_ALTO < g.alto ? BARRA_ABAJO_ALTO : g.alto;
    return zona_hacer(SIDEBAR_ANCHO, g.alto - alto, ancho_util(), alto);
}

Zona zona_trabajo(void) {
    Zona abajo = barra_abajo_zona();
    int arriba = AREA_SUP_ALTO + BARRA_ALTO;
    int alto = abajo.y - arriba;
    if (alto < MIN_LADO) alto = MIN_LADO;
    return zona_hacer(SIDEBAR_ANCHO, arriba, ancho_util(), alto);
}

/* Lienzo del gestor de modulos: todo el hueco central entre los paneles
 * laterales, desde la barra de menus hasta la barra inferior. */
Zona modulos_lienzo_zona(void) {
    Zona abajo = barra_abajo_zona();
    int panel = sidebar_derecho_ancho();
    int ancho = g.ancho - SIDEBAR_ANCHO - panel;
    int alto = abajo.y - AREA_SUP_ALTO;
    if (ancho < MIN_LADO) ancho = MIN_LADO;
    if (alto < MIN_LADO) alto = MIN_LADO;
    return zona_hacer(SIDEBAR_ANCHO, AREA_SUP_ALTO, ancho, alto);
}

/* Boton rectangular sin nombre del panel derecho del gestor. */
Zona modulos_boton_zona(void) {
    int panel = sidebar_derecho_ancho();
    int x0 = g.ancho - panel;
    int ancho = panel - SIDEBAR_MARGEN * 2;
    if (ancho < 8) ancho = 8;
    return zona_hacer(x0 + SIDEBAR_MARGEN, AREA_SUP_ALTO + SIDEBAR_MARGEN,
                      ancho, MODULOS_BOTON_ALTO);
}

static int grupos_sidebar(int total, int grupos[3]) {
    int normales = total - HERRAMIENTAS_ESPECIALES;
    int division;
    if (normales < 0) normales = 0;
    division = DIVISION_HERRAMIENTAS < normales ? DIVISION_HERRAMIENTAS : normales;
    grupos[0] = division;
    grupos[1] = normales - division;
    grupos[2] = total - normales;
    return 0;
}

static int filas_grupo(int botones) {
    return (botones + COLUMNAS - 1) / COLUMNAS;
}

Zona herramienta_zona(int indice) {
    int grupos[3];
    int columna = indice % COLUMNAS;
    int inicio = 0, filas_antes = 0, hueco = 0, k;
    grupos_sidebar(H_TOTAL, grupos);
    for (k = 0; k < 3; k++) {
        int cantidad = grupos[k];
        if (cantidad == 0) continue;
        if (indice < inicio + cantidad) {
            int fila = filas_antes + (indice - inicio) / COLUMNAS;
            return zona_hacer(SIDEBAR_MARGEN + columna * (BOTON_LADO + HUECO),
                              AREA_SUP_ALTO + SIDEBAR_MARGEN + fila * (BOTON_LADO + HUECO) + hueco,
                              BOTON_LADO, BOTON_LADO);
        }
        inicio += cantidad;
        filas_antes += filas_grupo(cantidad);
        hueco += SEPARADOR_ALTO;
    }
    return zona_hacer(SIDEBAR_MARGEN + columna * (BOTON_LADO + HUECO),
                      AREA_SUP_ALTO + SIDEBAR_MARGEN + filas_antes * (BOTON_LADO + HUECO) + hueco,
                      BOTON_LADO, BOTON_LADO);
}

static void cuadros_logo(Zona *superior, Zona *inferior) {
    Zona barra = barra_zona();
    int x = barra.x + CUADRO_MARGEN;
    int conjunto = CUADRO_LADO + CUADRO_DESFASE;
    int y = barra.y + (BARRA_ALTO - conjunto) / 2;
    if (y < barra.y + 2) y = barra.y + 2;
    *superior = zona_hacer(x, y, CUADRO_LADO, CUADRO_LADO);
    *inferior = zona_hacer(x + CUADRO_DESFASE, y + CUADRO_DESFASE, CUADRO_LADO, CUADRO_LADO);
}

Zona mini_zona(int indice) {
    Zona sup, inf;
    Zona barra = barra_zona();
    int rejilla_alto = MINI_FILAS * MINI_LADO + (MINI_FILAS - 1) * MINI_HUECO;
    int x0, y0, fila, columna;
    cuadros_logo(&sup, &inf);
    x0 = zona_derecha(inf) + MINI_SEPARACION;
    y0 = barra.y + (BARRA_ALTO - rejilla_alto) / 2;
    fila = indice / MINI_COLUMNAS;
    columna = indice % MINI_COLUMNAS;
    return zona_hacer(x0 + columna * (MINI_LADO + MINI_HUECO),
                      y0 + fila * (MINI_LADO + MINI_HUECO),
                      MINI_LADO, MINI_LADO);
}

Zona arcoiris_zona(void) {
    Zona barra = barra_zona();
    Zona ultima = mini_zona(MINI_TOTAL - 1);
    return zona_hacer(zona_derecha(ultima) + MINI_SEPARACION,
                      barra.y + (BARRA_ALTO - ARCOIRIS_LADO) / 2,
                      ARCOIRIS_LADO, ARCOIRIS_LADO);
}

Zona boton_guardar_tile_zona(void) {
    Zona a = arcoiris_zona();
    return zona_hacer(zona_derecha(a) + MINI_SEPARACION,
                      a.y + (ARCOIRIS_LADO - GUARDAR_TILE_LADO) / 2,
                      GUARDAR_TILE_LADO, GUARDAR_TILE_LADO);
}

Zona panel_textura_zona(void) {
    Zona ultima = herramienta_zona(H_TOTAL - 1);
    int x = (SIDEBAR_ANCHO - TEXTURA_DIAMETRO) / 2;
    int y = zona_abajo(ultima) + TEXTURA_MARGEN;
    return zona_hacer(x, y, TEXTURA_DIAMETRO, TEXTURA_DIAMETRO);
}

Zona zoom_pista_zona(void) {
    Zona barra = barra_abajo_zona();
    int etiqueta_x = zona_derecha(barra) - ZOOM_MARGEN - ZOOM_ETIQUETA_ANCHO;
    int mas_x = etiqueta_x - ZOOM_HUECO - ZOOM_BOTON;
    int pista_x = mas_x - ZOOM_HUECO - ZOOM_PISTA_ANCHO;
    return zona_hacer(pista_x, barra.y + (barra.alto - ZOOM_PISTA_ALTO) / 2,
                      ZOOM_PISTA_ANCHO, ZOOM_PISTA_ALTO);
}

Zona zoom_boton_menos(void) {
    Zona pista = zoom_pista_zona();
    Zona barra = barra_abajo_zona();
    int x = pista.x - ZOOM_HUECO - ZOOM_BOTON;
    return zona_hacer(x, barra.y + (barra.alto - ZOOM_BOTON) / 2, ZOOM_BOTON, ZOOM_BOTON);
}

Zona zoom_boton_mas(void) {
    Zona barra = barra_abajo_zona();
    int etiqueta_x = zona_derecha(barra) - ZOOM_MARGEN - ZOOM_ETIQUETA_ANCHO;
    int x = etiqueta_x - ZOOM_HUECO - ZOOM_BOTON;
    return zona_hacer(x, barra.y + (barra.alto - ZOOM_BOTON) / 2, ZOOM_BOTON, ZOOM_BOTON);
}

Zona zoom_etiqueta_zona(void) {
    Zona barra = barra_abajo_zona();
    return zona_hacer(zona_derecha(barra) - ZOOM_MARGEN - ZOOM_ETIQUETA_ANCHO,
                      barra.y, ZOOM_ETIQUETA_ANCHO, barra.alto);
}

Zona grosor_pista_zona(void) {
    Zona barra = barra_abajo_zona();
    Zona menos = zoom_boton_menos();
    int x = menos.x - GROSOR_SEPARACION - GROSOR_PISTA_ANCHO;
    return zona_hacer(x, barra.y + (barra.alto - ZOOM_PISTA_ALTO) / 2,
                      GROSOR_PISTA_ANCHO, ZOOM_PISTA_ALTO);
}

Zona grosor_etiqueta_zona(void) {
    Zona barra = barra_abajo_zona();
    Zona pista = grosor_pista_zona();
    return zona_hacer(pista.x - GROSOR_ETIQUETA_ANCHO, barra.y,
                      GROSOR_ETIQUETA_ANCHO, barra.alto);
}

/* Tamano y posicion de la ventana de opciones. Sin posicion explicita se
 * centra arriba del area de trabajo; con posicion, se recorta para que la
 * ventana entera quede dentro del area (igual que `zona_panel_cubo`). */
Zona opciones_zona(void) {
    Zona trabajo = zona_trabajo();
    int ancho = OPCIONES_ANCHO;
    int alto = OPCIONES_ALTO;
    int x, y;
    if (ancho > trabajo.ancho - OPCIONES_MARGEN * 2) ancho = trabajo.ancho - OPCIONES_MARGEN * 2;
    if (ancho < 0) ancho = 0;
    if (alto > trabajo.alto) alto = trabajo.alto;
    if (alto < 0) alto = 0;
    if (g.panel_opciones_x < 0 || g.panel_opciones_y < 0) {
        x = trabajo.x + (trabajo.ancho - ancho) / 2;
        if (x < trabajo.x) x = trabajo.x;
        y = trabajo.y + OPCIONES_MARGEN;
    } else {
        int max_x = zona_derecha(trabajo) - ancho;
        int max_y = zona_abajo(trabajo) - alto;
        if (max_x < trabajo.x) max_x = trabajo.x;
        if (max_y < trabajo.y) max_y = trabajo.y;
        x = g.panel_opciones_x;
        y = g.panel_opciones_y;
        if (x < trabajo.x) x = trabajo.x;
        if (x > max_x) x = max_x;
        if (y < trabajo.y) y = trabajo.y;
        if (y > max_y) y = max_y;
    }
    return zona_hacer(x, y, ancho, alto);
}

Zona opciones_titulo_zona(void) {
    Zona panel = opciones_zona();
    return zona_hacer(panel.x, panel.y, panel.ancho,
                      OPCIONES_TITULO_ALTO < panel.alto ? OPCIONES_TITULO_ALTO : panel.alto);
}

Zona opciones_minimizar_zona(void) {
    Zona panel = opciones_zona();
    return zona_hacer(zona_derecha(panel) - OPCIONES_PADDING - OPCIONES_MIN_LADO,
                      panel.y + (OPCIONES_TITULO_ALTO - OPCIONES_MIN_LADO) / 2,
                      OPCIONES_MIN_LADO, OPCIONES_MIN_LADO);
}

/* Pista de una fila de deslizador dentro de la ventana. */
Zona opciones_pista_zona(int fila) {
    Zona panel = opciones_zona();
    int x0 = panel.x + OPCIONES_PADDING;
    int fin = zona_derecha(panel) - OPCIONES_PADDING;
    int fila_y = panel.y + OPCIONES_TITULO_ALTO + fila * OPCIONES_FILA_ALTO;
    int ancho = OPCIONES_PISTA_ANCHO;
    if (ancho > fin - (x0 + OPCIONES_PISTA_X)) ancho = fin - (x0 + OPCIONES_PISTA_X);
    if (ancho < 0) ancho = 0;
    return zona_hacer(x0 + OPCIONES_PISTA_X,
                      fila_y + (OPCIONES_FILA_ALTO - OPCIONES_PISTA_ALTO) / 2,
                      ancho, OPCIONES_PISTA_ALTO);
}

/* Tira que representa la ventana plegada: vive justo debajo del nombre de la
 * herramienta, en el panel izquierdo. */
Zona opciones_minimizada_zona(void) {
    Zona circulo = panel_textura_zona();
    int nombre_y = zona_abajo(circulo) + AJUSTES_MARGEN;
    return zona_hacer(SIDEBAR_MARGEN,
                      nombre_y + AJUSTES_NOMBRE_ALTO + OPCIONES_VENTANA_MIN_HUECO,
                      AJUSTES_ANCHO, OPCIONES_VENTANA_MIN_ALTO);
}

Zona tile_mini_zona(int indice) {
    int panel = sidebar_derecho_ancho();
    int x0 = g.ancho - panel + SIDEBAR_MARGEN;
    int y0 = AREA_SUP_ALTO + TILES_SELECTOR_TITULO_ALTO;
    int columna = indice % TILES_SELECTOR_COLUMNAS;
    int fila = indice / TILES_SELECTOR_COLUMNAS;
    return zona_hacer(x0 + columna * (TILE_MINI_LADO + TILE_MINI_HUECO),
                      y0 + fila * (TILE_MINI_LADO + TILE_MINI_HUECO),
                      TILE_MINI_LADO, TILE_MINI_LADO);
}

Zona capa_fila_zona(int indice) {
    int panel = sidebar_derecho_ancho();
    int x0 = g.ancho - panel + SIDEBAR_MARGEN;
    int y0 = AREA_SUP_ALTO + CAPAS_TITULO_ALTO + TILES_SELECTOR_ALTO;
    return zona_hacer(x0, y0 + indice * (CAPA_FILA_ALTO + CAPA_FILA_HUECO),
                      panel - SIDEBAR_MARGEN * 2, CAPA_FILA_ALTO);
}

Zona botones_capa_zona(int *mas, int *menos) {
    Zona barra = barra_abajo_zona();
    int y = g.alto - 26;
    if (y < zona_abajo(barra)) y = zona_abajo(barra);
    if (mas) *mas = 0;
    if (menos) *menos = 1;
    return zona_hacer(g.ancho - sidebar_derecho_ancho() + SIDEBAR_MARGEN, y, TILES_REJILLA_ANCHO, 22);
}

/* Zonas del pie del panel derecho: boton + (anadir capa) y x (borrar). */
int botones_panel_derecho_zona(Zona *mas, Zona *borrar) {
    int panel = sidebar_derecho_ancho();
    int total = BOTON_LADO * 2 + HUECO;
    int y, x;
    if (panel < total + SIDEBAR_MARGEN * 2) return 0;
    y = g.alto - PANEL_DERECHO_MARGEN_INFERIOR - BOTON_LADO;
    if (y < AREA_SUP_ALTO) return 0;
    x = g.ancho - panel + (panel - total) / 2;
    if (mas) *mas = zona_hacer(x, y, BOTON_LADO, BOTON_LADO);
    if (borrar) *borrar = zona_hacer(x + BOTON_LADO + HUECO, y, BOTON_LADO, BOTON_LADO);
    return 1;
}

/* Boton de ojo dentro de una fila de capa, alineado a la derecha. */
Zona capa_ojo_zona(Zona fila) {
    int lado = CAPA_OJO_LADO;
    if (lado > fila.alto - 4) lado = fila.alto - 4;
    if (lado < 8) lado = 8;
    return zona_hacer(zona_derecha(fila) - 4 - lado,
                      fila.y + (fila.alto - lado) / 2, lado, lado);
}

/* Cuadro indicador del canal alfa, a la izquierda del ojo. */
Zona capa_alfa_zona(Zona fila) {
    int lado = CAPA_ALFA_LADO;
    Zona ojo = capa_ojo_zona(fila);
    if (lado > fila.alto - 6) lado = fila.alto - 6;
    if (lado < 8) lado = 8;
    return zona_hacer(ojo.x - 4 - lado, fila.y + (fila.alto - lado) / 2, lado, lado);
}

/* Menu contextual de canal alfa. Devuelve el panel y deja las dos opciones. */
Zona menu_capa_panel_zona(Zona *anadir, Zona *quitar) {
    int ancho_panel = MENU_CAPA_ANCHO;
    int alto_panel = MENU_CAPA_MARGEN * 2 + MENU_CAPA_OPCION_ALTO * 2;
    int x, y;
    Zona panel;
    if (ancho_panel > g.ancho - 8) ancho_panel = g.ancho - 8;
    if (ancho_panel < 60) ancho_panel = 60;
    x = g.capa_menu_x;
    if (x + ancho_panel > g.ancho - 4) x = g.ancho - ancho_panel - 4;
    y = g.capa_menu_y;
    if (y + alto_panel > g.alto - 4) y = g.alto - alto_panel - 4;
    if (x < 4) x = 4;
    if (y < AREA_SUP_ALTO + 4) y = AREA_SUP_ALTO + 4;
    panel = zona_hacer(x, y, ancho_panel, alto_panel);
    if (anadir) *anadir = zona_hacer(panel.x + MENU_CAPA_MARGEN,
                                     panel.y + MENU_CAPA_MARGEN,
                                     panel.ancho - MENU_CAPA_MARGEN * 2,
                                     MENU_CAPA_OPCION_ALTO);
    if (quitar) *quitar = zona_hacer(panel.x + MENU_CAPA_MARGEN,
                                     panel.y + MENU_CAPA_MARGEN + MENU_CAPA_OPCION_ALTO,
                                     panel.ancho - MENU_CAPA_MARGEN * 2,
                                     MENU_CAPA_OPCION_ALTO);
    return panel;
}

/* Ventana modal de confirmacion y sus dos botones. */
Zona confirmar_panel_zona(Zona *si, Zona *no) {
    int ancho_panel = CONFIRMAR_ANCHO;
    int alto_panel = CONFIRMAR_ALTO;
    int boton_ancho = 76, boton_alto = 26, hueco = 24, total, x, y;
    Zona panel;
    if (ancho_panel > g.ancho - 16) ancho_panel = g.ancho - 16;
    if (ancho_panel < 180) ancho_panel = 180;
    if (alto_panel > g.alto - 16) alto_panel = g.alto - 16;
    if (alto_panel < 100) alto_panel = 100;
    panel = zona_hacer((g.ancho - ancho_panel) / 2, (g.alto - alto_panel) / 2,
                       ancho_panel, alto_panel);
    total = boton_ancho * 2 + hueco;
    x = panel.x + (panel.ancho - total) / 2;
    y = zona_abajo(panel) - 14 - boton_alto;
    if (si) *si = zona_hacer(x, y, boton_ancho, boton_alto);
    if (no) *no = zona_hacer(x + boton_ancho + hueco, y, boton_ancho, boton_alto);
    return panel;
}

/* ------------------------------------------------------------------ */
/* Barra de menus                                                      */
/* ------------------------------------------------------------------ */
/* Las etiquetas se rasterizan con la fuente normal del sistema
 * ("Segoe UI" a 15 px, calidad antialias) y se mezcla la cobertura alfa.
 * Se hace con GDI para que anchos y posiciones sean exactos. */
#define MENU_FUENTE_SISTEMA L"Segoe UI"
#define MENU_TAMANO_SISTEMA 15

static HFONT menu_fuente_sistema(void) {
    static HFONT fuente = NULL;
    if (!fuente) {
        fuente = CreateFontW(-MENU_TAMANO_SISTEMA, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH, MENU_FUENTE_SISTEMA);
    }
    return fuente;
}

static void menu_dc_abrir(HDC *dc, HFONT *anterior) {
    *dc = CreateCompatibleDC(NULL);
    *anterior = (HFONT)SelectObject(*dc, menu_fuente_sistema());
    SetBkMode(*dc, TRANSPARENT);
    SetTextColor(*dc, RGB(255, 255, 255));
}

/* Los textos de la interfaz estan en UTF-8. GDI mide y dibuja en UTF-16,
 * con la misma idea que `rasterizar_texto`. */
static int texto_utf16(const char *texto, WCHAR *dst, int maximo) {
    int n;
    if (!texto || maximo < 1) return 0;
    n = MultiByteToWideChar(CP_UTF8, 0, texto, -1, dst, maximo);
    if (n <= 0) {
        dst[0] = 0;
        return 0;
    }
    return n - 1;
}

#define MENU_CACHE_TEXTO 24
#define MENU_CACHE_LARGO 96

static int texto_menu_ancho(const char *texto) {
    static char cache_texto[MENU_CACHE_TEXTO][MENU_CACHE_LARGO];
    static int cache_ancho[MENU_CACHE_TEXTO];
    static int cache_total = 0;
    HDC dc;
    HFONT anterior;
    SIZE tam;
    WCHAR utf16[256];
    int i, n;
    if (!texto) return 0;
    for (i = 0; i < cache_total; i++) {
        if (strcmp(cache_texto[i], texto) == 0) return cache_ancho[i];
    }
    n = texto_utf16(texto, utf16, 256);
    menu_dc_abrir(&dc, &anterior);
    GetTextExtentPoint32W(dc, utf16, n, &tam);
    SelectObject(dc, anterior);
    DeleteDC(dc);
    if (tam.cx < 0) tam.cx = 0;
    if (cache_total < MENU_CACHE_TEXTO && (int)strlen(texto) < MENU_CACHE_LARGO) {
        strcpy(cache_texto[cache_total], texto);
        cache_ancho[cache_total] = tam.cx;
        cache_total++;
    }
    return tam.cx;
}

static int texto_menu_alto(void) {
    static int alto = 0;
    HDC dc;
    HFONT anterior;
    SIZE tam;
    WCHAR utf16[4];
    int n;
    if (alto > 0) return alto;
    menu_dc_abrir(&dc, &anterior);
    n = texto_utf16("Ag", utf16, 4);
    GetTextExtentPoint32W(dc, utf16, n, &tam);
    SelectObject(dc, anterior);
    DeleteDC(dc);
    alto = tam.cy > MENU_TAMANO_SISTEMA ? tam.cy : MENU_TAMANO_SISTEMA;
    if (alto < 1) alto = 1;
    return alto;
}

/* Dibuja una etiqueta de menu con la cobertura alfa de GDI, como
 * `dibujar_texto_menu`. */
static void dibujar_texto_menu(int x, int y, const char *texto, u32 color) {
    HDC dc;
    HFONT fuente_anterior;
    HBITMAP dib, mapa_anterior;
    BITMAPINFO bmi;
    void *bits = NULL;
    u32 *pix;
    int ancho = texto_menu_ancho(texto);
    int alto = texto_menu_alto();
    int dx, dy, n;
    WCHAR utf16[512];

    if (ancho < 1 || alto < 1 || !texto || !texto[0]) return;
    dc = CreateCompatibleDC(NULL);
    if (!dc) return;
    fuente_anterior = (HFONT)SelectObject(dc, menu_fuente_sistema());
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = ancho;
    bmi.bmiHeader.biHeight = -alto;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    dib = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!dib || !bits) {
        SelectObject(dc, fuente_anterior);
        DeleteDC(dc);
        return;
    }
    mapa_anterior = (HBITMAP)SelectObject(dc, dib);
    memset(bits, 0, (size_t)ancho * (size_t)alto * 4);
    n = texto_utf16(texto, utf16, 512);
    TextOutW(dc, 0, 0, utf16, n);

    pix = (u32 *)bits;
    for (dy = 0; dy < alto; dy++) {
        for (dx = 0; dx < ancho; dx++) {
            u32 p = pix[dy * ancho + dx];
            int cobertura = (int)(p & 0xFF);
            int verde = (int)((p >> 8) & 0xFF);
            int rojo = (int)((p >> 16) & 0xFF);
            if (verde > cobertura) cobertura = verde;
            if (rojo > cobertura) cobertura = rojo;
            if (cobertura <= 0) continue;
            dibujar_pixel(x + dx, y + dy,
                          color_mezclar(dibujar_leer(x + dx, y + dy),
                                        0xFF000000u | (color & 0x00FFFFFFu), cobertura));
        }
    }

    SelectObject(dc, mapa_anterior);
    DeleteObject(dib);
    SelectObject(dc, fuente_anterior);
    DeleteDC(dc);
}

static int menu_ancho_etiqueta(const char *texto) {
    return texto_menu_ancho(texto) + MENU_PADDING * 2;
}

static Zona menu_etiqueta_zona(int indice, int *x_salida) {
    int x = MENU_MARGEN;
    int i;
    for (i = 0; i < 8; i++) {
        int ancho = menu_ancho_etiqueta(MENU_ETIQUETAS[i]);
        if (i == indice) {
            if (x_salida) *x_salida = x;
            return zona_hacer(x, TAB_BARRA_ALTO, ancho, MENU_BARRA_ALTO);
        }
        x += ancho;
    }
    if (x_salida) *x_salida = x;
    return zona_hacer(x, TAB_BARRA_ALTO, 0, MENU_BARRA_ALTO);
}

int menu_barra_en(int x, int y) {
    int i;
    for (i = 0; i < 8; i++) {
        Zona z = menu_etiqueta_zona(i, NULL);
        if (zona_derecha(z) > g.ancho) return -1;
        if (zona_contiene(z, x, y)) return i;
    }
    return -1;
}

static const char *EDITAR_TEXTO[5] = {
    "Deshacer", "Rehacer", "Cortar", "Copiar", "Pegar"
};
static const char *EDITAR_ATAJO[5] = {
    "Ctrl+Z", "Ctrl+Y", "Ctrl+X", "Ctrl+C", "Ctrl+V"
};
static const char *SELECCIONAR_TEXTO[3] = {
    "Todo", "Nada", "Invertir"
};
static const char *SELECCIONAR_ATAJO[3] = {
    "Ctrl+A", "May\u00fas+Ctrl+A", "Ctrl+I"
};

#define MENU_ARCHIVO_ANCHO  210
#define MENU_ARCHIVO_ALTO   132
#define MENU_ARCHIVO_MAPA   196
#define MENU_FILA_ALTO      30
#define MENU_ARCHIVO_PASO   32

int menu_num_filas(int menu) {
    switch (menu) {
        case 0: return g.vista == VISTA_MAPA ? 6 : 4;
        case 1: return 4;
        case 2: return 5;
        case 4: return 3;
        default: return 0;
    }
}

Zona menu_panel_zona(int menu) {
    Zona etiqueta;
    int x = 0, interior, ancho, i;
    if (menu < 0 || menu >= 8) return zona_vacia();
    etiqueta = menu_etiqueta_zona(menu, &x);
    switch (menu) {
        case 0:
            return zona_hacer(etiqueta.x, zona_abajo(etiqueta), MENU_ARCHIVO_ANCHO,
                              g.vista == VISTA_MAPA ? MENU_ARCHIVO_MAPA : MENU_ARCHIVO_ALTO);
        case 1:
            ancho = texto_menu_ancho("Visualizar patron de textura") + 16;
            interior = g.ancho - etiqueta.x - 2;
            if (interior < 40) interior = 40;
            if (ancho > interior) ancho = interior;
            return zona_hacer(etiqueta.x, zona_abajo(etiqueta), ancho,
                              3 + MENU_FILA_ALTO * 4 + 3);
        case 2:
            interior = 0;
            for (i = 0; i < 5; i++) {
                int w = texto_menu_ancho(EDITAR_TEXTO[i]) + texto_menu_ancho(EDITAR_ATAJO[i]);
                if (w > interior) interior = w;
            }
            ancho = interior + 8 + 8 + 24;
            interior = g.ancho - etiqueta.x - 2;
            if (interior < 80) interior = 80;
            if (ancho > interior) ancho = interior;
            return zona_hacer(etiqueta.x, zona_abajo(etiqueta), ancho,
                              3 + MENU_FILA_ALTO * 5 + 3);
        case 4:
            interior = 0;
            for (i = 0; i < 3; i++) {
                int w = texto_menu_ancho(SELECCIONAR_TEXTO[i]) +
                        texto_menu_ancho(SELECCIONAR_ATAJO[i]);
                if (w > interior) interior = w;
            }
            ancho = interior + 8 + 8 + 24;
            interior = g.ancho - etiqueta.x - 2;
            if (interior < 80) interior = 80;
            if (ancho > interior) ancho = interior;
            return zona_hacer(etiqueta.x, zona_abajo(etiqueta), ancho,
                              3 + MENU_FILA_ALTO * 3 + 3);
        default:
            return zona_vacia();
    }
}

Zona menu_desplegable_zona(void) {
    return menu_panel_zona(g.menu_abierto);
}

Zona menu_fila_zona(int menu, int fila) {
    Zona panel = menu_panel_zona(menu);
    int paso = (menu == 0) ? MENU_ARCHIVO_PASO : MENU_FILA_ALTO;
    if (panel.ancho <= 0) return zona_vacia();
    return zona_hacer(panel.x + 2, panel.y + 3 + fila * paso,
                      panel.ancho - 4, MENU_FILA_ALTO);
}

int menu_fila_en(int x, int y) {
    int n, i;
    if (g.menu_abierto < 0) return -1;
    n = menu_num_filas(g.menu_abierto);
    for (i = 0; i < n; i++) {
        if (zona_contiene(menu_fila_zona(g.menu_abierto, i), x, y)) return i;
    }
    return -1;
}

int menu_fila_activa(int menu, int fila) {
    switch (menu) {
        case 0:
            if (g.vista == VISTA_MAPA && fila == 1) return g.mapa_ruta[0] != 0;
            return 1;
        case 1:
            return 1;
        case 2:
            if (fila == 0) return g.deshacer.num > 0;
            if (fila == 1) return g.rehacer.num > 0;
            if (fila == 2 || fila == 3) return g.sel.activa;
            if (fila == 4) return g.portapapeles != NULL;
            return 0;
        case 4:
            if (fila == 0) return 1;
            if (fila == 1 || fila == 2) return g.sel.activa;
            return 0;
        default:
            return 0;
    }
}

/* Dibuja una fila del desplegable con la fuente del sistema. */
static void dibujar_fila_menu(Zona z, const char *texto, const char *atajo,
                              int hover, int activo, int color_apagado) {
    int ty = z.y + (z.alto - texto_menu_alto()) / 2;
    u32 color;
    if (hover) dibujar_rect_z(z, MENU_BARRA_HOVER);
    if (!activo) color = color_apagado;
    else if (hover) color = 0x00FFFFFFu;
    else color = MENU_TEXTO;
    dibujar_texto_menu(z.x + 8, ty, texto, color);
    if (atajo && atajo[0]) {
        int ax = zona_derecha(z) - 8 - texto_menu_ancho(atajo);
        dibujar_texto_menu(ax, ty, atajo,
                           !activo ? 0x00B0B0B0u : (hover ? 0x00FFFFFFu : 0x00606060u));
    }
}

/* ------------------------------------------------------------------ */
/* Pestanas: geometria y dibujo                                        */
/* ------------------------------------------------------------------ */
Zona pestanas_zona(void) {
    return zona_hacer(0, 0, g.ancho, TAB_BARRA_ALTO);
}

/* Ancho de una pestana. Se reparte el espacio disponible entre todas para que
 * nunca tapen el boton de nueva pestana, con un minimo y un maximo. */
static int pestana_ancho(int indice) {
    int n = g.num_pestanas > 0 ? g.num_pestanas : 1;
    int disponible = g.ancho - TAB_MARGEN * 2 - TAB_NUEVO_LADO - 12 -
                     (n - 1) * TAB_HUECO;
    int maximo = disponible / n;
    int ancho;
    const char *titulo = pestanas_titulo(indice);
    if (maximo > TAB_ANCHO_MAX) maximo = TAB_ANCHO_MAX;
    if (maximo < TAB_ANCHO_MIN) maximo = TAB_ANCHO_MIN;
    /* favicon + margen + texto + punto de modificado + cerrar */
    ancho = texto_ancho(titulo) + 61;
    if (ancho < TAB_ANCHO_MIN) ancho = TAB_ANCHO_MIN;
    if (ancho > maximo) ancho = maximo;
    return ancho;
}

static int pestana_x(int indice) {
    int i, x = TAB_MARGEN;
    for (i = 0; i < indice; i++) x += pestana_ancho(i) + TAB_HUECO;
    return x;
}

Zona pestana_zona(int indice) {
    int ancho;
    if (indice < 0 || indice >= g.num_pestanas) return zona_vacia();
    ancho = pestana_ancho(indice);
    return zona_hacer(pestana_x(indice), 4, ancho, TAB_BARRA_ALTO - 4);
}

Zona pestana_cerrar_zona(int indice) {
    Zona t = pestana_zona(indice);
    int lado = TAB_CERRAR_LADO;
    if (t.ancho <= 0) return zona_vacia();
    return zona_hacer(zona_derecha(t) - lado - 6,
                      t.y + (t.alto - lado) / 2, lado, lado);
}

Zona pestana_nueva_zona(void) {
    int x = TAB_MARGEN;
    int i;
    for (i = 0; i < g.num_pestanas; i++) x += pestana_ancho(i) + TAB_HUECO;
    x += 6;
    return zona_hacer(x, (TAB_BARRA_ALTO - TAB_NUEVO_LADO) / 2,
                      TAB_NUEVO_LADO, TAB_NUEVO_LADO);
}

int pestana_en(int x, int y) {
    int i;
    for (i = 0; i < g.num_pestanas; i++) {
        if (zona_contiene(pestana_zona(i), x, y)) return i;
    }
    return -1;
}

/* Borde izquierdo del trapecio para una fila local. La parte superior se
 * redondea y la base se abre con una curva concava hasta el ancho completo. */
static int tab_izquierda(int ancho, int alto, int y) {
    int s = TAB_LADO_INSET;
    int r = TAB_RADIO_SUP;
    int b = TAB_CURVA_BASE;
    if (ancho <= 0 || alto <= 0) return 0;
    if (y < 0) y = 0;
    if (y >= alto) y = alto - 1;
    if (b > alto / 2) b = alto / 2;
    if (r > alto / 2) r = alto / 2;
    if (y < r) {
        int dy = r - y;
        int dentro = r * r - dy * dy;
        int f = (int)(sqrtf((float)dentro) + 0.5f);
        return s + r - f;
    }
    if (y < alto - b) return s;
    {
        int k = y - (alto - b);
        float t = (float)k / (float)(b > 0 ? b : 1);
        float inv = 1.0f - t;
        return (int)((float)s * inv * inv + 0.5f);
    }
}

/* Rellena el trapecio y dibuja su contorno. La base queda abierta para que la
 * pestana se funda con la barra inferior. */
static void dibujar_forma_pestana(Zona t, int activa, u32 fondo) {
    int y;
    u32 borde = activa ? TAB_ACTIVA_BORDE : TAB_INACTIVA_BORDE;
    for (y = 0; y < t.alto; y++) {
        int xl = tab_izquierda(t.ancho, t.alto, y);
        int xr = t.ancho - 1 - xl;
        int py = t.y + y;
        if (xr <= xl) continue;
        dibujar_rect(t.x + xl, py, t.x + xr + 1, py + 1, fondo);
        if (y == 0) {
            dibujar_rect(t.x + xl, py, t.x + xr + 1, py + 1, borde);
        } else {
            dibujar_rect(t.x + xl, py, t.x + xl + 1, py + 1, borde);
            dibujar_rect(t.x + xr, py, t.x + xr + 1, py + 1, borde);
            if (!activa && y == 1)
                dibujar_rect(t.x + xl + 1, py, t.x + xr, py + 1,
                             TAB_BARRA_LUZ);
        }
    }
}

/* Favicon: pequena muestra de textura a cuadros, como una miniatura del tile. */
static void dibujar_favicon(int x, int y, int activa) {
    u32 a = activa ? TAB_FAVICON_A : 0x008C9EAEu;
    u32 b = activa ? TAB_FAVICON_B : 0x00D6E0EAu;
    u32 marco = activa ? 0x00788CA0u : 0x0098A8B8u;
    dibujar_rect(x, y, x + 6, y + 6, a);
    dibujar_rect(x + 6, y, x + 12, y + 6, b);
    dibujar_rect(x, y + 6, x + 6, y + 12, b);
    dibujar_rect(x + 6, y + 6, x + 12, y + 12, a);
    dibujar_marco(x, y, x + 12, y + 12, marco);
}

/* Copia el titulo recortandolo con puntos suspensivos si no cabe. */
static void titulo_pestana_recortado(char *dst, int tam, const char *src,
                                     int max_ancho) {
    int n;
    if (!dst || tam < 1) return;
    dst[0] = 0;
    if (!src) return;
    if (texto_ancho(src) <= max_ancho) {
        recortar_texto(dst, src, tam);
        return;
    }
    n = (int)strlen(src);
    if (n > tam - 4) n = tam - 4;
    while (n > 0) {
        char prueba[160];
        n--;
        if (n > (int)sizeof(prueba) - 4) n = (int)sizeof(prueba) - 4;
        memcpy(prueba, src, (size_t)n);
        prueba[n] = 0;
        strcat(prueba, "...");
        if (texto_ancho(prueba) <= max_ancho) {
            recortar_texto(dst, prueba, tam);
            return;
        }
    }
    recortar_texto(dst, "...", tam);
}

static void dibujar_cruz(int x, int y, int lado, u32 color) {
    int m = lado >= 12 ? 4 : 3;
    dibujar_linea(x + m, y + m, x + lado - m - 1, y + lado - m - 1, color);
    dibujar_linea(x + lado - m - 1, y + m, x + m, y + lado - m - 1, color);
}

void ui_dibujar_pestanas(void) {
    int i;
    int mouse_x = g.raton_x, mouse_y = g.raton_y;
    int raton_en_barra = (mouse_y >= 0 && mouse_y < TAB_BARRA_ALTO);
    int tab_hover = raton_en_barra ? pestana_en(mouse_x, mouse_y) : -1;
    Zona nueva = pestana_nueva_zona();

    /* Fondo de la franja y linea de union con la barra de menus. */
    dibujar_rect(0, 0, g.ancho, TAB_BARRA_ALTO, TAB_BARRA_FONDO);
    dibujar_rect(0, 0, g.ancho, 1, TAB_BARRA_LUZ);
    dibujar_rect(0, TAB_BARRA_ALTO - 2, g.ancho, TAB_BARRA_ALTO,
                 TAB_BARRA_BORDE);

    for (i = 0; i < g.num_pestanas; i++) {
        Zona t = pestana_zona(i);
        Zona cerrar = pestana_cerrar_zona(i);
        int activa = (i == g.pestana_activa);
        int hover = (i == tab_hover);
        int sobre_cerrar = raton_en_barra &&
                           zona_contiene(cerrar, mouse_x, mouse_y);
        int modificada = pestanas_esta_modificada(i);
        char titulo[160];
        u32 fondo = activa ? TAB_ACTIVA_FONDO
                           : (hover ? TAB_HOVER_FONDO : TAB_INACTIVA_FONDO);
        u32 texto = activa ? TAB_ACTIVA_TEXTO : TAB_INACTIVA_TEXTO;
        int fav_x, texto_x, texto_max;

        if (t.ancho <= 0 || t.x >= g.ancho) break;

        dibujar_forma_pestana(t, activa, fondo);

        fav_x = t.x + TAB_LADO_INSET + 5;
        dibujar_favicon(fav_x, t.y + (t.alto - 12) / 2, activa);

        texto_x = fav_x + 12 + 6;
        texto_max = cerrar.x - texto_x - 6;
        if (modificada) texto_max -= 9;
        if (texto_max < 6) texto_max = 6;
        titulo_pestana_recortado(titulo, sizeof(titulo),
                                 pestanas_titulo(i), texto_max);
        dibujar_texto(texto_x, t.y + (t.alto - texto_alto()) / 2,
                      titulo, texto);

        if (modificada) {
            int mx = cerrar.x - 8;
            int my = t.y + t.alto / 2;
            dibujar_circulo(mx, my, 2, activa ? 0x00507090u : texto, 1);
        }

        if (sobre_cerrar) {
            int cx = cerrar.x + cerrar.ancho / 2;
            int cy = cerrar.y + cerrar.alto / 2;
            dibujar_circulo(cx, cy, 8, TAB_CERRAR_HOVER, 1);
        }
        dibujar_cruz(cerrar.x, cerrar.y, cerrar.ancho,
                     sobre_cerrar ? 0x00FFFFFFu : texto);
    }

    /* Boton circular de nueva pestana. */
    if (zona_derecha(nueva) < g.ancho - 2) {
        int hover = raton_en_barra && zona_contiene(nueva, mouse_x, mouse_y);
        int cx = nueva.x + nueva.ancho / 2;
        int cy = nueva.y + nueva.alto / 2;
        u32 color = hover ? TAB_ACTIVA_TEXTO : TAB_INACTIVA_TEXTO;
        dibujar_circulo(cx, cy, 9, hover ? TAB_ACTIVA_BORDE : TAB_BARRA_BORDE, 1);
        dibujar_circulo(cx, cy, 8,
                        hover ? TAB_HOVER_FONDO : TAB_BARRA_FONDO, 1);
        dibujar_rect(cx - 5, cy - 1, cx + 6, cy + 1, color);
        dibujar_rect(cx - 1, cy - 5, cx + 1, cy + 6, color);
    }
}

/* ------------------------------------------------------------------ */
/* Barra de menus: dibujo                                               */
/* ------------------------------------------------------------------ */
void ui_dibujar_barra_menus(void) {
    int i;
    int mouse_en = menu_barra_en(g.raton_x, g.raton_y);

    dibujar_rect(0, TAB_BARRA_ALTO, g.ancho, AREA_SUP_ALTO, MENU_BARRA_FONDO);

    for (i = 0; i < 8; i++) {
        Zona z = menu_etiqueta_zona(i, NULL);
        int hover = (i == mouse_en);
        if (zona_derecha(z) > g.ancho) break;
        if (hover) {
            dibujar_rect_z(z, MENU_BARRA_HOVER);
            dibujar_texto_menu(z.x + (z.ancho - texto_menu_ancho(MENU_ETIQUETAS[i])) / 2,
                               z.y + (z.alto - texto_menu_alto()) / 2,
                               MENU_ETIQUETAS[i], MENU_BARRA_HOVER_TXT);
        } else {
            dibujar_texto_menu(z.x + (z.ancho - texto_menu_ancho(MENU_ETIQUETAS[i])) / 2,
                               z.y + (z.alto - texto_menu_alto()) / 2,
                               MENU_ETIQUETAS[i], MENU_BARRA_TEXTO);
        }
    }
    dibujar_rect(0, AREA_SUP_ALTO - 1, g.ancho, AREA_SUP_ALTO, BARRA_BORDE);
}

static void nombre_archivo_mapa(char *dst, int maximo) {
    const char *p = g.mapa_ruta;
    const char *nombre = p;
    if (!dst || maximo < 1) return;
    while (*p) {
        if (*p == '\\' || *p == '/') nombre = p + 1;
        p++;
    }
    {
        size_t largo = strlen(nombre);
        if (largo > (size_t)maximo - 1) largo = (size_t)maximo - 1;
        memcpy(dst, nombre, largo);
        dst[largo] = 0;
    }
}

void ui_dibujar_menu_desplegable(void) {
    Zona panel;
    int menu = g.menu_abierto;
    int n, i, mapa;
    if (menu < 0) return;
    panel = menu_panel_zona(menu);
    if (panel.ancho <= 0) return;
    n = menu_num_filas(menu);
    mapa = (g.vista == VISTA_MAPA);

    dibujar_rect(panel.x + 2, panel.y + 2, zona_derecha(panel) + 2, zona_abajo(panel) + 2, 0x00A0A0A0u);
    dibujar_rect_z(panel, MENU_FONDO);
    dibujar_marco_z(panel, MENU_BORDE);

    for (i = 0; i < n; i++) {
        Zona z = menu_fila_zona(menu, i);
        int activo = menu_fila_activa(menu, i);
        int hover = activo && zona_contiene(z, g.raton_x, g.raton_y);
        const char *texto = "";
        char sobrescribir[128];
        char archivo[96];

        switch (menu) {
            case 0:
                if (mapa) {
                    switch (i) {
                        case 0: texto = "Guardar mapa .mav"; break;
                        case 1:
                            nombre_archivo_mapa(archivo, sizeof(archivo));
                            if (activo)
                                _snprintf(sobrescribir, sizeof(sobrescribir),
                                          "Sobrescribir %s", archivo);
                            else
                                strcpy(sobrescribir, "Sobrescribir (no disponible)");
                            sobrescribir[sizeof(sobrescribir) - 1] = 0;
                            texto = sobrescribir;
                            break;
                        case 2: texto = "Abrir"; break;
                        case 3: texto = "Borrar todo"; break;
                        case 4: texto = "Exportar a PNG"; break;
                        default: texto = "Exportar a BMP"; break;
                    }
                } else {
                    switch (i) {
                        case 0: texto = "Abrir"; break;
                        case 1: texto = "Borrar todo"; break;
                        case 2: texto = "Exportar a PNG"; break;
                        default: texto = "Exportar a BMP"; break;
                    }
                }
                dibujar_fila_menu(z, texto, NULL, hover, activo, 0x00A0A0A0u);
                break;
            case 1:
                if (i == 0) texto = "Modo pintar";
                else if (i == 1) texto = "Visualizar patron de textura";
                else if (i == 2) texto = "Modo tiles";
                else texto = "Editor de mapas";
                if (hover) dibujar_rect_z(z, MENU_BARRA_HOVER);
                if ((i == 0 && g.vista == VISTA_PINTAR && !g.modo_tiles) ||
                    (i == 2 && g.modo_tiles) || (i == 3 && mapa)) {
                    dibujar_rect(z.x + 1, z.y + 6, z.x + 4, zona_abajo(z) - 6,
                                 hover ? MENU_BARRA_HOVER_TXT : BOTON_SELECCION);
                }
                dibujar_texto_menu(z.x + 8, z.y + (z.alto - texto_menu_alto()) / 2,
                                   texto, hover ? 0x00FFFFFFu : MENU_TEXTO);
                break;
            case 2:
                dibujar_fila_menu(z, EDITAR_TEXTO[i], EDITAR_ATAJO[i], hover, activo,
                                  0x00909090u);
                break;
            case 4:
                dibujar_fila_menu(z, SELECCIONAR_TEXTO[i], SELECCIONAR_ATAJO[i], hover,
                                  activo, 0x00909090u);
                break;
            default:
                break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Sidebar de herramientas                                             */
/* ------------------------------------------------------------------ */
static void dibujar_icono_herramienta(int i, Zona z) {
    u32 *pix;
    int w = 0, h = 0;
    int escala, dw, dh, x0, y0, dx, dy;
    pix = icono_herramienta(i, &w, &h);
    if (pix && w > 0 && h > 0) {
        /* Mismo escalado entero que `dibujar_icono`: el icono conserva su
         * tamano nativo si cabe y se centra en el boton. */
        int ew = ICONO_CAJA / w;
        int eh = ICONO_CAJA / h;
        escala = ew < eh ? ew : eh;
        if (escala < 1) escala = 1;
        dw = w * escala;
        dh = h * escala;
        x0 = z.x + (z.ancho - dw) / 2;
        y0 = z.y + (z.alto - dh) / 2;
        for (dy = 0; dy < dh; dy++) {
            for (dx = 0; dx < dw; dx++) {
                u32 c = pix[(size_t)(dy / escala) * w + (dx / escala)];
                int alfa = (int)(c >> 24);
                if (alfa == 0) continue;
                dibujar_pixel(x0 + dx, y0 + dy,
                              color_mezclar(dibujar_leer(x0 + dx, y0 + dy), c, alfa));
            }
        }
    } else {
        char inicial[2];
        inicial[0] = NOMBRES_HERRAMIENTA[i][0];
        inicial[1] = 0;
        dibujar_texto_centrado(z, inicial, TEXTO_UI);
    }
}

/* Parte el nombre de la herramienta en una o dos lineas, en mayusculas,
 * con la logica de `lineas_nombre_herramienta`. */
static int lineas_nombre_herramienta(const char *nombre, int ancho, char lineas[2][64]) {
    int maximo = (ancho + 1) / 6;
    int nlineas = 0;
    char actual[64];
    const char *p = nombre;
    actual[0] = 0;
    if (maximo < 1) maximo = 1;
    while (*p) {
        const char *ini;
        char palabra[64];
        char prueba[128];
        int plen = 0, i;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        ini = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        plen = (int)(p - ini);
        if (plen > maximo) plen = maximo;
        for (i = 0; i < plen; i++) {
            char c = ini[i];
            if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            palabra[i] = c;
        }
        palabra[plen] = 0;
        if (actual[0] == 0) {
            strcpy(prueba, palabra);
        } else {
            _snprintf(prueba, sizeof(prueba), "%s %s", actual, palabra);
            prueba[sizeof(prueba) - 1] = 0;
        }
        if (fuente_texto_ancho(prueba) <= ancho || actual[0] == 0) {
            strcpy(actual, prueba);
        } else {
            strcpy(lineas[nlineas++], actual);
            strcpy(actual, palabra);
        }
        if (nlineas == 2) break;
    }
    if (actual[0] && nlineas < 2) strcpy(lineas[nlineas++], actual);
    return nlineas;
}

/* ------------------------------------------------------------------ */
/* Circulo de textura y su menu cuadrado                               */
/* ------------------------------------------------------------------ */
static u32 ruido_textura(int x, int y) {
    u32 h = (u32)x * 0x27D4EB2Du ^ (u32)y * 0x165667B1u;
    h ^= h >> 15;
    h *= 0x2545F491u;
    return h ^ (h >> 16);
}

int textura_juego_actual(void) {
    return g.herramienta == H_AEROGRAFO ? 1 : 0;
}

static int total_juego(int juego) {
    return juego ? ROCIO_TOTAL : TEXTURA_TOTAL;
}

static int muestras_en_fila(int fila, int juego) {
    int restantes = total_juego(juego) - fila * MENU_COLUMNAS;
    if (restantes < 0) restantes = 0;
    if (restantes > MENU_COLUMNAS) restantes = MENU_COLUMNAS;
    return restantes;
}

Zona textura_menu_zona(void) {
    Zona c = panel_textura_zona();
    return zona_hacer(zona_derecha(c) + MENU_HUECO,
                      c.y + (c.alto - MENU_LADO) / 2,
                      MENU_LADO, MENU_LADO);
}

Zona textura_muestra_zona(int indice, int juego) {
    Zona menu = textura_menu_zona();
    int fila = indice / MENU_COLUMNAS;
    int columna = indice % MENU_COLUMNAS;
    int en_fila = muestras_en_fila(fila, juego);
    int usado;
    if (en_fila < 1) en_fila = 1;
    usado = en_fila * MUESTRA_DIAMETRO + (en_fila - 1) * MENU_HUECO_MUESTRA;
    return zona_hacer(menu.x + (MENU_LADO - usado) / 2 +
                          columna * (MUESTRA_DIAMETRO + MENU_HUECO_MUESTRA),
                      menu.y + 6 + fila * (MUESTRA_DIAMETRO + MENU_HUECO_MUESTRA),
                      MUESTRA_DIAMETRO, MUESTRA_DIAMETRO);
}

int textura_muestra_en(int x, int y, int juego) {
    int i, total = total_juego(juego);
    for (i = 0; i < total; i++) {
        Zona z = textura_muestra_zona(i, juego);
        int dx = x - (z.x + z.ancho / 2);
        int dy = y - (z.y + z.alto / 2);
        int r = z.ancho / 2;
        if (dx * dx + dy * dy <= r * r) return i;
    }
    return -1;
}

/* Mezcla de cobertura, como `mezclar`: ignora el canal alfa del color de
 * origen y usa solo la cobertura indicada. */
static void mezclar_cobertura(int x, int y, u32 color, int alfa) {
    u32 dst;
    int a, inv, r, g2, b;
    if (x < 0 || y < 0 || x >= g.ancho || y >= g.alto) return;
    if (alfa > 255) alfa = 255;
    if (alfa <= 0) return;
    a = alfa;
    inv = 255 - a;
    dst = dibujar_leer(x, y);
    r = (int)(((((color >> 16) & 0xFF) * a + ((dst >> 16) & 0xFF) * inv) / 255) & 0xFF);
    g2 = (int)(((((color >> 8) & 0xFF) * a + ((dst >> 8) & 0xFF) * inv) / 255) & 0xFF);
    b = (int)((((color & 0xFF) * a + (dst & 0xFF) * inv) / 255) & 0xFF);
    dibujar_pixel(x, y, 0xFF000000u | ((u32)r << 16) | ((u32)g2 << 8) | (u32)b);
}

static void rellenar_disco(float cx, float cy, float radio, u32 color) {
    int x0, x1, y0, y1, x, y;
    if (radio <= 0.0f) return;
    x0 = (int)floorf(cx - radio - 1.0f);
    x1 = (int)ceilf(cx + radio + 1.0f);
    y0 = (int)floorf(cy - radio - 1.0f);
    y1 = (int)ceilf(cy + radio + 1.0f);
    for (y = y0; y < y1; y++) {
        if (y < 0 || y >= g.alto) continue;
        for (x = x0; x < x1; x++) {
            float dx, dy, distancia, cobertura;
            int alfa;
            if (x < 0 || x >= g.ancho) continue;
            dx = ((float)x + 0.5f) - cx;
            dy = ((float)y + 0.5f) - cy;
            distancia = sqrtf(dx * dx + dy * dy);
            cobertura = radio + 0.5f - distancia;
            if (cobertura < 0.0f) cobertura = 0.0f;
            if (cobertura > 1.0f) cobertura = 1.0f;
            if (cobertura <= 0.0f) continue;
            alfa = (int)roundf(cobertura * 255.0f);
            mezclar_cobertura(x, y, color, alfa);
        }
    }
}

static u32 semilla_textura(int textura) {
    switch (textura) {
        case TEXTURA_CERDA: return 0x5EED0001u;
        case TEXTURA_ACUARELA: return 0x5EED0002u;
        case TEXTURA_TINTA: return 0x5EED0003u;
        case TEXTURA_SECO: return 0x5EED0008u;
        case TEXTURA_AEROGRAFO: return 0x5EED0005u;
        default: return 0x5EED0001u;
    }
}

static u32 semilla_rocio(int rocio) {
    switch (rocio) {
        case ROCIO_FINA: return 0x50C10001u;
        case ROCIO_MEDIA: return 0x50C10002u;
        case ROCIO_GRUESA: return 0x50C10003u;
        case ROCIO_DISPERSA: return 0x50C10004u;
        case ROCIO_GOTEO: return 0x50C10005u;
        default: return 0x50C10001u;
    }
}

static int indice_cerda(float largo, float ancho, float radio, float cerdas, u32 semilla) {
    float onda = (float)(ruido_textura((int)(largo * 0.35f), (int)semilla) % 100) / 100.0f;
    float banda = (ancho / radio + 1.0f) * 0.5f * cerdas + (onda - 0.5f) * 0.9f;
    int k = (int)floorf(banda);
    if (k < 0) k = 0;
    if (k > (int)cerdas - 1) k = (int)cerdas - 1;
    return k;
}

static float carga_cerda(u32 semilla, int cerda, u32 sal) {
    return (float)(ruido_textura((int)(semilla ^ sal), cerda) % 1000) / 999.0f;
}

static float flujo_cerda(float largo, u32 semilla, int cerda) {
    return (float)(ruido_textura((int)(largo * 0.30f) + cerda * 61, (int)semilla) % 100) / 100.0f;
}

/* Color que deja la punta de la textura en un punto del trazo. */
static int pigmento_textura(int textura, float largo, float ancho, float distancia,
                            float radio, u32 semilla, u32 *tono) {
    float r = radio < 0.5f ? 0.5f : radio;
    float borde = distancia / r;
    float anc = ancho / r;
    float centro;
    if (borde < 0.0f) borde = 0.0f;
    if (borde > 1.0f) borde = 1.0f;
    if (anc < -1.0f) anc = -1.0f;
    if (anc > 1.0f) anc = 1.0f;
    centro = 1.0f - fabsf(anc);

    switch (textura) {
        case TEXTURA_CERDA: {
            float cerdas = r * 0.7f;
            int k;
            float carga;
            if (cerdas < 3.0f) cerdas = 3.0f;
            if (cerdas > 14.0f) cerdas = 14.0f;
            k = indice_cerda(largo, ancho, r, cerdas, semilla);
            carga = carga_cerda(semilla, k, 0x00CE2D01u)
                  * (0.55f + 0.45f * flujo_cerda(largo, semilla, k))
                  * (0.78f + 0.22f * centro);
            if (carga < 0.12f) return 0;
            if (carga < 0.32f) *tono = TEXTURA_CLARA;
            else if (carga < 0.54f) *tono = TEXTURA_MEDIA;
            else *tono = TEXTURA_BASE;
            if (borde > 0.78f) {
                int recorte = (int)(ruido_textura((int)(largo * 4.0f) ^ (k * 37),
                                                  (int)(ancho * 6.0f)) % 100);
                if (recorte < 45) return 0;
                if (recorte < 80) { *tono = TEXTURA_CLARA; return 1; }
            }
            return 1;
        }
        case TEXTURA_ACUARELA: {
            float cerdas = r * 0.55f;
            int k;
            float carga;
            if (cerdas < 2.0f) cerdas = 2.0f;
            if (cerdas > 10.0f) cerdas = 10.0f;
            k = indice_cerda(largo, ancho, r, cerdas, semilla);
            carga = carga_cerda(semilla, k, 0x00AC0A01u)
                  * (0.45f + 0.55f * flujo_cerda(largo, semilla, k))
                  * (0.60f + 0.40f * centro);
            if (carga < 0.30f) *tono = TEXTURA_CLARA;
            else if (carga < 0.58f) *tono = TEXTURA_MEDIA;
            else *tono = TEXTURA_BASE;
            if (borde > 0.62f) {
                int azar = (int)(ruido_textura((int)(largo * 3.0f) ^ (k * 29),
                                               (int)(ancho * 5.0f)) % 100);
                if (azar < 25) return 0;
                *tono = TEXTURA_CLARA;
            }
            return 1;
        }
        case TEXTURA_TINTA:
            if (borde > 0.90f) {
                int azar = (int)(ruido_textura((int)(largo * 6.0f),
                                               (int)(ancho * 6.0f)) % 100);
                if (azar < 55) return 0;
            }
            *tono = TEXTURA_BASE;
            return 1;
        case TEXTURA_SECO: {
            float cerdas = r * 0.65f;
            int k;
            float carga, azar;
            if (cerdas < 3.0f) cerdas = 3.0f;
            if (cerdas > 12.0f) cerdas = 12.0f;
            k = indice_cerda(largo, ancho, r, cerdas, semilla);
            carga = carga_cerda(semilla, k, 0x005EC0u)
                  * (0.25f + 0.50f * flujo_cerda(largo, semilla, k))
                  * (0.65f + 0.35f * centro);
            azar = (float)(ruido_textura((int)(largo * 1.3f) ^ (k * 17),
                                         (int)semilla) % 100) / 100.0f;
            if (carga < 0.06f || azar < 0.42f) return 0;
            if (borde > 0.72f) {
                int recorte = (int)(ruido_textura((int)(largo * 5.0f) ^ (k * 53),
                                                  (int)(ancho * 7.0f)) % 100);
                if (recorte < 70) return 0;
            }
            if (carga < 0.35f) *tono = TEXTURA_CLARA;
            else if (carga < 0.60f) *tono = TEXTURA_MEDIA;
            else *tono = TEXTURA_BASE;
            return 1;
        }
        case TEXTURA_AEROGRAFO: {
            float densidad = powf(1.0f - borde, 1.4f) * 0.95f;
            float azar = (float)(ruido_textura((int)(largo * 1.7f) ^ (int)semilla,
                                               (int)(ancho * 1.7f)) % 1000) / 999.0f;
            int t;
            if (azar > densidad) return 0;
            t = (int)(ruido_textura((int)(largo * 2.0f),
                                    (int)(ancho * 2.0f) ^ (int)semilla) % 3);
            *tono = (t == 0) ? TEXTURA_CLARA : (t == 1 ? TEXTURA_MEDIA : TEXTURA_BASE);
            return 1;
        }
        default:
            return 0;
    }
}

static int pigmento_muestra(int textura, float dx, float dy, float radio, u32 *tono) {
    float radio_pincel = radio * 0.42f;
    float mitad;
    float largo, distancia;
    if (radio_pincel < 2.0f) radio_pincel = 2.0f;
    mitad = radio * 0.70f;
    if (mitad < radio_pincel) mitad = radio_pincel;
    if (fabsf(dx) > mitad) return 0;
    if (fabsf(dx) <= mitad - radio_pincel) {
        largo = dx;
        distancia = fabsf(dy);
    } else {
        float ex = fabsf(dx) - (mitad - radio_pincel);
        float d = sqrtf(ex * ex + dy * dy);
        if (d > radio_pincel) return 0;
        largo = dx;
        distancia = d;
    }
    if (distancia > radio_pincel) return 0;
    return pigmento_textura(textura, largo, dy, distancia, radio_pincel,
                            semilla_textura(textura), tono);
}

static void rellenar_textura(float cx, float cy, float radio, int textura) {
    int x0, x1, y0, y1, x, y;
    u32 semilla = semilla_textura(textura);
    rellenar_disco(cx, cy, radio, TEXTURA_FONDO);
    x0 = (int)floorf(cx - radio - 1.0f);
    x1 = (int)ceilf(cx + radio + 1.0f);
    y0 = (int)floorf(cy - radio - 1.0f);
    y1 = (int)ceilf(cy + radio + 1.0f);
    for (y = y0; y < y1; y++) {
        if (y < 0 || y >= g.alto) continue;
        for (x = x0; x < x1; x++) {
            float dx, dy;
            u32 tono;
            (void)semilla;
            if (x < 0 || x >= g.ancho) continue;
            dx = ((float)x + 0.5f) - cx;
            dy = ((float)y + 0.5f) - cy;
            if (dx * dx + dy * dy >= (radio - 0.5f) * (radio - 0.5f)) continue;
            if (pigmento_muestra(textura, dx, dy, radio, &tono))
                dibujar_pixel(x, y, tono);
        }
    }
}

static void gota_en(int rocio, int indice, u32 semilla, u32 paso, float *gx, float *gy) {
    u32 s = semilla ^ (paso * 0x9E3779B9u);
    float azar_x = (float)(ruido_textura(indice, (int)s) % 1000) / 999.0f;
    float azar_y = (float)(ruido_textura(indice ^ 0x1357, (int)(s >> 16)) % 1000) / 999.0f;
    if (rocio == ROCIO_GOTEO) {
        *gx = (azar_x - 0.5f) * 0.7f;
        *gy = -0.4f + azar_y * 1.5f;
    } else {
        float angulo = azar_x * 6.2831855f;
        float raiz = sqrtf(azar_y);
        float radio = raiz * (0.55f + 0.45f * raiz);
        *gx = cosf(angulo) * radio;
        *gy = sinf(angulo) * radio;
    }
}

static void rellenar_rocio(float cx, float cy, float radio, int rocio) {
    static const int GOTAS[ROCIO_TOTAL] = {9, 15, 20, 11, 8};
    static const int LADOS[ROCIO_TOTAL] = {1, 1, 2, 1, 2};
    u32 semilla = semilla_rocio(rocio);
    float alcance = (radio - 2.0f < 1.0f ? 1.0f : radio - 2.0f) * 0.90f;
    float limite = radio - 1.0f;
    float limite2 = limite * limite;
    int lado = LADOS[rocio];
    int indice;
    rellenar_disco(cx, cy, radio, TEXTURA_FONDO);
    for (indice = 0; indice < GOTAS[rocio] * 6; indice++) {
        float nx, ny, px, py, dx, dy;
        u32 tono;
        int inicio_x, inicio_y, gy, gx;
        gota_en(rocio, indice, semilla, 0, &nx, &ny);
        px = cx + nx * alcance;
        py = cy + ny * alcance;
        dx = px - cx;
        dy = py - cy;
        if (dx * dx + dy * dy > limite2) continue;
        switch (ruido_textura(indice, 0x00510000) % 4) {
            case 0: tono = TEXTURA_CLARA; break;
            case 1: case 2: tono = TEXTURA_MEDIA; break;
            default: tono = TEXTURA_BASE; break;
        }
        inicio_x = (int)roundf(px - (float)lado / 2.0f);
        inicio_y = (int)roundf(py - (float)lado / 2.0f);
        for (gy = inicio_y; gy < inicio_y + lado; gy++) {
            if (gy < 0 || gy >= g.alto) continue;
            for (gx = inicio_x; gx < inicio_x + lado; gx++) {
                if (gx < 0 || gx >= g.ancho) continue;
                dibujar_pixel(gx, gy, tono);
            }
        }
    }
}

static void dibujar_muestra(float cx, float cy, float radio, int juego, int indice) {
    if (juego == 0) {
        if (indice >= 0 && indice < TEXTURA_TOTAL)
            rellenar_textura(cx, cy, radio, indice);
    } else {
        if (indice >= 0 && indice < ROCIO_TOTAL)
            rellenar_rocio(cx, cy, radio, indice);
    }
}

void ui_dibujar_texturas(void) {
    int juego = textura_juego_actual();
    int total = total_juego(juego);
    int elegida = juego ? g.rocio : g.textura;
    Zona circulo = panel_textura_zona();
    float cx, cy, radio;
    int bajo_cursor, i;
    char lineas[2][64];
    int n, ny;

    if (circulo.y >= g.alto) return;
    if (elegida < 0 || elegida >= total) elegida = 0;
    cx = (float)circulo.x + (float)circulo.ancho / 2.0f;
    cy = (float)circulo.y + (float)circulo.alto / 2.0f;
    radio = (float)circulo.ancho / 2.0f;
    bajo_cursor = g.raton_dentro && zona_contiene(circulo, g.raton_x, g.raton_y);

    rellenar_disco(cx, cy, radio,
                   (bajo_cursor || g.textura_menu_abierto) ? TEXTURA_RESALTE : TEXTURA_BORDE);
    dibujar_muestra(cx, cy, radio - 2.0f, juego, elegida);

    if (g.textura_menu_abierto) {
        Zona menu = textura_menu_zona();
        int muestra_bajo = g.raton_dentro
            ? textura_muestra_en(g.raton_x, g.raton_y, juego) : -1;
        const char *nombre;
        dibujar_rect_z(menu, MENU_FONDO);
        for (i = 0; i < total; i++) {
            Zona z = textura_muestra_zona(i, juego);
            float mx = (float)z.x + (float)z.ancho / 2.0f;
            float my = (float)z.y + (float)z.alto / 2.0f;
            float mr = (float)z.ancho / 2.0f;
            u32 aro;
            if (i == elegida) aro = TEXTURA_RESALTE;
            else if (i == muestra_bajo) aro = MENU_HOVER;
            else aro = MENU_BORDE;
            rellenar_disco(mx, my, mr, aro);
            dibujar_muestra(mx, my, mr - 2.0f, juego, i);
        }
        if (muestra_bajo >= 0)
            nombre = juego ? ROCIO_NOMBRE[muestra_bajo] : TEXTURAS_NOMBRE[muestra_bajo];
        else
            nombre = juego ? ROCIO_NOMBRE[elegida] : TEXTURAS_NOMBRE[elegida];
        {
            int arriba = menu.y + 6 + 2 * (MUESTRA_DIAMETRO + MENU_HUECO_MUESTRA);
            int abajo = zona_abajo(menu) - 6;
            int ty = arriba + (abajo - arriba - 7) / 2;
            int tx = menu.x + (menu.ancho - texto_ancho(nombre)) / 2;
            dibujar_texto(tx, ty, nombre, MENU_TEXTO);
        }
        dibujar_marco_z(menu, MENU_BORDE);
    }

    /* Nombre de la herramienta activa bajo el circulo. */
    ny = zona_abajo(circulo) + AJUSTES_MARGEN;
    n = lineas_nombre_herramienta(NOMBRES_HERRAMIENTA[g.herramienta],
                                  AJUSTES_ANCHO, lineas);
    for (i = 0; i < n; i++) {
        int tx = (SIDEBAR_ANCHO - fuente_texto_ancho(lineas[i])) / 2;
        fuente_dibujar(tx, ny + i * (AJUSTES_ETIQUETA_ALTO + 1), lineas[i], TEXTO_UI);
    }
}

void ui_dibujar_sidebar(void) {
    int i, k;
    int grupos[3];
    int filas_antes = 0, hueco = 0, primero = 1;
    Zona sidebar = zona_hacer(0, AREA_SUP_ALTO, SIDEBAR_ANCHO, g.alto - AREA_SUP_ALTO);
    dibujar_rect_z(sidebar, SIDEBAR);
    dibujar_rect(SIDEBAR_ANCHO - 1, AREA_SUP_ALTO, SIDEBAR_ANCHO, g.alto, SEPARADOR);

    /* Linea fina en el hueco que separa cada grupo del anterior. */
    grupos_sidebar(H_TOTAL, grupos);
    for (k = 0; k < 3; k++) {
        int cantidad = grupos[k];
        if (cantidad == 0) continue;
        if (!primero) {
            int arriba = AREA_SUP_ALTO + SIDEBAR_MARGEN +
                         filas_antes * (BOTON_LADO + HUECO) + hueco;
            int y = arriba - HUECO - SEPARADOR_ALTO + SEPARADOR_MARGEN;
            int derecha = SIDEBAR_MARGEN + COLUMNAS * BOTON_LADO +
                          (COLUMNAS - 1) * HUECO;
            dibujar_rect(SIDEBAR_MARGEN, y, derecha, y + 1, SEPARADOR);
        }
        primero = 0;
        filas_antes += filas_grupo(cantidad);
        hueco += SEPARADOR_ALTO;
    }

    /* Solo la herramienta activa tiene fondo; las demas dejan ver el sidebar. */
    for (i = 0; i < H_TOTAL; i++) {
        Zona z = herramienta_zona(i);
        if (z.y >= g.alto) break;
        if ((int)g.herramienta == i) dibujar_rect_z(z, BOTON_SELECCION);
        dibujar_icono_herramienta(i, z);
    }
}

/* ------------------------------------------------------------------ */
/* Barra superior                                                      */
/* ------------------------------------------------------------------ */
/* Mezcla lineal con redondeo, como `mezclar_color`. */
static u32 mezclar_color_redondeado(u32 a, u32 b, float t) {
    int ca, cb, r, g2, bb;
    if (t <= 0.0f) return a & 0x00FFFFFFu;
    if (t >= 1.0f) return b & 0x00FFFFFFu;
    ca = (int)((a >> 16) & 0xFF); cb = (int)((b >> 16) & 0xFF);
    r = (int)floorf((float)ca + (float)(cb - ca) * t + 0.5f);
    ca = (int)((a >> 8) & 0xFF); cb = (int)((b >> 8) & 0xFF);
    g2 = (int)floorf((float)ca + (float)(cb - ca) * t + 0.5f);
    ca = (int)(a & 0xFF); cb = (int)(b & 0xFF);
    bb = (int)floorf((float)ca + (float)(cb - ca) * t + 0.5f);
    return ((u32)r << 16) | ((u32)g2 << 8) | (u32)bb;
}

/* Color del espectro para un punto `t` de 0 (rojo) a 1 (violeta). */
static u32 color_arcoiris(float t) {
    int ultimo = 6;
    float pos;
    int indice;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    pos = t * (float)ultimo;
    indice = (int)floorf(pos);
    if (indice > ultimo) indice = ultimo;
    if (indice == ultimo) return ARCOIRIS[ultimo];
    return mezclar_color_redondeado(ARCOIRIS[indice], ARCOIRIS[indice + 1],
                                    pos - (float)indice);
}

/* Degradado diagonal suave: rojo arriba a la izquierda, violeta abajo a la
 * derecha. Sin borde salvo el resalte bajo el cursor. */
static void dibujar_arcoiris(Zona z, int resaltado) {
    int y, x;
    float rango = (float)((z.ancho - 1 + z.alto - 1) > 1 ? z.ancho - 1 + z.alto - 1 : 1);
    for (y = 0; y < z.alto; y++) {
        for (x = 0; x < z.ancho; x++) {
            float t = (float)(x + y) / rango;
            dibujar_pixel(z.x + x, z.y + y, 0xFF000000u | color_arcoiris(t));
        }
    }
    if (resaltado) dibujar_marco_z(z, BOTON_SELECCION);
}

/* Icono escalado con el mayor aumento entero que quepa en la caja. */
static void dibujar_icono_ajustado(const u32 *pix, int iw, int ih, Zona caja) {
    int escala, dw, dh, x0, y0, dx, dy;
    if (!pix || iw < 1 || ih < 1 || caja.ancho < 1 || caja.alto < 1) return;
    escala = caja.ancho / iw;
    if (caja.alto / ih < escala) escala = caja.alto / ih;
    if (escala < 1) escala = 1;
    dw = iw * escala;
    dh = ih * escala;
    x0 = caja.x + (caja.ancho - dw) / 2;
    y0 = caja.y + (caja.alto - dh) / 2;
    for (dy = 0; dy < dh; dy++) {
        for (dx = 0; dx < dw; dx++) {
            u32 c = pix[(size_t)(dy / escala) * iw + (dx / escala)];
            int alfa = (int)(c >> 24);
            if (alfa == 0) continue;
            dibujar_pixel(x0 + dx, y0 + dy,
                          color_mezclar(dibujar_leer(x0 + dx, y0 + dy), c, alfa));
        }
    }
}

void ui_dibujar_topbar(void) {
    Zona barra = barra_zona();
    Zona sup, inf;
    int i;

    dibujar_rect_z(barra, BARRA);
    dibujar_rect(barra.x, zona_abajo(barra) - 1, zona_derecha(barra), zona_abajo(barra), BARRA_BORDE);

    cuadros_logo(&sup, &inf);

    /* Cuadrado de fondo detras y cuadrado frontal delante, sin bordes propios. */
    dibujar_rect_z(inf, color_casilla(g.fondo));
    dibujar_rect_z(sup, color_casilla(g.frente));
    /* Solo el cuadrado seleccionado lleva borde azul de 1 px por dentro. */
    dibujar_marco_z(g.casilla_elegida == 0 ? sup : inf, BOTON_SELECCION);

    /* Paleta de cuadraditos: solo el elegido lleva borde azul. */
    for (i = 0; i < MINI_TOTAL; i++) {
        Zona z = mini_zona(i);
        u32 c = g.paleta_llena[i] ? g.paleta[i] : MINI_VACIO;
        dibujar_rect_z(z, color_casilla(c));
        if (g.mini_elegido == i) dibujar_marco_z(z, BOTON_SELECCION);
    }

    dibujar_arcoiris(arcoiris_zona(), zona_contiene(arcoiris_zona(), g.raton_x, g.raton_y));

    /* Boton guardar tile. */
    {
        Zona z = boton_guardar_tile_zona();
        int hover = zona_contiene(z, g.raton_x, g.raton_y);
        dibujar_rect_z(z, hover ? 0x00B4B4B4u : BARRA);
        dibujar_marco_z(z, hover ? BOTON_SELECCION : BARRA_BORDE);
        {
            int w = 0, h = 0;
            u32 *pix = icono_guardar_imagen(&w, &h);
            Zona caja = zona_hacer(z.x + 3, z.y + 3, z.ancho - 6, z.alto - 6);
            if (pix) dibujar_icono_ajustado(pix, w, h, caja);
            else dibujar_texto_centrado(z, "SAVE", TEXTO_UI);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Barra inferior                                                      */
/* ------------------------------------------------------------------ */
/* Centro del tirador para un indice, igual que `deslizador_x_de_indice`. */
static int deslizador_x_indice(int indice, Zona pista, int niveles) {
    int minimo = pista.x + ZOOM_KNOB_ANCHO / 2;
    int maximo = zona_derecha(pista) - (ZOOM_KNOB_ANCHO - ZOOM_KNOB_ANCHO / 2);
    if (niveles < 2) return minimo;
    return minimo + (int)((float)(maximo - minimo) * (float)indice / (float)(niveles - 1) + 0.5f);
}

/* Pista de 4 px, tramo recorrido y tirador blanco con borde. */
static void dibujar_deslizador(Zona pista, int indice, int niveles,
                               u32 color_pista, u32 color_relleno, u32 color_knob) {
    int y_pista, centro, kx, ky, fin;
    if (pista.ancho == 0) return;
    y_pista = pista.y + (pista.alto - 4) / 2;
    dibujar_rect(pista.x, y_pista, zona_derecha(pista), y_pista + 4, color_pista);
    centro = deslizador_x_indice(indice, pista, niveles);
    fin = centro > pista.x ? centro : pista.x;
    dibujar_rect(pista.x, y_pista, fin, y_pista + 4, color_relleno);

    kx = centro - ZOOM_KNOB_ANCHO / 2;
    ky = pista.y + (pista.alto - ZOOM_KNOB_ALTO) / 2;
    dibujar_rect(kx, ky, kx + ZOOM_KNOB_ANCHO, ky + ZOOM_KNOB_ALTO, color_relleno);
    dibujar_rect(kx + 1, ky + 1, kx + ZOOM_KNOB_ANCHO - 1, ky + ZOOM_KNOB_ALTO - 1, color_knob);
}

/* Glifo de 9x9 de los botones - y +, sin marco. */
static void dibujar_zoom_boton(Zona z, int es_mas, int activo) {
    u32 color = activo ? ZOOM_TEXTO : ZOOM_APAGADO;
    int gx = z.x + z.ancho / 2 - 9 / 2;
    int gy = z.y + z.alto / 2 - 9 / 2;
    dibujar_rect(gx, gy + 4, gx + 9, gy + 5, color);
    if (es_mas) dibujar_rect(gx + 4, gy, gx + 5, gy + 9, color);
}

void ui_dibujar_barra_abajo(void) {
    Zona barra = barra_abajo_zona();
    Zona pista = zoom_pista_zona();
    Zona etiqueta_zoom = zoom_etiqueta_zona();
    Zona menos = zoom_boton_menos();
    Zona mas = zoom_boton_mas();
    Zona grosor = grosor_pista_zona();
    Zona etiqueta_grosor = grosor_etiqueta_zona();
    char texto[128];
    int zoom_pct = zoom_porcentaje(g.zoom_idx);

    dibujar_rect_z(barra, BARRA);
    dibujar_rect(barra.x, barra.y, zona_derecha(barra), barra.y + 1, BARRA_BORDE);

    /* Resolucion: "80 x 80 px". */
    formatear(texto, sizeof(texto), "%d x %d px", g.doc.ancho, g.doc.alto);
    fuente_dibujar(barra.x + 8, barra.y + (barra.alto - 7) / 2, texto, TEXTO_UI);

    /* Grosor: etiqueta "N px" centrada en su hueco fijo. */
    formatear(texto, sizeof(texto), "%d px", pintura_grosor_px(g.grosor_idx));
    fuente_dibujar(etiqueta_grosor.x + (etiqueta_grosor.ancho - fuente_texto_ancho(texto)) / 2,
                   etiqueta_grosor.y + (BARRA_ABAJO_ALTO - 7) / 2, texto, ZOOM_TEXTO);
    dibujar_deslizador(grosor, g.grosor_idx, 10, ZOOM_PISTA, BOTON_SELECCION, ZOOM_KNOB);

    /* Zoom: - [pista] + [porcentaje]. */
    dibujar_zoom_boton(menos, 0, g.zoom_idx > 0);
    dibujar_zoom_boton(mas, 1, g.zoom_idx < 18);
    dibujar_deslizador(pista, g.zoom_idx, 19, ZOOM_PISTA, ZOOM_RELLENO, ZOOM_KNOB);

    formatear(texto, sizeof(texto), "%d%%", zoom_pct);
    fuente_dibujar(etiqueta_zoom.x + (etiqueta_zoom.ancho - fuente_texto_ancho(texto)) / 2,
                   etiqueta_zoom.y + (BARRA_ABAJO_ALTO - 7) / 2, texto, ZOOM_TEXTO);
}

/* ------------------------------------------------------------------ */
/* Panel derecho: tiles y capas                                        */
/* ------------------------------------------------------------------ */
#define CUBO_PANEL_FONDO 0x00C8C8C8u
#define CUBO_PANEL_BORDE 0x00808080u
#define CUBO_SOMBRA      0x00A0A0A0u
#define CUBO_TEXTO       ZOOM_TEXTO
#define CUBO_VALOR       OPCIONES_APAGADO
#define CUBO_APAGADO     0x00606060u
#define CUBO_RESALTE     MENU_HOVER

/* Forma de ojo de la fila, 13 x 9 pixeles. El centro hace de pupila. */
static const unsigned short ICONO_OJO[9] = {
    0x00F8, 0x030C, 0x0404, 0x08E2, 0x08E2,
    0x08E2, 0x0404, 0x030C, 0x00F8
};

/* Ojo del boton de visibilidad. Oculto se ve atenuado y tachado. */
static void dibujar_icono_ojo(Zona z, int visible) {
    u32 color = visible ? TEXTO_UI : BARRA_BORDE;
    int x0 = z.x + (z.ancho - 13) / 2;
    int y0 = z.y + (z.alto - 9) / 2;
    int fila, columna;
    for (fila = 0; fila < 9; fila++) {
        unsigned short bits = ICONO_OJO[fila];
        for (columna = 0; columna < 13; columna++) {
            if (bits & (1u << (12 - columna))) {
                dibujar_pixel(x0 + columna, y0 + fila, color);
            }
        }
    }
    if (!visible) {
        int lado = (z.ancho < z.alto ? z.ancho : z.alto) - 5;
        int paso;
        for (paso = 0; paso < lado; paso++) {
            int x = z.x + 3 + paso;
            int y = z.y + 3 + lado - 1 - paso;
            dibujar_rect(x, y, x + 2, y + 2, TEXTO_UI);
        }
    }
}

/* Cuadro de transparencia: damero si tiene alfa, blanco si no. */
static void dibujar_indicador_alfa(Zona z, int alfa) {
    dibujar_rect(z.x + 1, z.y + 1, zona_derecha(z) - 1, zona_abajo(z) - 1, LIENZO);
    if (alfa) {
        int celda = 4;
        int y = z.y + 1, fila = 0;
        while (y < zona_abajo(z) - 1) {
            int x = z.x + 1, columna = 0;
            while (x < zona_derecha(z) - 1) {
                if ((fila + columna) % 2 == 0) {
                    int x2 = x + celda, y2 = y + celda;
                    if (x2 > zona_derecha(z) - 1) x2 = zona_derecha(z) - 1;
                    if (y2 > zona_abajo(z) - 1) y2 = zona_abajo(z) - 1;
                    dibujar_rect(x, y, x2, y2, 0x00A8A8A8u);
                }
                x += celda;
                columna++;
            }
            y += celda;
            fila++;
        }
    }
    dibujar_marco_z(z, BARRA_BORDE);
}

/* Miniatura del selector, escalada y centrada en su celda. */
static void dibujar_miniatura_tile(Zona z, const TileGuardado *t, int resaltado) {
    Zona interior = zona_hacer(z.x + 2, z.y + 2,
                               z.ancho - 4 > 0 ? z.ancho - 4 : 0,
                               z.alto - 4 > 0 ? z.alto - 4 : 0);
    dibujar_rect_z(z, LIENZO);
    if (interior.ancho > 0 && interior.alto > 0 && t->ancho > 0 && t->alto > 0) {
        float escala = (float)interior.ancho / (float)t->ancho;
        float fh = (float)interior.alto / (float)t->alto;
        int dw, dh, x0, y0, dy, dx;
        if (fh < escala) escala = fh;
        dw = (int)((float)t->ancho * escala + 0.5f);
        dh = (int)((float)t->alto * escala + 0.5f);
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;
        if (dw > interior.ancho) dw = interior.ancho;
        if (dh > interior.alto) dh = interior.alto;
        x0 = interior.x + (interior.ancho - dw) / 2;
        y0 = interior.y + (interior.alto - dh) / 2;
        for (dy = 0; dy < dh; dy++) {
            int sy = (dy * t->alto) / dh;
            if (sy > t->alto - 1) sy = t->alto - 1;
            for (dx = 0; dx < dw; dx++) {
                int sx = (dx * t->ancho) / dw;
                if (sx > t->ancho - 1) sx = t->ancho - 1;
                dibujar_pixel(x0 + dx, y0 + dy, t->pixeles[(size_t)sy * t->ancho + sx]);
            }
        }
    }
    dibujar_marco_z(z, resaltado ? BOTON_SELECCION : BARRA_BORDE);
}

/* Pinta una miniatura del selector, escalada y centrada en su celda. */
static void dibujar_tiles_panel_derecho(void) {
    int panel = sidebar_derecho_ancho();
    int x, total, visibles, i;
    char titulo[32];
    if (panel < SIDEBAR_MARGEN * 2 + 20) return;
    x = g.ancho - panel + SIDEBAR_MARGEN;
    if (g.num_tiles > 0) formatear(titulo, sizeof(titulo), "TILES %d", g.num_tiles);
    else strcpy(titulo, "TILES");
    fuente_dibujar(x, AREA_SUP_ALTO + 8, titulo, TEXTO_UI);
    if (g.num_tiles == 0) {
        fuente_dibujar(x, AREA_SUP_ALTO + TILES_SELECTOR_TITULO_ALTO + 8,
                       "SIN TILES", BARRA_BORDE);
        return;
    }
    total = g.num_tiles;
    visibles = total - g.tiles_scroll;
    if (visibles > TILES_SELECTOR_VISIBLES) visibles = TILES_SELECTOR_VISIBLES;
    if (visibles < 0) visibles = 0;
    for (i = 0; i < visibles; i++) {
        int idx = total - 1 - (g.tiles_scroll + i);
        if (idx >= 0 && idx < total) {
            dibujar_miniatura_tile(tile_mini_zona(i), &g.tiles[idx], g.hover_tile == idx);
        }
    }
    if (total > TILES_SELECTOR_VISIBLES) {
        int y0 = AREA_SUP_ALTO + TILES_SELECTOR_TITULO_ALTO;
        int track_x = x + TILES_REJILLA_ANCHO + TILES_SCROLL_HUECO;
        int track_w = TILES_SCROLL_ANCHO;
        int track_h = TILES_REJILLA_ALTO;
        if (track_x + track_w <= g.ancho - SIDEBAR_MARGEN) {
            int max_scroll = total - TILES_SELECTOR_VISIBLES;
            int scroll = g.tiles_scroll;
            int thumb_h, thumb_y;
            if (max_scroll < 1) max_scroll = 1;
            if (scroll > max_scroll) scroll = max_scroll;
            thumb_h = (int)(((float)TILES_SELECTOR_VISIBLES / (float)total) * (float)track_h + 0.5f);
            if (thumb_h < 12) thumb_h = 12;
            if (thumb_h > track_h) thumb_h = track_h;
            thumb_y = y0 + (int)((float)scroll / (float)max_scroll * (float)(track_h - thumb_h));
            dibujar_rect(track_x, y0, track_x + track_w, y0 + track_h, LIENZO);
            dibujar_rect(track_x, thumb_y, track_x + track_w, thumb_y + thumb_h, 0x00808080u);
        }
    }
    /* Linea que separa el selector de la lista de capas. */
    {
        int y = AREA_SUP_ALTO + TILES_SELECTOR_ALTO - 1;
        dibujar_rect(x, y, g.ancho - SIDEBAR_MARGEN, y + 1, SEPARADOR);
    }
}

/* Pinta los dos botones + y x al pie del panel derecho. */
static void dibujar_botones_panel_derecho(void) {
    Zona mas, borrar;
    u32 color;
    int brazo = 7, cx, cy, d;
    if (!botones_panel_derecho_zona(&mas, &borrar)) return;
    dibujar_rect_z(mas, BARRA);
    dibujar_marco_z(mas, BARRA_BORDE);
    dibujar_rect_z(borrar, BARRA);
    dibujar_marco_z(borrar, BARRA_BORDE);

    color = TEXTO_UI;
    cx = mas.x + mas.ancho / 2;
    cy = mas.y + mas.alto / 2;
    dibujar_rect(cx - brazo, cy - 1, cx + brazo + 1, cy + 1, color);
    dibujar_rect(cx - 1, cy - brazo, cx + 1, cy + brazo + 1, color);

    color = (g.doc.num_capas > 1) ? TEXTO_UI : BARRA_BORDE;
    cx = borrar.x + borrar.ancho / 2;
    cy = borrar.y + borrar.alto / 2;
    for (d = -brazo; d <= brazo; d++) {
        dibujar_rect(cx + d, cy + d, cx + d + 2, cy + d + 2, color);
        dibujar_rect(cx + d, cy - d, cx + d + 2, cy - d + 2, color);
    }
}

void ui_dibujar_panel_derecho(void) {
    int panel = sidebar_derecho_ancho();
    int x0 = g.ancho - panel;
    int i;
    Zona zona = zona_hacer(x0, AREA_SUP_ALTO, panel, g.alto - AREA_SUP_ALTO);

    dibujar_rect_z(zona, SIDEBAR);
    dibujar_rect(x0, AREA_SUP_ALTO, x0 + 1, g.alto, SEPARADOR);

    dibujar_tiles_panel_derecho();

    /* Titulo de capas y contador "N DE M". */
    {
        int x = x0 + SIDEBAR_MARGEN;
        int y = AREA_SUP_ALTO + 8 + TILES_SELECTOR_ALTO;
        char contador[32];
        int cx;
        if (g.doc.num_capas < 1) { dibujar_botones_panel_derecho(); return; }
        fuente_dibujar(x, y, "CAPAS", TEXTO_UI);
        formatear(contador, sizeof(contador), "%d DE %d", g.doc.activa + 1, g.doc.num_capas);
        cx = g.ancho - SIDEBAR_MARGEN - fuente_texto_ancho(contador);
        if (cx < x) cx = x;
        fuente_dibujar(cx, y, contador, TEXTO_UI);
    }

    /* Las capas se listan de arriba (ultima) a abajo (primera). */
    {
        Zona mas, borrar;
        int limite = g.alto - 8;
        if (botones_panel_derecho_zona(&mas, &borrar)) limite = mas.y - 8;
        for (i = 0; i < g.doc.num_capas; i++) {
            int indice = g.doc.num_capas - 1 - i;
            Zona z = capa_fila_zona(i);
            int activa = (indice == g.doc.activa);
            char etiqueta[32];
            Zona alfa, ojo;
            if (zona_abajo(z) > limite) break;
            dibujar_rect_z(z, activa ? BOTON_SELECCION : BARRA);
            dibujar_marco_z(z, BARRA_BORDE);
            formatear(etiqueta, sizeof(etiqueta), "CAPA %d", indice + 1);
            fuente_dibujar(z.x + 6, z.y + (z.alto - 7) / 2, etiqueta,
                           activa ? 0x00FFFFFFu : TEXTO_UI);
            /* Indicador de transparencia y boton de ojo, a la derecha. */
            alfa = capa_alfa_zona(z);
            dibujar_indicador_alfa(alfa, doc_capa_tiene_alfa(&g.doc, indice));
            ojo = capa_ojo_zona(z);
            dibujar_rect_z(ojo, activa ? BOTON_SELECCION : BARRA);
            dibujar_marco_z(ojo, activa ? 0x00FFFFFFu : BARRA_BORDE);
            dibujar_icono_ojo(ojo, g.doc.capas[indice].visible);
        }
    }

    dibujar_botones_panel_derecho();
}

/* Menu contextual con las acciones de canal alfa de una capa. */
void ui_dibujar_menu_capa(void) {
    Zona panel, anadir, quitar;
    int tiene_alfa;
    if (g.capa_menu_abierto < 0) return;
    if (g.capa_menu_abierto >= g.doc.num_capas) return;
    panel = menu_capa_panel_zona(&anadir, &quitar);
    tiene_alfa = doc_capa_tiene_alfa(&g.doc, g.capa_menu_abierto);
    dibujar_rect(panel.x + 3, panel.y + 3, zona_derecha(panel) + 3, zona_abajo(panel) + 3, CUBO_SOMBRA);
    dibujar_rect_z(panel, CUBO_PANEL_FONDO);
    dibujar_marco_z(panel, CUBO_PANEL_BORDE);
    {
        Zona zonas[2];
        const char *textos[2];
        int activas[2];
        int k;
        zonas[0] = anadir;
        zonas[1] = quitar;
        textos[0] = "ANADIR CANAL ALFA";
        textos[1] = "ELIMINAR CANAL ALFA";
        activas[0] = !tiene_alfa;
        activas[1] = tiene_alfa;
        for (k = 0; k < 2; k++) {
            if (activas[k] && zona_contiene(zonas[k], g.raton_x, g.raton_y)) {
                dibujar_rect_z(zonas[k], MENU_HOVER);
            }
            fuente_dibujar(zonas[k].x + 6, zonas[k].y + (zonas[k].alto - 7) / 2,
                           textos[k], activas[k] ? TEXTO_UI : BARRA_BORDE);
        }
    }
}

/* Ventana modal de confirmacion con titulo y dos lineas de aviso. */
static void dibujar_confirmacion(const char *titulo, const char *aviso, const char *nota,
                                 int si_hover, int no_hover) {
    Zona panel, si, no;
    panel = confirmar_panel_zona(&si, &no);
    dibujar_rect(panel.x + 3, panel.y + 3, zona_derecha(panel) + 3, zona_abajo(panel) + 3, CUBO_SOMBRA);
    dibujar_rect_z(panel, CUBO_PANEL_FONDO);
    dibujar_marco_z(panel, CUBO_PANEL_BORDE);
    fuente_dibujar(panel.x + 14, panel.y + 14, titulo, TEXTO_UI);
    fuente_dibujar(panel.x + 14, panel.y + 38, aviso, TEXTO_UI);
    fuente_dibujar(panel.x + 14, panel.y + 56, nota, TEXTO_UI);
    dibujar_rect_z(si, si_hover ? MENU_HOVER : BARRA);
    dibujar_marco_z(si, si_hover ? BOTON_SELECCION : BARRA_BORDE);
    dibujar_texto_centrado(si, "SI", TEXTO_UI);
    dibujar_rect_z(no, no_hover ? MENU_HOVER : BARRA);
    dibujar_marco_z(no, no_hover ? BOTON_SELECCION : BARRA_BORDE);
    dibujar_texto_centrado(no, "NO", TEXTO_UI);
}

void ui_dibujar_confirmaciones(void) {
    Zona panel, si, no;
    if (g.confirmar_tile >= 0) {
        panel = confirmar_panel_zona(&si, &no);
        dibujar_confirmacion("BORRAR TILE", "SE ELIMINARA DEL EQUIPO.",
                             "NO SE PUEDE DESHACER.",
                             zona_contiene(si, g.raton_x, g.raton_y),
                             zona_contiene(no, g.raton_x, g.raton_y));
        (void)panel;
    } else if (g.confirmar_borrado) {
        panel = confirmar_panel_zona(&si, &no);
        dibujar_confirmacion("BORRAR CAPA", "SE ELIMINARA LA CAPA ACTIVA.",
                             "PUEDES DESHACER CON CTRL+Z",
                             zona_contiene(si, g.raton_x, g.raton_y),
                             zona_contiene(no, g.raton_x, g.raton_y));
        (void)panel;
    }
}

/* ------------------------------------------------------------------ */
/* Visor del patron de textura                                         */
/* ------------------------------------------------------------------ */
#define PATRON_ANCHO 460
#define PATRON_ALTO  360
#define PATRON_TITULO 22
#define PATRON_PADDING 8
#define PATRON_MARGEN_UI 8

Zona patron_zona(void) {
    Zona tr = zona_trabajo();
    int ancho = PATRON_ANCHO;
    int alto = PATRON_ALTO;
    int x, y;

    if (ancho > tr.ancho - PATRON_MARGEN_UI * 2)
        ancho = tr.ancho - PATRON_MARGEN_UI * 2;
    if (ancho < 0) ancho = 0;
    if (alto > tr.alto) alto = tr.alto;
    if (alto < 0) alto = 0;

    if (g.patron_x >= 0 && g.patron_y >= 0) {
        int max_x = zona_derecha(tr) - ancho;
        int max_y = zona_abajo(tr) - alto;
        if (max_x < tr.x) max_x = tr.x;
        if (max_y < tr.y) max_y = tr.y;
        x = g.patron_x;
        y = g.patron_y;
        if (x < tr.x) x = tr.x;
        if (y < tr.y) y = tr.y;
        if (x > max_x) x = max_x;
        if (y > max_y) y = max_y;
    } else {
        int medio = (tr.ancho - ancho) / 2;
        if (medio < 0) medio = 0;
        x = tr.x + medio;
        y = tr.y + PATRON_MARGEN_UI;
    }
    return zona_hacer(x, y, ancho, alto);
}

Zona patron_minimizar_zona(void) {
    Zona v = patron_zona();
    return zona_hacer(zona_derecha(v) - PATRON_PADDING - 16,
                      v.y + (PATRON_TITULO - 16) / 2, 16, 16);
}

/* Tamano del mosaico: misma escala en ancho y alto, con al menos tres copias
 * por lado. */
static void patron_tamano_mosaico(int dibujo_w, int dibujo_h, int disp_w,
                                  int disp_h, int repeticiones,
                                  int *salida_w, int *salida_h) {
    int max_w, max_h, ancho, alto;
    if (dibujo_w <= 0 || dibujo_h <= 0 || disp_w <= 0 || disp_h <= 0) {
        *salida_w = 1;
        *salida_h = 1;
        return;
    }
    if (repeticiones < 1) repeticiones = 1;
    max_w = disp_w / repeticiones;
    max_h = disp_h / repeticiones;
    if (max_w < 1) max_w = 1;
    if (max_h < 1) max_h = 1;
    ancho = dibujo_w < max_w ? dibujo_w : max_w;
    alto = dibujo_h * ancho / dibujo_w;
    if (alto < 1) alto = 1;
    if (alto > max_h) {
        alto = max_h;
        ancho = dibujo_w * alto / dibujo_h;
        if (ancho < 1) ancho = 1;
    }
    *salida_w = ancho;
    *salida_h = alto;
}

/* `dibujar_patron_repetido`: mosaicos pegados, muestreo por vecino mas cercano. */
static void patron_repetir(Zona zona) {
    int mw, mh, x, y, px, py;
    Documento *dv = texto_documento_visible();
    if (zona.ancho <= 0 || zona.alto <= 0 || dv->ancho <= 0 || dv->alto <= 0)
        return;
    patron_tamano_mosaico(dv->ancho, dv->alto, zona.ancho, zona.alto, 3,
                          &mw, &mh);
    if (mw < 1) mw = 1;
    if (mh < 1) mh = 1;
    for (y = zona.y; y < zona_abajo(zona); y += mh) {
        int alto_copia = mh;
        if (alto_copia > zona_abajo(zona) - y)
            alto_copia = zona_abajo(zona) - y;
        for (x = zona.x; x < zona_derecha(zona); x += mw) {
            int ancho_copia = mw;
            if (ancho_copia > zona_derecha(zona) - x)
                ancho_copia = zona_derecha(zona) - x;
            for (py = 0; py < alto_copia; py++) {
                int sy = py * dv->alto / mh;
                if (sy >= dv->alto) sy = dv->alto - 1;
                for (px = 0; px < ancho_copia; px++) {
                    int sx = px * dv->ancho / mw;
                    u32 c;
                    if (sx >= dv->ancho) sx = dv->ancho - 1;
                    c = dv->compuesto[(size_t)sy * dv->ancho + sx];
                    dibujar_pixel(x + px, y + py,
                                  0xFF000000u | (c & 0x00FFFFFFu));
                }
            }
        }
    }
}

void ui_dibujar_patron(void) {
    Zona v, minimizar, area, interior;
    if (!g.patron_visible || g.vista != VISTA_PINTAR) return;
    v = patron_zona();
    if (v.ancho <= 0 || v.alto <= 0 || v.y >= g.alto) return;
    minimizar = patron_minimizar_zona();
    area = zona_hacer(v.x + PATRON_PADDING, v.y + PATRON_TITULO + PATRON_PADDING,
                      v.ancho - PATRON_PADDING * 2,
                      v.alto - PATRON_TITULO - PATRON_PADDING * 2);
    {
        Documento *dv = texto_documento_visible();
        doc_componer_todo(dv);
        if (!dv->compuesto) return;
    }

    dibujar_rect(v.x + 3, v.y + 3, zona_derecha(v) + 3, zona_abajo(v) + 3,
                 OPCIONES_SOMBRA);
    dibujar_rect_z(v, OPCIONES_FONDO);
    dibujar_marco_z(v, MENU_BORDE);

    fuente_dibujar(v.x + PATRON_PADDING, v.y + (PATRON_TITULO - 7) / 2,
                   "PATRON DE TEXTURA", 0x00303030u);

    if (zona_contiene(minimizar, g.raton_x, g.raton_y))
        dibujar_rect_z(minimizar, OPCIONES_RESALTE);
    dibujar_marco_z(minimizar, MENU_BORDE);
    dibujar_rect(minimizar.x + 2, minimizar.y + minimizar.alto / 2,
                 minimizar.x + 2 + minimizar.ancho - 4,
                 minimizar.y + minimizar.alto / 2 + 1, 0x00303030u);

    dibujar_rect(v.x, v.y + PATRON_TITULO - 1, zona_derecha(v),
                 v.y + PATRON_TITULO, MENU_BORDE);

    if (area.ancho > 0 && area.alto > 0) {
        dibujar_rect_z(area, 0x00FFFFFFu);
        dibujar_marco_z(area, MENU_BORDE);
        interior = zona_hacer(area.x + 1, area.y + 1,
                              area.ancho - 2 > 0 ? area.ancho - 2 : 0,
                              area.alto - 2 > 0 ? area.alto - 2 : 0);
        patron_repetir(interior);
    }
}

/* ------------------------------------------------------------------ */
/* Ventana de opciones (deslizadores del pincel)                       */
/* ------------------------------------------------------------------ */
/* Niveles discretos de cada deslizador. El valor guardado en `g` es el valor
 * fisico (porcentaje o grados), no el indice. */
static const int NIVELES_OPACIDAD[11] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 100};
static const int NIVELES_ANGULO[13] = {0, 15, 30, 45, 60, 75, 90, 105, 120, 135, 150, 165, 180};
static const int NIVELES_ESPACIADO[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
static const int ESPACIADO_PORCENTAJES[10] = {5, 10, 20, 30, 40, 50, 75, 100, 150, 200};
static const int NIVELES_DUREZA[11] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
static const char *ETIQUETAS_OPCION[5] = {
    "OPACIDAD", "GROSOR", "ANGULO", "ESPACIADO", "DUREZA"
};

int opciones_niveles(int fila) {
    switch (fila) {
        case 0: return 11;
        case 1: return 10;
        case 2: return 13;
        case 3: return 10;
        case 4: return 11;
        default: return 0;
    }
}

/* Indice del nivel mas cercano a un valor fisico. */
static int indice_mas_cercano(const int *niveles, int n, int valor) {
    int i, mejor = 0, mejor_dif = -1;
    for (i = 0; i < n; i++) {
        int dif = niveles[i] - valor;
        if (dif < 0) dif = -dif;
        if (mejor_dif < 0 || dif < mejor_dif) {
            mejor = i;
            mejor_dif = dif;
        }
    }
    return mejor;
}

int opciones_indice_actual(int fila) {
    switch (fila) {
        case 0: return indice_mas_cercano(NIVELES_OPACIDAD, 11, g.opacidad);
        case 1: return g.grosor_idx < 0 ? 0 : (g.grosor_idx > 9 ? 9 : g.grosor_idx);
        case 2: return indice_mas_cercano(NIVELES_ANGULO, 13, g.angulo);
        case 3: return indice_mas_cercano(ESPACIADO_PORCENTAJES, 10, g.espaciado);
        case 4: return indice_mas_cercano(NIVELES_DUREZA, 11, g.dureza);
        default: return 0;
    }
}

const char *opciones_etiqueta(int fila) {
    if (fila < 0 || fila >= OPCIONES_NUM_FILAS) return "";
    return ETIQUETAS_OPCION[fila];
}

const char *opciones_valor(int fila, char *buffer, int tam) {
    int indice = opciones_indice_actual(fila);
    switch (fila) {
        case 0: formatear(buffer, tam, "%d", NIVELES_OPACIDAD[indice]); break;
        case 1: formatear(buffer, tam, "%d", pintura_grosor_px(indice)); break;
        case 2: formatear(buffer, tam, "%d", NIVELES_ANGULO[indice]); break;
        case 3: formatear(buffer, tam, "%d", NIVELES_ESPACIADO[indice]); break;
        case 4: formatear(buffer, tam, "%d", NIVELES_DUREZA[indice]); break;
        default: buffer[0] = 0; break;
    }
    return buffer;
}

/* Indice del deslizador de una fila para una `x` de pantalla, con la metrica
 * de `deslizador_indice_en_x`. */
static int opciones_indice_en_x(int fila, int x) {
    Zona pista = opciones_pista_zona(fila);
    int niveles = opciones_niveles(fila);
    int minimo = pista.x + ZOOM_KNOB_ANCHO / 2;
    int maximo = zona_derecha(pista) - (ZOOM_KNOB_ANCHO - ZOOM_KNOB_ANCHO / 2);
    float t;
    if (niveles < 2 || maximo <= minimo) return 0;
    if (x < minimo) x = minimo;
    if (x > maximo) x = maximo;
    t = (float)(x - minimo) / (float)(maximo - minimo);
    return (int)(t * (float)(niveles - 1) + 0.5f);
}

void opciones_fijar_desde_x(int fila, int x) {
    int indice = opciones_indice_en_x(fila, x);
    switch (fila) {
        case 0: g.opacidad = NIVELES_OPACIDAD[indice]; break;
        case 1: g.grosor_idx = indice; break;
        case 2: g.angulo = NIVELES_ANGULO[indice]; break;
        case 3: g.espaciado = ESPACIADO_PORCENTAJES[indice]; break;
        case 4: g.dureza = NIVELES_DUREZA[indice]; break;
        default: return;
    }
    app_invalidar();
}

/* ------------------------------------------------------------------ */
/* Opciones del cubo                                                   */
/* ------------------------------------------------------------------ */
/* Niveles de opacidad y umbral. La opacidad admite el 0 % y el umbral va de
 * 5 en 5 hasta el 100 %. */
static const int CUBO_NIVELES_OPACIDAD[12] = {
    0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 100
};
static const int CUBO_NIVELES_UMBRAL[21] = {
    0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50,
    55, 60, 65, 70, 75, 80, 85, 90, 95, 100
};

static int cubo_fila_y(int fila) {
    Zona panel = opciones_zona();
    return panel.y + OPCIONES_TITULO_ALTO + fila * CUBO_FILA_ALTO;
}

static int cubo_control_y(int fila) {
    return cubo_fila_y(fila) + (CUBO_FILA_ALTO - CUBO_CONTROL_ALTO) / 2;
}

/* Indice del nivel mas cercano a una `x`. */
static int cubo_indice_en_x(Zona pista, int niveles, int x) {
    int minimo = pista.x + ZOOM_KNOB_ANCHO / 2;
    int maximo = zona_derecha(pista) - (ZOOM_KNOB_ANCHO - ZOOM_KNOB_ANCHO / 2);
    float t;
    if (niveles < 2 || maximo <= minimo) return 0;
    if (x < minimo) x = minimo;
    if (x > maximo) x = maximo;
    t = (float)(x - minimo) / (float)(maximo - minimo);
    return (int)(t * (float)(niveles - 1) + 0.5f);
}

Zona cubo_opacidad_zona(void) {
    Zona panel = opciones_zona();
    int x0 = panel.x + OPCIONES_PADDING;
    return zona_hacer(x0 + 64, cubo_control_y(0), 110, OPCIONES_PISTA_ALTO);
}

Zona cubo_tipo_zona(int indice) {
    static const char *ETIQUETAS[3] = {"PP", "FD", "PATRON"};
    Zona panel = opciones_zona();
    int x = panel.x + OPCIONES_PADDING + 248;
    int i;
    for (i = 0; i < 3; i++) {
        int ancho = fuente_texto_ancho(ETIQUETAS[i]) + 10;
        if (i == indice)
            return zona_hacer(x, cubo_control_y(0), ancho, CUBO_CONTROL_ALTO);
        x += ancho + 4;
    }
    return zona_vacia();
}

Zona cubo_area_zona(int indice) {
    static const char *ETIQUETAS[3] = {
        "TODA LA SELECCION", "COLORES SIMILARES", "ARTE LINEAL"
    };
    Zona panel = opciones_zona();
    int x = panel.x + OPCIONES_PADDING + 55;
    int i;
    for (i = 0; i < 3; i++) {
        int ancho = fuente_texto_ancho(ETIQUETAS[i]) + 10;
        if (i == indice)
            return zona_hacer(x, cubo_control_y(1), ancho, CUBO_CONTROL_ALTO);
        x += ancho + 4;
    }
    return zona_vacia();
}

Zona cubo_busqueda_zona(int indice) {
    static const char *ETIQUETAS[4] = {
        "TRANSPARENTES", "COMBINADA", "DIAGONALES", "ALISADO"
    };
    Zona panel = opciones_zona();
    int x = panel.x + OPCIONES_PADDING + 55;
    int i;
    for (i = 0; i < 4; i++) {
        int ancho = CUBO_CAJA + 4 + fuente_texto_ancho(ETIQUETAS[i]);
        if (i == indice)
            return zona_hacer(x, cubo_control_y(2), ancho, CUBO_CONTROL_ALTO);
        x += ancho + CUBO_HUECO;
    }
    return zona_vacia();
}

Zona cubo_umbral_zona(void) {
    Zona panel = opciones_zona();
    int x0 = panel.x + OPCIONES_PADDING;
    return zona_hacer(x0 + 425, cubo_control_y(2), 90, OPCIONES_PISTA_ALTO);
}

Zona cubo_compuesto_zona(void) {
    Zona panel = opciones_zona();
    int x0 = panel.x + OPCIONES_PADDING;
    return zona_hacer(x0 + 82, cubo_control_y(3),
                      CUBO_CAJA + 4 + fuente_texto_ancho("COMPUESTO"),
                      CUBO_CONTROL_ALTO);
}

int cubo_control_en(int x, int y) {
    Zona panel = opciones_zona();
    int i;
    if (!zona_contiene(panel, x, y)) return -1;
    if (zona_contiene(cubo_opacidad_zona(), x, y)) return CUBO_CTRL_OPACIDAD;
    for (i = 0; i < 3; i++) {
        if (zona_contiene(cubo_tipo_zona(i), x, y)) return CUBO_CTRL_TIPO + i;
    }
    for (i = 0; i < 3; i++) {
        if (zona_contiene(cubo_area_zona(i), x, y)) return CUBO_CTRL_AREA + i;
    }
    for (i = 0; i < 4; i++) {
        if (zona_contiene(cubo_busqueda_zona(i), x, y)) return CUBO_CTRL_BUSQUEDA + i;
    }
    if (zona_contiene(cubo_umbral_zona(), x, y)) return CUBO_CTRL_UMBRAL;
    if (zona_contiene(cubo_compuesto_zona(), x, y)) return CUBO_CTRL_COMPUESTO;
    return -1;
}

int cubo_es_deslizador(int control) {
    return control == CUBO_CTRL_OPACIDAD || control == CUBO_CTRL_UMBRAL;
}

/* Aplica un control. Devuelve 1 si es un deslizador que queda agarrado. */
int cubo_aplicar_control(int control, int x) {
    if (control >= CUBO_CTRL_TIPO && control < CUBO_CTRL_TIPO + 3) {
        g.cubo_tipo = control - CUBO_CTRL_TIPO;
        return 0;
    }
    if (control >= CUBO_CTRL_AREA && control < CUBO_CTRL_AREA + 3) {
        g.cubo_area = control - CUBO_CTRL_AREA;
        return 0;
    }
    if (control >= CUBO_CTRL_BUSQUEDA && control < CUBO_CTRL_BUSQUEDA + 4) {
        switch (control - CUBO_CTRL_BUSQUEDA) {
            case 0: g.cubo_transparentes = !g.cubo_transparentes; break;
            case 1: g.cubo_combinada = !g.cubo_combinada; break;
            case 2: g.cubo_diagonales = !g.cubo_diagonales; break;
            default: g.cubo_alisado = !g.cubo_alisado; break;
        }
        return 0;
    }
    switch (control) {
        case CUBO_CTRL_OPACIDAD:
            g.cubo_opacidad = cubo_indice_en_x(cubo_opacidad_zona(), 12, x);
            return 1;
        case CUBO_CTRL_UMBRAL:
            g.cubo_umbral = cubo_indice_en_x(cubo_umbral_zona(), 21, x);
            return 1;
        case CUBO_CTRL_COMPUESTO:
            g.cubo_compuesto = !g.cubo_compuesto;
            return 0;
        default:
            return 0;
    }
}

/* Boton del panel del cubo: azul si esta elegido, gris al pasar el raton. */
static void dibujar_boton_cubo(Zona z, const char *etiqueta,
                               int activo, int resaltado) {
    u32 color;
    int tx, ty;
    if (activo) {
        dibujar_rect_z(z, ZOOM_RELLENO);
    } else if (resaltado) {
        dibujar_rect_z(z, CUBO_RESALTE);
    }
    dibujar_marco_z(z, activo ? ZOOM_RELLENO : CUBO_PANEL_BORDE);
    color = activo ? 0x00FFFFFFu : CUBO_TEXTO;
    tx = z.x + (z.ancho - fuente_texto_ancho(etiqueta)) / 2;
    ty = z.y + (z.alto - 7) / 2;
    fuente_dibujar(tx, ty, etiqueta, color);
}

/* Casilla de verificacion con su etiqueta. La zona abarca caja y texto. */
static void dibujar_casilla_cubo(Zona z, const char *etiqueta,
                                 int marcada, int resaltada) {
    Zona caja;
    int ty;
    if (resaltada) dibujar_rect_z(z, CUBO_RESALTE);
    caja = zona_hacer(z.x + 1, z.y + (z.alto - CUBO_CAJA) / 2,
                      CUBO_CAJA, CUBO_CAJA);
    dibujar_rect_z(caja, 0x00FFFFFFu);
    dibujar_marco_z(caja, marcada ? ZOOM_RELLENO : CUBO_APAGADO);
    if (marcada) {
        dibujar_rect(caja.x + 2, caja.y + 2,
                     zona_derecha(caja) - 2, zona_abajo(caja) - 2,
                     ZOOM_RELLENO);
    }
    ty = z.y + (z.alto - 7) / 2;
    fuente_dibujar(z.x + CUBO_CAJA + 4, ty, etiqueta,
                   marcada ? CUBO_TEXTO : CUBO_APAGADO);
}

void ui_dibujar_panel_cubo(void) {
    Zona panel = opciones_zona();
    Zona boton = opciones_minimizar_zona();
    int x0, ty, tx, i;
    char valor[32];

    if (panel.ancho <= 0 || panel.alto <= 0 || panel.y >= g.alto) return;
    x0 = panel.x + OPCIONES_PADDING;

    /* Sombra suave y panel claro con borde. */
    dibujar_rect(panel.x + 3, panel.y + 3,
                 zona_derecha(panel) + 3, zona_abajo(panel) + 3, CUBO_SOMBRA);
    dibujar_rect_z(panel, CUBO_PANEL_FONDO);
    dibujar_marco_z(panel, CUBO_PANEL_BORDE);

    /* Titulo, recordatorio de atajos y boton de minimizar. */
    ty = panel.y + (OPCIONES_TITULO_ALTO - 7) / 2;
    fuente_dibujar(x0, ty, "OPCIONES DEL CUBO", CUBO_TEXTO);
    tx = boton.x - 8 - fuente_texto_ancho("ALT COLOR   MAYUS AREA   MOVER PANEL");
    fuente_dibujar(tx, ty, "ALT COLOR   MAYUS AREA   MOVER PANEL", CUBO_APAGADO);
    if (g.raton_dentro && zona_contiene(boton, g.raton_x, g.raton_y))
        dibujar_rect_z(boton, CUBO_RESALTE);
    dibujar_marco_z(boton, CUBO_PANEL_BORDE);
    dibujar_rect(boton.x + 2, boton.y + boton.alto / 2,
                 boton.x + boton.ancho - 2, boton.y + boton.alto / 2 + 1,
                 ZOOM_TEXTO);
    dibujar_rect(panel.x, panel.y + OPCIONES_TITULO_ALTO - 1,
                 zona_derecha(panel), panel.y + OPCIONES_TITULO_ALTO,
                 CUBO_PANEL_BORDE);

    /* Fila 0: opacidad y tipo de relleno. */
    {
        int texto_y = cubo_fila_y(0) + (CUBO_FILA_ALTO - 7) / 2;
        int indice = g.cubo_opacidad;
        int activa = g.hover_cubo == CUBO_CTRL_OPACIDAD ||
                     g.cubo_arrastre == CUBO_CTRL_OPACIDAD;
        if (indice < 0) indice = 0;
        if (indice > 11) indice = 11;
        fuente_dibujar(x0, texto_y, "OPACIDAD", CUBO_TEXTO);
        dibujar_deslizador(cubo_opacidad_zona(), indice, 12,
                           activa ? CUBO_RESALTE : ZOOM_PISTA,
                           ZOOM_RELLENO, ZOOM_KNOB);
        formatear(valor, sizeof(valor), "%d%%", CUBO_NIVELES_OPACIDAD[indice]);
        fuente_dibujar(zona_derecha(cubo_opacidad_zona()) + 6, texto_y,
                       valor, CUBO_VALOR);
        fuente_dibujar(x0 + 214, texto_y, "TIPO", CUBO_TEXTO);
        for (i = 0; i < 3; i++) {
            static const char *ETIQUETAS[3] = {"PP", "FD", "PATRON"};
            dibujar_boton_cubo(cubo_tipo_zona(i), ETIQUETAS[i],
                               g.cubo_tipo == i,
                               g.hover_cubo == CUBO_CTRL_TIPO + i ||
                               g.cubo_arrastre == CUBO_CTRL_TIPO + i);
        }
    }

    /* Fila 1: superficie afectada. */
    {
        int texto_y = cubo_fila_y(1) + (CUBO_FILA_ALTO - 7) / 2;
        static const char *ETIQUETAS[3] = {
            "TODA LA SELECCION", "COLORES SIMILARES", "ARTE LINEAL"
        };
        fuente_dibujar(x0, texto_y, "AREA", CUBO_TEXTO);
        for (i = 0; i < 3; i++) {
            dibujar_boton_cubo(cubo_area_zona(i), ETIQUETAS[i],
                               g.cubo_area == i,
                               g.hover_cubo == CUBO_CTRL_AREA + i ||
                               g.cubo_arrastre == CUBO_CTRL_AREA + i);
        }
    }

    /* Fila 2: busqueda de colores similares y umbral. */
    {
        int texto_y = cubo_fila_y(2) + (CUBO_FILA_ALTO - 7) / 2;
        int indice = g.cubo_umbral;
        int activa = g.hover_cubo == CUBO_CTRL_UMBRAL ||
                     g.cubo_arrastre == CUBO_CTRL_UMBRAL;
        static const char *ETIQUETAS[4] = {
            "TRANSPARENTES", "COMBINADA", "DIAGONALES", "ALISADO"
        };
        int marcadas[4];
        fuente_dibujar(x0, texto_y, "BUSCAR", CUBO_TEXTO);
        marcadas[0] = g.cubo_transparentes;
        marcadas[1] = g.cubo_combinada;
        marcadas[2] = g.cubo_diagonales;
        marcadas[3] = g.cubo_alisado;
        for (i = 0; i < 4; i++) {
            dibujar_casilla_cubo(cubo_busqueda_zona(i), ETIQUETAS[i],
                                 marcadas[i],
                                 g.hover_cubo == CUBO_CTRL_BUSQUEDA + i ||
                                 g.cubo_arrastre == CUBO_CTRL_BUSQUEDA + i);
        }
        fuente_dibujar(x0 + 390, texto_y, "UMBRAL", CUBO_TEXTO);
        if (indice < 0) indice = 0;
        if (indice > 20) indice = 20;
        dibujar_deslizador(cubo_umbral_zona(), indice, 21,
                           activa ? CUBO_RESALTE : ZOOM_PISTA,
                           ZOOM_RELLENO, ZOOM_KNOB);
        formatear(valor, sizeof(valor), "%d%%", CUBO_NIVELES_UMBRAL[indice]);
        fuente_dibujar(zona_derecha(cubo_umbral_zona()) + 6, texto_y,
                       valor, CUBO_VALOR);
    }

    /* Fila 3: modo compuesto. */
    {
        int texto_y = cubo_fila_y(3) + (CUBO_FILA_ALTO - 7) / 2;
        fuente_dibujar(x0, texto_y, "RELLENAR CON", CUBO_TEXTO);
        dibujar_casilla_cubo(cubo_compuesto_zona(), "COMPUESTO",
                             g.cubo_compuesto,
                             g.hover_cubo == CUBO_CTRL_COMPUESTO ||
                             g.cubo_arrastre == CUBO_CTRL_COMPUESTO);
        fuente_dibujar(x0 + 190, texto_y, "MEZCLA LINEAS Y BORDES",
                       CUBO_APAGADO);
    }
}

static void texto_mayusculas(char *destino, int tam, const char *origen) {
    int i = 0;
    if (tam <= 0) return;
    for (; origen && origen[i] && i < tam - 1; i++) {
        char c = origen[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        destino[i] = c;
    }
    destino[i] = 0;
}

/* Tira plegada de la ventana de opciones, bajo el nombre de la herramienta. */
static void dibujar_opciones_minimizada(void) {
    Zona tira = opciones_minimizada_zona();
    int hover;
    if (tira.ancho <= 0 || tira.y >= g.alto) return;
    hover = g.raton_dentro && zona_contiene(tira, g.raton_x, g.raton_y);
    dibujar_rect_z(tira, hover ? OPCIONES_RESALTE : OPCIONES_FONDO);
    dibujar_marco_z(tira, MENU_BORDE);
    {
        char lineas[2][64];
        if (lineas_nombre_herramienta(ui_nombre_herramienta(g.herramienta),
                                      tira.ancho - 12, lineas) > 0) {
            fuente_dibujar(tira.x + 6, tira.y + (tira.alto - 7) / 2, lineas[0], ZOOM_TEXTO);
        }
    }
    /* Flecha hacia arriba para restaurar. */
    {
        int ax = zona_derecha(tira) - 10;
        int ay = tira.y + tira.alto / 2;
        dibujar_rect(ax, ay, ax + 5, ay + 1, ZOOM_TEXTO);
        dibujar_rect(ax + 2, ay - 1, ax + 3, ay + 1, ZOOM_TEXTO);
    }
}

void ui_dibujar_opciones(void) {
    Zona panel;
    char texto[160];
    int i;

    if (!g.panel_opciones) {
        dibujar_opciones_minimizada();
        return;
    }
    /* Con el cubo la ventana muestra sus opciones; con el resto de
     * herramientas, los deslizadores del pincel. */
    if (g.herramienta == H_CUBO) {
        ui_dibujar_panel_cubo();
        return;
    }
    panel = opciones_zona();
    if (panel.ancho <= 0 || panel.alto <= 0 || panel.y >= g.alto) return;

    /* Sombra y fondo gris, igual que la ventana del cubo. */
    dibujar_rect(panel.x + 3, panel.y + 3, zona_derecha(panel) + 3,
                 zona_abajo(panel) + 3, OPCIONES_SOMBRA);
    dibujar_rect_z(panel, OPCIONES_FONDO);
    dibujar_marco_z(panel, MENU_BORDE);

    /* Titulo: "OPCIONES  <HERRAMIENTA>" y la pista "MOVER PANEL". */
    {
        char nombre[64];
        Zona boton = opciones_minimizar_zona();
        int ty = panel.y + (OPCIONES_TITULO_ALTO - 7) / 2;
        int tx;
        int hover;
        texto_mayusculas(nombre, sizeof(nombre), ui_nombre_herramienta(g.herramienta));
        formatear(texto, sizeof(texto), "OPCIONES  %s", nombre);
        fuente_dibujar(panel.x + OPCIONES_PADDING, ty, texto, ZOOM_TEXTO);
        tx = boton.x - 8 - fuente_texto_ancho("MOVER PANEL");
        fuente_dibujar(tx, ty, "MOVER PANEL", OPCIONES_APAGADO);

        hover = g.raton_dentro && zona_contiene(boton, g.raton_x, g.raton_y);
        if (hover) dibujar_rect_z(boton, OPCIONES_RESALTE);
        dibujar_marco_z(boton, MENU_BORDE);
        dibujar_rect(boton.x + 2, boton.y + boton.alto / 2,
                     boton.x + boton.ancho - 2, boton.y + boton.alto / 2 + 1, ZOOM_TEXTO);
    }
    dibujar_rect(panel.x, panel.y + OPCIONES_TITULO_ALTO - 1,
                 zona_derecha(panel), panel.y + OPCIONES_TITULO_ALTO, MENU_BORDE);

    for (i = 0; i < OPCIONES_NUM_FILAS; i++) {
        Zona pista = opciones_pista_zona(i);
        int fila_y = panel.y + OPCIONES_TITULO_ALTO + i * OPCIONES_FILA_ALTO;
        int texto_y = fila_y + (OPCIONES_FILA_ALTO - 7) / 2;
        int indice = opciones_indice_actual(i);
        int activa = (g.hover_opcion == i || g.opcion_arrastrada == i);
        char valor[32];
        opciones_valor(i, valor, sizeof(valor));
        fuente_dibujar(panel.x + OPCIONES_PADDING, texto_y,
                       opciones_etiqueta(i), ZOOM_TEXTO);
        /* El numero se alinea a la derecha de la columna de etiquetas. */
        fuente_dibujar(panel.x + OPCIONES_PADDING + OPCIONES_PISTA_X - 8 -
                           fuente_texto_ancho(valor),
                       texto_y, valor, OPCIONES_APAGADO);
        dibujar_deslizador(pista, indice, opciones_niveles(i),
                           activa ? OPCIONES_RESALTE : ZOOM_PISTA,
                           ZOOM_RELLENO, ZOOM_KNOB);
    }
}

/* ------------------------------------------------------------------ */
/* Ventana de informacion                                              */
/* ------------------------------------------------------------------ */
static Zona info_zona(void) {
    int x = (g.ancho - 448) / 2;
    int y = (g.alto - 382) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    return zona_hacer(x, y, 448, 382);
}

static Zona info_cerrar_zona(void) {
    Zona v = info_zona();
    int lado = 18;
    int derecha = zona_derecha(v) < g.ancho ? zona_derecha(v) : g.ancho;
    int x = derecha - lado - 3;
    if (x < v.x) x = v.x;
    return zona_hacer(x, v.y + 3, lado, lado);
}

/* Constantes de la ventana de informacion. */
#define INFO_ANCHO_V       448
#define INFO_TITULO_ALTO_V 24
#define INFO_MARGEN_V      16
#define INFO_IMAGEN_V      240
#define INFO_SEPARACION_V  12
#define INFO_ESCALA_V      2
#define INFO_LINEA_ALTO_V  14
#define INFO_LINEA_HUECO_V 6
#define INFO_FONDO_V       0x00F4F4F4u
#define INFO_BORDE_V       0x00808080u
#define INFO_TITULO_FONDO_V 0x00DCDCDCu
#define INFO_TEXTO_V       0x00303030u
#define INFO_SOMBRA_ALFA_V 120

static const char *INFO_LINEAS_V[4] = {
    "HERRAMIENTA DE USO LIBRE",
    "GREEN BLUE STUDIOS 2026",
    "DIRECTOR ISMAEL SIERRA.",
    "DESARROLLADOR: RAUL DIAZ GUTIERREZ"
};

static Zona info_imagen_zona(void) {
    Zona v = info_zona();
    return zona_hacer(v.x + (v.ancho - INFO_IMAGEN_V) / 2,
                      v.y + INFO_TITULO_ALTO_V + INFO_MARGEN_V,
                      INFO_IMAGEN_V, INFO_IMAGEN_V);
}

/* Mezcla de `mezclar` con un color negro y el alfa indicado. */
static void info_mezclar_negro(int x, int y, int alfa) {
    u32 dst;
    int inv, r, gg, b;
    if (!g.pixeles || x < 0 || y < 0 || x >= g.ancho || y >= g.alto) return;
    if (alfa <= 0) return;
    if (alfa > 255) alfa = 255;
    inv = 255 - alfa;
    dst = g.pixeles[(size_t)y * g.ancho + x];
    r = ((int)((dst >> 16) & 0xFF) * inv) / 255;
    gg = ((int)((dst >> 8) & 0xFF) * inv) / 255;
    b = ((int)(dst & 0xFF) * inv) / 255;
    g.pixeles[(size_t)y * g.ancho + x] =
        0xFF000000u | ((u32)r << 16) | ((u32)gg << 8) | (u32)b;
}

/* Como `dibujar_imagen`: tamano natural centrado en la caja, mezclando el
 * alfa de cada pixel. */
static void info_dibujar_imagen(const u32 *pix, int pw, int ph, Zona caja) {
    int x0, y0, dx, dy;
    if (!pix || pw <= 0 || ph <= 0) return;
    x0 = caja.x + (caja.ancho - pw) / 2;
    y0 = caja.y + (caja.alto - ph) / 2;
    for (dy = 0; dy < ph; dy++) {
        for (dx = 0; dx < pw; dx++) {
            u32 c = pix[(size_t)dy * pw + dx];
            int alfa = (int)((c >> 24) & 0xFF);
            if (alfa == 0) continue;
            if (alfa >= 255) {
                dibujar_pixel(x0 + dx, y0 + dy, 0xFF000000u | (c & 0x00FFFFFFu));
            } else {
                u32 dst = dibujar_leer(x0 + dx, y0 + dy);
                int inv = 255 - alfa;
                int r = ((int)((c >> 16) & 0xFF) * alfa +
                         (int)((dst >> 16) & 0xFF) * inv) / 255;
                int gg = ((int)((c >> 8) & 0xFF) * alfa +
                          (int)((dst >> 8) & 0xFF) * inv) / 255;
                int b = ((int)(c & 0xFF) * alfa +
                         (int)(dst & 0xFF) * inv) / 255;
                dibujar_pixel(x0 + dx, y0 + dy,
                              0xFF000000u | ((u32)r << 16) | ((u32)gg << 8) | (u32)b);
            }
        }
    }
}

void ui_dibujar_info(void) {
    Zona v;
    Zona cerrar;
    int i;
    const int banda = 8;
    if (!g.info_visible) return;
    v = info_zona();
    cerrar = info_cerrar_zona();

    /* Sombra suave: aureola desplazada 4 px con alfa decreciente. */
    {
        int sx = v.x + 4, sy = v.y + 4;
        int x, y;
        for (y = sy - banda; y < zona_abajo(v) + 4 + banda; y++) {
            for (x = sx - banda; x < zona_derecha(v) + 4 + banda; x++) {
                int dx, dy, d, alfa;
                if (zona_contiene(v, x, y)) continue;
                dx = sx - x;
                if (x - (zona_derecha(v) + 4 - 1) > dx)
                    dx = x - (zona_derecha(v) + 4 - 1);
                if (dx < 0) dx = 0;
                dy = sy - y;
                if (y - (zona_abajo(v) + 4 - 1) > dy)
                    dy = y - (zona_abajo(v) + 4 - 1);
                if (dy < 0) dy = 0;
                d = dx > dy ? dx : dy;
                if (d >= banda) continue;
                alfa = INFO_SOMBRA_ALFA_V * (banda - d) / banda;
                info_mezclar_negro(x, y, alfa);
            }
        }
    }

    dibujar_rect_z(v, INFO_FONDO_V);
    dibujar_rect(v.x, v.y, zona_derecha(v), v.y + INFO_TITULO_ALTO_V,
                 INFO_TITULO_FONDO_V);
    dibujar_rect(v.x, v.y + INFO_TITULO_ALTO_V - 1, zona_derecha(v),
                 v.y + INFO_TITULO_ALTO_V, INFO_BORDE_V);

    fuente_dibujar_escala(v.x + 10,
                          v.y + (INFO_TITULO_ALTO_V - 7 * INFO_ESCALA_V) / 2,
                          "INFO", INFO_TEXTO_V, INFO_ESCALA_V);

    if (zona_contiene(cerrar, g.raton_x, g.raton_y))
        dibujar_rect_z(cerrar, BOTON_SELECCION);
    fuente_dibujar_escala(
        cerrar.x + (cerrar.ancho - fuente_texto_ancho_escala("X", INFO_ESCALA_V)) / 2,
        cerrar.y + (cerrar.alto - 7 * INFO_ESCALA_V) / 2,
        "X",
        zona_contiene(cerrar, g.raton_x, g.raton_y) ? 0x00FFFFFFu : INFO_TEXTO_V,
        INFO_ESCALA_V);

    if (g.info_imagen && g.info_w > 0) {
        Zona img = info_imagen_zona();
        info_dibujar_imagen(g.info_imagen, g.info_w, g.info_h, img);
        dibujar_marco_z(img, 0x00C8C8C8u);
    }

    for (i = 0; i < 4; i++) {
        int y = v.y + INFO_TITULO_ALTO_V + INFO_MARGEN_V + INFO_IMAGEN_V +
                INFO_SEPARACION_V + i * (INFO_LINEA_ALTO_V + INFO_LINEA_HUECO_V);
        int w = fuente_texto_ancho_escala(INFO_LINEAS_V[i], INFO_ESCALA_V);
        fuente_dibujar_escala(v.x + (v.ancho - w) / 2, y, INFO_LINEAS_V[i],
                              INFO_TEXTO_V, INFO_ESCALA_V);
    }

    /* El borde va al final para cerrar la ventana por los cuatro lados. */
    dibujar_marco_z(v, INFO_BORDE_V);
}

/* ------------------------------------------------------------------ */
/* Dibujo general                                                      */
/* ------------------------------------------------------------------ */
/* Recorta un rectangulo a la zona indicada. Devuelve 0 si no queda nada. */
static int modulos_recortar(Zona z, int *x0, int *y0, int *x1, int *y1) {
    if (*x0 < z.x) *x0 = z.x;
    if (*y0 < z.y) *y0 = z.y;
    if (*x1 > zona_derecha(z)) *x1 = zona_derecha(z);
    if (*y1 > zona_abajo(z)) *y1 = zona_abajo(z);
    return *x0 < *x1 && *y0 < *y1;
}

static void modulos_dibujar_rejilla(Zona lienzo) {
    int off_x = g.pan_x % MODULOS_CUADRICULA;
    int off_y = g.pan_y % MODULOS_CUADRICULA;
    int x, y;

    if (off_x < 0) off_x += MODULOS_CUADRICULA;
    if (off_y < 0) off_y += MODULOS_CUADRICULA;

    for (x = lienzo.x + off_x; x < zona_derecha(lienzo); x += MODULOS_CUADRICULA)
        dibujar_rect(x, lienzo.y, x + 1, zona_abajo(lienzo), MODULOS_REJILLA);
    for (y = lienzo.y + off_y; y < zona_abajo(lienzo); y += MODULOS_CUADRICULA)
        dibujar_rect(lienzo.x, y, zona_derecha(lienzo), y + 1, MODULOS_REJILLA);

    /* Origen del mundo: ayuda a percibir el desplazamiento. */
    x = lienzo.x + g.pan_x;
    y = lienzo.y + g.pan_y;
    if (x >= lienzo.x && x < zona_derecha(lienzo))
        dibujar_rect(x, lienzo.y, x + 1, zona_abajo(lienzo), MODULOS_EJE);
    if (y >= lienzo.y && y < zona_abajo(lienzo))
        dibujar_rect(lienzo.x, y, zona_derecha(lienzo), y + 1, MODULOS_EJE);
}

/* Guias de acople: linea vertical u horizontal que marca el lado imantado. */
static void modulos_dibujar_guia(Zona lienzo, int linea_x, int linea_y) {
    if (linea_x >= 0) {
        int sx = lienzo.x + g.pan_x + linea_x;
        if (sx >= lienzo.x && sx < zona_derecha(lienzo))
            dibujar_rect(sx, lienzo.y, sx + 1, zona_abajo(lienzo),
                         BOTON_SELECCION);
    }
    if (linea_y >= 0) {
        int sy = lienzo.y + g.pan_y + linea_y;
        if (sy >= lienzo.y && sy < zona_abajo(lienzo))
            dibujar_rect(lienzo.x, sy, zona_derecha(lienzo), sy + 1,
                         BOTON_SELECCION);
    }
}

/* Lienzo desplazable del gestor: rejilla y elementos insertados. */
static void modulos_dibujar_lienzo(void) {
    Zona lienzo = modulos_lienzo_zona();
    int i;

    modulos_dibujar_rejilla(lienzo);

    for (i = 0; i < g.num_modulos; i++) {
        ModuloElemento *e = &g.modulos[i];
        int activo = (g.modulo_arrastrando == 3 && i == g.modulo_drag_i);
        int x0 = lienzo.x + g.pan_x + e->x;
        int y0 = lienzo.y + g.pan_y + e->y;
        int x1 = x0 + e->ancho;
        int y1 = y0 + e->alto;
        if (!modulos_recortar(lienzo, &x0, &y0, &x1, &y1)) continue;
        dibujar_rect(x0, y0, x1, y1, activo ? 0x00E4ECF6u : BARRA);
        dibujar_marco(x0, y0, x1, y1,
                      activo ? BOTON_SELECCION : SEPARADOR);
    }

    /* Guias del elemento que se esta moviendo. */
    if (g.modulo_arrastrando == 3)
        modulos_dibujar_guia(lienzo, g.modulo_snap_linea_x,
                             g.modulo_snap_linea_y);

    /* Previsualizacion del elemento que se arrastra desde el panel derecho,
     * aplicando el mismo acople que se usara al soltarlo. */
    if (g.modulo_arrastrando == 1) {
        int wx = g.raton_x - lienzo.x - g.pan_x - MODULOS_ELEMENTO_ANCHO / 2;
        int wy = g.raton_y - lienzo.y - g.pan_y - MODULOS_ELEMENTO_ALTO / 2;
        int lx = -1, ly = -1;
        int x0, y0, x1, y1;
        modulos_acoplar(&wx, &wy, MODULOS_ELEMENTO_ANCHO,
                        MODULOS_ELEMENTO_ALTO, -1, &lx, &ly);
        x0 = lienzo.x + g.pan_x + wx;
        y0 = lienzo.y + g.pan_y + wy;
        x1 = x0 + MODULOS_ELEMENTO_ANCHO;
        y1 = y0 + MODULOS_ELEMENTO_ALTO;
        if (modulos_recortar(lienzo, &x0, &y0, &x1, &y1)) {
            dibujar_rect(x0, y0, x1, y1, 0x004A5A6Au);
            dibujar_marco(x0, y0, x1, y1, BOTON_SELECCION);
        }
        modulos_dibujar_guia(lienzo, lx, ly);
    }
}

/* Boton rectangular sin nombre que se arrastra al lienzo. */
static void ui_dibujar_boton_modulos(void) {
    Zona b = modulos_boton_zona();
    int hover = zona_contiene(b, g.raton_x, g.raton_y);

    dibujar_rect_z(b, hover ? 0x00EDF2F7u : BARRA);
    dibujar_rect(b.x, b.y, zona_derecha(b), b.y + 1, 0x00F8F8F8u);
    dibujar_rect(b.x, b.y, b.x + 1, zona_abajo(b), 0x00F8F8F8u);
    dibujar_rect(b.x, zona_abajo(b) - 1, zona_derecha(b), zona_abajo(b),
                 SEPARADOR);
    dibujar_rect(zona_derecha(b) - 1, b.y, zona_derecha(b), zona_abajo(b),
                 SEPARADOR);
}

/* Paneles del gestor de modulos: los mismos huecos que en modo normal
 * (izquierdo, derecho e inferior) pero completamente vacios. */
static void ui_dibujar_paneles_vacios(void) {
    Zona sidebar = zona_hacer(0, AREA_SUP_ALTO, SIDEBAR_ANCHO,
                              g.alto - AREA_SUP_ALTO);
    Zona abajo = barra_abajo_zona();
    int panel = sidebar_derecho_ancho();
    int x0 = g.ancho - panel;
    Zona derecha = zona_hacer(x0, AREA_SUP_ALTO, panel, g.alto - AREA_SUP_ALTO);

    /* Barra lateral izquierda. */
    dibujar_rect_z(sidebar, SIDEBAR);
    dibujar_rect(SIDEBAR_ANCHO - 1, AREA_SUP_ALTO, SIDEBAR_ANCHO, g.alto,
                 SEPARADOR);

    /* Panel derecho. */
    dibujar_rect_z(derecha, SIDEBAR);
    dibujar_rect(x0, AREA_SUP_ALTO, x0 + 1, g.alto, SEPARADOR);

    /* Barra inferior. */
    dibujar_rect_z(abajo, BARRA);
    dibujar_rect(abajo.x, abajo.y, zona_derecha(abajo), abajo.y + 1,
                 BARRA_BORDE);

    /* Boton de insercion en el panel derecho. */
    ui_dibujar_boton_modulos();
}

void ui_dibujar(void) {
    int i;

    /* Gestor de modulos: paneles vacios, lienzo desplazable y boton. */
    if (g.vista == VISTA_MODULOS) {
        dibujar_rect(0, 0, g.ancho, g.alto, FONDO);
        ui_dibujar_paneles_vacios();
        modulos_dibujar_lienzo();
        ui_dibujar_pestanas();
        ui_dibujar_barra_menus();
        ui_dibujar_menu_desplegable();
        return;
    }

    /* Fondo general. */
    dibujar_rect(0, 0, g.ancho, g.alto, FONDO);

    ui_dibujar_pestanas();
    ui_dibujar_barra_menus();

    ui_dibujar_sidebar();
    ui_dibujar_topbar();
    ui_dibujar_barra_abajo();
    ui_dibujar_panel_derecho();

    if (g.vista == VISTA_MAPA) {
        mapa_dibujar();
    } else if (g.modo_tiles) {
        ui_dibujar_tiles();
        pintura_dibujar_solo_overlays();
    } else {
        pintura_dibujar_lienzo();
    }

    ui_dibujar_texturas();

    /* Mensaje de estado flotante. */
    if (g.estado[0] && GetTickCount() < g.estado_hasta) {
        int w = texto_ancho(g.estado) + 16;
        int x = SIDEBAR_ANCHO + (ancho_util() - w) / 2;
        int y = g.alto - BARRA_ABAJO_ALTO - 30;
        if (x < SIDEBAR_ANCHO) x = SIDEBAR_ANCHO;
        dibujar_rect(x, y, x + w, y + 22, 0x00F4F4F4u);
        dibujar_marco(x, y, x + w, y + 22, MENU_BORDE);
        dibujar_texto(x + 8, y + (22 - texto_alto()) / 2, g.estado, TEXTO_UI);
    }

    /* Las opciones se dibujan debajo de la ventana de patron y del visor de
     * tiles; estos dos van por encima. */
    ui_dibujar_opciones();
    ui_dibujar_patron();
    ui_dibujar_tiles_visor();
    ui_dibujar_menu_capa();
    ui_dibujar_info();
    ui_dibujar_menu_desplegable();
    texto_dibujar_barra();
    ui_dibujar_confirmaciones();

    /* Linea de estado de estructuras no usadas. */
    for (i = 0; i < 0; i++) { }
}
