/* ==========================================================================
 * plantillas.c - juegos nuevos a partir de plantillas (fase 44).
 *
 * Este modulo es nuevo y no sustituye a ejemplo.c: `--ejemplo` sigue creando
 * exactamente el mismo proyecto LOGIC2 que antes. Aqui solo se escriben
 * proyectos LOGIC3 con el escritor de texto del formato, calculando el CRC
 * con `logic3_crc32` para no tocar el lector ni el escritor originales.
 *
 * Todas las plantillas dejan una escena, un personaje, tres objetos de
 * recogida y una meta. La de plataformas, ademas, mueve al jugador con las
 * flechas y comprueba las colisiones, que es el criterio de salida de la
 * fase. El resto comparte el esqueleto con otros nombres y posiciones para
 * servir de punto de partida.
 * ========================================================================== */
#include "logic3.h"

#define PLANTILLAS_TAM 16384

typedef struct {
    const char *id;
    const char *nombre;
    const char *descripcion;
    const char *escena;
    const char *jugador;
    const char *recoge[3];
    const char *meta;
    const char *mensaje;
    int px, py;
    int rx[3], ry[3];
    int mx, my;
} Plantilla;

static const Plantilla PLANTILLAS[PLANTILLAS_TOTAL] = {
    {
        "plataformas", "Plataformas", "Salta, recoge monedas y llega a la meta",
        "Nivel 1", "Jugador", { "Moneda 1", "Moneda 2", "Moneda 3" },
        "Meta", "Has llegado a la meta",
        48, 400, { 144, 288, 432 }, { 376, 320, 376 }, 560, 368
    },
    {
        "shooter", "Shooter", "Esquiva y recoge dianas antes de salir",
        "Sector 1", "Nave", { "Diana 1", "Diana 2", "Diana 3" },
        "Salida", "Sector despejado",
        48, 240, { 176, 320, 464 }, { 120, 352, 200 }, 560, 240
    },
    {
        "puzzle", "Puzzle", "Busca llaves y abre la puerta",
        "Camara 1", "Cubo", { "Llave 1", "Llave 2", "Llave 3" },
        "Puerta", "Puerta abierta",
        48, 384, { 160, 304, 448 }, { 336, 240, 368 }, 560, 384
    },
    {
        "aventura", "Aventura", "Explora, recoge pistas y encuentra la salida",
        "Bosque", "Explorador", { "Pista 1", "Pista 2", "Pista 3" },
        "Salida", "Has encontrado la salida",
        48, 400, { 152, 296, 440 }, { 352, 312, 384 }, 560, 400
    },
    {
        "laberinto", "Laberinto", "Recoge pistas y localiza la salida",
        "Pasillo 1", "Explorador", { "Pista 1", "Pista 2", "Pista 3" },
        "Salida", "Has salido del laberinto",
        48, 416, { 160, 304, 448 }, { 336, 272, 400 }, 560, 416
    }
};

typedef struct {
    char datos[PLANTILLAS_TAM];
    int largo;
} BufPlantilla;

static void bp_anadir(BufPlantilla *b, const char *texto) {
    int n = 0;
    if (!texto) return;
    while (texto[n] && b->largo + n < PLANTILLAS_TAM - 1) {
        b->datos[b->largo + n] = texto[n];
        n++;
    }
    b->largo += n;
    b->datos[b->largo] = 0;
}

static void bp_formato(BufPlantilla *b, const char *formato, ...) {
    char temporal[768];
    va_list args;
    va_start(args, formato);
    vsnprintf(temporal, sizeof(temporal), formato, args);
    va_end(args);
    temporal[sizeof(temporal) - 1] = 0;
    bp_anadir(b, temporal);
}

/* Escapa el codigo lobo para meterlo dentro de un campo entre comillas. */
static void bp_codigo(BufPlantilla *b, const char *codigo) {
    int i;
    for (i = 0; codigo[i]; i++) {
        char par[3];
        char c = codigo[i];
        if (c == '\n') {
            bp_anadir(b, "\\n");
        } else if (c == '"' || c == '\\') {
            par[0] = '\\';
            par[1] = c;
            par[2] = 0;
            bp_anadir(b, par);
        } else {
            par[0] = c;
            par[1] = 0;
            bp_anadir(b, par);
        }
    }
}

static void bp_bloque(BufPlantilla *b, int evento, const char *codigo) {
    bp_formato(b, "bloque %d evento %d padre 0 orden 0 tipo \"codigo.lobo\"\n",
               evento, evento);
    bp_anadir(b, "  campo \"codigo\" \"");
    bp_codigo(b, codigo);
    bp_anadir(b, "\"\n");
}

