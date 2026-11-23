/* modelo_log.h - lector del .log actual de ejemplo (plan 16.2).
 *
 * Convierte un .log LOGIC3 con seccion `montaje` en un ProjectModel. No
 * modifica el archivo de entrada, no dibuja y no escribe LOGIC3. Los IDs de
 * los nodos se conservan; el proyecto recibe el primer ID libre posterior y
 * las conexiones se crean al final con `pm_conectar`.
 *
 * El diagnostico incluye el numero de linea cuando el fallo procede del
 * texto. Devuelve NULL si no se pudo convertir.
 */
#ifndef MODELO_LOG_H
#define MODELO_LOG_H

#include "proyecto_modelo.h"

ProjectModel *ml_leer(const char *ruta, char *diagnostico, size_t longitud);

#endif
