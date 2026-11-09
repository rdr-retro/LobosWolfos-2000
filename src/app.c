/* ==========================================================================
 * Entrada de la aplicacion: menus, paneles, zoom, atajos y dialogos.
 * ========================================================================== */
#include "app.h"

static int arrastre = 0;          /* 0 nada, 1 panel, 10 zoom, 11 grosor, 12 opciones, 20 info */
static int arrastre_offset_x = 0;
static int arrastre_offset_y = 0;

/* ------------------------------------------------------------------ */
/* Acciones del menu                                                   */
/* ------------------------------------------------------------------ */
static int ruta_existe(const char *ruta) {
    return ruta && ruta[0] && GetFileAttributesA(ruta) != INVALID_FILE_ATTRIBUTES;
}

static int pedir_ruta_mapa(char *ruta) {
    OPENFILENAMEA ofn;
    ruta[0] = '\0';
    if (g.mapa_ruta[0]) {
        const char *p = g.mapa_ruta, *nombre = p;
        while (*p) {
            if (*p == '\\' || *p == '/') nombre = p + 1;
            p++;
        }
        strncpy(ruta, nombre, MAX_PATH - 1);
        ruta[MAX_PATH - 1] = 0;
    } else {
        strcpy(ruta, "mapa.mav");
    }
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = "Mapa de tiles (*.mav)\0*.mav\0Todos los archivos\0*.*\0\0";
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "mav";
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    return GetSaveFileNameA(&ofn) != 0;
}

static void guardar_mapa_actual(void) {
    char ruta[MAX_PATH];
    int pide = 0;
    if (g.mapa_ruta[0]) {
        strcpy(ruta, g.mapa_ruta);
    } else {
        if (!pedir_ruta_mapa(ruta)) return;
        pide = 1;
    }
    if (ruta_existe(ruta) &&
        !dialogo_confirmar("Sobrescribir mapa",
                           "El archivo ya existe.\n¿Sobrescribirlo?")) {
        return;
    }
    if (mapa_guardar(ruta)) {
        if (pide) {
            strncpy(g.mapa_ruta, ruta, MAX_PATH - 1);
            g.mapa_ruta[MAX_PATH - 1] = 0;
        }
        app_estado("Mapa guardado: %s", ruta);
    } else {
        dialogo_error("No se pudo guardar el mapa.");
    }
}

static void accion_archivo(int opcion) {
    int modo_mapa = (g.vista == VISTA_MAPA);
    int fila = opcion;
    if (!modo_mapa) {
        /* Sin editor de mapas las filas son: Abrir, Borrar, PNG, BMP. */
        switch (fila) {
            case 0: dialogo_abrir_imagen(); return;
            case 1: fila = 3; break;
            case 2: fila = 4; break;
            case 3: fila = 5; break;
            default: return;
        }
    }
    switch (fila) {
        case 0: guardar_mapa_actual(); break;
        case 1:
            if (ruta_existe(g.mapa_ruta) && mapa_guardar(g.mapa_ruta))
                app_estado("Mapa guardado: %s", g.mapa_ruta);
            break;
        case 2: dialogo_abrir_imagen(); break;
        case 3:
            pila_guardar(&g.deshacer, &g.doc);
            pila_limpiar(&g.rehacer);
            doc_borrar_capa(&g.doc);
            doc_componer_todo(&g.doc);
            app_invalidar();
            break;
        case 4: dialogo_guardar_png(); break;
        case 5: dialogo_exportar_bmp(); break;
        default: break;
    }
}

static void accion_modo(int opcion) {
    switch (opcion) {
        case 0:
            /* Modo pintar: sale de tiles y oculta el visor del patron. */
            if (g.modo_tiles) tiles_modo_salir();
            g.patron_visible = 0;
            app_cambiar_vista(VISTA_PINTAR);
            app_invalidar();
            break;
        case 1:
            g.patron_visible = 1;
            if (g.vista != VISTA_PINTAR) app_cambiar_vista(VISTA_PINTAR);
            app_invalidar();
            break;
        case 2:
            if (g.modo_tiles) {
                tiles_modo_salir();
            } else {
                tiles_modo_entrar();
            }
            app_invalidar();
            break;
        case 3:
            if (g.vista == VISTA_MAPA) {
                app_cambiar_vista(VISTA_PINTAR);
            } else {
                char ruta[MAX_PATH];
                if (g.modo_tiles) tiles_modo_salir();
                if (!pedir_ruta_mapa(ruta)) break;
                strncpy(g.mapa_ruta, ruta, MAX_PATH - 1);
                g.mapa_ruta[MAX_PATH - 1] = 0;
                mapa_reiniciar();
                if (ruta_existe(ruta)) mapa_cargar(ruta);
                app_cambiar_vista(VISTA_MAPA);
            }
            break;
        default:
            break;
    }
}

static void accion_editar(int opcion) {
    switch (opcion) {
        case 0:
            if (pila_deshacer(&g.deshacer, &g.rehacer, &g.doc)) {
                doc_componer_todo(&g.doc);
                lienzo_actualizar();
                app_invalidar();
            }
            break;
        case 1:
            if (pila_rehacer(&g.rehacer, &g.deshacer, &g.doc)) {
                doc_componer_todo(&g.doc);
                lienzo_actualizar();
                app_invalidar();
            }
            break;
        case 2: pintura_copiar(1); break;
        case 3: pintura_copiar(0); break;
        case 4: pintura_pegar(); break;
        default: break;
    }
}

static void accion_capa(int opcion) {
    pila_guardar(&g.deshacer, &g.doc);
    pila_limpiar(&g.rehacer);
    switch (opcion) {
        case 0: doc_agregar_capa(&g.doc, NULL); break;
        case 1: doc_duplicar_capa(&g.doc); break;
        case 2: doc_eliminar_capa(&g.doc, g.doc.activa); break;
        case 3: doc_mover_capa(&g.doc, g.doc.activa, 1); break;
        case 4: doc_mover_capa(&g.doc, g.doc.activa, -1); break;
        case 5: doc_fusionar_abajo(&g.doc); break;
        default: break;
    }
    doc_componer_todo(&g.doc);
    app_invalidar();
}

