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

/* Los mapas se guardan siempre en la carpeta `maps` junto al ejecutable. */
static int pedir_ruta_mapa(char *ruta) {
    OPENFILENAMEA ofn;
    char carpeta[MAX_PATH];
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
    carpeta_maps(carpeta, sizeof(carpeta));
    crear_carpeta(carpeta);
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = "Mapa de tiles (*.mav)\0*.mav\0Todos los archivos\0*.*\0\0";
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = carpeta;
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
        /* Sin editor de mapas las filas son: Abrir, Borrar, PNG, BMP,
         * Abrir logica y Guardar logica. */
        switch (fila) {
            case 0: dialogo_abrir_imagen(); return;
            case 1: fila = 3; break;
            case 2: fila = 4; break;
            case 3: fila = 5; break;
            case 4: fila = 6; break;
            case 5: fila = 7; break;
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
        case 6: dialogo_abrir_logica(); break;
        case 7: dialogo_guardar_logica(); break;
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
                if (g.modo_tiles) tiles_modo_salir();
                /* Si el mapa ya tiene celdas en memoria se conserva tal cual.
                 * Solo se pide archivo o se recarga cuando esta vacio. */
                if (g.mapa.num == 0) {
                    if (g.mapa_ruta[0] == 0) {
                        char ruta[MAX_PATH];
                        if (!pedir_ruta_mapa(ruta)) break;
                        strncpy(g.mapa_ruta, ruta, MAX_PATH - 1);
                        g.mapa_ruta[MAX_PATH - 1] = 0;
                        mapa_reiniciar();
                        if (ruta_existe(ruta)) mapa_cargar(ruta);
                    } else if (ruta_existe(g.mapa_ruta)) {
                        mapa_reiniciar();
                        mapa_cargar(g.mapa_ruta);
                    }
                }
                /* Al entrar, si no habia tile elegido, usar el mas reciente. */
                if (g.mapa.tile_sel < 0 && g.num_tiles > 0) {
                    int indice = g.mapa_tile_sel;
                    if (indice < 0 || indice >= g.num_tiles) indice = g.num_tiles - 1;
                    g.mapa.tile_sel = indice;
                    g.mapa_tile_sel = indice;
                }
                app_cambiar_vista(VISTA_MAPA);
            }
            break;
        case 4:
            /* Gestor de logica: panel de elementos. */
            app_alternar_gestor_modulos();
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
        case 3: accion_seleccionar(opcion); break;
        case 4: accion_capa(opcion); break;
        case 5: accion_colores(opcion); break;
        case 6:
            /* COMPILE: las dos opciones aun no hacen nada. */
            app_estado(opcion == 0
                          ? "Compilar para BIOS: sin hacer todavia"
                          : "Compilar para Windows: sin hacer todavia");
            break;
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
    if (menu == 4 || menu == 5) {
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
/* Gestor de logica: elementos del panel izquierdo                     */
/* ------------------------------------------------------------------ */
/* Reinicia la logica unica: sin elementos y con la vista centrada. */
void logica_iniciar(void) {
    g.elementos_scroll = 0;
    g.num_modulos = 0;
    g.modulo_siguiente_id = 1;
    g.pan_x = 0;
    g.pan_y = 0;
    g.modulo_arrastrando = 0;
    g.modulo_drag_i = -1;
    g.modulo_snap_linea_x = -1;
    g.modulo_snap_linea_y = -1;
    g.hover_modulo_funcion = -1;
    g.modulo_funcion_pulsada = -1;
    g.modulo_funcion_movido = 0;
}

/* ------------------------------------------------------------------ */
/* Archivo de logica (.logic)                                          */
/* ------------------------------------------------------------------ */
/* Formato de texto, una logica por archivo:
 *   LOGIC1
 *   <pan_x> <pan_y>
 *   <num_elementos>
 *   <x> <y> <ancho> <alto> <id> <plegado> "<mapa>" <funcion>
 *     (una linea por elemento; funcion es el indice en MODULOS_FUNCIONES,
 *      -1 = item de mapa) */
int logica_guardar_archivo(const char *ruta) {
    FILE *f;
    int i;

    f = fopen(ruta, "wb");
    if (!f) return 0;
    fprintf(f, "LOGIC1\n");
    fprintf(f, "%d %d\n", g.pan_x, g.pan_y);
    fprintf(f, "%d\n", g.num_modulos);
    for (i = 0; i < g.num_modulos; i++) {
        const ModuloElemento *m = &g.modulos[i];
        fprintf(f, "%d %d %d %d %d %d \"%s\" %d\n",
                m->x, m->y, m->ancho, m->alto, m->id, m->plegado, m->mapa,
                m->funcion);
    }
    fclose(f);
    return 1;
}

/* Carga una logica guardada (LOGIC1). Sustituye la logica activa por la
 * del archivo y devuelve 1; 0 si no se puede leer o el formato no es
 * LOGIC1. El campo del mapa y el de la funcion se toman de entre
 * comillas y del numero final: las lineas antiguas sin funcion (fase 0)
 * se aceptan como items de mapa, igual que un nombre vacio. */
int logica_cargar_archivo(const char *ruta) {
    FILE *f;
    char linea[512];
    int pan_x, pan_y, num, i, max_id;

    f = fopen(ruta, "rb");
    if (!f) return 0;
    if (!fgets(linea, sizeof(linea), f) ||
        strncmp(linea, "LOGIC1", 6) != 0 ||
        !fgets(linea, sizeof(linea), f) ||
        sscanf(linea, "%d %d", &pan_x, &pan_y) != 2 ||
        !fgets(linea, sizeof(linea), f)) {
        fclose(f);
        return 0;
    }
    num = atoi(linea);
    if (num < 0 || num > MODULOS_MAX) {
        fclose(f);
        return 0;
    }

    logica_iniciar();
    g.pan_x = pan_x;
    g.pan_y = pan_y;

    max_id = 0;
    for (i = 0; i < num; i++) {
        ModuloElemento *e;
        char mapa[MAPS_NOMBRE_MAX];
        int x, y, ancho, alto, id, plegado, funcion;
        const char *q1, *q2;

        if (!fgets(linea, sizeof(linea), f)) break;
        if (sscanf(linea, "%d %d %d %d %d %d",
                   &x, &y, &ancho, &alto, &id, &plegado) != 6) continue;

        mapa[0] = 0;
        q1 = strchr(linea, '"');
        if (q1) {
            q2 = strchr(q1 + 1, '"');
            if (q2) {
                int n = (int)(q2 - q1 - 1);
                if (n >= MAPS_NOMBRE_MAX) n = MAPS_NOMBRE_MAX - 1;
                if (n > 0) memcpy(mapa, q1 + 1, (size_t)n);
                mapa[n] = 0;
            }
        }
        funcion = -1;
        if (q1 && strchr(q1 + 1, '"')) {
            const char *fin = strchr(q1 + 1, '"') + 1;
            while (*fin == ' ' || *fin == '\t') fin++;
            if (*fin == '-' || (*fin >= '0' && *fin <= '9'))
                funcion = atoi(fin);
        }
        if (funcion >= modulos_funciones_num()) funcion = -1;

        /* Tamanos degenerados vuelven al estandar del gestor. */
        if (ancho < 8 || ancho > 4096) ancho = MODULOS_ELEMENTO_ANCHO;
        if (alto < 8 || alto > 4096) alto = MODULOS_ELEMENTO_ALTO;
        if (id <= 0) id = max_id + 1;
        if (id > max_id) max_id = id;
        plegado = plegado ? 1 : 0;

        e = &g.modulos[g.num_modulos++];
        e->x = x;
        e->y = y;
        e->ancho = ancho;
        e->alto = alto;
        e->id = id;
        e->plegado = plegado;
        e->funcion = funcion;
        recortar_texto(e->mapa, mapa, sizeof(e->mapa));
    }
    fclose(f);
    g.modulo_siguiente_id = max_id + 1;
    return 1;
}

/* Dialogo para guardar la logica activa como .logic. */
void dialogo_guardar_logica(void) {
    char ruta[MAX_PATH];
    OPENFILENAMEA ofn;

    strcpy(ruta, "logica.logic");
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = "Logica (*.logic)\0*.logic\0Todos los archivos\0*.*\0\0";
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "logic";
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
                OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameA(&ofn)) return;
    if (logica_guardar_archivo(ruta))
        app_estado("Logica guardada: %s", ruta);
    else
        dialogo_error("No se pudo guardar la logica.");
    app_invalidar();
}

/* Dialogo para abrir una logica .logic: sustituye la logica activa y
 * entra al gestor para verla cargada. */
void dialogo_abrir_logica(void) {
    char ruta[MAX_PATH];
    OPENFILENAMEA ofn;

    strcpy(ruta, "logica.logic");
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = "Logica (*.logic)\0*.logic\0Todos los archivos\0*.*\0\0";
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "logic";
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |
                OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&ofn)) return;
    if (logica_cargar_archivo(ruta)) {
        if (g.vista != VISTA_MODULOS) app_alternar_gestor_modulos();
        app_estado("Logica cargada: %s", ruta);
    } else {
        dialogo_error("No se pudo abrir la logica.");
    }
    app_invalidar();
}

