/* ==========================================================================
 * Pestanas de modo: todas comparten el mismo documento, historial, seleccion
 * y rejilla de tiles. Cada pestana guarda solo su estado de vista: modo de
 * trabajo, zoom y desplazamiento. El titulo se deriva del modo activo.
 * ========================================================================== */
#include "app.h"

/* ------------------------------------------------------------------ */
/* Estado de la pestana activa                                         */
/* ------------------------------------------------------------------ */
static void guardar_estado_activo(void) {
    Pestana *p;
    if (g.pestana_activa < 0 || g.pestana_activa >= g.num_pestanas) return;
    p = &g.pestanas[g.pestana_activa];
    p->zoom_idx = g.zoom_idx;
    p->vista = g.vista;
    p->patron_visible = g.patron_visible;
    p->modo_tiles = g.modo_tiles;
    p->pan_x = g.pan_x;
    p->pan_y = g.pan_y;
    p->tiles_visible = g.tiles_visible;
    p->tiles_plegado = g.tiles_plegado;
}

static void aplicar_estado(const Pestana *p) {
    g.zoom_idx = p->zoom_idx;
    g.vista = p->vista;
    g.patron_visible = p->patron_visible;
    g.modo_tiles = p->modo_tiles;
    g.pan_x = p->pan_x;
    g.pan_y = p->pan_y;
    g.tiles_layout_valido = 0;
    if (g.modo_tiles) {
        g.tiles_visible = p->tiles_visible;
        g.tiles_plegado = p->tiles_plegado;
        tiles_modo_layout();
    } else {
        g.tiles_visible = 0;
        g.tiles_plegado = 0;
    }
    lienzo_actualizar();
}

/* Cierra cualquier edicion flotante antes de cambiar de modo. El documento es
 * compartido, asi que lo que hubiera en curso se confirma en el. */
static void preparar_cambio(void) {
    if (g.texto_activo) texto_confirmar();
    if (g.curva_activa) pintura_figura_confirmar(0, 0, 0);
    if (g.sel.activa && g.sel.flotante)
        seleccion_soltar(&g.sel, &g.doc, &g.deshacer);
    g.pintando = 0;
    g.preview_activa = 0;
    g.poligono_activo = 0;
    g.poligono_num = 0;
    g.sel_arrastrando = 0;
    g.sel_tirador = -1;
    g.sel_moviendo = 0;
    g.lienzo_tirador = -1;
    g.texto_fuentes_abierto = 0;
    g.textura_menu_abierto = 0;
    g.menu_abierto = -1;
    g.capa_menu_abierto = -1;
    g.confirmar_borrado = 0;
    g.confirmar_tile = -1;
    g.arrastre_tiles = 0;
    g.modulo_arrastrando = 0;
    g.modulo_drag_i = -1;
    g.modulo_snap_linea_x = -1;
    g.modulo_snap_linea_y = -1;
    g.hover_modulo_boton = 0;
}

static void limpiar_estado_transitorio(void) {
    texto_limpiar();
    g.pintando = 0;
    g.boton_pintura = -1;
    g.preview_activa = 0;
    g.poligono_activo = 0;
    g.poligono_num = 0;
    g.curva_activa = 0;
    g.sel_arrastrando = 0;
    g.sel_tirador = -1;
    g.sel_moviendo = 0;
    g.lienzo_tirador = -1;
    g.lienzo_hover = -1;
    g.menu_abierto = -1;
    g.textura_menu_abierto = 0;
    g.capa_menu_abierto = -1;
    g.confirmar_borrado = 0;
    g.confirmar_tile = -1;
    g.arrastre_tiles = 0;
    g.info_arrastrando = 0;
}

/* Deja una pestana recien creada en el modo Pintar. */
static void pestana_pintar(Pestana *p, int zoom_idx) {
    memset(p, 0, sizeof(*p));
    p->zoom_idx = zoom_idx;
    p->vista = VISTA_PINTAR;
    p->modo_tiles = 0;
    p->patron_visible = 0;
}

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */
void pestanas_iniciar(void) {
    memset(g.pestanas, 0, sizeof(g.pestanas));
    g.num_pestanas = 1;
    g.pestana_activa = 0;
    pestana_pintar(&g.pestanas[0], g.zoom_idx);
}

void pestanas_terminar(void) {
    /* El documento, el historial y los tiles son globales: los libera
     * app_terminar. Aqui no hay nada propio que soltar. */
    g.num_pestanas = 0;
    g.pestana_activa = -1;
}

/* Duplica el modo y la vista actuales en una pestana nueva. No crea ni vacia
 * el documento: las dos pestanas siguen mostrando el mismo. */
int pestanas_nueva_modo(void) {
    Pestana copia;
    int idx;

    if (g.num_pestanas >= MAX_PESTANAS) {
        dialogo_error("No se pueden abrir mas pestanas.");
        return 0;
    }
    if (g.num_pestanas <= 0 || g.pestana_activa < 0) return 0;

    preparar_cambio();
    guardar_estado_activo();
    copia = g.pestanas[g.pestana_activa];

    idx = g.num_pestanas++;
    g.pestanas[idx] = copia;
    g.pestana_activa = idx;

    limpiar_estado_transitorio();
    app_actualizar_titulo();
    app_invalidar();
    return 1;
}

