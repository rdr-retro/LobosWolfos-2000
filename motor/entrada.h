/* entrada.h - estado de entrada extendido (fase 37).
 *
 * El formato clasico sigue siendo la palabra de 6 bits de motor.h. Este
 * modulo anade 32 ranuras de teclado con flancos y el estado del raton.
 * todo es memoria del editor y no se serializa en .logic ni en .logic3.
 */
#ifndef MOTOR_ENTRADA_H
#define MOTOR_ENTRADA_H

#define MOTOR_TECLAS_MAX 32

/* Ranuras de las 32 teclas configurables. Las seis primeras conservan el
 * orden de MOTOR_ENT_*, y del slot 6 al 31 van las letras a..z. */
enum {
    ENT_IZQUIERDA = 0,
    ENT_DERECHA,
    ENT_ARRIBA,
    ENT_ABAJO,
    ENT_ESPACIO,
    ENT_ENTER,
    ENT_A = 6
};

typedef struct MotorEntrada {
    unsigned teclas;        /* mascara de las 32 ranuras pulsadas ahora */
    unsigned anterior;      /* mascara del fotograma anterior, para flancos */
    int raton_x, raton_y;   /* posicion en pixeles de pantalla */
    int raton_dx, raton_dy; /* desplazamiento desde el ultimo fotograma */
    int raton_rueda;        /* pasos de rueda acumulados */
    unsigned raton_botones; /* bit 0 izquierdo, 1 derecho, 2 medio */
    int raton_dentro;       /* 1 si el cursor esta sobre la ventana */
} MotorEntrada;

void motor_entrada_iniciar(MotorEntrada *e);
int  motor_entrada_slot(const char *nombre);
int  motor_entrada_slot_letra(char ch);
void motor_entrada_poner(MotorEntrada *e, int slot, int pulsado);
void motor_entrada_fijar(MotorEntrada *e, const char *nombres);
void motor_entrada_siguiente(MotorEntrada *e);
int  motor_entrada_pulsada(const MotorEntrada *e, int slot);
int  motor_entrada_mantenida(const MotorEntrada *e, int slot);
int  motor_entrada_soltada(const MotorEntrada *e, int slot);
int  motor_entrada_raton_boton(const MotorEntrada *e, int boton);
void motor_entrada_volcar(const MotorEntrada *e, const char *ruta);

#endif