/* ------------------------------------------------------------------ */
/* Gestor de logica: boton arrastrable, lienzo y acople magnetico     */
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

/* Elimina un elemento del lienzo y compacta el array. */
static void modulos_eliminar(int idx) {
    char etiqueta[MAPS_NOMBRE_MAX];
    int i;
    if (idx < 0 || idx >= g.num_modulos) return;
    if (g.modulos[idx].mapa[0])
        recortar_texto(etiqueta, g.modulos[idx].mapa, sizeof(etiqueta));
    else
        formatear(etiqueta, sizeof(etiqueta), "Elemento %d", g.modulos[idx].id);
    for (i = idx; i < g.num_modulos - 1; i++) g.modulos[i] = g.modulos[i + 1];
    g.num_modulos--;
    /* La lista no debe quedar desplazada mas alla de su final. */
    {
        int orden[MODULOS_MAX], hijos[MODULOS_MAX];
        int filas = elementos_ordenar_lista(orden, hijos);
        int maximo = filas - elementos_visibles();
        if (maximo < 0) maximo = 0;
        if (g.elementos_scroll > maximo) g.elementos_scroll = maximo;
    }
    app_estado("Elemento eliminado: %s", etiqueta);
}

/* Dos elementos estan unidos cuando comparten un lado completo o parcial,
 * que es como los deja el acople magnetico. */
