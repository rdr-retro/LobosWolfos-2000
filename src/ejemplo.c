/* ==========================================================================
 * ejemplo.c - juego de ejemplo para quien empieza (fase 22).
 *
 * El menu AYUDA > CREAR JUEGO DE EJEMPLO llama a `ejemplo_crear_juego`.
 * En un solo paso deja un proyecto jugable sin pedir nada al usuario:
 *
 *   1. Dibuja tres tiles de 32x32 (arbol, agua y roca) y los guarda como
 *      PNG en la carpeta `tiles`.
 *   2. Monta un pequeno claro rodeado de arboles en el mapa
 *      `maps\primer_juego.mav`.
 *   3. Crea la logica con el item de ese mapa y los ajustes del proyecto
 *      (640x480, 60 FPS, fondo verde).
 *   4. Guarda la logica en `logic\primer_juego.logic` y deja el gestor
 *      abierto para que el principiante vea lo que se ha creado.
 *
 * El suelo del claro son celdas vacias: el motor pinta el color de fondo
 * del proyecto y deja pasar al personaje. Los tiles colocados (arboles,
 * agua y rocas) son solidos, asi que sirven de paredes.
 * ========================================================================== */
#include "app.h"

#define EJ_LADO 32

/* ------------------------------------------------------------------ */
/* Dibujo de tiles                                                     */
/* ------------------------------------------------------------------ */
static void ej_poner(u32 *pix, int x, int y, u32 color) {
    if (x < 0 || y < 0 || x >= EJ_LADO || y >= EJ_LADO) return;
    pix[(size_t)y * EJ_LADO + x] = color;
}

static int ej_en_circulo(int x, int y, int cx, int cy, int r) {
    int dx = x - cx;
    int dy = y - cy;
    return dx * dx + dy * dy <= r * r;
}

static int ej_en_elipse(int x, int y, int cx, int cy, int rx, int ry) {
    int dx = x - cx;
    int dy = y - cy;
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;
    return (dx * dx) * ry * ry + (dy * dy) * rx * rx <= rx * rx * ry * ry;
}

static void ej_limpiar(u32 *pix) {
    memset(pix, 0, (size_t)EJ_LADO * EJ_LADO * sizeof(u32));
}

/* Arbol visto desde arriba: copa redonda y tronco asomando por abajo. */
static void ej_tile_arbol(u32 *pix) {
    int x, y;
    ej_limpiar(pix);
    for (y = 19; y < EJ_LADO; y++) {
        for (x = 14; x <= 17; x++)
            ej_poner(pix, x, y, x == 17 ? 0xFF4A2C16u : 0xFF6B4226u);
    }
    for (y = 0; y < EJ_LADO; y++) {
        for (x = 0; x < EJ_LADO; x++) {
            int copa = ej_en_circulo(x, y, 16, 12, 12) ||
                       ej_en_circulo(x, y, 9, 16, 8) ||
                       ej_en_circulo(x, y, 23, 16, 8);
            u32 c;
            if (!copa) continue;
            c = 0xFF2E7D32u;
            if (y > 18) c = 0xFF1B5E20u;
            if (ej_en_circulo(x, y, 12, 9, 6) ||
                ej_en_circulo(x, y, 21, 10, 4))
                c = 0xFF4CAF50u;
            if (ej_en_circulo(x, y, 8, 19, 4) ||
                ej_en_circulo(x, y, 22, 20, 4))
                c = 0xFF1B5E20u;
            ej_poner(pix, x, y, c);
        }
    }
}

/* Agua: azul con ondas claras y un borde mas oscuro. */
static void ej_tile_agua(u32 *pix) {
    int x, y;
    for (y = 0; y < EJ_LADO; y++) {
        for (x = 0; x < EJ_LADO; x++) {
            u32 c = 0xFF2A6DB5u;
            int onda = (y + (x / 7) * 2) % 9;
            if (onda == 0) c = 0xFF4A90D9u;
            else if (onda == 4) c = 0xFF1E4E86u;
            if (x == 0 || y == 0 || x == EJ_LADO - 1 || y == EJ_LADO - 1)
                c = 0xFF163E6Du;
            ej_poner(pix, x, y, c);
        }
    }
}

/* Roca: dos piedras grises con luz arriba y sombra abajo. */
static void ej_tile_roca(u32 *pix) {
    int x, y;
    ej_limpiar(pix);
    for (y = 0; y < EJ_LADO; y++) {
        for (x = 0; x < EJ_LADO; x++) {
            int piedra = ej_en_elipse(x, y, 16, 21, 13, 9) ||
                         ej_en_elipse(x, y, 11, 15, 7, 6) ||
                         ej_en_elipse(x, y, 23, 17, 6, 5);
            u32 c;
            if (!piedra) continue;
            c = 0xFF7D7D7Du;
            if (ej_en_elipse(x, y, 12, 13, 5, 4) ||
                ej_en_elipse(x, y, 22, 16, 4, 3))
                c = 0xFFADADADu;
            if (y > 23) c = 0xFF4F4F4Fu;
            ej_poner(pix, x, y, c);
        }
    }
}