static void accion_colores(int opcion) {
    switch (opcion) {
        case 0: dialogo_elegir_color(0); break;
        case 1: dialogo_elegir_color(1); break;
        case 2: {
            u32 t = g.frente; g.frente = g.fondo; g.fondo = t;
            g.casilla_elegida = g.casilla_elegida ? 0 : 1;
            app_invalidar();
            break;
        }
        default: break;
    }
}

static void accion_seleccionar(int opcion) {
    switch (opcion) {
        case 0: seleccion_todo(&g.sel, g.doc.ancho, g.doc.alto); break;
        case 1: seleccion_limpiar(&g.sel); break;
        case 2: seleccion_invertir(&g.sel, g.doc.ancho, g.doc.alto); break;
        default: break;
    }
    app_invalidar();
}

static void ejecutar_menu(int menu, int opcion) {
    switch (menu) {
        case 0: accion_archivo(opcion); break;
        case 1: accion_modo(opcion); break;
        case 2: accion_editar(opcion); break;
        case 4: accion_seleccionar(opcion); break;
        case 5: accion_capa(opcion); break;
        case 6: accion_colores(opcion); break;
        case 7:
            g.info_visible = 1;
            app_invalidar();
            break;
        default: break;
    }
}

/* ------------------------------------------------------------------ */
/* Menu desplegable                                                    */
/* ------------------------------------------------------------------ */
static int opcion_menu_en(int x, int y) {
    return menu_fila_en(x, y);
}

/* ------------------------------------------------------------------ */
/* Canvas: zoom y pan                                                  */
/* ------------------------------------------------------------------ */
static void zoom_desde_pista(int x) {
    Zona pista = zoom_pista_zona();
    float t;
    int idx;
    if (pista.ancho <= ZOOM_KNOB_ANCHO) return;
    t = (float)(x - pista.x - ZOOM_KNOB_ANCHO / 2) / (float)(pista.ancho - ZOOM_KNOB_ANCHO);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    idx = redondear(t * 18.0f);
    zoom_aplicar(idx);
}

static void grosor_desde_pista(int x) {
    Zona pista = grosor_pista_zona();
    float t;
    int idx;
    if (pista.ancho <= ZOOM_KNOB_ANCHO) return;
    t = (float)(x - pista.x - ZOOM_KNOB_ANCHO / 2) / (float)(pista.ancho - ZOOM_KNOB_ANCHO);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    idx = redondear(t * 9.0f);
    g.grosor_idx = limitar(idx, 0, 9);
    app_invalidar();
}

/* ------------------------------------------------------------------ */
/* Panel de opciones                                                   */
/* ------------------------------------------------------------------ */
/* Deslizador del pincel bajo el raton. Solo cuenta la pista; la etiqueta y
 * el titulo no responden. */
