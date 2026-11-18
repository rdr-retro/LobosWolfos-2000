/* render.c - render de entidades, trajes y camara (fase 30).
 *
 * C puro: no windows.h ni llamadas al sistema. Las imagenes vienen de
 * activo.c y los pixeles son 0xAARRGGBB, con alpha 0 = transparente.
 *
 * La animacion une los dos mecanismos pedidos por el plan: un traje puede
 * ser una tira horizontal de `cuadros` imagenes y el objeto puede tener
 * varios trajes que se recorren como una lista. Cada traje dura
 * `velocidad * cuadros` fotogramas; `mi.traje` con un indice distinto de
 * cero fija la lista a ese traje y solo anima su tira.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "motor.h"
#include "logic3_motor.h"
#include "entidades.h"
#include "render.h"

#define RENDER_RUTA_MAX  260
#define RENDER_TRAJES_MAX 128
#define RENDER_OBJETOS_MAX 8

static char g_proyecto_ruta[RENDER_RUTA_MAX];

typedef struct TrajeCache {
    const MotorL3Traje *traje;
    char ruta[RENDER_RUTA_MAX];
    MotorImagen imagen;
    int estado; /* 0 sin probar, 1 cargado, -1 fallido */
} TrajeCache;

static TrajeCache g_trajes[RENDER_TRAJES_MAX];
static int g_num_trajes;