/* El documento compartido se ha sustituido por otro. Se conservan los modos
 * de cada pestana, pero el zoom se ajusta al nuevo tamano y la rejilla de
 * tiles empieza limpia. */
void pestanas_documento_reemplazado(void) {
    int i;
    int z = zoom_para_caber(g.doc.ancho, g.doc.alto);
    for (i = 0; i < g.num_pestanas; i++) {
        g.pestanas[i].zoom_idx = z;
        g.pestanas[i].pan_x = 0;
        g.pestanas[i].pan_y = 0;
    }
    g.zoom_idx = z;
    g.pan_x = 0;
    g.pan_y = 0;
    g.tile_cx = 0;
    g.tile_cy = 0;
    g.tiles_layout_valido = 0;
    if (g.modo_tiles) tiles_modo_layout();
    lienzo_actualizar();
}

int pestanas_activar(int indice) {
    if (indice < 0 || indice >= g.num_pestanas) return 0;
    if (indice == g.pestana_activa) return 0;

    preparar_cambio();
    guardar_estado_activo();
    g.pestana_activa = indice;
    aplicar_estado(&g.pestanas[indice]);
    limpiar_estado_transitorio();
    app_actualizar_titulo();
    app_invalidar();
    return 1;
}

int pestanas_cerrar(int indice) {
    if (indice < 0 || indice >= g.num_pestanas) return 0;

    preparar_cambio();

    if (indice == g.pestana_activa) {
        if (g.num_pestanas <= 1) {
            /* Ultima pestana: no se destruye el documento, solo se vuelve al
             * modo Pintar. */
            pestana_pintar(&g.pestanas[0], g.zoom_idx);
            g.pestana_activa = 0;
            g.num_pestanas = 1;
            g.vista = VISTA_PINTAR;
            g.patron_visible = 0;
            g.modo_tiles = 0;
            g.tiles_visible = 0;
            g.tiles_plegado = 0;
            g.tile_cx = 0;
            g.tile_cy = 0;
            g.tiles_layout_valido = 0;
            g.pan_x = 0;
            g.pan_y = 0;
            limpiar_estado_transitorio();
            lienzo_actualizar();
        } else {
            memmove(&g.pestanas[indice], &g.pestanas[indice + 1],
                    (size_t)(g.num_pestanas - indice - 1) * sizeof(Pestana));
            g.num_pestanas--;
            memset(&g.pestanas[g.num_pestanas], 0, sizeof(Pestana));
            g.pestana_activa = indice < g.num_pestanas ? indice
                                                       : g.num_pestanas - 1;
            aplicar_estado(&g.pestanas[g.pestana_activa]);
            limpiar_estado_transitorio();
        }
    } else {
        memmove(&g.pestanas[indice], &g.pestanas[indice + 1],
                (size_t)(g.num_pestanas - indice - 1) * sizeof(Pestana));
        g.num_pestanas--;
        memset(&g.pestanas[g.num_pestanas], 0, sizeof(Pestana));
        if (indice < g.pestana_activa) g.pestana_activa--;
    }

    app_actualizar_titulo();
    app_invalidar();
    return 1;
}

int pestanas_hay_cambios(void) {
    return g.doc.modificado != 0;
}

void pestanas_marcar_guardada(const char *ruta) {
    g.doc.modificado = 0;
    if (ruta && ruta[0]) recortar_texto(g.doc_ruta, ruta, sizeof(g.doc_ruta));
    app_invalidar();
}

void pestanas_avanzar(int delta) {
    int n = g.num_pestanas;
    int i;
    if (n <= 1 || delta == 0) return;
    i = (g.pestana_activa + delta) % n;
    while (i < 0) i += n;
    pestanas_activar(i);
}

/* El titulo de una pestana describe su modo de trabajo. La pestana activa se
 * lee del estado global para que el titulo cambie al instante. */
const char *pestanas_titulo(int indice) {
    int vista, patron, tiles;
    if (indice < 0 || indice >= g.num_pestanas) return "";
    if (indice == g.pestana_activa) {
        vista = g.vista;
        patron = g.patron_visible;
        tiles = g.modo_tiles;
    } else {
        vista = g.pestanas[indice].vista;
        patron = g.pestanas[indice].patron_visible;
        tiles = g.pestanas[indice].modo_tiles;
    }
    if (vista == VISTA_MAPA) return "Mapa";
    if (vista == VISTA_MODULOS) return "Logica";
    if (vista == VISTA_MENUS) return "Menus";
    if (tiles) return "Tiles";
    if (patron) return "Patron";
    return "Pintar";
}

const char *pestanas_ruta(int indice) {
    (void)indice;
    return g.doc_ruta;
}

int pestanas_esta_modificada(int indice) {
    if (indice < 0 || indice >= g.num_pestanas) return 0;
    /* El documento es unico: el punto de cambios se muestra solo en la
     * pestana activa para no repetirlo en todas. */
    return indice == g.pestana_activa && g.doc.modificado;
}
