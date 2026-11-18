/* colisiones.h - colisiones y fisica basica de la fase 29.
 *
 * Los flags de las celdas viven en arrays paralelos de MotorMapa para no
 * cambiar el tamano de MotorCelda: el render en ensamblador sigue leyendo
 * { x, y, imagen } (12 bytes). El indice de cada flag corresponde al indice
 * de la celda en MotorMapa.celdas.
 *
 * C puro: lo enlazan game.exe y el editor.
 */
#ifndef COLISIONES_H
#define COLISIONES_H

#include "motor.h"

/* Traslada una caja por el mapa, eje a eje, devolviendo en `triggers` los
 * indices de celdas trigger tocadas (hasta `max_triggers`). `danado` se pone
 * a 1 si se toco una celda con dano. Devuelve 1 si hubo bloqueo solido. */
int colisionar_caja_mapa(const MotorMapa *m,
                         int *x, int *y, int w, int h, int dx, int dy,
                         int *triggers, int max_triggers, int *num_triggers,
                         int *danado);

/* Paso de fisica: aplica gravedad, rebote y friccion, mueve la caja contra
 * el mapa y devuelve 1 si ha tocado el suelo en este paso. */
int fisica_paso(const MotorMapa *m,
                int *x, int *y, int w, int h,
                int *vel_x, int *vel_y, int gravedad, int rebote,
                int friccion, int *en_suelo);

/* Comprueba si una caja toca una celda concreta (para triggers sin
 * bloqueo). */
int caja_toca_celda(const MotorMapa *m, int i, int x, int y, int w, int h);

/* Ocupacion solida de una caja. dy=0 cuenta las one-way como solidas;
 * dy>0 solo las cuenta si se viene de arriba. */
int mapa_caja_bloqueada(const MotorMapa *m, int x, int y, int w, int h, int dy);

#endif /* COLISIONES_H */
