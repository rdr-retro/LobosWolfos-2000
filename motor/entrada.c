/* ==========================================================================
 * Fase 37: entrada extendida del runtime.
 *
 * Treinta y dos ranuras de teclado con flancos (pulsada, mantenida,
 * soltada) y estado del raton (posicion, botones, rueda y dentro/fuera).
 * Se usa desde game.exe y desde la vista previa del editor. No toca la
 * palabra de 6 bits del formato clasico ni los opcodes existentes.
 * ========================================================================== */
#include "entrada.h"
#include <stdio.h>
#include <string.h>

static int igual(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

int motor_entrada_slot_letra(char ch) {
    if (ch >= 'a' && ch <= 'z') return ENT_A + (ch - 'a');
    if (ch >= 'A' && ch <= 'Z') return ENT_A + (ch - 'A');
    return -1;
}

int motor_entrada_slot(const char *nombre) {
    if (!nombre || !nombre[0]) return -1;
    if (igual(nombre, "izquierda")) return ENT_IZQUIERDA;
    if (igual(nombre, "derecha")) return ENT_DERECHA;
    if (igual(nombre, "arriba")) return ENT_ARRIBA;
    if (igual(nombre, "abajo")) return ENT_ABAJO;
    if (igual(nombre, "espacio") || igual(nombre, "salto")) return ENT_ESPACIO;
    if (igual(nombre, "enter") || igual(nombre, "intro")) return ENT_ENTER;
    if (nombre[1] == 0) return motor_entrada_slot_letra(nombre[0]);
    return -1;
}

void motor_entrada_iniciar(MotorEntrada *e) {
    if (!e) return;
    memset(e, 0, sizeof(*e));
}

void motor_entrada_poner(MotorEntrada *e, int slot, int pulsado) {
    unsigned bit;
    if (!e || slot < 0 || slot >= MOTOR_TECLAS_MAX) return;
    bit = 1u << slot;
    if (pulsado) e->teclas |= bit;
    else e->teclas &= ~bit;
}

/* Fija el estado sostenido desde una lista de nombres separada por comas.
 * La mascara anterior queda igual, de modo que las teclas listadas se
 * cuentan como pulsadas en este fotograma y mantenidas en los siguientes. */
void motor_entrada_fijar(MotorEntrada *e, const char *nombres) {
    unsigned mascara = 0;
    const char *p = nombres;
    if (!e) return;
    if (p) {
        while (*p) {
            char nombre[32];
            int n = 0;
            int slot;
            while (*p && *p != ',' && *p != ' ' && n < (int)sizeof(nombre) - 1)
                nombre[n++] = *p++;
            nombre[n] = 0;
            slot = motor_entrada_slot(nombre);
            if (slot >= 0) mascara |= 1u << slot;
            while (*p == ',' || *p == ' ') p++;
        }
    }
    e->anterior = e->teclas;
    e->teclas = mascara;
}

/* Cierra el fotograma: la mascara actual pasa a ser la anterior. */
void motor_entrada_siguiente(MotorEntrada *e) {
    if (!e) return;
    e->anterior = e->teclas;
}

static unsigned bito(const MotorEntrada *e, int slot) {
    if (!e || slot < 0 || slot >= MOTOR_TECLAS_MAX) return 0;
    return 1u << slot;
}

int motor_entrada_pulsada(const MotorEntrada *e, int slot) {
    unsigned b = bito(e, slot);
    return b != 0 && (e->teclas & b) != 0 && (e->anterior & b) == 0;
}

int motor_entrada_mantenida(const MotorEntrada *e, int slot) {
    unsigned b = bito(e, slot);
    return b != 0 && (e->teclas & b) != 0;
}

int motor_entrada_soltada(const MotorEntrada *e, int slot) {
    unsigned b = bito(e, slot);
    return b != 0 && (e->teclas & b) == 0 && (e->anterior & b) != 0;
}

int motor_entrada_raton_boton(const MotorEntrada *e, int boton) {
    if (!e || boton < 1 || boton > 3) return 0;
    return (e->raton_botones & (1u << (boton - 1))) != 0;
}

void motor_entrada_volcar(const MotorEntrada *e, const char *ruta) {
    FILE *f;
    int i;
    if (!ruta || !ruta[0]) return;
    f = fopen(ruta, "w");
    if (!f) return;
    fprintf(f, "teclas %u\n", e ? e->teclas : 0u);
    fprintf(f, "anterior %u\n", e ? e->anterior : 0u);
    fprintf(f, "raton_x %d\n", e ? e->raton_x : 0);
    fprintf(f, "raton_y %d\n", e ? e->raton_y : 0);
    fprintf(f, "raton_dx %d\n", e ? e->raton_dx : 0);
    fprintf(f, "raton_dy %d\n", e ? e->raton_dy : 0);
    fprintf(f, "raton_rueda %d\n", e ? e->raton_rueda : 0);
    fprintf(f, "raton_botones %u\n", e ? e->raton_botones : 0u);
    fprintf(f, "raton_dentro %d\n", e ? e->raton_dentro : 0);
    for (i = 0; i < MOTOR_TECLAS_MAX; i++) {
        fprintf(f, "slot %d mantenida %d pulsada %d soltada %d\n", i,
                motor_entrada_mantenida(e, i),
                motor_entrada_pulsada(e, i),
                motor_entrada_soltada(e, i));
    }
    fclose(f);
}
