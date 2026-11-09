/* ==========================================================================
 * LobosWolfos 2000 - editor de texturas para Windows 2000/XP.
 *
 * Interfaz Win32/GDI dibujada a mano sobre un buffer de 32 bits. Compatible
 * con Windows XP (WINVER 0x0501).
 * ========================================================================== */
#ifndef TEXTURE_PAINT_H
#define TEXTURE_PAINT_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>

#include "recurso.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TP_NOMBRE    "LobosWolfos 2000"
#define TP_VERSION   "1.0 beta"

typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;

/* ------------------------------------------------------------------ */
/* Colores (0xAARRGGBB)                                                */
/* ------------------------------------------------------------------ */
#define FONDO            0x00101820u
#define LIENZO           0x00FFFFFFu
#define TRAZO            0x00202020u
#define SIDEBAR          0x00C9C9C9u
#define SEPARADOR        0x00909090u
#define BOTON_SELECCION  0x003278E0u

#define BARRA            0x00DCDCDCu
#define BARRA_BORDE      0x00A0A0A0u
#define TEXTO_UI         0x00303030u
#define TEXTO_CLARO      0x00FFFFFFu

#define MENU_BARRA_FONDO     0x00E4E4E4u
#define MENU_BARRA_TEXTO     0x00303030u
#define MENU_BARRA_HOVER     0x003278E0u
#define MENU_BARRA_HOVER_TXT 0x00FFFFFFu

/* Pestanas estilo navegador clasico: trapecio con esquinas superiores
 * redondeadas y curvas concavas en la base. */
#define TAB_BARRA_FONDO      0x00B9C2CCu
#define TAB_BARRA_BORDE      0x00808C9Au
#define TAB_BARRA_LUZ        0x00D8E0E8u
#define TAB_ACTIVA_FONDO     0x00FFFFFFu
#define TAB_INACTIVA_FONDO   0x00C3CCD6u
#define TAB_HOVER_FONDO      0x00D3DCE6u
#define TAB_ACTIVA_TEXTO     0x00181818u
#define TAB_INACTIVA_TEXTO   0x00304050u
#define TAB_CERRAR_HOVER     0x00C94A4Au
#define TAB_ACTIVA_BORDE     0x00708090u
#define TAB_INACTIVA_BORDE   0x0090A0B0u
#define TAB_FAVICON_A        0x00647C94u
#define TAB_FAVICON_B        0x00DCE6F0u

#define MANGO            0x004090F0u
#define MANGO_BORDE      0x00205080u
#define MANGO_ACTIVO     0x00F08030u

#define MINI_VACIO       0x00ADADADu
#define ZOOM_PISTA       0x00B4B4B4u
#define ZOOM_RELLENO     0x003278E0u
#define ZOOM_KNOB        0x00FFFFFFu
#define ZOOM_TEXTO       0x00303030u
#define ZOOM_HOVER       0x00C4C4C4u
#define ZOOM_APAGADO     0x00909090u

#define MENU_FONDO       0x00F4F4F4u
#define MENU_BORDE       0x00808080u
#define MENU_HOVER       0x00B4B4B4u
#define MENU_TEXTO       0x00303030u

#define MAPA_FONDO       0x00182430u
#define MAPA_CELDA       0x0022303Eu
#define MAPA_REJILLA     0x00344658u
#define MAPA_EJE         0x00E8A040u
#define MAPA_HOVER       0x00FFC860u
#define MAPA_TEXTO       0x00D8E2ECu
#define MAPA_PANEL       0x00101A24u

/* ------------------------------------------------------------------ */
/* Medidas de la interfaz                                              */
/* ------------------------------------------------------------------ */
#define ANCHO_INICIAL     1200
#define ALTO_INICIAL      700

#define MENU_BARRA_ALTO   36
#define TAB_BARRA_ALTO    34
#define AREA_SUP_ALTO     (TAB_BARRA_ALTO + MENU_BARRA_ALTO)
#define BARRA_ALTO        60
#define BARRA_ABAJO_ALTO  32

#define SIDEBAR_MARGEN    6
#define BOTON_LADO        32
#define HUECO             3
#define COLUMNAS          2
#define SIDEBAR_ANCHO     (SIDEBAR_MARGEN * 2 + BOTON_LADO * COLUMNAS + HUECO * (COLUMNAS - 1))

#define TILE_MINI_LADO    32
#define TILE_MINI_HUECO   4
#define CARPETA_TILES_TEXTO "tiles"
#define TILES_SELECTOR_COLUMNAS 4
#define TILES_SELECTOR_FILAS    4
#define TILES_SELECTOR_VISIBLES 16
#define TILES_SCROLL_ANCHO      8
#define TILES_SCROLL_HUECO      6
#define TILES_REJILLA_ANCHO \
    (TILES_SELECTOR_COLUMNAS * TILE_MINI_LADO + (TILES_SELECTOR_COLUMNAS - 1) * TILE_MINI_HUECO)
#define TILES_REJILLA_ALTO \
    (TILES_SELECTOR_FILAS * TILE_MINI_LADO + (TILES_SELECTOR_FILAS - 1) * TILE_MINI_HUECO)
#define TILES_SELECTOR_TITULO_ALTO 24
#define TILES_SELECTOR_MARGEN_INFERIOR 8
#define TILES_SELECTOR_ALTO \
    (TILES_SELECTOR_TITULO_ALTO + TILES_REJILLA_ALTO + TILES_SELECTOR_MARGEN_INFERIOR)
#define CAPAS_TITULO_ALTO 24
#define CAPA_FILA_ALTO    22
#define CAPA_FILA_HUECO   2
#define CAPA_OJO_LADO     16
#define CAPA_ALFA_LADO    14
#define MENU_CAPA_ANCHO   136
#define MENU_CAPA_OPCION_ALTO 24
#define MENU_CAPA_MARGEN  4
#define CONFIRMAR_ANCHO   320
#define CONFIRMAR_ALTO    132
#define PANEL_DERECHO_MARGEN_INFERIOR 12

/* Gestor de escenas: lienzo desplazable, boton de insercion y escenas. */
#define MODULOS_MAX            128
#define MODULOS_CUADRICULA     32
#define MODULOS_ELEMENTO_ANCHO 96
#define MODULOS_ELEMENTO_ALTO  64
#define MODULOS_BOTON_ALTO     56
#define MODULOS_IMAN           12
#define MODULOS_REJILLA        0x001A2430u
#define MODULOS_EJE            0x00304050u

