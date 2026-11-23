/* modelo_logic3.h - adaptador del ProjectModel a LOGIC3 (plan 16.5).
 *
 * Convierte un ProjectModel ya validado a las estructuras canonicas de
 * `logic3.h` y delega la escritura en `logic3_guardar`. No lee archivos,
 * no dibuja y no toca el lector ni el escritor de LOGIC3. La cabecera del
 * proyecto sale de la entidad PM_KIND_PROYECTO y cada nodo sin conexion
 * sale como un bloque `sistema.pieza` con los mismos campos que produce
 * el lector de la seccion `montaje`. Las entidades PM_KIND_CONEXION no
 * crean bloques: sobrescriben el campo `enlace` de la pieza de origen.
 */
#ifndef MODELO_LOGIC3_H
#define MODELO_LOGIC3_H

#include "proyecto_modelo.h"

/* Devuelve 1 y escribe `ruta` con LOGIC3 canonico. Si falla devuelve 0 y
 * deja el motivo en `diagnostico`. */
int ml3_generar(const ProjectModel *m, const char *ruta,
                char *diagnostico, size_t longitud);

#endif
