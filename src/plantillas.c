/* plantillas.c - rectangulos de funciones listos para arrastrar (fase 10).
 *
 * La idea es que programar el juego empiece por elegir un rectangulo de
 * la biblioteca en vez de escribir todo desde cero. Cada plantilla usa
 * solo el lenguaje lobo de la fase 8 (x, y, vel, fotograma, teclas,
 * choca y cerrar), asi que compila igual en el editor, en game.exe y en
 * el kernel loboswolfos. Al soltarla se copia como una funcion normal:
 * el usuario puede editarla y el .logic la guarda como cualquier otra.
 *
 * Los comentarios van sin acentos (es codigo lobo); los nombres y los
 * resumenes tambien para que el .logic sea texto ASCII estable. */
#include "plantillas.h"

#include "../motor/lobo.h"

typedef struct PlantillaFuncion {
    const char *nombre;
    const char *resumen;
    const char *codigo;
} PlantillaFuncion;

static const PlantillaFuncion g_plantillas[] = {
    {
        "mover con flechas",
        "4 direcciones con choques",
        "# mover en cuatro direcciones\n"
        "si tecla_izquierda & no choca(x - vel, y)\n"
        "  x = x - vel\n"
        "fin\n"
        "si tecla_derecha & no choca(x + vel, y)\n"
        "  x = x + vel\n"
        "fin\n"
        "si tecla_arriba & no choca(x, y - vel)\n"
        "  y = y - vel\n"
        "fin\n"
        "si tecla_abajo & no choca(x, y + vel)\n"
        "  y = y + vel\n"
        "fin\n"
    },
    {
        "mover horizontal",
        "solo izquierda y derecha",
        "# mover a los lados con choques\n"
        "si tecla_izquierda & no choca(x - vel, y)\n"
        "  x = x - vel\n"
        "fin\n"
        "si tecla_derecha & no choca(x + vel, y)\n"
        "  x = x + vel\n"
        "fin\n"
    },
    {
        "salto y gravedad",
        "salta y cae con la gravedad",
        "# salto con gravedad (para plataformas)\n"
        "si choca(x, y + 1)\n"
        "  vy = 0\n"
        "  si tecla_salto\n"
        "    vy = 0 - vel * 2\n"
        "  fin\n"
        "sino\n"
        "  vy = vy + 1\n"
        "fin\n"
        "si no choca(x, y + vy)\n"
        "  y = y + vy\n"
        "sino\n"
        "  vy = 0\n"
        "fin\n"
    },
    {
        "jugador plataformas",
        "flechas, salto y gravedad",
        "# jugador completo de plataformas\n"
        "si tecla_izquierda & no choca(x - vel, y)\n"
        "  x = x - vel\n"
        "fin\n"
        "si tecla_derecha & no choca(x + vel, y)\n"
        "  x = x + vel\n"
        "fin\n"
        "si choca(x, y + 1)\n"
        "  vy = 0\n"
        "  si tecla_salto\n"
        "    vy = 0 - vel * 2\n"
        "  fin\n"
        "sino\n"
        "  vy = vy + 1\n"
        "fin\n"
        "si no choca(x, y + vy)\n"
        "  y = y + vy\n"
        "sino\n"
        "  vy = 0\n"
        "fin\n"
        "si tecla_cerrar\n"
        "  cerrar()\n"
        "fin\n"
    },
    {
        "patrulla",
        "va y viene al chocar",
        "# patrulla automatica de izquierda a derecha\n"
        "si sentido == 0\n"
        "  sentido = 1\n"
        "fin\n"
        "si sentido == 1\n"
        "  si no choca(x + vel, y)\n"
        "    x = x + vel\n"
        "  sino\n"
        "    sentido = 2\n"
        "  fin\n"
        "sino\n"
        "  si no choca(x - vel, y)\n"
        "    x = x - vel\n"
        "  sino\n"
        "    sentido = 1\n"
        "  fin\n"
        "fin\n"
    },
    {
        "vaiven vertical",
        "sube y baja sin parar",
        "# ascensor: sube y baja al chocar\n"
        "si fase == 0\n"
        "  fase = 1\n"
        "fin\n"
        "si fase == 1\n"
        "  si no choca(x, y - vel)\n"
        "    y = y - vel\n"
        "  sino\n"
        "    fase = 2\n"
        "  fin\n"
        "sino\n"
        "  si no choca(x, y + vel)\n"
        "    y = y + vel\n"
        "  sino\n"
        "    fase = 1\n"
        "  fin\n"
        "fin\n"
    },
    {
        "caer con gravedad",
        "cae hasta tocar el suelo",
        "# caida simple hasta el suelo\n"
        "si no choca(x, y + vel)\n"
        "  y = y + vel\n"
        "fin\n"
    },
    {
        "temblor",
        "vibra en el sitio",
        "# temblor de un pixel\n"
        "si fotograma % 2 == 0\n"
        "  x = x + 1\n"
        "sino\n"
        "  x = x - 1\n"
        "fin\n"
    },
    {
        "cerrar con tecla",
        "termina con tecla_cerrar",
        "# cerrar el juego con su tecla\n"
        "si tecla_cerrar\n"
        "  cerrar()\n"
        "fin\n"
    }
};

int plantillas_num(void) {
    return (int)(sizeof(g_plantillas) / sizeof(g_plantillas[0]));
}

const char *plantillas_nombre(int indice) {
    if (indice < 0 || indice >= plantillas_num()) return "";
    return g_plantillas[indice].nombre;
}

const char *plantillas_resumen(int indice) {
    if (indice < 0 || indice >= plantillas_num()) return "";
    return g_plantillas[indice].resumen;
}

const char *plantillas_codigo(int indice) {
    if (indice < 0 || indice >= plantillas_num()) return "";
    return g_plantillas[indice].codigo;
}

int plantillas_validar(int *indice, char *error, int tam_error) {
    LoboPrograma prog;
    char mensaje[LOBO_ERROR_MAX];
    int i;

    if (indice) *indice = -1;
    if (error && tam_error > 0) error[0] = 0;
    for (i = 0; i < plantillas_num(); i++) {
        if (!lobo_parsear(g_plantillas[i].codigo, &prog, mensaje,
                          sizeof(mensaje))) {
            if (indice) *indice = i;
            if (error && tam_error > 0) {
                int j;
                for (j = 0; j + 1 < tam_error && mensaje[j]; j++)
                    error[j] = mensaje[j];
                error[j] = 0;
            }
            return 0;
        }
    }
    return 1;
}