/* Escenas: cada una guarda la configuracion de rectangulos del lienzo. */
#define ESCENAS_MAX            16
#define ESCENA_FILA_ALTO       24
#define ESCENA_FILA_HUECO      2
#define ESCENA_TITULO_ALTO     24
#define ESCENA_NUEVA_ALTO      24

/* Lista de elementos del lienzo, encima de la lista de escenas. */
#define ELEMENTO_TITULO_ALTO   22
#define ELEMENTO_FILA_ALTO     20
#define ELEMENTO_FILA_HUECO    2
#define ELEMENTOS_VISIBLES     6

#define SIDEBAR_DERECHO_ANCHO \
    (((SIDEBAR_MARGEN * 2 + TILES_REJILLA_ANCHO + TILES_SCROLL_HUECO + TILES_SCROLL_ANCHO) > (SIDEBAR_ANCHO * 2)) \
        ? (SIDEBAR_MARGEN * 2 + TILES_REJILLA_ANCHO + TILES_SCROLL_HUECO + TILES_SCROLL_ANCHO) \
        : (SIDEBAR_ANCHO * 2))

#define MINI_FILAS        3
#define MINI_COLUMNAS     10
#define MINI_TOTAL        30
#define MINI_PERSONALIZADOS 20
#define MINI_LADO         12
#define MINI_HUECO        5
#define MINI_SEPARACION   10

#define ARCOIRIS_LADO     40

#define CUADRO_LADO       26
#define CUADRO_DESFASE    10
#define CUADRO_MARGEN     10

#define GUARDAR_TILE_LADO 40

#define MANGO_TAM         6
#define TOLERANCIA_MANGO  4
#define MIN_LADO          4
#define LIENZO_INICIAL    80
#define LIENZO_MAX        4096

#define ZOOM_MARGEN       8
#define ZOOM_BOTON        18
#define ZOOM_HUECO        6
#define ZOOM_PISTA_ANCHO  160
#define ZOOM_PISTA_ALTO   20
#define ZOOM_KNOB_ANCHO   12
#define ZOOM_KNOB_ALTO    16
#define ZOOM_ETIQUETA_ANCHO 34

#define GROSOR_PISTA_ANCHO 100
#define GROSOR_ETIQUETA_ANCHO 32
#define GROSOR_SEPARACION 18

#define TEXTURA_DIAMETRO  56
#define TEXTURA_MARGEN    10
#define TEXTURA_BORDE     0x00808080u
#define TEXTURA_RESALTE   BOTON_SELECCION

/* Menu cuadrado de texturas, a la derecha del circulo. */
#define MENU_HUECO          16
#define MENU_COLUMNAS       3
#define MENU_HUECO_MUESTRA  6
#define MUESTRA_DIAMETRO    32
#define MENU_LADO (6 * 2 + MENU_COLUMNAS * MUESTRA_DIAMETRO + \
                   MENU_HUECO_MUESTRA * (MENU_COLUMNAS - 1))

/* Tonos de las muestras de textura. */
#define TEXTURA_CLARA     0x00787878u
#define TEXTURA_MEDIA     0x004C4C4Cu
#define TEXTURA_BASE      TRAZO
#define TEXTURA_FONDO     0x00F4F4F4u

#define AJUSTES_MARGEN        10
#define AJUSTES_ANCHO         (SIDEBAR_ANCHO - SIDEBAR_MARGEN * 2)
#define AJUSTES_NOMBRE_ALTO   16
#define AJUSTES_ETIQUETA_ALTO 7

/* Ventana flotante de opciones (620x128). Comparte aspecto con el resto de
 * ventanas flotantes. */
#define OPCIONES_ANCHO        620
#define OPCIONES_ALTO         128
#define OPCIONES_MARGEN       8
#define OPCIONES_PADDING      8
#define OPCIONES_TITULO_ALTO  22
#define OPCIONES_FILA_ALTO    20
#define OPCIONES_PISTA_X      120
#define OPCIONES_PISTA_ANCHO  460
#define OPCIONES_PISTA_ALTO   16
#define OPCIONES_MIN_LADO     16
#define OPCIONES_VENTANA_MIN_ALTO   20

/* Opciones del cubo (bote de pintura). */
#define CUBO_FILA_ALTO     26
#define CUBO_CONTROL_ALTO  18
#define CUBO_CAJA          9
#define CUBO_HUECO         8
#define CUBO_PISTA_X       120
#define CUBO_PISTA_ANCHO   460

#define CUBO_CTRL_OPACIDAD  0
#define CUBO_CTRL_UMBRAL    1
#define CUBO_CTRL_TIPO      10   /* +0..2 */
#define CUBO_CTRL_AREA      20   /* +0..2 */
#define CUBO_CTRL_BUSQUEDA  30   /* +0..3 */
#define CUBO_CTRL_COMPUESTO 40

#define CUBO_TIPO_PP      0
#define CUBO_TIPO_FD      1
#define CUBO_TIPO_PATRON  2

#define CUBO_AREA_SELECCION 0
#define CUBO_AREA_SIMILARES 1
#define CUBO_AREA_ARTE      2
#define OPCIONES_VENTANA_MIN_HUECO  6
#define OPCIONES_NUM_FILAS    5
#define OPCIONES_RESALTE      0x00B4B4B4u
#define OPCIONES_FONDO        0x00C8C8C8u
#define OPCIONES_SOMBRA       0x00A0A0A0u
#define OPCIONES_APAGADO      0x00606060u

/* Modo tiles: cuadricula de lienzos independientes. */
#define TILES_MODO_MAX        128
#define TILES_HUECO           18   /* separacion entre tiles (cabe el boton "+") */
#define TILES_HUECO_VISTA     6    /* separacion reducida dentro del visor */
#define TILES_BOTON           14   /* lado de los botones "+" de cada cara */
#define TILES_BORDE           0x00808080u
#define TILES_BORDE_ACTIVO    0x003278E0u
#define TILES_FONDO           0x00101820u
#define TILES_PANEL_FONDO     0x00C8C8C8u
#define TILES_PANEL_BORDE     0x00808080u
#define TILES_SOMBRA          0x00A0A0A0u
#define TILES_TEXTO           0x00303030u
#define TILES_RESALTE         0x00B4B4B4u
#define TILES_VISTA_ANCHO     460
#define TILES_VISTA_ALTO      360
#define TILES_VISTA_TITULO    22
#define TILES_VISTA_PADDING   8
#define TILES_VISTA_MARGEN    8
#define TILES_MIN_LADO        16

