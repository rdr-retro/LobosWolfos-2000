/* hud.c - HUD, texto, UI y dialogos (fase 38).
 *
 * Fuente bitmap 5x7 propia, con la misma metrica que el editor y el panel del
 * depurador: 7 filas de 5 pixeles, bit 4 a la izquierda y avance de 6 pixeles
 * por caracter (12 con escala 2). Las cadenas de interfaz conservan sus
 * acentos; al dibujar, las vocales acentuadas y la ene caen en su letra base
 * para que la fuente, que solo tiene 5x7, pueda mostrarlas.
 *
 * Todo se pinta directamente en el framebuffer, igual que el mundo, de modo
 * que una captura determinista ve exactamente lo que ve la ventana.
 */
#include <stdio.h>
#include <string.h>

#include "hud.h"
#include "entrada.h"

typedef struct GlifoHud {
    unsigned char filas[7];
} GlifoHud;

static const GlifoHud G_NUM[10] = {
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

static const GlifoHud G_LETRAS[26] = {
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
    /* X */ { { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 } },
    /* Y */ { { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 } },
    /* Z */ { { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F } },
};

static const GlifoHud G_ESPACIO = { { 0, 0, 0, 0, 0, 0, 0 } };
static const GlifoHud G_DOS_PUNTOS = { { 0x00, 0x00, 0x04, 0x04,
                                         0x00, 0x04, 0x04 } };
static const GlifoHud G_IGUAL = { { 0x00, 0x00, 0x1F, 0x00,
                                    0x1F, 0x00, 0x00 } };
static const GlifoHud G_MENOS = { { 0x00, 0x00, 0x00, 0x1F,
                                    0x00, 0x00, 0x00 } };
static const GlifoHud G_MAYOR = { { 0x02, 0x04, 0x08, 0x10,
                                    0x08, 0x04, 0x02 } };
static const GlifoHud G_PUNTO = { { 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x04, 0x04 } };
static const GlifoHud G_DIAGONAL = { { 0x01, 0x01, 0x02, 0x04,
                                       0x08, 0x10, 0x10 } };
static const GlifoHud G_NUMERAL = { { 0x0A, 0x0A, 0x1F, 0x0A,
                                      0x0A, 0x00, 0x00 } };
static const GlifoHud G_INTERROGACION = { { 0x0E, 0x11, 0x01, 0x02,
                                            0x04, 0x00, 0x04 } };
static const GlifoHud G_CORCHETE = { { 0x0E, 0x10, 0x10, 0x10,
                                       0x10, 0x10, 0x0E } };

/* Colores del HUD (el framebuffer es de 32 bits, como en el resto del motor). */
#define HUD_FONDO      0xC010141Cu
#define HUD_BORDE      0xFFD8D8D8u
#define HUD_TEXTO      0xFFF0F0F0u
#define HUD_RESAR      0xFFFFC040u
#define HUD_BARRA      0xFF40C060u
#define HUD_BARRA_VACIA 0xFF603030u
#define HUD_BOTON      0xFF305080u

static int activo;
static int vidas;
static int puntos;
static char dialogo[HUD_DIALOGO_LINEAS][HUD_TEXTO_MAX];
static int dialogo_n;
static int dialogo_actual;
static int plantilla;
static HudElemento elementos[HUD_ELEMENTOS_MAX];
static int nelementos;
static HudImagenFn imagen_fn;
static void *imagen_ctx;
static int clic_ahora;
static int clic_antes;
static int raton_x_hud;
static int raton_y_hud;

void motor_hud_activar(int valor) { activo = valor ? 1 : 0; }
int motor_hud_esta_activo(void) { return activo; }

void motor_hud_limpiar(void) {
    vidas = 0;
    puntos = 0;
    dialogo_n = 0;
    dialogo_actual = 0;
    plantilla = 0;
    nelementos = 0;
    memset(dialogo, 0, sizeof(dialogo));
    memset(elementos, 0, sizeof(elementos));
}

void motor_hud_vidas(int valor) { vidas = valor; }
void motor_hud_puntos(int valor) { puntos = valor; }
int motor_hud_leer_vidas(void) { return vidas; }
int motor_hud_leer_puntos(void) { return puntos; }

static void copiar_hud(char *dst, int tam, const char *src) {
    int i = 0;
    if (!src) src = "";
    while (src[i] && i < tam - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void motor_hud_dialogo(int linea, const char *texto) {
    if (linea < 0 || linea >= HUD_DIALOGO_LINEAS) return;
    copiar_hud(dialogo[linea], HUD_TEXTO_MAX, texto);
    if (dialogo[linea][0] && linea + 1 > dialogo_n) dialogo_n = linea + 1;
}

void motor_hud_dialogo_tres(const char *a, const char *b, const char *c) {
    motor_hud_dialogo_limpiar();
    motor_hud_dialogo(0, a);
    motor_hud_dialogo(1, b);
    motor_hud_dialogo(2, c);
}

void motor_hud_dialogo_limpiar(void) {
    memset(dialogo, 0, sizeof(dialogo));
    dialogo_n = 0;
    dialogo_actual = 0;
}

int motor_hud_dialogo_lineas(void) { return dialogo_n; }

void motor_hud_imagen_callback(HudImagenFn fn, void *ctx) {
    imagen_fn = fn;
    imagen_ctx = ctx;
}

static HudElemento *elemento_nuevo(int tipo) {
    HudElemento *e;
    if (nelementos >= HUD_ELEMENTOS_MAX) return NULL;
    e = &elementos[nelementos++];
    memset(e, 0, sizeof(*e));
    e->tipo = tipo;
    return e;
}

int motor_hud_texto(int x, int y, const char *texto, u32 color, int escala) {
    HudElemento *e = elemento_nuevo(HUD_ELEM_TEXTO);
    if (!e) return 0;
    e->x = x;
    e->y = y;
    e->color = color;
    e->extra = escala < 1 ? 1 : (escala > 2 ? 2 : escala);
    copiar_hud(e->texto, HUD_TEXTO_MAX, texto);
    return 1;
}

int motor_hud_rect(int x, int y, int w, int h, u32 color) {
    HudElemento *e = elemento_nuevo(HUD_ELEM_RECT);
    if (!e) return 0;
    e->x = x; e->y = y; e->w = w; e->h = h; e->color = color;
    return 1;
}

int motor_hud_barra(int x, int y, int w, int h, int valor, int max,
                    u32 color) {
    HudElemento *e = elemento_nuevo(HUD_ELEM_BARRA);
    if (!e) return 0;
    e->x = x; e->y = y; e->w = w; e->h = h;
    e->valor = valor; e->max = max; e->color = color;
    return 1;
}

int motor_hud_imagen(int id, int x, int y) {
    HudElemento *e = elemento_nuevo(HUD_ELEM_IMAGEN);
    if (!e) return 0;
    e->id = id; e->x = x; e->y = y;
    return 1;
}

static int clic_dentro(int x, int y, int w, int h) {
    return raton_x_hud >= x && raton_x_hud < x + w &&
           raton_y_hud >= y && raton_y_hud < y + h;
}

int motor_hud_boton(int id, int x, int y, int w, int h,
                    const char *etiqueta) {
    HudElemento *e = elemento_nuevo(HUD_ELEM_BOTON);
    if (!e) return 0;
    e->id = id; e->x = x; e->y = y; e->w = w; e->h = h;
    copiar_hud(e->texto, HUD_TEXTO_MAX, etiqueta);
    e->pulsado = clic_ahora && clic_dentro(x, y, w, h);
    return e->pulsado;
}

void motor_hud_plantilla(int cual) { plantilla = cual; }

/* ------------------------------------------------------------------ */
/* Ciclo de fotograma                                                  */
/* ------------------------------------------------------------------ */

void motor_hud_fotograma(const void *entrada) {
    const MotorEntrada *e = (const MotorEntrada *)entrada;
    int dentro = 0, boton = 0;

    nelementos = 0;
    clic_antes = clic_ahora;
    clic_ahora = 0;
    if (!e) return;

    raton_x_hud = e->raton_x;
    raton_y_hud = e->raton_y;
    boton = motor_entrada_raton_boton(e, 1);
    clic_ahora = boton && !clic_antes;
    dentro = e->raton_dentro &&
             e->raton_x >= 0 && e->raton_y >= 0;
    if (clic_ahora && !dentro) clic_ahora = 0;

    if (motor_entrada_pulsada(e, ENT_ENTER) ||
        motor_entrada_pulsada(e, ENT_ESPACIO)) {
        dialogo_actual++;
        if (dialogo_actual >= HUD_DIALOGO_LINEAS || dialogo_actual >= dialogo_n)
            dialogo_actual = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Dibujo                                                             */
/* ------------------------------------------------------------------ */

static const GlifoHud *glifo_hud(char c) {
    unsigned char u;
    u = (unsigned char)c;
    /* Vocales acentuadas y ene: la cadena conserva el acento, el glifo no. */
    if (u == 0xC1 || u == 0xE1) c = 'A';
    else if (u == 0xC9 || u == 0xE9) c = 'E';
    else if (u == 0xCD || u == 0xED) c = 'I';
    else if (u == 0xD3 || u == 0xF3 || u == 0xD4 || u == 0xF4) c = 'O';
    else if (u == 0xDA || u == 0xFA) c = 'U';
    else if (u == 0xD1 || u == 0xF1) c = 'N';
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
    if (c == '?' || u == 0xA1 || u == 0xBF) return &G_INTERROGACION;
    if (c == '[' || c == ']') return &G_CORCHETE;
    return &G_ESPACIO;
}

static void pixel_hud(u32 *fb, int ancho, int alto, int x, int y, u32 color) {
    if (x < 0 || y < 0 || x >= ancho || y >= alto) return;
    fb[y * ancho + x] = color;
}

static void rect_hud(u32 *fb, int ancho, int alto,
                     int x, int y, int w, int h, u32 color) {
    int i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            pixel_hud(fb, ancho, alto, i, j, color);
}

static void borde_hud(u32 *fb, int ancho, int alto,
                      int x, int y, int w, int h, u32 color) {
    rect_hud(fb, ancho, alto, x, y, w, 1, color);
    rect_hud(fb, ancho, alto, x, y + h - 1, w, 1, color);
    rect_hud(fb, ancho, alto, x, y, 1, h, color);
    rect_hud(fb, ancho, alto, x + w - 1, y, 1, h, color);
}

static void texto_hud(u32 *fb, int ancho, int alto, int x, int y,
                      const char *texto, u32 color, int escala) {
    int n = 0, avance = 6 * escala;
    while (texto && texto[n]) {
        const GlifoHud *g = glifo_hud(texto[n]);
        int fila, col;
        for (fila = 0; fila < 7; fila++) {
            for (col = 0; col < 5; col++) {
                if (!(g->filas[fila] & (0x10u >> col))) continue;
                rect_hud(fb, ancho, alto, x + n * avance + col * escala,
                         y + fila * escala, escala, escala, color);
            }
        }
        n++;
    }
}

static int ancho_texto_hud(const char *texto, int escala) {
    int n = 0;
    while (texto && texto[n]) n++;
    return n * 6 * escala;
}

static void barra_hud(u32 *fb, int ancho, int alto, const HudElemento *e) {
    int lleno = 0;
    rect_hud(fb, ancho, alto, e->x, e->y, e->w, e->h, HUD_BARRA_VACIA);
    if (e->max > 0 && e->valor > 0) {
        long p = (long)e->w * e->valor / e->max;
        if (p < 0) p = 0;
        if (p > e->w) p = e->w;
        lleno = (int)p;
    }
    rect_hud(fb, ancho, alto, e->x, e->y, lleno, e->h, e->color);
    borde_hud(fb, ancho, alto, e->x, e->y, e->w, e->h, HUD_BORDE);
}

static void boton_hud(u32 *fb, int ancho, int alto, const HudElemento *e) {
    int tx, ty;
    rect_hud(fb, ancho, alto, e->x, e->y, e->w, e->h, HUD_BOTON);
    borde_hud(fb, ancho, alto, e->x, e->y, e->w, e->h, HUD_BORDE);
    tx = e->x + (e->w - ancho_texto_hud(e->texto, 1)) / 2;
    ty = e->y + (e->h - 7) / 2;
    texto_hud(fb, ancho, alto, tx, ty, e->texto, HUD_TEXTO, 1);
}

static void cabecera_hud(u32 *fb, int ancho, int alto) {
    char puntos_txt[24], vidas_txt[16];
    int px;
    rect_hud(fb, ancho, alto, 0, 0, ancho, 22, HUD_FONDO);
    rect_hud(fb, ancho, alto, 0, 21, ancho, 1, HUD_BORDE);
    _snprintf(puntos_txt, sizeof(puntos_txt), "PUNTOS: %d", puntos);
    puntos_txt[sizeof(puntos_txt) - 1] = '\0';
    _snprintf(vidas_txt, sizeof(vidas_txt), "%d", vidas);
    vidas_txt[sizeof(vidas_txt) - 1] = '\0';
    texto_hud(fb, ancho, alto, 8, 7, "VIDAS:", HUD_RESAR, 1);
    texto_hud(fb, ancho, alto, 50, 7, vidas_txt, HUD_RESAR, 1);
    px = ancho - 8 - ancho_texto_hud(puntos_txt, 1);
    if (px < 8) px = 8;
    texto_hud(fb, ancho, alto, px, 7, puntos_txt, HUD_TEXTO, 1);
}

static void dialogo_hud(u32 *fb, int ancho, int alto) {
    int y, i;
    if (dialogo_n <= 0) return;
    y = alto - 48;
    if (y < 24) return;
    rect_hud(fb, ancho, alto, 8, y, ancho - 16, 44, HUD_FONDO);
    borde_hud(fb, ancho, alto, 8, y, ancho - 16, 44, HUD_BORDE);
    for (i = 0; i < HUD_DIALOGO_LINEAS; i++) {
        u32 color = i == dialogo_actual ? HUD_RESAR : HUD_TEXTO;
        texto_hud(fb, ancho, alto, 16, y + 7 + i * 12,
                  dialogo[i], color, 1);
    }
}

static void plantilla_hud(u32 *fb, int ancho, int alto) {
    int w, h, x, y;
    if (!plantilla) return;
    w = ancho / 2;
    h = 76;
    if (w < 180) w = ancho - 16;
    x = (ancho - w) / 2;
    y = (alto - h) / 2;
    rect_hud(fb, ancho, alto, x, y, w, h, HUD_FONDO);
    borde_hud(fb, ancho, alto, x, y, w, h, HUD_BORDE);
    texto_hud(fb, ancho, alto, x + 12, y + 14,
              plantilla == 1 ? "PAUSA" : "FIN DEL JUEGO", HUD_RESAR, 2);
    texto_hud(fb, ancho, alto, x + 12, y + 48,
              "PULSA ENTER PARA SEGUIR", HUD_TEXTO, 1);
}

void motor_hud_dibujar(u32 *fb, int ancho, int alto) {
    int i;
    if (!activo || !fb) return;
    cabecera_hud(fb, ancho, alto);
    for (i = 0; i < nelementos; i++) {
        HudElemento *e = &elementos[i];
        switch (e->tipo) {
        case HUD_ELEM_TEXTO:
            texto_hud(fb, ancho, alto, e->x, e->y,
                      e->texto, e->color, e->extra);
            break;
        case HUD_ELEM_RECT:
            rect_hud(fb, ancho, alto, e->x, e->y, e->w, e->h, e->color);
            break;
        case HUD_ELEM_BARRA:
            barra_hud(fb, ancho, alto, e);
            break;
        case HUD_ELEM_IMAGEN:
            if (imagen_fn) imagen_fn(imagen_ctx, e->id, e->x, e->y);
            break;
        case HUD_ELEM_BOTON:
            boton_hud(fb, ancho, alto, e);
            break;
        default:
            break;
        }
    }
    dialogo_hud(fb, ancho, alto);
    plantilla_hud(fb, ancho, alto);
}

/* ------------------------------------------------------------------ */
/* Volcado determinista                                                */
/* ------------------------------------------------------------------ */

void motor_hud_volcar(const char *ruta) {
    FILE *f;
    int i;
    if (!ruta || !ruta[0]) return;
    f = fopen(ruta, "wt");
    if (!f) return;
    fprintf(f, "hud: activo %d\n", activo);
    fprintf(f, "hud: vidas %d\n", vidas);
    fprintf(f, "hud: puntos %d\n", puntos);
    fprintf(f, "hud: dialogo %d\n", dialogo_n);
    for (i = 0; i < HUD_DIALOGO_LINEAS; i++)
        fprintf(f, "hud: linea %d %s\n", i, dialogo[i]);
    fprintf(f, "hud: actual %d\n", dialogo_actual);
    fprintf(f, "hud: elementos %d\n", nelementos);
    for (i = 0; i < nelementos; i++) {
        HudElemento *e = &elementos[i];
        fprintf(f, "hud: elem %d %d %d %d %d %d %d %s\n",
                i, e->tipo, e->x, e->y, e->w, e->h, e->pulsado, e->texto);
    }
    fprintf(f, "hud: plantilla %d\n", plantilla);
    fclose(f);
}
