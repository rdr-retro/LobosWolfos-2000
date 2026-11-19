/* hud.h - HUD, texto, UI y dialogos (fase 38).
 *
 * Capa de interfaz que se dibuja sobre el framebuffer del juego, despues del
 * mundo. Con --hud el motor muestra siempre una franja superior delgada con
 * vidas y puntuacion y, cuando hay texto, un panel de dialogo de tres lineas
 * en la parte inferior. Ese panel deja el mundo a la vista en el centro.
 *
 * Los scripts pueden ademas pedir texto, rectangulos, barras, imagenes y
 * botones sueltos a traves de los opcodes 72..80. Las salidas nuevas estan
 * detras de --hud y --volcar-hud: sin esos flags las capturas y las trazas de
 * las fases anteriores son byte a byte identicas.
 */
#ifndef MOTOR_HUD_H
#define MOTOR_HUD_H

#include "motor.h"

#define HUD_DIALOGO_LINEAS 3
#define HUD_TEXTO_MAX      96
#define HUD_ELEMENTOS_MAX  32

enum {
    HUD_ELEM_TEXTO = 0,
    HUD_ELEM_RECT,
    HUD_ELEM_BARRA,
    HUD_ELEM_IMAGEN,
    HUD_ELEM_BOTON
};

/* Un elemento suelo lo registra un script en el fotograma y se descarta al
 * empezar el siguiente. `extra` es la escala en texto y el maximo en barra. */
typedef struct HudElemento {
    int tipo;
    int x, y, w, h;
    int valor, max, extra;
    u32 color;
    int id;
    char texto[HUD_TEXTO_MAX];
    int pulsado;
} HudElemento;

/* Dibujo de una imagen de entity: lo resuelve el juego para no crear una
 * dependencia de hud.c con render.h. */
typedef int (*HudImagenFn)(void *ctx, int id, int x, int y);

void motor_hud_activar(int valor);
int  motor_hud_esta_activo(void);
void motor_hud_limpiar(void);

void motor_hud_vidas(int valor);
void motor_hud_puntos(int valor);
int  motor_hud_leer_vidas(void);
int  motor_hud_leer_puntos(void);

void motor_hud_dialogo(int linea, const char *texto);
void motor_hud_dialogo_tres(const char *a, const char *b, const char *c);
void motor_hud_dialogo_limpiar(void);
int  motor_hud_dialogo_lineas(void);

int  motor_hud_texto(int x, int y, const char *texto, u32 color, int escala);
int  motor_hud_rect(int x, int y, int w, int h, u32 color);
int  motor_hud_barra(int x, int y, int w, int h, int valor, int max,
                     u32 color);
int  motor_hud_imagen(int id, int x, int y);
int  motor_hud_boton(int id, int x, int y, int w, int h,
                     const char *etiqueta);
void motor_hud_plantilla(int cual);
void motor_hud_imagen_callback(HudImagenFn fn, void *ctx);

/* Cierra los flancos del HUD y descarta los elementos del fotograma anterior.
 * `entrada` puede ser NULL en motores sin raton. */
void motor_hud_fotograma(const void *entrada);

void motor_hud_dibujar(u32 *fb, int ancho, int alto);
void motor_hud_volcar(const char *ruta);

#endif /* MOTOR_HUD_H */