/* Direcciones de los botones "+" de un tile. */
#define TILES_DIR_ARRIBA      0
#define TILES_DIR_DERECHA     1
#define TILES_DIR_ABAJO       2
#define TILES_DIR_IZQUIERDA   3

#define MAPA_LADO_INICIAL 32
#define MAPA_LADO_MIN     8
#define MAPA_LADO_MAX     128
#define MAPA_LADO_PASO    8
#define MAPA_RUEDA_PASO   48

#define MAX_CAPAS         32
#define MAX_TILES         256
#define MAX_CELDAS        8192
#define MAX_UNDO          40
#define MAX_TEXTO         32768
#define MAX_PESTANAS      12

#define TAB_MARGEN        6
#define TAB_HUECO         3
#define TAB_ANCHO_MIN     92
#define TAB_ANCHO_MAX     230
#define TAB_CERRAR_LADO   16
#define TAB_NUEVO_LADO    24
#define TAB_LADO_INSET    4
#define TAB_RADIO_SUP     6

/* Herramienta de texto: medidas y margenes. */
#define TEXTO_MARGEN                 2
#define TEXTO_ANCHO_MIN              160
#define TEXTO_ALTO_MIN               32
#define TEXTO_TAMANO_POR_DEFECTO     12
#define TEXTO_FUENTE_POR_DEFECTO     0
#define TEXTO_MENU_ALTO              24
#define TEXTO_MENU_PADDING           6
#define TEXTO_MENU_HUECO             5
#define TEXTO_MENU_BOTON             18
#define TEXTO_MENU_SEPARACION_CAJA   6
#define TEXTO_MENU_COLOR_LADO        18
#define TEXTO_MENU_ITEM_ALTO         18
#define TEXTO_MENU_FLECHA            8
#define TEXTO_FUENTES_TOTAL          6
#define TEXTO_TAMANOS_TOTAL          18

/* ------------------------------------------------------------------ */
/* Zonas                                                               */
/* ------------------------------------------------------------------ */
typedef struct Zona {
    int x, y, ancho, alto;
} Zona;

static __inline int zona_derecha(Zona z) { return z.x + z.ancho; }
static __inline int zona_abajo(Zona z) { return z.y + z.alto; }
static __inline int zona_contiene(Zona z, int x, int y) {
    return x >= z.x && y >= z.y && x < z.x + z.ancho && y < z.y + z.alto;
}
static __inline Zona zona_hacer(int x, int y, int ancho, int alto) {
    Zona z; z.x = x; z.y = y; z.ancho = ancho; z.alto = alto; return z;
}
static __inline Zona zona_vacia(void) { return zona_hacer(0, 0, 0, 0); }

typedef struct Lienzo {
    int x0, y0, x1, y1;
} Lienzo;

/* ------------------------------------------------------------------ */
/* Herramientas                                                        */
/* ------------------------------------------------------------------ */
typedef enum {
    H_LAPIZ = 0,
    H_PINCEL,
    H_CUBO,
    H_AEROGRAFO,
    H_BORRADOR,     /* "draft": pinta con el color de fondo */
    H_TEXTO,
    H_CUENTAGOTAS,
    H_LUPA,
    H_LINEA,
    H_CURVA,
    H_CIRCULO,
    H_POLIGONO,
    H_RECTANGULO,
    H_RECT_REDONDO,
    H_SEL_LIBRE,
    H_SEL_RECT,
    H_TOTAL
} Herramienta;

typedef enum {
    TEXTURA_CERDA = 0,
    TEXTURA_ACUARELA,
    TEXTURA_TINTA,
    TEXTURA_SECO,
    TEXTURA_AEROGRAFO,
    TEXTURA_TOTAL
} Textura;

typedef enum {
    ROCIO_FINA = 0,
    ROCIO_MEDIA,
    ROCIO_GRUESA,
    ROCIO_DISPERSA,
    ROCIO_GOTEO,
    ROCIO_TOTAL
} Rocio;

typedef enum {
    RELLENO_CONTIGUO = 0,
    RELLENO_SIMILAR,
    RELLENO_LINEAL
} ModoRelleno;

/* ------------------------------------------------------------------ */
/* Documento y capas                                                   */
/* ------------------------------------------------------------------ */
typedef struct Capa {
    u32 *pixeles;      /* ancho*alto ARGB; alfa 0 = transparente */
    int visible;
    int alfa;          /* 1 = conserva canal alfa; 0 = opaca */
    int opacidad;      /* 0..255 */
    char nombre[64];
} Capa;

typedef struct Documento {
    int ancho, alto;
    int num_capas;
    int activa;
    Capa capas[MAX_CAPAS];
    u32 fondo;
    int modificado;
    u32 *compuesto;
    int compuesto_sucio;
} Documento;

typedef struct Foto {
    int ancho, alto, num_capas, activa;
    Capa capas[MAX_CAPAS];
} Foto;

/* Tile inactivo dentro de una instantanea: celda y copia profunda. */
typedef struct FotoTile {
    int cx, cy;
    Foto foto;
} FotoTile;

/* Fotografia completa del estado editable: documento activo, resto de tiles,
 * celda activa, modo tiles y zoom. Equivale a `Instantanea`. */
typedef struct Instantanea {
    Foto dibujo;
    FotoTile tiles[TILES_MODO_MAX];
    int num_tiles;
    int tile_cx, tile_cy;
    int modo_tiles;
    int zoom_idx;
} Instantanea;

typedef struct PilaUndo {
    Instantanea *estados;
    int num;
    int capacidad;
    size_t bytes;
} PilaUndo;

/* ------------------------------------------------------------------ */
/* Seleccion                                                           */
/* ------------------------------------------------------------------ */
typedef struct Seleccion {
    int activa;
    int tipo;          /* 0 = rectangular, 1 = libre */
    int x0, y0, x1, y1;   /* caja en coordenadas del documento */
    u8 *mascara;          /* ancho*alto, 1 = seleccionado */
    int mover;            /* arrastrando contenido */
    int redimensionando;  /* tirador activo (-1 ninguno) */
    int dx, dy;           /* desplazamiento del contenido flotante */
    u32 *flotante;        /* pixeles levantados (ancho*alto) */
    int flotante_w, flotante_h;
    int flotante_x, flotante_y;
} Seleccion;

