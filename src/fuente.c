/* ==========================================================================
 * Fuente bitmap 5x7 propia.
 *
 * Cada glifo son 7 filas de 5 pixeles; el bit 4 de cada fila es el pixel de
 * la izquierda. El avance es de 6 px (5 del glifo + 1 de separacion) y la
 * escala es entera. La tabla y la metrica son las de `glifo`, `texto_ancho`,
 * `dibujar_texto` y `dibujar_texto_escala`.
 *
 * Convencion de alineacion vertical para que las letras no bailen dentro
 * de una misma linea: mayusculas y digitos ocupan las filas 0-6, las
 * minusculas sin ascendente ocupan la banda de altura-x de las filas 2-6
 * (los puntos bajos de dos puntos y el punto van tambien ahi) y las
 * minusculas con ascendente (b, d, f, l, t y la u con acento) suben hasta
 * la fila 0. Ninguna minuscula debe empezar en la fila 1 ni terminar en la
 * fila 5.
 * ========================================================================== */
#include "app.h"

typedef struct Glifo {
    unsigned char filas[7];
} Glifo;

/* Digitos 0-9. */
static const Glifo G_DIGITOS[10] = {
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

static const Glifo G_PORCENTAJE = { { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 } };
static const Glifo G_P          = { { 0x00, 0x00, 0x1E, 0x11, 0x11, 0x1E, 0x10 } };
static const Glifo G_X          = { { 0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11 } };
static const Glifo G_C          = { { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } };
static const Glifo G_MIN_C      = { { 0x00, 0x00, 0x0E, 0x10, 0x10, 0x11, 0x0E } };
static const Glifo G_A          = { { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } };
static const Glifo G_T          = { { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } };
static const Glifo G_S          = { { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } };
static const Glifo G_MIN_L      = { { 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x06 } };
static const Glifo G_MIN_G      = { { 0x00, 0x00, 0x0F, 0x11, 0x11, 0x0F, 0x01 } };
static const Glifo G_MIN_F      = { { 0x06, 0x08, 0x1E, 0x08, 0x08, 0x08, 0x08 } };
static const Glifo G_R          = { { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } };
static const Glifo G_E          = { { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } };
static const Glifo G_D          = { { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E } };
static const Glifo G_O          = { { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } };
static const Glifo G_N          = { { 0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11 } };
static const Glifo G_MIN_R      = { { 0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10 } };
static const Glifo G_MIN_E      = { { 0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E } };
static const Glifo G_MIN_D      = { { 0x01, 0x01, 0x0F, 0x11, 0x11, 0x11, 0x0F } };
static const Glifo G_MIN_B      = { { 0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x1E } };
static const Glifo G_MIN_O      = { { 0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E } };
static const Glifo G_MIN_N      = { { 0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11 } };
static const Glifo G_L          = { { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } };
static const Glifo G_G          = { { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E } };
static const Glifo G_MIN_I      = { { 0x04, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04 } };
static const Glifo G_MIN_S      = { { 0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E } };
static const Glifo G_MIN_A      = { { 0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F } };
static const Glifo G_MIN_Y      = { { 0x00, 0x00, 0x11, 0x11, 0x0F, 0x01, 0x0E } };
static const Glifo G_MIN_U      = { { 0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0D } };
static const Glifo G_MIN_U_ACENTO = { { 0x0C, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0D } };
static const Glifo G_MIN_T      = { { 0x08, 0x08, 0x1E, 0x08, 0x08, 0x09, 0x06 } };
static const Glifo G_MIN_M      = { { 0x00, 0x00, 0x1A, 0x15, 0x15, 0x15, 0x15 } };
static const Glifo G_MIN_W      = { { 0x00, 0x00, 0x11, 0x11, 0x15, 0x1B, 0x11 } };
static const Glifo G_MIN_V      = { { 0x00, 0x00, 0x11, 0x11, 0x11, 0x0A, 0x04 } };
static const Glifo G_H          = { { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } };
static const Glifo G_MAY_I      = { { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } };
static const Glifo G_V          = { { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 } };
static const Glifo G_M          = { { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } };
static const Glifo G_MAY_U      = { { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } };
static const Glifo G_MAY_Y      = { { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 } };
static const Glifo G_MAY_F      = { { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } };
static const Glifo G_MAY_P      = { { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } };
static const Glifo G_MAY_B      = { { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } };
static const Glifo G_MAY_Z      = { { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F } };
static const Glifo G_DOS_PUNTOS = { { 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x04 } };
static const Glifo G_PUNTO      = { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04 } };
static const Glifo G_ESPACIO    = { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
static const Glifo G_MAS        = { { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 } };
static const Glifo G_MENOS      = { { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 } };

/* Simbolos del lenguaje lobo (fase 8): comentario, comparaciones,
 * operadores logicos, parentesis y parentesis de choca(x, y). */
static const Glifo G_NUMERAL    = { { 0x0A, 0x0A, 0x1F, 0x0A, 0x0A, 0x00, 0x00 } };
static const Glifo G_IGUAL     = { { 0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00 } };
static const Glifo GUION_BAJO   = { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F } };
static const Glifo G_MENOR      = { { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 } };
static const Glifo G_MAYOR      = { { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 } };
static const Glifo G_ADMIRACION = { { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 } };
static const Glifo G_AMPERSAND  = { { 0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D } };
static const Glifo G_BARRA_VERT = { { 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } };
static const Glifo G_PAR_ABRIR  = { { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 } };
static const Glifo G_PAR_CERRAR = { { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 } };
static const Glifo G_DIAGONAL   = { { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 } };
static const Glifo G_ASTERISCO  = { { 0x00, 0x0A, 0x04, 0x1F, 0x04, 0x0A, 0x00 } };
static const Glifo G_COMA       = { { 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08 } };