static int elementos_unidos(const ModuloElemento *a, const ModuloElemento *b) {
    int solapan_v = a->y < b->y + b->alto && b->y < a->y + a->alto;
    int solapan_h = a->x < b->x + b->ancho && b->x < a->x + a->ancho;

    if (solapan_v && (a->x + a->ancho == b->x || b->x + b->ancho == a->x))
        return 1;
    if (solapan_h && (a->y + a->alto == b->y || b->y + b->alto == a->y))
        return 1;
    return 0;
}

/* Ordena la lista de elementos por grupos unidos. Dentro de cada grupo manda
 * el id de creacion: el menor queda como raiz y el resto detras. En `hijos`:
 * 0 = elemento suelto, 1 = raiz con hijos, 2 = hijo indentado. Si la raiz esta
 * plegada solo se emite su fila. Devuelve el numero de filas visibles. */
int elementos_ordenar_lista(int *orden, int *hijos) {
    int comp[MODULOS_MAX], comp_min[MODULOS_MAX], comp_usado[MODULOS_MAX];
    int total = g.num_modulos, ncomp = 0, n = 0;
    int i, j;

    for (i = 0; i < total; i++) comp[i] = -1;
    for (i = 0; i < total; i++) {
        int cola[MODULOS_MAX], ini = 0, fin = 0;
        if (comp[i] >= 0) continue;
        comp[i] = ncomp;
        cola[fin++] = i;
        while (ini < fin) {
            int a = cola[ini++];
            for (j = 0; j < total; j++) {
                if (comp[j] >= 0) continue;
                if (!elementos_unidos(&g.modulos[a], &g.modulos[j])) continue;
                comp[j] = ncomp;
                cola[fin++] = j;
            }
        }
        ncomp++;
    }

    for (i = 0; i < ncomp; i++) {
        comp_min[i] = -1;
        comp_usado[i] = 0;
    }
    for (i = 0; i < total; i++) {
        int c = comp[i];
        if (comp_min[c] < 0 ||
            g.modulos[i].id < g.modulos[comp_min[c]].id)
            comp_min[c] = i;
    }

    while (n < total) {
        int mejor = -1, ultimo = -1, tam = 0;
        for (i = 0; i < ncomp; i++) {
            if (comp_usado[i]) continue;
            if (mejor < 0 ||
                g.modulos[comp_min[i]].id < g.modulos[comp_min[mejor]].id)
                mejor = i;
        }
        if (mejor < 0) break;
        comp_usado[mejor] = 1;
        for (i = 0; i < total; i++)
            if (comp[i] == mejor) tam++;
        for (;;) {
            int elegido = -1;
            for (j = 0; j < total; j++) {
                if (comp[j] != mejor) continue;
                if (ultimo >= 0 && g.modulos[j].id <= g.modulos[ultimo].id)
                    continue;
                if (elegido < 0 || g.modulos[j].id < g.modulos[elegido].id)
                    elegido = j;
            }
            if (elegido < 0) break;
            orden[n] = elegido;
            if (hijos) hijos[n] = (ultimo < 0) ? (tam > 1 ? 1 : 0) : 2;
            n++;
            ultimo = elegido;
            /* Grupo plegado: solo se muestra su raiz. */
            if (ultimo == comp_min[mejor] && tam > 1 &&
                g.modulos[ultimo].plegado)
                break;
        }
    }
    return n;
}

