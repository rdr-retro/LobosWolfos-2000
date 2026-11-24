/* ==========================================================================
 * Nucleo: buffer de dibujo, primitivas, ventana, bucle de mensajes y dialogos.
 * ========================================================================== */
#include "app.h"
#include "logic3.h"
#include "proyecto_modelo.h"
#include "modelo_log.h"
#include "modelo_logic3.h"
#include "migracion.h"
#include "inicio.h"

App g;

/* Banda del area cliente que se usa para redimensionar sin marco nativo. */
#define BORDE_VENTANA 6

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

/* Parte un valor de flag escrito como "a|b|c" sin modificar la original. */
static void tramo_flag_modelo(char *dst, int max, const char **p) {
    const char *inicio = *p ? *p : "";
    const char *fin = strchr(inicio, '|');
    int n = fin ? (int)(fin - inicio) : (int)strlen(inicio);
    if (max <= 0) return;
    if (n >= max) n = max - 1;
    memcpy(dst, inicio, (size_t)n);
    dst[n] = '\0';
    *p = fin ? fin + 1 : inicio + strlen(inicio);
}

int termina_con(const char *texto, const char *sufijo) {
    size_t a, b;
    if (!texto || !sufijo) return 0;
    a = strlen(texto);
    b = strlen(sufijo);
    if (b > a) return 0;
    return _stricmp(texto + a - b, sufijo) == 0;
}

/* Fixture en memoria del contrato del plan 16.1. Solo se construye cuando
 * el CLI pide --volcar-modelo: no abre ventanas, no lee .log y no toca
 * LOGIC3. Las pruebas comparan el volcado determinista de este grafo. */
