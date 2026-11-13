/* ==========================================================================
 * Pintura: lienzo, herramientas de dibujo, formas, texto y seleccion.
 * ========================================================================== */
#include "app.h"

#define MAX_LIBRE 4096

static const int GROSOR_NIVELES[10] = {1, 2, 3, 4, 5, 6, 8, 10, 12, 16};
static const int ZOOM_NIVELES[19] = {10, 25, 50, 75, 100, 125, 150, 200, 300, 400,
                                     500, 600, 800, 1000, 1250, 1500, 2000, 3000, 4000};

static u32 semilla_trazo = 12345;

static int libre_x[MAX_LIBRE];
static int libre_y[MAX_LIBRE];
static int libre_num = 0;

static u32 *respaldo_forma = NULL;   /* copia de la capa antes de una figura */
static int respaldo_w = 0, respaldo_h = 0;

static u32 *original_escala = NULL;  /* copia para redimensionar la seleccion */
static int original_escala_w = 0, original_escala_h = 0;
static int original_x0 = 0, original_y0 = 0, original_x1 = 0, original_y1 = 0;

/* Mayus real o fijado por `--mayus` en las capturas internas. */
static int mayus_activo(void) {
    return g.shift_simulado || (GetKeyState(VK_SHIFT) & 0x8000);
}

/* ------------------------------------------------------------------ */
/* Ayudas                                                              */
/* ------------------------------------------------------------------ */
static int sel_ancho(Seleccion *s) { return s->x1 - s->x0; }
static int sel_alto(Seleccion *s) { return s->y1 - s->y0; }

int pintura_herramienta_actual(void) { return (int)g.herramienta; }

static int grosor_px(void) {
    int i = g.grosor_idx;
    if (i < 0) i = 0;
    if (i > 9) i = 9;
    return GROSOR_NIVELES[i];
}

int zoom_porcentaje(int indice) {
    if (indice < 0) indice = 0;
    if (indice > 18) indice = 18;
    return ZOOM_NIVELES[indice];
}

int pintura_grosor_px(int indice) {
    if (indice < 0) indice = 0;
    if (indice > 9) indice = 9;
    return GROSOR_NIVELES[indice];
}

float zoom_escala(int porcentaje) {
    float p = (float)porcentaje / 100.0f;
    if (p >= 1.0f) return p;
    return 0.40f + (1.0f - 0.40f) * p;
}

int lado_con_zoom(int lado, int porcentaje) {
    return redondear((float)lado * zoom_escala(porcentaje));
}

/* Zona interior del area de trabajo donde cabe el lienzo con sus tiradores,
 * como `zona_lienzo`. */
static Zona zona_lienzo_app(Zona zona) {
    Zona z;
    z.x = zona.x + MANGO_TAM;
    z.y = zona.y + MANGO_TAM;
    z.ancho = zona.ancho - MANGO_TAM * 2;
    z.alto = zona.alto - MANGO_TAM * 2;
    if (z.ancho < MIN_LADO) z.ancho = MIN_LADO;
    if (z.alto < MIN_LADO) z.alto = MIN_LADO;
    return z;
}

/* Lienzo en pantalla para un tamano logico y un nivel de zoom, centrado en el
 * area de trabajo (`lienzo_desde_logico`). */
static Lienzo lienzo_desde_logico(int ancho, int alto, int indice, Zona zona) {
    Zona util = zona_lienzo_app(zona);
    int porcentaje = zoom_porcentaje(indice);
    int w = lado_con_zoom(ancho, porcentaje);
    int h = lado_con_zoom(alto, porcentaje);
    Lienzo l;
    if (w < MIN_LADO) w = MIN_LADO;
    if (h < MIN_LADO) h = MIN_LADO;
    l.x0 = util.x + (util.ancho - w) / 2;
    l.y0 = util.y + (util.alto - h) / 2;
    l.x1 = l.x0 + w;
    l.y1 = l.y0 + h;
    return l;
}

/* Tamano logico que corresponde a un lienzo ya dibujado (`sincronizar_logico`). */
static void sincronizar_logico(Lienzo l, int indice, int *ancho, int *alto) {
    float factor = zoom_escala(zoom_porcentaje(indice));
    int w = redondear((float)(l.x1 - l.x0) / factor);
    int h = redondear((float)(l.y1 - l.y0) / factor);
    if (w < MIN_LADO) w = MIN_LADO;
    if (h < MIN_LADO) h = MIN_LADO;
    *ancho = w;
    *alto = h;
}

/* Centra un lienzo conservando su tamano (`centrado_en`). */
static Lienzo lienzo_centrado_en(Lienzo l, Zona zona) {
    Zona util = zona_lienzo_app(zona);
    int w = l.x1 - l.x0;
    int h = l.y1 - l.y0;
    if (w < MIN_LADO) w = MIN_LADO;
    if (h < MIN_LADO) h = MIN_LADO;
    l.x0 = util.x + (util.ancho - w) / 2;
    l.y0 = util.y + (util.alto - h) / 2;
    l.x1 = l.x0 + w;
    l.y1 = l.y0 + h;
    return l;
}

/* Indice de zoom que hace caber un documento dentro de una zona. `maximo`
 * limita el nivel mas alto que se puede elegir (4 = 100% para no ampliar los
 * documentos pequenos; 18 = 4000% para aprovechar todo el hueco). */
static int zoom_para_caber_en(Zona zona, int ancho, int alto, int maximo) {
    Zona util = zona;
    int i;
    util.x += MANGO_TAM;
    util.y += MANGO_TAM;
    util.ancho = (util.ancho - MANGO_TAM * 2) > MIN_LADO ? util.ancho - MANGO_TAM * 2 : MIN_LADO;
    util.alto = (util.alto - MANGO_TAM * 2) > MIN_LADO ? util.alto - MANGO_TAM * 2 : MIN_LADO;
    for (i = maximo; i >= 0; i--) {
        int p = ZOOM_NIVELES[i];
        if (lado_con_zoom(ancho, p) <= util.ancho && lado_con_zoom(alto, p) <= util.alto) return i;
    }
    return 0;
}

int zoom_para_caber(int ancho, int alto) {
    /* El zoom persistente del documento siempre se calcula sobre el area del
     * modo pintar, nunca sobre el hueco reducido de otra vista, y no amplia
     * por encima del 100%. */
    return zoom_para_caber_en(zona_trabajo_pintar(), ancho, alto, 4);
}

void lienzo_actualizar(void) {
    int indice = g.zoom_idx;
    /* En modo tiles el lienzo es la celda activa y su tamano depende del
     * layout de toda la cuadricula. Recalcularlo aqui evita que el zoom, el
     * redimensionado o un cambio de documento dejen `g.lienzo` con la
     * geometria del modo pintar. */
    if (g.modo_tiles) {
        tiles_modo_layout();
        return;
    }
    /* En el editor de menus el lienzo se ajusta al hueco entre los paneles
     * sin modificar el zoom guardado del modo pintar. */
    if (g.vista == VISTA_MENUS)
        indice = zoom_para_caber_en(zona_trabajo(), g.doc.ancho, g.doc.alto, 18);
    /* Igual que `lienzo_desde_logico`: el lienzo se recoloca siempre centrado
     * en el area de trabajo. El zoom no guarda desplazamientos manuales. */
    g.lienzo = lienzo_desde_logico(g.doc.ancho, g.doc.alto, indice, zona_trabajo());
}

void zoom_aplicar(int indice) {
    if (indice < 0) indice = 0;
    if (indice > 18) indice = 18;
    g.zoom_idx = indice;
    lienzo_actualizar();
    app_invalidar();
}

int lienzo_contiene(int x, int y) {
    return x >= g.lienzo.x0 && x < g.lienzo.x1 && y >= g.lienzo.y0 && y < g.lienzo.y1;
}

/* Division entera con techo, como `division_techo`. */
static int division_techo(long long valor, long long divisor) {
    if (divisor <= 0) return 0;
    return (int)((valor + divisor - 1) / divisor);
}

/* Pixel logico bajo una posicion de pantalla. Usa la misma division entera
 * que `pixel_logico`: cada pixel visible toma la columna
 * `(x - x0) * ancho / ancho_lienzo`, recortada al ultimo pixel. */
static int doc_desde_pantalla(int x, int y, int *dx, int *dy) {
    int cw = g.lienzo.x1 - g.lienzo.x0;
    int ch = g.lienzo.y1 - g.lienzo.y0;
    if (cw <= 0 || ch <= 0 || g.doc.ancho <= 0 || g.doc.alto <= 0) return 0;
    if (!lienzo_contiene(x, y)) return 0;
    *dx = (int)(((long long)(x - g.lienzo.x0) * g.doc.ancho) / cw);
    *dy = (int)(((long long)(y - g.lienzo.y0) * g.doc.alto) / ch);
    if (*dx >= g.doc.ancho) *dx = g.doc.ancho - 1;
    if (*dy >= g.doc.alto) *dy = g.doc.alto - 1;
    return *dx >= 0 && *dy >= 0;
}

int pintura_punto_documento(int x, int y, int *dx, int *dy) {
    return doc_desde_pantalla(x, y, dx, dy);
}

/* Borde superior izquierdo de un rectangulo logico inclusivo, como
 * `rectangulo_pantalla`: `dx * ancho_lienzo / ancho_dibujo` redondeado hacia
 * arriba. */
static void pantalla_desde_doc(int dx, int dy, int *sx, int *sy) {
    int cw = g.lienzo.x1 - g.lienzo.x0;
    int ch = g.lienzo.y1 - g.lienzo.y0;
    int aw = g.doc.ancho > 0 ? g.doc.ancho : 1;
    int ah = g.doc.alto > 0 ? g.doc.alto : 1;
    *sx = g.lienzo.x0 + division_techo((long long)dx * cw, aw);
    *sy = g.lienzo.y0 + division_techo((long long)dy * ch, ah);
}

/* Borde inferior derecho exclusivo: se usa con las coordenadas ya exclusivas
 * del rectangulo, igual que `(x1 + 1) * ancho_lienzo / ancho_dibujo`. */
static void pantalla_desde_doc_fin(int dx, int dy, int *sx, int *sy) {
    int cw = g.lienzo.x1 - g.lienzo.x0;
    int ch = g.lienzo.y1 - g.lienzo.y0;
    int aw = g.doc.ancho > 0 ? g.doc.ancho : 1;
    int ah = g.doc.alto > 0 ? g.doc.alto : 1;
    *sx = g.lienzo.x0 + division_techo((long long)dx * cw, aw);
    *sy = g.lienzo.y0 + division_techo((long long)dy * ch, ah);
}

static Capa *capa_activa(void) {
    if (g.doc.activa < 0 || g.doc.activa >= g.doc.num_capas) return NULL;
    return &g.doc.capas[g.doc.activa];
}

static u32 color_de_boton(int boton) {
    if (g.herramienta == H_BORRADOR) return g.fondo;
    return boton == 0 ? g.frente : g.fondo;
}

static int alfa_opacidad(int alfa) {
    return limitar(alfa * g.opacidad / 100, 0, 255);
}

static void marcar_cambio(void) {
    g.doc.compuesto_sucio = 1;
    g.doc.modificado = 1;
}

static void componer_zona(int x0, int y0, int x1, int y1) {
    doc_componer_region(&g.doc, x0 - 2, y0 - 2, x1 + 2, y1 + 2);
}

/* ------------------------------------------------------------------ */
/* Pincel: cobertura y textura                                         */
/* ------------------------------------------------------------------ */
static float cobertura_pincel(int x, int y, int cx, int cy, int radio, int textura) {
    float dx = (float)(x - cx) + 0.5f;
    float dy = (float)(y - cy) + 0.5f;
    float d = sqrtf(dx * dx + dy * dy);
    float r = (float)radio;
    float cob;
    unsigned int h;
    if (r < 0.75f) r = 0.75f;
    if (d > r + 1.0f) return 0.0f;
    cob = 1.0f - (d - (r - 1.0f));
    if (cob > 1.0f) cob = 1.0f;
    if (cob < 0.0f) cob = 0.0f;

    /* Dureza: cuanto mas baja, mas se difumina el borde. */
    if (g.dureza < 100) {
        float borde = 1.0f - d / r;
        float inicio = (float)g.dureza / 100.0f;
        float suave;
        if (r <= 1.0f) {
            suave = 1.0f;
        } else if (borde < inicio) {
            suave = 0.0f;
        } else {
            suave = (borde - inicio) / (1.0f - inicio + 0.0001f);
        }
        if (suave > 1.0f) suave = 1.0f;
        cob *= suave;
    }

    h = hash32((unsigned int)(x * 73856093) ^ (unsigned int)(y * 19349663) ^ semilla_trazo);
    switch (textura) {
        case TEXTURA_CERDA: {
            float surco = 0.55f + 0.45f * (float)((h >> 9) & 255) / 255.0f;
            float ang = (float)g.angulo * 3.14159265f / 180.0f;
            float proy = (float)x * cosf(ang) + (float)y * sinf(ang);
            surco *= 0.85f + 0.15f * sinf(proy * 0.9f);
            cob *= surco;
            break;
        }
        case TEXTURA_ACUARELA:
            cob *= 0.35f + 0.5f * (float)((h >> 8) & 255) / 255.0f;
            break;
        case TEXTURA_TINTA:
            break;
        case TEXTURA_SECO:
            cob *= ((h >> 8) & 255) > 90 ? 0.95f : 0.12f;
            break;
        case TEXTURA_AEROGRAFO:
            cob *= 0.10f + 0.28f * (float)((h >> 8) & 255) / 255.0f;
            break;
        default:
            break;
    }
    if (cob > 1.0f) cob = 1.0f;
    return cob;
}