/* Raiz del grupo al que pertenece un elemento (el miembro de menor id). */
static int elementos_raiz(int idx) {
    int visto[MODULOS_MAX] = {0};
    int cola[MODULOS_MAX], ini = 0, fin = 0, i, raiz = idx;

    if (idx < 0 || idx >= g.num_modulos) return -1;
    visto[idx] = 1;
    cola[fin++] = idx;
    while (ini < fin) {
        int a = cola[ini++];
        if (g.modulos[a].id < g.modulos[raiz].id) raiz = a;
        for (i = 0; i < g.num_modulos; i++) {
            if (visto[i] || !elementos_unidos(&g.modulos[a], &g.modulos[i]))
                continue;
            visto[i] = 1;
            cola[fin++] = i;
        }
    }
    return raiz;
}

/* Inserta un elemento centrado en un punto de pantalla, con acople. */
/* Cuerpo comun de insercion: coloca un elemento nuevo centrado en (x, y)
 * del cliente, con el acople del iman. `funcion` es el indice en
 * MODULOS_FUNCIONES o -1 para un item de mapa. */
static void modulos_insertar_en(int x, int y, int funcion) {
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
    e->id = g.modulo_siguiente_id++;
    e->plegado = 0;
    e->funcion = funcion;
    if (funcion >= 0)
        e->mapa[0] = 0;
    else
        recortar_texto(e->mapa, g.item_mapa, sizeof(e->mapa));
    /* Si el elemento cae en un grupo plegado, se despliega para mostrarlo. */
    {
        int raiz = elementos_raiz(g.num_modulos - 1);
        if (raiz >= 0) g.modulos[raiz].plegado = 0;
    }
    /* La lista muestra el elemento recien anadido, aunque quede dentro de
     * un grupo y no al final. */
    if (g.num_modulos > elementos_visibles()) {
        int orden[MODULOS_MAX], hijos[MODULOS_MAX];
        int fila, total = elementos_ordenar_lista(orden, hijos);
        for (fila = 0; fila < total; fila++)
            if (orden[fila] == g.num_modulos - 1) break;
        if (fila < total && fila >= elementos_visibles())
            g.elementos_scroll = fila - elementos_visibles() + 1;
    }
}

