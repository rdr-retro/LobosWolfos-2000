/* inicio.h - vista de arranque del gestor de escenas y nodos. */
#ifndef INICIO_H
#define INICIO_H

#include "proyecto_modelo.h"

/* Servicio CLI de Inicio, compartido con la vista visual. Las ordenes de
 * inicio_ejecutar se escriben como
 *   cargar|ruta;crear|kind|nombre|padre;borrar|id;reparentar|id|padre;
 *   crear_script|nombre|ruta;escribir_script|id|archivo;compilar|id;
 *   asociar|script|destino|evento;asociar_escena|script|destino|evento;
 *   desasociar|id;deshacer;rehacer;volcar|ruta;volcar_scripts|ruta
 * El padre es opcional y 0 significa "sin padre". */
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
/* Fase 6: scripts de texto y asociaciones sobre el mismo historial.
 * escritor_script guarda el texto ya leido en el buffer del editor. */
int inicio_script_crear(Inicio *in, const char *nombre, const char *ruta,
                        char *diagnostico, size_t longitud);
int inicio_script_leer(Inicio *in, int id, char *codigo, size_t tam,
                       char *diagnostico, size_t longitud);
int inicio_script_escribir(Inicio *in, int id, const char *codigo,
                           char *diagnostico, size_t longitud);
int inicio_script_compilar(Inicio *in, int id, char *diagnostico,
                           size_t longitud);
int inicio_asociar(Inicio *in, int id_script, int id_destino, int es_escena,
                   const char *evento, char *diagnostico, size_t longitud);
int inicio_desasociar(Inicio *in, int id, char *diagnostico, size_t longitud);
void inicio_volcar_scripts(const Inicio *in, const char *ruta);
int inicio_ejecutar(Inicio *in, const char *comandos, char *diagnostico,
                    size_t longitud);
int inicio_kind(const char *texto);

void inicio_modelo_cargar(ProjectModel *modelo, const char *ruta);
void inicio_modelo_limpiar(void);
int inicio_modelo_editar(int id, const char *campo, const char *valor,
                         char *diagnostico, size_t longitud);
int inicio_modelo_guardar(const char *ruta, char *diagnostico, size_t longitud);
void inicio_dibujar(void);
void inicio_pulsar(int x, int y, int boton);
void inicio_soltar(int x, int y, int boton);
void inicio_mover(int x, int y, int botones);
void inicio_rueda(int x, int y, int delta, int ctrl);
void inicio_tecla(int vk, int ctrl, int shift);
void inicio_caracter(int ch);
void inicio_vista_volcar(const char *ruta);

#endif