/* ------------------------------------------------------------------ */
/* Texto                                                               */
/* ------------------------------------------------------------------ */
/* Una linea ya colocada del texto. `inicio` y `fin` son indices de unidad
 * UTF-16 en el contenido (fin exclusivo, sin incluir el salto de linea) y
 * `anchos` guarda la coordenada x de cada frontera de caracter, para colocar
 * el cursor. */
typedef struct LineaTexto {
    int inicio, fin;
    int *anchos;
    int num_anchos;
} LineaTexto;

/* Texto rasterizado a pixeles logicos. La cobertura (0..=255) se compone con
 * el color elegido al previsualizar y al confirmar. */
typedef struct TextoRaster {
    int ancho, alto, alto_linea;
    u8 *cobertura;
    LineaTexto *lineas;
    int num_lineas;
} TextoRaster;

/* Datos guardados al agarrar un tirador de la caja de texto: desde que borde
 * se empezo y donde estaba el puntero, para que la caja no salte. */
typedef struct RedimensionTexto {
    int mango;
    int desfase_x, desfase_y;
    int x0, y0, x1, y1;
} RedimensionTexto;

/* Estado de la herramienta de texto: la caja que se esta definiendo o el
 * texto que se esta editando. Mientras `inicio_activo` es cierto se esta
 * arrastrando una caja nueva; despues queda activa hasta que se confirma al
 * pulsar fuera, al cambiar de herramienta o con Escape (que la descarta). */
typedef struct Texto {
    /* Caja base del arrastre: x0, y0, x1, y1 (inclusive). */
    int x0, y0, x1, y1;
    /* Punto donde empezo el arrastre, solo mientras se define la caja. */
    int inicio_activo;
    int inicio_x, inicio_y;
    u16 contenido[MAX_TEXTO];
    int largo;
    /* Posicion del cursor como indice de unidad (0..=largo). */
    int cursor;
    int fuente;
    int tamano;
    u32 color;
    /* Cambia con cada edicion para invalidar la rasterizacion cacheada. */
    unsigned long long revision;
    TextoRaster cache;
    int cache_valida;
    int cache_clave_valida;
    int cache_fuente;
    int cache_tamano;
    int cache_ancho;
    unsigned long long cache_revision;
    /* Redimension en curso con uno de los tiradores de la caja. */
    int redimensionando;
    RedimensionTexto redimension;
} Texto;

typedef struct MenuTexto {
    Zona zona;
    Zona fuente;
    Zona menos;
    Zona tamano;
    Zona mas;
    Zona color;
    /* Entradas del desplegable de fuentes: zona en pantalla e indice. */
    Zona desplegable[TEXTO_FUENTES_TOTAL];
    int num_desplegable;
} MenuTexto;

/* ------------------------------------------------------------------ */
/* Tiles y mapa                                                        */
/* ------------------------------------------------------------------ */
typedef struct TileGuardado {
    char nombre[64];
    char ruta[MAX_PATH];
    int ancho, alto;
    u32 *pixeles;
} TileGuardado;

typedef struct TileMapa {
    int x, y;
    char imagen[64];
    int ancho, alto;
    u32 *pixeles;
} TileMapa;

typedef struct Mapa {
    TileMapa celdas[MAX_CELDAS];
    int num;
    int lado;                 /* lado de celda en pantalla (zoom propio) */
    int origen_x, origen_y;   /* esquina de la celda (0,0) en pantalla */
    int tile_sel;
} Mapa;

/* ------------------------------------------------------------------ */
/* Modo tiles                                                          */
/* ------------------------------------------------------------------ */
typedef struct TileCoord {
    int cx, cy;
} TileCoord;

/* Posicion en pantalla de la cuadricula de tiles. Las celdas se numeran con
 * coordenadas enteras y la composicion entera queda centrada en el area de
 * trabajo. Cada tile conserva el mismo tamano en pantalla. */
typedef struct TilesLayout {
    int min_cx, min_cy;
    int tile_w, tile_h;
    int hueco;
    int paso_x, paso_y;
    int origen_x, origen_y;
} TilesLayout;

/* Documento guardado en una celda inactiva. El tile activo vive en `g.doc`. */
typedef struct TileDoc {
    int ocupado;
    int cx, cy;
    Documento doc;
} TileDoc;

/* Elemento insertado en el lienzo del gestor de escenas. La posicion se guarda
 * en coordenadas del mundo y se dibuja con el desplazamiento de la vista. */
typedef struct ModuloElemento {
    int x, y;
    int ancho, alto;
    int id; /* orden de creacion, estable al moverlo en el lienzo */
    int plegado; /* solo en la raiz del grupo: lista de hijos oculta */
} ModuloElemento;

/* Escena: configuracion de rectangulos del lienzo, con su vista. */
typedef struct Escena {
    char nombre[32];
    ModuloElemento elementos[MODULOS_MAX];
    int num_elementos;
    int pan_x, pan_y;
} Escena;

/* ------------------------------------------------------------------ */
/* Estado global                                                       */
/* ------------------------------------------------------------------ */
typedef enum {
    VISTA_PINTAR = 0,
    VISTA_MAPA,
    VISTA_MODULOS
} Vista;

/* Estado de una pestana. Todas las pestanas comparten el mismo documento, el
 * historial, la seleccion y la rejilla de tiles; aqui solo se guarda como se
 * esta viendo ese documento: modo de trabajo, zoom y desplazamiento. */
typedef struct Pestana {
    int zoom_idx;
    Vista vista;
    int patron_visible;
    int modo_tiles;
    int pan_x, pan_y;
    int tiles_visible;
    int tiles_plegado;
} Pestana;