static int opciones_fila_en(int x, int y) {
    int i;
    if (!g.panel_opciones) return -1;
    if (!zona_contiene(opciones_zona(), x, y)) return -1;
    for (i = 0; i < OPCIONES_NUM_FILAS; i++) {
        if (zona_contiene(opciones_pista_zona(i), x, y)) return i;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Info                                                                */
/* ------------------------------------------------------------------ */
static Zona info_zona_app(void) {
    int x = (g.ancho - 448) / 2;
    int y = (g.alto - 382) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    return zona_hacer(x, y, 448, 382);
}

static Zona info_cerrar_app(void) {
    Zona v = info_zona_app();
    int derecha = zona_derecha(v) < g.ancho ? zona_derecha(v) : g.ancho;
    int x = derecha - 18 - 3;
    if (x < v.x) x = v.x;
    return zona_hacer(x, v.y + 3, 18, 18);
}

static int info_en(int x, int y) {
    if (!g.info_visible) return 0;
    return zona_contiene(info_zona_app(), x, y);
}

/* ------------------------------------------------------------------ */
/* Visor del patron                                                    */
/* ------------------------------------------------------------------ */
int clic_patron(int x, int y) {
    Zona v, cerrar;
    if (!g.patron_visible || g.vista != VISTA_PINTAR) return 0;
    v = patron_zona();
    if (!zona_contiene(v, x, y)) return 0;
    cerrar = patron_minimizar_zona();
    if (zona_contiene(cerrar, x, y)) {
        g.patron_visible = 0;
        app_invalidar();
        return 1;
    }
    if (y < v.y + 22) {
        arrastre = 30;
        arrastre_offset_x = x - v.x;
        arrastre_offset_y = y - v.y;
        return 1;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Clics por zona                                                      */
/* ------------------------------------------------------------------ */
int clic_menu(int x, int y) {
    int i = menu_barra_en(x, y);
    if (i >= 0) {
        g.menu_abierto = (g.menu_abierto == i) ? -1 : i;
        app_invalidar();
        return 1;
    }
    return 0;
}

int clic_sidebar(int x, int y) {
    int i;
    for (i = 0; i < H_TOTAL; i++) {
        if (zona_contiene(herramienta_zona(i), x, y)) {
            app_poner_herramienta(i);
            return 1;
        }
    }
    if (zona_contiene(panel_textura_zona(), x, y)) {
        g.textura_menu_abierto = !g.textura_menu_abierto;
        app_invalidar();
        return 1;
    }
    if (g.textura_menu_abierto) {
        int juego = textura_juego_actual();
        int muestra = textura_muestra_en(x, y, juego);
        if (muestra >= 0) {
            if (juego) g.rocio = muestra;
            else g.textura = muestra;
            g.textura_menu_abierto = 0;
            app_invalidar();
            return 1;
        }
    }
    return 0;
}

int clic_topbar(int x, int y) {
    int i;
    Zona sup, inf;
    /* Logo: cambia que cuadro recibe el color. */
    {
        Zona barra = barra_zona();
        int lx = barra.x + CUADRO_MARGEN;
        int conjunto = CUADRO_LADO + CUADRO_DESFASE;
        int ly = barra.y + (BARRA_ALTO - conjunto) / 2;
        if (ly < barra.y + 2) ly = barra.y + 2;
        sup = zona_hacer(lx, ly, CUADRO_LADO, CUADRO_LADO);
        inf = zona_hacer(lx + CUADRO_DESFASE, ly + CUADRO_DESFASE, CUADRO_LADO, CUADRO_LADO);
    }
    if (zona_contiene(sup, x, y)) {
        g.casilla_elegida = 0;
        app_invalidar();
        return 1;
    }
    if (zona_contiene(inf, x, y)) {
        g.casilla_elegida = 1;
        app_invalidar();
        return 1;
    }
    for (i = 0; i < MINI_TOTAL; i++) {
        if (zona_contiene(mini_zona(i), x, y)) {
            g.mini_elegido = i;
            if (g.paleta_llena[i]) {
                if (g.casilla_elegida == 0) g.frente = g.paleta[i];
                else g.fondo = g.paleta[i];
            } else {
                dialogo_elegir_color(g.casilla_elegida == 1);
            }
            app_invalidar();
            return 1;
        }
    }
    if (zona_contiene(arcoiris_zona(), x, y)) {
        dialogo_elegir_color(g.casilla_elegida == 1);
        return 1;
    }
    if (zona_contiene(boton_guardar_tile_zona(), x, y)) {
        if (tiles_guardar_actual()) {
            if (g.vista == VISTA_MAPA) {
                g.mapa_tile_sel = g.num_tiles - 1;
                g.mapa.tile_sel = g.num_tiles - 1;
            }
            app_invalidar();
        }
        return 1;
    }
    return 0;
}

int clic_barra_abajo(int x, int y) {
    Zona pista = zoom_pista_zona();
    Zona menos = zoom_boton_menos();
    Zona mas = zoom_boton_mas();
    Zona grosor = grosor_pista_zona();
    if (zona_contiene(menos, x, y)) {
        zoom_aplicar(g.zoom_idx - 1);
        return 1;
    }
    if (zona_contiene(mas, x, y)) {
        zoom_aplicar(g.zoom_idx + 1);
        return 1;
    }
    if (zona_contiene(pista, x, y)) {
        arrastre = 10;
        zoom_desde_pista(x);
        return 1;
    }
    if (zona_contiene(grosor, x, y)) {
        arrastre = 11;
        grosor_desde_pista(x);
        return 1;
    }
    return 0;
}

/* Indice del tile del selector bajo el cursor. Solo cuentan las miniaturas
 * realmente visibles, como en `tile_guardado_en`. */
static int tile_panel_indice_en(int x, int y) {
    int total = g.num_tiles;
    int visibles = total - g.tiles_scroll;
    int i;
    if (visibles > TILES_SELECTOR_VISIBLES) visibles = TILES_SELECTOR_VISIBLES;
    if (visibles < 0) visibles = 0;
    for (i = 0; i < visibles; i++) {
        if (zona_contiene(tile_mini_zona(i), x, y))
            return total - 1 - (g.tiles_scroll + i);
    }
    return -1;
}

/* Indice de la capa cuya fila esta bajo el cursor. */
static int capa_panel_indice_en(int x, int y) {
    Zona mas, borrar;
    int limite;
    int i;
    if (botones_panel_derecho_zona(&mas, &borrar)) limite = mas.y - 8;
    else limite = g.alto - 8;
    for (i = 0; i < g.doc.num_capas; i++) {
        Zona z = capa_fila_zona(i);
        if (zona_abajo(z) > limite) break;
        if (zona_contiene(z, x, y)) return g.doc.num_capas - 1 - i;
    }
    return -1;
}

int clic_panel_derecho(int x, int y, int boton) {
    int panel = sidebar_derecho_ancho();
    Zona mas, borrar;
    int indice;

    /* Modal de confirmacion abierto: cualquier clic izquierdo lo cierra y,
     * si cae en SI, aplica el borrado. El derecho solo se consume. */
    if (g.confirmar_borrado || g.confirmar_tile >= 0) {
        if (boton == 0) {
            Zona si, no;
            confirmar_panel_zona(&si, &no);
            if (zona_contiene(si, x, y)) {
                if (g.confirmar_tile >= 0) {
                    indice = g.confirmar_tile;
                    if (indice >= 0 && indice < g.num_tiles) {
                        DeleteFileA(g.tiles[indice].ruta);
                        tiles_cargar();
                        if (g.mapa_tile_sel >= g.num_tiles) g.mapa_tile_sel = g.num_tiles - 1;
                        if (g.mapa.tile_sel >= g.num_tiles) g.mapa.tile_sel = g.num_tiles - 1;
                    }
                } else {
                    pintura_confirmar_seleccion();
                    pila_guardar(&g.deshacer, &g.doc);
                    pila_limpiar(&g.rehacer);
                    indice = g.doc.activa;
                    doc_eliminar_capa(&g.doc, indice);
                    if (g.doc.activa > 0) g.doc.activa = indice > 0 ? indice - 1 : 0;
                    if (g.doc.activa >= g.doc.num_capas) g.doc.activa = g.doc.num_capas - 1;
                    doc_componer_todo(&g.doc);
                }
                app_invalidar();
            }
            g.confirmar_tile = -1;
            g.confirmar_borrado = 0;
        }
        return 1;
    }

    /* Menu contextual de capa: el izquierdo aplica o cierra; el derecho solo
     * lo cierra si se pulsa fuera del panel. */
    if (g.capa_menu_abierto >= 0) {
        Zona menu, anadir, quitar;
        menu = menu_capa_panel_zona(&anadir, &quitar);
        if (boton == 0) {
            indice = g.capa_menu_abierto;
            if (indice >= 0 && indice < g.doc.num_capas) {
                int tiene = doc_capa_tiene_alfa(&g.doc, indice);
                if (zona_contiene(anadir, x, y) && !tiene) {
                    doc_poner_alfa(&g.doc, indice, 1);
                    doc_componer_todo(&g.doc);
                } else if (zona_contiene(quitar, x, y) && tiene) {
                    doc_poner_alfa(&g.doc, indice, 0);
                    doc_componer_todo(&g.doc);
                }
            }
            g.capa_menu_abierto = -1;
            app_invalidar();
        } else if (!zona_contiene(menu, x, y)) {
            g.capa_menu_abierto = -1;
            app_invalidar();
        }
        return 1;
    }

    if (x < g.ancho - panel || y < AREA_SUP_ALTO) return 0;

    if (boton == 1) {
        /* Clic derecho sobre una miniatura: confirma el borrado del tile. */
        indice = tile_panel_indice_en(x, y);
        if (indice >= 0) {
            g.confirmar_borrado = 0;
            g.confirmar_tile = indice;
            app_invalidar();
            return 1;
        }
        /* Clic derecho sobre una capa: abre su menu de canal alfa. */
        indice = capa_panel_indice_en(x, y);
        if (indice >= 0) {
            pintura_confirmar_seleccion();
            g.doc.activa = indice;
            g.capa_menu_abierto = indice;
            g.capa_menu_x = x;
            g.capa_menu_y = y;
            app_invalidar();
            return 1;
        }
        return 1;
    }

    if (boton != 0) return 1;

    /* Botones + y x del pie. */
    if (botones_panel_derecho_zona(&mas, &borrar)) {
        if (zona_contiene(mas, x, y)) {
            pintura_confirmar_seleccion();
            pila_guardar(&g.deshacer, &g.doc);
            pila_limpiar(&g.rehacer);
            doc_agregar_capa(&g.doc, NULL);
            doc_componer_todo(&g.doc);
            app_invalidar();
            return 1;
        }
        if (zona_contiene(borrar, x, y)) {
            if (g.doc.num_capas > 1) {
                g.confirmar_tile = -1;
                g.confirmar_borrado = 1;
                app_invalidar();
            }
            return 1;
        }
    }

    /* Clic izquierdo sobre una miniatura: en el editor de mapas elige el
     * tile que se colocara despues en la rejilla. */
    indice = tile_panel_indice_en(x, y);
    if (indice >= 0) {
        if (g.vista == VISTA_MAPA) {
            g.mapa.tile_sel = indice;
            g.mapa_tile_sel = indice;
            app_estado("Tile elegido: %s", g.tiles[indice].nombre);
            app_invalidar();
        }
        return 1;
    }

    /* Clic izquierdo sobre una capa: el ojo alterna la visibilidad y el resto
     * de la fila selecciona la capa. */
    indice = capa_panel_indice_en(x, y);
    if (indice >= 0) {
        Zona z = capa_fila_zona(g.doc.num_capas - 1 - indice);
        if (zona_contiene(capa_ojo_zona(z), x, y)) {
            g.doc.capas[indice].visible = !g.doc.capas[indice].visible;
            doc_componer_todo(&g.doc);
        } else if (indice != g.doc.activa) {
            pintura_confirmar_seleccion();
            g.doc.activa = indice;
        }
        app_invalidar();
        return 1;
    }

    return 1;
}

int clic_opciones(int x, int y) {
    Zona panel;
    int i;
    if (!g.panel_opciones) {
        /* Plegada: pulsar la tira del sidebar la restaura. */
        if (zona_contiene(opciones_minimizada_zona(), x, y)) {
            g.panel_opciones = 1;
            app_invalidar();
            return 1;
        }
        return 0;
    }
    panel = opciones_zona();
    if (!zona_contiene(panel, x, y)) return 0;
    if (zona_contiene(opciones_minimizar_zona(), x, y)) {
        g.panel_opciones = 0;
        g.opcion_arrastrada = -1;
        g.cubo_arrastre = -1;
        app_invalidar();
        return 1;
    }
    /* La ventana del cubo tiene sus propios controles; el resto de
     * herramientas usa los deslizadores del pincel. */
    if (g.herramienta == H_CUBO) {
        int control = cubo_control_en(x, y);
        if (control >= 0) {
            g.cubo_arrastre = cubo_es_deslizador(control) ? control : -1;
            cubo_aplicar_control(control, x);
            app_invalidar();
            return 1;
        }
        if (zona_contiene(opciones_titulo_zona(), x, y)) {
            arrastre = 1;
            arrastre_offset_x = x - panel.x;
            arrastre_offset_y = y - panel.y;
            return 1;
        }
        return 1;
    }
    i = opciones_fila_en(x, y);
    if (i >= 0) {
        g.opcion_arrastrada = i;
        opciones_fijar_desde_x(i, x);
        return 1;
    }
    if (zona_contiene(opciones_titulo_zona(), x, y)) {
        arrastre = 1;
        arrastre_offset_x = x - panel.x;
        arrastre_offset_y = y - panel.y;
        return 1;
    }
    /* El resto de la ventana no toca el lienzo. */
    return 1;
}

int clic_info(int x, int y) {
    Zona v;
    if (!g.info_visible) return 0;
    v = info_zona_app();
    if (!zona_contiene(v, x, y)) return 0;
    if (zona_contiene(info_cerrar_app(), x, y)) {
        g.info_visible = 0;
        app_invalidar();
        return 1;
    }
    if (y < v.y + 24) {
        arrastre = 20;
        arrastre_offset_x = x - v.x;
        arrastre_offset_y = y - v.y;
        return 1;
    }
    return 1;
}

int clic_lienzo(int x, int y) {
    if (g.vista == VISTA_MAPA) {
        mapa_pulsar(x, y, 0);
        return 1;
    }
    (void)x; (void)y;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Entrada principal                                                   */
/* ------------------------------------------------------------------ */
static void clic_etiqueta_menu(int menu) {
    if (menu == 7) {
        g.menu_abierto = -1;
        g.textura_menu_abierto = 0;
        g.info_visible = 1;
        app_invalidar();
        return;
    }
    if (menu == 3) {
        /* GESTOR DE MODULOS: espacio de trabajo en blanco. */
        g.menu_abierto = -1;
        app_alternar_gestor_modulos();
        app_invalidar();
        return;
    }
    if (menu == 5 || menu == 6) {
        /* CAPA y COLORES no tienen desplegable: solo su etiqueta. */
        g.menu_abierto = -1;
        app_invalidar();
        return;
    }
    g.menu_abierto = (g.menu_abierto == menu) ? -1 : menu;
    app_invalidar();
}

/* ------------------------------------------------------------------ */
/* Pestanas: entrada                                                   */
/* ------------------------------------------------------------------ */
int clic_pestanas(int x, int y, int boton) {
    int i, idx;
    if (y < 0 || y >= TAB_BARRA_ALTO) return 0;

    g.menu_abierto = -1;
    g.textura_menu_abierto = 0;

    /* Rueda/central: cerrar la pestana apuntada. */
    if (boton == 2) {
        idx = pestana_en(x, y);
        if (idx >= 0) pestanas_cerrar(idx);
        return 1;
    }
    if (boton != 0) return 1;

    for (i = 0; i < g.num_pestanas; i++) {
        if (zona_contiene(pestana_cerrar_zona(i), x, y)) {
            pestanas_cerrar(i);
            return 1;
        }
    }
    idx = pestana_en(x, y);
    if (idx >= 0) {
        pestanas_activar(idx);
        return 1;
    }
    if (zona_contiene(pestana_nueva_zona(), x, y)) {
        pestanas_nueva_modo();
        return 1;
    }
    /* Clic en una zona vacia de la franja: no pasa al resto de la interfaz. */
    return 1;
}

/* ------------------------------------------------------------------ */
/* Gestor de modulos: boton arrastrable, lienzo y acople magnetico     */
/* ------------------------------------------------------------------ */
/* Ajusta una posicion para acoplar los lados del rectangulo a otro
 * elemento ya colocado. `linea_x`/`linea_y` reciben la guia de acople en
 * coordenadas del mundo, o -1 si ese eje no se ha acoplado. */
void modulos_acoplar(int *x, int *y, int ancho, int alto, int ignorar,
                     int *linea_x, int *linea_y) {
    int i;
    int mejor_x = MODULOS_IMAN + 1, mejor_y = MODULOS_IMAN + 1;
    int dx = 0, dy = 0, lx = -1, ly = -1;

    for (i = 0; i < g.num_modulos; i++) {
        ModuloElemento *e = &g.modulos[i];
        int cerca_v, cerca_h;
        if (i == ignorar) continue;

        /* Solo se acoplan lados que se miran: en horizontal los rectangulos
         * deben compartir altura (o estar cerca) y viceversa. */
        cerca_v = *y < e->y + e->alto + MODULOS_IMAN &&
                  *y + alto > e->y - MODULOS_IMAN;
        cerca_h = *x < e->x + e->ancho + MODULOS_IMAN &&
                  *x + ancho > e->x - MODULOS_IMAN;

        if (cerca_v) {
            int cand[4], guia[4], k;
            cand[0] = e->x - (*x + ancho);            /* acoplar a su izquierda */
            cand[1] = e->x + e->ancho - *x;           /* acoplar a su derecha */
            cand[2] = e->x - *x;                      /* alinear izquierdas */
            cand[3] = e->x + e->ancho - (*x + ancho); /* alinear derechas */
            guia[0] = guia[2] = e->x;
            guia[1] = guia[3] = e->x + e->ancho;
            for (k = 0; k < 4; k++) {
                int d = cand[k] < 0 ? -cand[k] : cand[k];
                if (d < mejor_x) {
                    mejor_x = d;
                    dx = cand[k];
                    lx = guia[k];
                }
            }
        }
        if (cerca_h) {
            int cand[4], guia[4], k;
            cand[0] = e->y - (*y + alto);             /* acoplar encima */
            cand[1] = e->y + e->alto - *y;            /* acoplar debajo */
            cand[2] = e->y - *y;                      /* alinear superiores */
            cand[3] = e->y + e->alto - (*y + alto);   /* alinear inferiores */
            guia[0] = guia[2] = e->y;
            guia[1] = guia[3] = e->y + e->alto;
            for (k = 0; k < 4; k++) {
                int d = cand[k] < 0 ? -cand[k] : cand[k];
                if (d < mejor_y) {
                    mejor_y = d;
                    dy = cand[k];
                    ly = guia[k];
                }
            }
        }
    }

    if (mejor_x <= MODULOS_IMAN) *x += dx;
    else lx = -1;
    if (mejor_y <= MODULOS_IMAN) *y += dy;
    else ly = -1;

    if (linea_x) *linea_x = lx;
    if (linea_y) *linea_y = ly;
}

/* Elemento bajo el cursor, de arriba (ultimo dibujado) a abajo. */
static int modulos_elemento_en(int x, int y) {
    Zona lienzo = modulos_lienzo_zona();
    int i;
    for (i = g.num_modulos - 1; i >= 0; i--) {
        ModuloElemento *e = &g.modulos[i];
        int x0 = lienzo.x + g.pan_x + e->x;
        int y0 = lienzo.y + g.pan_y + e->y;
        if (x >= x0 && x < x0 + e->ancho &&
            y >= y0 && y < y0 + e->alto)
            return i;
    }
    return -1;
}

static void modulos_al_frente(int idx) {
    ModuloElemento tmp;
    int i;
    if (idx < 0 || idx >= g.num_modulos || idx == g.num_modulos - 1) return;
    tmp = g.modulos[idx];
    for (i = idx; i < g.num_modulos - 1; i++) g.modulos[i] = g.modulos[i + 1];
    g.modulos[g.num_modulos - 1] = tmp;
}

/* Inserta un elemento centrado en un punto de pantalla, con acople. */
void modulos_insertar(int x, int y) {
    Zona lienzo = modulos_lienzo_zona();
    ModuloElemento *e;
    int wx, wy;

    if (!zona_contiene(lienzo, x, y) || g.num_modulos >= MODULOS_MAX) return;

    wx = x - lienzo.x - g.pan_x - MODULOS_ELEMENTO_ANCHO / 2;
    wy = y - lienzo.y - g.pan_y - MODULOS_ELEMENTO_ALTO / 2;
    modulos_acoplar(&wx, &wy, MODULOS_ELEMENTO_ANCHO, MODULOS_ELEMENTO_ALTO,
                    -1, NULL, NULL);

    e = &g.modulos[g.num_modulos++];
    e->x = wx;
    e->y = wy;
    e->ancho = MODULOS_ELEMENTO_ANCHO;
    e->alto = MODULOS_ELEMENTO_ALTO;
}

static void modulos_pulsar(int x, int y, int boton) {
    Zona lienzo = modulos_lienzo_zona();

    /* Boton del panel derecho: empieza el arrastre de un elemento nuevo. */
    if (boton == 0 && zona_contiene(modulos_boton_zona(), x, y)) {
        g.modulo_arrastrando = 1;
        g.hover_modulo_boton = 1;
        g.modulo_snap_linea_x = -1;
        g.modulo_snap_linea_y = -1;
        app_invalidar();
        return;
    }

    if (zona_contiene(lienzo, x, y)) {
        /* Con el boton izquierdo, si hay un elemento debajo se mueve. */
        if (boton == 0) {
            int idx = modulos_elemento_en(x, y);
            if (idx >= 0) {
                ModuloElemento *e;
                int wx = x - lienzo.x - g.pan_x;
                int wy = y - lienzo.y - g.pan_y;
                modulos_al_frente(idx);
                idx = g.num_modulos - 1;
                e = &g.modulos[idx];
                g.modulo_arrastrando = 3;
                g.modulo_drag_i = idx;
                g.modulo_elem_off_x = wx - e->x;
                g.modulo_elem_off_y = wy - e->y;
                g.modulo_snap_linea_x = -1;
                g.modulo_snap_linea_y = -1;
                app_invalidar();
                return;
            }
        }
        /* Zona vacia (o boton central): desplazar el lienzo. */
        if (boton == 0 || boton == 2) {
            g.modulo_arrastrando = 2;
            g.modulo_drag_x = x - g.pan_x;
            g.modulo_drag_y = y - g.pan_y;
            app_invalidar();
            return;
        }
    }
    /* Los paneles vacios no responden a clics. */
}

static void modulos_soltar(int x, int y) {
    if (g.modulo_arrastrando == 1) {
        modulos_insertar(x, y);
        g.modulo_arrastrando = 0;
        g.modulo_snap_linea_x = -1;
        g.modulo_snap_linea_y = -1;
        app_invalidar();
        return;
    }

    if (g.modulo_arrastrando == 2 || g.modulo_arrastrando == 3) {
        g.modulo_arrastrando = 0;
        g.modulo_snap_linea_x = -1;
        g.modulo_snap_linea_y = -1;
        app_invalidar();
    }
}

static void modulos_mover(int x, int y) {
    int hover;

    if (g.modulo_arrastrando == 2) {
        g.pan_x = x - g.modulo_drag_x;
        g.pan_y = y - g.modulo_drag_y;
        app_invalidar();
        return;
    }
    if (g.modulo_arrastrando == 1) {
        app_invalidar();
        return;
    }
    if (g.modulo_arrastrando == 3) {
        Zona lienzo = modulos_lienzo_zona();
        ModuloElemento *e = &g.modulos[g.modulo_drag_i];
        int wx = x - lienzo.x - g.pan_x - g.modulo_elem_off_x;
        int wy = y - lienzo.y - g.pan_y - g.modulo_elem_off_y;
        modulos_acoplar(&wx, &wy, e->ancho, e->alto, g.modulo_drag_i,
                        &g.modulo_snap_linea_x, &g.modulo_snap_linea_y);
        e->x = wx;
        e->y = wy;
        app_invalidar();
        return;
    }

    hover = zona_contiene(modulos_boton_zona(), x, y);
    if (hover != g.hover_modulo_boton) {
        g.hover_modulo_boton = hover;
        app_invalidar();
    }
}

static void modulos_rueda(int x, int y, int delta, int ctrl) {
    if (!zona_contiene(modulos_lienzo_zona(), x, y)) return;
    if (ctrl) {
        g.pan_x += delta > 0 ? MODULOS_CUADRICULA : -MODULOS_CUADRICULA;
    } else {
        g.pan_y += delta > 0 ? MODULOS_CUADRICULA : -MODULOS_CUADRICULA;
    }
    app_invalidar();
}

void app_pulsar(int x, int y, int boton) {
    g.raton_x = x;
    g.raton_y = y;

    /* El menu contextual de capa y las confirmaciones bloquean el resto de la
     * interfaz mientras estan abiertos. */
    if (g.confirmar_borrado || g.confirmar_tile >= 0 || g.capa_menu_abierto >= 0) {
        clic_panel_derecho(x, y, boton);
        return;
    }

    /* Un clic fuera del circulo de textura o de su menu cierra el menu. */
    if (g.textura_menu_abierto) {
        int juego = textura_juego_actual();
        if (!zona_contiene(panel_textura_zona(), x, y) &&
            textura_muestra_en(x, y, juego) < 0) {
            g.textura_menu_abierto = 0;
            app_invalidar();
        }
    }

    /* La franja de pestanas esta por encima de la barra de menus. */
    if (y < TAB_BARRA_ALTO) {
        clic_pestanas(x, y, boton);
        return;
    }

    /* Menu abierto: tiene prioridad. */
    if (g.menu_abierto >= 0) {
        int op = opcion_menu_en(x, y);
        if (op >= 0) {
            int menu = g.menu_abierto;
            g.menu_abierto = -1;
            if (menu_fila_activa(menu, op)) ejecutar_menu(menu, op);
            app_invalidar();
            return;
        }
        if (y >= TAB_BARRA_ALTO && y < AREA_SUP_ALTO) {
            int menu = menu_barra_en(x, y);
            if (menu >= 0) {
                clic_etiqueta_menu(menu);
            } else {
                g.menu_abierto = -1;
                app_invalidar();
            }
            return;
        }
        /* Un clic fuera del desplegable solo lo cierra. */
        g.menu_abierto = -1;
        app_invalidar();
        return;
    }

    if (y >= TAB_BARRA_ALTO && y < AREA_SUP_ALTO) {
        int menu = menu_barra_en(x, y);
        if (menu >= 0) clic_etiqueta_menu(menu);
        return;
    }

    /* El gestor de modulos tiene su propio boton y lienzo desplazable. */
    if (g.vista == VISTA_MODULOS) {
        modulos_pulsar(x, y, boton);
        return;
    }

    if (g.vista == VISTA_PINTAR && texto_pulsar(x, y, boton)) return;

    if (clic_info(x, y)) return;
    if (clic_patron(x, y)) return;
    if (clic_tiles_visor(x, y)) return;
    if (g.info_visible && !info_en(x, y)) {
        /* El resto de la interfaz sigue funcionando, la info se queda. */
    }
    if (clic_opciones(x, y)) return;

    if (y >= AREA_SUP_ALTO && y < AREA_SUP_ALTO + BARRA_ALTO) {
        if (clic_topbar(x, y)) return;
    }
    if (x < SIDEBAR_ANCHO) {
        if (clic_sidebar(x, y)) return;
        return;
    }
    if (y >= g.alto - BARRA_ABAJO_ALTO) {
        if (clic_barra_abajo(x, y)) return;
        return;
    }
    if (clic_panel_derecho(x, y, boton)) return;

    /* Lienzo. */
    if (g.vista == VISTA_MAPA) {
        mapa_pulsar(x, y, boton);
        return;
    }
    if (g.modo_tiles && tiles_modo_pulsar(x, y, boton)) return;
    pintura_pulsar(x, y, boton);
}

void app_soltar(int x, int y, int boton) {
    g.raton_x = x;
    g.raton_y = y;
    g.opcion_arrastrada = -1;
    g.cubo_arrastre = -1;
    if (g.vista == VISTA_MODULOS) {
        modulos_soltar(x, y);
        return;
    }
    if (g.arrastre_tiles) {
        tiles_modo_soltar();
        app_invalidar();
        return;
    }
    if (arrastre == 30) {
        Zona v = patron_zona();
        g.patron_x = x - arrastre_offset_x;
        g.patron_y = y - arrastre_offset_y;
        if (g.patron_x + v.ancho > g.ancho) g.patron_x = g.ancho - v.ancho;
        if (g.patron_y + v.alto > g.alto) g.patron_y = g.alto - v.alto;
        if (g.patron_x < 0) g.patron_x = 0;
        if (g.patron_y < AREA_SUP_ALTO) g.patron_y = AREA_SUP_ALTO;
        arrastre = 0;
        app_invalidar();
        return;
    }
    if (arrastre == 1 || arrastre == 12 || arrastre == 20) {
        if (arrastre == 20) {
            /* Nada que soltar: la info mantiene su posicion. */
        }
        arrastre = 0;
        return;
    }
    if (arrastre == 10 || arrastre == 11) {
        arrastre = 0;
        return;
    }
    if (g.vista == VISTA_MAPA) {
        mapa_soltar(x, y, boton);
        return;
    }
    if (g.vista == VISTA_PINTAR) texto_soltar(x, y, boton);
    pintura_soltar(x, y, boton);
}

void app_mover(int x, int y, int botones) {
    int i;
    g.raton_x = x;
    g.raton_y = y;

    /* En el gestor se actualiza el arrastre y el hover del boton. */
    if (g.vista == VISTA_MODULOS) {
        modulos_mover(x, y);
        if (y < AREA_SUP_ALTO) app_invalidar();
        return;
    }

    /* La franja de pestanas solo necesita repintar el hover. */
    if (y >= 0 && y < TAB_BARRA_ALTO && arrastre == 0 && !g.arrastre_tiles &&
        g.cubo_arrastre < 0 && g.opcion_arrastrada < 0) {
        if (g.hover_herramienta >= 0) {
            g.hover_herramienta = -1;
            app_actualizar_titulo();
        }
        app_invalidar();
        return;
    }

    /* Los menus no cambian al pasar el raton: solo al pulsar una etiqueta. */

    if (arrastre == 1) {
        Zona p;
        g.panel_opciones_x = x - arrastre_offset_x;
        g.panel_opciones_y = y - arrastre_offset_y;
        /* La posicion se recorta al area de trabajo. */
        p = opciones_zona();
        g.panel_opciones_x = p.x;
        g.panel_opciones_y = p.y;
        app_invalidar();
        return;
    }
    if (g.arrastre_tiles) {
        tiles_modo_mover_visor(x, y);
        return;
    }
    if (arrastre == 10) { zoom_desde_pista(x); return; }
    if (arrastre == 11) { grosor_desde_pista(x); return; }
    if (g.cubo_arrastre >= 0) {
        cubo_aplicar_control(g.cubo_arrastre, x);
        app_invalidar();
        return;
    }
    if (g.opcion_arrastrada >= 0) {
        opciones_fijar_desde_x(g.opcion_arrastrada, x);
        return;
    }
    if (arrastre == 12) {
        int idx = opciones_fila_en(x, y);
        if (idx >= 0) opciones_fijar_desde_x(idx, x);
        return;
    }
    if (arrastre == 20) {
        g.info_x = x - arrastre_offset_x;
        g.info_y = y - arrastre_offset_y;
        app_invalidar();
        return;
    }
    if (arrastre == 30) {
        Zona v = patron_zona();
        g.patron_x = x - arrastre_offset_x;
        g.patron_y = y - arrastre_offset_y;
        if (g.patron_x + v.ancho > g.ancho) g.patron_x = g.ancho - v.ancho;
        if (g.patron_y + v.alto > g.alto) g.patron_y = g.alto - v.alto;
        if (g.patron_x < 0) g.patron_x = 0;
        if (g.patron_y < AREA_SUP_ALTO) g.patron_y = AREA_SUP_ALTO;
        app_invalidar();
        return;
    }

    /* Hover de herramientas, paleta y panel. */
    {
        int anterior = g.hover_herramienta;
        g.hover_herramienta = -1;
        for (i = 0; i < H_TOTAL; i++) {
            if (zona_contiene(herramienta_zona(i), x, y)) { g.hover_herramienta = i; break; }
        }
        g.hover_paleta = -1;
        for (i = 0; i < MINI_TOTAL; i++) {
            if (zona_contiene(mini_zona(i), x, y)) { g.hover_paleta = i; break; }
        }
        g.hover_tile = -1;
        for (i = 0; i < TILES_SELECTOR_VISIBLES; i++) {
            if (zona_contiene(tile_mini_zona(i), x, y)) {
                int idx = g.num_tiles - 1 - (g.tiles_scroll + i);
                if (idx >= 0 && idx < g.num_tiles) g.hover_tile = idx;
                break;
            }
        }
        g.hover_capa = -1;
        for (i = 0; i < g.doc.num_capas; i++) {
            if (zona_contiene(capa_fila_zona(i), x, y)) {
                g.hover_capa = g.doc.num_capas - 1 - i;
                break;
            }
        }
        if (anterior != g.hover_herramienta) {
            app_actualizar_titulo();
            app_invalidar();
        }
        /* Hover de la ventana de opciones: pista resaltada, control del cubo
         * o boton de plegar. */
        {
            int anterior_opcion = g.hover_opcion;
            int anterior_cubo = g.hover_cubo;
            int invalidar = 0;
            if (g.panel_opciones && zona_contiene(opciones_zona(), x, y)) {
                if (g.herramienta == H_CUBO) {
                    g.hover_cubo = cubo_control_en(x, y);
                    g.hover_opcion = -1;
                } else {
                    g.hover_opcion = opciones_fila_en(x, y);
                    g.hover_cubo = -1;
                }
            } else {
                g.hover_opcion = -1;
                g.hover_cubo = -1;
                if (!g.panel_opciones && zona_contiene(opciones_minimizada_zona(), x, y))
                    invalidar = 1;
            }
            if (anterior_opcion != g.hover_opcion || anterior_cubo != g.hover_cubo ||
                invalidar)
                app_invalidar();
        }
        /* Tirador del lienzo bajo el cursor (resalte naranja). En modo tiles
         * no hay tiradores: el tamano es comun a todos los tiles. */
        {
            int anterior_lienzo = g.lienzo_hover;
            if (g.vista == VISTA_PINTAR && !g.modo_tiles &&
                zona_contiene(zona_trabajo(), x, y))
                g.lienzo_hover = lienzo_tirador_en(x, y);
            else
                g.lienzo_hover = -1;
            if (anterior_lienzo != g.lienzo_hover) app_invalidar();
        }
    }

    if (g.vista == VISTA_PINTAR && g.texto_activo) {
        if (g.texto.inicio_activo || g.texto.redimensionando) {
            texto_mover(x, y, botones);
            app_invalidar();
            return;
        }
        texto_mover(x, y, botones);
    }

    if (g.vista == VISTA_MAPA) {
        mapa_mover(x, y, botones);
        app_invalidar();
        return;
    }

    pintura_mover(x, y, botones);
}

void app_rueda(int x, int y, int delta, int ctrl) {
    g.raton_x = x;
    g.raton_y = y;
    if (g.vista == VISTA_MODULOS) {
        modulos_rueda(x, y, delta, ctrl);
        return;
    }
    if (g.vista == VISTA_MAPA && zona_contiene(zona_trabajo(), x, y)) {
        mapa_rueda(x, y, delta, ctrl);
        return;
    }
    /* Ctrl + rueda cambia el zoom en cualquier punto de la ventana. */
    if (ctrl) {
        zoom_aplicar(g.zoom_idx + (delta > 0 ? 1 : -1));
        return;
    }
    /* Sin Ctrl, la rueda solo desplaza el selector de tiles del panel derecho;
     * sobre el lienzo no hace nada (no hay desplazamiento manual). */
    if (x >= g.ancho - sidebar_derecho_ancho()) {
        if (delta > 0 && g.tiles_scroll > 0) g.tiles_scroll--;
        else if (delta < 0 && g.tiles_scroll + TILES_SELECTOR_VISIBLES < g.num_tiles) g.tiles_scroll++;
        app_invalidar();
    }
}

void app_tecla(int vk, int ctrl, int shift) {
    if (g.menu_abierto >= 0 && vk == VK_ESCAPE) {
        g.menu_abierto = -1;
        app_invalidar();
        return;
    }
    if (ctrl && vk == 'T') { pestanas_nueva_modo(); return; }
    if (ctrl && vk == 'W') { pestanas_cerrar(g.pestana_activa); return; }
    if (ctrl && vk == VK_TAB) { pestanas_avanzar(shift ? -1 : 1); return; }
    if (ctrl && vk >= '1' && vk <= '9') {
        int indice = vk - '1';
        if (indice < g.num_pestanas) pestanas_activar(indice);
        return;
    }
    if (ctrl && vk == 'N') { dialogo_nuevo(); return; }
    if (ctrl && vk == 'O') { dialogo_abrir_imagen(); return; }
    if (ctrl && vk == 'S') { dialogo_guardar_png(); return; }
    if (ctrl && vk == 'M') { app_cambiar_vista(g.vista == VISTA_MAPA ? VISTA_PINTAR : VISTA_MAPA); return; }
    if (vk == VK_F1) { g.info_visible = !g.info_visible; app_invalidar(); return; }
    if (g.vista == VISTA_MODULOS) return;
    if (g.vista == VISTA_PINTAR && texto_tecla(vk, ctrl, shift)) return;
    pintura_tecla(vk, ctrl, shift);
}

void app_caracter(int ch) {
    if (g.vista == VISTA_MODULOS) return;
    if (g.vista == VISTA_PINTAR && texto_caracter(ch)) return;
    pintura_caracter(ch);
}

void app_doble_clic(int x, int y) {
    if (y >= 0 && y < TAB_BARRA_ALTO) {
        if (pestana_en(x, y) < 0 &&
            !zona_contiene(pestana_nueva_zona(), x, y))
            pestanas_nueva_modo();
        return;
    }
    if (g.vista == VISTA_MAPA || g.vista == VISTA_MODULOS) return;
    pintura_doble_clic(x, y);
}

/* ------------------------------------------------------------------ */
/* Dialogos de mapa                                                    */
/* ------------------------------------------------------------------ */
void dialogo_guardar_mapa(void) {
    guardar_mapa_actual();
}

void dialogo_abrir_mapa(void) {
    char ruta[MAX_PATH];
    OPENFILENAMEA ofn;
    ruta[0] = '\0';
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = "Mapa Lua (*.mav)\0*.mav\0Todos los archivos\0*.*\0\0";
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&ofn)) return;
    if (mapa_cargar(ruta)) {
        strncpy(g.mapa_ruta, ruta, MAX_PATH - 1);
        g.mapa_ruta[MAX_PATH - 1] = 0;
        app_estado("Mapa cargado: %s", ruta);
    }
    else dialogo_error("No se pudo cargar el mapa.");
    app_invalidar();
}
