/* ==========================================================================
 * Pestanas: cada pestana guarda un documento independiente con su historial,
 * su seleccion, su zoom y su espacio de modo tiles.
 *
 * La pestana activa vive en el estado global (`g.doc`, `g.deshacer`,
 * `g.rehacer`, `g.sel`, `g.tiles_modo`). Al cambiar de pestana se intercambia
 * ese estado con el guardado en la estructura `Pestana`; asi el resto del
 * editor sigue funcionando sin cambios.
 * ========================================================================== */
#include "app.h"

/* ------------------------------------------------------------------ */
/* Utilidades internas                                                 */
/* ------------------------------------------------------------------ */
static void intercambiar_documento(Documento *a, Documento *b) {
    Documento t = *a;
    *a = *b;
    *b = t;
}

static void intercambiar_pila(PilaUndo *a, PilaUndo *b) {
    PilaUndo t = *a;
    *a = *b;
    *b = t;
}

static void intercambiar_seleccion(Seleccion *a, Seleccion *b) {
    Seleccion t = *a;
    *a = *b;
    *b = t;
}

static void intercambiar_tiles(Pestana *p) {
    int i;
    for (i = 0; i < TILES_MODO_MAX; i++) {
        TileDoc t = g.tiles_modo[i];
        g.tiles_modo[i] = p->tiles_modo[i];
        p->tiles_modo[i] = t;
    }
    {
        int n = g.num_tiles_modo;
        g.num_tiles_modo = p->num_tiles_modo;
        p->num_tiles_modo = n;
    }
}

static void liberar_tiles_pestana(Pestana *p) {
    int i;
    for (i = 0; i < p->num_tiles_modo && i < TILES_MODO_MAX; i++) {
        if (p->tiles_modo[i].ocupado) doc_liberar(&p->tiles_modo[i].doc);
        p->tiles_modo[i].ocupado = 0;
    }
    p->num_tiles_modo = 0;
}

static void liberar_pestana(Pestana *p) {
    if (!p) return;
    doc_liberar(&p->doc);
    pila_liberar(&p->deshacer);
    pila_liberar(&p->rehacer);
    seleccion_liberar(&p->sel);
    liberar_tiles_pestana(p);
    memset(p, 0, sizeof(*p));
}

/* Cierra cualquier edicion flotante antes de mover el estado de sitio. */
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
}

static void guardar_activa(void) {
    Pestana *p;
    if (g.num_pestanas <= 0 ||
        g.pestana_activa < 0 || g.pestana_activa >= g.num_pestanas)
        return;
    p = &g.pestanas[g.pestana_activa];
    intercambiar_documento(&p->doc, &g.doc);
    intercambiar_pila(&p->deshacer, &g.deshacer);
    intercambiar_pila(&p->rehacer, &g.rehacer);
    intercambiar_seleccion(&p->sel, &g.sel);
    intercambiar_tiles(p);
    p->zoom_idx = g.zoom_idx;
    p->vista = g.vista;
    p->modo_tiles = g.modo_tiles;
    p->tile_cx = g.tile_cx;
    p->tile_cy = g.tile_cy;
}

static void cargar_activa(void) {
    Pestana *p;
    if (g.num_pestanas <= 0 ||
        g.pestana_activa < 0 || g.pestana_activa >= g.num_pestanas)
        return;
    p = &g.pestanas[g.pestana_activa];
    intercambiar_documento(&p->doc, &g.doc);
    intercambiar_pila(&p->deshacer, &g.deshacer);
    intercambiar_pila(&p->rehacer, &g.rehacer);
    intercambiar_seleccion(&p->sel, &g.sel);
    intercambiar_tiles(p);
    g.zoom_idx = p->zoom_idx;
    g.vista = p->vista;
    g.modo_tiles = p->modo_tiles;
    g.tile_cx = p->tile_cx;
    g.tile_cy = p->tile_cy;
    g.tiles_layout_valido = 0;
    if (g.modo_tiles) {
        if (!g.tiles_visible) g.tiles_visible = 1;
        tiles_modo_layout();
    } else {
        g.tiles_visible = 0;
        g.tiles_plegado = 0;
    }
    lienzo_actualizar();
}

