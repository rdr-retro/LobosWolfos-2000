/* prueba.c - banco de pruebas de consola para el nucleo del motor.
 *
 * No forma parte del juego: se compila aparte con
 *   gcc -m32 -std=c99 -O0 nucleo32.s activo.c prueba.c -o prueba.exe ^
 *       -lgdiplus -lgdi32 -luser32 -lkernel32 -static-libgcc
 * y se lanza desde la raiz del repo (para que resuelva maps/ y tiles/)
 * pasando como argumento la ruta de un .mav para probar la carga real:
 *   .\motor\prueba.exe "C:\...\maps\mapa.mav"
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "motor.h"

static u32 fb[64 * 64];
/* sprite 4x4 de prueba. El pixel (0,1) tiene alpha 0: es transparente. */
static u32 sprite[16] = {
    0xFF112233u, 0xFF445566u, 0xFF778899u, 0xFFAABBCCu,
    0x00555555u, 0xFFFFFFFFu, 0xFF00FF00u, 0xFF0000FFu,
    0xFF123456u, 0xFF654321u, 0xFFABCDEFu, 0xFFFEDCBAu,
    0xFF00FFFFu, 0xFFFF00FFu, 0xFFFFFF00u, 0xFF808080u,
};

int main(int argc, char **argv) {
    MotorCelda celdas[2];
    MotorImagen img;
    u32 tile[8 * 8];
    int i, fallos = 0;

    /* --- motor_limpiar --- */
    memset(fb, 0, sizeof(fb));
    motor_limpiar(fb, 64, 64, 0xFF001122u);
    if (fb[0] != 0xFF001122u || fb[64 * 64 - 1] != 0xFF001122u) {
        printf("[FALLO] motor_limpiar\n");
        fallos++;
    } else printf("[ok] motor_limpiar\n");

    /* --- motor_blit: centro --- */
    motor_blit(fb, 64, 64, 30, 30, sprite, 4, 4);
    if (fb[30 * 64 + 30] != 0xFF112233u || fb[33 * 64 + 33] != 0xFF808080u) {
        printf("[FALLO] motor_blit centro: px0=%08X px33=%08X\n",
               fb[30 * 64 + 30], fb[33 * 64 + 33]);
        fallos++;
    } else printf("[ok] motor_blit centro\n");

    /* --- motor_blit con recorte: dx=-2, dy=-1 --- */
    motor_blit(fb, 64, 64, -2, -1, sprite, 4, 4);
    /* destino (0,0) = fuente (2,1) = 0xFF00FF00 */
    if (fb[0] != 0xFF00FF00u) {
        printf("[FALLO] motor_blit recorte sup-izq: %08X (esperado 00FF00)\n",
               fb[0]);
        fallos++;
    } else printf("[ok] motor_blit recorte sup-izq\n");

    /* --- motor_blit_alpha: el pixel transparente se salta --- */
    motor_blit_alpha(fb, 64, 64, 40, 40, sprite, 4, 4);
    if (fb[40 * 64 + 40] != 0xFF112233u) {
        printf("[FALLO] motor_blit_alpha px opaco: %08X\n", fb[40 * 64 + 40]);
        fallos++;
    }
    if (fb[41 * 64 + 40] != 0xFF001122u) { /* fuente (0,1) alpha 0: fondo */        printf("[FALLO] motor_blit_alpha px transparente: %08X\n",
               fb[41 * 64 + 40]);
        fallos++;
    }
    if (fb[41 * 64 + 41] != 0xFFFFFFFFu) {
        printf("[FALLO] motor_blit_alpha px blanco: %08X\n",
               fb[41 * 64 + 41]);
        fallos++;
    }
    if (fb[40 * 64 + 40] == 0xFF112233u && fb[41 * 64 + 41] == 0xFFFFFFFFu &&
        fb[41 * 64 + 40] == 0xFF001122u)
        printf("[ok] motor_blit_alpha\n");

    /* --- motor_aabb --- */
    if (!motor_aabb(0, 0, 10, 10, 5, 5, 10, 10)) { printf("[FALLO] aabb solapa\n"); fallos++; }
    if (motor_aabb(0, 0, 10, 10, 10, 0, 10, 10)) { printf("[FALLO] aabb tangente\n"); fallos++; }
    if (motor_aabb(0, 0, 10, 10, 50, 50, 10, 10)) { printf("[FALLO] aabb lejos\n"); fallos++; }
    if (!motor_aabb(-5, -5, 10, 10, 0, 0, 10, 10)) { printf("[FALLO] aabb negativo\n"); fallos++; }
    printf("[ok] motor_aabb\n");

    /* --- motor_mapa_render con un tile 8x8 de colores --- */
    for (i = 0; i < 64; i++) tile[i] = 0xFF7F3F1Fu + (u32)(i & 7);
    img.pixeles = tile;
    img.ancho = 8;
    img.alto = 8;
    memset(&celdas, 0, sizeof(celdas));
    celdas[0].x = 0;  celdas[0].y = 0;  celdas[0].imagen = 0;
    celdas[1].x = 1;  celdas[1].y = 0;  celdas[1].imagen = 0;
    motor_limpiar(fb, 64, 64, 0xFF001122u);
    motor_mapa_render(fb, 64, 64, 0, 0, 16, celdas, 2, &img);
    if (fb[0] == 0xFF001122u || fb[16] == 0xFF001122u || fb[8 * 64] == 0xFF001122u) {
        printf("[FALLO] motor_mapa_render no pinto las celdas\n");
        fallos++;
    } else printf("[ok] motor_mapa_render\n");

    /* --- mapa_render con camara desplazada --- */
    motor_limpiar(fb, 64, 64, 0xFF001122u);
    motor_mapa_render(fb, 64, 64, 8, 4, 16, celdas, 2, &img);
    if (fb[0] == 0xFF001122u) {
        printf("[FALLO] motor_mapa_render con camara\n");
        fallos++;
    } else printf("[ok] motor_mapa_render camara\n");

    /* --- carga del proyecto real si nos lo piden --- */
    if (argc > 1) {
        MotorMapa m;
        int ok;
        activo_gdiplus_iniciar();
        ok = activo_cargar_mapa(argv[1], &m);
        printf("mapa %s: ok=%d celdas=%d imagenes=%d lado=%d\n", argv[1], ok,
               m.num_celdas, m.num_imagenes, m.lado);
        if (m.num_imagenes > 0)
            printf("  imagen 0: pixeles=%s ancho=%d alto=%d\n",
                   m.imagenes[0].pixeles ? "cargada" : "NULL",
                   m.imagenes[0].ancho, m.imagenes[0].alto);
        mapa_liberar(&m);
        activo_gdiplus_terminar();
    }

    printf(fallos ? "FALLOS: %d\n" : "TODO OK (%d fallos)\n", fallos);
    return fallos ? 1 : 0;
}
