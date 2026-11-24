/* runtime_comun.h - reglas del runtime compartidas por preview y game.exe.
 *
 * Aqui viven las piezas que deben comportarse exactamente igual en la
 * vista previa del editor y en el ejecutable: instanciacion del arbol de
 * escena, calculo de la camara, remapeo de variables de entidad, siembra de
 * la memoria de la VM y cola de cambios de escena.
 *
 * C puro: ni windows.h ni malloc. Lo enlazan game.exe y el editor.
 */
#ifndef RUNTIME_COMUN_H
#define RUNTIME_COMUN_H

#include <stdio.h>
#include "lobo.h"
#include "logic3_motor.h"

#define RUNTIME_COLA_ESCENAS_MAX 16

/* Cola FIFO de cambios de escena. Una peticion repetida seguida de la
 * anterior se coalesce: un script que sigue pidiendo la misma escena mientras
 * la transicion se descubre no debe aplicar dos cambios. Los ids distintos
 * se conservan en orden. Si se llena, el contador `descartados` deja
 * constancia en el volcado. */
typedef struct RuntimeColaEscenas {
    int id[RUNTIME_COLA_ESCENAS_MAX];
    int ini;
    int num;
    int descartados;
} RuntimeColaEscenas;

void runtime_cola_iniciar(RuntimeColaEscenas *cola);
int runtime_cola_num(const RuntimeColaEscenas *cola);
int runtime_cola_meter(RuntimeColaEscenas *cola, int id);
int runtime_cola_sacar(RuntimeColaEscenas *cola);

void runtime_var_slots_reiniciar(void);
int runtime_var_slot(const char *nombre);
void runtime_remap_programa_cb(void *ctx, LoboPrograma *programa);
void runtime_memoria_preparar(LoboMemoria *memoria, const MotorL3 *proy);
void runtime_estado_entidades(LoboEstado *st);

int runtime_preparar_entidades(const MotorL3 *proy, int escena);
int runtime_instanciar_escena(const MotorL3 *proy, int escena);

void runtime_camara_centro(const MotorL3 *proy, int ancho, int alto,
                           int *x, int *y);
void runtime_volcar_jerarquia(FILE *f, const MotorL3 *proy);

#endif
