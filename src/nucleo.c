/* ==========================================================================
 * Nucleo: buffer de dibujo, primitivas, ventana, bucle de mensajes y dialogos.
 * ========================================================================== */
#include "app.h"

App g;

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */
int limitar(int v, int min, int max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

int redondear(float v) {
    return (int)(v + (v >= 0.0f ? 0.5f : -0.5f));
}

void copiar_pixeles(u32 *dst, const u32 *src, int n) {
    if (dst && src && n > 0) memcpy(dst, src, (size_t)n * sizeof(u32));
}

void formatear(char *dst, int tam, const char *formato, ...) {
    va_list args;
    if (!dst || tam <= 0) return;
    va_start(args, formato);
    _vsnprintf(dst, (size_t)tam - 1, formato, args);
    va_end(args);
    dst[tam - 1] = '\0';
}

unsigned int hash32(unsigned int x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float rnd01(unsigned int *estado) {
    *estado = *estado * 1664525u + 1013904223u;
    return (float)((*estado >> 8) & 0xFFFFFFu) / 16777216.0f;
}

void recortar_texto(char *dst, const char *src, int max) {
    int n;
    if (!dst || max <= 0) return;
    if (!src) src = "";
    n = (int)strlen(src);
    if (n >= max) n = max - 1;
    memcpy(dst, src, (size_t)n);
    dst[n] = '\0';
}

int termina_con(const char *texto, const char *sufijo) {
    size_t a, b;
    if (!texto || !sufijo) return 0;
    a = strlen(texto);
    b = strlen(sufijo);
    if (b > a) return 0;
    return _stricmp(texto + a - b, sufijo) == 0;
}

void ruta_carpeta(char *dst, int tam, const char *ruta) {
    int i, corte = -1;
    if (!dst || tam <= 0) return;
    dst[0] = '\0';
    if (!ruta) return;
    for (i = 0; ruta[i]; i++) {
        if (ruta[i] == '\\' || ruta[i] == '/') corte = i;
    }
    if (corte < 0) {
        recortar_texto(dst, "", tam);
        return;
    }
    if (corte >= tam) corte = tam - 1;
    memcpy(dst, ruta, (size_t)corte);
    dst[corte] = '\0';
}

void ruta_nombre(char *dst, int tam, const char *ruta) {
    int i, corte = 0;
    if (!dst || tam <= 0) return;
    dst[0] = '\0';
    if (!ruta) return;
    for (i = 0; ruta[i]; i++) {
        if (ruta[i] == '\\' || ruta[i] == '/') corte = i + 1;
    }
    recortar_texto(dst, ruta + corte, tam);
}

void ruta_sin_extension(char *dst, int tam, const char *ruta) {
    char nombre[MAX_PATH];
    char *punto;
    ruta_nombre(nombre, sizeof(nombre), ruta);
    punto = strrchr(nombre, '.');
    if (punto && punto != nombre) *punto = '\0';
    recortar_texto(dst, nombre, tam);
}

int crear_carpeta(const char *ruta) {
    if (!ruta || !ruta[0]) return 0;
    if (CreateDirectoryA(ruta, NULL)) return 1;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

int leer_archivo(const char *ruta, char **datos, int *largo) {
    FILE *f;
    long n;
    char *buf;
    if (!ruta || !datos || !largo) return 0;
    *datos = NULL;
    *largo = 0;
    f = fopen(ruta, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    n = ftell(f);
    if (n < 0 || n > 64 * 1024 * 1024) { fclose(f); return 0; }
    rewind(f);
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return 0; }
    if (n > 0 && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return 0;
    }
    buf[n] = '\0';
    fclose(f);
    *datos = buf;
    *largo = (int)n;
    return 1;
}

int escribir_archivo(const char *ruta, const char *datos, int largo) {
    FILE *f;
    if (!ruta) return 0;
    f = fopen(ruta, "wb");
    if (!f) return 0;
    if (largo > 0 && fwrite(datos, 1, (size_t)largo, f) != (size_t)largo) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

void obtener_ruta_exe(char *dst, int tam) {
    char ruta[MAX_PATH];
    GetModuleFileNameA(NULL, ruta, MAX_PATH);
    ruta_carpeta(dst, tam, ruta);
}

void unir_ruta(char *dst, int tam, const char *a, const char *b) {
    if (a && a[0]) {
        formatear(dst, tam, "%s\\%s", a, b ? b : "");
    } else {
        recortar_texto(dst, b ? b : "", tam);
    }
}

/* ------------------------------------------------------------------ */
/* Colores                                                             */
/* ------------------------------------------------------------------ */
COLORREF color_a_win(u32 c) {
    return RGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

u32 color_de_win(COLORREF c) {
    return 0x00000000u | ((u32)GetRValue(c) << 16) | ((u32)GetGValue(c) << 8) | (u32)GetBValue(c);
}

u32 color_mezclar(u32 dst, u32 src, int alfa) {
    int sa, ia, r, gg, b;
    if (alfa <= 0) return dst;
    if (alfa > 255) alfa = 255;
    sa = (int)((src >> 24) & 0xFF) * alfa / 255;
    if (sa <= 0) return dst;
    if (sa >= 255) return 0xFF000000u | (src & 0x00FFFFFFu);
    ia = 255 - sa;
    r = ((int)((src >> 16) & 0xFF) * sa + (int)((dst >> 16) & 0xFF) * ia) / 255;
    gg = ((int)((src >> 8) & 0xFF) * sa + (int)((dst >> 8) & 0xFF) * ia) / 255;
    b = ((int)(src & 0xFF) * sa + (int)(dst & 0xFF) * ia) / 255;
    return 0xFF000000u | ((u32)r << 16) | ((u32)gg << 8) | (u32)b;
}

u32 color_mezclar_color(u32 a, u32 b, float t) {
    int r, gg, bb;
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    r = (int)((a >> 16) & 0xFF) + (int)(((int)((b >> 16) & 0xFF) - (int)((a >> 16) & 0xFF)) * t);
    gg = (int)((a >> 8) & 0xFF) + (int)(((int)((b >> 8) & 0xFF) - (int)((a >> 8) & 0xFF)) * t);
    bb = (int)(a & 0xFF) + (int)(((int)(b & 0xFF) - (int)(a & 0xFF)) * t);
    return 0xFF000000u | ((u32)limitar(r, 0, 255) << 16) | ((u32)limitar(gg, 0, 255) << 8) |
           (u32)limitar(bb, 0, 255);
}

int color_diferencia(u32 a, u32 b) {
    int dr = abs((int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF));
    int dg = abs((int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF));
    int db = abs((int)(a & 0xFF) - (int)(b & 0xFF));
    int da = abs((int)((a >> 24) & 0xFF) - (int)((b >> 24) & 0xFF));
    if (dr < dg) dr = dg;
    if (dr < db) dr = db;
    if (dr < da) dr = da;
    return dr;
}

u32 color_oscurecer(u32 c, int delta) {
    int r = limitar((int)((c >> 16) & 0xFF) - delta, 0, 255);
    int g2 = limitar((int)((c >> 8) & 0xFF) - delta, 0, 255);
    int b = limitar((int)(c & 0xFF) - delta, 0, 255);
    return 0xFF000000u | ((u32)r << 16) | ((u32)g2 << 8) | (u32)b;
}

u32 color_aclarar(u32 c, int delta) {
    return color_oscurecer(c, -delta);
}

/* ------------------------------------------------------------------ */
/* Buffer de dibujo                                                    */
/* ------------------------------------------------------------------ */
void dibujar_pixel(int x, int y, u32 color) {
    if (x < 0 || y < 0 || x >= g.ancho || y >= g.alto || !g.pixeles) return;
    g.pixeles[y * g.ancho + x] = color;
}

u32 dibujar_leer(int x, int y) {
    if (x < 0 || y < 0 || x >= g.ancho || y >= g.alto || !g.pixeles) return 0;
    return g.pixeles[y * g.ancho + x];
}

void dibujar_rect(int x0, int y0, int x1, int y1, u32 color) {
    int y, x;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > g.ancho) x1 = g.ancho;
    if (y1 > g.alto) y1 = g.alto;
    if (x0 >= x1 || y0 >= y1 || !g.pixeles) return;
    for (y = y0; y < y1; y++) {
        u32 *fila = g.pixeles + (size_t)y * g.ancho;
        for (x = x0; x < x1; x++) fila[x] = color;
    }
}

void dibujar_rect_z(Zona z, u32 color) {
    dibujar_rect(z.x, z.y, zona_derecha(z), zona_abajo(z), color);
}

void dibujar_marco(int x0, int y0, int x1, int y1, u32 color) {
    if (x1 <= x0 || y1 <= y0) return;
    dibujar_rect(x0, y0, x1, y0 + 1, color);
    dibujar_rect(x0, y1 - 1, x1, y1, color);
    dibujar_rect(x0, y0, x0 + 1, y1, color);
    dibujar_rect(x1 - 1, y0, x1, y1, color);
}

void dibujar_marco_z(Zona z, u32 color) {
    dibujar_marco(z.x, z.y, zona_derecha(z), zona_abajo(z), color);
}

void dibujar_linea(int x0, int y0, int x1, int y1, u32 color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        dibujar_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = err * 2;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

void dibujar_circulo(int cx, int cy, int radio, u32 color, int relleno) {
    int x = radio, y = 0, err = 1 - radio;
    if (radio <= 0) { dibujar_pixel(cx, cy, color); return; }
    while (x >= y) {
        if (relleno) {
            dibujar_rect(cx - x, cy + y, cx + x + 1, cy + y + 1, color);
            dibujar_rect(cx - x, cy - y, cx + x + 1, cy - y + 1, color);
            dibujar_rect(cx - y, cy + x, cx + y + 1, cy + x + 1, color);
            dibujar_rect(cx - y, cy - x, cx + y + 1, cy - x + 1, color);
        } else {
            dibujar_pixel(cx + x, cy + y, color);
            dibujar_pixel(cx - x, cy + y, color);
            dibujar_pixel(cx + x, cy - y, color);
            dibujar_pixel(cx - x, cy - y, color);
            dibujar_pixel(cx + y, cy + x, color);
            dibujar_pixel(cx - y, cy + x, color);
            dibujar_pixel(cx + y, cy - x, color);
            dibujar_pixel(cx - y, cy - x, color);
        }
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Texto                                                               */
/* ------------------------------------------------------------------ */
void dibujar_texto_fuente(int x, int y, const char *texto, u32 color, HFONT fuente) {
    if (!g.memdc || !texto || !texto[0]) return;
    SelectObject(g.memdc, fuente ? fuente : g.fuente);
    SetBkMode(g.memdc, TRANSPARENT);
    SetTextColor(g.memdc, color_a_win(color));
    TextOutA(g.memdc, x, y, texto, (int)strlen(texto));
}

void dibujar_texto(int x, int y, const char *texto, u32 color) {
    dibujar_texto_fuente(x, y, texto, color, g.fuente);
}

void dibujar_texto_centrado(Zona z, const char *texto, u32 color) {
    int w = texto_ancho(texto);
    int h = texto_alto();
    dibujar_texto(z.x + (z.ancho - w) / 2, z.y + (z.alto - h) / 2, texto, color);
}

void dibujar_texto_derecha(int x_derecha, int y, const char *texto, u32 color) {
    dibujar_texto(x_derecha - texto_ancho(texto), y, texto, color);
}

int texto_ancho_fuente(const char *texto, HFONT fuente) {
    SIZE s;
    if (!g.memdc || !texto || !texto[0]) return 0;
    SelectObject(g.memdc, fuente ? fuente : g.fuente);
    if (!GetTextExtentPoint32A(g.memdc, texto, (int)strlen(texto), &s)) return 0;
    return s.cx;
}

int texto_ancho(const char *texto) {
    return texto_ancho_fuente(texto, g.fuente);
}

int texto_alto(void) {
    TEXTMETRICA tm;
    if (!g.memdc) return 14;
    SelectObject(g.memdc, g.fuente);
    if (!GetTextMetricsA(g.memdc, &tm)) return 14;
    return tm.tmHeight;
}

/* ------------------------------------------------------------------ */
/* Buffer / ventana                                                    */
/* ------------------------------------------------------------------ */
static void liberar_buffer(void) {
    if (g.memdc && g.dib_anterior) {
        SelectObject(g.memdc, g.dib_anterior);
        g.dib_anterior = NULL;
    }
    if (g.dib) {
        DeleteObject(g.dib);
        g.dib = NULL;
    }
    if (g.memdc) {
        DeleteDC(g.memdc);
        g.memdc = NULL;
    }
    g.pixeles = NULL;
    g.pix_ancho = g.pix_alto = 0;
}

void app_redimensionar(int ancho, int alto) {
    HDC pantalla;
    BITMAPINFO bmi;
    if (ancho < 1) ancho = 1;
    if (alto < 1) alto = 1;
    if (ancho == g.pix_ancho && alto == g.pix_alto && g.pixeles) {
        g.ancho = ancho;
        g.alto = alto;
        return;
    }
    liberar_buffer();
    pantalla = GetDC(NULL);
    g.memdc = CreateCompatibleDC(pantalla);
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = ancho;
    bmi.bmiHeader.biHeight = -alto;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    g.dib = CreateDIBSection(pantalla, &bmi, DIB_RGB_COLORS, (void **)&g.pixeles, NULL, 0);
    if (g.memdc && g.dib) g.dib_anterior = SelectObject(g.memdc, g.dib);
    ReleaseDC(NULL, pantalla);
    g.ancho = ancho;
    g.alto = alto;
    g.pix_ancho = ancho;
    g.pix_alto = alto;
    if (g.pixeles) memset(g.pixeles, 0, (size_t)ancho * alto * sizeof(u32));
    lienzo_actualizar();
}

static void crear_fuentes(void) {
    g.fuente = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                           DEFAULT_PITCH | FF_SWISS, "Tahoma");
    g.fuente_peq = CreateFontA(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                               DEFAULT_PITCH | FF_SWISS, "Tahoma");
    g.fuente_negrita = CreateFontA(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, "Tahoma");
}

static void liberar_fuentes(void) {
    if (g.fuente) DeleteObject(g.fuente);
    if (g.fuente_peq) DeleteObject(g.fuente_peq);
    if (g.fuente_negrita) DeleteObject(g.fuente_negrita);
    g.fuente = g.fuente_peq = g.fuente_negrita = NULL;
}

void app_invalidar(void) {
    if (g.hwnd) InvalidateRect(g.hwnd, NULL, FALSE);
}

/* Titulo de ventana: "Texture Paint 2000 - <herramienta>". Bajo el cursor manda
 * la herramienta apuntada; si no, la activa. */
void app_actualizar_titulo(void) {
    char titulo[128];
    const char *nombre;
    if (!g.hwnd) return;
    if (g.vista == VISTA_MODULOS) {
        strcpy(titulo, "Texture Paint 2000 - Gestor de escenas");
    } else if (g.hover_herramienta >= 0) {
        nombre = ui_nombre_herramienta(g.hover_herramienta);
        _snprintf(titulo, sizeof(titulo), "Texture Paint 2000 - %s", nombre);
    } else {
        nombre = ui_nombre_herramienta((int)g.herramienta);
        if (nombre && nombre[0]) {
            _snprintf(titulo, sizeof(titulo), "Texture Paint 2000 - %s", nombre);
        } else {
            strcpy(titulo, "Texture Paint 2000");
        }
    }
    titulo[sizeof(titulo) - 1] = 0;
    SetWindowTextA(g.hwnd, titulo);
}

void app_dibujar(void) {
    ui_dibujar();
}

void app_estado(const char *formato, ...) {
    va_list args;
    char texto[256];
    va_start(args, formato);
    _vsnprintf(texto, sizeof(texto) - 1, formato, args);
    va_end(args);
    texto[sizeof(texto) - 1] = '\0';
    recortar_texto(g.estado, texto, sizeof(g.estado));
    g.estado_hasta = GetTickCount() + 3500;
    app_invalidar();
}

/* ------------------------------------------------------------------ */
/* Iconos de las herramientas (cargados de recursos PNG)               */
/* ------------------------------------------------------------------ */
static u32 *iconos[H_TOTAL];
static int icono_w[H_TOTAL], icono_h[H_TOTAL];
static u32 *icono_logo = NULL;
static int logo_w = 0, logo_h = 0;
static u32 *icono_guardar_tile = NULL;
static int guardar_tile_w = 0, guardar_tile_h = 0;

u32 *recurso_png(int id, int *ancho, int *alto) {
    HRSRC hr;
    HGLOBAL hg;
    void *datos;
    DWORD largo;
    hr = FindResourceA(NULL, MAKEINTRESOURCEA(id), RT_RCDATA);
    if (!hr) return NULL;
    hg = LoadResource(NULL, hr);
    if (!hg) return NULL;
    datos = LockResource(hg);
    largo = SizeofResource(NULL, hr);
    if (!datos || largo == 0) return NULL;
    return imagen_desde_memoria(datos, (int)largo, ancho, alto);
}

static void cargar_iconos(void) {
    static const int ids[H_TOTAL] = {
        IDR_PENCIL, IDR_BRUSH, IDR_CUBE, IDR_SPRAY, IDR_DRAFT, IDR_TEXT,
        IDR_DROPPER, IDR_ZOOM, IDR_LINE, IDR_CURVE, IDR_CIRCLE, IDR_POLYGON,
        IDR_RECTANGLE, IDR_ROUNDED, IDR_FREEFORM, IDR_SELRECT
    };
    int i;
    for (i = 0; i < H_TOTAL; i++) {
        iconos[i] = recurso_png(ids[i], &icono_w[i], &icono_h[i]);
    }
    icono_logo = recurso_png(IDR_LOGO, &logo_w, &logo_h);
    icono_guardar_tile = recurso_png(IDR_SAVE, &guardar_tile_w, &guardar_tile_h);
    g.info_imagen = recurso_png(IDR_GBS, &g.info_w, &g.info_h);
}

static void liberar_iconos(void) {
    int i;
    for (i = 0; i < H_TOTAL; i++) {
        if (iconos[i]) free(iconos[i]);
        iconos[i] = NULL;
    }
    if (icono_logo) free(icono_logo);
    icono_logo = NULL;
    if (icono_guardar_tile) free(icono_guardar_tile);
    icono_guardar_tile = NULL;
    if (g.info_imagen) free(g.info_imagen);
    g.info_imagen = NULL;
}

u32 *icono_herramienta(int indice, int *ancho, int *alto) {
    if (indice < 0 || indice >= H_TOTAL) return NULL;
    if (ancho) *ancho = icono_w[indice];
    if (alto) *alto = icono_h[indice];
    return iconos[indice];
}

u32 *icono_logo_imagen(int *ancho, int *alto) {
    if (ancho) *ancho = logo_w;
    if (alto) *alto = logo_h;
    return icono_logo;
}

u32 *icono_guardar_imagen(int *ancho, int *alto) {
    if (ancho) *ancho = guardar_tile_w;
    if (alto) *alto = guardar_tile_h;
    return icono_guardar_tile;
}

/* ------------------------------------------------------------------ */
/* Inicio / fin                                                        */
/* ------------------------------------------------------------------ */
static void cargar_preferencias(void) {
    g.herramienta = H_LAPIZ;
    g.grosor_idx = 0;
    g.opacidad = 100;
    g.angulo = 0;
    g.espaciado = 5;
    g.dureza = 100;
    g.textura = TEXTURA_CERDA;
    g.rocio = ROCIO_MEDIA;
    g.textura_menu_abierto = 0;
    g.mapa_ruta[0] = 0;
    g.modo_tiles = 0;
    g.modo_relleno = RELLENO_CONTIGUO;
    g.tolerancia_relleno = 32;
    g.frente = 0x00000000u;
    g.fondo = 0x00FFFFFFu;
    g.casilla_elegida = 0;
    g.zoom_idx = 13;
    g.menu_abierto = -1;
    g.hover_menu = -1;
    g.hover_herramienta = -1;
    g.hover_paleta = -1;
    g.hover_tile = -1;
    g.hover_capa = -1;
    g.vista = VISTA_PINTAR;
    g.panel_opciones = 1;
    g.panel_opciones_x = -1;
    g.panel_opciones_y = -1;
    g.hover_opcion = -1;
    g.opcion_arrastrada = -1;
    g.hover_cubo = -1;
    g.cubo_opacidad = 11;   /* 100 % */
    g.cubo_umbral = 2;      /* 10 % */
    g.cubo_tipo = CUBO_TIPO_PP;
    g.cubo_area = CUBO_AREA_SIMILARES;
    g.cubo_transparentes = 0;
    g.cubo_combinada = 1;
    g.cubo_diagonales = 0;
    g.cubo_alisado = 0;
    g.cubo_compuesto = 1;
    g.cubo_arrastre = -1;
    g.patron_visible = 0;
    g.patron_x = -1;
    g.patron_y = -1;
    g.tiles_scroll = 0;
    g.capa_menu_abierto = -1;
    g.capa_menu_x = 0;
    g.capa_menu_y = 0;
    g.confirmar_borrado = 0;
    g.confirmar_tile = -1;
    g.mapa_tile_sel = -1;
    g.lienzo_tirador = -1;
    g.lienzo_hover = -1;
    g.sel_tirador = -1;
    /* text state initialized in texto_iniciar */
    g.boton_pintura = -1;
    g.info_x = 250;
    g.info_y = 130;
}

static void cargar_paleta(void) {
    static const u32 base[20] = {
        0x00000000u, 0x00555555u, 0x00800000u, 0x00FF0000u,
        0x00FF8000u, 0x00FFFF00u, 0x00008000u, 0x0000BFFFu,
        0x00303F9Fu, 0x00800080u,
        0x00FFFFFFu, 0x00C0C0C0u, 0x00C49A6Cu, 0x00FFB6C1u,
        0x00FFD700u, 0x00FFFF99u, 0x0090EE90u, 0x0000FFFFu,
        0x009FA8DAu, 0x00DDA0DDu
    };
    int i;
    for (i = 0; i < MINI_TOTAL; i++) {
        if (i < 20) {
            g.paleta[i] = base[i];
            g.paleta_llena[i] = 1;
        } else {
            g.paleta[i] = MINI_VACIO;
            g.paleta_llena[i] = 0;
        }
    }
}

int app_iniciar(void) {
    char carpeta[MAX_PATH];
    memset(&g, 0, sizeof(g));
    g.modulo_drag_i = -1;
    g.modulo_snap_linea_x = -1;
    g.modulo_snap_linea_y = -1;
    cargar_preferencias();
    cargar_paleta();

    if (!imagen_iniciar()) {
        MessageBoxA(NULL, "No se pudo iniciar GDI+.", TP_NOMBRE, MB_ICONERROR);
        return 0;
    }

    g.doc = doc_nuevo(LIENZO_INICIAL, LIENZO_INICIAL, LIENZO);
    if (!g.doc.capas[0].pixeles) {
        MessageBoxA(NULL, "No hay memoria para el lienzo inicial.", TP_NOMBRE, MB_ICONERROR);
        imagen_terminar();
        return 0;
    }
    doc_componer_todo(&g.doc);
    pila_iniciar(&g.deshacer, MAX_UNDO);
    pila_iniciar(&g.rehacer, MAX_UNDO);
    seleccion_limpiar(&g.sel);
    mapa_iniciar();
    tiles_modo_iniciar();
    texto_iniciar();
    pestanas_iniciar();
    escenas_iniciar();

    crear_fuentes();
    cargar_iconos();
    tiles_cargar();

    obtener_ruta_exe(carpeta, sizeof(carpeta));
    unir_ruta(carpeta, sizeof(carpeta), carpeta, "tiles");
    crear_carpeta(carpeta);

    /* El lienzo inicial de 80x80 arranca al 1000% (zoom 13), centrado en el
     * area de trabajo y recortado si no cabe. El ajuste exacto se hace al
     * conocer el tamano (app_redimensionar). */
    g.zoom_idx = 13;
    return 1;
}

void app_terminar(void) {
    int i;
    pestanas_terminar();
    pila_liberar(&g.deshacer);
    pila_liberar(&g.rehacer);
    doc_liberar(&g.doc);
    seleccion_liberar(&g.sel);
    mapa_liberar();
    tiles_modo_liberar();
    texto_terminar();
    tiles_liberar();
    liberar_iconos();
    if (g.portapapeles) free(g.portapapeles);
    if (g.preview) free(g.preview);
    for (i = 0; i < g.sel.flotante_w * 0; i++) { }
    liberar_fuentes();
    liberar_buffer();
    imagen_terminar();
}

/* ------------------------------------------------------------------ */
/* Cambios de vista y documentos                                       */
/* ------------------------------------------------------------------ */
/* Sustituye el documento compartido por otro. Los modos de las pestanas se
 * conservan, pero el historial, la seleccion y la rejilla de tiles empiezan
 * de cero porque pertenecen al documento anterior. */
static void reemplazar_documento(Documento *nuevo, const char *ruta) {
    /* Cancela cualquier edicion en curso: pertenece al documento anterior. */
    if (g.texto_activo) texto_cancelar();
    if (g.preview_activa || g.curva_activa) pintura_figura_cancelar();
    g.pintando = 0;
    g.poligono_activo = 0;
    g.lienzo_tirador = -1;
    g.sel_tirador = -1;
    doc_liberar(&g.doc);
    g.doc = *nuevo;
    memset(nuevo, 0, sizeof(*nuevo));
    pila_limpiar(&g.deshacer);
    pila_limpiar(&g.rehacer);
    seleccion_limpiar(&g.sel);
    tiles_modo_limpiar();
    escenas_iniciar();
    if (ruta && ruta[0])
        recortar_texto(g.doc_ruta, ruta, sizeof(g.doc_ruta));
    else
        g.doc_ruta[0] = 0;
    pestanas_documento_reemplazado();
    /* El documento nuevo se muestra en el lienzo, no en el gestor. */
    if (g.vista == VISTA_MODULOS) g.vista = VISTA_PINTAR;
    app_actualizar_titulo();
    app_invalidar();
}

void app_nueva_imagen(int ancho, int alto) {
    Documento nuevo;
    ancho = limitar(ancho, MIN_LADO, LIENZO_MAX);
    alto = limitar(alto, MIN_LADO, LIENZO_MAX);
    nuevo = doc_nuevo(ancho, alto, LIENZO);
    if (!nuevo.capas[0].pixeles) {
        doc_liberar(&nuevo);
        dialogo_error("No hay memoria para crear la imagen.");
        return;
    }
    doc_componer_todo(&nuevo);
    reemplazar_documento(&nuevo, NULL);
}

void app_abrir_imagen(const char *ruta) {
    int w = 0, h = 0;
    u32 *pix = imagen_cargar(ruta, &w, &h);
    Documento nuevo;
    if (!pix) {
        dialogo_error("No se pudo abrir la imagen.");
        return;
    }
    if (w < 1 || h < 1 || w > LIENZO_MAX || h > LIENZO_MAX) {
        free(pix);
        dialogo_error("La imagen es demasiado grande o no es valida.");
        return;
    }
    nuevo = doc_nuevo(w, h, LIENZO);
    if (!nuevo.capas[0].pixeles) {
        free(pix);
        doc_liberar(&nuevo);
        dialogo_error("No hay memoria para abrir la imagen.");
        return;
    }
    /* La imagen se coloca en la capa base. */
    copiar_pixeles(nuevo.capas[0].pixeles, pix, w * h);
    free(pix);
    doc_componer_todo(&nuevo);
    reemplazar_documento(&nuevo, ruta);
    app_estado("Imagen abierta: %dx%d", w, h);
}

void app_guardar_imagen(const char *ruta, int png) {
    doc_componer_todo(&g.doc);
    if (png ? imagen_guardar_png(ruta, g.doc.compuesto, g.doc.ancho, g.doc.alto)
            : imagen_guardar_bmp(ruta, g.doc.compuesto, g.doc.ancho, g.doc.alto)) {
        pestanas_marcar_guardada(ruta);
        app_estado("Guardado: %s", ruta);
    } else {
        dialogo_error("No se pudo guardar la imagen.");
    }
}

void app_cambiar_vista(Vista vista) {
    if (vista == g.vista) return;
    g.vista = vista;
    g.menu_abierto = -1;
    /* Cancela cualquier arrastre del gestor al cambiar de modo. */
    g.modulo_arrastrando = 0;
    g.modulo_drag_i = -1;
    g.modulo_snap_linea_x = -1;
    g.modulo_snap_linea_y = -1;
    g.hover_modulo_boton = 0;
    g.hover_escena = -1;
    g.hover_escena_nueva = 0;
    app_actualizar_titulo();
    app_invalidar();
}

void app_alternar_mapa(void) {
    if (g.vista == VISTA_MAPA) {
        app_cambiar_vista(VISTA_PINTAR);
    } else {
        app_cambiar_vista(VISTA_MAPA);
        mapa_reiniciar();
    }
}

/* Gestor de escenas: espacio de trabajo con la lista de escenas en el panel
 * izquierdo. Pulsar de nuevo la etiqueta vuelve a Pintar. */
void app_alternar_gestor_modulos(void) {
    if (g.vista == VISTA_MODULOS) {
        app_cambiar_vista(VISTA_PINTAR);
        return;
    }
    /* Cierra cualquier edicion en curso antes de ocultar el lienzo. */
    if (g.texto_activo) texto_confirmar();
    if (g.curva_activa) pintura_figura_confirmar(0, 0, 0);
    if (g.preview_activa) pintura_figura_cancelar();
    if (g.sel.activa && g.sel.flotante)
        seleccion_soltar(&g.sel, &g.doc, &g.deshacer);
    g.pintando = 0;
    g.poligono_activo = 0;
    g.lienzo_tirador = -1;
    g.sel_tirador = -1;
    g.capa_menu_abierto = -1;
    g.confirmar_borrado = 0;
    g.confirmar_tile = -1;
    if (g.num_escenas == 0) escenas_iniciar();
    app_cambiar_vista(VISTA_MODULOS);
}

void app_poner_herramienta(int herramienta) {
    if (herramienta < 0 || herramienta >= H_TOTAL) return;
    if (g.herramienta == H_TEXTO && g.texto_activo) texto_confirmar();
    if (g.curva_activa) pintura_figura_confirmar(0, 0, 0);
    if (g.sel.activa && g.sel.flotante) seleccion_soltar(&g.sel, &g.doc, &g.deshacer);
    g.herramienta = (Herramienta)herramienta;
    g.poligono_activo = 0;
    app_actualizar_titulo();
    app_invalidar();
}

void app_agregar_color_paleta(u32 color) {
    int i;
    for (i = 0; i < MINI_PERSONALIZADOS; i++) {
        if (g.paleta_llena[i] && g.paleta[i] == color) {
            g.casilla_elegida = 0;
            return;
        }
    }
    for (i = MINI_PERSONALIZADOS; i < MINI_TOTAL; i++) {
        if (!g.paleta_llena[i]) {
            g.paleta[i] = color;
            g.paleta_llena[i] = 1;
            g.casilla_elegida = 0;
            return;
        }
    }
    for (i = MINI_PERSONALIZADOS; i < MINI_TOTAL - 1; i++) {
        g.paleta[i] = g.paleta[i + 1];
        g.paleta_llena[i] = g.paleta_llena[i + 1];
    }
    g.paleta[MINI_TOTAL - 1] = color;
    g.paleta_llena[MINI_TOTAL - 1] = 1;
    g.casilla_elegida = 0;
}

/* ------------------------------------------------------------------ */
/* Dialogos nativos                                                    */
/* ------------------------------------------------------------------ */
void dialogo_error(const char *mensaje) {
    MessageBoxA(g.hwnd, mensaje, TP_NOMBRE, MB_OK | MB_ICONERROR);
}

void dialogo_informacion(const char *titulo, const char *mensaje) {
    MessageBoxA(g.hwnd, mensaje, titulo, MB_OK | MB_ICONINFORMATION);
}

int dialogo_confirmar(const char *titulo, const char *mensaje) {
    return MessageBoxA(g.hwnd, mensaje, titulo, MB_YESNO | MB_ICONQUESTION) == IDYES;
}

static OPENFILENAMEA preparar_dialogo(char *ruta, const char *filtro, const char *defecto) {
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    if (ruta[0] == '\0' && defecto) recortar_texto(ruta, defecto, MAX_PATH);
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = filtro;
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    return ofn;
}

void dialogo_nuevo(void) {
    if (g.doc.modificado) {
        if (!dialogo_confirmar("Nuevo lienzo",
                "El documento tiene cambios sin guardar.\n"
                "¿Crear uno nuevo de todos modos?"))
            return;
    }
    app_nueva_imagen(LIENZO_INICIAL, LIENZO_INICIAL);
}

void dialogo_abrir_imagen(void) {
    char ruta[MAX_PATH];
    OPENFILENAMEA ofn;
    ruta[0] = '\0';
    ofn = preparar_dialogo(ruta,
        "Imagenes\0*.png;*.bmp;*.jpg;*.jpeg;*.gif;*.tif;*.tiff\0Todos los archivos\0*.*\0\0",
        "");
    ofn.Flags |= OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameA(&ofn)) return;
    if (g.doc.modificado) {
        if (!dialogo_confirmar("Abrir imagen",
                "El documento tiene cambios sin guardar.\n"
                "¿Abrir otra imagen de todos modos?"))
            return;
    }
    app_abrir_imagen(ruta);
}

void dialogo_guardar_png(void) {
    char ruta[MAX_PATH];
    OPENFILENAMEA ofn;
    const char *actual = pestanas_ruta(g.pestana_activa);
    ruta[0] = '\0';
    if (actual && actual[0]) ruta_nombre(ruta, sizeof(ruta), actual);
    ofn = preparar_dialogo(ruta, "Imagen PNG\0*.png\0Todos los archivos\0*.*\0\0", "textura.png");
    ofn.lpstrDefExt = "png";
    ofn.Flags |= OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameA(&ofn)) app_guardar_imagen(ruta, 1);
}

void dialogo_exportar_bmp(void) {
    char ruta[MAX_PATH];
    OPENFILENAMEA ofn;
    ruta[0] = '\0';
    ofn = preparar_dialogo(ruta, "Imagen BMP\0*.bmp\0Todos los archivos\0*.*\0\0", "textura.bmp");
    ofn.lpstrDefExt = "bmp";
    ofn.Flags |= OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameA(&ofn)) app_guardar_imagen(ruta, 0);
}

void dialogo_elegir_color(int para_fondo) {
    CHOOSECOLORA cc;
    static COLORREF personalizados[16];
    u32 inicial = para_fondo ? g.fondo : g.frente;
    memset(&cc, 0, sizeof(cc));
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = g.hwnd;
    cc.rgbResult = color_a_win(inicial);
    cc.lpCustColors = personalizados;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR;
    if (ChooseColorA(&cc)) {
        u32 color = color_de_win(cc.rgbResult);
        if (para_fondo) g.fondo = color;
        else g.frente = color;
        app_agregar_color_paleta(color);
        g.casilla_elegida = para_fondo ? 1 : 0;
        app_estado("Color %06X", color & 0xFFFFFF);
    }
    app_invalidar();
}

int dialogo_elegir_color_valor(u32 inicial, u32 *salida) {
    CHOOSECOLORA cc;
    static COLORREF personalizados[16];
    u32 *color = salida;
    memset(&cc, 0, sizeof(cc));
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = g.hwnd;
    cc.rgbResult = color_a_win(inicial);
    cc.lpCustColors = personalizados;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR;
    if (ChooseColorA(&cc)) {
        u32 c = color_de_win(cc.rgbResult);
        if (color) *color = c;
        app_agregar_color_paleta(c);
        g.casilla_elegida = 0;
        app_estado("Color %06X", c & 0xFFFFFF);
        app_invalidar();
        return 1;
    }
    app_invalidar();
    return 0;
}

/* ------------------------------------------------------------------ */
/* Ventana                                                             */
/* ------------------------------------------------------------------ */
static void aplicar_icono(HWND hwnd) {
    HICON icono = (HICON)LoadImageA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(IDI_APP), IMAGE_ICON,
                                    0, 0, LR_DEFAULTSIZE);
    if (icono) {
        SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icono);
        SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icono);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            RECT r;
            GetClientRect(hwnd, &r);
            app_redimensionar(r.right - r.left, r.bottom - r.top);
            aplicar_icono(hwnd);
            SetTimer(hwnd, 1, 200, NULL);
            return 0;
        }
        case WM_SIZE:
            app_redimensionar(LOWORD(lp), HIWORD(lp));
            app_invalidar();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (!g.pixeles) app_redimensionar(g.ancho ? g.ancho : ANCHO_INICIAL, g.alto ? g.alto : ALTO_INICIAL);
            app_dibujar();
            if (g.memdc && g.pixeles) {
                BitBlt(dc, 0, 0, g.ancho, g.alto, g.memdc, 0, 0, SRCCOPY);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (!g.raton_dentro) {
                TRACKMOUSEEVENT tme;
                g.raton_dentro = 1;
                memset(&tme, 0, sizeof(tme));
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
            }
            g.raton_x = x;
            g.raton_y = y;
            app_mover(x, y, (int)wp);
            return 0;
        }
        case WM_MOUSELEAVE:
            g.raton_dentro = 0;
            g.raton_x = -100;
            g.raton_y = -100;
            g.hover_herramienta = -1;
            app_actualizar_titulo();
            app_invalidar();
            return 0;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            SetFocus(hwnd);
            g.boton_izq = 1;
            app_pulsar(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 0);
            return 0;
        case WM_LBUTTONUP:
            g.boton_izq = 0;
            app_soltar(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 0);
            if (!g.boton_der && !g.boton_medio) ReleaseCapture();
            return 0;
        case WM_RBUTTONDOWN:
            SetCapture(hwnd);
            SetFocus(hwnd);
            g.boton_der = 1;
            app_pulsar(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 1);
            return 0;
        case WM_RBUTTONUP:
            g.boton_der = 0;
            app_soltar(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 1);
            if (!g.boton_izq && !g.boton_medio) ReleaseCapture();
            return 0;
        case WM_MBUTTONDOWN:
            SetCapture(hwnd);
            g.boton_medio = 1;
            app_pulsar(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 2);
            return 0;
        case WM_MBUTTONUP:
            g.boton_medio = 0;
            app_soltar(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 2);
            if (!g.boton_izq && !g.boton_der) ReleaseCapture();
            return 0;
        case WM_LBUTTONDBLCLK:
            app_doble_clic(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        case WM_MOUSEWHEEL: {
            POINT p;
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            p.x = GET_X_LPARAM(lp);
            p.y = GET_Y_LPARAM(lp);
            ScreenToClient(hwnd, &p);
            app_rueda(p.x, p.y, delta, (GetKeyState(VK_CONTROL) & 0x8000) != 0);
            return 0;
        }
        case WM_KEYDOWN: {
            int ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            int shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            app_tecla((int)wp, ctrl, shift);
            return 0;
        }
        case WM_CHAR:
            if (wp >= 32 || wp == 8 || wp == 9 || wp == 13) app_caracter((int)wp);
            return 0;
        case WM_TIMER:
            if (wp == 1) {
                if (g.sel.activa ||
                    (g.estado_hasta && GetTickCount() > g.estado_hasta) ||
                    g.texto_activo) {
                    app_invalidar();
                }
            }
            return 0;
        case WM_SETCURSOR: {
            int zona = LOWORD(lp);
            if (zona == HTCLIENT) {
                if (lienzo_contiene(g.raton_x, g.raton_y) && g.vista == VISTA_PINTAR) {
                    SetCursor(LoadCursorA(NULL, IDC_CROSS));
                } else {
                    SetCursor(LoadCursorA(NULL, IDC_ARROW));
                }
                return TRUE;
            }
            break;
        }
        case WM_CLOSE:
            if (pestanas_hay_cambios()) {
                if (!dialogo_confirmar("Salir", "El documento tiene cambios sin guardar.\n¿Salir de todos modos?"))
                    return 0;
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* Valor de un argumento de linea de comandos: `--nombre valor` o
 * `--nombre=valor`, con comillas opcionales. Devuelve un puntero dentro de
 * `cmd` (que es modificable). */
static char *arg_valor(char *cmd, const char *nombre) {
    char *p = cmd;
    size_t n = strlen(nombre);
    if (!p) return NULL;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (_strnicmp(p, nombre, n) == 0 &&
            (p[n] == '=' || p[n] == ' ' || p[n] == '\t' || p[n] == 0)) {
            p += n;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '=') {
                p++;
                while (*p == ' ' || *p == '\t') p++;
            }
            if (*p == '"') {
                char *fin;
                p++;
                fin = strchr(p, '"');
                if (fin) *fin = 0;
                return p;
            }
            return p;
        }
        if (*p == '"') {
            p++;
            while (*p && *p != '"') p++;
            if (*p) p++;
        } else {
            while (*p && *p != ' ' && *p != '\t') p++;
        }
    }
    return NULL;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int mostrar) {
    WNDCLASSEXA wc;
    HWND hwnd;
    MSG msg;
    RECT r;
    HMODULE user32;
    char *captura;
    (void)prev;

    /* DPI: solo si existe (Vista o superior). En XP no hace falta. */
    user32 = GetModuleHandleA("user32.dll");
    if (user32) {
        typedef BOOL(WINAPI * FnSetDPIAware)(void);
        FnSetDPIAware fn = (FnSetDPIAware)GetProcAddress(user32, "SetProcessDPIAware");
        if (fn) fn();
    }

    if (!app_iniciar()) return 1;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = "TexturePaint2000";
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(IDI_APP));
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExA(&wc)) {
        app_terminar();
        return 1;
    }

    r.left = 0; r.top = 0;
    r.right = ANCHO_INICIAL; r.bottom = ALTO_INICIAL;
    AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
    hwnd = CreateWindowExA(0, "TexturePaint2000", TP_NOMBRE " " TP_VERSION,
                           WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top,
                           NULL, NULL, inst, NULL);
    if (!hwnd) {
        app_terminar();
        return 1;
    }
    g.hwnd = hwnd;
    app_actualizar_titulo();

    /* Modo de captura interna: dibuja un fotograma en el buffer y lo guarda
     * como BMP. Sirve para comparar la interfaz sin depender de PrintWindow
     * ni de la pantalla. */
    captura = arg_valor(cmd, "--captura");
    if (captura) {
        int ok;
        char *menu_arg = arg_valor(cmd, "--menu");
        char *vista_arg = arg_valor(cmd, "--vista");
        char *raton_arg = arg_valor(cmd, "--raton");
        char *herr_arg = arg_valor(cmd, "--herramienta");
        char *clics_arg = arg_valor(cmd, "--clics");
        char *arrastre_arg = arg_valor(cmd, "--arrastre");
        if (menu_arg) g.menu_abierto = atoi(menu_arg);
        if (vista_arg && strncmp(vista_arg, "mapa", 4) == 0) g.vista = VISTA_MAPA;
        if (strstr(cmd, "--tiles") != NULL) tiles_modo_entrar();
        if (strstr(cmd, "--gestor") != NULL) g.vista = VISTA_MODULOS;
        /* Elementos de prueba del gestor: "x,y;x,y". */
        {
            char *mods_arg = arg_valor(cmd, "--modulos");
            if (mods_arg && g.vista == VISTA_MODULOS) {
                const char *p = mods_arg;
                while (*p) {
                    int mx = 0, my = 0, n = 0;
                    if (sscanf(p, "%d,%d%n", &mx, &my, &n) != 2) break;
                    modulos_insertar(mx, my);
                    p += n;
                    while (*p == ';' || *p == ' ' || *p == ',') p++;
                }
            }
        }
        if (strstr(cmd, "--info") != NULL) g.info_visible = 1;
        if (strstr(cmd, "--patron") != NULL) g.patron_visible = 1;
        if (strstr(cmd, "--mayus") != NULL) g.shift_simulado = 1;
        if (herr_arg) {
            int h = atoi(herr_arg);
            if (h >= 0 && h < H_TOTAL) g.herramienta = (Herramienta)h;
        }
        if (raton_arg) {
            int rx = 0, ry = 0;
            if (sscanf(raton_arg, "%d,%d", &rx, &ry) == 2) {
                g.raton_x = rx;
                g.raton_y = ry;
            }
        }
        /* Clics simulados "x,y;x,y": mover, pulsar y soltar el boton izquierdo
         * como si llegasen del sistema. Permite probar cajas de texto y otros
         * gestos sin depender del foco de la ventana. */
        if (clics_arg) {
            const char *p = clics_arg;
            while (*p) {
                int cx = 0, cy = 0, n = 0;
                if (sscanf(p, "%d,%d%n", &cx, &cy, &n) == 2) {
                    app_mover(cx, cy, 0);
                    g.boton_izq = 1;
                    app_pulsar(cx, cy, 0);
                    g.boton_izq = 0;
                    app_soltar(cx, cy, 0);
                    p += n;
                } else {
                    break;
                }
                while (*p == ';' || *p == ' ' || *p == ',') p++;
            }
        }
        /* Arrastre simulado "x0,y0;x1,y1": pulsa en el primer punto, recorre
         * el trayecto con el boton izquierdo apretado y suelta en el segundo.
         * Sirve para probar gestos como el redimensionado del lienzo. */
        if (arrastre_arg) {
            int ax0 = 0, ay0 = 0, ax1 = 0, ay1 = 0;
            if (sscanf(arrastre_arg, "%d,%d;%d,%d", &ax0, &ay0, &ax1, &ay1) == 4) {
                int paso;
                app_mover(ax0, ay0, 0);
                g.boton_izq = 1;
                app_pulsar(ax0, ay0, 0);
                for (paso = 1; paso <= 24; paso++) {
                    int mx = ax0 + (ax1 - ax0) * paso / 24;
                    int my = ay0 + (ay1 - ay0) * paso / 24;
                    app_mover(mx, my, 1);
                }
                g.boton_izq = 0;
                app_soltar(ax1, ay1, 0);
            }
        }
        /* Posicion final del raton: actualiza tambien el hover, como un
         * movimiento real, para capturar resaltes y cursores. */
        if (raton_arg) {
            int rx = 0, ry = 0;
            if (sscanf(raton_arg, "%d,%d", &rx, &ry) == 2) app_mover(rx, ry, 0);
        }
        app_dibujar();
        ok = imagen_guardar_bmp(captura, g.pixeles, g.ancho, g.alto);
        DestroyWindow(hwnd);
        g.hwnd = NULL;
        app_terminar();
        return ok ? 0 : 2;
    }

    ShowWindow(hwnd, mostrar == SW_SHOWMAXIMIZED ? SW_SHOWMAXIMIZED : SW_SHOW);
    UpdateWindow(hwnd);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    app_terminar();
    return 0;
}