void modulos_insertar(int x, int y) {
    modulos_insertar_en(x, y, -1);
}

/* Inserta un elemento de funcion logica (p. ej. egine, el motor) en el
 * punto del lienzo donde se ha soltado. */
void modulos_insertar_funcion(int x, int y, int funcion) {
    modulos_insertar_en(x, y, funcion);
}

/* ------------------------------------------------------------------ */
/* Selector de items: lista de mapas .mav de la carpeta `maps`         */
/* ------------------------------------------------------------------ */
void selector_mapa_abrir(void) {
    g.selector_mapa_num = mapa_listar_mapas(g.selector_mapas, MAPS_LISTA_MAX);
    g.selector_mapa_scroll = 0;
    g.selector_mapa_hover = -1;
    g.selector_mapa_abierto = 1;
    app_invalidar();
}

void selector_mapa_cerrar(void) {
    g.selector_mapa_abierto = 0;
    g.selector_mapa_hover = -1;
    app_invalidar();
}

void selector_mapa_elegir(int indice) {
    if (indice < 0 || indice >= g.selector_mapa_num) return;
    recortar_texto(g.item_mapa, g.selector_mapas[indice], sizeof(g.item_mapa));
    g.selector_mapa_abierto = 0;
    g.selector_mapa_hover = -1;
    app_estado("Item: %s", g.item_mapa);
    app_invalidar();
}

