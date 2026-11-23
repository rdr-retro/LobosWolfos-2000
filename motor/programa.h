/* programa.h - runtime minimo del constructor de programa.
 *
 * Convierte un perfil de control en movimiento de entidades. Mantiene la
 * capa de entrada separada del juego para que game.exe y la vista previa
 * puedan usar exactamente las mismas reglas.
 */
#ifndef MOTOR_PROGRAMA_H
#define MOTOR_PROGRAMA_H

#include "motor.h"
#include "entrada.h"
#include "logic3_motor.h"

/* Devuelve el primer control activo de un proyecto, o NULL. */
const MotorL3Control *motor_programa_control_principal(const MotorL3 *p);

/* Calcula el desplazamiento de un control en este fotograma. `clasica` es
 * la mascara de seis bits usada por los proyectos antiguos. */
int motor_programa_movimiento(const MotorL3Control *control,
                              const MotorEntrada *entrada_ext,
                              unsigned clasica, int velocidad,
                              int *dx, int *dy);

/* Aplica todos los controles activos a la primera instancia de su actor.
 * Devuelve el numero de entidades que se movieron. */
int motor_programa_aplicar(const MotorL3 *p, const MotorEntrada *entrada_ext,
                           unsigned clasica, const MotorMapa *mapa,
                           int velocidad);

#endif