static void capa_pintar(int x, int y, u32 color, int alfa) {
    Capa *c = capa_activa();
    size_t p;
    if (!c || alfa <= 0) return;
    if (x < 0 || y < 0 || x >= g.doc.ancho || y >= g.doc.alto) return;
    if (g.sel.activa && !g.sel.mover && !seleccion_contiene(&g.sel, x, y)) return;
    p = (size_t)y * g.doc.ancho + x;
    if (alfa > 255) alfa = 255;
    c->pixeles[p] = color_mezclar(c->pixeles[p], 0xFF000000u | (color & 0x00FFFFFFu), alfa);
}

static void sellar_pincel(int cx, int cy, int color, int boton) {
    if (g.herramienta == H_LAPIZ) {
        int grosor = grosor_px();
        if (grosor < 1) grosor = 1;
        int inicio_x = cx - (grosor - 1) / 2;
        int inicio_y = cy - (grosor - 1) / 2;
        float centro_x = (float)inicio_x + (float)grosor / 2.0f;
        float centro_y = (float)inicio_y + (float)grosor / 2.0f;
        float radio = (float)grosor / 2.0f;
        int py, px;
        for (py = inicio_y; py < inicio_y + grosor; py++) {
            if (py < 0 || py >= g.doc.alto) continue;
            for (px = inicio_x; px < inicio_x + grosor; px++) {
                if (px < 0 || px >= g.doc.ancho) continue;
                float ddx = (float)px + 0.5f - centro_x;
                float ddy = (float)py + 0.5f - centro_y;
                float dist = sqrtf(ddx * ddx + ddy * ddy);
                float normal = (radio > 0.0f) ? (dist / radio) : 0.0f;
                if (normal > 1.0f) normal = 1.0f;
                if (normal < 0.0f) normal = 0.0f;
                float dur = (float)g.dureza / 100.0f;
                float caida;
                if (dur >= 0.999f || normal <= dur) caida = 1.0f;
                else {
                    float denom = (1.0f - dur);
                    if (denom < 0.001f) denom = 0.001f;
                    caida = (1.0f - normal) / denom;
                }
                float op = (float)g.opacidad / 100.0f;
                float alfa = op * caida;
                if (alfa > 1.0f) alfa = 1.0f;
                if (alfa < 0.0f) alfa = 0.0f;
                int a = (int)(alfa * 255.0f);
                capa_pintar(px, py, color, a);
            }
        }
        componer_zona(inicio_x, inicio_y, inicio_x + grosor - 1, inicio_y + grosor - 1);
        return;
    }
    int radio = grosor_px();
    int r = (radio + 1) / 2;
    int x, y;
    int alfa_base = 255;
    int textura = g.textura;
    if (radio <= 1) r = 1;
    if (r < 1) r = 1;
    if (g.herramienta == H_LAPIZ || g.herramienta == H_BORRADOR) textura = TEXTURA_TINTA;
    if (g.herramienta == H_AEROGRAFO) textura = TEXTURA_AEROGRAFO;
    alfa_base = limitar(255 * (100 - (g.espaciado / 3)) / 100 + 40, 40, 255);
    (void)alfa_base;
    for (y = cy - r - 1; y <= cy + r + 1; y++) {
        for (x = cx - r - 1; x <= cx + r + 1; x++) {
            float cob = cobertura_pincel(x, y, cx, cy, r, textura);
            int a;
            if (cob <= 0.01f) continue;
            a = (int)(cob * 255.0f);
            a = alfa_opacidad(a);
            capa_pintar(x, y, color, a);
        }
    }
    componer_zona(cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1);
}

static void sellar_aerografo(int cx, int cy, int color) {
    int radio = grosor_px() * 2;
    int gotas = 4 + g.grosor_idx * 2;
    int i;
    int ligero = 35 + g.opacidad * 2 / 3;
    radio = limitar(radio + (int)g.rocio * 3, 2, 60);
    for (i = 0; i < gotas; i++) {
        float ang = rnd01(&semilla_trazo) * 6.2831853f;
        float dist = sqrtf(rnd01(&semilla_trazo)) * (float)radio;
        int px = cx + (int)(cosf(ang) * dist);
        int py = cy + (int)(sinf(ang) * dist);
        int lado = (g.rocio == ROCIO_GRUESA || g.rocio == ROCIO_GOTEO) ? 2 : 1;
        int x, y;
        for (y = 0; y < lado; y++) {
            for (x = 0; x < lado; x++) {
                int a = alfa_opacidad(ligero);
                capa_pintar(px + x, py + y, color, a);
            }
        }
    }
    componer_zona(cx - radio - 2, cy - radio - 2, cx + radio + 2, cy + radio + 2);
}

static void trazo_hasta(int x, int y, int boton) {
    int color = color_de_boton(boton);
    int x0 = g.ult_x, y0 = g.ult_y;
    float dx = (float)(x - x0);
    float dy = (float)(y - y0);
    float dist = sqrtf(dx * dx + dy * dy);
    float paso = (float)grosor_px() * (float)g.espaciado / 100.0f;
    float i;
    if (paso < 1.0f) paso = 1.0f;
    if (dist <= 0.0f) {
        if (g.herramienta == H_AEROGRAFO) sellar_aerografo(x, y, color);
        else sellar_pincel(x, y, color, boton);
        g.ult_x = x; g.ult_y = y;
        return;
    }
    for (i = paso; i <= dist; i += paso) {
        int px = x0 + (int)(dx * i / dist);
        int py = y0 + (int)(dy * i / dist);
        if (g.herramienta == H_AEROGRAFO) sellar_aerografo(px, py, color);
        else sellar_pincel(px, py, color, boton);
    }
    g.ult_x = x;
    g.ult_y = y;
}

void pintura_trazo_punto(int x, int y, int boton, int primero) {
    int color = color_de_boton(boton);
    if (primero) {
        g.ult_x = x;
        g.ult_y = y;
        if (g.herramienta == H_AEROGRAFO) sellar_aerografo(x, y, color);
        else sellar_pincel(x, y, color, boton);
    } else {
        trazo_hasta(x, y, boton);
    }
}

void pintura_trazo_linea(int x0, int y0, int x1, int y1, int boton) {
    g.ult_x = x0;
    g.ult_y = y0;
    trazo_hasta(x1, y1, boton);
}

/* ------------------------------------------------------------------ */
/* Relleno (cubo)                                                      */
/* ------------------------------------------------------------------ */
/* Diferencia maxima entre los canales RGB (el alfa no cuenta), como
 * `diferencia_color`. */
static int diferencia_rgb(u32 a, u32 b) {
    int dr = (int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF);
    int dg = (int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF);
    int db = (int)(a & 0xFF) - (int)(b & 0xFF);
    if (dr < 0) dr = -dr;
    if (dg < 0) dg = -dg;
    if (db < 0) db = -db;
    if (dr < dg) dr = dg;
    if (dr < db) dr = db;
    return dr;
}

static float luminancia_color(u32 c) {
    return 0.299f * (float)((c >> 16) & 0xFF) +
           0.587f * (float)((c >> 8) & 0xFF) +
           0.114f * (float)(c & 0xFF);
}

/* Si el pixel pertenece a una linea del dibujo. Sin "compuesto" solo cuenta
 * la tinta oscura; con el, tambien los bordes de fuerte contraste. */
static int es_borde_linea(const u32 *muestra, int ancho, int alto,
                          int x, int y, int compuesto) {
    static const int VX[4] = {-1, 1, 0, 0};
    static const int VY[4] = {0, 0, -1, 1};
    u32 color = muestra[(size_t)y * ancho + x];
    int i;
    if (luminancia_color(color) < 96.0f) return 1;
    if (!compuesto) return 0;
    for (i = 0; i < 4; i++) {
        int nx = x + VX[i], ny = y + VY[i];
        if (nx < 0 || ny < 0 || nx >= ancho || ny >= alto) continue;
        if (diferencia_rgb(color, muestra[(size_t)ny * ancho + nx]) >= 40)
            return 1;
    }
    return 0;
}

/* Patron de 8x8 con forma de pino. Cada bit a 1 usa el color de primer
 * plano y cada 0 el de fondo. */
static int patron_pino(int x, int y) {
    static const unsigned char PATRON_PINO[8] = {
        0x18, 0x3C, 0x7E, 0x18, 0x3C, 0x7E, 0xFF, 0x18
    };
    int rx = x % 8, ry = y % 8;
    if (rx < 0) rx += 8;
    if (ry < 0) ry += 8;
    return (PATRON_PINO[ry] >> (7 - rx)) & 1;
}

/* Mezcla lineal de dos colores RGB, con redondeo como `mezclar_color`. */
static u32 mezclar_cubo(u32 a, u32 b, float t) {
    int r, gg, bl;
    if (t <= 0.0f) return 0xFF000000u | (a & 0x00FFFFFFu);
    if (t >= 1.0f) return 0xFF000000u | (b & 0x00FFFFFFu);
    r = (int)((float)((a >> 16) & 0xFF) +
              ((float)((b >> 16) & 0xFF) - (float)((a >> 16) & 0xFF)) * t + 0.5f);
    gg = (int)((float)((a >> 8) & 0xFF) +
               ((float)((b >> 8) & 0xFF) - (float)((a >> 8) & 0xFF)) * t + 0.5f);
    bl = (int)((float)(a & 0xFF) +
               ((float)(b & 0xFF) - (float)(a & 0xFF)) * t + 0.5f);
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (gg < 0) gg = 0;
    if (gg > 255) gg = 255;
    if (bl < 0) bl = 0;
    if (bl > 255) bl = 255;
    return 0xFF000000u | ((u32)r << 16) | ((u32)gg << 8) | (u32)bl;
}

/* Nivel de opacidad del cubo (0..100) para un indice del panel. */
static int nivel_opacidad_cubo(int indice) {
    static const int NIVELES[12] = {
        0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 100
    };
    if (indice < 0) indice = 0;
    if (indice > 11) indice = 11;
    return NIVELES[indice];
}

/* Nivel de umbral del cubo (0..100) para un indice del panel. */
static int nivel_umbral_cubo(int indice) {
    if (indice < 0) indice = 0;
    if (indice > 20) indice = 20;
    return indice * 5;
}

/* Relleno con las opciones del cubo. La zona se calcula sobre la imagen
 * visible (incluida la seleccion flotante) y el color se aplica al bloque
 * flotante si tapa el pixel o a la capa activa si no. */
