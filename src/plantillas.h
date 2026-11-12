/* plantillas.h - biblioteca de rectangulos de funciones (fase 10).
 *
 * Cada plantilla es una funcion lobo ya escrita que el usuario puede
 * arrastrar al lienzo para empezar a programar sin teclear. El texto
 * vive aqui, en el editor; al soltarla se crea una FuncionCodigo normal
 * que viaja en el .logic como cualquier otra. */
#ifndef PLANTILLAS_H
#define PLANTILLAS_H

/* Numero de plantillas de la biblioteca. */
int plantillas_num(void);

/* Nombre corto de la plantilla `indice` (el que tomara la funcion). */
const char *plantillas_nombre(int indice);

/* Resumen de una linea para el rectangulo del panel. */
const char *plantillas_resumen(int indice);

/* Codigo lobo completo (con saltos de linea) de la plantilla. */
const char *plantillas_codigo(int indice);

/* Comprueba que todas las plantillas compilan con el parser del motor.
 * Devuelve 1 si todas valen; si no, 0 y deja en `indice` la primera que
 * falla y en `error` el mensaje "linea N: motivo". */
int plantillas_validar(int *indice, char *error, int tam_error);

#endif /* PLANTILLAS_H */
