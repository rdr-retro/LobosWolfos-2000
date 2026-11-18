/* render.h - render de entidades, trajes y camara (fase 30).
 *
 * El modulo resuelve y cachea los PNG/BMP de `trajes/`, calcula el cuadro
 * de animacion de cada entidad (tira horizontal o lista de trajes), aplica
 * pivote, flip, rotacion y zoom entero, recorta lo que cae fuera de la
 * pantalla y limita el numero de blits por fotograma.
 *
 * No depende de windows.h: la carga de imagenes y la resolucion de rutas
 * estan en activo.c. Lo enlazan game.exe y, mas adelante, la vista previa
 * del editor.
 */
#ifndef RENDER_H
#define RENDER_H

#include "motor.h"
#include "logic3_motor.h"
#include "entidades.h"

#define RENDER_CAMARA_JUGADOR 0
#define RENDER_BLITS_MAX      256 /* presupuesto por fotograma */

/* Camara del fotograma: objetivo, limites del mundo y zoom entero. */
typedef struct RenderCamara {
    int objetivo;          /* id de objeto LOGIC3, 0 = sprite del jugador */
    int x, y, ancho, alto; /* limites del mundo; 0 = sin limites */
    int zoom;              /* 1, 2 o 4 */
} RenderCamara;

/* Un traje ya seleccionado y listo para estampar. */
typedef struct RenderSeleccion {
    MotorImagen imagen;    /* pixeles del cache, NULL si no se encontro */
    int indice;            /* indice del traje dentro del objeto */
    int cuadro;            /* cuadro dentro de la tira */
    int sx, sy, sw, sh;    /* rectangulo de origen en la imagen */
    int pivote_x, pivote_y;/* pivote dentro del cuadro */
    int flip_x, flip_y;
    int rotacion;
    int capa, id;
} RenderSeleccion;

/* Contadores del fotograma, para la traza --render. */
typedef struct RenderInfo {
    int blits;      /* entidades estampadas */
    int recortados; /* descartadas por culling */
    int saltados;   /* descartadas por presupuesto o sin imagen */
    int cam_x, cam_y;
} RenderInfo;

/* Prepara el cache. `proyecto_ruta` es el .logic3 (o "" si no hay); se
 * llama despues de activo_gdiplus_iniciar(). */
int  render_iniciar(const char *proyecto_ruta);
void render_terminar(void);

/* Copia la camara del LOGIC3 y aplica las opciones de linea de comandos.
 * `objetivo` < 0, `zoom` <= 0 y `limites` NULL significan "no cambiar". */
void render_camara_iniciar(const MotorL3 *p, RenderCamara *c);
void render_camara_aplicar(RenderCamara *c, int objetivo, int zoom,
                           int limites[4]);

/* Calcula la esquina de la camara. `jugador_x/y` es el centro del sprite
 * clasico, usado cuando el objetivo es 0 o no existe. Devuelve 1 si encontro
 * el objeto objetivo y 0 si cayo al jugador. */
int render_camara_calcular(const RenderCamara *c, int fb_ancho, int fb_alto,
                           int jugador_x, int jugador_y,
                           int *cam_x, int *cam_y);

/* Selecciona el traje y el cuadro correspondientes a una entidad. Devuelve
 * 1 si hay imagen cargada; 0 si la entidad debe dibujarse como rectangulo. */
int render_seleccionar(const MotorL3 *p, const Entidad *e, int fotograma,
                       RenderSeleccion *sel);

/* Estampa la seleccion. Devuelve 1 si se dibujo; 0 si se recorto o entro
 * en el presupuesto. */
int render_dibujar(u32 *fb, int fb_ancho, int fb_alto,
                   const RenderSeleccion *sel, int x, int y, int zoom,
                   RenderInfo *info);

/* Blit generico con flip, rotacion y zoom. `dx,dy` es la posicion del
 * pivote en el destino y `sx,sy,sw,sh` el rectangulo de origen. */
void render_blit(u32 *fb, int fb_ancho, int fb_alto, int dx, int dy,
                 const MotorImagen *img, int sx, int sy, int sw, int sh,
                 int pivote_x, int pivote_y,
                 int flip_x, int flip_y, int rotacion, int zoom);

#endif /* RENDER_H */
