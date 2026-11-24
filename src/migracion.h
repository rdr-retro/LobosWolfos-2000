/* migracion.h - importacion del gestor visual antiguo a Inicio.
 *
 * La migracion lee el archivo original con los lectores existentes, escribe
 * una copia de seguridad byte a byte y genera un LOG 2 jerarquico con escenas,
 * nodos, scripts externos y asociaciones. El archivo de entrada no se toca.
 *
 * `entrada` admite un .log con seccion `montaje`, un LOG 2 ya jerarquico o un
 * LOGIC3 con bloques `sistema.*`. `salida` es el LOG 2 que abre Inicio.
 * `copia` e `informe` son opcionales; si `copia` esta vacia se usa
 * `entrada.bak`. El informe enumera cada conversion y cada aviso.
 */
#ifndef MIGRACION_H
#define MIGRACION_H

#include <stddef.h>

int migrar_visual(const char *entrada, const char *salida,
                  const char *copia, const char *informe,
                  char *diagnostico, size_t longitud);

#endif /* MIGRACION_H */