/* Letras que faltaban para los nombres lobo del usuario. */
static const Glifo G_MIN_H      = { { 0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x11 } };
static const Glifo G_MIN_K      = { { 0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12 } };
static const Glifo G_MIN_Q      = { { 0x00, 0x00, 0x0F, 0x11, 0x11, 0x1F, 0x01 } };
static const Glifo G_MIN_Z      = { { 0x00, 0x00, 0x1F, 0x02, 0x04, 0x08, 0x1F } };
static const Glifo G_J          = { { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C } };
static const Glifo G_K          = { { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } };
static const Glifo G_Q          = { { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x0E, 0x01 } };
static const Glifo G_MAY_W      = { { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A } };

#define G(g) ((g).filas)

/* Mapeo de `glifo(caracter)`. Devuelve las 7 filas del glifo o NULL si no hay
 * glifo; el texto sigue avanzando 6 px. */
const unsigned char *fuente_glifo(int codepoint) {
    if (codepoint >= '0' && codepoint <= '9') return G(G_DIGITOS[codepoint - '0']);
    switch (codepoint) {
        case '%': return G(G_PORCENTAJE);
        case '#': return G(G_NUMERAL);
        case '=': return G(G_IGUAL);
        case '_': return G(GUION_BAJO);
        case '<': return G(G_MENOR);
        case '>': return G(G_MAYOR);
        case '!': return G(G_ADMIRACION);
        case '&': return G(G_AMPERSAND);
        case '|': return G(G_BARRA_VERT);
        case '(': return G(G_PAR_ABRIR);
        case ')': return G(G_PAR_CERRAR);
        case '/': return G(G_DIAGONAL);
        case '*': return G(G_ASTERISCO);
        case ',': return G(G_COMA);
        case 'h': return G(G_MIN_H);
        case 'k': return G(G_MIN_K);
        case 'q': return G(G_MIN_Q);
        case 'z': return G(G_MIN_Z);
        case 'J': return G(G_J);
        case 'K': return G(G_K);
        case 'Q': return G(G_Q);
        case 'W': return G(G_MAY_W);
        case 'p': return G(G_P);
        case 'P': return G(G_MAY_P);
        case 'B': return G(G_MAY_B);
        case 'Z': return G(G_MAY_Z);
        case ':': return G(G_DOS_PUNTOS);
        case '.': return G(G_PUNTO);
        case ' ': return G(G_ESPACIO);
        case '+': return G(G_MAS);
        case '-': return G(G_MENOS);
        case 'x': case 'X': return G(G_X);
        case 'C': return G(G_C);
        case 'c': return G(G_MIN_C);
        case 'A': return G(G_A);
        case 'T': return G(G_T);
        case 'S': return G(G_S);
        case 'l': return G(G_MIN_L);
        case 'g': return G(G_MIN_G);
        case 'f': return G(G_MIN_F);
        case 'F': return G(G_MAY_F);
        case 'R': return G(G_R);
        case 'r': return G(G_MIN_R);
        case 'E': return G(G_E);
        case 'e': return G(G_MIN_E);
        case 'D': return G(G_D);
        case 'd': return G(G_MIN_D);
        case 'b': return G(G_MIN_B);
        case 'O': return G(G_O);
        case 'o': return G(G_MIN_O);
        case 'N': return G(G_N);
        case 'n': return G(G_MIN_N);
        case 'L': return G(G_L);
        case 'G': return G(G_G);
        case 'H': return G(G_H);
        case 'I': return G(G_MAY_I);
        case 'M': return G(G_M);
        case 'V': return G(G_V);
        case 'Y': return G(G_MAY_Y);
        case 'U': return G(G_MAY_U);
        case 'i': return G(G_MIN_I);
        case 's': return G(G_MIN_S);
        case 'a': return G(G_MIN_A);
        case 'y': return G(G_MIN_Y);
        case 'u': return G(G_MIN_U);
        case 0xFA: return G(G_MIN_U_ACENTO);   /* ú */
        case 't': return G(G_MIN_T);
        case 'm': return G(G_MIN_M);
        case 'w': return G(G_MIN_W);
        case 'v': return G(G_MIN_V);
        default: return NULL;
    }
}