typedef struct App {
    HWND hwnd;
    int ancho, alto;
    u32 *pixeles;
    int pix_ancho, pix_alto;
    HDC memdc;
    HBITMAP dib;
    HGDIOBJ dib_anterior;

    HFONT fuente;
    HFONT fuente_peq;
    HFONT fuente_negrita;

    int raton_x, raton_y;
    int raton_dentro;
    int boton_izq, boton_der, boton_medio;

    Documento doc;
    char doc_ruta[MAX_PATH];
    PilaUndo deshacer, rehacer;
    Seleccion sel;

    Herramienta herramienta;
    int grosor_idx;
    int opacidad;       /* 0..100 */
    int angulo;         /* -180..180 */
    int espaciado;      /* 1..200 % */
    int dureza;         /* 0..100 */
    int textura;
    int rocio;
    int textura_menu_abierto;
    int modo_relleno;
    int tolerancia_relleno;
    /* Opciones del cubo (indices a sus niveles). */
    int cubo_opacidad;
    int cubo_umbral;
    int cubo_tipo;
    int cubo_area;
    int cubo_transparentes;
    int cubo_combinada;
    int cubo_diagonales;
    int cubo_alisado;
    int cubo_compuesto;
    int cubo_arrastre;     /* control agarrado o -1 */

    u32 frente, fondo;
    u32 paleta[MINI_TOTAL];
    int paleta_llena[MINI_TOTAL];
    int casilla_elegida;   /* 0 = frente, 1 = fondo */
    int mini_elegido;      /* cuadradito de la paleta con borde azul */

    int zoom_idx;
    Lienzo lienzo;
    int pan_x, pan_y;      /* desplazamiento manual del lienzo (px pantalla) */
    int shift_simulado;    /* Mayus fijado por --mayus en las capturas */

    int menu_abierto;      /* -1 = ninguno */
    int hover_menu;
    int hover_herramienta;
    int hover_paleta;
    int hover_tile;
    int hover_capa;
    int tiles_scroll;
    int capa_menu_abierto; /* capa del menu contextual; -1 = ninguno */
    int capa_menu_x, capa_menu_y; /* ancla del menu contextual */
    int confirmar_borrado; /* 1 = modal de borrar capa visible */
    int confirmar_tile;    /* indice del tile a borrar; -1 = ninguno */

    Vista vista;

    /* Pestanas: la activa vive en el estado global de arriba. */
    Pestana pestanas[MAX_PESTANAS];
    int num_pestanas;
    int pestana_activa;

    int panel_opciones;    /* 1 = ventana visible; 0 = plegada en el sidebar */
    int panel_opciones_x, panel_opciones_y;  /* -1 = posicion automatica */
    int arrastrando_panel;
    int hover_opcion;      /* fila de la ventana de opciones bajo el cursor */
    int opcion_arrastrada; /* fila cuyo deslizador se esta arrastrando */
    int hover_cubo;        /* control del panel del cubo bajo el cursor o -1 */

    Mapa mapa;
    int mapa_tile_sel;
    char mapa_ruta[MAX_PATH];

    /* Gestor de escenas: elementos insertados en el lienzo y estado del
     * arrastre. El desplazamiento de la vista usa pan_x/pan_y. */
    ModuloElemento modulos[MODULOS_MAX];
    int num_modulos;
    int modulo_arrastrando; /* 0 = nada, 1 = elemento nuevo, 2 = paneo,
                             * 3 = mover un elemento existente */
    int modulo_drag_x, modulo_drag_y;
    int modulo_drag_i;
    int modulo_elem_off_x, modulo_elem_off_y;
    int modulo_snap_linea_x, modulo_snap_linea_y; /* mundo, -1 = ninguna */
    int hover_modulo_boton;

    /* Escenas creadas y escena activa. La escena activa se edita en
     * modulos/num_modulos y se guarda al cambiar o crear otra. */
    Escena escenas[ESCENAS_MAX];
    int num_escenas;
    int escena_activa;    /* indice en escenas[], -1 si no hay ninguna */
    int escena_siguiente; /* numero para el nombre de la proxima escena */
    int hover_escena;     /* fila de escena bajo el cursor, -1 ninguna */
    int hover_escena_nueva;
    int elementos_scroll; /* primer elemento visible de la lista */
    int modulo_siguiente_id; /* id del proximo elemento insertado */

    /* Modo tiles */
    int modo_tiles;
    int tile_cx, tile_cy;               /* celda del documento activo */
    TileDoc tiles_modo[TILES_MODO_MAX]; /* documentos inactivos */
    int num_tiles_modo;
    TilesLayout tiles_layout;
    int tiles_layout_valido;
    TileCoord tiles_visibles[TILES_MODO_MAX + 1];
    int num_tiles_visibles;
    int tiles_visible;      /* visor flotante abierto */
    int tiles_plegado;      /* visor minimizado en el sidebar */
    int tiles_x, tiles_y;   /* posicion del visor; -1 = automatica */
    int arrastre_tiles;     /* 1 mientras se arrastra el visor */
    int arrastre_tiles_x, arrastre_tiles_y;

    TileGuardado tiles[MAX_TILES];
    int num_tiles;

    /* Estado de arrastre de herramientas de pintura */
    int pintando;
    int boton_pintura;     /* 0 izquierdo, 1 derecho */
    int ult_x, ult_y;      /* ultimo punto en coordenadas del documento */
    int ancla_x, ancla_y;  /* punto inicial de la figura */
    int preview_activa;
    u32 *preview;          /* copia para previsualizar figuras */
    int preview_w, preview_h;
    /* Curva en curso, igual que `Curva`: la primera pulsacion fija los
     * extremos, la segunda coloca el primer control y la tercera el segundo;
     * al soltar el segundo control se estampa. */
    int curva_activa;
    int curva_arrastrando;
    int curva_ini_x, curva_ini_y;
    int curva_fin_x, curva_fin_y;
    int curva_c1_x, curva_c1_y;
    int curva_c2_x, curva_c2_y;
    int curva_control1;
    int curva_control2;
    int poligono_activo;
    int poligono_puntos[64][2];
    int poligono_num;

    /* Seleccion: arrastre */
    int sel_arrastrando;
    int sel_tirador;       /* -1 = ninguno */
    int sel_moviendo;
    int sel_ini_x, sel_ini_y;
    Seleccion sel_antes;

    /* Texto */
    /* Herramienta de texto: caja, contenido, cache y barra flotante. */
    Texto texto;
    int texto_activo;
    int texto_fuentes_abierto;
    int texto_hover_parte;    /* ParteMenuTexto o -1 */
    int texto_hover_fuente;   /* indice de fuente o -1 */
    int texto_mango_hover;    /* tirador de la caja o -1 */

    /* Recorte flotante (cortar/pegar) */
    u32 *portapapeles;
    int portapapeles_w, portapapeles_h;

    /* Ventana de informacion */
    int info_visible;
    int info_x, info_y;
    int info_arrastrando;
    u32 *info_imagen;
    int info_w, info_h;

    /* Visor del patron de textura */
    int patron_visible;
    int patron_x, patron_y;

    /* Redimension de lienzo: se guarda la geometria inicial y el desfase
     * entre el cursor y el borde que mueve el tirador. */
    int lienzo_tirador;    /* -1 ninguno, 0..7 tirador que se arrastra */
    int lienzo_hover;      /* -1 ninguno, 0..7 tirador bajo el cursor */
    Lienzo lienzo_base;
    int lienzo_offx, lienzo_offy;

    /* Mensajes */
    char estado[256];
    u32 estado_hasta;

    int salir;
} App;