static void bp_cuerpo(BufPlantilla *b, const Plantilla *p) {
    char codigo[1024];
    int i;

    bp_formato(b, "proyecto \"%s\" 640 480 1 60 3 00112233\n", p->nombre);
    bp_anadir(b, "carpetas \"tiles\" \"maps\" \"trajes\" \"sonidos\"\n");
    bp_formato(b, "escena 1 \"%s\" \"\" 640 480 00112233\n", p->escena);

    bp_formato(b, "objeto 1 \"%s\" escena 1 x %d y %d w 24 h 24 visible 1 capa 0\n",
               p->jugador, p->px, p->py);
    bp_anadir(b, "  variable \"monedas\" 0 0\n");
    bp_anadir(b, "  variable \"ganado\" 0 0\n");
    bp_anadir(b, "  evento 1 fotograma\n");
    bp_anadir(b, "  evento 2 tecla \"derecha\" mantener\n");
    bp_anadir(b, "  evento 3 tecla \"izquierda\" mantener\n");
    bp_anadir(b, "  evento 4 tecla \"arriba\" mantener\n");

    for (i = 0; i < 3; i++) {
        bp_formato(b, "objeto %d \"%s\" escena 1 x %d y %d w 16 h 16 visible 1 capa 0\n",
                   i + 2, p->recoge[i], p->rx[i], p->ry[i]);
        bp_anadir(b, "  traje 0 \"\" 0 0 0 0\n");
    }
    bp_formato(b, "objeto 5 \"%s\" escena 1 x %d y %d w 28 h 28 visible 1 capa 0\n",
               p->meta, p->mx, p->my);
    bp_anadir(b, "  traje 0 \"\" 0 0 0 0\n");

    /* Fotograma: recoge los tres objetos y comprueba la meta. */
    codigo[0] = 0;
    for (i = 0; i < 3; i++) {
        char trozo[160];
        snprintf(trozo, sizeof(trozo),
                 "si toca(%d)\nborrar(%d)\n"
                 "mi.variable(\"monedas\") = mi.variable(\"monedas\") + 1\n"
                 "fin\n", i + 2, i + 2);
        strcat(codigo, trozo);
    }
    strcat(codigo, "si toca(5)\n");
    strcat(codigo, "mi.variable(\"ganado\") = 1\n");
    strcat(codigo, "enviar(\"");
    strcat(codigo, p->mensaje);
    strcat(codigo, "\")\nfin\n");
    bp_bloque(b, 1, codigo);

    bp_bloque(b, 2, "mi.x = mi.x + 3\n");
    bp_bloque(b, 3, "mi.x = mi.x - 3\n");
    bp_bloque(b, 4, "mi.y = mi.y - 4\n");
    bp_anadir(b, "FIN\n");
}

int plantillas_total(void) {
    return PLANTILLAS_TOTAL;
}

const char *plantillas_id(int indice) {
    if (indice < 0 || indice >= PLANTILLAS_TOTAL) return "";
    return PLANTILLAS[indice].id;
}

const char *plantillas_nombre(int indice) {
    if (indice < 0 || indice >= PLANTILLAS_TOTAL) return "";
    return PLANTILLAS[indice].nombre;
}

const char *plantillas_descripcion(int indice) {
    if (indice < 0 || indice >= PLANTILLAS_TOTAL) return "";
    return PLANTILLAS[indice].descripcion;
}

int plantillas_indice(const char *id) {
    int i;
    if (!id) return -1;
    for (i = 0; i < PLANTILLAS_TOTAL; i++)
        if (strcmp(PLANTILLAS[i].id, id) == 0) return i;
    return -1;
}

int plantillas_crear(const char *id, char *ruta, int tam) {
    BufPlantilla b;
    int indice = plantillas_indice(id);
    if (ruta && tam > 0) ruta[0] = 0;
    if (indice < 0) return 0;

    memset(&b, 0, sizeof(b));
    bp_anadir(&b, "LOGIC3\nversion 3\n");
    bp_cuerpo(&b, &PLANTILLAS[indice]);
    bp_formato(&b, "CRC32 %08X\n", logic3_crc32(b.datos, b.largo));

    crear_carpeta("logic");
    if (ruta && tam > 0) {
        recortar_texto(ruta, "logic\\primer_juego.logic3", tam);
    }
    if (!escribir_archivo("logic\\primer_juego.logic3", b.datos, b.largo))
        return 0;
    if (!logic3_cargar_editor("logic\\primer_juego.logic3")) return 0;
    recortar_texto(g.plantilla_creada, "logic\\primer_juego.logic3",
                   sizeof(g.plantilla_creada));
    app_estado("Creado %s", PLANTILLAS[indice].nombre);
    return 1;
}

void plantillas_volcar(const char *ruta) {
    FILE *f;
    int i;
    char creada[MAX_PATH];
    if (!ruta || !ruta[0]) return;
    f = fopen(ruta, "w");
    if (!f) return;
    fprintf(f, "plantillas=%d\n", plantillas_total());
    fprintf(f, "catalogo=");
    for (i = 0; i < plantillas_total(); i++) {
        fprintf(f, "%s%s", i ? ";" : "", plantillas_id(i));
    }
    fprintf(f, "\n");
    fprintf(f, "seleccion=%s\n", plantillas_id(g.juego_nuevo_sel));
    fprintf(f, "jn=%d jn_sel=%d tut=%d tut_paso=%d ayuda=%d\n",
            g.juego_nuevo_visible, g.juego_nuevo_sel,
            g.tutorial_visible, g.tutorial_paso, g.ayuda_def);
    recortar_texto(creada, g.plantilla_creada, sizeof(creada));
    fprintf(f, "creada=%s\n", creada[0] ? creada : "-");
    fclose(f);
}