/* Decodifica un caracter UTF-8 y avanza el puntero. Los textos de la interfaz
 * son ASCII salvo "ú", que aparece en el atajo "Mayús+Ctrl+A". */
static int fuente_siguiente(const unsigned char **p) {
    const unsigned char *s = *p;
    unsigned char c = *s;
    if (c == 0) return 0;
    if (c < 0x80) {
        *p = s + 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0) {
        int cp = ((c & 0x1F) << 6) | (s[1] & 0x3F);
        *p = s + 2;
        return cp;
    }
    if ((c & 0xF0) == 0xE0) {
        int cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *p = s + 3;
        return cp;
    }
    if ((c & 0xF8) == 0xF0) {
        int cp = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
                 ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *p = s + 4;
        return cp;
    }
    *p = s + 1;
    return c;
}

static void fuente_pixel(int x, int y, u32 color) {
    if (!g.pixeles || x < 0 || y < 0 || x >= g.ancho || y >= g.alto) return;
    g.pixeles[(size_t)y * (size_t)g.ancho + (size_t)x] = color;
}

/* Igual que `texto_ancho`: 5 px por caracter mas 1 de separacion. */
int fuente_texto_ancho(const char *texto) {
    const unsigned char *p;
    int n = 0;
    if (!texto) return 0;
    p = (const unsigned char *)texto;
    while (*p) {
        fuente_siguiente(&p);
        n++;
    }
    return n == 0 ? 0 : n * 6 - 1;
}

int fuente_texto_ancho_escala(const char *texto, int escala) {
    if (escala < 1) escala = 1;
    return fuente_texto_ancho(texto) * escala;
}

/* Igual que `dibujar_texto`: 5x7, avance 6 y recorte al buffer. */
void fuente_dibujar(int x, int y, const char *texto, u32 color) {
    const unsigned char *p;
    int cursor = x;
    if (!texto) return;
    p = (const unsigned char *)texto;
    while (*p) {
        int cp = fuente_siguiente(&p);
        const unsigned char *glifo = fuente_glifo(cp);
        if (glifo) {
            int fila, columna;
            for (fila = 0; fila < 7; fila++) {
                unsigned char bits = glifo[fila];
                for (columna = 0; columna < 5; columna++) {
                    if (bits & (1 << (4 - columna))) {
                        fuente_pixel(cursor + columna, y + fila, color);
                    }
                }
            }
        }
        cursor += 6;
    }
}

/* Igual que `dibujar_texto_escala`: cada pixel del glifo ocupa escala x escala. */
void fuente_dibujar_escala(int x, int y, const char *texto, u32 color, int escala) {
    const unsigned char *p;
    int cursor = x;
    if (!texto) return;
    if (escala < 1) escala = 1;
    p = (const unsigned char *)texto;
    while (*p) {
        int cp = fuente_siguiente(&p);
        const unsigned char *glifo = fuente_glifo(cp);
        if (glifo) {
            int fila, columna, dx, dy;
            for (fila = 0; fila < 7; fila++) {
                unsigned char bits = glifo[fila];
                for (columna = 0; columna < 5; columna++) {
                    if (bits & (1 << (4 - columna))) {
                        int px = cursor + columna * escala;
                        int py = y + fila * escala;
                        for (dy = 0; dy < escala; dy++) {
                            for (dx = 0; dx < escala; dx++) {
                                fuente_pixel(px + dx, py + dy, color);
                            }
                        }
                    }
                }
            }
        }
        cursor += 6 * escala;
    }
}
