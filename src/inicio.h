/* inicio.h - vista de arranque del gestor de escenas y nodos. */
#ifndef INICIO_H
#define INICIO_H

#include "proyecto_modelo.h"

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
void inicio_volcar(const char *ruta);

#endif
