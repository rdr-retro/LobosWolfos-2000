/* colisiones.c - colisiones y fisica basica de la fase 29.
 *
 * Todo se resuelve contra las celdas del mapa con flags. Las cajas son
 * rectangulos en pixeles del mundo; el movimiento se aplica por ejes para
 * poder deslizarse por los muros. Los triggers y las celdas con dano se
 * anotan pero nunca bloquean, y las one-way solo bloquean bajando.
 */

#include "colisiones.h"

/* Devuelve los flags de una celda. Sin array (mapas construidos a mano en
 * las pruebas del nucleo) todas las celdas son solidas, como en el v1. */
static int flags_de(const MotorMapa *m, int i) {
    if (!m || !m->flags || i < 0 || i >= m->num_celdas)
        return MOTOR_CELDA_SOLIDO;
    return m->flags[i];
}

static int celda_anotada(const MotorMapa *m, int i, int bandera,
                         int x, int y, int w, int h) {
    return (flags_de(m, i) & bandera) != 0 &&
           motor_aabb(x, y, w, h,
                      m->celdas[i].x * m->lado, m->celdas[i].y * m->lado,
                      m->lado, m->lado);
}

static void anadir_trigger(int i, int *triggers, int max,
                           int *num) {
    int k;
    if (!triggers || *num >= max) return;
    for (k = 0; k < *num; k++)
        if (triggers[k] == i) return;
    triggers[*num] = i;
    (*num)++;
}

int caja_toca_celda(const MotorMapa *m, int i, int x, int y, int w, int h) {
    if (!m || i < 0 || i >= m->num_celdas) return 0;
    return motor_aabb(x, y, w, h,
                      m->celdas[i].x * m->lado, m->celdas[i].y * m->lado,
                      m->lado, m->lado);
}

int mapa_caja_bloqueada(const MotorMapa *m, int x, int y, int w, int h,
                        int dy) {
    int i;
    if (!m) return 0;
    for (i = 0; i < m->num_celdas; i++) {
        int f = flags_de(m, i);
        int cy;
        if (!(f & MOTOR_CELDA_SOLIDO) || (f & MOTOR_CELDA_HUECO)) continue;
        cy = m->celdas[i].y * m->lado;
        /* dy=0 sondea un movimiento horizontal: las one-way no cortan.
         * dy>0 sondea la caida: solo bloquean si se venia de arriba. */
        if ((f & MOTOR_CELDA_ONE_WAY) &&
            !(dy > 0 && y + h <= cy))
            continue;
        if (caja_toca_celda(m, i, x, y, w, h)) return 1;
    }
    return 0;
}

int colisionar_caja_mapa(const MotorMapa *m,
                         int *x, int *y, int w, int h, int dx, int dy,
                         int *triggers, int max_triggers, int *num_triggers,
                         int *danado) {
    int x0, y0, i, bloqueo = 0;

    if (!m || !x || !y) return 0;
    if (num_triggers) *num_triggers = 0;
    if (danado) *danado = 0;
    x0 = *x;
    y0 = *y;

    if (dx) {
        *x += dx;
        for (i = 0; i < m->num_celdas; i++) {
            int f = flags_de(m, i);
            /* Las one-way no cortan el movimiento horizontal; los triggers
             * y el dano tampoco. Las huecas no existen. */
            if (!(f & MOTOR_CELDA_SOLIDO) || (f & MOTOR_CELDA_HUECO) ||
                (f & MOTOR_CELDA_ONE_WAY))
                continue;
            if (caja_toca_celda(m, i, *x, *y, w, h)) {
                *x = x0;
                bloqueo = 1;
                break;
            }
        }
    }

    if (dy) {
        *y += dy;
        for (i = 0; i < m->num_celdas; i++) {
            int f = flags_de(m, i);
            int cy;
            if (!(f & MOTOR_CELDA_SOLIDO) || (f & MOTOR_CELDA_HUECO))
                continue;
            cy = m->celdas[i].y * m->lado;
            /* Una plataforma one-way solo detiene si se venia de arriba y
             * la caja no habia traspasado ya su borde superior. */
            if ((f & MOTOR_CELDA_ONE_WAY) &&
                !(dy > 0 && y0 + h <= cy))
                continue;
            if (caja_toca_celda(m, i, *x, *y, w, h)) {
                *y = y0;
                bloqueo = 1;
                break;
            }
        }
    }

    /* Triggers, dano y tags se leen siempre sobre la posicion final, aunque
     * el movimiento haya sido bloqueado por un muro solido. */
    for (i = 0; i < m->num_celdas; i++) {
        if (celda_anotada(m, i, MOTOR_CELDA_TRIGGER, *x, *y, w, h))
            anadir_trigger(i, triggers, max_triggers, num_triggers);
        if (celda_anotada(m, i, MOTOR_CELDA_DANO, *x, *y, w, h) && danado)
            *danado = 1;
    }
    return bloqueo;
}

/* Rebote en porcentaje: 0 no rebota, 100 devuelve toda la velocidad. */
static int rebotar(int vel, int rebote) {
    long long v;
    if (rebote <= 0) return 0;
    v = -(long long)vel * rebote / 100;
    if (v > 2147483647LL) v = 2147483647LL;
    if (v < -2147483648LL) v = -2147483648LL;
    return (int)v;
}

int fisica_paso(const MotorMapa *m,
                int *x, int *y, int w, int h,
                int *vel_x, int *vel_y, int gravedad, int rebote,
                int friccion, int *en_suelo) {
    int toco_suelo = 0;
    int trig[8], ntrig = 0, dano = 0;

    if (!x || !y || !vel_x || !vel_y) return 0;

    /* Friccion hacia cero, sin cambiar el signo. */
    if (friccion > 0) {
        if (*vel_x > 0) *vel_x = (*vel_x > friccion) ? *vel_x - friccion : 0;
        else if (*vel_x < 0) *vel_x = (*vel_x < -friccion) ? *vel_x + friccion : 0;
    }
    /* En el suelo, la gravedad no se acumula: un vel_y negativo es un
     * salto y si se deja de pisar, en_suelo se apaga y cae al instante. */
    if (en_suelo && *en_suelo && *vel_y >= 0)
        *vel_y = 0;
    else
        *vel_y += gravedad;

    if (colisionar_caja_mapa(m, x, y, w, h, *vel_x, 0,
                             trig, 8, &ntrig, &dano))
        *vel_x = rebotar(*vel_x, rebote);

    if (colisionar_caja_mapa(m, x, y, w, h, 0, *vel_y,
                             trig, 8, &ntrig, &dano)) {
        if (*vel_y > 0) toco_suelo = 1;
        *vel_y = rebotar(*vel_y, rebote);
        if (*vel_y == 0 && toco_suelo) *vel_y = 0;
    } else if (en_suelo && *en_suelo) {
        /* Se conserva el aviso de suelo un fotograma para que el script
         * pueda leer en_suelo justo despues de aterrizar. */
        toco_suelo = 1;
    }
    if (en_suelo) *en_suelo = toco_suelo;
    return toco_suelo;
}