static void soltar_estado_activo(void) {
    doc_liberar(&g.doc);
    pila_liberar(&g.deshacer);
    pila_liberar(&g.rehacer);
    seleccion_liberar(&g.sel);
    tiles_modo_limpiar();
}

static void resetear_global_tras_cambio(void) {
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

static void poner_titulo(Pestana *p, const char *titulo, const char *ruta,
                         int zoom_idx) {
    memset(p, 0, sizeof(*p));
    if (titulo && titulo[0])
        recortar_texto(p->titulo, titulo, sizeof(p->titulo));
    if (ruta) recortar_texto(p->ruta, ruta, sizeof(p->ruta));
    p->zoom_idx = zoom_idx;
    p->vista = VISTA_PINTAR;
    p->modo_tiles = 0;
}

static void activo_documento_nuevo(const char *titulo) {
    Documento d = doc_nuevo(LIENZO_INICIAL, LIENZO_INICIAL, LIENZO);
    if (!d.capas[0].pixeles) {
        dialogo_error("No hay memoria para crear el lienzo.");
        return;
    }
    g.doc = d;
    pila_iniciar(&g.deshacer, MAX_UNDO);
    pila_iniciar(&g.rehacer, MAX_UNDO);
    seleccion_limpiar(&g.sel);
    g.zoom_idx = zoom_para_caber(d.ancho, d.alto);
    g.pan_x = g.pan_y = 0;
    g.vista = VISTA_PINTAR;
    g.modo_tiles = 0;
    g.tiles_visible = 0;
    g.tiles_plegado = 0;
    g.tile_cx = g.tile_cy = 0;
    g.tiles_layout_valido = 0;
    lienzo_actualizar();
    if (titulo && titulo[0])
        recortar_texto(g.pestanas[0].titulo, titulo,
                       sizeof(g.pestanas[0].titulo));
    g.pestanas[0].ruta[0] = 0;
    g.pestanas[0].zoom_idx = g.zoom_idx;
    g.pestanas[0].vista = g.vista;
    g.pestanas[0].modo_tiles = 0;
    g.pestanas[0].tile_cx = 0;
    g.pestanas[0].tile_cy = 0;
}

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */
void pestanas_iniciar(void) {
    memset(g.pestanas, 0, sizeof(g.pestanas));
    g.num_pestanas = 1;
    g.pestana_activa = 0;
    recortar_texto(g.pestanas[0].titulo, "Sin titulo 1",
                   sizeof(g.pestanas[0].titulo));
    g.pestanas[0].zoom_idx = g.zoom_idx;
    g.pestanas[0].vista = g.vista;
    g.pestanas[0].modo_tiles = 0;
    g.pestanas[0].tile_cx = 0;
    g.pestanas[0].tile_cy = 0;
}

void pestanas_terminar(void) {
    int i;
    for (i = 0; i < g.num_pestanas; i++) {
        if (i == g.pestana_activa) {
            /* El estado activo lo libera app_terminar. */
            continue;
        }
        liberar_pestana(&g.pestanas[i]);
    }
    g.num_pestanas = 0;
    g.pestana_activa = -1;
}

int pestanas_nueva_documento(Documento *nuevo, const char *titulo,
                             const char *ruta, int zoom_idx) {
    int idx;
    char auto_titulo[128];
    if (!nuevo || !nuevo->capas[0].pixeles) return 0;
    if (g.num_pestanas >= MAX_PESTANAS) {
        dialogo_error("No se pueden abrir mas pestanas.");
        return 0;
    }

    preparar_cambio();
    guardar_activa();

    idx = g.num_pestanas++;
    if (!titulo || !titulo[0]) {
        formatear(auto_titulo, sizeof(auto_titulo), "Sin titulo %d", idx + 1);
        titulo = auto_titulo;
    }
    poner_titulo(&g.pestanas[idx], titulo, ruta, zoom_idx);
    g.pestana_activa = idx;

    /* Tras guardar la activa, el estado global esta vacio. */
    soltar_estado_activo();
    g.doc = *nuevo;
    memset(nuevo, 0, sizeof(*nuevo));
    pila_iniciar(&g.deshacer, MAX_UNDO);
    pila_iniciar(&g.rehacer, MAX_UNDO);
    seleccion_limpiar(&g.sel);
    g.zoom_idx = zoom_idx;
    g.pan_x = g.pan_y = 0;
    g.vista = VISTA_PINTAR;
    g.modo_tiles = 0;
    g.tiles_visible = 0;
    g.tiles_plegado = 0;
    g.tile_cx = g.tile_cy = 0;
    g.tiles_layout_valido = 0;
    resetear_global_tras_cambio();
    lienzo_actualizar();
    app_actualizar_titulo();
    app_invalidar();
    return 1;
}

int pestanas_activar(int indice) {
    if (indice < 0 || indice >= g.num_pestanas) return 0;
    if (indice == g.pestana_activa) return 0;

    preparar_cambio();
    guardar_activa();
    g.pestana_activa = indice;
    cargar_activa();
    resetear_global_tras_cambio();
    app_actualizar_titulo();
    app_invalidar();
    return 1;
}

int pestanas_cerrar(int indice) {
    Pestana *p;
    char mensaje[256];
    int era_activa;

    if (indice < 0 || indice >= g.num_pestanas) return 0;
    p = &g.pestanas[indice];
    if (pestanas_esta_modificada(indice)) {
        formatear(mensaje, sizeof(mensaje),
                  "La pestana \"%s\" tiene cambios sin guardar.\n"
                  "¿Cerrarla de todos modos?",
                  p->titulo);
        if (!dialogo_confirmar("Cerrar pestana", mensaje)) return 0;
    }

    preparar_cambio();
    era_activa = (indice == g.pestana_activa);

    if (era_activa) {
        soltar_estado_activo();
        liberar_pestana(p);
        if (g.num_pestanas <= 1) {
            g.num_pestanas = 1;
            g.pestana_activa = 0;
            memset(&g.pestanas[0], 0, sizeof(g.pestanas[0]));
            activo_documento_nuevo("Sin titulo 1");
            resetear_global_tras_cambio();
        } else {
            memmove(&g.pestanas[indice], &g.pestanas[indice + 1],
                    (size_t)(g.num_pestanas - indice - 1) * sizeof(Pestana));
            g.num_pestanas--;
            memset(&g.pestanas[g.num_pestanas], 0, sizeof(Pestana));
            g.pestana_activa = indice < g.num_pestanas ? indice : g.num_pestanas - 1;
            cargar_activa();
            resetear_global_tras_cambio();
        }
    } else {
        liberar_pestana(p);
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
    int i;
    if (g.doc.modificado) return 1;
    for (i = 0; i < g.num_pestanas; i++) {
        if (i == g.pestana_activa) continue;
        if (g.pestanas[i].doc.modificado) return 1;
    }
    return 0;
}

void pestanas_marcar_guardada(const char *ruta) {
    Pestana *p;
    char nombre[MAX_PATH];
    if (g.pestana_activa < 0 || g.pestana_activa >= g.num_pestanas) return;
    p = &g.pestanas[g.pestana_activa];
    if (ruta && ruta[0]) {
        recortar_texto(p->ruta, ruta, sizeof(p->ruta));
        ruta_nombre(nombre, sizeof(nombre), ruta);
        recortar_texto(p->titulo, nombre, sizeof(p->titulo));
    }
    g.doc.modificado = 0;
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

const char *pestanas_titulo(int indice) {
    if (indice < 0 || indice >= g.num_pestanas) return "";
    return g.pestanas[indice].titulo;
}

const char *pestanas_ruta(int indice) {
    if (indice < 0 || indice >= g.num_pestanas) return "";
    return g.pestanas[indice].ruta;
}

int pestanas_esta_modificada(int indice) {
    if (indice < 0 || indice >= g.num_pestanas) return 0;
    if (indice == g.pestana_activa) return g.doc.modificado;
    return g.pestanas[indice].doc.modificado;
}