static void rellenar_zona(int x, int y, int boton) {
    Capa *c = capa_activa();
    int w, h, total, click, i;
    u32 *muestra = NULL;
    u8 *cubierto = NULL, *hueco = NULL, *permitido = NULL;
    u8 *objetivo = NULL, *visitado = NULL, *borde = NULL;
    float *factor = NULL;
    int *pila = NULL;
    int tope = 0;
    int tipo, area, combinada, transparentes, diagonales, alisado, compuesto;
    int fw = 0, fh = 0, fx = 0, fy = 0, hay_flotante;
    u32 color_frente, color_fondo;
    float alfa_base, tolerancia;
    int listo = 1;

    if (!c) return;
    w = g.doc.ancho;
    h = g.doc.alto;
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    total = w * h;
    click = y * w + x;

    /* Alt y el boton derecho cambian al color de fondo; Mayusculas obliga a
     * pintar toda la seleccion, como `opciones_cubo_clic`. */
    tipo = g.cubo_tipo;
    area = g.cubo_area;
    if (boton == 1 || (GetKeyState(VK_MENU) & 0x8000)) tipo = CUBO_TIPO_FD;
    if (mayus_activo()) area = CUBO_AREA_SELECCION;
    combinada = g.cubo_combinada;
    transparentes = g.cubo_transparentes;
    diagonales = g.cubo_diagonales;
    alisado = g.cubo_alisado;
    compuesto = g.cubo_compuesto;
    color_frente = g.frente & 0x00FFFFFFu;
    color_fondo = g.fondo & 0x00FFFFFFu;

    pila_guardar(&g.deshacer, &g.doc);
    pila_limpiar(&g.rehacer);

    muestra = (u32 *)malloc((size_t)total * sizeof(u32));
    cubierto = (u8 *)calloc((size_t)total, 1);
    hueco = (u8 *)calloc((size_t)total, 1);
    permitido = (u8 *)malloc((size_t)total);
    objetivo = (u8 *)calloc((size_t)total, 1);
    visitado = (u8 *)calloc((size_t)total, 1);
    borde = (u8 *)calloc((size_t)total, 1);
    factor = (float *)calloc((size_t)total, sizeof(float));
    pila = (int *)malloc((size_t)total * sizeof(int));
    if (!muestra || !cubierto || !hueco || !permitido || !objetivo ||
        !visitado || !borde || !factor || !pila) {
        listo = 0;
    }

    if (listo) {
        memcpy(muestra, c->pixeles, (size_t)total * sizeof(u32));

        /* Imagen visible: el lienzo con el bloque flotante encima. Sin
         * "muestra combinada" se analiza solo el lienzo de fondo. */
        hay_flotante = g.sel.flotante != NULL;
        if (hay_flotante) {
            int ly, lx;
            fw = sel_ancho(&g.sel);
            fh = sel_alto(&g.sel);
            fx = g.sel.x0;
            fy = g.sel.y0;
            for (ly = 0; ly < fh; ly++) {
                int gy = fy + ly;
                if (gy < 0 || gy >= h) continue;
                for (lx = 0; lx < fw; lx++) {
                    int gx = fx + lx;
                    int local, indice, visible;
                    if (gx < 0 || gx >= w) continue;
                    local = ly * fw + lx;
                    visible = !g.sel.mascara || g.sel.mascara[local];
                    indice = gy * w + gx;
                    if (visible) {
                        cubierto[indice] = 1;
                        if (combinada)
                            muestra[indice] = g.sel.flotante[local] & 0x00FFFFFFu;
                    } else {
                        hueco[indice] = 1;
                    }
                }
            }
        }

        /* La seleccion activa limita el relleno. Los huecos de una seleccion
         * libre solo se pintan con "rellenar areas transparentes". */
        for (i = 0; i < total; i++) permitido[i] = 1;
        if (g.sel.activa) {
            int gx, gy;
            memset(permitido, 0, (size_t)total);
            for (gy = g.sel.y0; gy < g.sel.y1; gy++) {
                if (gy < 0 || gy >= h) continue;
                for (gx = g.sel.x0; gx < g.sel.x1; gx++) {
                    int indice;
                    if (gx < 0 || gx >= w) continue;
                    indice = gy * w + gx;
                    if (seleccion_contiene(&g.sel, gx, gy) ||
                        (transparentes && hueco[indice]))
                        permitido[indice] = 1;
                }
            }
        }
        if (!transparentes) {
            for (i = 0; i < total; i++)
                if (hueco[i]) permitido[i] = 0;
        }

        if (area == CUBO_AREA_SELECCION) {
            memcpy(objetivo, permitido, (size_t)total);
        } else if (permitido[click]) {
            int arte = area == CUBO_AREA_ARTE;
            u32 color_objetivo = muestra[click];
            int nvec = diagonales ? 8 : 4;
            static const int VX[8] = {-1, 1, 0, 0, -1, 1, -1, 1};
            static const int VY[8] = {0, 0, -1, 1, -1, -1, 1, 1};
            tolerancia = (float)nivel_umbral_cubo(g.cubo_umbral) * 255.0f / 100.0f;

            if (!(arte && es_borde_linea(muestra, w, h, x, y, compuesto))) {
                pila[tope++] = click;
                visitado[click] = 1;
                while (tope > 0) {
                    int p = pila[--tope];
                    int px = p % w, py = p / w;
                    int k;
                    objetivo[p] = 1;
                    for (k = 0; k < nvec; k++) {
                        int nx = px + VX[k], ny = py + VY[k];
                        int indice, acepta;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                        indice = ny * w + nx;
                        if (visitado[indice] || !permitido[indice]) continue;
                        if (arte)
                            acepta = !es_borde_linea(muestra, w, h, nx, ny, compuesto);
                        else
                            acepta = (float)diferencia_rgb(muestra[indice],
                                                           color_objetivo) <= tolerancia;
                        if (acepta) {
                            visitado[indice] = 1;
                            pila[tope++] = indice;
                        }
                    }
                }
            }
        }

        /* Factor de opacidad por pixel. Con alisado, el borde del relleno se
         * pinta a medias y el pixel de fuera recibe un velo suave. */
        for (i = 0; i < total; i++)
            if (objetivo[i]) factor[i] = 1.0f;
        if (alisado) {
            int py, px;
            for (py = 0; py < h; py++) {
                for (px = 0; px < w; px++) {
                    int indice = py * w + px;
                    int en_borde;
                    if (!objetivo[indice]) continue;
                    en_borde = 0;
                    {
                        int k;
                        static const int VX[4] = {-1, 1, 0, 0};
                        static const int VY[4] = {0, 0, -1, 1};
                        for (k = 0; k < 4; k++) {
                            int nx = px + VX[k], ny = py + VY[k];
                            if (nx < 0 || ny < 0 || nx >= w || ny >= h ||
                                !objetivo[ny * w + nx]) {
                                en_borde = 1;
                                break;
                            }
                        }
                    }
                    if (en_borde) {
                        borde[indice] = 1;
                        factor[indice] = 0.6f;
                    }
                }
            }
            for (py = 0; py < h; py++) {
                for (px = 0; px < w; px++) {
                    static const int VX[4] = {-1, 1, 0, 0};
                    static const int VY[4] = {0, 0, -1, 1};
                    int k;
                    if (!borde[py * w + px]) continue;
                    for (k = 0; k < 4; k++) {
                        int nx = px + VX[k], ny = py + VY[k];
                        int indice;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                        indice = ny * w + nx;
                        if (permitido[indice] && !objetivo[indice] &&
                            factor[indice] < 0.3f)
                            factor[indice] = 0.3f;
                    }
                }
            }
        }

        alfa_base = (float)nivel_opacidad_cubo(g.cubo_opacidad) / 100.0f;
        if (alfa_base > 0.0f) {
            int py, px;
            int minx = w, miny = h, maxx = -1, maxy = -1;
            int sucio_antes = g.doc.compuesto_sucio;
            for (py = 0; py < h; py++) {
                for (px = 0; px < w; px++) {
                    int indice = py * w + px;
                    float sombra = factor[indice];
                    float alfa;
                    u32 color;
                    if (sombra <= 0.0f) continue;
                    if (tipo == CUBO_TIPO_FD) {
                        color = color_fondo;
                    } else if (tipo == CUBO_TIPO_PATRON) {
                        color = patron_pino(px, py) ? color_frente : color_fondo;
                    } else {
                        color = color_frente;
                    }
                    alfa = alfa_base * sombra;
                    if (alfa > 1.0f) alfa = 1.0f;
                    if (cubierto[indice]) {
                        int lx = px - fx, ly = py - fy;
                        if (hay_flotante && lx >= 0 && ly >= 0 &&
                            lx < fw && ly < fh) {
                            int local = ly * fw + lx;
                            g.sel.flotante[local] =
                                mezclar_cubo(g.sel.flotante[local], color, alfa);
                        }
                    } else {
                        c->pixeles[indice] = mezclar_cubo(c->pixeles[indice],
                                                          color, alfa);
                    }
                    if (px < minx) minx = px;
                    if (px > maxx) maxx = px;
                    if (py < miny) miny = py;
                    if (py > maxy) maxy = py;
                }
            }
            marcar_cambio();
            /* Solo la zona rellenada cambia: recomponer todo el lienzo hacia
             * que el cubo pareciera lento en imagenes grandes. */
            if (maxx >= minx && maxy >= miny) {
                doc_componer_region(&g.doc, minx, miny, maxx + 1, maxy + 1);
                g.doc.compuesto_sucio = sucio_antes;
            }
        }
    }

    free(muestra);
    free(cubierto);
    free(hueco);
    free(permitido);
    free(objetivo);
    free(visitado);
    free(borde);
    free(factor);
    free(pila);
}

void pintura_rellenar(int x, int y, int boton) {
    rellenar_zona(x, y, boton);
}

/* ------------------------------------------------------------------ */
/* Formas                                                              */
/* ------------------------------------------------------------------ */
static int respaldo_iniciar(void) {
    Capa *c = capa_activa();
    if (!c) return 0;
    if (respaldo_forma) free(respaldo_forma);
    respaldo_forma = (u32 *)malloc((size_t)g.doc.ancho * g.doc.alto * sizeof(u32));
    if (!respaldo_forma) return 0;
    copiar_pixeles(respaldo_forma, c->pixeles, g.doc.ancho * g.doc.alto);
    respaldo_w = g.doc.ancho;
    respaldo_h = g.doc.alto;
    return 1;
}

static void respaldo_restaurar(void) {
    Capa *c = capa_activa();
    if (!c || !respaldo_forma) return;
    if (respaldo_w != g.doc.ancho || respaldo_h != g.doc.alto) return;
    copiar_pixeles(c->pixeles, respaldo_forma, g.doc.ancho * g.doc.alto);
}

/* ------------------------------------------------------------------ */
/* Curva de Bezier (herramienta Curve)                                 */
/* ------------------------------------------------------------------ */
/* La punta de la curva es la misma que la del lapiz: un cuadrado
 * `grosor` x `grosor` centrado en el pixel logico, con el alfa de
 * `alfa_punta` (opacidad y dureza). Las formas usan el mismo trazado que
 * `Dibujo::pintar`. */
static void pintar_punto_cuadrado(int cx, int cy, u32 color) {
    int grosor = grosor_px();
    int inicio_x, inicio_y, py, px;
    float centro_x, centro_y, radio, op, dur;
    if (grosor < 1) grosor = 1;
    inicio_x = cx - (grosor - 1) / 2;
    inicio_y = cy - (grosor - 1) / 2;
    centro_x = (float)inicio_x + (float)grosor / 2.0f;
    centro_y = (float)inicio_y + (float)grosor / 2.0f;
    radio = (float)grosor / 2.0f;
    op = (float)g.opacidad / 100.0f;
    dur = (float)g.dureza / 100.0f;
    for (py = inicio_y; py < inicio_y + grosor; py++) {
        if (py < 0 || py >= g.doc.alto) continue;
        for (px = inicio_x; px < inicio_x + grosor; px++) {
            float ddx, ddy, dist, normal, caida, alfa;
            int a;
            if (px < 0 || px >= g.doc.ancho) continue;
            ddx = (float)px + 0.5f - centro_x;
            ddy = (float)py + 0.5f - centro_y;
            dist = sqrtf(ddx * ddx + ddy * ddy);
            normal = (radio > 0.0f) ? (dist / radio) : 0.0f;
            if (normal > 1.0f) normal = 1.0f;
            if (normal < 0.0f) normal = 0.0f;
            if (dur >= 0.999f || normal <= dur) {
                caida = 1.0f;
            } else {
                float denom = 1.0f - dur;
                if (denom < 0.001f) denom = 0.001f;
                caida = (1.0f - normal) / denom;
            }
            alfa = op * caida;
            if (alfa > 1.0f) alfa = 1.0f;
            if (alfa < 0.0f) alfa = 0.0f;
            a = (int)(alfa * 255.0f);
            capa_pintar(px, py, color, a);
        }
    }
}

