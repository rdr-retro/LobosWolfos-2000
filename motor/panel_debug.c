/* panel_debug.c - panel visual del depurador (fase 34).
 *
 * Fuente bitmap 5x7 propia, identica en metrica a la del editor: 7 filas
 * de 5 pixeles, bit 4 a la izquierda y avance de 6 pixeles. Se dibuja
 * directamente en el framebuffer para que aparezca tanto en la ventana
 * como en las capturas deterministas.
 */
#include <stdio.h>
#include <string.h>

#include "panel_debug.h"
#include "depurador.h"

typedef struct GlifoDebug {
    unsigned char filas[7];
} GlifoDebug;

/* Digitos y mayusculas: los caracteres que necesita el panel. */
static const GlifoDebug G_NUM[10] = {
    { { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } },
    { { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } },
    { { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E } },
    { { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } },
    { { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
    { { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } },
    { { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
    { { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } },
    { { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
};

static const GlifoDebug G_LETRAS[26] = {
    /* A */ { { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
    /* B */ { { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
    /* C */ { { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } },
    /* D */ { { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E } },
    /* E */ { { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } },
    /* F */ { { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
    /* G */ { { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E } },
    /* H */ { { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
    /* I */ { { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    /* J */ { { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C } },
    /* K */ { { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } },
    /* L */ { { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
    /* M */ { { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } },
    /* N */ { { 0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11 } },
    /* O */ { { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
    /* P */ { { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
    /* Q */ { { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x0E, 0x01 } },
    /* R */ { { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } },
    /* S */ { { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } },
    /* T */ { { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    /* U */ { { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
    /* V */ { { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 } },
    /* W */ { { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A } },
    /* X */ { { 0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11 } },
    /* Y */ { { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 } },
    /* Z */ { { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F } },
};

static const GlifoDebug G_ESPACIO = { { 0, 0, 0, 0, 0, 0, 0 } };
static const GlifoDebug G_DOS_PUNTOS = { { 0x00, 0x00, 0x04, 0x04,
                                           0x00, 0x04, 0x04 } };
static const GlifoDebug G_IGUAL = { { 0x00, 0x00, 0x1F, 0x00,
                                      0x1F, 0x00, 0x00 } };
static const GlifoDebug G_MENOS = { { 0x00, 0x00, 0x00, 0x1F,
                                      0x00, 0x00, 0x00 } };
static const GlifoDebug G_MAYOR = { { 0x02, 0x04, 0x08, 0x10,
                                      0x08, 0x04, 0x02 } };
static const GlifoDebug G_PUNTO = { { 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x04, 0x04 } };
static const GlifoDebug G_DIAGONAL = { { 0x01, 0x01, 0x02, 0x04,
                                         0x08, 0x10, 0x10 } };
static const GlifoDebug G_NUMERAL = { { 0x0A, 0x0A, 0x1F, 0x0A,
                                        0x0A, 0x00, 0x00 } };
static const GlifoDebug G_INTERROGACION = { { 0x0E, 0x11, 0x01, 0x02,
                                              0x04, 0x00, 0x04 } };

static int activo;

void panel_debug_activar(int valor) { activo = valor ? 1 : 0; }
int panel_debug_esta_activo(void) { return activo; }

static const GlifoDebug *glifo_debug(char c) {
    if (c >= '0' && c <= '9') return &G_NUM[c - '0'];
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return &G_LETRAS[c - 'A'];
    if (c == ':') return &G_DOS_PUNTOS;
    if (c == '=') return &G_IGUAL;
    if (c == '-') return &G_MENOS;
    if (c == '>') return &G_MAYOR;
    if (c == '.') return &G_PUNTO;
    if (c == '/') return &G_DIAGONAL;
    if (c == '#') return &G_NUMERAL;
    if (c == '?') return &G_INTERROGACION;
    return &G_ESPACIO;
}

static void pixel_debug(u32 *fb, int ancho, int alto, int x, int y, u32 color) {
    if (x < 0 || y < 0 || x >= ancho || y >= alto) return;
    fb[y * ancho + x] = color;
}

static void rect_debug(u32 *fb, int ancho, int alto,
                       int x, int y, int w, int h, u32 color) {
    int i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            pixel_debug(fb, ancho, alto, i, j, color);
}

static void texto_debug(u32 *fb, int ancho, int alto,
                        int x, int y, const char *texto, u32 color) {
    int n = 0;
    while (texto && texto[n]) {
        const GlifoDebug *g = glifo_debug(texto[n]);
        int fila, col;
        for (fila = 0; fila < 7; fila++) {
            for (col = 0; col < 5; col++) {
                if (g->filas[fila] & (0x10u >> col))
                    pixel_debug(fb, ancho, alto, x + n * 6 + col,
                                y + fila, color);
            }
        }
        n++;
    }
}

/* Linea de observado: NOMBRE=VALOR, o ? si no se pudo resolver. */
static void linea_observado(int i, const void *memoria, const void *proyecto,
                            char *salida, int tam) {
    int valor, encontrado = 0;
    const char *nombre = depurador_observado(i);
    valor = depurador_observado_valor(i, memoria, proyecto, &encontrado);
    if (encontrado)
        _snprintf(salida, tam, "%s=%d", nombre, valor);
    else
        _snprintf(salida, tam, "%s=?", nombre);
    salida[tam - 1] = '\0';
}

void panel_debug_dibujar(u32 *fb, int ancho, int alto,
                         const void *memoria, const void *proyecto) {
    const u32 fondo = 0xFF0A1420u;
    const u32 borde = 0xFF3A8AC0u;
    const u32 texto = 0xFFD0E8FFu;
    const u32 resaltado = 0xFFFFD070u;
    const int x = 4, y = 4, ancho_panel = 228, alto_panel = 112;
    char linea[DEPURADOR_NOMBRE_MAX + 32];
    int fila = 0, i;
    int pausado = depurador_esta_pausado();

    if (!activo || !fb || ancho < ancho_panel + 8 || alto < alto_panel + 8)
        return;

    rect_debug(fb, ancho, alto, x, y, ancho_panel, alto_panel, fondo);
    rect_debug(fb, ancho, alto, x, y, ancho_panel, 1, borde);
    rect_debug(fb, ancho, alto, x, y + alto_panel - 1, ancho_panel, 1, borde);
    rect_debug(fb, ancho, alto, x, y, 1, alto_panel, borde);
    rect_debug(fb, ancho, alto, x + ancho_panel - 1, y, 1, alto_panel, borde);

    _snprintf(linea, sizeof(linea), "> DEPURADOR PAUSA=%d", pausado);
    linea[sizeof(linea) - 1] = '\0';
    texto_debug(fb, ancho, alto, x + 4, y + 4 + fila * 8, linea, resaltado);
    fila++;

    _snprintf(linea, sizeof(linea), " BLOQUE %d EVENTO %d",
              depurador_bloque_actual(), depurador_evento_actual());
    linea[sizeof(linea) - 1] = '\0';
    texto_debug(fb, ancho, alto, x + 4, y + 4 + fila * 8, linea,
                pausado ? resaltado : texto);
    fila++;

    _snprintf(linea, sizeof(linea), " SCRIPT %d PC %d ERR %d",
              depurador_script_actual(), depurador_pc_actual(),
              depurador_error_actual());
    linea[sizeof(linea) - 1] = '\0';
    texto_debug(fb, ancho, alto, x + 4, y + 4 + fila * 8, linea, texto);
    fila++;

    texto_debug(fb, ancho, alto, x + 4, y + 4 + fila * 8,
                "> OBSERVADOS", resaltado);
    fila++;
    for (i = 0; i < depurador_num_observados() && i < 3; i++) {
        linea_observado(i, memoria, proyecto, linea, sizeof(linea));
        texto_debug(fb, ancho, alto, x + 4, y + 4 + fila * 8, linea, texto);
        fila++;
    }

    texto_debug(fb, ancho, alto, x + 4, y + 4 + fila * 8,
                "> CONSOLA", resaltado);
    fila++;
    i = depurador_num_consola() - 2;
    if (i < 0) i = 0;
    for (; i < depurador_num_consola() && fila < 13; i++) {
        _snprintf(linea, sizeof(linea), " %s", depurador_consola_linea(i));
        linea[sizeof(linea) - 1] = '\0';
        texto_debug(fb, ancho, alto, x + 4, y + 4 + fila * 8, linea, texto);
        fila++;
    }
}