extern App g;

/* ------------------------------------------------------------------ */
/* Nucleo: buffer, color, texto y utilidades                           */
/* ------------------------------------------------------------------ */
int  app_iniciar(void);
void app_terminar(void);
void app_redimensionar(int ancho, int alto);
void app_dibujar(void);
void app_invalidar(void);
void app_actualizar_titulo(void);
void app_procesar(void);
void app_estado(const char *formato, ...);
void dibujar_rect(int x0, int y0, int x1, int y1, u32 color);
void dibujar_rect_z(Zona z, u32 color);
void dibujar_marco(int x0, int y0, int x1, int y1, u32 color);
void dibujar_marco_z(Zona z, u32 color);
void dibujar_pixel(int x, int y, u32 color);
u32   dibujar_leer(int x, int y);
void dibujar_linea(int x0, int y0, int x1, int y1, u32 color);
void dibujar_circulo(int cx, int cy, int radio, u32 color, int relleno);
void dibujar_texto(int x, int y, const char *texto, u32 color);
void dibujar_texto_fuente(int x, int y, const char *texto, u32 color, HFONT fuente);
void dibujar_texto_centrado(Zona z, const char *texto, u32 color);
void dibujar_texto_derecha(int x_derecha, int y, const char *texto, u32 color);
int  texto_ancho(const char *texto);
int  texto_ancho_fuente(const char *texto, HFONT fuente);
int  texto_alto(void);
const unsigned char *fuente_glifo(int codepoint);
int  fuente_texto_ancho(const char *texto);
int  fuente_texto_ancho_escala(const char *texto, int escala);
void fuente_dibujar(int x, int y, const char *texto, u32 color);
void fuente_dibujar_escala(int x, int y, const char *texto, u32 color, int escala);
u32   color_mezclar(u32 dst, u32 src, int alfa);
u32   color_mezclar_color(u32 a, u32 b, float t);
int   color_diferencia(u32 a, u32 b);
u32   color_oscurecer(u32 c, int delta);
u32   color_aclarar(u32 c, int delta);
COLORREF color_a_win(u32 c);
u32   color_de_win(COLORREF c);
unsigned int hash32(unsigned int x);
float rnd01(unsigned int *estado);
int   limitar(int v, int min, int max);
int   redondear(float v);
void  copiar_pixeles(u32 *dst, const u32 *src, int n);
void  formatear(char *dst, int tam, const char *formato, ...);
void  recortar_texto(char *dst, const char *src, int max);
int   termina_con(const char *texto, const char *sufijo);
void  ruta_carpeta(char *dst, int tam, const char *ruta);
void  ruta_nombre(char *dst, int tam, const char *ruta);
void  ruta_sin_extension(char *dst, int tam, const char *ruta);
int   crear_carpeta(const char *ruta);
int   leer_archivo(const char *ruta, char **datos, int *largo);
int   escribir_archivo(const char *ruta, const char *datos, int largo);
void  obtener_ruta_exe(char *dst, int tam);
void  unir_ruta(char *dst, int tam, const char *a, const char *b);

/* ------------------------------------------------------------------ */
/* Documento                                                           */
/* ------------------------------------------------------------------ */
Documento doc_nuevo(int ancho, int alto, u32 fondo);
void doc_liberar(Documento *d);
int  doc_agregar_capa(Documento *d, const char *nombre);
int  doc_duplicar_capa(Documento *d);
int  doc_eliminar_capa(Documento *d, int indice);
int  doc_mover_capa(Documento *d, int indice, int delta);
int  doc_capa_tiene_alfa(Documento *d, int indice);
int  doc_poner_alfa(Documento *d, int indice, int alfa);
int  doc_fusionar_abajo(Documento *d);
void doc_redimensionar(Documento *d, int ancho, int alto);
void doc_componer_todo(Documento *d);
void doc_componer_region(Documento *d, int x0, int y0, int x1, int y1);
u32  doc_color_visible(Documento *d, int x, int y);
void doc_borrar_capa(Documento *d);
int  doc_asegurar_compuesto(Documento *d);
void doc_copiar_en(Documento *dst, const Documento *src);
void doc_intercambiar(Documento *a, Documento *b);

void pila_iniciar(PilaUndo *p, int capacidad);
void pila_liberar(PilaUndo *p);
void pila_guardar(PilaUndo *p, const Documento *d);
int  pila_deshacer(PilaUndo *p, PilaUndo *rehacer, Documento *d);
int  pila_rehacer(PilaUndo *p, PilaUndo *rehacer, Documento *d);
void pila_limpiar(PilaUndo *p);

/* ------------------------------------------------------------------ */
/* Seleccion                                                           */
/* ------------------------------------------------------------------ */
void seleccion_liberar(Seleccion *s);
void seleccion_limpiar(Seleccion *s);
void seleccion_todo(Seleccion *s, int ancho, int alto);
void seleccion_invertir(Seleccion *s, int ancho, int alto);
int  seleccion_contiene(Seleccion *s, int x, int y);
Zona seleccion_caja(Seleccion *s);
void seleccion_levantar(Seleccion *s, Documento *d);
void seleccion_soltar(Seleccion *s, Documento *d, PilaUndo *deshacer);
void seleccion_mover(Seleccion *s, Documento *d, int dx, int dy);
void seleccion_borrar_contenido(Seleccion *s, Documento *d);
void seleccion_reescalar(Seleccion *s, int ancho, int alto);

/* ------------------------------------------------------------------ */
/* Pintura / herramientas                                              */
/* ------------------------------------------------------------------ */
void pintura_iniciar(void);
void pintura_terminar(void);
void pintura_pulsar(int x, int y, int boton);
void pintura_mover(int x, int y, int botones);
void pintura_soltar(int x, int y, int boton);
void pintura_doble_clic(int x, int y);
void pintura_tecla(int vk, int ctrl, int shift);
void pintura_caracter(int ch);
void pintura_dibujar_lienzo(void);
void pintura_dibujar_documento(Documento *d, Lienzo l);
void pintura_dibujar_solo_overlays(void);
int  pintura_punto_documento(int x, int y, int *dx, int *dy);
int  pintura_herramienta_actual(void);