/* Bresenham con la estampa cuadrada, como `recorrer_linea`. */
static void pintar_linea_cuadrada(int x0, int y0, int x1, int y1, u32 color) {
    int x = x0, y = y0;
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        int e2;
        pintar_punto_cuadrado(x, y, color);
        if (x == x1 && y == y1) break;
        e2 = err * 2;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

static void punto_bezier(int x0, int y0, int cx1, int cy1,
                         int cx2, int cy2, int x1, int y1,
                         float t, int *px, int *py) {
    float mt = 1.0f - t;
    float a = mt * mt * mt;
    float b = 3.0f * mt * mt * t;
    float c = 3.0f * mt * t * t;
    float d = t * t * t;
    *px = redondear(a * (float)x0 + b * (float)cx1 + c * (float)cx2 + d * (float)x1);
    *py = redondear(a * (float)y0 + b * (float)cy1 + c * (float)cy2 + d * (float)y1);
}

/* Muestras de la cubica unidas con segmentos, como `pintar_curva`: mas
 * pasos cuanto mas larga, con un minimo para que las curvas cortas sean
 * suaves y un maximo para no pasarse. */
static void pintar_curva_bezier(int x0, int y0, int cx1, int cy1,
                                int cx2, int cy2, int x1, int y1,
                                u32 color) {
    float largo = 0.0f;
    float dx, dy;
    int pasos, paso;
    int ax = x0, ay = y0;
    dx = (float)(cx1 - x0); dy = (float)(cy1 - y0);
    largo += sqrtf(dx * dx + dy * dy);
    dx = (float)(cx2 - cx1); dy = (float)(cy2 - cy1);
    largo += sqrtf(dx * dx + dy * dy);
    dx = (float)(x1 - cx2); dy = (float)(y1 - cy2);
    largo += sqrtf(dx * dx + dy * dy);
    pasos = (int)ceilf(largo / 2.0f);
    if (pasos < 24) pasos = 24;
    if (pasos > 512) pasos = 512;
    for (paso = 1; paso <= pasos; paso++) {
        float t = (float)paso / (float)pasos;
        int bx, by;
        punto_bezier(x0, y0, cx1, cy1, cx2, cy2, x1, y1, t, &bx, &by);
        pintar_linea_cuadrada(ax, ay, bx, by, color);
        ax = bx; ay = by;
    }
}

/* Curva cuadratica convertida en los dos controles de la cubica
 * equivalente, con el mismo redondeo que `cubica_desde_cuadratica`. */
static void cubica_desde_cuadratica(int ix, int iy, int cx, int cy,
                                    int fx, int fy,
                                    int *c1x, int *c1y, int *c2x, int *c2y) {
    *c1x = redondear((float)ix + (2.0f / 3.0f) * (float)(cx - ix));
    *c1y = redondear((float)iy + (2.0f / 3.0f) * (float)(cy - iy));
    *c2x = redondear((float)fx + (2.0f / 3.0f) * (float)(cx - fx));
    *c2y = redondear((float)fy + (2.0f / 3.0f) * (float)(cy - fy));
}

static int curva_completa(void) {
    return g.curva_control2;
}

static void curva_mover(int x, int y) {
    if (g.curva_control2) {
        g.curva_c2_x = x; g.curva_c2_y = y;
    } else if (g.curva_control1) {
        g.curva_c1_x = x; g.curva_c1_y = y;
    } else {
        g.curva_fin_x = x; g.curva_fin_y = y;
    }
}

static void curva_ajustar(int x, int y) {
    if (!g.curva_control1) {
        g.curva_control1 = 1;
        g.curva_c1_x = x; g.curva_c1_y = y;
    } else {
        g.curva_control2 = 1;
        g.curva_c2_x = x; g.curva_c2_y = y;
    }
}

static void curva_estampar(u32 color) {
    if (!g.curva_control1) {
        pintar_linea_cuadrada(g.curva_ini_x, g.curva_ini_y,
                              g.curva_fin_x, g.curva_fin_y, color);
    } else if (!g.curva_control2) {
        int c1x, c1y, c2x, c2y;
        cubica_desde_cuadratica(g.curva_ini_x, g.curva_ini_y,
                                g.curva_c1_x, g.curva_c1_y,
                                g.curva_fin_x, g.curva_fin_y,
                                &c1x, &c1y, &c2x, &c2y);
        pintar_curva_bezier(g.curva_ini_x, g.curva_ini_y, c1x, c1y,
                            c2x, c2y, g.curva_fin_x, g.curva_fin_y, color);
    } else {
        pintar_curva_bezier(g.curva_ini_x, g.curva_ini_y,
                            g.curva_c1_x, g.curva_c1_y,
                            g.curva_c2_x, g.curva_c2_y,
                            g.curva_fin_x, g.curva_fin_y, color);
    }
}

/* Copia limpia del lienzo con la curva provisional, sin confirmarla. */
static void curva_preview(int boton) {
    respaldo_restaurar();
    curva_estampar(color_de_boton(boton));
    doc_componer_todo(&g.doc);
}

/* Puntos que recorre la curva con su estado actual (extremos y muestra de
 * la bezier) y caja que ocupa con el grosor, recortada al lienzo. */
static int curva_limites(int *ox0, int *oy0, int *ox1, int *oy1) {
    int x0, y0, x1, y1, margen, i;
    int c1x = 0, c1y = 0, c2x = 0, c2y = 0;
    int tiene_controles = 0;
    x0 = y0 = 0x7FFFFFFF;
    x1 = y1 = (-0x7FFFFFFF - 1);
    if (g.curva_ini_x < x0) x0 = g.curva_ini_x;
    if (g.curva_ini_y < y0) y0 = g.curva_ini_y;
    if (g.curva_ini_x > x1) x1 = g.curva_ini_x;
    if (g.curva_ini_y > y1) y1 = g.curva_ini_y;
    if (g.curva_fin_x < x0) x0 = g.curva_fin_x;
    if (g.curva_fin_y < y0) y0 = g.curva_fin_y;
    if (g.curva_fin_x > x1) x1 = g.curva_fin_x;
    if (g.curva_fin_y > y1) y1 = g.curva_fin_y;
    if (g.curva_control1) {
        if (g.curva_control2) {
            c1x = g.curva_c1_x; c1y = g.curva_c1_y;
            c2x = g.curva_c2_x; c2y = g.curva_c2_y;
        } else {
            cubica_desde_cuadratica(g.curva_ini_x, g.curva_ini_y,
                                    g.curva_c1_x, g.curva_c1_y,
                                    g.curva_fin_x, g.curva_fin_y,
                                    &c1x, &c1y, &c2x, &c2y);
        }
        tiene_controles = 1;
    }
    if (tiene_controles) {
        for (i = 0; i <= 48; i++) {
            float t = (float)i / 48.0f;
            int px, py;
            punto_bezier(g.curva_ini_x, g.curva_ini_y, c1x, c1y,
                         c2x, c2y, g.curva_fin_x, g.curva_fin_y, t, &px, &py);
            if (px < x0) x0 = px;
            if (py < y0) y0 = py;
            if (px > x1) x1 = px;
            if (py > y1) y1 = py;
        }
    }
    if (x1 < x0) return 0;
    margen = grosor_px() / 2;
    *ox0 = limitar(x0 - margen, 0, g.doc.ancho - 1);
    *oy0 = limitar(y0 - margen, 0, g.doc.alto - 1);
    *ox1 = limitar(x1 + margen, 0, g.doc.ancho - 1);
    *oy1 = limitar(y1 + margen, 0, g.doc.alto - 1);
    return 1;
}

/* Deja activa una seleccion rectangular sobre la curva recien estampada.
 * La caja llega en coordenadas inclusivas, como `seleccionar_rect`; la
 * seleccion interna usa el borde derecho/inferior exclusivo. */
static void seleccionar_caja_curva(int x0, int y0, int x1, int y1) {
    int w, h, n;
    if (g.sel.flotante) {
        seleccion_soltar(&g.sel, &g.doc, &g.deshacer);
        doc_componer_todo(&g.doc);
    }
    seleccion_limpiar(&g.sel);
    if (g.doc.ancho <= 0 || g.doc.alto <= 0) return;
    x0 = limitar(x0, 0, g.doc.ancho - 1);
    y0 = limitar(y0, 0, g.doc.alto - 1);
    x1 = limitar(x1, 0, g.doc.ancho - 1);
    y1 = limitar(y1, 0, g.doc.alto - 1);
    w = x1 - x0 + 1;
    h = y1 - y0 + 1;
    if (w <= 0 || h <= 0) return;
    g.sel.activa = 1;
    g.sel.tipo = 0;
    g.sel.x0 = x0; g.sel.y0 = y0;
    g.sel.x1 = x0 + w; g.sel.y1 = y0 + h;
    n = w * h;
    g.sel.mascara = (u8 *)malloc((size_t)n);
    if (g.sel.mascara) memset(g.sel.mascara, 1, (size_t)n);
}

/* Confirma la curva. Si `con_seleccion` es cierto (Enter o soltar el
 * segundo control), deja seleccionada la caja que ocupa. */
static void curva_terminar(int con_seleccion) {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    int tiene_caja = curva_limites(&x0, &y0, &x1, &y1);
    if (respaldo_forma) free(respaldo_forma);
    respaldo_forma = NULL;
    g.preview_activa = 0;
    g.curva_activa = 0;
    g.curva_arrastrando = 0;
    g.curva_control1 = 0;
    g.curva_control2 = 0;
    g.doc.compuesto_sucio = 1;
    g.doc.modificado = 1;
    doc_componer_todo(&g.doc);
    if (con_seleccion && tiene_caja) seleccionar_caja_curva(x0, y0, x1, y1);
    app_invalidar();
}

void pintura_figura_cancelar(void) {
    respaldo_restaurar();
    if (respaldo_forma) free(respaldo_forma);
    respaldo_forma = NULL;
    g.preview_activa = 0;
    g.curva_activa = 0;
    g.curva_arrastrando = 0;
    g.curva_control1 = 0;
    g.curva_control2 = 0;
    g.poligono_activo = 0;
    g.poligono_num = 0;
    doc_componer_todo(&g.doc);
    app_invalidar();
}

void pintura_figura_confirmar(int x, int y, int shift) {
    (void)x; (void)y; (void)shift;
    if (respaldo_forma) free(respaldo_forma);
    respaldo_forma = NULL;
    g.preview_activa = 0;
    g.curva_activa = 0;
    g.curva_arrastrando = 0;
    g.curva_control1 = 0;
    g.curva_control2 = 0;
    g.poligono_activo = 0;
    g.poligono_num = 0;
    g.doc.compuesto_sucio = 1;
    g.doc.modificado = 1;
    doc_componer_todo(&g.doc);
    app_invalidar();
}

static void pintar_segmento_grueso(int x0, int y0, int x1, int y1, u32 color) {
    int r = (grosor_px() + 1) / 2;
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        int ox, oy;
        for (oy = -r; oy <= r; oy++) {
            for (ox = -r; ox <= r; ox++) {
                if (ox * ox + oy * oy <= r * r + r) {
                    capa_pintar(x0 + ox, y0 + oy, color, alfa_opacidad(255));
                }
            }
        }
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = err * 2;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

static void pintar_rectangulo(int x0, int y0, int x1, int y1, u32 color, int redondo) {
    int r = redondo ? limitar((abs(x1 - x0) < abs(y1 - y0) ? abs(x1 - x0) : abs(y1 - y0)) / 5, 2, 18) : 0;
    int x, y;
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            int dentro = 1;
            if (redondo) {
                int cx = (x < x0 + r) ? x0 + r : ((x > x1 - r) ? x1 - r : x);
                int cy = (y < y0 + r) ? y0 + r : ((y > y1 - r) ? y1 - r : y);
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy > r * r) dentro = 0;
            }
            if (dentro) capa_pintar(x, y, color, alfa_opacidad(255));
        }
    }
}

static void pintar_circulo(int cx, int cy, int rx, int ry, u32 color) {
    int x, y;
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;
    for (y = cy - ry; y <= cy + ry; y++) {
        for (x = cx - rx; x <= cx + rx; x++) {
            float nx = (float)(x - cx) / (float)rx;
            float ny = (float)(y - cy) / (float)ry;
            if (nx * nx + ny * ny <= 1.0f) capa_pintar(x, y, color, alfa_opacidad(255));
        }
    }
}

static void forma_desde_ancla(int x, int y, int shift, int boton) {
    int ax = g.ancla_x, ay = g.ancla_y;
    u32 color = color_de_boton(boton);
    respaldo_restaurar();
    switch (g.herramienta) {
        case H_LINEA:
            pintar_segmento_grueso(ax, ay, x, y, color);
            break;
        case H_RECTANGULO:
            if (shift) {
                int lado = abs(x - ax) > abs(y - ay) ? abs(x - ax) : abs(y - ay);
                x = ax + (x >= ax ? lado : -lado);
                y = ay + (y >= ay ? lado : -lado);
            }
            pintar_rectangulo(ax, ay, x, y, color, 0);
            break;
        case H_RECT_REDONDO:
            if (shift) {
                int lado = abs(x - ax) > abs(y - ay) ? abs(x - ax) : abs(y - ay);
                x = ax + (x >= ax ? lado : -lado);
                y = ay + (y >= ay ? lado : -lado);
            }
            pintar_rectangulo(ax, ay, x, y, color, 1);
            break;
        case H_CIRCULO: {
            int rx = abs(x - ax), ry = abs(y - ay);
            if (shift) { if (rx > ry) ry = rx; else rx = ry; }
            pintar_circulo(ax, ay, rx, ry, color);
            break;
        }
        default:
            break;
    }
}