static void volcar_modelo_minimo(const char *ruta) {
    ProjectModel *m = pm_crear();
    EntidadModelo *e;
    int id_proyecto, id_imagen, id_mapa, id_escena;
    int id_actor, id_instancia, id_control, id_conexion;
    char diagnostico[PM_ERROR_MAX];

    if (!m) return;

    id_proyecto = pm_anadir(m, PM_KIND_PROYECTO, "Primer juego");
    e = pm_entidad(m, id_proyecto);
    pm_origen(e, "ejemplos/juego_ia.log", 2);
    pm_ui(e, 20, 20, 180, 40, 0, 0x003278E0u, 0);
    pm_propiedad(e, "ancho", "640", 2);
    pm_propiedad(e, "alto", "480", 2);
    pm_propiedad(e, "fps", "60", 2);
    pm_propiedad(e, "fondo", "#102030", 2);

    id_imagen = pm_anadir_con_id(m, 10, PM_KIND_RECURSO, "Jugador");
    e = pm_entidad(m, id_imagen);
    pm_origen(e, "ejemplos/juego_ia.log", 5);
    pm_propiedad(e, "tipo", "imagen", 5);
    pm_propiedad(e, "ruta", "tiles/jugador.png", 5);
    pm_propiedad(e, "origen", "pintado", 5);

    id_mapa = pm_anadir_con_id(m, 11, PM_KIND_RECURSO, "Pradera");
    e = pm_entidad(m, id_mapa);
    pm_origen(e, "ejemplos/juego_ia.log", 6);
    pm_propiedad(e, "tipo", "mapa", 6);
    pm_propiedad(e, "ruta", "maps/pradera.mav", 6);

    id_escena = pm_anadir_con_id(m, 20, PM_KIND_ESCENA, "Nivel 1");
    e = pm_entidad(m, id_escena);
    pm_origen(e, "ejemplos/juego_ia.log", 8);
    pm_ui(e, 20, 40, 180, 48, 0, 0x003278E0u, 0);
    pm_referencia(e, "mapa", id_mapa, "maps/pradera.mav", 8);
    pm_propiedad(e, "inicial", "1", 8);

    id_actor = pm_anadir_con_id(m, 21, PM_KIND_ACTOR, "Jugador");
    e = pm_entidad(m, id_actor);
    pm_origen(e, "ejemplos/juego_ia.log", 9);
    pm_referencia(e, "recurso", id_imagen, "tiles/jugador.png", 9);
    pm_propiedad(e, "visible", "1", 9);

    id_instancia = pm_anadir_con_id(m, 22, PM_KIND_INSTANCIA, "Jugador en Nivel 1");
    e = pm_entidad(m, id_instancia);
    pm_origen(e, "ejemplos/juego_ia.log", 10);
    pm_referencia(e, "actor", id_actor, NULL, 10);
    pm_referencia(e, "escena", id_escena, NULL, 10);
    pm_propiedad(e, "x", "80", 10);
    pm_propiedad(e, "y", "80", 10);
    pm_propiedad(e, "capa", "10", 10);

    id_control = pm_anadir_con_id(m, 23, PM_KIND_CONTROL, "Jugador 1");
    e = pm_entidad(m, id_control);
    pm_origen(e, "ejemplos/juego_ia.log", 11);
    pm_ui(e, 20, 118, 180, 48, 0, 0x003278E0u, 0);
    pm_referencia(e, "instancia", id_instancia, NULL, 11);
    pm_propiedad(e, "dispositivo", "teclado", 11);
    pm_propiedad(e, "teclas", "derecha", 11);

    id_conexion = pm_conectar(m, id_imagen, "salida", id_actor, "textura", 12);
    pm_origen(pm_entidad(m, id_conexion), "ejemplos/juego_ia.log", 12);
    id_conexion = pm_conectar(m, id_control, "salida", id_instancia,
                              "movimiento", 13);
    pm_origen(pm_entidad(m, id_conexion), "ejemplos/juego_ia.log", 13);

    pm_validar(m, diagnostico, sizeof(diagnostico));
    pm_volcar(m, ruta);
    pm_destruir(m);
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
    /* La lista de elementos del gestor puede mostrar menos filas tras
     * reducir el alto de la ventana: el scroll no debe pasarse del final. */
    {
        int orden[MODULOS_MAX], hijos[MODULOS_MAX];
        int filas = elementos_ordenar_lista(orden, hijos);
        int maximo = filas - elementos_visibles();
        if (maximo < 0) maximo = 0;
        if (g.elementos_scroll > maximo) g.elementos_scroll = maximo;
    }
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

/* Titulo de ventana: "LobosWolfos 2000 - <herramienta>". Bajo el cursor manda
 * la herramienta apuntada; si no, la activa. */
void app_actualizar_titulo(void) {
    char titulo[128];
    const char *nombre;
    if (!g.hwnd) return;
    if (g.vista == VISTA_MODULOS) {
        strcpy(titulo, "LobosWolfos 2000 - Gestor de lógica");
    } else if (g.vista == VISTA_MENUS) {
        strcpy(titulo, "LobosWolfos 2000 - Editor de menús");
    } else if (g.vista == VISTA_INICIO) {
        strcpy(titulo, "LobosWolfos 2000 - Inicio del proyecto");
    } else if (g.hover_herramienta >= 0) {
        nombre = ui_nombre_herramienta(g.hover_herramienta);
        _snprintf(titulo, sizeof(titulo), "LobosWolfos 2000 - %s", nombre);
    } else {
        nombre = ui_nombre_herramienta((int)g.herramienta);
        if (nombre && nombre[0]) {
            _snprintf(titulo, sizeof(titulo), "LobosWolfos 2000 - %s", nombre);
        } else {
            strcpy(titulo, "LobosWolfos 2000");
        }
    }
    titulo[sizeof(titulo) - 1] = 0;
    recortar_texto(g.titulo, titulo, sizeof(g.titulo));
    /* El titulo lleva acentos en UTF-8: se convierte a UTF-16 para que
     * SetWindowTextW los muestre bien en la barra de titulos. */
    {
        WCHAR wtitulo[128];
        if (MultiByteToWideChar(CP_UTF8, 0, titulo, -1, wtitulo, 128) > 0)
            SetWindowTextW(g.hwnd, wtitulo);
        else
            SetWindowTextA(g.hwnd, titulo);
    }
    app_invalidar();
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
static u32 *icono_media[BARRA_MEDIA_BOTONES];
static int media_w[BARRA_MEDIA_BOTONES], media_h[BARRA_MEDIA_BOTONES];

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
    icono_media[0] = recurso_png(IDR_PLAY, &media_w[0], &media_h[0]);
    icono_media[1] = recurso_png(IDR_PAUSE, &media_w[1], &media_h[1]);
    icono_media[2] = recurso_png(IDR_STOP, &media_w[2], &media_h[2]);
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
    for (i = 0; i < BARRA_MEDIA_BOTONES; i++) {
        if (icono_media[i]) free(icono_media[i]);
        icono_media[i] = NULL;
    }
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

/* Icono de un boton de reproduccion: 0 = play, 1 = pausa, 2 = stop. */
u32 *icono_media_imagen(int indice, int *ancho, int *alto) {
    if (indice < 0 || indice >= BARRA_MEDIA_BOTONES) return NULL;
    if (ancho) *ancho = media_w[indice];
    if (alto) *alto = media_h[indice];
    return icono_media[indice];
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
    g.zoom_idx = 10;   /* 500 % al arrancar */
    g.menu_abierto = -1;
    g.hover_menu = -1;
    g.hover_media = -1;
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
    g.ventana_activa = 1;
    g.titulo_boton_pulsado = -1;
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
    pila_iniciar(&g.deshacer_menus, MAX_UNDO);
    pila_iniciar(&g.rehacer_menus, MAX_UNDO);
    g.doc_menus_listo = 0;
    seleccion_limpiar(&g.sel);
    mapa_iniciar();
    tiles_modo_iniciar();
    texto_iniciar();
    pestanas_iniciar();
    logica_iniciar();

    crear_fuentes();
    cargar_iconos();
    tiles_cargar();

    /* Carpetas del proyecto junto al ejecutable: los tiles, los mapas, los
     * trajes, las logicas y los titulos se guardan siempre en su carpeta. */
    obtener_ruta_exe(carpeta, sizeof(carpeta));
    {
        static const char *carpetas[] = {
            CARPETA_TILES_TEXTO, CARPETA_MAPS_TEXTO,
            CARPETA_TRAJES_TEXTO, CARPETA_LOGIC_TEXTO,
            CARPETA_TITLES_TEXTO
        };
        int i;
        for (i = 0; i < (int)(sizeof(carpetas) / sizeof(carpetas[0])); i++) {
            char ruta_carpeta[MAX_PATH];
            unir_ruta(ruta_carpeta, sizeof(ruta_carpeta), carpeta, carpetas[i]);
            crear_carpeta(ruta_carpeta);
        }
    }

    /* El lienzo inicial de 80x80 arranca al 500% (zoom 10), centrado en el
     * area de trabajo y recortado si no cabe. El ajuste exacto se hace al
     * conocer el tamano (app_redimensionar). */
    g.zoom_idx = 10;
    return 1;
}

void app_terminar(void) {
    int i;
    inicio_modelo_limpiar();
    juego_ventana_terminar();
    pestanas_terminar();
    pila_liberar(&g.deshacer);
    pila_liberar(&g.rehacer);
    pila_liberar(&g.deshacer_menus);
    pila_liberar(&g.rehacer_menus);
    doc_liberar(&g.doc);
    if (g.doc_menus_listo) doc_liberar(&g.doc_menus);
    seleccion_liberar(&g.sel);
    mapa_liberar();
    mapa_miniaturas_liberar();
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
    logica_iniciar();
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

/* Fase 8: cada guardado de recurso alimenta el registro del proyecto abierto.
 * Las rutas absolutas se reducen a la carpeta del recurso para que la
 * validacion de recursos siga funcionando con rutas relativas al proyecto. */
static void registrar_recurso_guardado(const char *ruta, const char *tipo) {
    char relativo[MAX_PATH];
    char nombre[MAX_PATH];
    char diagnostico[PM_ERROR_MAX];
    if (!ruta || !ruta[0]) return;
    recortar_texto(relativo, ruta, sizeof(relativo));
    if (relativo[0] == '/' || relativo[0] == '\\' || relativo[1] == ':') {
        const char *carpeta = "tiles";
        ruta_nombre(nombre, sizeof(nombre), relativo);
        if (strcmp(tipo, "wav") == 0) carpeta = "sonidos";
        else if (strcmp(tipo, "mav") == 0) carpeta = "maps";
        else if (strcmp(tipo, "wolfo") == 0) carpeta = "";
        if (carpeta[0])
            _snprintf(relativo, sizeof(relativo), "%s/%s", carpeta, nombre);
        else
            recortar_texto(relativo, nombre, sizeof(relativo));
        relativo[sizeof(relativo) - 1] = 0;
    }
    diagnostico[0] = 0;
    inicio_registrar_recurso(relativo, tipo, NULL, diagnostico,
                             sizeof(diagnostico));
}

void app_guardar_imagen(const char *ruta, int png) {
    doc_componer_todo(&g.doc);
    if (png ? imagen_guardar_png(ruta, g.doc.compuesto, g.doc.ancho, g.doc.alto)
            : imagen_guardar_bmp(ruta, g.doc.compuesto, g.doc.ancho, g.doc.alto)) {
        pestanas_marcar_guardada(ruta);
        registrar_recurso_guardado(ruta, png ? "png" : "bmp");
        app_estado("Guardado: %s", ruta);
    } else {
        dialogo_error("No se pudo guardar la imagen.");
    }
}

/* Intercambia dos pilas de deshacer sin copiar sus instantaneas. */
static void intercambiar_pilas(PilaUndo *a, PilaUndo *b) {
    PilaUndo t = *a;
    *a = *b;
    *b = t;
}

/* Prepara el lienzo propio del editor de menus con la resolucion del
 * proyecto. Se crea la primera vez y se redimensiona si las propiedades
 * del proyecto cambian. */
static void menus_doc_asegurar(void) {
    int w = limitar(g.ajustes.ancho, MIN_LADO, LIENZO_MAX);
    int h = limitar(g.ajustes.alto, MIN_LADO, LIENZO_MAX);
    if (!g.doc_menus_listo) {
        g.doc_menus = doc_nuevo(w, h, LIENZO);
        if (!g.doc_menus.capas[0].pixeles) {
            doc_liberar(&g.doc_menus);
            return;
        }
        doc_componer_todo(&g.doc_menus);
        g.doc_menus_listo = 1;
    } else if (g.doc_menus.ancho != w || g.doc_menus.alto != h) {
        doc_redimensionar(&g.doc_menus, w, h);
        doc_componer_todo(&g.doc_menus);
        /* Las instantaneas antiguas tienen el tamano anterior. */
        pila_limpiar(&g.deshacer_menus);
        pila_limpiar(&g.rehacer_menus);
    }
}

/* Entra en el editor de menus: cambia `g.doc` por el lienzo del proyecto y
 * guarda el documento de pintar (con su historial) en `doc_menus`. */
static void menus_doc_entrar(void) {
    menus_doc_asegurar();
    if (!g.doc_menus_listo) return;
    doc_intercambiar(&g.doc, &g.doc_menus);
    intercambiar_pilas(&g.deshacer, &g.deshacer_menus);
    intercambiar_pilas(&g.rehacer, &g.rehacer_menus);
    seleccion_limpiar(&g.sel);
}

/* Sale del editor de menus: devuelve el documento de pintar y su historial. */
static void menus_doc_salir(void) {
    if (!g.doc_menus_listo) return;
    doc_intercambiar(&g.doc, &g.doc_menus);
    intercambiar_pilas(&g.deshacer, &g.deshacer_menus);
    intercambiar_pilas(&g.rehacer, &g.rehacer_menus);
    seleccion_limpiar(&g.sel);
}

/* Ajusta el documento activo al tipo de vista: el editor de menus usa su
 * lienzo propio, el resto de vistas el documento de pintar. */
void app_vista_documento(Vista anterior, Vista nueva) {
    if (anterior == nueva) return;
    if (anterior == VISTA_MENUS) menus_doc_salir();
    if (nueva == VISTA_MENUS) menus_doc_entrar();
}

/* Las propiedades del proyecto mandan: si cambian estando en el editor de
 * menus, el lienzo se redimensiona en el acto. */
void app_menus_resolucion(void) {
    int w, h;
    if (g.vista != VISTA_MENUS || !g.doc_menus_listo) return;
    w = limitar(g.ajustes.ancho, MIN_LADO, LIENZO_MAX);
    h = limitar(g.ajustes.alto, MIN_LADO, LIENZO_MAX);
    if (g.doc.ancho == w && g.doc.alto == h) return;
    doc_redimensionar(&g.doc, w, h);
    doc_componer_todo(&g.doc);
    pila_limpiar(&g.deshacer);
    pila_limpiar(&g.rehacer);
    lienzo_actualizar();
    app_invalidar();
}

void app_cambiar_vista(Vista vista) {
    int desde_paneles;
    int saliendo_menus;
    Vista anterior;
    if (vista == g.vista) return;
    anterior = g.vista;
    desde_paneles = (g.vista == VISTA_MODULOS || g.vista == VISTA_MENUS ||
                     g.vista == VISTA_ESTUDIO);
    saliendo_menus = (g.vista == VISTA_MENUS);
    /* Cierra la edicion en curso sobre el lienzo que se va a ocultar. */
    if (saliendo_menus || vista == VISTA_MENUS) {
        if (g.texto_activo) texto_confirmar();
        if (g.sel.activa && g.sel.flotante)
            seleccion_soltar(&g.sel, &g.doc, &g.deshacer);
    }
    g.vista = vista;
    app_vista_documento(anterior, vista);
    g.menu_abierto = -1;
    /* Cancela cualquier arrastre del gestor al cambiar de modo. */
    g.modulo_arrastrando = 0;
    g.modulo_drag_i = -1;
    g.modulo_snap_linea_x = -1;
    g.modulo_snap_linea_y = -1;
    g.hover_modulo_boton = 0;
    g.modulo_boton_pulsado = 0;
    g.modulo_boton_movido = 0;
    g.selector_mapa_abierto = 0;
    g.selector_mapa_hover = -1;
    /* Al volver de una vista con paneles (gestor o editor de menus) el
     * lienzo se recoloca centrado en el area de trabajo normal. Tambien se
     * recoloca al salir del editor de menus hacia cualquier otra vista, para
     * no arrastrar el zoom de encaje. */
    if (vista == VISTA_PINTAR && desde_paneles) lienzo_actualizar();
    else if (saliendo_menus) lienzo_actualizar();
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

/* Gestor de logica: espacio de trabajo con la lista de elementos en el panel
 * izquierdo. Elegirlo de nuevo en el menu MODO vuelve a Pintar. */
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
    app_cambiar_vista(VISTA_MODULOS);
    /* Fase 48: la entrada al gestor muestra el constructor. La vista LOGIC2
     * sigue disponible con el boton COMPAT. de la barra inferior. */
    programa_modo_poner(1);
}

/* Editor de menus: mismos paneles que el gestor pero vacios, con el lienzo
 * del modo pintar en el centro. Elegirlo de nuevo vuelve a Pintar. */
void app_alternar_editor_menus(void) {
    if (g.vista == VISTA_MENUS) {
        app_cambiar_vista(VISTA_PINTAR);
        return;
    }
    /* Cierra cualquier edicion en curso antes de cambiar de vista. */
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
    app_cambiar_vista(VISTA_MENUS);
    /* El lienzo se centra en el hueco entre los paneles del editor. */
    lienzo_actualizar();
}

/* Vista Estudio: mismos paneles que el gestor, con el escenario en el centro
 * y el inspector a la derecha. Elegirlo de nuevo vuelve a Pintar. */
void app_alternar_estudio(void) {
    if (g.vista == VISTA_ESTUDIO) {
        app_cambiar_vista(VISTA_PINTAR);
        return;
    }
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
    if (!logic3_activo()) estudio_proyecto_cambiar("");
    app_cambiar_vista(VISTA_ESTUDIO);
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

/* Los PNG/BMP de pintar y tiles viven en `tiles`; los del editor de menus
 * son titulos y viven en `titles`. */
static void carpeta_imagenes(char *dst, int tam) {
    char exe[MAX_PATH];
    obtener_ruta_exe(exe, sizeof(exe));
    unir_ruta(dst, tam, exe,
              g.vista == VISTA_MENUS ? CARPETA_TITLES_TEXTO
                                     : CARPETA_TILES_TEXTO);
    crear_carpeta(dst);
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
    char carpeta[MAX_PATH];
    OPENFILENAMEA ofn;
    ruta[0] = '\0';
    ofn = preparar_dialogo(ruta,
        "Imagenes\0*.png;*.bmp;*.jpg;*.jpeg;*.gif;*.tif;*.tiff\0Todos los archivos\0*.*\0\0",
        "");
    carpeta_imagenes(carpeta, sizeof(carpeta));
    ofn.lpstrInitialDir = carpeta;
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
    char carpeta[MAX_PATH];
    OPENFILENAMEA ofn;
    const char *actual = pestanas_ruta(g.pestana_activa);
    ruta[0] = '\0';
    if (actual && actual[0]) ruta_nombre(ruta, sizeof(ruta), actual);
    ofn = preparar_dialogo(ruta, "Imagen PNG\0*.png\0Todos los archivos\0*.*\0\0", "textura.png");
    carpeta_imagenes(carpeta, sizeof(carpeta));
    ofn.lpstrInitialDir = carpeta;
    ofn.lpstrDefExt = "png";
    ofn.Flags |= OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameA(&ofn)) app_guardar_imagen(ruta, 1);
}

void dialogo_exportar_bmp(void) {
    char ruta[MAX_PATH];
    char carpeta[MAX_PATH];
    OPENFILENAMEA ofn;
    ruta[0] = '\0';
    ofn = preparar_dialogo(ruta, "Imagen BMP\0*.bmp\0Todos los archivos\0*.*\0\0", "textura.bmp");
    carpeta_imagenes(carpeta, sizeof(carpeta));
    ofn.lpstrInitialDir = carpeta;
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
        case WM_GETMINMAXINFO: {
            /* Un WS_POPUP maximizado ocupa pantalla completa y tapa la barra
             * de tareas. El area de trabajo del monitor es la ventana llena;
             * asi el doble clic da ventana maximizada, no pantalla completa. */
            MINMAXINFO *info = (MINMAXINFO *)lp;
            HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi;
            memset(&mi, 0, sizeof(mi));
            mi.cbSize = sizeof(mi);
            if (monitor && GetMonitorInfoA(monitor, &mi)) {
                info->ptMaxPosition.x = mi.rcWork.left;
                info->ptMaxPosition.y = mi.rcWork.top;
                info->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
                info->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
            } else {
                RECT area;
                if (SystemParametersInfoA(SPI_GETWORKAREA, 0, &area, 0)) {
                    info->ptMaxPosition.x = area.left;
                    info->ptMaxPosition.y = area.top;
                    info->ptMaxSize.x = area.right - area.left;
                    info->ptMaxSize.y = area.bottom - area.top;
                }
            }
            info->ptMinTrackSize.x = 640;
            info->ptMinTrackSize.y = 400;
            return 0;
        }
        case WM_ACTIVATE:
            g.ventana_activa = (LOWORD(wp) != WA_INACTIVE);
            if (!g.ventana_activa) g.titulo_boton_pulsado = -1;
            app_invalidar();
            return 0;
        case WM_CAPTURECHANGED:
            if (g.titulo_boton_pulsado >= 0) {
                g.titulo_boton_pulsado = -1;
                app_invalidar();
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_NCHITTEST: {
            /* No hay marco de Windows: la barra, las pestanas y los controles
             * son propios. Los bordes del cliente se comportan como tiradores
             * de redimensionado y el resto de la franja superior se arrastra
             * como si fuera el caption. */
            LRESULT ht;
            POINT p;
            int ancho = g.ancho ? g.ancho : ANCHO_INICIAL;
            int alto = g.alto ? g.alto : ALTO_INICIAL;
            p.x = GET_X_LPARAM(lp);
            p.y = GET_Y_LPARAM(lp);
            ScreenToClient(hwnd, &p);
            if (!IsZoomed(hwnd)) {
                if (p.x < BORDE_VENTANA) {
                    if (p.y < BORDE_VENTANA) return HTTOPLEFT;
                    if (p.y >= alto - BORDE_VENTANA) return HTBOTTOMLEFT;
                    return HTLEFT;
                }
                if (p.x >= ancho - BORDE_VENTANA) {
                    if (p.y < BORDE_VENTANA) return HTTOPRIGHT;
                    if (p.y >= alto - BORDE_VENTANA) return HTBOTTOMRIGHT;
                    return HTRIGHT;
                }
                if (p.y < BORDE_VENTANA) return HTTOP;
                if (p.y >= alto - BORDE_VENTANA) return HTBOTTOM;
            }
            ht = DefWindowProcA(hwnd, msg, wp, lp);
            if (ht == HTCLIENT && p.y >= 0 && p.y < TAB_BARRA_ALTO &&
                !titulo_zona_interactiva(p.x, p.y))
                return HTCAPTION;
            return ht;
        }
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
            g.hover_media = -1;
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
                if (lienzo_contiene(g.raton_x, g.raton_y) &&
                    (g.vista == VISTA_PINTAR ||
                     (g.vista == VISTA_MENUS && g.herramienta == H_TEXTO))) {
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
/* Devuelve el valor del argumento `nombre` tal como llega en la linea de
 * comandos. El valor se copia a un buffer estatico rotatorio (se mantienen
 * hasta 16 valores vivos a la vez): entre comillas se toma hasta la
 * comilla de cierre, y sin comillas hasta el siguiente argumento que
 * empiece por "--" o el final de la linea. Asi `--captura ruta.bmp
 * --gestor` no arrastra las flags siguientes dentro del valor. */
static char *arg_valor_n(char *cmd, const char *nombre, int indice) {
    static char bufs[32][MAX_PATH];
    static int buf_i = 0;
    char *p = cmd;
    size_t n = strlen(nombre);
    int visto = 0;
    if (!p) return NULL;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (_strnicmp(p, nombre, n) == 0 &&
            (p[n] == '=' || p[n] == ' ' || p[n] == '\t' || p[n] == 0)) {
            if (visto++ >= indice) {
                const char *inicio, *fin;
                p += n;
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '=') {
                    p++;
                    while (*p == ' ' || *p == '\t') p++;
                }
                inicio = p;
                if (*p == '"') {
                    inicio = p + 1;
                    fin = strchr(inicio, '"');
                    if (!fin) fin = inicio + strlen(inicio);
                } else {
                    fin = strstr(p, " --");
                    if (!fin) fin = p + strlen(p);
                }
                n = (size_t)(fin - inicio);
                if (n >= MAX_PATH) n = MAX_PATH - 1;
                {
                    char *dst = bufs[buf_i++ & 15];
                    memcpy(dst, inicio, n);
                    dst[n] = 0;
                    return dst;
                }
            }
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

/* Primera aparicion de una flag con valor. Las flags de la fase 35 que se
 * repiten usan arg_valor_n directamente. */
static char *arg_valor(char *cmd, const char *nombre) {
    return arg_valor_n(cmd, nombre, 0);
}

/* Fase 35: detecta una flag exacta sin confundir --datos con --datos-lista. */
static int arg_presente(char *cmd, const char *nombre) {
    char *p = cmd;
    size_t n = strlen(nombre);
    if (!p) return 0;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (_strnicmp(p, nombre, n) == 0 &&
            (p[n] == 0 || p[n] == ' ' || p[n] == '\t')) return 1;
        if (*p == '"') {
            p++;
            while (*p && *p != '"') p++;
            if (*p) p++;
        } else {
            while (*p && *p != ' ' && *p != '\t') p++;
        }
    }
    return 0;
}

/* Divide por '|' un argumento de las pruebas de la fase 35. */
static int partir_argumento(char *texto, char **campos, int max) {
    int n = 1;
    campos[0] = texto;
    while (*texto && n < max) {
        if (*texto == '|') {
            *texto = 0;
            campos[n++] = texto + 1;
        }
        texto++;
    }
    return n;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int mostrar) {
    WNDCLASSEXA wc;
    HWND hwnd;
    MSG msg;
    DWORD estilo;
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
    wc.lpszClassName = "LobosWolfos2000";
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(IDI_APP));
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExA(&wc)) {
        app_terminar();
        return 1;
    }

    /* Sin caption ni marco de Windows: la barra de titulo, las pestanas y
     * los controles son propios y viven en el area cliente. Se crea como
     * WS_POPUP para que Windows no dibuje ningun borde; el redimensionado
     * se gestiona en WM_NCHITTEST con una banda sensible en los bordes. */
    {
        RECT marco;
        int px, py;
        marco.left = 0; marco.top = 0;
        marco.right = ANCHO_INICIAL; marco.bottom = ALTO_INICIAL;
        estilo = WS_POPUP | WS_CLIPCHILDREN;
        AdjustWindowRectEx(&marco, estilo, FALSE, 0);
        px = (GetSystemMetrics(SM_CXSCREEN) - (marco.right - marco.left)) / 2;
        py = (GetSystemMetrics(SM_CYSCREEN) - (marco.bottom - marco.top)) / 3;
        hwnd = CreateWindowExA(0, "LobosWolfos2000", TP_NOMBRE " " TP_VERSION,
                               estilo, px < 0 ? 0 : px, py < 0 ? 0 : py,
                               marco.right - marco.left,
                               marco.bottom - marco.top,
                               NULL, NULL, inst, NULL);
    }
    if (!hwnd) {
        app_terminar();
        return 1;
    }
    g.hwnd = hwnd;
    /* Menu del sistema (Alt+Espacio), minimizar y maximizar sin caption. */
    SetWindowLongA(hwnd, GWL_STYLE,
                   GetWindowLongA(hwnd, GWL_STYLE) |
                   WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    /* El area cliente debe medir lo mismo que el buffer inicial. Al no haber
     * marco el ajuste suele ser cero, pero se conserva la correccion por si
     * el gestor de ventanas anade algun area no cliente al activar WS_SYSMENU. */
    {
        RECT cr, wr;
        GetClientRect(hwnd, &cr);
        GetWindowRect(hwnd, &wr);
        SetWindowPos(hwnd, NULL, 0, 0,
                     (wr.right - wr.left) + (ANCHO_INICIAL - cr.right),
                     (wr.bottom - wr.top) + (ALTO_INICIAL - cr.bottom),
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    app_actualizar_titulo();

    /* Modo de captura interna: dibuja un fotograma en el buffer y lo guarda
     * como BMP. Sirve para comparar la interfaz sin depender de PrintWindow
     * ni de la pantalla. */
    captura = arg_valor(cmd, "--captura");
    /* En modo normal tambien se puede abrir una logica o un proyecto .wolfo
     * al arrancar (util para probar la vista previa sin pasar por el dialogo).
     * En modo captura lo hace el bloque de abajo, con su orden de siempre. */
    if (!captura) {
        char *wolfo_arg = arg_valor(cmd, "--abrir-wolfo");
        if (wolfo_arg && wolfo_arg[0]) {
            char wolfo_ruta[MAX_PATH];
            strncpy(wolfo_ruta, wolfo_arg, MAX_PATH - 1);
            wolfo_ruta[MAX_PATH - 1] = 0;
            if (compilar_cargar_wolfo(wolfo_ruta)) {
                g.vista = VISTA_MODULOS;
                app_actualizar_titulo();
            }
        } else {
            char *cargar_arg = arg_valor(cmd, "--abrir-logica");
            if (cargar_arg) {
                char logic_ruta[MAX_PATH];
                int n = 0;
                while (cargar_arg[n] && cargar_arg[n] != ' ' &&
                       cargar_arg[n] != '\t' && n < MAX_PATH - 1) {
                    logic_ruta[n] = cargar_arg[n];
                    n++;
                }
                logic_ruta[n] = 0;
                if (logic_ruta[0] && logica_cargar_archivo(logic_ruta))
                    g.vista = VISTA_MODULOS;
            }
        }
        /* Apertura directa de un proyecto LOGIC3 (fase 24). */
        {
            char *l3_arg = arg_valor(cmd, "--abrir-logic3");
            if (l3_arg) {
                char l3_ruta[MAX_PATH];
                /* arg_valor ya respeta las comillas: conservar los espacios
                 * de la ruta (por ejemplo "GreenBlue kit"). */
                recortar_texto(l3_ruta, l3_arg, sizeof(l3_ruta));
                if (l3_ruta[0] && logic3_cargar_editor(l3_ruta)) {
                    g.vista = VISTA_MODULOS;
                    app_actualizar_titulo();
                }
            }
        }
        /* Misma ruta que ARCHIVO > Abrir logica .logic sin cuadro de
         * archivo. Detras de --abrir-logic3 reproduce abrir un LOGIC2
         * clasico despues de un proyecto con piezas. */
        {
            char *menu_arg = arg_valor(cmd, "--abrir-logica-menu");
            if (menu_arg) {
                char logic_ruta[MAX_PATH];
                recortar_texto(logic_ruta, menu_arg, sizeof(logic_ruta));
                if (logic_ruta[0]) logica_abrir_desde_menu(logic_ruta);
            }
        }
        /* Fase 48: abrir LOGIC3 o LOGIC2 termina en el gestor, y el gestor
         * siempre abre el constructor. Asi las dos cargas tienen la misma
         * vista y no dependen de si el proyecto trae sistema.opciones. */
        if (g.vista == VISTA_MODULOS)
            programa_vista_preparar(arg_presente(cmd, "--compatibilidad") ? 0 : 1);
        /* La vista Inicio tambien funciona en el editor normal; el flag de
         * captura repite esta carga mas abajo para conservar el orden de las
         * pruebas automatizadas. */
        {
            char *im = arg_valor(cmd, "--inicio-modelo");
            if (im) {
                char entrada[MAX_PATH];
                char diagnostico[PM_ERROR_MAX];
                ProjectModel *m;
                recortar_texto(entrada, im, sizeof(entrada));
                diagnostico[0] = '\0';
                m = ml_leer(entrada, diagnostico, sizeof(diagnostico));
                if (m) inicio_modelo_cargar(m, entrada);
                else inicio_modelo_cargar(NULL, entrada);
                g.vista = VISTA_INICIO;
                app_actualizar_titulo();
            }
        }
    }
    if (captura) {
        int inicio_ok = 1;
        int ok;
        char *menu_arg = arg_valor(cmd, "--menu");
        char *vista_arg = arg_valor(cmd, "--vista");
        char *raton_arg = arg_valor(cmd, "--raton");
        char *herr_arg = arg_valor(cmd, "--herramienta");
        char *clics_arg = arg_valor(cmd, "--clics");
        char *arrastre_arg = arg_valor(cmd, "--arrastre");
        /* En modo captura no se procesa el bucle de mensajes, asi que ni
         * WM_SIZE ni WM_PAINT han preparado el buffer todavia. Se deja listo
         * el lienzo antes de simular clics para que la interaccion parta de
         * la geometria real y no de un lienzo 0,0,0,0. */
        /* Fuerza el tamano nominal aunque WM_SIZE haya ajustado la ventana
         * a una pantalla pequena (por ejemplo un CRT a 1024x768). Asi los
         * patrones de regresion no dependen de la resolucion del monitor. */
        if (!strstr(cmd, "--maximizar"))
            app_redimensionar(ANCHO_INICIAL, ALTO_INICIAL);
        /* Prueba del estado maximizado sin bucle de mensajes: WM_SIZE deja
         * el buffer con el tamano de la pantalla y el dibujo usa la barra
         * superior completa. */
        if (strstr(cmd, "--maximizar") != NULL)
            ShowWindow(hwnd, SW_MAXIMIZE);
        lienzo_actualizar();
        /* Proyecto .wolfo para pruebas: restaura logica, mapas y tiles antes
         * de simular clics, como el dialogo ARCHIVO > Abrir proyecto. */
        {
            char *wolfo_arg = arg_valor(cmd, "--abrir-wolfo");
            if (wolfo_arg && wolfo_arg[0]) {
                char wolfo_ruta[MAX_PATH];
                strncpy(wolfo_ruta, wolfo_arg, MAX_PATH - 1);
                wolfo_ruta[MAX_PATH - 1] = 0;
                if (compilar_cargar_wolfo(wolfo_ruta))
                    g.vista = VISTA_MODULOS;
            }
        }
        if (menu_arg) g.menu_abierto = atoi(menu_arg);
        if (vista_arg && strncmp(vista_arg, "mapa", 4) == 0) g.vista = VISTA_MAPA;
        /* Ruta de mapa para pruebas: carga el .mav antes de simular clics. */
        {
            char *mruta = arg_valor(cmd, "--mapa-ruta");
            if (mruta) {
                int n = 0;
                while (mruta[n] && mruta[n] != ' ' && mruta[n] != '\t' &&
                       n < MAX_PATH - 1) {
                    g.mapa_ruta[n] = mruta[n];
                    n++;
                }
                g.mapa_ruta[n] = 0;
                mapa_reiniciar();
                mapa_cargar(g.mapa_ruta);
            }
        }
        if (strstr(cmd, "--tiles") != NULL) tiles_modo_entrar();
        /* Captura limpia: oculta las ventanas flotantes que taparian el
         * lienzo en las pruebas automatizadas. */
        if (strstr(cmd, "--sin-ventanas") != NULL) {
            g.panel_opciones = 0;
            g.patron_visible = 0;
            g.info_visible = 0;
            g.tiles_visible = 0;
        }
        /* Vista Inicio: se carga antes de --clics para que las pruebas y la
         * automatizacion puedan seleccionar nodos del arbol o del lienzo. */
        {
            char *im = arg_valor(cmd, "--inicio-modelo");
            if (im) {
                char entrada[MAX_PATH];
                char diagnostico[PM_ERROR_MAX];
                ProjectModel *m;
                recortar_texto(entrada, im, sizeof(entrada));
                diagnostico[0] = '\0';
                m = ml_leer(entrada, diagnostico, sizeof(diagnostico));
                if (m) {
                    inicio_modelo_cargar(m, entrada);
                    g.vista = VISTA_INICIO;
                    g.programa_modo = 0;
                } else {
                    inicio_modelo_cargar(NULL, entrada);
                    g.vista = VISTA_INICIO;
                }
                app_actualizar_titulo();
            }
        }
        /* Flujo reproducible de texturas: permite crear un lienzo pequeno,
         * pintarlo con --arrastre y exportarlo como PNG sin dialogos. */
        {
            char *tam_arg = arg_valor(cmd, "--tamano-imagen");
            if (tam_arg && g.vista == VISTA_PINTAR) {
                int ancho = 0, alto = 0;
                if (sscanf(tam_arg, "%d,%d", &ancho, &alto) == 2 &&
                    ancho >= 1 && ancho <= LIENZO_MAX &&
                    alto >= 1 && alto <= LIENZO_MAX) {
                    doc_redimensionar(&g.doc, ancho, alto);
                    doc_componer_todo(&g.doc);
                    lienzo_actualizar();
                }
            }
        }
        /* Fase 48: cualquier entrada al gestor abre el constructor. La vista
         * interactiva usa programa_modo_poner para mostrar el aviso; aqui se
         * asigna directo para no ensuciar las capturas con un mensaje que
         * caduca. --compatibilidad puede conmutar despues. */
        if (strstr(cmd, "--gestor") != NULL) {
            g.vista = VISTA_MODULOS;
            g.programa_modo = 1;
        }
        /* Fase 47: entrada directa a la capa Programa del gestor. */
        if (arg_presente(cmd, "--programa")) {
            if (g.vista != VISTA_MODULOS) g.vista = VISTA_MODULOS;
            if (!logic3_activo()) logic3_nuevo_editor();
            programa_modo_poner(1);
        }
        /* Item de prueba del gestor: nombre de un .mav de la carpeta maps. */
        {
            char *item_arg = arg_valor(cmd, "--item-mapa");
            if (item_arg) {
                int n = 0;
                while (item_arg[n] && item_arg[n] != ' ' &&
                       item_arg[n] != '\t' && n < MAPS_NOMBRE_MAX - 1) {
                    g.item_mapa[n] = item_arg[n];
                    n++;
                }
                g.item_mapa[n] = 0;
            }
        }
        if (strstr(cmd, "--selector-mapa") != NULL) selector_mapa_abrir();
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
        /* Logica cargada de un .logic para pruebas (fase 2): sustituye la
         * logica activa y muestra el gestor, como el dialogo. */
        {
            char *cargar_arg = arg_valor(cmd, "--abrir-logica");
            if (cargar_arg) {
                char logic_ruta[MAX_PATH];
                int n = 0;
                while (cargar_arg[n] && cargar_arg[n] != ' ' &&
                       cargar_arg[n] != '\t' && n < MAX_PATH - 1) {
                    logic_ruta[n] = cargar_arg[n];
                    n++;
                }
                logic_ruta[n] = 0;
                if (logic_ruta[0] && logica_cargar_archivo(logic_ruta))
                    g.vista = VISTA_MODULOS;
            }
        }
        /* Proyecto LOGIC3 explicito (fase 24). */
        {
            char *l3_arg = arg_valor(cmd, "--abrir-logic3");
            if (l3_arg) {
                char l3_ruta[MAX_PATH];
                recortar_texto(l3_ruta, l3_arg, sizeof(l3_ruta));
                if (l3_ruta[0] && logic3_cargar_editor(l3_ruta))
                    g.vista = VISTA_MODULOS;
            }
        }
        /* Misma ruta del menu en modo captura, detras del LOGIC3 para
         * reproducir la recarga con piezas previas. */
        {
            char *menu_arg = arg_valor(cmd, "--abrir-logica-menu");
            if (menu_arg) {
                char logic_ruta[MAX_PATH];
                recortar_texto(logic_ruta, menu_arg, sizeof(logic_ruta));
                if (logic_ruta[0]) logica_abrir_desde_menu(logic_ruta);
            }
        }
        /* Segunda normalizacion: con --captura las aperturas se repiten aqui
         * y deben dejar la misma vista constructor que en la carga anterior. */
        if (g.vista == VISTA_MODULOS)
            programa_vista_preparar(arg_presente(cmd, "--compatibilidad") ? 0 : 1);
        /* Entrada directa al editor de menus para pruebas. Va despues de
         * cargar la logica para poder comprobar el lienzo a la resolucion
         * del proyecto recien abierto. */
        if (vista_arg && strncmp(vista_arg, "menus", 5) == 0)
            app_alternar_editor_menus();
        /* Vista Estudio (fase 31) y seleccion inicial por id. Va despues de
         * cargar el LOGIC3 y antes de simular clics. */
        if (strstr(cmd, "--estudio") != NULL ||
            (vista_arg && strncmp(vista_arg, "estudio", 7) == 0))
            app_alternar_estudio();
        /* Lienzo de bloques (fase 32): entra por la pestana nueva sin
         * desplazar los indices de menu usados por las pruebas. */
        if (strstr(cmd, "--bloques") != NULL) {
            if (g.vista != VISTA_ESTUDIO) app_alternar_estudio();
            g.estudio_pestana = BLOQUES_TAB;
            bloques_importar_modelo();
            estudio_guardar_estado();
        }
        if (g.vista == VISTA_ESTUDIO) {
            char *escena_sel = arg_valor(cmd, "--escena");
            char *objeto_sel = arg_valor(cmd, "--objeto");
            if (escena_sel && escena_sel[0])
                estudio_seleccionar_escena(atoi(escena_sel));
            if (objeto_sel && objeto_sel[0])
                estudio_seleccionar_objeto(atoi(objeto_sel));
        }
        /* Editor de codigo abierto por id de funcion (fase 8): como un
         * clic en el boton de la seccion FUNCIONES. */
        {
            char *fabrir_arg = arg_valor(cmd, "--funcion-abrir");
            if (fabrir_arg && g.vista == VISTA_MODULOS) {
                int id = atoi(fabrir_arg);
                int indice = modulos_funcion_indice_de(id);
                if (indice >= 0) {
                    g.funcion_abierta = indice;
                    modulos_funciones_scroll_ver(indice + 1);
                }
            }
        }
        /* Clic derecho simulado sobre un boton de la seccion FUNCIONES:
         * pide confirmacion para borrar la funcion (fase 8). Va antes de
         * --clics para poder confirmar el aviso en la misma corrida. */
        {
            char *fborrar_arg = arg_valor(cmd, "--funcion-borrar");
            if (fborrar_arg && g.vista == VISTA_MODULOS) {
                int fx = 0, fy = 0;
                if (sscanf(fborrar_arg, "%d,%d", &fx, &fy) == 2) {
                    app_mover(fx, fy, MK_RBUTTON);
                    g.boton_der = 1;
                    app_pulsar(fx, fy, 1);
                    g.boton_der = 0;
                    app_soltar(fx, fy, 1);
                }
            }
        }
        if (strstr(cmd, "--info") != NULL) g.info_visible = 1;
        /* Guia paso a paso abierta (fase 22): --guia o --guia-paso N. Va
         * antes de --raton y --clics para poder probar sus botones. */
        if (strstr(cmd, "--guia") != NULL) {
            char *gp = arg_valor(cmd, "--guia-paso");
            guia_abrir();
            if (gp) {
                int p = atoi(gp);
                if (p < 0) p = 0;
                if (p >= GUIA_PASOS) p = GUIA_PASOS - 1;
                g.guia_paso = p;
            }
        }
        if (strstr(cmd, "--patron") != NULL) g.patron_visible = 1;
        if (strstr(cmd, "--mayus") != NULL) g.shift_simulado = 1;
        /* Propiedades del proyecto abiertas (fase 14). */
        if (strstr(cmd, "--ajustes") != NULL) ajustes_proyecto_abrir();
        /* Juego de ejemplo (fase 22): lo crea igual que AYUDA > Crear juego
         * de ejemplo, sin pasar por el menu. Va al final para que la captura
         * muestre ya el gestor con el item y los ajustes puestos. */
        if (strstr(cmd, "--ejemplo") != NULL) ejemplo_crear_juego();
        /* Fase 44: juego nuevo, tutorial y ayuda por bloque. Van al final,
         * como --ejemplo, para que ninguna suite anterior cambie de estado. */
        if (strstr(cmd, "--juego-nuevo") != NULL) {
            char *pid = arg_valor(cmd, "--plantilla");
            if (pid && pid[0]) {
                char ruta_plantilla[MAX_PATH];
                plantillas_crear(pid, ruta_plantilla, sizeof(ruta_plantilla));
            } else {
                juego_nuevo_abrir();
            }
        }
        if (strstr(cmd, "--tutorial") != NULL) {
            char *tp = arg_valor(cmd, "--tutorial-paso");
            tutorial_abrir();
            if (tp) {
                int paso = atoi(tp);
                if (paso < 0) paso = 0;
                if (paso >= TUTORIAL_PASOS) paso = TUTORIAL_PASOS - 1;
                g.tutorial_paso = paso;
            }
        }
        {
            char *ab = arg_valor(cmd, "--ayuda-bloque");
            if (ab && ab[0]) {
                int def = -1;
                if (ab[0] >= '0' && ab[0] <= '9') def = atoi(ab);
                else def = bloques_ayuda_indice(ab);
                ayuda_abrir(def);
            }
        }
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
        /* Edicion determinista del ProjectModel de Inicio. Cada segmento es
         * "id:campo=valor" y se pueden encadenar con punto y coma. El mismo
         * contrato lo usa el inspector con clic + teclado, de modo que las
         * IAs pueden modificar el .log sin simular coordenadas. */
        {
            char *editar_arg = arg_valor(cmd, "--inicio-editar");
            if (editar_arg && g.vista == VISTA_INICIO) {
                const char *seg = editar_arg;
                while (*seg) {
                    char parte[PM_TEXTO_MAX * 2];
                    char campo[PM_NOMBRE_MAX];
                    const char *dos_puntos = strchr(seg, ':');
                    const char *igual = strchr(seg, '=');
                    const char *fin = strchr(seg, ';');
                    const char *limite = fin ? fin : seg + strlen(seg);
                    int id = 0;
                    size_t nparte, ncampo, nvalor;
                    char diagnostico[PM_ERROR_MAX];
                    if (!dos_puntos || !igual || dos_puntos >= igual ||
                        dos_puntos >= limite || igual >= limite) break;
                    nparte = (size_t)(limite - seg);
                    if (nparte >= sizeof(parte)) nparte = sizeof(parte) - 1;
                    memcpy(parte, seg, nparte);
                    parte[nparte] = 0;
                    if (sscanf(parte, "%d", &id) != 1) break;
                    ncampo = (size_t)(igual - (dos_puntos + 1));
                    nvalor = (size_t)(limite - (igual + 1));
                    if (ncampo == 0 || ncampo >= sizeof(campo)) break;
                    memcpy(campo, dos_puntos + 1, ncampo);
                    campo[ncampo] = 0;
                    if (nvalor >= PM_TEXTO_MAX) nvalor = PM_TEXTO_MAX - 1;
                    {
                        char valor[PM_TEXTO_MAX];
                        memcpy(valor, igual + 1, nvalor);
                        valor[nvalor] = 0;
                        diagnostico[0] = 0;
                        inicio_modelo_editar(id, campo, valor, diagnostico,
                                             sizeof(diagnostico));
                    }
                    seg = fin ? fin + 1 : limite;
                }
            }
        }
        /* Fase 8: registro directo de recursos y arrastre desde el arbol de
         * Inicio hasta un nodo. El arrastre usa los mismos eventos que el
         * raton real, asi que la vista y el servicio comparten resultado. */
        {
            int reg_indice;
            for (reg_indice = 0; reg_indice < 16; reg_indice++) {
                char *reg_arg = arg_valor_n(cmd, "--registrar-recurso",
                                            reg_indice);
                char ruta[MAX_PATH];
                char tipo[PM_NOMBRE_MAX];
                char nombre[PM_NOMBRE_MAX];
                char diagnostico[PM_ERROR_MAX];
                const char *primero, *segundo;
                size_t n;
                if (!reg_arg) break;
                primero = strchr(reg_arg, '|');
                segundo = primero ? strchr(primero + 1, '|') : NULL;
                ruta[0] = tipo[0] = nombre[0] = diagnostico[0] = 0;
                if (primero) {
                    n = (size_t)(primero - reg_arg);
                    if (n >= sizeof(ruta)) n = sizeof(ruta) - 1;
                    memcpy(ruta, reg_arg, n);
                    ruta[n] = 0;
                    primero++;
                } else {
                    recortar_texto(ruta, reg_arg, sizeof(ruta));
                }
                if (primero) {
                    const char *fin = segundo ? segundo : primero + strlen(primero);
                    n = (size_t)(fin - primero);
                    if (n >= sizeof(tipo)) n = sizeof(tipo) - 1;
                    memcpy(tipo, primero, n);
                    tipo[n] = 0;
                }
                if (segundo) recortar_texto(nombre, segundo + 1, sizeof(nombre));
                if (ruta[0] && tipo[0])
                    inicio_registrar_recurso(ruta, tipo, nombre[0] ? nombre : NULL,
                                             diagnostico, sizeof(diagnostico));
            }
        }
        {
            char *arr_arg = arg_valor(cmd, "--inicio-arrastrar");
            if (arr_arg && g.vista == VISTA_INICIO) {
                int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
                if (sscanf(arr_arg, "%d,%d;%d,%d", &x0, &y0, &x1, &y1) == 4) {
                    app_pulsar(x0, y0, 0);
                    app_mover(x1, y1, MK_LBUTTON);
                    app_soltar(x1, y1, 0);
                }
            }
        }
        /* Teclado del inspector de Inicio despues de los clics. Es una
         * variante aislada de --teclas para poder reproducir el gesto real:
         * seleccionar entidad, pulsar un campo, escribir y confirmar. */
        {
            char *inicio_teclas_arg = arg_valor(cmd, "--inicio-teclas");
            if (inicio_teclas_arg && g.vista == VISTA_INICIO) {
                const char *p = inicio_teclas_arg;
                while (*p) {
                    if (p[0] == '\\' && p[1]) {
                        if (p[1] == 'b') { app_tecla(VK_BACK, 0, 0); p += 2; continue; }
                        if (p[1] == 'n') { app_tecla(VK_RETURN, 0, 0); p += 2; continue; }
                        if (p[1] == 'e') { app_tecla(VK_ESCAPE, 0, 0); p += 2; continue; }
                        if (p[1] == '\\') { app_caracter('\\'); p += 2; continue; }
                        /* Atajos de Inicio para las pruebas automaticas:
                         * \z deshacer, \Z rehacer con Shift, \y rehacer,
                         * \d borrar, \i crear nodo, \I crear escena,
                         * \s guardar script, \f compilar, \h/\l/\u/\o mover
                         * el cursor del editor de codigo. */
                        if (p[1] == 'z') { app_tecla('Z', 1, 0); p += 2; continue; }
                        if (p[1] == 'Z') { app_tecla('Z', 1, 1); p += 2; continue; }
                        if (p[1] == 'y') { app_tecla('Y', 1, 0); p += 2; continue; }
                        if (p[1] == 'd') { app_tecla(VK_DELETE, 0, 0); p += 2; continue; }
                        if (p[1] == 'i') { app_tecla(VK_INSERT, 0, 0); p += 2; continue; }
                        if (p[1] == 'I') { app_tecla(VK_INSERT, 0, 1); p += 2; continue; }
                        if (p[1] == 's') { app_tecla('G', 1, 0); p += 2; continue; }
                        if (p[1] == 'f') { app_tecla(VK_F7, 0, 0); p += 2; continue; }
                        if (p[1] == 'h') { app_tecla(VK_LEFT, 0, 0); p += 2; continue; }
                        if (p[1] == 'l') { app_tecla(VK_RIGHT, 0, 0); p += 2; continue; }
                        if (p[1] == 'u') { app_tecla(VK_UP, 0, 0); p += 2; continue; }
                        if (p[1] == 'o') { app_tecla(VK_DOWN, 0, 0); p += 2; continue; }
                        p++;
                        continue;
                    }
                    app_caracter((unsigned char)*p);
                    p++;
                }
            }
        }
        /* Fase 47: gestos de arrastre del constructor de programa. El formato
         * es "boton:x0,y0;x1,y1|...", el mismo que los lienzos de bloques. */
        {
            char *pg_arg = arg_valor(cmd, "--arrastre-programa");
            if (pg_arg && g.vista == VISTA_MODULOS) {
                const char *p = pg_arg;
                while (*p) {
                    int boton = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0, n = 0;
                    if (sscanf(p, "%d:%d,%d;%d,%d%n", &boton, &x0, &y0,
                               &x1, &y1, &n) != 5) break;
                    app_mover(x0, y0, 0);
                    if (boton == 1) g.boton_der = 1;
                    else g.boton_izq = 1;
                    app_pulsar(x0, y0, boton);
                    if (boton == 1) g.boton_der = 0;
                    else g.boton_izq = 0;
                    app_mover(x1, y1, 0);
                    app_soltar(x1, y1, boton);
                    p += n;
                    while (*p == '|' || *p == ' ' || *p == ',') p++;
                }
            }
        }
        /* Fase 35: entrar en las pestanas reservadas despues de --clics
         * para no reinterpretar los clics de creacion de escena y objeto. */
        if (arg_presente(cmd, "--datos") || arg_presente(cmd, "--sonidos")) {
            if (g.vista != VISTA_ESTUDIO) app_alternar_estudio();
            if (arg_presente(cmd, "--sonidos")) g.estudio_pestana = SONIDOS_TAB;
            else g.estudio_pestana = DATOS_TAB;
            if (g.estudio_ruta[0]) datos_cargar(g.estudio_ruta);
            estudio_guardar_estado();
        }
        /* Fase 35: mutaciones deterministas. Van despues de --clics para
         * que el proyecto creado desde los botones ya este disponible. */
        {
            char *dc = arg_valor(cmd, "--datos-cargar");
            if (dc && dc[0]) datos_cargar(dc);
        }
        {
            int iv = 0;
            char *dv;
            while ((dv = arg_valor_n(cmd, "--datos-variable", iv)) != NULL &&
                   dv[0]) {
                char *c[5];
                char nombre[DATOS_CLAVE_MAX];
                char valor[64];
                char obs[DATOS_OBS_TEXTO_MAX];
                int objeto = 0, tipo = L3_TIPO_ENTERO;
                if (partir_argumento(dv, c, 5) >= 4) {
                    objeto = atoi(c[0]);
                    recortar_texto(nombre, c[1], sizeof(nombre));
                    tipo = logic3_tipo_desde_texto(c[2]);
                    if (tipo < 0) tipo = atoi(c[2]);
                    recortar_texto(valor, c[3], sizeof(valor));
                    if (c[4]) recortar_texto(obs, c[4], sizeof(obs));
                    else obs[0] = 0;
                    datos_variable_nueva(objeto, nombre, tipo, valor, obs);
                }
                iv++;
            }
        }
        {
            char *dl = arg_valor(cmd, "--datos-lista");
            if (dl && dl[0]) {
                char *c[4];
                int tipo = L3_TIPO_ENTERO;
                if (partir_argumento(dl, c, 4) >= 3) {
                    tipo = logic3_tipo_desde_texto(c[2]);
                    if (tipo < 0) tipo = atoi(c[2]);
                    datos_lista_nueva(c[0], atoi(c[1]), tipo, c[3]);
                }
            }
        }
        {
            char *de = arg_valor(cmd, "--datos-editar");
            if (de && de[0]) {
                int lista = 0, celda = 0;
                char valor[128];
                if (sscanf(de, "%d,%d,%127[^,]", &lista, &celda, valor) == 3)
                    datos_lista_editar(lista, celda, valor);
            }
        }
        {
            char *ti = arg_valor(cmd, "--traje-importar");
            if (ti && ti[0]) {
                char *c[4];
                int x = 0, y = 0, w = 32, h = 32;
                int px = 16, py = 16, cuadros = 1, cw = 32, ch = 32;
                if (partir_argumento(ti, c, 4) >= 4) {
                    sscanf(c[1], "%d,%d,%d,%d", &x, &y, &w, &h);
                    sscanf(c[2], "%d,%d", &px, &py);
                    sscanf(c[3], "%d,%d,%d", &cuadros, &cw, &ch);
                    datos_traje_importar(c[0], x, y, w, h, px, py,
                                         cuadros, cw, ch);
                }
            }
        }
        {
            char *si = arg_valor(cmd, "--sonido-importar");
            if (si && si[0]) {
                char *c[2];
                if (partir_argumento(si, c, 2) >= 2)
                    datos_sonido_importar(c[0], c[1]);
            }
        }
        {
            char *sp = arg_valor(cmd, "--sonido-probar");
            if (sp && sp[0]) datos_sonido_probar(atoi(sp));
        }
        {
            char *sm = arg_valor(cmd, "--datos-migrar");
            if (sm && sm[0]) datos_migrar_sonidos(sm);
        }
        {
            char *db = arg_valor(cmd, "--datos-borrar");
            if (db && db[0]) {
                int tipo = 0, indice = 0;
                char reemplazo[64] = "";
                if (sscanf(db, "%d,%d,%63s", &tipo, &indice, reemplazo) < 2)
                    reemplazo[0] = 0;
                datos_borrar(tipo, indice,
                             arg_presente(cmd, "--confirmar"),
                             reemplazo[0] ? reemplazo : NULL);
            }
        }
        /* Fase 36: productividad del estudio. Todas las entradas y salidas
         * son nuevas; las suites anteriores no las usan. */
        {
            char *prod = arg_valor(cmd, "--productividad-marcar");
            if (prod && prod[0] && g.vista == VISTA_ESTUDIO)
                productividad_registrar(prod);
        }
        if (strstr(cmd, "--productividad-deshacer") != NULL &&
            g.vista == VISTA_ESTUDIO)
            productividad_deshacer();
        if (strstr(cmd, "--productividad-rehacer") != NULL &&
            g.vista == VISTA_ESTUDIO)
            productividad_rehacer();
        if (strstr(cmd, "--productividad-copiar") != NULL &&
            g.vista == VISTA_ESTUDIO)
            productividad_objeto_copiar();
        {
            char *prod = arg_valor(cmd, "--productividad-pegar");
            if (prod && g.vista == VISTA_ESTUDIO)
                productividad_objeto_pegar(prod[0] ? prod : NULL);
        }
        {
            char *prod = arg_valor(cmd, "--productividad-duplicar");
            if (prod && g.vista == VISTA_ESTUDIO)
                productividad_objeto_duplicar(prod[0] ? prod : NULL);
        }
        {
            char *prod = arg_valor(cmd, "--productividad-buscar");
            if (prod && prod[0] && g.vista == VISTA_ESTUDIO)
                productividad_buscar(prod);
        }
        {
            char *prod = arg_valor(cmd, "--productividad-lote");
            if (prod && prod[0] && g.vista == VISTA_ESTUDIO) {
                char *c[2];
                int cantidad = 1;
                if (partir_argumento(prod, c, 2) >= 1) {
                    cantidad = atoi(c[0]);
                    productividad_duplicar_seleccion(cantidad, c[1]);
                }
            }
        }
        {
            char *prod = arg_valor(cmd, "--productividad-snippet");
            if (prod && prod[0] && g.vista == VISTA_ESTUDIO)
                productividad_snippet_crear(prod);
        }
        {
            char *prod = arg_valor(cmd, "--productividad-ayuda");
            if (prod && prod[0] && g.vista == VISTA_ESTUDIO)
                app_estado("%s", productividad_ayuda(prod));
        }
        if (strstr(cmd, "--productividad-avanzado") != NULL &&
            g.vista == VISTA_ESTUDIO)
            productividad_modo_avanzado(1);
        if (strstr(cmd, "--productividad-principiante") != NULL &&
            g.vista == VISTA_ESTUDIO)
            productividad_modo_avanzado(0);
        /* Teclado simulado en el editor de codigo (fase 8): el texto se
         * escribe caracter a caracter, con \n \t \b \\ como escapes y
         * cada \n como salto de linea. Va despues de --clics para poder
         * abrir el editor con un clic en el boton y escribir despues. */
        {
            char *teclas_arg = arg_valor(cmd, "--codigo-teclas");
            if (teclas_arg && g.vista == VISTA_MODULOS &&
                g.funcion_abierta >= 0) {
                const char *p = teclas_arg;
                while (*p) {
                    if (p[0] == '\\' && p[1]) {
                        if (p[1] == 'n') { app_tecla(VK_RETURN, 0, 0); p += 2; continue; }
                        if (p[1] == 't') { app_tecla(VK_TAB, 0, 0); p += 2; continue; }
                        if (p[1] == 'b') { app_tecla(VK_BACK, 0, 0); p += 2; continue; }
                        if (p[1] == '\\') { app_caracter('\\'); p += 2; continue; }
                        p++; /* escape desconocido: solo saltar la barra */
                        continue;
                    }
                    app_caracter((unsigned char)*p);
                    p++;
                }
            }
        }
        /* Teclado simulado general (fase 14): escribe caracteres y admite
         * \b (borrar), \n (Enter) y \e (Escape) para editar el nombre del
         * proyecto o aceptar/cancelar sus propiedades. Va despues de --clics
         * y de --ajustes. */
        {
            char *t_arg = arg_valor(cmd, "--teclas");
            if (t_arg) {
                const char *p = t_arg;
                while (*p) {
                    if (p[0] == '\\' && p[1]) {
                        if (p[1] == 'b') { app_tecla(VK_BACK, 0, 0); p += 2; continue; }
                        if (p[1] == 'n') { app_tecla(VK_RETURN, 0, 0); p += 2; continue; }
                        if (p[1] == 'e') { app_tecla(VK_ESCAPE, 0, 0); p += 2; continue; }
                        if (p[1] == 'h') { app_tecla(VK_HOME, 0, 0); p += 2; continue; }
                        if (p[1] == 'f') { app_tecla(VK_END, 0, 0); p += 2; continue; }
                        if (p[1] == 'l') { app_tecla(VK_LEFT, 0, 0); p += 2; continue; }
                        if (p[1] == 'r') { app_tecla(VK_RIGHT, 0, 0); p += 2; continue; }
                        if (p[1] == '\\') { app_caracter('\\'); p += 2; continue; }
                        p++;
                        continue;
                    }
                    app_caracter((unsigned char)*p);
                    p++;
                }
            }
        }
        /* Arrastre simulado "x0,y0;x1,y1": pulsa en el primer punto, recorre
         * el trayecto con el boton izquierdo apretado y suelta en el segundo.
         * Admite varios gestos separados por "|" y un prefijo opcional
         * "herramienta:" en cada uno, por ejemplo
         * "12:100,200;300,400|15:150,250;150,350". Asi se puede dibujar y
         * despues mover una seleccion en la misma prueba. */
        if (arrastre_arg) {
            const char *seg = arrastre_arg;
            while (*seg) {
                int ax0 = 0, ay0 = 0, ax1 = 0, ay1 = 0, n = 0, herr = -1;
                const char *p = seg;
                if (sscanf(p, "%d:%n", &herr, &n) == 1 && n > 0) p += n;
                if (sscanf(p, "%d,%d;%d,%d%n", &ax0, &ay0, &ax1, &ay1, &n) != 4)
                    break;
                if (herr >= 0 && herr < H_TOTAL) g.herramienta = (Herramienta)herr;
                {
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
                seg = p + n;
                while (*seg == '|' || *seg == ' ' || *seg == ';') seg++;
            }
        }
        /* Arrastres del lienzo de bloques (fase 32). Cada gesto es
         * "x0,y0;x1,y1" con prefijo de categoria opcional "cat:", y varios
         * se separan con "|". Van despues de los clics para poder elegir
         * antes la categoria de la paleta. */
        {
            char *barrastre = arg_valor(cmd, "--arrastre-bloque");
            if (barrastre && g.vista == VISTA_ESTUDIO &&
                g.estudio_pestana == BLOQUES_TAB) {
                const char *seg = barrastre;
                while (*seg) {
                    int bx0 = 0, by0 = 0, bx1 = 0, by1 = 0, usados = 0;
                    int cat = -1, leidos = 0;
                    const char *p = seg;
                    if (sscanf(p, "%d:%n", &cat, &leidos) == 1 && leidos > 0)
                        p += leidos;
                    if (sscanf(p, "%d,%d;%d,%d%n", &bx0, &by0, &bx1, &by1,
                               &usados) != 4) break;
                    if (cat >= 0 && cat < BLOQUES_CATEGORIAS)
                        g.bloques_categoria = cat;
                    bloques_ui_arrastrar(bx0, by0, bx1, by1);
                    seg = p + usados;
                    while (*seg == '|' || *seg == ' ' || *seg == ';') seg++;
                }
            }
        }
        if (strstr(cmd, "--deshacer-bloque") != NULL &&
            g.vista == VISTA_ESTUDIO && g.estudio_pestana == BLOQUES_TAB)
            bloques_deshacer();
        /* Campo de un bloque por id (fase 33): atajo de prueba para escribir
         * texto sin depender delorden de clics y arrastres. */
        {
            char *campo_arg = arg_valor(cmd, "--campo-bloque");
            if (campo_arg && g.vista == VISTA_ESTUDIO &&
                g.estudio_pestana == BLOQUES_TAB) {
                int id_campo = 0;
                const char *p = strchr(campo_arg, ':');
                if (p && sscanf(campo_arg, "%d", &id_campo) == 1) {
                    int k;
                    p++;
                    /* El campo puede venir con un valor por defecto: se
                     * limpia para que el texto de la prueba lo reemplace. */
                    for (k = 0; k < 8; k++)
                        bloques_campo_anadir(id_campo, 0, '\b');
                    while (*p) {
                        bloques_campo_anadir(id_campo, 0, (unsigned char)*p);
                        p++;
                    }
                }
            }
        }
        /* La seleccion por id va al final: los arrastres posteriores podian
         * dejar seleccionado el ultimo bloque creado. */
        if (g.vista == VISTA_ESTUDIO) {
            char *bloque_sel = arg_valor(cmd, "--bloque");
            if (bloque_sel && bloque_sel[0])
                bloques_seleccionar(atoi(bloque_sel));
        }
        /* Compilador de bloques (fase 33): genera lobo 3, abre el visor de
         * solo lectura o convierte el lienzo en un bloque codigo.lobo. */
        if (g.vista == VISTA_ESTUDIO && g.estudio_pestana == BLOQUES_TAB) {
            if (strstr(cmd, "--compilar-bloques") != NULL)
                compilador_bloques_generar();
            if (strstr(cmd, "--visor-codigo") != NULL) {
                if (strstr(cmd, "--compilar-bloques") == NULL)
                    compilador_bloques_generar();
                g.bloques_ver_codigo = 1;
            }
            if (strstr(cmd, "--convertir-bloques") != NULL)
                compilador_bloques_convertir();
        }
        /* Portapapeles simulado tras el arrastre: copiar/cortar la seleccion
         * y pegar el bloque flotante para comprobar que se dibuja. */
        if (strstr(cmd, "--copiar") != NULL) pintura_copiar(0);
        if (strstr(cmd, "--cortar") != NULL) pintura_copiar(1);
        if (strstr(cmd, "--pegar") != NULL) pintura_pegar();
        /* Teclado simulado para la herramienta de texto despues de
         * --arrastre: permite definir una caja que cruza tiles y escribir. */
        {
            char *tt_arg = arg_valor(cmd, "--texto-teclas");
            if (tt_arg) {
                const char *p = tt_arg;
                while (*p) {
                    if (p[0] == '\\' && p[1]) {
                        if (p[1] == 'b') { app_tecla(VK_BACK, 0, 0); p += 2; continue; }
                        if (p[1] == 'n') { app_tecla(VK_RETURN, 0, 0); p += 2; continue; }
                        if (p[1] == 'e') { app_tecla(VK_ESCAPE, 0, 0); p += 2; continue; }
                        if (p[1] == '\\') { app_caracter('\\'); p += 2; continue; }
                        p++;
                        continue;
                    }
                    app_caracter((unsigned char)*p);
                    p++;
                }
            }
        }
        /* Confirmacion simulada del texto: un clic simple fuera de la caja. */
        {
            char *cf_arg = arg_valor(cmd, "--clic-final");
            if (cf_arg) {
                int cx = 0, cy = 0;
                if (sscanf(cf_arg, "%d,%d", &cx, &cy) == 2) {
                    g.boton_izq = 1;
                    app_pulsar(cx, cy, 0);
                    g.boton_izq = 0;
                    app_soltar(cx, cy, 0);
                    /* El clic fuera confirma y abre una caja nueva vacia; se
                     * descarta para que la captura muestre solo lo estampado. */
                    if (g.texto_activo && g.texto.largo == 0)
                        app_tecla(VK_ESCAPE, 0, 0);
                }
            }
        }
        /* Deshacer simulado tras el texto para comprobar la instantanea. */
        if (strstr(cmd, "--deshacer") != NULL) {
            app_tecla(0x5A, 1, 0);
            /* La instantanea de tiles reactiva el visor; en captura limpia se
             * vuelve a ocultar para que no tape el lienzo. */
            if (strstr(cmd, "--sin-ventanas") != NULL) g.tiles_visible = 0;
        }
        /* Paneo simulado del editor de mapas con el boton central. */
        {
            char *pan_arg = arg_valor(cmd, "--mapa-pan");
            if (pan_arg) {
                int px0 = 0, py0 = 0, px1 = 0, py1 = 0;
                if (sscanf(pan_arg, "%d,%d;%d,%d", &px0, &py0, &px1, &py1) == 4) {
                    int paso;
                    app_pulsar(px0, py0, 2);
                    g.boton_medio = 1;
                    for (paso = 1; paso <= 24; paso++) {
                        int mx = px0 + (px1 - px0) * paso / 24;
                        int my = py0 + (py1 - py0) * paso / 24;
                        app_mover(mx, my, MK_MBUTTON);
                    }
                    g.boton_medio = 0;
                    app_soltar(px1, py1, 2);
                }
            }
        }
        /* Rueda simulada en el editor de mapas: "x,y,delta[,ctrl]". */
        {
            char *rueda_arg = arg_valor(cmd, "--mapa-rueda");
            if (rueda_arg) {
                int rx = 0, ry = 0, d = 0, n = 0, ctrl = 0;
                if (sscanf(rueda_arg, "%d,%d,%d%n", &rx, &ry, &d, &n) >= 3) {
                    if (rueda_arg[n] == ',') ctrl = atoi(rueda_arg + n + 1);
                    mapa_rueda(rx, ry, d, ctrl);
                }
            }
        }
        /* Borrado simulado del editor de mapas arrastrando el boton derecho. */
        {
            char *borrar_arg = arg_valor(cmd, "--mapa-borrar");
            if (borrar_arg) {
                int bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
                if (sscanf(borrar_arg, "%d,%d;%d,%d", &bx0, &by0, &bx1, &by1) == 4) {
                    int paso;
                    app_pulsar(bx0, by0, 1);
                    g.boton_der = 1;
                    for (paso = 1; paso <= 24; paso++) {
                        int mx = bx0 + (bx1 - bx0) * paso / 24;
                        int my = by0 + (by1 - by0) * paso / 24;
                        app_mover(mx, my, MK_RBUTTON);
                    }
                    g.boton_der = 0;
                    app_soltar(bx1, by1, 1);
                }
            }
        }
        /* Clic derecho simulado en el lienzo del gestor: elimina el
         * rectangulo bajo el cursor. */
        {
            char *mborrar_arg = arg_valor(cmd, "--modulos-borrar");
            if (mborrar_arg) {
                int dx = 0, dy = 0;
                if (sscanf(mborrar_arg, "%d,%d", &dx, &dy) == 2 &&
                    g.vista == VISTA_MODULOS) {
                    app_mover(dx, dy, MK_RBUTTON);
                    g.boton_der = 1;
                    app_pulsar(dx, dy, 1);
                    g.boton_der = 0;
                    app_soltar(dx, dy, 1);
                }
            }
        }
        /* Rueda simulada en el gestor: "x,y,delta[,ctrl]" (fase 8, para
         * desplazar la seccion FUNCIONES). */
        {
            char *mrueda_arg = arg_valor(cmd, "--modulos-rueda");
            if (mrueda_arg && g.vista == VISTA_MODULOS) {
                int rx = 0, ry = 0, d = 0, n = 0, ctrl = 0;
                if (sscanf(mrueda_arg, "%d,%d,%d%n", &rx, &ry, &d, &n) >= 3) {
                    if (mrueda_arg[n] == ',') ctrl = atoi(mrueda_arg + n + 1);
                    app_rueda(rx, ry, d, ctrl);
                }
            }
        }
        /* Guardado directo del mapa para pruebas, sin dialogo. */
        {
            char *guardar_arg = arg_valor(cmd, "--guardar-mapa");
            if (guardar_arg) {
                char ruta[MAX_PATH];
                int n = 0;
                while (guardar_arg[n] && guardar_arg[n] != ' ' &&
                       guardar_arg[n] != '\t' && n < MAX_PATH - 1) {
                    ruta[n] = guardar_arg[n];
                    n++;
                }
                ruta[n] = 0;
                if (ruta[0] && mapa_guardar(ruta)) {
                    strncpy(g.mapa_ruta, ruta, MAX_PATH - 1);
                    g.mapa_ruta[MAX_PATH - 1] = 0;
                    registrar_recurso_guardado(ruta, "mav");
                }
            }
        }
        /* Guardado directo de la logica activa (pruebas sin dialogo). Va
         * despues de los clics para poder comprobar el estado plegado. */
        {
            char *logic_arg = arg_valor(cmd, "--guardar-logica");
            if (logic_arg) {
                char logic_ruta[MAX_PATH];
                int n = 0;
                while (logic_arg[n] && logic_arg[n] != ' ' &&
                       logic_arg[n] != '\t' && n < MAX_PATH - 1) {
                    logic_ruta[n] = logic_arg[n];
                    n++;
                }
                logic_ruta[n] = 0;
                logica_guardar_archivo(logic_ruta);
            }
        }
        /* Guardado directo de un LOGIC3 activo (fase 24). */
        {
            char *l3_arg = arg_valor(cmd, "--guardar-logic3");
            if (l3_arg) {
                char l3_ruta[MAX_PATH];
                int n = 0;
                while (l3_arg[n] && l3_arg[n] != ' ' &&
                       l3_arg[n] != '\t' && n < MAX_PATH - 1) {
                    l3_ruta[n] = l3_arg[n];
                    n++;
                }
                l3_ruta[n] = 0;
                if (l3_ruta[0]) logic3_guardar_actual(l3_ruta);
            }
        }
        /* Exporta el lienzo que se acaba de pintar. Es deliberadamente
         * independiente del LOGIC3: el .log solo referencia el nombre PNG. */
        {
            char *imagen_arg = arg_valor(cmd, "--guardar-imagen");
            if (imagen_arg) {
                char imagen_ruta[MAX_PATH];
                int n = 0;
                while (imagen_arg[n] && imagen_arg[n] != ' ' &&
                       imagen_arg[n] != '\t' && n < MAX_PATH - 1) {
                    imagen_ruta[n] = imagen_arg[n];
                    n++;
                }
                imagen_ruta[n] = 0;
                if (imagen_ruta[0]) app_guardar_imagen(imagen_ruta, 1);
            }
        }
        /* Fase 35: guarda el LOGIC3 y el archivo lateral de datos. El
         * escritor canonico no cambia; el sidecar solo se crea con esta
         * flag o desde el boton del editor. */
        {
            char *dg = arg_valor(cmd, "--datos-guardar");
            if (dg) {
                char dg_ruta[MAX_PATH];
                int n = 0;
                while (dg[n] && dg[n] != ' ' && dg[n] != '\t' &&
                       n < MAX_PATH - 1) {
                    dg_ruta[n] = dg[n];
                    n++;
                }
                dg_ruta[n] = 0;
                if (dg_ruta[0] && logic3_guardar_actual(dg_ruta))
                    datos_guardar(dg_ruta);
            }
        }
        /* Conversion LOGIC2 -> LOGIC3 (fase 24): escribe <ruta>.logic3
         * y deja el modelo convertido activo. */
        {
            char *conv_arg = arg_valor(cmd, "--convertir-logic2");
            if (conv_arg) {
                char conv_ruta[MAX_PATH];
                int n = 0;
                while (conv_arg[n] && conv_arg[n] != ' ' &&
                       conv_arg[n] != '\t' && n < MAX_PATH - 1) {
                    conv_ruta[n] = conv_arg[n];
                    n++;
                }
                conv_ruta[n] = 0;
                if (conv_ruta[0]) logic3_convertir_logic2(conv_ruta);
            }
        }
        /* Exportacion de emergencia LOGIC3 -> LOGIC2 (fase 24). */
        {
            char *exp_arg = arg_valor(cmd, "--exportar-logic2");
            if (exp_arg) {
                char exp_ruta[MAX_PATH];
                int n = 0;
                while (exp_arg[n] && exp_arg[n] != ' ' &&
                       exp_arg[n] != '\t' && n < MAX_PATH - 1) {
                    exp_ruta[n] = exp_arg[n];
                    n++;
                }
                exp_ruta[n] = 0;
                if (exp_ruta[0]) logic3_exportar_logic2(exp_ruta);
            }
        }
        /* Guardado directo del paquete .wolfo (pruebas sin dialogo, fase
         * 15): escribe logica, mapas y tiles en un solo archivo. */
        {
            char *wolfo_arg = arg_valor(cmd, "--guardar-wolfo");
            if (wolfo_arg) {
                char wolfo_ruta[MAX_PATH];
                int n = 0;
                while (wolfo_arg[n] && wolfo_arg[n] != ' ' &&
                       wolfo_arg[n] != '\t' && n < MAX_PATH - 1) {
                    wolfo_ruta[n] = wolfo_arg[n];
                    n++;
                }
                wolfo_ruta[n] = 0;
                /* Fase 45: --wolfo-logic3 anade el LOGIC3 nativo y los
                 * trajes. Sin esa flag el guardado sigue siendo el de la
                 * fase 15, para no cambiar paquetes antiguos. */
                if (wolfo_ruta[0])
                    compilar_guardar_wolfo(wolfo_ruta,
                        strstr(cmd, "--wolfo-logic3") != NULL);
            }
        }
        /* Compilacion directa para Windows (pruebas sin dialogo, fase
         * 4): escribe el game.exe del proyecto activo en la ruta dada. */
        {
            char *comp_arg = arg_valor(cmd, "--compilar-windows");
            if (comp_arg) {
                char comp_ruta[MAX_PATH];
                int n = 0;
                while (comp_arg[n] && comp_arg[n] != ' ' &&
                       comp_arg[n] != '\t' && n < MAX_PATH - 1) {
                    comp_ruta[n] = comp_arg[n];
                    n++;
                }
                comp_ruta[n] = 0;
                if (comp_ruta[0]) compilar_windows(comp_ruta);
            }
        }
        /* Compilacion directa para BIOS (pruebas sin dialogo, fase 7):
         * escribe la imagen USB del proyecto activo en la ruta dada. */
        {
            char *bios_arg = arg_valor(cmd, "--compilar-bios");
            if (bios_arg) {
                char bios_ruta[MAX_PATH];
                recortar_texto(bios_ruta, bios_arg, sizeof(bios_ruta));
                /* Fase 43: --bios-avisos <ruta> vuelca los avisos de
                 * no portabilidad sin cambiar el menu ni otras flags. */
                {
                    char *avisos_arg = arg_valor(cmd, "--bios-avisos");
                    if (avisos_arg) {
                        char avisos_ruta[MAX_PATH];
                        recortar_texto(avisos_ruta, avisos_arg, sizeof(avisos_ruta));
                        compilar_bios_avisos(avisos_ruta);
                    } else {
                        compilar_bios_avisos(NULL);
                    }
                }
                if (bios_ruta[0]) compilar_bios(bios_ruta);
            }
        }
        /* Captura determinista de la vista previa del juego (fase 9):
         * reune el proyecto en memoria y actualiza N fotogramas sin abrir
         * ventana, para comparar con el game.exe compilado. */
        {
            char *juego_arg = arg_valor(cmd, "--captura-juego");
            if (juego_arg) {
                char juego_ruta[MAX_PATH];
                char *fotos_arg = arg_valor(cmd, "--fotogramas");
                char *rt_traza = arg_valor(cmd, "--captura-juego-traza");
                char *rt_volcar = arg_valor(cmd, "--captura-juego-volcar");
                int rt_activo = arg_presente(cmd, "--runtime-jerarquico");
                int jn = 0, fotos = 60;
                while (juego_arg[jn] && juego_arg[jn] != ' ' &&
                       juego_arg[jn] != '\t' && jn < MAX_PATH - 1) {
                    juego_ruta[jn] = juego_arg[jn];
                    jn++;
                }
                juego_ruta[jn] = 0;
                if (fotos_arg) fotos = atoi(fotos_arg);
                if (juego_ruta[0])
                    juego_ventana_capturar(juego_ruta, fotos, rt_activo,
                                           rt_traza, rt_volcar);
            }
        }
        /* Posicion final del raton: actualiza tambien el hover, como un
         * movimiento real, para capturar resaltes y cursores. */
        if (raton_arg) {
            int rx = 0, ry = 0;
            if (sscanf(raton_arg, "%d,%d", &rx, &ry) == 2) app_mover(rx, ry, 0);
        }
        /* Volcado de estado para pruebas: vista, documento activo y
         * geometria del lienzo en una linea de texto. */
        {
            char *volcar_arg = arg_valor(cmd, "--volcar");
            if (volcar_arg) {
                char volcar_ruta[MAX_PATH];
                int vn = 0;
                FILE *vf;
                Zona vt;
                Lienzo vl;
                int l3 = 0, l3e = 0, l3o = 0, l3ev = 0, l3b = 0, l3v = 0;
                while (volcar_arg[vn] && volcar_arg[vn] != ' ' &&
                       volcar_arg[vn] != '\t' && vn < MAX_PATH - 1) {
                    volcar_ruta[vn] = volcar_arg[vn];
                    vn++;
                }
                volcar_ruta[vn] = 0;
                vf = volcar_ruta[0] ? fopen(volcar_ruta, "w") : NULL;
                if (vf) {
                    vt = zona_trabajo();
                    vl = g.lienzo;
                    l3 = logic3_activo();
                    if (l3) logic3_contar(&l3e, &l3o, &l3ev, &l3b, &l3v);
                    fprintf(vf,
                            "ancho=%d alto=%d vista=%d doc=%dx%d "
                            "menus=%dx%d listo=%d ajustes=%dx%d "
                            "lienzo=%d,%d,%d,%d zoom=%d trab=%d,%d,%d,%d "
                            "menu=%d zona_menus=%d,%d,%d,%d modulos=%d "
                            "logic3=%d escenas=%d objetos=%d eventos=%d "
                            "bloques=%d variables=%d "
                            "guia=%d paso=%d mapa=%s args=%s|%s\n",
                            g.ancho, g.alto, (int)g.vista,
                            g.doc.ancho, g.doc.alto,
                            g.doc_menus.ancho, g.doc_menus.alto,
                            g.doc_menus_listo,
                            g.ajustes.ancho, g.ajustes.alto,
                            vl.x0, vl.y0, vl.x1, vl.y1, g.zoom_idx,
                            vt.x, vt.y, vt.ancho, vt.alto,
                            g.menu_abierto,
                            modulos_lienzo_zona().x, modulos_lienzo_zona().y,
                            modulos_lienzo_zona().ancho,
                            modulos_lienzo_zona().alto,
                            g.num_modulos,
                            l3, l3e, l3o, l3ev, l3b, l3v,
                            g.guia_visible, g.guia_paso,
                            g.mapa_ruta[0] ? g.mapa_ruta : "-",
                            menu_arg ? menu_arg : "-",
                            clics_arg ? clics_arg : "-");
                    if (g.vista == VISTA_ESTUDIO) {
                        Zona ez = estudio_lienzo_zona();
                        fprintf(vf,
                                "estudio=1 paneles=%d,%d,%d "
                                "sel_escena=%d sel_objeto=%d "
                                "scroll_estudio=%d pestana_estudio=%d "
                                "ruta_estudio=%s\n",
                                ESTUDIO_IZQ_ANCHO, ez.ancho,
                                ESTUDIO_DER_ANCHO, g.estudio_escena,
                                g.estudio_objeto, g.estudio_scroll,
                                g.estudio_pestana,
                                g.estudio_ruta[0] ? g.estudio_ruta : "-");
                    }
                    if (g.vista == VISTA_ESTUDIO &&
                        g.estudio_pestana == BLOQUES_TAB) {
                        char cb_error[LOBO_ERROR_MAX];
                        const char *e = compilador_bloques_error();
                        int n = 0;
                        fprintf(vf,
                                "lb_bloques=%d lb_sel=%d lb_ranuras=%d "
                                "lb_cuerpo=%d lb_paleta=%d lb_categoria=%d "
                                "lb_foco=%d lb_zoom=%d lb_pan=%d,%d\n",
                                bloques_cantidad(), bloques_id_seleccionado(),
                                bloques_num_ranuras(), bloques_num_hijos(),
                                g.bloques_paleta_sel, g.bloques_categoria,
                                g.bloques_foco, g.bloques_zoom,
                                g.bloques_pan_x, g.bloques_pan_y);
                        /* Fase 33: el error se sanitiza para que no rompa
                         * el formato de una linea por clave. */
                        while (e && e[n] && n < LOBO_ERROR_MAX - 1) {
                            char ch = e[n];
                            cb_error[n] = (ch == ' ' || ch == '\t' ||
                                           ch == '\n' || ch == '\r' ||
                                           ch == '=') ? '_' : ch;
                            n++;
                        }
                        cb_error[n] = 0;
                        /* Los campos cb_* solo aparecen cuando el compilador
                         * ha intervenido: asi los volcados de la fase 32 no
                         * cambian ni una linea. */
                        if (g.bloques_ver_codigo || g.bloques_convertido ||
                            cb_error[0] || compilador_bloques_lineas() > 0)
                            fprintf(vf,
                                    "cb_visor=%d cb_lineas=%d cb_error=%s "
                                    "cb_bloque=%d cb_linea=%d cb_convertido=%d\n",
                                    g.bloques_ver_codigo,
                                    compilador_bloques_lineas(),
                                    cb_error[0] ? cb_error : "-",
                                    compilador_bloques_bloque_error(),
                                    compilador_bloques_linea_error(),
                                    g.bloques_convertido);
                    }
                    fclose(vf);
                }
            }
        }
        /* Fase 44: volcado propio. Archivo nuevo para no alterar ni una
         * linea de los volcados de las suites anteriores. */
        {
            char *vp = arg_valor(cmd, "--volcar-plantillas");
            if (vp) {
                char vp_ruta[MAX_PATH];
                recortar_texto(vp_ruta, vp, sizeof(vp_ruta));
                plantillas_volcar(vp_ruta);
            }
        }
        /* Fase 35: volcado propio, solo cuando se pide de forma explicita
         * para no cambiar los patrones de las doce suites anteriores. */
        {
            char *vd = arg_valor(cmd, "--volcar-datos");
            if (vd) {
                char vd_ruta[MAX_PATH];
                int n = 0;
                while (vd[n] && vd[n] != ' ' && vd[n] != '\t' &&
                       n < MAX_PATH - 1) {
                    vd_ruta[n] = vd[n];
                    n++;
                }
                vd_ruta[n] = 0;
                datos_volcar(vd_ruta);
            }
        }
        /* Fase 36: volcado propio, detrás de un flag nuevo. */
        {
            char *pv = arg_valor(cmd, "--volcar-productividad");
            if (pv) {
                char pv_ruta[MAX_PATH];
                int n = 0;
                while (pv[n] && pv[n] != ' ' && pv[n] != '\t' &&
                       n < MAX_PATH - 1) {
                    pv_ruta[n] = pv[n];
                    n++;
                }
                pv_ruta[n] = 0;
                productividad_volcar(pv_ruta);
            }
        }
        /* Fase 40: herramienta de saves del editor, detras de flags nuevos. */
        {
            char *sc = arg_valor(cmd, "--saves-carpeta");
            char *sv = arg_valor(cmd, "--saves-volcar");
            char *sb = arg_valor(cmd, "--saves-borrar");
            if (sv) {
                char salida[MAX_PATH], carpeta[MAX_PATH], borrar[MAX_PATH];
                int n;
                n = 0;
                while (sv[n] && sv[n] != ' ' && sv[n] != '\t' &&
                       n < MAX_PATH - 1) { salida[n] = sv[n]; n++; }
                salida[n] = 0;
                n = 0;
                if (sc)
                    while (sc[n] && sc[n] != ' ' && sc[n] != '\t' &&
                           n < MAX_PATH - 1) { carpeta[n] = sc[n]; n++; }
                carpeta[n] = 0;
                n = 0;
                if (sb)
                    while (sb[n] && sb[n] != ' ' && sb[n] != '\t' &&
                           n < MAX_PATH - 1) { borrar[n] = sb[n]; n++; }
                borrar[n] = 0;
                datos_saves_herramienta(carpeta, salida, borrar);
            }
        }
        /* Plan 16.4: muestra el mismo grafo en el gestor y en el inspector.
         * --gestor-modelo "entrada.log" solo proyecta en memoria: no guarda
         * LOGIC3 y no crea actor, jugador ni movimiento. */
        {
            char *gm = arg_valor(cmd, "--gestor-modelo");
            if (gm) {
                char entrada[MAX_PATH];
                char diagnostico[PM_ERROR_MAX];
                ProjectModel *m;
                int i;

                recortar_texto(entrada, gm, sizeof(entrada));
                diagnostico[0] = '\0';
                m = ml_leer(entrada, diagnostico, sizeof(diagnostico));
                if (m) {
                    g.modelo_gestor_entidades = m->num_entidades;
                    g.modelo_gestor_conexiones = 0;
                    for (i = 0; i < m->num_entidades; i++)
                        if (m->entidades[i].kind == PM_KIND_CONEXION)
                            g.modelo_gestor_conexiones++;
                    recortar_texto(g.modelo_gestor_origen, entrada,
                                   (int)sizeof(g.modelo_gestor_origen));
                    g.modelo_gestor_activo = 1;
                    programa_proyectar_modelo(m);
                    pm_destruir(m);
                } else {
                    g.modelo_gestor_activo = 0;
                    g.modelo_gestor_entidades = 0;
                    g.modelo_gestor_conexiones = 0;
                    g.modelo_gestor_origen[0] = '\0';
                    programa_reiniciar();
                }
                g.vista = VISTA_MODULOS;
                g.programa_modo = 1;
            }
        }
        /* Fase 9: importa el gestor visual antiguo y abre el resultado en
         * Inicio. --migrar-visual "entrada|salida|copia|informe" acepta los
         * dos ultimos campos opcionales; la copia por defecto es .bak. */
        {
            char *mv = arg_valor(cmd, "--migrar-visual");
            if (mv) {
                char entrada[MAX_PATH];
                char salida[MAX_PATH];
                char copia[MAX_PATH];
                char informe[MAX_PATH];
                char diagnostico[PM_ERROR_MAX];
                const char *p = mv;
                FILE *df;

                tramo_flag_modelo(entrada, (int)sizeof(entrada), &p);
                tramo_flag_modelo(salida, (int)sizeof(salida), &p);
                tramo_flag_modelo(copia, (int)sizeof(copia), &p);
                tramo_flag_modelo(informe, (int)sizeof(informe), &p);
                diagnostico[0] = '\0';
                if (!copia[0]) _snprintf(copia, sizeof(copia), "%s.bak", entrada);
                if (migrar_visual(entrada, salida, copia, informe,
                                  diagnostico, sizeof(diagnostico))) {
                    g.vista = VISTA_INICIO;
                    g.programa_modo = 0;
                    app_actualizar_titulo();
                } else {
                    df = fopen("migracion.diagnostico.txt", "w");
                    if (df) {
                        fputs(diagnostico, df);
                        fputc('\n', df);
                        fclose(df);
                    }
                }
            }
        }
        {
            char *vi = arg_valor(cmd, "--volcar-inicio");
            if (vi) {
                char ruta[MAX_PATH];
                recortar_texto(ruta, vi, sizeof(ruta));
                inicio_vista_volcar(ruta);
            }
        }
        /* Fase 6: edicion determinista de scripts y asociaciones. Se ejecuta
         * antes de --guardar-modelo para que un mismo proceso pueda crear el
         * .lobo, asociarlo y guardar el .log resultante. */
        {
            char *sc = arg_valor(cmd, "--inicio-scripts");
            if (sc) {
                char diagnostico[PM_ERROR_MAX];
                int scripts_ok = inicio_ejecutar(inicio_servicio(), sc,
                                                 diagnostico, sizeof(diagnostico));
                if (!scripts_ok) {
                    FILE *df = fopen("scripts.diagnostico.txt", "w");
                    if (df) {
                        fputs(diagnostico, df);
                        fputc('\n', df);
                        fclose(df);
                    }
                }
            }
        }
        /* Guarda el ProjectModel vivo de Inicio en LOG 2, manteniendo el
         * mismo orden semantico que el lector y todas sus referencias. */
        {
            char *gm2 = arg_valor(cmd, "--guardar-modelo");
            if (gm2 && g.vista == VISTA_INICIO) {
                char ruta[MAX_PATH];
                char diagnostico[PM_ERROR_MAX];
                recortar_texto(ruta, gm2, sizeof(ruta));
                diagnostico[0] = 0;
                if (!inicio_modelo_guardar(ruta, diagnostico, sizeof(diagnostico)))
                    app_estado("no se pudo guardar el modelo: %s", diagnostico);
            }
        }
        /* Fase 47: volcado propio del constructor, con flag nuevo para no
         * alterar ninguna suite anterior. */
        {
            char *vpr = arg_valor(cmd, "--volcar-programa");
            if (vpr) {
                char ruta[MAX_PATH];
                recortar_texto(ruta, vpr, sizeof(ruta));
                programa_volcar(ruta);
            }
        }
        /* Contrato del ProjectModel del plan 16.1, sin tocar ninguna vista. */
        {
            char *vm = arg_valor(cmd, "--volcar-modelo");
            if (vm) {
                char ruta[MAX_PATH];
                recortar_texto(ruta, vm, sizeof(ruta));
                volcar_modelo_minimo(ruta);
            }
        }
        /* Plan 16.2 y 16.3: convierte el .log al ProjectModel y valida
         * recursos. El ultimo campo es opcional:
         * --leer-modelo "entrada.log|volcado.txt|diagnostico.txt|recursos.txt" */
        {
            char *lm = arg_valor(cmd, "--leer-modelo");
            if (lm) {
                char entrada[MAX_PATH];
                char volcado[MAX_PATH];
                char ruta_diag[MAX_PATH];
                char ruta_recursos[MAX_PATH];
                char raiz[MAX_PATH];
                char diagnostico[PM_ERROR_MAX];
                char recursos_diag[PM_ERROR_MAX];
                const char *p = lm;
                ProjectModel *m;
                FILE *df;

                tramo_flag_modelo(entrada, (int)sizeof(entrada), &p);
                tramo_flag_modelo(volcado, (int)sizeof(volcado), &p);
                tramo_flag_modelo(ruta_diag, (int)sizeof(ruta_diag), &p);
                tramo_flag_modelo(ruta_recursos, (int)sizeof(ruta_recursos), &p);
                diagnostico[0] = '\0';
                m = ml_leer(entrada, diagnostico, sizeof(diagnostico));
                if (m) {
                    pm_volcar(m, volcado);
                    if (ruta_recursos[0]) {
                        ruta_carpeta(raiz, (int)sizeof(raiz), entrada);
                        if (!raiz[0]) strcpy(raiz, ".");
                        pm_validar_recursos(m, raiz, recursos_diag,
                                            sizeof(recursos_diag));
                        if ((df = fopen(ruta_recursos, "w")) != NULL) {
                            fputs(recursos_diag, df);
                            fputc('\n', df);
                            fclose(df);
                        }
                    }
                    pm_destruir(m);
                }
                if (ruta_diag[0] && (df = fopen(ruta_diag, "w")) != NULL) {
                    fputs(diagnostico, df);
                    fputc('\n', df);
                    fclose(df);
                }
            }
        }
        /* Plan 16.7: diagnostico JSON de recursos faltantes. Usa el mismo
         * ProjectModel que --leer-modelo y la misma comprobacion en disco;
         * solo cambia el formato de salida para que una IA pueda leerlo. */
        {
            char *dj = arg_valor(cmd, "--diagnostico-json");
            if (dj) {
                char entrada[MAX_PATH];
                char salida[MAX_PATH];
                char raiz[MAX_PATH];
                char diagnostico[PM_ERROR_MAX];
                char json[32768];
                const char *p = dj;
                ProjectModel *m;
                FILE *df;

                tramo_flag_modelo(entrada, (int)sizeof(entrada), &p);
                tramo_flag_modelo(salida, (int)sizeof(salida), &p);
                diagnostico[0] = '\0';
                m = ml_leer(entrada, diagnostico, sizeof(diagnostico));
                if (m) {
                    ruta_carpeta(raiz, (int)sizeof(raiz), entrada);
                    if (!raiz[0]) strcpy(raiz, ".");
                    pm_validar_recursos_json(m, raiz, json, sizeof(json));
                    pm_destruir(m);
                } else {
                    pm_validar_recursos_json(NULL, NULL, json, sizeof(json));
                }
                if (salida[0] && (df = fopen(salida, "w")) != NULL) {
                    fputs(json, df);
                    fputc('\n', df);
                    fclose(df);
                }
            }
        }
        /* Plan 16.6: puente sin ventanas .log -> modelo -> LOGIC3. El
         * adaptador usa el escritor canonico y deja un diagnostico opcional. */
        {
            char *m3 = arg_valor(cmd, "--modelo-logic3");
            if (m3) {
                char entrada[MAX_PATH];
                char salida[MAX_PATH];
                char ruta_diag[MAX_PATH];
                char diagnostico[PM_ERROR_MAX];
                const char *p = m3;
                ProjectModel *m;
                FILE *df;

                tramo_flag_modelo(entrada, (int)sizeof(entrada), &p);
                tramo_flag_modelo(salida, (int)sizeof(salida), &p);
                tramo_flag_modelo(ruta_diag, (int)sizeof(ruta_diag), &p);
                diagnostico[0] = '\0';
                m = ml_leer(entrada, diagnostico, sizeof(diagnostico));
                if (m) {
                    if (ml3_generar(m, salida, diagnostico,
                                    sizeof(diagnostico)))
                        strcpy(diagnostico, "sin errores");
                    pm_destruir(m);
                }
                if (ruta_diag[0] && (df = fopen(ruta_diag, "w")) != NULL) {
                    fputs(diagnostico, df);
                    fputc('\n', df);
                    fclose(df);
                }
            }
        }
        /* Inicio: creacion, borrado, reparentado y undo/redo del ProjectModel.
         * Una sola flag encadena las ordenes separadas por ';' para poder
         * congelar estados intermedios con 'volcar|ruta' en las pruebas. */
        {
            char *ini = arg_valor(cmd, "--inicio");
            if (ini) {
                char diagnostico[PM_ERROR_MAX];
                inicio_ok = inicio_ejecutar(inicio_servicio(), ini,
                                            diagnostico, sizeof(diagnostico));
                if (!inicio_ok) {
                    FILE *df = fopen("inicio.diagnostico.txt", "w");
                    if (df) {
                        fputs(diagnostico, df);
                        fputc('\n', df);
                        fclose(df);
                    }
                }
            }
        }
        /* Vuelca el lobo generado (fase 33) como texto plano. */
        {
            char *codigo_arg = arg_valor(cmd, "--volcar-codigo");
            if (codigo_arg) {
                char ruta[MAX_PATH];
                int n = 0;
                FILE *cf;
                while (codigo_arg[n] && codigo_arg[n] != ' ' &&
                       codigo_arg[n] != '\t' && n < MAX_PATH - 1) {
                    ruta[n] = codigo_arg[n];
                    n++;
                }
                ruta[n] = 0;
                cf = ruta[0] ? fopen(ruta, "w") : NULL;
                if (cf) {
                    fputs(compilador_bloques_codigo(), cf);
                    fclose(cf);
                }
            }
        }
        app_dibujar();
        ok = imagen_guardar_bmp(captura, g.pixeles, g.ancho, g.alto);
        DestroyWindow(hwnd);
        g.hwnd = NULL;
        app_terminar();
        return (ok && inicio_ok) ? 0 : 2;
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
