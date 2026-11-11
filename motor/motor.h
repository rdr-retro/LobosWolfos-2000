/* motor.h - interfaz comun del motor egine.
 *
 * egine tiene dos variantes del nucleo en ensamblador (ver PLAN.md):
 *
 *   nucleo32.s  x86 de 32 bits  -> game.exe en Windows 2000/XP (MinGW i686)
 *   nucleo64.s  x86_64          -> kernel loboswolfos (fase 6, pendiente)
 *
 * Esta cabecera define la parte comun: los tipos de datos y los prototipos
 * que el nucleo en ensamblador implementa. La capa de juego (juego.c) solo
 * ve estas funciones, nunca el ensamblador.
 *
 * Formato de pixel de todo el motor: u32 0xAARRGGBB (igual que el editor y
 * que GDI+). El alpha solo se usa como mascara (0 = transparente) en el
 * blit con transparencia; al presentar el framebuffer el byte alpha se
 * ignora.
 */
#ifndef MOTOR_H
#define MOTOR_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

/* ------------------------------------------------------------------ */
/* Tipos de datos                                                      */
/* ------------------------------------------------------------------ */

/* Imagen del juego: una capa de pixeles ya cargada en memoria. */
typedef struct MotorImagen {
    u32 *pixeles; /* NULL = imagen no encontrada, la celda queda hueca */
    int ancho, alto;
} MotorImagen;

/* Celda del mapa: coordenada de rejilla (celdas por unidad) e indice de la
 * imagen en el array de imagenes del proyecto. */
typedef struct MotorCelda {
    int x, y;
    int imagen;
} MotorCelda;

/* Mapa cargado de un .mav: rejilla de celdas con su lado y las imagenes
 * deduplicadas que usan. */
typedef struct MotorMapa {
    MotorCelda *celdas;
    int num_celdas;
    int lado; /* tile_size del .mav */
    MotorImagen *imagenes; /* array deduplicado por nombre de tile */
    int num_imagenes;
} MotorMapa;

/* Entrada empaquetada en bits: una sola u32 con el estado del teclado. */
#define MOTOR_ENT_IZQUIERDA 0x01u
#define MOTOR_ENT_DERECHA   0x02u
#define MOTOR_ENT_ARRIBA   0x04u
#define MOTOR_ENT_ABAJO    0x08u
#define MOTOR_ENT_SALTO    0x10u
#define MOTOR_ENT_CERRAR   0x20u

/* ------------------------------------------------------------------ */
/* Nucleo en ensamblador (cdecl en 32 bits)                            */
/* ------------------------------------------------------------------ */

/* Rellena el framebuffer con un color solido. Copia de 8 en 8 pixeles
 * con MMX. */
void motor_limpiar(u32 *dst, int ancho, int alto, u32 color);

/* Blit opaco: copia la imagen entera en (dx, dy) del framebuffer, con
 * recorte por los bordes. Nucleo del volcado de sprites, MMX. */
void motor_blit(u32 *dst, int dst_w, int dst_h, int dx, int dy,
               const u32 *src, int src_w, int src_h);

/* Blit con transparencia: igual que motor_blit pero saltando los pixeles
 * con alpha 0 (la mascara de los PNG). */
void motor_blit_alpha(u32 *dst, int dst_w, int dst_h, int dx, int dy,
                      const u32 *src, int src_w, int src_h);

/* Render del mapa: recorre las celdas del .mav, descarta las que caen
 * fuera de la camara y estampa cada tile escalado y centrado en su
 * celda de `lado` pixeles (el mismo sello que el editor de mapas). */
void motor_mapa_render(u32 *dst, int dst_w, int dst_h,
                       int cam_x, int cam_y, int lado,
                       const MotorCelda *celdas, int num_celdas,
                       const MotorImagen *imagenes);

/* Colision AABB: 1 si los rectangulos se solapan, 0 si no. */
int motor_aabb(int ax, int ay, int aw, int ah,
               int bx, int by, int bw, int bh);

/* La entrada y el bucle sincronizado dependen del sistema: en Windows
 * (game.exe) se leen con la API de Win32 en juego.c y en el kernel
 * loboswolfos vendran del driver PS/2 en ensamblador (fase 6). Aqui solo
 * esta el formato empaquetado en bits de arriba. */

/* ------------------------------------------------------------------ */
/* Carga del proyecto del juego (activo.c, C comun)                    */
/* ------------------------------------------------------------------ */

/* Carga un mapa .mav (mismo formato que guarda el editor) con sus
 * imagenes resueltas. Devuelve 0 si falla. El resultado se libera con
 * mapa_liberar. */
int activo_cargar_mapa(const char *ruta, MotorMapa *m);

/* Carga un proyecto .logic del gestor y elige el mapa del primer
 * elemento con mapa asociado (las pantallas del juego). Si no hay
 * ninguno, cae a mapa_default. Devuelve la ruta usada en mapa_usado.
 * Acepta LOGIC1 y LOGIC2 (ver abajo las funciones de codigo). */
int activo_cargar_proyecto(const char *ruta_logic, MotorMapa *m,
                           char *mapa_usado, int tam_usado);

/* ------------------------------------------------------------------ */
/* Funciones de codigo lobo del proyecto (LOGIC2, fase 8)              */
/* ------------------------------------------------------------------ */
/* El gestor guarda cada boton FUNCIONES con su id y su codigo lobo
 * (ver lobo.h). El campo `funcion` de un elemento pasa de indice a
 * referencia: -1 = item de mapa, 0 = egine, >= 1 = id de funcion. Estos
 * limites son los mismos del lado del editor (app.h) y del kernel
 * (activo.rs): un proyecto que caben en los tres. */
#define ACTIVO_FUNCIONES_MAX      32  /* botones FUNCIONES por proyecto */
#define ACTIVO_FUNCION_CODIGO_MAX 2048 /* bytes de codigo por funcion */
#define ACTIVO_USADAS_MAX         128 /* una entrada por elemento */

/* Codigo lobo de la funcion con ese id, terminado en 0 (NULL si el
 * proyecto no la tiene). El puntero es del motor y vive hasta cargar
 * otro proyecto. */
const char *activo_funcion_codigo(int id);

/* Rellena `ids` (hasta `max`) con los ids de las funciones que usan
 * los elementos del lienzo, en orden de aparicion: un script por
 * elemento, cada uno con su maquina. Devuelve el total (truncado). */
int activo_funciones_usadas(int *ids, int max);

/* Inicia y apaga GDI+ para poder leer PNG. */
int activo_gdiplus_iniciar(void);
void activo_gdiplus_terminar(void);

/* Lee el proyecto incrustado (pie LGB1) del final del propio
 * ejecutable y lo registra como archivos virtuales: a partir de aqui,
 * la carga del proyecto no toca el disco. Devuelve el numero de
 * archivos virtuales; 0 si el exe no lleva blob. */
int activo_blob_cargar(void);

/* Guarda un framebuffer como BMP de 24 bits (para --captura). */
int activo_guardar_bmp(const char *ruta, const u32 *pixeles,
                       int ancho, int alto);

void mapa_liberar(MotorMapa *m);

#endif /* MOTOR_H */