void pintura_figura_preview(int x, int y, int shift) {
    if (!respaldo_forma) return;
    forma_desde_ancla(x, y, shift, g.boton_pintura);
    marcar_cambio();
    doc_componer_todo(&g.doc);
    app_invalidar();
}

static void poligono_preview(int x, int y, int boton) {
    int i;
    u32 color = color_de_boton(boton);
    respaldo_restaurar();
    if (g.poligono_num > 0) {
        pintar_segmento_grueso(g.poligono_puntos[0][0], g.poligono_puntos[0][1],
                               g.poligono_puntos[0][0], g.poligono_puntos[0][1], color);
    }
    for (i = 1; i < g.poligono_num; i++) {
        pintar_segmento_grueso(g.poligono_puntos[i - 1][0], g.poligono_puntos[i - 1][1],
                               g.poligono_puntos[i][0], g.poligono_puntos[i][1], color);
    }
    if (g.poligono_num > 0) {
        pintar_segmento_grueso(g.poligono_puntos[g.poligono_num - 1][0],
                               g.poligono_puntos[g.poligono_num - 1][1], x, y, color);
        pintar_segmento_grueso(x, y, g.poligono_puntos[0][0], g.poligono_puntos[0][1], color);
    }
    marcar_cambio();
    doc_componer_todo(&g.doc);
}

/* ------------------------------------------------------------------ */
/* Seleccion                                                           */
/* ------------------------------------------------------------------ */
static void sel_commit(void) {
    if (g.sel.flotante) {
        seleccion_soltar(&g.sel, &g.doc, &g.deshacer);
        doc_componer_todo(&g.doc);
    }
}

/* Confirma una seleccion flotante antes de cambiar de capa o de estructura,
 * igual que `sel.confirmar`. */
void pintura_confirmar_seleccion(void) {
    sel_commit();
}

static void sel_crear_libre(void) {
    int minx, miny, maxx, maxy, w, h, y, i;
    u8 *mascara;
    if (libre_num < 3) { seleccion_limpiar(&g.sel); return; }
    minx = maxx = libre_x[0];
    miny = maxy = libre_y[0];
    for (i = 1; i < libre_num; i++) {
        if (libre_x[i] < minx) minx = libre_x[i];
        if (libre_x[i] > maxx) maxx = libre_x[i];
        if (libre_y[i] < miny) miny = libre_y[i];
        if (libre_y[i] > maxy) maxy = libre_y[i];
    }
    minx = limitar(minx, 0, g.doc.ancho - 1);
    miny = limitar(miny, 0, g.doc.alto - 1);
    maxx = limitar(maxx, 0, g.doc.ancho - 1);
    maxy = limitar(maxy, 0, g.doc.alto - 1);
    w = maxx - minx + 1;
    h = maxy - miny + 1;
    if (w < 2 || h < 2) { seleccion_limpiar(&g.sel); return; }
    mascara = (u8 *)calloc((size_t)w * h, 1);
    if (!mascara) return;
    for (y = 0; y < h; y++) {
        float sy = (float)(miny + y) + 0.5f;
        int cruces[256];
        int num = 0;
        for (i = 0; i < libre_num; i++) {
            int j = (i + 1) % libre_num;
            float ax = (float)libre_x[i], ay = (float)libre_y[i];
            float bx = (float)libre_x[j], by = (float)libre_y[j];
            if ((ay <= sy && by > sy) || (by <= sy && ay > sy)) {
                float t = (sy - ay) / (by - ay);
                int px = redondear(ax + t * (bx - ax));
                if (num < 256) cruces[num++] = px;
            }
        }
        for (i = 1; i < num; i++) {
            int v = cruces[i], j = i - 1;
            while (j >= 0 && cruces[j] > v) { cruces[j + 1] = cruces[j]; j--; }
            cruces[j + 1] = v;
        }
        for (i = 0; i + 1 < num; i += 2) {
            int xa = limitar(cruces[i] - minx, 0, w);
            int xb = limitar(cruces[i + 1] - minx, 0, w);
            int xx;
            for (xx = xa; xx < xb; xx++) mascara[y * w + xx] = 1;
        }
    }
    seleccion_limpiar(&g.sel);
    g.sel.activa = 1;
    g.sel.tipo = 1;
    g.sel.x0 = minx; g.sel.y0 = miny; g.sel.x1 = maxx + 1; g.sel.y1 = maxy + 1;
    g.sel.mascara = mascara;
}

void pintura_seleccion_iniciar(int x, int y, int boton, int libre) {
    int dx, dy;
    (void)boton;
    if (!doc_desde_pantalla(x, y, &dx, &dy)) return;
    if (g.sel.activa) {
        Zona caja = seleccion_caja(&g.sel);
        if (caja.ancho > 0 && dx >= caja.x && dy >= caja.y &&
            dx < caja.x + caja.ancho && dy < caja.y + caja.alto && seleccion_contiene(&g.sel, dx, dy)) {
            pila_guardar(&g.deshacer, &g.doc);
            pila_limpiar(&g.rehacer);
            seleccion_levantar(&g.sel, &g.doc);
            g.sel_moviendo = 1;
            g.sel_ini_x = dx;
            g.sel_ini_y = dy;
            g.sel_arrastrando = 1;
            return;
        }
        sel_commit();
    }
    seleccion_limpiar(&g.sel);
    if (libre) {
        libre_num = 0;
        libre_x[libre_num] = dx;
        libre_y[libre_num] = dy;
        libre_num++;
        g.sel_arrastrando = 2;
    } else {
        g.sel.activa = 1;
        g.sel.tipo = 0;
        g.sel.x0 = dx; g.sel.y0 = dy; g.sel.x1 = dx + 1; g.sel.y1 = dy + 1;
        g.sel.mascara = NULL;
        g.sel_moviendo = 0;
        /* Esquina de anclaje fija: no usa las coordenadas normalizadas, que
         * se intercambian al arrastrar hacia arriba o hacia la izquierda. */
        g.sel_ini_x = dx;
        g.sel_ini_y = dy;
        g.sel_arrastrando = 3;
    }
}

void pintura_seleccion_mover(int x, int y) {
    int dx, dy;
    if (!g.sel_arrastrando) return;
    if (g.sel_arrastrando == 2) {
        if (!doc_desde_pantalla(x, y, &dx, &dy)) {
            float f = zoom_escala(zoom_porcentaje(g.zoom_idx));
            dx = limitar((int)floorf((float)(x - g.lienzo.x0) / f), 0, g.doc.ancho - 1);
            dy = limitar((int)floorf((float)(y - g.lienzo.y0) / f), 0, g.doc.alto - 1);
        }
        if (libre_num < MAX_LIBRE) {
            if (libre_num == 0 || libre_x[libre_num - 1] != dx || libre_y[libre_num - 1] != dy) {
                libre_x[libre_num] = dx;
                libre_y[libre_num] = dy;
                libre_num++;
            }
        }
        app_invalidar();
        return;
    }
    if (!doc_desde_pantalla(x, y, &dx, &dy)) {
        float f = zoom_escala(zoom_porcentaje(g.zoom_idx));
        dx = (int)floorf((float)(x - g.lienzo.x0) / f);
        dy = (int)floorf((float)(y - g.lienzo.y0) / f);
    }
    if (g.sel_arrastrando == 3) {
        int x0 = g.sel_ini_x, y0 = g.sel_ini_y;
        int x1 = dx, y1 = dy;
        if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
        if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
        g.sel.x0 = limitar(x0, 0, g.doc.ancho);
        g.sel.y0 = limitar(y0, 0, g.doc.alto);
        g.sel.x1 = limitar(x1, 0, g.doc.ancho);
        g.sel.y1 = limitar(y1, 0, g.doc.alto);
        if (g.sel.x1 <= g.sel.x0) g.sel.x1 = g.sel.x0 + 1;
        if (g.sel.y1 <= g.sel.y0) g.sel.y1 = g.sel.y0 + 1;
        app_invalidar();
        return;
    }
    if (g.sel_moviendo) {
        int mx = dx - g.sel_ini_x;
        int my = dy - g.sel_ini_y;
        g.sel_ini_x = dx;
        g.sel_ini_y = dy;
        seleccion_mover(&g.sel, &g.doc, mx, my);
        app_invalidar();
    }
}

void pintura_seleccion_soltar(int x, int y) {
    (void)x; (void)y;
    if (g.sel_arrastrando == 3) {
        int w = sel_ancho(&g.sel), h = sel_alto(&g.sel);
        if (w >= 1 && h >= 1) {
            g.sel.mascara = (u8 *)malloc((size_t)w * h);
            if (g.sel.mascara) memset(g.sel.mascara, 1, (size_t)w * h);
            if (w < 2 || h < 2) seleccion_limpiar(&g.sel);
        } else {
            seleccion_limpiar(&g.sel);
        }
    } else if (g.sel_arrastrando == 2) {
        sel_crear_libre();
    }
    g.sel_arrastrando = 0;
    g.sel_moviendo = 0;
    app_invalidar();
}

void pintura_seleccion_cancelar(void) {
    sel_commit();
    seleccion_limpiar(&g.sel);
    g.sel_arrastrando = 0;
    g.sel_moviendo = 0;
    app_invalidar();
}

void pintura_copiar(int cortar) {
    Capa *c = capa_activa();
    int x, y, w, h;
    if (!c || !g.sel.activa) return;
    if (g.portapapeles) { free(g.portapapeles); g.portapapeles = NULL; }
    w = sel_ancho(&g.sel);
    h = sel_alto(&g.sel);
    if (w <= 0 || h <= 0) return;
    g.portapapeles = (u32 *)calloc((size_t)w * h, sizeof(u32));
    if (!g.portapapeles) return;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int px = g.sel.x0 + x, py = g.sel.y0 + y;
            if (px < 0 || py < 0 || px >= g.doc.ancho || py >= g.doc.alto) continue;
            if (g.sel.mascara && !g.sel.mascara[y * w + x]) continue;
            if (g.sel.flotante) g.portapapeles[y * w + x] = g.sel.flotante[y * w + x];
            else g.portapapeles[y * w + x] = c->pixeles[(size_t)py * g.doc.ancho + px];
        }
    }
    g.portapapeles_w = w;
    g.portapapeles_h = h;
    if (cortar) {
        pila_guardar(&g.deshacer, &g.doc);
        pila_limpiar(&g.rehacer);
        if (g.sel.flotante) {
            free(g.sel.flotante);
            g.sel.flotante = NULL;
        }
        seleccion_borrar_contenido(&g.sel, &g.doc);
        doc_componer_todo(&g.doc);
    }
    app_invalidar();
}

void pintura_borrar_seleccion(void) {
    if (!g.sel.activa) return;
    pila_guardar(&g.deshacer, &g.doc);
    pila_limpiar(&g.rehacer);
    if (g.sel.flotante) {
        free(g.sel.flotante);
        g.sel.flotante = NULL;
    }
    seleccion_borrar_contenido(&g.sel, &g.doc);
    doc_componer_todo(&g.doc);
    app_invalidar();
}

void pintura_pegar(void) {
    int w = g.portapapeles_w, h = g.portapapeles_h;
    if (!g.portapapeles || w <= 0 || h <= 0) {
        app_estado("El portapapeles esta vacio");
        return;
    }
    sel_commit();
    pila_guardar(&g.deshacer, &g.doc);
    pila_limpiar(&g.rehacer);
    seleccion_limpiar(&g.sel);
    g.sel.activa = 1;
    g.sel.tipo = 0;
    g.sel.x0 = 0;
    g.sel.y0 = 0;
    g.sel.x1 = w;
    g.sel.y1 = h;
    g.sel.mascara = (u8 *)malloc((size_t)w * h);
    g.sel.flotante = (u32 *)malloc((size_t)w * h * sizeof(u32));
    if (g.sel.mascara) memset(g.sel.mascara, 1, (size_t)w * h);
    if (g.sel.flotante) copiar_pixeles(g.sel.flotante, g.portapapeles, w * h);
    g.herramienta = H_SEL_RECT;
    app_estado("Pegado: arrastra para colocar y pulsa fuera para confirmar");
    app_invalidar();
}

/* ------------------------------------------------------------------ */
/* Lienzo: tiradores                                                   */
/* ------------------------------------------------------------------ */
/* Zona del tirador: MANGO_TAM x MANGO_TAM px totalmente fuera del lienzo,
 * tocando el borde o la esquina (`Mango::zona`). */