/* Codigo de copia corto (activo.c usa MAX_PATH, aqui no queremos windows.h). */
static void copiar_ruta(char *dst, int tam, const char *src) {
    int i;
    if (tam <= 0) return;
    for (i = 0; i < tam - 1 && src && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

int render_iniciar(const char *proyecto_ruta) {
    int i;
    render_terminar();
    copiar_ruta(g_proyecto_ruta, sizeof(g_proyecto_ruta),
                proyecto_ruta ? proyecto_ruta : "");
    if (!activo_gdiplus_iniciar()) return 0;
    for (i = 0; i < RENDER_TRAJES_MAX; i++) g_trajes[i].estado = -1;
    return 1;
}

void render_terminar(void) {
    int i;
    for (i = 0; i < g_num_trajes; i++) {
        if (g_trajes[i].imagen.pixeles) free(g_trajes[i].imagen.pixeles);
        memset(&g_trajes[i], 0, sizeof(g_trajes[i]));
    }
    g_num_trajes = 0;
    g_proyecto_ruta[0] = '\0';
}

/* Busca la entrada del cache de un traje y la reserva si hace falta. */
static TrajeCache *cache_traje(const MotorL3Traje *t) {
    int i;
    if (!t) return NULL;
    for (i = 0; i < g_num_trajes; i++)
        if (g_trajes[i].traje == t) return &g_trajes[i];
    if (g_num_trajes >= RENDER_TRAJES_MAX) return NULL;
    i = g_num_trajes++;
    memset(&g_trajes[i], 0, sizeof(g_trajes[i]));
    g_trajes[i].traje = t;
    return &g_trajes[i];
}

/* Resuelve y carga el PNG una sola vez por traje. Un traje sin imagen
 * queda marcado como fallido para no volver a tocar el disco cada
 * fotograma. */
static int cache_obtener(const MotorL3Traje *t, MotorImagen *img) {
    TrajeCache *c = cache_traje(t);
    if (!c || !t->imagen[0]) return 0;
    if (c->estado == 0) {
        char ruta[RENDER_RUTA_MAX];
        if (!activo_resolver_activo(g_proyecto_ruta, "trajes", t->imagen,
                                    ruta, sizeof(ruta)) ||
            !activo_cargar_imagen(ruta, &c->imagen)) {
            c->estado = -1;
        } else {
            copiar_ruta(c->ruta, sizeof(c->ruta), ruta);
            c->estado = 1;
        }
    }
    if (c->estado != 1) return 0;
    if (img) *img = c->imagen;
    return 1;
}

static const MotorL3Objeto *objeto_l3_por_id(const MotorL3 *p, int id) {
    int i;
    if (!p) return NULL;
    for (i = 0; i < p->num_objetos; i++)
        if (p->objetos[i].id == id) return &p->objetos[i];
    return NULL;
}

static int duracion_traje(const MotorL3Traje *t) {
    int vel = t->velocidad > 0 ? t->velocidad : 1;
    int cuadros = t->cuadros > 0 ? t->cuadros : 1;
    return vel * cuadros;
}

/* Calcula el rectangulo de origen del cuadro dentro de la tira. Si el
 * traje no declara tamano de cuadro se deduce de `ancho` (o de la imagen
 * completa cuando `ancho` es 0). */
static void rectangulo_cuadro(const MotorL3Traje *t, const MotorImagen *img,
                              int cuadro, RenderSeleccion *sel) {
    int fw = t->cuadro_ancho;
    int fh = t->cuadro_alto;
    int sx, sy;

    if (fw <= 0)
        fw = (t->cuadros > 1 && t->ancho > 0) ? t->ancho / t->cuadros : t->ancho;
    if (fw <= 0) fw = img->ancho;
    if (fh <= 0) fh = t->alto;
    if (fh <= 0) fh = img->alto;
    if (fw > img->ancho) fw = img->ancho;
    if (fh > img->alto) fh = img->alto;
    if (fw < 1 || fh < 1) {
        sel->sx = 0; sel->sy = 0; sel->sw = 0; sel->sh = 0;
        return;
    }
    sx = cuadro * fw;
    sy = 0;
    if (sx + fw > img->ancho) sx = img->ancho - fw;
    if (sx < 0) sx = 0;
    sel->sx = sx;
    sel->sy = sy;
    sel->sw = fw;
    sel->sh = fh;
}

int render_seleccionar(const MotorL3 *p, const Entidad *e, int fotograma,
                       RenderSeleccion *sel) {
    const MotorL3Objeto *o;
    const MotorL3Traje *t = NULL;
    MotorImagen img;
    int i, explicitas = 0, tiempo = 0, cuadro = 0, total = 0;

    if (!p || !e || !sel) return 0;
    memset(sel, 0, sizeof(*sel));
    o = objeto_l3_por_id(p, entidades_tipo_objeto(e->tipo));
    if (!o || o->num_trajes <= 0) return 0;

    /* `mi.traje` (o el traje inicial) permite forzar un traje concreto. */
    if (e->traje != 0) {
        for (i = 0; i < o->num_trajes; i++) {
            if (o->trajes[i].indice == e->traje && o->trajes[i].imagen[0]) {
                t = &o->trajes[i];
                explicitas = 1;
                break;
            }
        }
    }

    if (!t) {
        for (i = 0; i < o->num_trajes; i++)
            if (o->trajes[i].imagen[0]) total += duracion_traje(&o->trajes[i]);
        if (total <= 0) return 0;
        tiempo = (fotograma - 1) % total;
        for (i = 0; i < o->num_trajes; i++) {
            int d;
            if (!o->trajes[i].imagen[0]) continue;
            d = duracion_traje(&o->trajes[i]);
            if (tiempo < d) {
                t = &o->trajes[i];
                break;
            }
            tiempo -= d;
        }
    } else {
        int vel = t->velocidad > 0 ? t->velocidad : 1;
        tiempo = ((fotograma - 1) / vel) * vel;
    }

    if (!t || !cache_obtener(t, &img)) return 0;
    if (explicitas) {
        int vel = t->velocidad > 0 ? t->velocidad : 1;
        cuadro = ((fotograma - 1) / vel) % (t->cuadros > 0 ? t->cuadros : 1);
    } else {
        int vel = t->velocidad > 0 ? t->velocidad : 1;
        cuadro = (tiempo / vel) % (t->cuadros > 0 ? t->cuadros : 1);
    }

    sel->imagen = img;
    sel->indice = t->indice;
    sel->cuadro = cuadro;
    sel->pivote_x = t->pivote_x;
    sel->pivote_y = t->pivote_y;
    sel->flip_x = t->flip_x ? 1 : 0;
    sel->flip_y = t->flip_y ? 1 : 0;
    sel->rotacion = t->rotacion;
    sel->capa = e->capa;
    sel->id = e->id;
    rectangulo_cuadro(t, &img, cuadro, sel);
    return sel->sw > 0 && sel->sh > 0;
}

/* Transforma una coordenada local del cuadro a desplazamiento respecto al
 * pivote. El flip se aplica antes de la rotacion, como en un sprite. */
static void transformar_punto(const RenderSeleccion *sel, int lx, int ly,
                              int *rx, int *ry) {
    int fx = sel->flip_x ? sel->sw - 1 - lx : lx;
    int fy = sel->flip_y ? sel->sh - 1 - ly : ly;
    int x = fx - sel->pivote_x;
    int y = fy - sel->pivote_y;
    int tmp;
    switch (sel->rotacion) {
    case 90:  tmp = x; x = -y; y = tmp; break;
    case 180: x = -x; y = -y; break;
    case 270: tmp = x; x = y; y = -tmp; break;
    default: break;
    }
    *rx = x;
    *ry = y;
}

void render_blit(u32 *fb, int fb_ancho, int fb_alto, int dx, int dy,
                 const MotorImagen *img, int sx, int sy, int sw, int sh,
                 int pivote_x, int pivote_y,
                 int flip_x, int flip_y, int rotacion, int zoom) {
    RenderSeleccion sel;
    int j, i, by, bx;

    if (!fb || !img || !img->pixeles || sw <= 0 || sh <= 0) return;
    if (zoom < 1) zoom = 1;
    if (zoom > 4) zoom = 4;
    memset(&sel, 0, sizeof(sel));
    sel.sw = sw;
    sel.sh = sh;
    sel.pivote_x = pivote_x;
    sel.pivote_y = pivote_y;
    sel.flip_x = flip_x;
    sel.flip_y = flip_y;
    sel.rotacion = rotacion;

    for (j = 0; j < sh; j++) {
        const u32 *fila = img->pixeles + (size_t)(sy + j) * (size_t)img->ancho;
        for (i = 0; i < sw; i++) {
            u32 color;
            int rx, ry, px, py;
            if (sx + i < 0 || sx + i >= img->ancho) continue;
            color = fila[sx + i];
            if ((color & 0xFF000000u) == 0) continue;
            transformar_punto(&sel, i, j, &rx, &ry);
            px = dx + rx * zoom;
            py = dy + ry * zoom;
            for (by = 0; by < zoom; by++) {
                int y = py + by;
                if (y < 0 || y >= fb_alto) continue;
                for (bx = 0; bx < zoom; bx++) {
                    int x = px + bx;
                    if (x < 0 || x >= fb_ancho) continue;
                    fb[(size_t)y * (size_t)fb_ancho + (size_t)x] = color;
                }
            }
        }
    }
}

int render_dibujar(u32 *fb, int fb_ancho, int fb_alto,
                   const RenderSeleccion *sel, int x, int y, int zoom,
                   RenderInfo *info) {
    int min_x, min_y, max_x, max_y, k;
    int puntos[4][2];
    int coords[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};

    if (!sel || !sel->imagen.pixeles || sel->sw <= 0 || sel->sh <= 0)
        return 0;
    if (info) {
        if (info->blits >= RENDER_BLITS_MAX) {
            info->saltados++;
            return 0;
        }
    }
    if (zoom < 1) zoom = 1;
    if (zoom > 4) zoom = 4;

    /* Culling: las cuatro esquinas del cuadro transformadas. Si ninguna
     * parte cae en pantalla, el blit se descarta sin tocar el framebuffer. */
    coords[1][0] = sel->sw - 1;
    coords[2][0] = sel->sw - 1; coords[2][1] = sel->sh - 1;
    coords[3][1] = sel->sh - 1;
    for (k = 0; k < 4; k++) {
        transformar_punto(sel, coords[k][0], coords[k][1],
                          &puntos[k][0], &puntos[k][1]);
        puntos[k][0] = x + puntos[k][0] * zoom;
        puntos[k][1] = y + puntos[k][1] * zoom;
    }
    min_x = max_x = puntos[0][0];
    min_y = max_y = puntos[0][1];
    for (k = 1; k < 4; k++) {
        if (puntos[k][0] < min_x) min_x = puntos[k][0];
        if (puntos[k][0] > max_x) max_x = puntos[k][0];
        if (puntos[k][1] < min_y) min_y = puntos[k][1];
        if (puntos[k][1] > max_y) max_y = puntos[k][1];
    }
    if (max_x < 0 || min_x >= fb_ancho || max_y < 0 || min_y >= fb_alto) {
        if (info) info->recortados++;
        return 0;
    }

    render_blit(fb, fb_ancho, fb_alto, x, y, &sel->imagen,
                sel->sx, sel->sy, sel->sw, sel->sh,
                sel->pivote_x, sel->pivote_y,
                sel->flip_x, sel->flip_y, sel->rotacion, zoom);
    if (info) info->blits++;
    return 1;
}

void render_camara_iniciar(const MotorL3 *p, RenderCamara *c) {
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->zoom = 1;
    if (!p) return;
    c->objetivo = p->camara.objetivo;
    c->x = p->camara.x;
    c->y = p->camara.y;
    c->ancho = p->camara.ancho;
    c->alto = p->camara.alto;
    c->zoom = (p->camara.zoom == 2 || p->camara.zoom == 4)
              ? p->camara.zoom : 1;
    /* Solo se limita al tamano del proyecto cuando el LOGIC3 trae una
     * linea `camara` sin `limites`. Sin esa linea no se limita nada, para
     * conservar la camara clasica que seguia al sprite sin topes. */
    if (p->camara.definida && (!c->ancho || !c->alto)) {
        c->x = 0; c->y = 0;
        c->ancho = p->ancho;
        c->alto = p->alto;
    }
}

void render_camara_aplicar(RenderCamara *c, int objetivo, int zoom,
                           int limites[4]) {
    if (!c) return;
    if (objetivo >= 0) c->objetivo = objetivo;
    if (zoom == 1 || zoom == 2 || zoom == 4) c->zoom = zoom;
    if (limites) {
        c->x = limites[0] < 0 ? 0 : limites[0];
        c->y = limites[1] < 0 ? 0 : limites[1];
        c->ancho = limites[2] < 0 ? 0 : limites[2];
        c->alto = limites[3] < 0 ? 0 : limites[3];
    }
}

int render_camara_calcular(const RenderCamara *c, int fb_ancho, int fb_alto,
                           int jugador_x, int jugador_y,
                           int *cam_x, int *cam_y) {
    int tx = jugador_x, ty = jugador_y;
    int vista_ancho, vista_alto, x, y;
    int encontrado = 0;

    if (!c || !cam_x || !cam_y) return 0;
    if (c->objetivo > 0) {
        int ids[RENDER_OBJETOS_MAX];
        int n = entidades_de_objeto(c->objetivo, ids, RENDER_OBJETOS_MAX);
        if (n > 0) {
            Entidad *e = entidad_buscar(ids[0]);
            if (e && e->visible) {
                tx = e->x + (e->w > 0 ? e->w / 2 : 8);
                ty = e->y + (e->h > 0 ? e->h / 2 : 8);
                encontrado = 1;
            }
        }
    }

    vista_ancho = fb_ancho / c->zoom;
    vista_alto = fb_alto / c->zoom;
    if (vista_ancho < 1) vista_ancho = 1;
    if (vista_alto < 1) vista_alto = 1;
    x = tx - vista_ancho / 2;
    y = ty - vista_alto / 2;

    if (c->ancho > 0 && c->alto > 0) {
        if (c->ancho <= vista_ancho)
            x = c->x + (c->ancho - vista_ancho) / 2;
        else {
            if (x < c->x) x = c->x;
            if (x > c->x + c->ancho - vista_ancho)
                x = c->x + c->ancho - vista_ancho;
        }
        if (c->alto <= vista_alto)
            y = c->y + (c->alto - vista_alto) / 2;
        else {
            if (y < c->y) y = c->y;
            if (y > c->y + c->alto - vista_alto)
                y = c->y + c->alto - vista_alto;
        }
    }
    *cam_x = x;
    *cam_y = y;
    return encontrado;
}