void pintura_trazo_punto(int x, int y, int boton, int primero);
void pintura_trazo_linea(int x0, int y0, int x1, int y1, int boton);
void pintura_rellenar(int x, int y, int boton);
void pintura_figura_preview(int x, int y, int shift);
void pintura_figura_confirmar(int x, int y, int shift);
void pintura_figura_cancelar(void);
void pintura_seleccion_iniciar(int x, int y, int boton, int libre);
void pintura_seleccion_mover(int x, int y);
void pintura_seleccion_soltar(int x, int y);
void pintura_confirmar_seleccion(void);
void pintura_copiar(int cortar);
void pintura_pegar(void);
void pintura_borrar_seleccion(void);

/* Herramienta de texto completa. */
void texto_iniciar(void);
void texto_terminar(void);
void texto_limpiar(void);
int  texto_editing(void);
int  texto_pulsar(int x, int y, int boton);
void texto_mover(int x, int y, int botones);
void texto_soltar(int x, int y, int boton);
int  texto_tecla(int vk, int ctrl, int shift);
int  texto_caracter(int ch);
void texto_empezar(int x, int y);
void texto_confirmar(void);
void texto_cancelar(void);
void texto_invalidar(void);
void texto_dibujar_barra(void);
void texto_dibujar_caja(void);
Documento *texto_documento_visible(void);
int  texto_en_menu(int x, int y);
int  texto_en_caja(int x, int y);
int  texto_mango_en(int x, int y);

/* Utilidades de pintura compartidas con la herramienta de texto. */
int  pintura_pixel_extendido(int mx, int my, int *x, int *y);
int  pintura_pixel_ajustado(int mx, int my, int *x, int *y);
void pintura_pantalla_caja(int x0, int y0, int x1, int y1,
                           int *sx0, int *sy0, int *sx1, int *sy1);
int  pintura_mango_en_caja(Lienzo caja, int mx, int my);
void pintura_dibujar_mangos_caja(Lienzo caja, int activo);
void pintura_dibujar_seleccion_caja(int x0, int y0, int x1, int y1, int fase);

/* ------------------------------------------------------------------ */
/* Imagen: PNG/BMP via GDI+ (con reserva para BMP)                     */
/* ------------------------------------------------------------------ */
int  imagen_iniciar(void);
void imagen_terminar(void);
u32 *imagen_cargar(const char *ruta, int *ancho, int *alto);
int  imagen_guardar(const char *ruta, const u32 *pixeles, int ancho, int alto);
int  imagen_guardar_png(const char *ruta, const u32 *pixeles, int ancho, int alto);
int  imagen_guardar_bmp(const char *ruta, const u32 *pixeles, int ancho, int alto);
u32 *imagen_escalar(const u32 *origen, int ow, int oh, int dw, int dh);
u32 *imagen_desde_memoria(const void *datos, int largo, int *ancho, int *alto);

/* Iconos incrustados en el ejecutable. */
u32 *recurso_png(int id, int *ancho, int *alto);
u32 *icono_herramienta(int indice, int *ancho, int *alto);
u32 *icono_logo_imagen(int *ancho, int *alto);
u32 *icono_guardar_imagen(int *ancho, int *alto);

/* ------------------------------------------------------------------ */
/* Tiles y mapa                                                        */
/* ------------------------------------------------------------------ */
void tiles_cargar(void);
void tiles_liberar(void);
void tiles_ordenar_por_fecha(void);
int  tiles_guardar_actual(void);
int  tiles_guardar_desde(const u32 *pix, int ancho, int alto, const char *nombre);
void tile_liberar(TileGuardado *t);
u32 *tile_miniatura(const TileGuardado *t, int lado);

void mapa_iniciar(void);
void mapa_liberar(void);
void mapa_reiniciar(void);
void mapa_colocar(int celda_x, int celda_y, int tile);
void mapa_borrar(int celda_x, int celda_y);
void mapa_dibujar(void);
void mapa_pulsar(int x, int y, int boton);
void mapa_soltar(int x, int y, int boton);
void mapa_mover(int x, int y, int botones);
void mapa_rueda(int x, int y, int delta, int ctrl);
int  mapa_guardar(const char *ruta);
int  mapa_cargar(const char *ruta);
void mapa_coord(int x, int y, int *cx, int *cy);

/* Modo tiles */
void tiles_modo_iniciar(void);
void tiles_modo_liberar(void);
void tiles_modo_entrar(void);
void tiles_modo_salir(void);
void tiles_modo_limpiar(void);
int  tiles_modo_agregar(TileCoord desde, int dir);
void tiles_modo_layout(void);
int  tiles_modo_intercambiar(TileCoord destino);
TileCoord tiles_modo_activo(void);
int  tiles_modo_celda_en(int x, int y, TileCoord *out);
int  tiles_modo_celda_ocupada(TileCoord c, int *indice);
Zona tiles_modo_zona_celda(TileCoord c);
Zona tiles_modo_boton_zona(TileCoord c, int dir, int *existe);
int  tiles_punto_global(int x, int y, int *gx, int *gy);
void tiles_punto_local(TileCoord c, int gx, int gy, int *lx, int *ly);
Zona tiles_visor_zona(void);
Zona tiles_visor_titulo_zona(void);
Zona tiles_visor_minimizar_zona(void);
Zona tiles_visor_contenido_zona(void);
Zona tiles_visor_plegado_zona(void);
int  clic_tiles_visor(int x, int y);
int  tiles_modo_pulsar(int x, int y, int boton);
void tiles_modo_mover_visor(int x, int y);
void tiles_modo_soltar(void);
void ui_dibujar_tiles(void);
void ui_dibujar_tiles_visor(void);
void ui_dibujar_tiles_minimizado(void);
int  tiles_modo_cruzar_trazo(int x, int y);

/* ------------------------------------------------------------------ */
/* Pestanas                                                            */
/* ------------------------------------------------------------------ */
void pestanas_iniciar(void);
void pestanas_terminar(void);
int  pestanas_nueva_modo(void);
void pestanas_documento_reemplazado(void);
int  pestanas_activar(int indice);
int  pestanas_cerrar(int indice);
int  pestanas_hay_cambios(void);
void pestanas_marcar_guardada(const char *ruta);
void pestanas_avanzar(int delta);
const char *pestanas_titulo(int indice);
const char *pestanas_ruta(int indice);
int  pestanas_esta_modificada(int indice);