static Zona mango_zona(int mango, Lienzo l) {
    int cx = (l.x0 + l.x1 - 1) / 2;
    int cy = (l.y0 + l.y1 - 1) / 2;
    int m = MANGO_TAM;
    int x = 0, y = 0;
    switch (mango) {
        case 0: x = l.x0 - m;     y = l.y0 - m; break;       /* Noroeste */
        case 1: x = cx - m / 2;   y = l.y0 - m; break;       /* Norte */
        case 2: x = l.x1;         y = l.y0 - m; break;       /* Noreste */
        case 3: x = l.x1;         y = cy - m / 2; break;     /* Este */
        case 4: x = l.x1;         y = l.y1; break;           /* Sureste */
        case 5: x = cx - m / 2;   y = l.y1; break;           /* Sur */
        case 6: x = l.x0 - m;     y = l.y1; break;           /* Suroeste */
        case 7: x = l.x0 - m;     y = cy - m / 2; break;     /* Oeste */
        default: break;
    }
    return zona_hacer(x, y, m, m);
}

/* Que bordes mueve cada tirador: (izquierda, derecha, arriba, abajo). */
static void mango_mueve(int mango, int *izq, int *der, int *arr, int *aba) {
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

static int mango_es_esquina(int mango) {
    return mango == 0 || mango == 2 || mango == 4 || mango == 6;
}

int lienzo_tirador_en(int x, int y) {
    int i;
    for (i = 0; i < 8; i++) {
        Zona z = mango_zona(i, g.lienzo);
        if (x >= z.x - TOLERANCIA_MANGO && x < zona_derecha(z) + TOLERANCIA_MANGO &&
            y >= z.y - TOLERANCIA_MANGO && y < zona_abajo(z) + TOLERANCIA_MANGO) {
            return i;
        }
    }
    return -1;
}

/* Desfase entre el cursor y el borde que mueve el tirador. Se descuenta al
 * arrastrar para que el lienzo no salte al agarrarlo (`desfase_arrastre`). */
static void desfase_arrastre(int mango, Lienzo l, int mx, int my,
                             int *ox, int *oy) {
    int izq, der, arr, aba;
    mango_mueve(mango, &izq, &der, &arr, &aba);
    *ox = izq ? mx - l.x0 : (der ? mx - l.x1 : 0);
    *oy = arr ? my - l.y0 : (aba ? my - l.y1 : 0);
}

/* Nuevo lienzo al arrastrar `mango` desde la geometria `base`. El borde
 * opuesto queda fijo y todo se ajusta a pixeles enteros (`redimensionar`). */
static Lienzo redimensionar(int mango, Lienzo base, int mx, int my,
                            int cuadrar, Zona zona) {
    Zona util = zona_lienzo_app(zona);
    int mueve_izq, mueve_der, mueve_arr, mueve_aba;
    int x0 = base.x0, y0 = base.y0, x1 = base.x1, y1 = base.y1;
    mango_mueve(mango, &mueve_izq, &mueve_der, &mueve_arr, &mueve_aba);

    if (cuadrar && mango_es_esquina(mango)) {
        int fx = mueve_izq ? base.x1 : base.x0;
        int fy = mueve_arr ? base.y1 : base.y0;
        int dx = mueve_izq ? -1 : 1;
        int dy = mueve_arr ? -1 : 1;
        int disponible_x = dx > 0 ? zona_derecha(util) - fx : fx - util.x;
        int disponible_y = dy > 0 ? zona_abajo(util) - fy : fy - util.y;
        int max_x = disponible_x > MIN_LADO ? disponible_x : MIN_LADO;
        int max_y = disponible_y > MIN_LADO ? disponible_y : MIN_LADO;
        int max_lado = max_x < max_y ? max_x : max_y;
        int lado_x = (mx - fx) * dx;
        int lado_y = (my - fy) * dy;
        int lado = lado_x > lado_y ? lado_x : lado_y;
        if (lado < MIN_LADO) lado = MIN_LADO;
        if (lado > max_lado) lado = max_lado;
        if (mueve_izq) x0 = fx + lado * dx;
        else x1 = fx + lado * dx;
        if (mueve_arr) y0 = fy + lado * dy;
        else y1 = fy + lado * dy;
        return (Lienzo){ x0, y0, x1, y1 };
    }

    {
        float factor = zoom_escala(zoom_porcentaje(g.zoom_idx));
        if (factor < 0.01f) factor = 0.01f;
        if (mueve_izq) {
            int doc = redondear((float)(x1 - mx) / factor);
            if (doc < MIN_LADO) doc = MIN_LADO;
            x0 = x1 - (int)redondear((float)doc * factor);
            if (x0 < util.x) x0 = util.x;
            if (x0 > x1 - MIN_LADO) x0 = x1 - MIN_LADO;
        }
        if (mueve_der) {
            int doc = redondear((float)(mx - x0) / factor);
            if (doc < MIN_LADO) doc = MIN_LADO;
            x1 = x0 + (int)redondear((float)doc * factor);
            if (x1 < x0 + MIN_LADO) x1 = x0 + MIN_LADO;
            if (x1 > zona_derecha(util)) x1 = zona_derecha(util);
        }
        if (mueve_arr) {
            int doc = redondear((float)(y1 - my) / factor);
            if (doc < MIN_LADO) doc = MIN_LADO;
            y0 = y1 - (int)redondear((float)doc * factor);
            if (y0 < util.y) y0 = util.y;
            if (y0 > y1 - MIN_LADO) y0 = y1 - MIN_LADO;
        }
        if (mueve_aba) {
            int doc = redondear((float)(my - y0) / factor);
            if (doc < MIN_LADO) doc = MIN_LADO;
            y1 = y0 + (int)redondear((float)doc * factor);
            if (y1 < y0 + MIN_LADO) y1 = y0 + MIN_LADO;
            if (y1 > zona_abajo(util)) y1 = zona_abajo(util);
        }
    }
    return (Lienzo){ x0, y0, x1, y1 };
}

/* Aplica el tamano logico del lienzo al documento, como `Dibujo::ajustar`. */
static void aplicar_tamano_logico(void) {
    int w, h;
    sincronizar_logico(g.lienzo, g.zoom_idx, &w, &h);
    doc_redimensionar(&g.doc, w, h);
}

void lienzo_redimensionar_iniciar(int tirador, int x, int y) {
    int ox, oy;
    g.lienzo_tirador = tirador;
    g.lienzo_base = g.lienzo;
    desfase_arrastre(tirador, g.lienzo, x, y, &ox, &oy);
    g.lienzo_offx = ox;
    g.lienzo_offy = oy;
    g.lienzo_hover = tirador;
}

void lienzo_redimensionar_mover(int x, int y, int shift) {
    if (g.lienzo_tirador < 0) return;
    g.lienzo = redimensionar(g.lienzo_tirador, g.lienzo_base,
                             x - g.lienzo_offx, y - g.lienzo_offy,
                             shift, zona_trabajo());
    aplicar_tamano_logico();
    app_invalidar();
    if (g.hwnd) UpdateWindow(g.hwnd);
}

void lienzo_redimensionar_soltar(int x, int y, int shift) {
    if (g.lienzo_tirador < 0) return;
    /* Se aplica tambien en el frame de soltado para no perder el ultimo pixel. */
    g.lienzo = redimensionar(g.lienzo_tirador, g.lienzo_base,
                             x - g.lienzo_offx, y - g.lienzo_offy,
                             shift, zona_trabajo());
    /* Al soltar, el lienzo vuelve al centro conservando el tamano y el zoom. */
    g.lienzo = lienzo_centrado_en(g.lienzo, zona_trabajo());
    aplicar_tamano_logico();
    g.lienzo_tirador = -1;
    app_invalidar();
    if (g.hwnd) UpdateWindow(g.hwnd);
}

/* ------------------------------------------------------------------ */
/* Dibujo del lienzo                                                   */
/* ------------------------------------------------------------------ */
static void dibujar_mangos(int x0, int y0, int x1, int y1, int activo) {
    int cx = (x0 + x1 - 1) / 2;
    int cy = (y0 + y1 - 1) / 2;
    int mx[8], my[8], i;
    /* Los tiradores van enteros por fuera del lienzo, tocando su borde. */
    mx[0] = x0 - MANGO_TAM; my[0] = y0 - MANGO_TAM;
    mx[1] = cx - MANGO_TAM / 2; my[1] = y0 - MANGO_TAM;
    mx[2] = x1; my[2] = y0 - MANGO_TAM;
    mx[3] = x1; my[3] = cy - MANGO_TAM / 2;
    mx[4] = x1; my[4] = y1;
    mx[5] = cx - MANGO_TAM / 2; my[5] = y1;
    mx[6] = x0 - MANGO_TAM; my[6] = y1;
    mx[7] = x0 - MANGO_TAM; my[7] = cy - MANGO_TAM / 2;
    for (i = 0; i < 8; i++) {
        u32 relleno = (i == activo) ? MANGO_ACTIVO : MANGO;
        dibujar_rect(mx[i], my[i], mx[i] + MANGO_TAM, my[i] + MANGO_TAM, MANGO_BORDE);
        dibujar_rect(mx[i] + 1, my[i] + 1, mx[i] + MANGO_TAM - 1,
                     my[i] + MANGO_TAM - 1, relleno);
    }
}

static void dibujar_hormigas(int x0, int y0, int x1, int y1, int fase) {
    int x, y;
    for (x = x0; x < x1; x++) {
        u32 c0 = (((x + y0 + fase) % 8) < 4) ? 0x00000000u : 0x00FFFFFFu;
        u32 c1 = (((x + (y1 - 1) + fase) % 8) < 4) ? 0x00000000u : 0x00FFFFFFu;
        dibujar_pixel(x, y0, c0);
        dibujar_pixel(x, y1 - 1, c1);
    }
    for (y = y0; y < y1; y++) {
        u32 c0 = (((x0 + y + fase) % 8) < 4) ? 0x00000000u : 0x00FFFFFFu;
        u32 c1 = (((x1 - 1 + y + fase) % 8) < 4) ? 0x00000000u : 0x00FFFFFFu;
        dibujar_pixel(x0, y, c0);
        dibujar_pixel(x1 - 1, y, c1);
    }
}

/* Dibuja un documento dentro de un lienzo de pantalla. La escala usa division
 * entera: cada pixel visible toma la columna `(x - x0) * ancho / cw`. Se usa en
 * el modo tiles y en su visor. */
void pintura_dibujar_documento(Documento *d, Lienzo l, Zona clip) {
    int cw = l.x1 - l.x0;
    int ch = l.y1 - l.y0;
    int x, y, x0, y0, x1, y1;

    if (!d || d->ancho <= 0 || d->alto <= 0 || cw <= 0 || ch <= 0) return;
    if (!d->compuesto || d->compuesto_sucio) doc_componer_todo(d);
    if (!d->compuesto) return;

    x0 = l.x0 < clip.x ? clip.x : l.x0;
    y0 = l.y0 < clip.y ? clip.y : l.y0;
    x1 = l.x1 > zona_derecha(clip) ? zona_derecha(clip) : l.x1;
    y1 = l.y1 > zona_abajo(clip) ? zona_abajo(clip) : l.y1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > g.ancho) x1 = g.ancho;
    if (y1 > g.alto) y1 = g.alto;
    dibujar_rect(x0, y0, x1, y1, LIENZO);
    if (x0 >= x1 || y0 >= y1) return;

    for (y = y0; y < y1; y++) {
        int sy = (int)(((long long)(y - l.y0) * d->alto) / ch);
        size_t fila;
        if (sy < 0) sy = 0;
        if (sy >= d->alto) sy = d->alto - 1;
        fila = (size_t)sy * d->ancho;
        for (x = x0; x < x1; x++) {
            int sx = (int)(((long long)(x - l.x0) * d->ancho) / cw);
            if (sx < 0) sx = 0;
            if (sx >= d->ancho) sx = d->ancho - 1;
            g.pixeles[(size_t)y * g.ancho + x] = d->compuesto[fila + sx];
        }
    }
}