/* Fila del selector bajo el cursor; devuelve el indice real de la lista. */
int selector_mapa_en(int x, int y) {
    int i;
    if (!g.selector_mapa_abierto) return -1;
    if (!zona_contiene(selector_mapa_zona(), x, y)) return -1;
    for (i = 0; i < SELECTOR_MAPA_FILAS; i++) {
        int idx = g.selector_mapa_scroll + i;
        if (idx >= g.selector_mapa_num) break;
        if (zona_contiene(selector_mapa_fila_zona(i), x, y)) return idx;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Funciones logicas del panel derecho                                  */
/* ------------------------------------------------------------------ */
/* Rectangulos de la seccion FUNCIONES del panel derecho. Por ahora solo
 * hay una ("egine") y todavia no hace nada. */
static const char *MODULOS_FUNCIONES[] = { "egine" };

int modulos_funciones_num(void) {
    return (int)(sizeof(MODULOS_FUNCIONES) / sizeof(MODULOS_FUNCIONES[0]));
}

const char *modulos_funcion_nombre(int indice) {
    if (indice < 0 || indice >= modulos_funciones_num()) return "";
    return MODULOS_FUNCIONES[indice];
}

/* Rectangulo de funcion bajo el cursor; -1 si no hay ninguno. */
int modulos_funcion_en(int x, int y) {
    int i;
    for (i = 0; i < modulos_funciones_num(); i++)
        if (zona_contiene(modulos_funcion_zona(i), x, y)) return i;
    return -1;
}

static void modulos_pulsar(int x, int y, int boton) {
    Zona lienzo = modulos_lienzo_zona();

    /* El selector de items esta por encima del resto del gestor. */
    if (g.selector_mapa_abierto) {
        if (boton == 0) {
            int idx = selector_mapa_en(x, y);
            if (idx >= 0) selector_mapa_elegir(idx);
            else if (!zona_contiene(selector_mapa_zona(), x, y))
                selector_mapa_cerrar();
        }
        return;
    }

    /* Boton del panel derecho: un clic elige item y un arrastre lo inserta. */
    if (boton == 0 && zona_contiene(modulos_boton_zona(), x, y)) {
        g.modulo_boton_pulsado = 1;
        g.modulo_boton_x = x;
        g.modulo_boton_y = y;
        g.modulo_boton_movido = 0;
        g.hover_modulo_boton = 1;
        app_invalidar();
        return;
    }

    /* Rectangulos de la seccion FUNCIONES: un clic sin arrastre avisa de
     * como usarlos y un arrastre inserta la funcion en el lienzo. */
    if (boton == 0) {
        int f = modulos_funcion_en(x, y);
        if (f >= 0) {
            g.modulo_funcion_pulsada = f;
            g.modulo_funcion_x = x;
            g.modulo_funcion_y = y;
            g.modulo_funcion_movido = 0;
            g.hover_modulo_funcion = f;
            app_invalidar();
            return;
        }
    }

    /* Panel izquierdo: lista de elementos (plegar grupos). */
    if (boton == 0) {
        Zona lista = elementos_lista_zona();
        if (zona_contiene(lista, x, y)) {
            int orden[MODULOS_MAX], hijos[MODULOS_MAX];
            int filas = elementos_ordenar_lista(orden, hijos);
            int paso = ELEMENTO_FILA_ALTO + ELEMENTO_FILA_HUECO;
            int fila = (y - lista.y) / paso;
            int visible = fila + g.elementos_scroll;
            if (fila >= 0 && fila < elementos_visibles() && visible < filas &&
                hijos[visible] == 1 &&
                x < elemento_fila_zona(fila).x + 20) {
                int idx = orden[visible];
                int maximo;
                g.modulos[idx].plegado = !g.modulos[idx].plegado;
                filas = elementos_ordenar_lista(orden, hijos);
                maximo = filas - elementos_visibles();
                if (maximo < 0) maximo = 0;
                if (g.elementos_scroll > maximo) g.elementos_scroll = maximo;
                app_invalidar();
            }
            return;
        }
    }

    if (zona_contiene(lienzo, x, y)) {
        /* Boton derecho: eliminar el rectangulo bajo el cursor. Se ignora
         * durante un arrastre para no descuadrar el elemento movido. */
        if (boton == 1) {
            int idx = (g.modulo_arrastrando == 0) ? modulos_elemento_en(x, y)
                                                  : -1;
            if (idx >= 0) {
                modulos_eliminar(idx);
                app_invalidar();
            }
            return;
        }
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
    /* Soltar un rectangulo de la seccion FUNCIONES: si hubo arrastre se
     * inserta la funcion como elemento del lienzo (egine = el motor); si
     * fue un clic, recuerda como se usa. */
    if (g.modulo_funcion_pulsada >= 0) {
        int f = g.modulo_funcion_pulsada;
        g.modulo_funcion_pulsada = -1;
        g.modulo_funcion_movido = 0;
        if (g.modulo_arrastrando == 4) {
            g.modulo_arrastrando = 0;
            g.modulo_snap_linea_x = -1;
            g.modulo_snap_linea_y = -1;
            modulos_insertar_funcion(x, y, f);
        } else if (modulos_funcion_en(x, y) == f) {
            app_estado("%s: arrastra al lienzo para usarlo",
                       modulos_funcion_nombre(f));
        }
        app_invalidar();
        return;
    }

    if (g.modulo_boton_pulsado) {
        int era_clic = !g.modulo_boton_movido;
        g.modulo_boton_pulsado = 0;
        g.modulo_boton_movido = 0;
        if (era_clic) {
            /* Sin movimiento: el clic abre el selector de items. */
            if (zona_contiene(modulos_boton_zona(), x, y))
                selector_mapa_abrir();
            g.modulo_arrastrando = 0;
            g.modulo_snap_linea_x = -1;
            g.modulo_snap_linea_y = -1;
            app_invalidar();
            return;
        }
    }

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

    /* Selector de items abierto: solo se actualiza la fila resaltada. */
    if (g.selector_mapa_abierto) {
        int h = selector_mapa_en(x, y);
        if (h != g.selector_mapa_hover) {
            g.selector_mapa_hover = h;
            app_invalidar();
        }
        return;
    }

    /* Umbral de clic frente a arrastre al tirar del boton del panel derecho. */
    if (g.modulo_boton_pulsado && !g.modulo_boton_movido) {
        int dx = x - g.modulo_boton_x;
        int dy = y - g.modulo_boton_y;
        if (dx * dx + dy * dy >= 16) {
            g.modulo_boton_movido = 1;
            g.modulo_arrastrando = 1;
            g.modulo_snap_linea_x = -1;
            g.modulo_snap_linea_y = -1;
        }
    }

    /* El mismo umbral al tirar de un rectangulo de la seccion FUNCIONES. */
    if (g.modulo_funcion_pulsada >= 0 && !g.modulo_funcion_movido) {
        int dx = x - g.modulo_funcion_x;
        int dy = y - g.modulo_funcion_y;
        if (dx * dx + dy * dy >= 16) {
            g.modulo_funcion_movido = 1;
            g.modulo_arrastrando = 4;
            g.modulo_snap_linea_x = -1;
            g.modulo_snap_linea_y = -1;
        }
    }

    if (g.modulo_arrastrando == 2) {
        g.pan_x = x - g.modulo_drag_x;
        g.pan_y = y - g.modulo_drag_y;
        app_invalidar();
        return;
    }
    if (g.modulo_arrastrando == 1 || g.modulo_arrastrando == 4) {
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

    /* Hover de los rectangulos de la seccion FUNCIONES. */
    hover = modulos_funcion_en(x, y);
    if (hover != g.hover_modulo_funcion) {
        g.hover_modulo_funcion = hover;
        app_invalidar();
    }
}

static void modulos_rueda(int x, int y, int delta, int ctrl) {
    /* Rueda sobre el selector de items: desplaza la lista de mapas. */
    if (g.selector_mapa_abierto) {
        if (zona_contiene(selector_mapa_zona(), x, y)) {
            int maximo = g.selector_mapa_num - SELECTOR_MAPA_FILAS;
            int scroll = g.selector_mapa_scroll + (delta > 0 ? -1 : 1);
            if (maximo < 0) maximo = 0;
            if (scroll < 0) scroll = 0;
            if (scroll > maximo) scroll = maximo;
            if (scroll != g.selector_mapa_scroll) {
                g.selector_mapa_scroll = scroll;
                g.selector_mapa_hover = -1;
                app_invalidar();
            }
        }
        return;
    }
    /* Rueda sobre la lista de elementos: desplaza la lista. */
    if (zona_contiene(elementos_lista_zona(), x, y)) {
        int orden[MODULOS_MAX], hijos[MODULOS_MAX];
        int filas = elementos_ordenar_lista(orden, hijos);
        int maximo = filas - elementos_visibles();
        int scroll = g.elementos_scroll + (delta > 0 ? -1 : 1);
        if (maximo < 0) maximo = 0;
        if (scroll < 0) scroll = 0;
        if (scroll > maximo) scroll = maximo;
        if (scroll != g.elementos_scroll) {
            g.elementos_scroll = scroll;
            app_invalidar();
        }
        return;
    }
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

    /* El gestor de logica tiene su propio boton, lienzo y lista. */
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
    /* Tambien se marca dentro cuando la prueba simula movimientos sin pasar
     * por WM_MOUSEMOVE; el hover y las capturas lo necesitan. */
    g.raton_dentro = 1;

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
    if (g.selector_mapa_abierto && vk == VK_ESCAPE) {
        selector_mapa_cerrar();
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
    char carpeta[MAX_PATH];
    OPENFILENAMEA ofn;
    ruta[0] = '\0';
    carpeta_maps(carpeta, sizeof(carpeta));
    crear_carpeta(carpeta);
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = "Mapa Lua (*.mav)\0*.mav\0Todos los archivos\0*.*\0\0";
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = carpeta;
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
