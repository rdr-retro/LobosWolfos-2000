/* panel_debug.h - panel visual del depurador en la vista Play (fase 34).
 *
 * Dibuja sobre el framebuffer del juego, con la misma fuente bitmap 5x7
 * que el editor, un resumen del estado del depurador: bloque en ejecucion,
 * variables observadas con su valor y ultimos mensajes de la consola.
 *
 * Solo se dibuja si se activa --panel-debug. Sin ese flag las capturas de
 * las fases anteriores son byte a byte identicas.
 */
#ifndef PANEL_DEBUG_H
#define PANEL_DEBUG_H

#include "motor.h"

void panel_debug_activar(int valor);
int panel_debug_esta_activo(void);

/* Dibuja el panel sobre `fb`. `memoria` es LoboMemoria* y `proyecto`
 * MotorL3*; ambos pueden ser NULL si el juego no cargo un LOGIC3. */
void panel_debug_dibujar(u32 *fb, int ancho, int alto,
                         const void *memoria, const void *proyecto);

#endif /* PANEL_DEBUG_H */