void pintura_dibujar_lienzo(void) {
    Documento *dv = texto_documento_visible();
    Zona tr = zona_trabajo();
    int cw = g.lienzo.x1 - g.lienzo.x0;
    int ch = g.lienzo.y1 - g.lienzo.y0;
    int x, y;
    int x0, y0, x1, y1;

    if (!dv->compuesto || dv->compuesto_sucio) doc_componer_todo(dv);
    if (!dv->compuesto) return;

    x0 = g.lienzo.x0 < tr.x ? tr.x : g.lienzo.x0;
    y0 = g.lienzo.y0 < tr.y ? tr.y : g.lienzo.y0;
    x1 = g.lienzo.x1 > zona_derecha(tr) ? zona_derecha(tr) : g.lienzo.x1;
    y1 = g.lienzo.y1 > zona_abajo(tr) ? zona_abajo(tr) : g.lienzo.y1;
    if (x0 >= x1 || y0 >= y1 || cw <= 0 || ch <= 0 ||
        dv->ancho <= 0 || dv->alto <= 0) {
        pintura_dibujar_solo_overlays();
        return;
    }

    /* El lienzo es blanco puro, sin sombra ni borde: la separacion con la
     * rejilla ya la marcan las barras. */
    dibujar_rect(x0, y0, x1, y1, 0x00FFFFFFu);

    /* Misma division entera que `dibujar_dibujo`/`pixel_logico`: cada pixel de
     * pantalla toma la columna `(x - x0) * ancho / ancho_lienzo`. */
    for (y = y0; y < y1; y++) {
        int sy = (int)(((long long)(y - g.lienzo.y0) * dv->alto) / ch);
        size_t fila;
        if (sy < 0) sy = 0;
        if (sy >= dv->alto) sy = dv->alto - 1;
        fila = (size_t)sy * dv->ancho;
        for (x = x0; x < x1; x++) {
            int sx = (int)(((long long)(x - g.lienzo.x0) * dv->ancho) / cw);
            if (sx < 0) sx = 0;
            if (sx >= dv->ancho) sx = dv->ancho - 1;
            g.pixeles[(size_t)y * g.ancho + x] = dv->compuesto[fila + sx];
        }
    }

    pintura_dibujar_solo_overlays();
}

/* Contenido flotante de una seleccion levantada (mover, cortar o pegar). La
 * capa de origen queda vacia al levantar, asi que sin esto el bloque parece
 * desaparecer. Se muestrea en espacio de pantalla con la misma division entera
 * que el blit del lienzo y se recorta a la zona de trabajo. */
static void pintura_dibujar_flotante(void) {
    Zona tr = zona_trabajo();
    Zona caja;
    int cw = g.lienzo.x1 - g.lienzo.x0;
    int ch = g.lienzo.y1 - g.lienzo.y0;
    int w, h, x, y, x0, y0, x1, y1;

    if (!g.sel.activa || !g.sel.flotante) return;
    if (cw <= 0 || ch <= 0 || g.doc.ancho <= 0 || g.doc.alto <= 0) return;
    caja = seleccion_caja(&g.sel);
    w = caja.ancho;
    h = caja.alto;
    if (w <= 0 || h <= 0) return;

    pantalla_desde_doc(caja.x, caja.y, &x0, &y0);
    pantalla_desde_doc_fin(caja.x + w, caja.y + h, &x1, &y1);
    if (x0 < tr.x) x0 = tr.x;
    if (y0 < tr.y) y0 = tr.y;
    if (x1 > zona_derecha(tr)) x1 = zona_derecha(tr);
    if (y1 > zona_abajo(tr)) y1 = zona_abajo(tr);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > g.ancho) x1 = g.ancho;
    if (y1 > g.alto) y1 = g.alto;
    if (x0 >= x1 || y0 >= y1) return;

    for (y = y0; y < y1; y++) {
        int sy = (int)(((long long)(y - g.lienzo.y0) * g.doc.alto) / ch);
        int ly;
        if (sy < caja.y || sy >= caja.y + h) continue;
        ly = sy - caja.y;
        for (x = x0; x < x1; x++) {
            int sx = (int)(((long long)(x - g.lienzo.x0) * g.doc.ancho) / cw);
            int lx, alfa;
            u32 src;
            size_t p;
            if (sx < caja.x || sx >= caja.x + w) continue;
            lx = sx - caja.x;
            if (g.sel.mascara && !g.sel.mascara[ly * w + lx]) continue;
            src = g.sel.flotante[ly * w + lx];
            alfa = (int)((src >> 24) & 0xFF);
            if (alfa <= 0) continue;
            p = (size_t)y * g.ancho + x;
            g.pixeles[p] = color_mezclar(g.pixeles[p], src, alfa);
        }
    }
}

/* Mangos, seleccion, caja de texto y cursor del documento activo. Se separa
 * del lienzo para poder pintarlo sobre la cuadricula del modo tiles. */