/* Busca el indice del tile recien guardado (nombre con extension). */
static int ej_buscar_tile(const char *archivo) {
    int i;
    for (i = 0; i < g.num_tiles; i++) {
        if (_stricmp(g.tiles[i].nombre, archivo) == 0) return i;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Mapa                                                               */
/* ------------------------------------------------------------------ */
static void ej_rectangulo(int tile, int x0, int y0, int x1, int y1) {
    int x, y;
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            mapa_colocar(x, y, tile);
}

/* Claro de 19x15 celdas (-9..9, -7..7) rodeado de arboles. El personaje
 * aparece en la celda (0,0), en el centro, y puede recorrer el interior
 * vacio; los arboles, el agua y las rocas bloquean el paso. */
static void ej_montar_mapa(int arbol, int agua, int roca) {
    mapa_reiniciar();
    ej_rectangulo(arbol, -10, -8, 10, -8);   /* borde superior */
    ej_rectangulo(arbol, -10, 8, 10, 8);     /* borde inferior */
    ej_rectangulo(arbol, -10, -7, -10, 7);   /* borde izquierdo */
    ej_rectangulo(arbol, 10, -7, 10, 7);     /* borde derecho */

    /* Estanque en la esquina superior derecha. */
    ej_rectangulo(agua, 3, -3, 5, -1);
    ej_rectangulo(agua, 4, -4, 5, -4);

    /* Rocas sueltas y dos grupos de arboles dentro del claro. */
    mapa_colocar(-5, 2, roca);
    mapa_colocar(4, 4, roca);
    mapa_colocar(-2, 5, roca);
    mapa_colocar(6, -5, roca);
    ej_rectangulo(arbol, -7, 4, -6, 5);
    ej_rectangulo(arbol, 7, 5, 8, 6);

    g.mapa.tile_sel = arbol;
    g.mapa_tile_sel = arbol;
}

/* ------------------------------------------------------------------ */
/* Juego de ejemplo                                                    */
/* ------------------------------------------------------------------ */
void ejemplo_crear_juego(void) {
    u32 *tile;
    char carpeta[MAX_PATH];
    char ruta[MAX_PATH];
    char exe[MAX_PATH];
    int arbol, agua, roca;

    tile = (u32 *)calloc((size_t)EJ_LADO * EJ_LADO, sizeof(u32));
    if (!tile) {
        dialogo_error("No hay memoria para crear el juego de ejemplo.");
        return;
    }

    /* 1. Tiles del escenario. */
    ej_tile_arbol(tile);
    if (!tiles_guardar_como(tile, EJ_LADO, EJ_LADO, "ejemplo_arbol")) {
        free(tile);
        dialogo_error("No se pudo guardar el tile del arbol.");
        return;
    }
    ej_tile_agua(tile);
    if (!tiles_guardar_como(tile, EJ_LADO, EJ_LADO, "ejemplo_agua")) {
        free(tile);
        dialogo_error("No se pudo guardar el tile del agua.");
        return;
    }
    ej_tile_roca(tile);
    if (!tiles_guardar_como(tile, EJ_LADO, EJ_LADO, "ejemplo_roca")) {
        free(tile);
        dialogo_error("No se pudo guardar el tile de la roca.");
        return;
    }
    free(tile);

    arbol = ej_buscar_tile("ejemplo_arbol.png");
    agua = ej_buscar_tile("ejemplo_agua.png");
    roca = ej_buscar_tile("ejemplo_roca.png");
    if (arbol < 0 || agua < 0 || roca < 0) {
        dialogo_error("No se pudieron recargar los tiles del ejemplo.");
        return;
    }

    /* 2. Mapa del claro y su ruta en la carpeta maps. */
    ej_montar_mapa(arbol, agua, roca);
    carpeta_maps(carpeta, sizeof(carpeta));
    crear_carpeta(carpeta);
    unir_ruta(ruta, sizeof(ruta), carpeta, "primer_juego.mav");
    if (!mapa_guardar(ruta)) {
        dialogo_error("No se pudo guardar maps\\primer_juego.mav.");
        return;
    }
    recortar_texto(g.mapa_ruta, ruta, sizeof(g.mapa_ruta));
    recortar_texto(g.item_mapa, "primer_juego.mav", sizeof(g.item_mapa));

    /* 3. Logica con un unico item de mapa y ajustes pensados para empezar. */
    logica_iniciar();
    memset(&g.modulos[0], 0, sizeof(g.modulos[0]));
    g.modulos[0].x = 96;
    g.modulos[0].y = 96;
    g.modulos[0].ancho = MODULOS_ELEMENTO_ANCHO;
    g.modulos[0].alto = MODULOS_ELEMENTO_ALTO;
    g.modulos[0].id = 1;
    g.modulos[0].plegado = 0;
    g.modulos[0].funcion = -1;
    recortar_texto(g.modulos[0].mapa, "primer_juego.mav",
                   sizeof(g.modulos[0].mapa));
    g.num_modulos = 1;
    g.modulo_siguiente_id = 2;
    g.pan_x = 32;
    g.pan_y = 32;

    recortar_texto(g.ajustes.nombre, "Primer juego",
                   sizeof(g.ajustes.nombre));
    g.ajustes.ancho = 640;
    g.ajustes.alto = 480;
    g.ajustes.limitar_cuadros = 1;
    g.ajustes.fps = 60;
    g.ajustes.velocidad = 3;
    g.ajustes.fondo = 0x001E5A2Eu; /* verde del claro */

    /* 4. Guardar la logica para poder recuperarla con ARCHIVO > Abrir. */
    obtener_ruta_exe(exe, sizeof(exe));
    unir_ruta(carpeta, sizeof(carpeta), exe, CARPETA_LOGIC_TEXTO);
    crear_carpeta(carpeta);
    unir_ruta(ruta, sizeof(ruta), carpeta, "primer_juego.logic");
    logica_guardar_archivo(ruta);

    if (g.vista != VISTA_MODULOS) app_alternar_gestor_modulos();
    g.menu_abierto = -1;
    app_estado("Juego de ejemplo listo. Pulsa PLAY para probarlo.");
    app_invalidar();
}
