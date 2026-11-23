/* inicio.h - servicio de la vista Inicio (plan 17, apartado 1).
 *
 * Un ProjectModel con historial de snapshots. Crear, borrar y reparentar
 * pasan por aqui, asi que deshacer y rehacer cubren las tres operaciones
 * igual. El servicio no dibuja, no abre ventanas y no escribe LOGIC3: la
 * vista visual se conectara despues a este mismo contrato.
 *
 * Las ordenes de inicio_ejecutar se escriben como
 *   cargar|ruta;crear|kind|nombre|padre;borrar|id;reparentar|id|padre;
 *   deshacer;rehacer;volcar|ruta
 * El padre es opcional y 0 significa "sin padre". Los volcados se pueden
 * intercalar para congelar estados intermedios en las pruebas.
 */
#ifndef INICIO_H
#define INICIO_H

#include "proyecto_modelo.h"

#define INICIO_HISTORIAL_MAX 16

typedef struct {
    ProjectModel modelo;
    ProjectModel pasado[INICIO_HISTORIAL_MAX];
    ProjectModel futuro[INICIO_HISTORIAL_MAX];
    int num_pasado;
    int num_futuro;
    char ultimo[PM_ERROR_MAX];
    char origen[PM_RUTA_MAX];
} Inicio;

/* Instancia compartida: cada snapshot pesa unos cientos de KB, asi que vive
 * en datos estaticos y nunca en la pila. */
Inicio *inicio_servicio(void);
void inicio_iniciar(Inicio *in);

int inicio_cargar(Inicio *in, const char *ruta, char *diagnostico,
                  size_t longitud);
int inicio_crear(Inicio *in, int kind, const char *nombre, int id_padre);
int inicio_borrar(Inicio *in, int id);
int inicio_reparentar(Inicio *in, int id, int id_padre);
int inicio_deshacer(Inicio *in);
int inicio_rehacer(Inicio *in);
void inicio_volcar(const Inicio *in, const char *ruta);
int inicio_ejecutar(Inicio *in, const char *comandos, char *diagnostico,
                    size_t longitud);
int inicio_kind(const char *texto);

#endif