void pintura_dibujar_solo_overlays(void) {
    float f = zoom_escala(zoom_porcentaje(g.zoom_idx));

    /* Bloque flotante: debe verse mientras se arrastra, se corta o se pega. */
    pintura_dibujar_flotante();

    /* Seleccion. */
    if (g.sel.activa) {
        int sx0, sy0, sx1, sy1;
        Zona caja = seleccion_caja(&g.sel);
        if (caja.ancho > 0) {
            pantalla_desde_doc(caja.x, caja.y, &sx0, &sy0);
            pantalla_desde_doc_fin(caja.x + caja.ancho, caja.y + caja.alto, &sx1, &sy1);
            if (g.sel.tipo == 0 || !g.sel.mascara) {
                dibujar_hormigas(sx0, sy0, sx1, sy1, (GetTickCount() / 90) % 8);
            } else {
                /* Contorno de la mascara libre. */
                int lx, ly;
                int fase = (GetTickCount() / 90) % 8;
                for (ly = 0; ly < caja.alto; ly++) {
                    for (lx = 0; lx < caja.ancho; lx++) {
                        int dentro = g.sel.mascara[ly * caja.ancho + lx];
                        int borde = 0;
                        if (lx == 0 || !g.sel.mascara[ly * caja.ancho + lx - 1]) borde = 1;
                        if (lx + 1 >= caja.ancho || !g.sel.mascara[ly * caja.ancho + lx + 1]) borde = 1;
                        if (ly == 0 || !g.sel.mascara[(ly - 1) * caja.ancho + lx]) borde = 1;
                        if (ly + 1 >= caja.alto || !g.sel.mascara[(ly + 1) * caja.ancho + lx]) borde = 1;
                        if (dentro && borde) {
                            int px, py;
                            pantalla_desde_doc(caja.x + lx, caja.y + ly, &px, &py);
                            dibujar_rect(px, py, px + (int)(f > 1 ? f : 1), py + (int)(f > 1 ? f : 1),
                                         (((px + py + fase) / 4) & 1) ? 0x00000000u : 0x00FFFFFFu);
                        }
                    }
                }
            }
            /* ants already drawn; handles moved below */
        }
    }

    /* Caja de texto. */
    if (g.texto_activo) {
        texto_dibujar_caja();
    }

    /* Tiradores del lienzo (canvas handles). En modo tiles el tamano es comun
     * a todos los tiles y no se redimensiona, asi que no se dibujan. */
    if (!g.modo_tiles)
        dibujar_mangos(g.lienzo.x0, g.lienzo.y0, g.lienzo.x1, g.lienzo.y1,
                       g.lienzo_tirador >= 0 ? g.lienzo_tirador : g.lienzo_hover);

    /* Tiradores de la seleccion. */
    if (g.sel.activa) {
        Zona caja = seleccion_caja(&g.sel);
        if (caja.ancho > 0) {
            int sx0, sy0, sx1, sy1;
            pantalla_desde_doc(caja.x, caja.y, &sx0, &sy0);
            pantalla_desde_doc_fin(caja.x + caja.ancho, caja.y + caja.alto, &sx1, &sy1);
            dibujar_mangos(sx0, sy0, sx1, sy1, g.sel_tirador);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Entrada del area de pintura                                         */
/* ------------------------------------------------------------------ */
void pintura_pulsar(int x, int y, int boton) {
    int dx, dy;
    int tirador = -1;

    /* Tiradores del lienzo: van 6 px por fuera del area blanca, asi que se
     * comprueban antes de descartar el clic por caer fuera del lienzo. */
    if (!g.modo_tiles && zona_contiene(zona_trabajo(), x, y))
        tirador = lienzo_tirador_en(x, y);
    if (tirador >= 0 && boton == 0) {
        if (g.sel.activa && g.sel.flotante) sel_commit();
        pila_guardar(&g.deshacer, &g.doc);
        pila_limpiar(&g.rehacer);
        lienzo_redimensionar_iniciar(tirador, x, y);
        return;
    }

    if (!lienzo_contiene(x, y)) {
        if (g.sel.activa && g.sel.flotante) sel_commit();
        return;
    }

    if (!doc_desde_pantalla(x, y, &dx, &dy)) return;

    /* Tiradores de la seleccion. */
    if (g.sel.activa) {
        Zona caja = seleccion_caja(&g.sel);
        int cx[8], cy[8], i;
        cx[0] = caja.x; cy[0] = caja.y;
        cx[1] = caja.x + caja.ancho / 2; cy[1] = caja.y;
        cx[2] = caja.x + caja.ancho; cy[2] = caja.y;
        cx[3] = caja.x + caja.ancho; cy[3] = caja.y + caja.alto / 2;
        cx[4] = caja.x + caja.ancho; cy[4] = caja.y + caja.alto;
        cx[5] = caja.x + caja.ancho / 2; cy[5] = caja.y + caja.alto;
        cx[6] = caja.x; cy[6] = caja.y + caja.alto;
        cx[7] = caja.x; cy[7] = caja.y + caja.alto / 2;
        for (i = 0; i < 8; i++) {
            if (abs(dx - cx[i]) * (int)(zoom_escala(zoom_porcentaje(g.zoom_idx)) + 1) <= 8 &&
                abs(dy - cy[i]) * (int)(zoom_escala(zoom_porcentaje(g.zoom_idx)) + 1) <= 8) {
                pila_guardar(&g.deshacer, &g.doc);
                pila_limpiar(&g.rehacer);
                seleccion_levantar(&g.sel, &g.doc);
                g.sel_tirador = i;
                if (original_escala) free(original_escala);
                original_escala = NULL;
                original_escala_w = sel_ancho(&g.sel);
                original_escala_h = sel_alto(&g.sel);
                original_x0 = g.sel.x0; original_y0 = g.sel.y0;
                original_x1 = g.sel.x1; original_y1 = g.sel.y1;
                if (g.sel.flotante) {
                    original_escala = (u32 *)malloc((size_t)original_escala_w * original_escala_h * sizeof(u32));
                    if (original_escala)
                        copiar_pixeles(original_escala, g.sel.flotante, original_escala_w * original_escala_h);
                }
                g.sel_arrastrando = 4;
                return;
            }
        }
    }

    switch (g.herramienta) {
        case H_CUENTAGOTAS: {
            u32 c = doc_color_visible(&g.doc, dx, dy);
            if (boton == 0) { g.frente = c; g.casilla_elegida = 0; }
            else { g.fondo = c; g.casilla_elegida = 1; }
            app_agregar_color_paleta(c);
            app_estado("Color %06X", c & 0xFFFFFF);
            app_invalidar();
            return;
        }
        case H_LUPA:
            /* La lupa no hace nada al pulsar: el zoom se cambia con Ctrl y la
             * rueda o con el control de la barra inferior. */
            return;
        case H_SEL_RECT:
            pintura_seleccion_iniciar(x, y, boton, 0);
            return;
        case H_SEL_LIBRE:
            pintura_seleccion_iniciar(x, y, boton, 1);
            return;
        case H_TEXTO:
            texto_empezar(dx, dy);
            app_invalidar();
            return;
        case H_CUBO:
            pintura_rellenar(dx, dy, boton);
            app_invalidar();
            return;
        case H_CURVA:
            if (!g.curva_activa) {
                /* Primer clic: empieza la recta base. */
                pila_guardar(&g.deshacer, &g.doc);
                pila_limpiar(&g.rehacer);
                respaldo_iniciar();
                g.preview_activa = 1;
                g.curva_activa = 1;
                g.curva_control1 = 0;
                g.curva_control2 = 0;
                g.curva_ini_x = dx; g.curva_ini_y = dy;
                g.curva_fin_x = dx; g.curva_fin_y = dy;
            } else {
                /* Segundo y tercer clic: colocan los puntos de control. */
                curva_ajustar(dx, dy);
            }
            g.boton_pintura = boton;
            g.curva_arrastrando = 1;
            curva_preview(boton);
            app_invalidar();
            return;
        case H_LINEA:
        case H_RECTANGULO:
        case H_RECT_REDONDO:
        case H_CIRCULO:
            pila_guardar(&g.deshacer, &g.doc);
            pila_limpiar(&g.rehacer);
            respaldo_iniciar();
            g.preview_activa = 1;
            g.ancla_x = dx;
            g.ancla_y = dy;
            g.boton_pintura = boton;
            forma_desde_ancla(dx, dy, mayus_activo(), boton);
            marcar_cambio();
            doc_componer_todo(&g.doc);
            app_invalidar();
            return;
        case H_POLIGONO:
            if (!g.poligono_activo) {
                pila_guardar(&g.deshacer, &g.doc);
                pila_limpiar(&g.rehacer);
                respaldo_iniciar();
                g.poligono_activo = 1;
                g.poligono_num = 0;
                g.boton_pintura = boton;
            }
            if (g.poligono_num < 64) {
                if (g.poligono_num >= 3) {
                    int ddx = dx - g.poligono_puntos[0][0];
                    int ddy = dy - g.poligono_puntos[0][1];
                    if (ddx * ddx + ddy * ddy <= 36) {
                        poligono_preview(dx, dy, boton);
                        pintura_figura_confirmar(dx, dy, 0);
                        return;
                    }
                }
                g.poligono_puntos[g.poligono_num][0] = dx;
                g.poligono_puntos[g.poligono_num][1] = dy;
                g.poligono_num++;
            }
            poligono_preview(dx, dy, boton);
            app_invalidar();
            return;
        default:
            pila_guardar(&g.deshacer, &g.doc);
            pila_limpiar(&g.rehacer);
            g.pintando = 1;
            g.boton_pintura = boton;
            semilla_trazo = (u32)GetTickCount() ^ (u32)(dx * 73856093) ^ (u32)(dy * 19349663);
            pintura_trazo_punto(dx, dy, boton, 1);
            return;
    }
}

void pintura_mover(int x, int y, int botones) {
    int dx, dy;
    if (g.lienzo_tirador >= 0) {
        lienzo_redimensionar_mover(x, y, mayus_activo());
        return;
    }
    if (g.sel_arrastrando == 4 && g.sel.activa) {
        int mx, my, i;
        Zona caja = seleccion_caja(&g.sel);
        int nx0 = original_x0, ny0 = original_y0, nx1 = original_x1, ny1 = original_y1;
        i = g.sel_tirador;
        mx = dx = (int)floorf((float)(x - g.lienzo.x0) / zoom_escala(zoom_porcentaje(g.zoom_idx)));
        my = dy = (int)floorf((float)(y - g.lienzo.y0) / zoom_escala(zoom_porcentaje(g.zoom_idx)));
        if (i == 0 || i == 6 || i == 7) nx0 = mx;
        if (i == 2 || i == 3 || i == 4) nx1 = mx;
        if (i == 0 || i == 1 || i == 2) ny0 = my;
        if (i == 4 || i == 5 || i == 6) ny1 = my;
        if (nx1 < nx0 + 2) nx1 = nx0 + 2;
        if (ny1 < ny0 + 2) ny1 = ny0 + 2;
        if (original_escala && original_escala_w > 0 && original_escala_h > 0) {
            int nw = nx1 - nx0, nh = ny1 - ny0;
            u32 *nuevo = (u32 *)calloc((size_t)nw * nh, sizeof(u32));
            u8 *mascara = (u8 *)calloc((size_t)nw * nh, 1);
            if (nuevo && mascara) {
                int xx, yy;
                for (yy = 0; yy < nh; yy++) {
                    int sy = yy * original_escala_h / nh;
                    for (xx = 0; xx < nw; xx++) {
                        int sx = xx * original_escala_w / nw;
                        nuevo[yy * nw + xx] = original_escala[sy * original_escala_w + sx];
                        mascara[yy * nw + xx] = g.sel.mascara ? 1 : 1;
                    }
                }
                free(g.sel.flotante);
                free(g.sel.mascara);
                g.sel.flotante = nuevo;
                g.sel.mascara = mascara;
                g.sel.x0 = nx0; g.sel.y0 = ny0; g.sel.x1 = nx1; g.sel.y1 = ny1;
            } else {
                free(nuevo); free(mascara);
            }
        }
        (void)caja;
        app_invalidar();
        return;
    }
    if (g.sel_arrastrando) {
        pintura_seleccion_mover(x, y);
        return;
    }
    if (g.pintando && g.modo_tiles) {
        if (tiles_modo_cruzar_trazo(x, y)) return;
    }
    if (g.pintando) {
        /* Igual que `pixel_actual`: si el cursor sale del lienzo no se pinta
         * nada; al volver se une con el ultimo punto valido. */
        if (doc_desde_pantalla(x, y, &dx, &dy))
            trazo_hasta(dx, dy, g.boton_pintura);
        app_invalidar();
        return;
    }
    if (g.preview_activa && (g.herramienta == H_LINEA || g.herramienta == H_RECTANGULO ||
                             g.herramienta == H_RECT_REDONDO || g.herramienta == H_CIRCULO)) {
        if (doc_desde_pantalla(x, y, &dx, &dy))
            pintura_figura_preview(dx, dy, mayus_activo());
        return;
    }
    if (g.curva_activa) {
        /* Solo se mueve el extremo o el control mientras el boton sigue
         * pulsado; al soltar, la previsualizacion se queda quieta. */
        if (g.curva_arrastrando && (botones & 3) &&
            pintura_pixel_ajustado(x, y, &dx, &dy)) {
            curva_mover(dx, dy);
            curva_preview(g.boton_pintura);
            app_invalidar();
        }
        return;
    }
    if (g.poligono_activo && g.preview_activa) {
        if (doc_desde_pantalla(x, y, &dx, &dy)) {
            poligono_preview(dx, dy, g.boton_pintura);
            app_invalidar();
        }
        return;
    }
}

void pintura_soltar(int x, int y, int boton) {
    if (g.lienzo_tirador >= 0) {
        lienzo_redimensionar_soltar(x, y, mayus_activo());
        g.lienzo_hover = zona_contiene(zona_trabajo(), x, y) ? lienzo_tirador_en(x, y) : -1;
        app_invalidar();
        return;
    }
    if (g.sel_arrastrando == 4) {
        if (original_escala) free(original_escala);
        original_escala = NULL;
        g.sel_arrastrando = 0;
        g.sel_tirador = -1;
        app_invalidar();
        return;
    }
    if (g.sel_arrastrando) {
        pintura_seleccion_soltar(x, y);
        return;
    }
    if (g.pintando) {
        g.pintando = 0;
        g.doc.compuesto_sucio = 1;
        doc_componer_todo(&g.doc);
        app_invalidar();
        return;
    }
    if (g.curva_activa) {
        g.curva_arrastrando = 0;
        /* Al soltar el segundo control la curva ya esta completa: se estampa
         * y queda seleccionada. Con uno solo sigue pendiente. */
        if (curva_completa()) curva_terminar(1);
        else app_invalidar();
        return;
    }
    if (g.preview_activa) {
        if (g.herramienta == H_POLIGONO) return;
        pintura_figura_confirmar(0, 0, 0);
        return;
    }
    (void)boton;
}

void pintura_doble_clic(int x, int y) {
    (void)x; (void)y;
    if (g.poligono_activo) {
        pintura_figura_confirmar(0, 0, 0);
    }
}

/* La rueda no desplaza el lienzo: sin Ctrl no hace nada. Ctrl + rueda cambia
 * el zoom desde `app_rueda`. */

void pintura_tecla(int vk, int ctrl, int shift) {
    if (ctrl && vk == 'Z') {
        if (pila_deshacer(&g.deshacer, &g.rehacer, &g.doc)) {
            doc_componer_todo(&g.doc);
            lienzo_actualizar();
            app_invalidar();
        }
        return;
    }
    if (ctrl && vk == 'Y') {
        if (pila_rehacer(&g.rehacer, &g.deshacer, &g.doc)) {
            doc_componer_todo(&g.doc);
            lienzo_actualizar();
            app_invalidar();
        }
        return;
    }
    if (ctrl && vk == 'X') { pintura_copiar(1); return; }
    if (ctrl && vk == 'C') { pintura_copiar(0); return; }
    if (ctrl && vk == 'V') { pintura_pegar(); return; }
    if (ctrl && vk == 'A') {
        if (shift) seleccion_limpiar(&g.sel);
        else seleccion_todo(&g.sel, g.doc.ancho, g.doc.alto);
        app_invalidar();
        return;
    }
    if (ctrl && vk == 'I') {
        seleccion_invertir(&g.sel, g.doc.ancho, g.doc.alto);
        app_invalidar();
        return;
    }
    if (vk == VK_DELETE) { pintura_borrar_seleccion(); return; }
    if (vk == VK_ESCAPE) {
        if (g.preview_activa) { pintura_figura_cancelar(); return; }
        if (g.sel.activa) { pintura_seleccion_cancelar(); return; }
        return;
    }
    if (vk == VK_RETURN) {
        if (g.curva_activa) {
            g.curva_arrastrando = 0;
            curva_terminar(1);
            return;
        }
        if (g.preview_activa) pintura_figura_confirmar(0, 0, 0);
        return;
    }
    if (!ctrl) {
        switch (vk) {
            case 'P': app_poner_herramienta(H_LAPIZ); break;
            case 'B': app_poner_herramienta(H_PINCEL); break;
            case 'G': app_poner_herramienta(H_CUBO); break;
            case 'S': app_poner_herramienta(H_AEROGRAFO); break;
            case 'E': app_poner_herramienta(H_BORRADOR); break;
            case 'T': app_poner_herramienta(H_TEXTO); break;
            case 'I': app_poner_herramienta(H_CUENTAGOTAS); break;
            case 'Z': app_poner_herramienta(H_LUPA); break;
            case 'L': app_poner_herramienta(H_LINEA); break;
            case 'C': app_poner_herramienta(H_CIRCULO); break;
            case 'R': app_poner_herramienta(H_RECTANGULO); break;
            case 'M': app_poner_herramienta(H_SEL_RECT); break;
            case '[': if (g.grosor_idx > 0) { g.grosor_idx--; app_invalidar(); } break;
            case ']': if (g.grosor_idx < 9) { g.grosor_idx++; app_invalidar(); } break;
            default: break;
        }
    }
    (void)shift;
}

void pintura_caracter(int ch) {
    (void)ch;
}

/* ------------------------------------------------------------------ */
/* Inicio / fin de pintura                                             */
/* ------------------------------------------------------------------ */
void pintura_iniciar(void) {
    respaldo_forma = NULL;
    original_escala = NULL;
    libre_num = 0;
    semilla_trazo = (u32)GetTickCount();
    g.curva_activa = 0;
    g.curva_arrastrando = 0;
    g.curva_control1 = 0;
    g.curva_control2 = 0;
}

void pintura_terminar(void) {
    if (respaldo_forma) free(respaldo_forma);
    if (original_escala) free(original_escala);
    respaldo_forma = NULL;
    original_escala = NULL;
}

/* ------------------------------------------------------------------ */
/* Ayudas compartidas con la herramienta de texto                       */
/* ------------------------------------------------------------------ */
int pintura_pixel_extendido(int mx, int my, int *x, int *y) {
    int cw = g.lienzo.x1 - g.lienzo.x0;
    int ch = g.lienzo.y1 - g.lienzo.y0;
    if (cw <= 0 || ch <= 0 || g.doc.ancho <= 0 || g.doc.alto <= 0) return 0;
    int mx2 = mx; int my2 = my;
    if (mx2 < g.lienzo.x0) mx2 = g.lienzo.x0;
    if (mx2 > g.lienzo.x1 - 1) mx2 = g.lienzo.x1 - 1;
    if (my2 < g.lienzo.y0) my2 = g.lienzo.y0;
    if (my2 > g.lienzo.y1 - 1) my2 = g.lienzo.y1 - 1;
    *x = (int)(((long long)(mx2 - g.lienzo.x0) * g.doc.ancho) / cw);
    if (*x >= g.doc.ancho) *x = g.doc.ancho - 1;
    if (*x < 0) *x = 0;
    *y = (int)(((long long)(my2 - g.lienzo.y0) * g.doc.alto) / ch);
    if (*y >= g.doc.alto) *y = g.doc.alto - 1;
    if (*y < 0) *y = 0;
    return 1;
}

int pintura_pixel_ajustado(int mx, int my, int *x, int *y) {
    return pintura_pixel_extendido(mx, my, x, y);
}

void pintura_pantalla_caja(int x0, int y0, int x1, int y1,
                           int *sx0, int *sy0, int *sx1, int *sy1) {
    pantalla_desde_doc(x0, y0, sx0, sy0);
    pantalla_desde_doc_fin(x1 + 1, y1 + 1, sx1, sy1);
}

int pintura_mango_en_caja(Lienzo caja, int mx, int my) {
    int i;
    for (i = 0; i < 8; i++) {
        Zona z = mango_zona(i, caja);
        if (mx >= z.x - TOLERANCIA_MANGO && mx < zona_derecha(z) + TOLERANCIA_MANGO &&
            my >= z.y - TOLERANCIA_MANGO && my < zona_abajo(z) + TOLERANCIA_MANGO) {
            return i;
        }
    }
    return -1;
}

void pintura_dibujar_mangos_caja(Lienzo caja, int activo) {
    dibujar_mangos(caja.x0, caja.y0, caja.x1, caja.y1, activo);
}

void pintura_dibujar_seleccion_caja(int x0, int y0, int x1, int y1, int fase) {
    dibujar_hormigas(x0, y0, x1, y1, fase);
}