/* ------------------------------------------------------------------ */
/* UI                                                                  */
/* ------------------------------------------------------------------ */
Zona pestanas_zona(void);
Zona pestana_zona(int indice);
Zona pestana_cerrar_zona(int indice);
Zona pestana_nueva_zona(void);
int  pestana_en(int x, int y);
int  clic_pestanas(int x, int y, int boton);
void ui_dibujar_pestanas(void);
void ui_dibujar(void);
void ui_dibujar_barra_menus(void);
void ui_dibujar_sidebar(void);
void ui_dibujar_topbar(void);
void ui_dibujar_barra_abajo(void);
void ui_dibujar_panel_derecho(void);
void ui_dibujar_opciones(void);
void ui_dibujar_info(void);
void ui_dibujar_menu_desplegable(void);
/* (cursor del raton: solo el cursor de Windows, sin circulo dibujado) */
void ui_dibujar_texturas(void);
const char *ui_nombre_herramienta(int indice);

Zona menu_barra_zona(void);
int  menu_barra_en(int x, int y);
Zona barra_zona(void);
Zona barra_abajo_zona(void);
Zona zona_trabajo(void);
Zona modulos_lienzo_zona(void);
Zona modulos_boton_zona(void);
Zona elementos_lista_zona(void);
Zona elemento_fila_zona(int indice);
Zona escena_nueva_zona(void);
Zona escena_fila_zona(int indice);
void modulos_acoplar(int *x, int *y, int ancho, int alto, int ignorar,
                     int *linea_x, int *linea_y);
void modulos_insertar(int x, int y);
int  elementos_ordenar_lista(int *orden, int *hijos);
void escenas_iniciar(void);
void escenas_nueva(void);
void escenas_activar(int indice);
int  sidebar_derecho_ancho(void);
Zona herramienta_zona(int indice);
Zona panel_textura_zona(void);
Zona textura_menu_zona(void);
Zona textura_muestra_zona(int indice, int juego);
int  textura_muestra_en(int x, int y, int juego);
int  textura_juego_actual(void);
Zona boton_guardar_tile_zona(void);
Zona mini_zona(int indice);
Zona arcoiris_zona(void);
Zona opciones_zona(void);
Zona opciones_titulo_zona(void);
Zona opciones_minimizar_zona(void);
Zona opciones_pista_zona(int fila);
Zona opciones_minimizada_zona(void);
int  opciones_niveles(int fila);
int  opciones_indice_actual(int fila);
void opciones_fijar_desde_x(int fila, int x);
const char *opciones_etiqueta(int fila);
const char *opciones_valor(int fila, char *buffer, int tam);

/* Controles del panel del cubo. `cubo_control_en` devuelve uno de los codigos
 * CUBO_CTRL_* o -1. */
Zona cubo_opacidad_zona(void);
Zona cubo_tipo_zona(int indice);
Zona cubo_area_zona(int indice);
Zona cubo_busqueda_zona(int indice);
Zona cubo_umbral_zona(void);
Zona cubo_compuesto_zona(void);
int  cubo_control_en(int x, int y);
int  cubo_es_deslizador(int control);
int  cubo_aplicar_control(int control, int x);
void ui_dibujar_panel_cubo(void);
Zona zoom_boton_menos(void);
Zona zoom_boton_mas(void);
Zona zoom_pista_zona(void);
Zona zoom_etiqueta_zona(void);
Zona grosor_pista_zona(void);
Zona grosor_etiqueta_zona(void);
Zona tile_mini_zona(int indice);
Zona capa_fila_zona(int indice);
Zona botones_capa_zona(int *mas, int *menos);
Zona capa_ojo_zona(Zona fila);
Zona capa_alfa_zona(Zona fila);
int  botones_panel_derecho_zona(Zona *mas, Zona *borrar);
Zona menu_capa_panel_zona(Zona *anadir, Zona *quitar);
Zona confirmar_panel_zona(Zona *si, Zona *no);
void ui_dibujar_menu_capa(void);
void ui_dibujar_confirmaciones(void);
Zona menu_desplegable_zona(void);
Zona menu_panel_zona(int menu);
Zona menu_fila_zona(int menu, int fila);
int  menu_num_filas(int menu);
int  menu_fila_en(int x, int y);
int  menu_fila_activa(int menu, int fila);
void ui_dibujar_patron(void);
Zona patron_zona(void);
Zona patron_minimizar_zona(void);
void menu_desplegable_opciones(const char ***textos, const char ***atajos, int *num);

int  clic_menu(int x, int y);
int  clic_sidebar(int x, int y);
int  clic_topbar(int x, int y);
int  clic_barra_abajo(int x, int y);
int  clic_panel_derecho(int x, int y, int boton);
int  clic_opciones(int x, int y);
int  clic_info(int x, int y);
int  clic_lienzo(int x, int y);

void app_pulsar(int x, int y, int boton);
void app_soltar(int x, int y, int boton);
void app_mover(int x, int y, int botones);
void app_rueda(int x, int y, int delta, int ctrl);
void app_tecla(int vk, int ctrl, int shift);
void app_caracter(int ch);
void app_doble_clic(int x, int y);

void lienzo_actualizar(void);
int  zoom_porcentaje(int indice);
int  pintura_grosor_px(int indice);
float zoom_escala(int porcentaje);
int  lado_con_zoom(int lado, int porcentaje);
int  zoom_para_caber(int ancho, int alto);
void zoom_aplicar(int indice);
int  lienzo_contiene(int x, int y);
int  lienzo_tirador_en(int x, int y);
void lienzo_redimensionar_iniciar(int tirador, int x, int y);
void lienzo_redimensionar_mover(int x, int y, int shift);
void lienzo_redimensionar_soltar(int x, int y, int shift);

void dialogo_nuevo(void);
void dialogo_abrir_imagen(void);
void dialogo_guardar_png(void);
void dialogo_exportar_bmp(void);
void dialogo_abrir_mapa(void);
void dialogo_guardar_mapa(void);
void dialogo_guardar_escena(void);
int  escena_guardar_archivo(const char *ruta);
void dialogo_elegir_color(int para_fondo);
int  dialogo_elegir_color_valor(u32 inicial, u32 *salida);
int  dialogo_confirmar(const char *titulo, const char *mensaje);
void dialogo_error(const char *mensaje);
void dialogo_informacion(const char *titulo, const char *mensaje);

void app_nueva_imagen(int ancho, int alto);
void app_abrir_imagen(const char *ruta);
void app_guardar_imagen(const char *ruta, int png);
void app_cambiar_vista(Vista vista);
void app_alternar_mapa(void);
void app_alternar_gestor_modulos(void);
void app_poner_herramienta(int herramienta);
void app_agregar_color_paleta(u32 color);

#ifdef __cplusplus
}
#endif

#endif /* TEXTURE_PAINT_H */
