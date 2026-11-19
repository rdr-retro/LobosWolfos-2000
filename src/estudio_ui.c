/* ==========================================================================
 * Vista Estudio: paneles, escenario e inspector (fase 31).
 *
 * Todo se dibuja sobre el buffer de 32 bits con la fuente bitmap 5x7. Las
 * tres columnas son 200, el resto y 260 px; a 1200 px el centro mide 740.
 * ========================================================================== */
#include "app.h"
#include "logic3.h"

#define ESTUDIO_LISTA_Y       (AREA_SUP_ALTO + 74)
#define ESTUDIO_REJILLA       32
#define ESTUDIO_COLOR_REJILLA 0x001E2A38u
#define ESTUDIO_COLOR_TRAJE   0x003278E0u
#define ESTUDIO_COLOR_TEXTO   0x00404040u

static const char *ESTUDIO_PESTANA_TEXTO[ESTUDIO_PESTANAS] = {
    "Escenario", "Objetos", "Datos", "Sonidos", "Bloques", "Play"
};

static const char *ESTUDIO_BOTON_TEXTO[ESTUDIO_BOTONES] = {
    "NUEVA ESC.", "NUEVO OBJ.", "IMPORTAR TRAJE", "BORRAR"
};

/* ------------------------------------------------------------------ */
/* Zonas                                                               */
/* ------------------------------------------------------------------ */
Zona estudio_panel_izq_zona(void) {
    return zona_hacer(0, AREA_SUP_ALTO, ESTUDIO_IZQ_ANCHO,
                      g.alto - AREA_SUP_ALTO);
}

Zona estudio_lienzo_zona(void) {
    int ancho = g.ancho - ESTUDIO_IZQ_ANCHO - ESTUDIO_DER_ANCHO;
    if (ancho < MIN_LADO) ancho = MIN_LADO;
    return zona_hacer(ESTUDIO_IZQ_ANCHO, AREA_SUP_ALTO, ancho,
                      g.alto - AREA_SUP_ALTO);
}

Zona estudio_panel_der_zona(void) {
    int x = g.ancho - ESTUDIO_DER_ANCHO;
    return zona_hacer(x, AREA_SUP_ALTO, ESTUDIO_DER_ANCHO,
                      g.alto - AREA_SUP_ALTO);
}

Zona estudio_barra_zona(void) {
    Zona z = estudio_lienzo_zona();
    return zona_hacer(z.x + 6, z.y + 4, z.ancho - 12, ESTUDIO_BARRA_ALTO);
}

Zona estudio_barra_boton_zona(int indice) {
    Zona barra = estudio_barra_zona();
    int paso, resto, x, ancho;
    if (indice < 0 || indice >= ESTUDIO_PESTANAS) return zona_vacia();
    paso = 4;
    resto = barra.ancho - paso * (ESTUDIO_PESTANAS - 1);
    ancho = resto / ESTUDIO_PESTANAS;
    x = barra.x + indice * (ancho + paso);
    return zona_hacer(x, barra.y, ancho, barra.alto);
}

Zona estudio_boton_zona(int indice) {
    int columna, fila;
    if (indice < 0 || indice >= ESTUDIO_BOTONES) return zona_vacia();
    columna = indice % 2;
    fila = indice / 2;
    return zona_hacer(6 + columna * 96, AREA_SUP_ALTO + 22 +
                      fila * (ESTUDIO_BOTON_ALTO + 4),
                      92, ESTUDIO_BOTON_ALTO);
}

Zona estudio_fila_zona(int fila) {
    if (fila < 0) return zona_vacia();
    return zona_hacer(6, ESTUDIO_LISTA_Y + fila * ESTUDIO_FILA_ALTO,
                      ESTUDIO_IZQ_ANCHO - 12, ESTUDIO_FILA_ALTO - 2);
}

/* ------------------------------------------------------------------ */
/* Ayudas de dibujo                                                    */
/* ------------------------------------------------------------------ */
static void texto_centro(Zona z, const char *texto, u32 color) {
    int w = fuente_texto_ancho(texto);
    int x = z.x + (z.ancho - w) / 2;
    int y = z.y + (z.alto - 7) / 2;
    if (x < z.x) x = z.x;
    fuente_dibujar(x, y, texto, color);
}

static void boton_estudio(Zona z, const char *texto, int activo, int hover) {
    u32 fondo = activo ? BOTON_SELECCION : (hover ? 0x00B4B4B4u : BARRA);
    u32 color = activo ? 0x00FFFFFFu : TEXTO_UI;
    dibujar_rect_z(z, fondo);
    dibujar_marco_z(z, activo ? BOTON_SELECCION : BARRA_BORDE);
    texto_centro(z, texto, color);
}

static void recortar(char *dst, int tam, const char *src) {
    int n = 0;
    if (!src) src = "";
    while (src[n] && n < tam - 1) {
        dst[n] = src[n];
        n++;
    }
    dst[n] = 0;
}

static void inspector_linea(int x, int y, const char *texto, u32 color) {
    fuente_dibujar(x, y, texto, color);
}

static Zona estudio_escena_zona(Zona lienzo) {
    int y = lienzo.y + ESTUDIO_BARRA_ALTO + 32;
    int alto = g.alto - y - 30;
    if (alto < ESTUDIO_REJILLA) alto = ESTUDIO_REJILLA;
    return zona_hacer(lienzo.x + 20, y, lienzo.ancho - 40, alto);
}

static Zona objeto_zona_en_escena(const Objeto3 *o, Zona escena) {
    int x = escena.x + 20 + (o->x % 320);
    int y = escena.y + 24 + (o->y % 240);
    int w = o->w * 2, h = o->h * 2;
    if (x + w > zona_derecha(escena)) w = zona_derecha(escena) - x;
    if (y + h > zona_abajo(escena)) h = zona_abajo(escena) - y;
    if (w < 4) w = 4;
    if (h < 4) h = 4;
    return zona_hacer(x, y, w, h);
}

/* ------------------------------------------------------------------ */
/* Dibujo                                                              */
/* ------------------------------------------------------------------ */
static void dibujar_toolbar(Zona lienzo) {
    Zona barra = estudio_barra_zona();
    int i;
    dibujar_rect_z(barra, MENU_FONDO);
    dibujar_marco_z(barra, MENU_BORDE);
    fuente_dibujar(barra.x + 6, barra.y + 8, "VISTA ESTUDIO",
                   ESTUDIO_COLOR_TEXTO);
    for (i = 0; i < ESTUDIO_PESTANAS; i++) {
        Zona z = estudio_barra_boton_zona(i);
        int activo = g.estudio_pestana == i;
        int hover = g.estudio_hover == 100 + i;
        u32 fondo = activo ? BOTON_SELECCION : (hover ? 0x00B4B4B4u : BARRA);
        dibujar_rect(z.x + 1, z.y + 18, zona_derecha(z) - 1,
                     zona_abajo(z) - 2, fondo);
        texto_centro(zona_hacer(z.x, z.y + 18, z.ancho, z.alto - 18),
                     ESTUDIO_PESTANA_TEXTO[i],
                     activo ? 0x00FFFFFFu : TEXTO_UI);
    }
    (void)lienzo;
}

static void dibujar_lista_estudio(const Proyecto3 *p) {
    char etiqueta[32];
    char tam[24];
    int visibles = estudio_filas_visibles();
    int total = estudio_filas_totales();
    int i;
    formatear(tam, sizeof(tam), "ESCENAS %d OBJETOS %d",
              p ? p->num_escenas : 0, p ? p->num_objetos : 0);
    fuente_dibujar(6, AREA_SUP_ALTO + 8, "PROYECTO", TEXTO_UI);
    fuente_dibujar(6, AREA_SUP_ALTO + 60, tam, 0x00606060u);
    for (i = 0; i < ESTUDIO_BOTONES; i++) {
        Zona z = estudio_boton_zona(i);
        int pulsado = g.estudio_boton_pulsado == i;
        boton_estudio(z, ESTUDIO_BOTON_TEXTO[i], pulsado,
                      g.estudio_hover == i);
    }
    for (i = 0; i < visibles; i++) {
        int fila = g.estudio_scroll + i;
        int tipo = -1, indice = -1;
        Zona z;
        int seleccionado, bloqueado;
        u32 mini;
        if (fila >= total) break;
        estudio_fila_info(fila, &tipo, &indice);
        z = estudio_fila_zona(i);
        seleccionado = (tipo == 0 && p->escenas[indice].id == g.estudio_escena &&
                        g.estudio_objeto == 0) ||
                       (tipo == 1 && p->objetos[indice].id == g.estudio_objeto);
        dibujar_rect_z(z, seleccionado ? BOTON_SELECCION : BARRA);
        dibujar_marco_z(z, seleccionado ? BOTON_SELECCION : BARRA_BORDE);
        mini = tipo == 0 ? 0x0043A047u :
               ((indice % 2) ? ESTUDIO_COLOR_TRAJE : 0x00E53935u);
        dibujar_rect(z.x + 3, z.y + 3, z.x + 9, z.y + 9, mini);
        if (tipo == 0)
            formatear(etiqueta, sizeof(etiqueta), "E %s",
                      p->escenas[indice].nombre);
        else
            formatear(etiqueta, sizeof(etiqueta), "O %s",
                      p->objetos[indice].nombre);
        fuente_dibujar(z.x + 13, z.y + 2, etiqueta,
                       seleccionado ? 0x00FFFFFFu : TEXTO_UI);
        bloqueado = (tipo == 1 &&
                     estudio_esta_bloqueado(p->objetos[indice].id));
        fuente_dibujar(zona_derecha(z) - 22, z.y + 2,
                       bloqueado ? "B" : "", 0x00606060u);
        if (tipo == 1)
            fuente_dibujar(zona_derecha(z) - 14, z.y + 2,
                           p->objetos[indice].visible ? "V" : "-",
                           0x00606060u);
    }
}

static void dibujar_escenario(const Proyecto3 *p, Zona lienzo) {
    Zona escena = estudio_escena_zona(lienzo);
    const Escena3 *actual = NULL;
    char titulo[L3_NOMBRE_MAX + 16];
    int i, x, y;
    if (p) {
        for (i = 0; i < p->num_escenas; i++)
            if (p->escenas[i].id == g.estudio_escena) {
                actual = &p->escenas[i];
                break;
            }
    }

    fuente_dibujar(lienzo.x + 12, lienzo.y + ESTUDIO_BARRA_ALTO + 10,
                   "ESTUDIO DE JUEGO", TEXTO_UI);
    dibujar_rect_z(escena, FONDO);
    dibujar_marco_z(escena, SEPARADOR);
    for (x = escena.x + ESTUDIO_REJILLA; x < zona_derecha(escena);
         x += ESTUDIO_REJILLA)
        dibujar_rect(x, escena.y, x + 1, zona_abajo(escena),
                     ESTUDIO_COLOR_REJILLA);
    for (y = escena.y + ESTUDIO_REJILLA; y < zona_abajo(escena);
         y += ESTUDIO_REJILLA)
        dibujar_rect(escena.x, y, zona_derecha(escena), y + 1,
                     ESTUDIO_COLOR_REJILLA);

    if (p && actual) {
        formatear(titulo, sizeof(titulo), "ESCENA: %s", actual->nombre);
        fuente_dibujar(escena.x + 8, escena.y + 6, titulo, 0x00C8C8C8u);
        for (i = 0; i < p->num_objetos; i++) {
            const Objeto3 *o = &p->objetos[i];
            Zona oz;
            if (o->escena != actual->id || !o->visible) continue;
            oz = objeto_zona_en_escena(o, escena);
            dibujar_rect_z(oz, i % 2 ? ESTUDIO_COLOR_TRAJE : 0x00E53935u);
            dibujar_marco_z(oz, 0x00FFFFFFu);
            if (o->id == g.estudio_objeto)
                fuente_dibujar(oz.x + 2, oz.y + 2, "*", 0x00FFFFFFu);
        }
    } else {
        fuente_dibujar(escena.x + 8, escena.y + 6,
                       "SIN ESCENA - PULSA NUEVA ESC.", 0x00C8C8C8u);
    }
    {
        char estado[64];
        int escenas = 0, objetos = 0;
        if (p) {
            escenas = p->num_escenas;
            objetos = p->num_objetos;
        }
        formatear(estado, sizeof(estado), "%d ESCENAS  %d OBJETOS",
                  escenas, objetos);
        fuente_dibujar(escena.x + 4, zona_abajo(escena) + 8, estado,
                       0x00606060u);
    }
}

static void dibujar_inspector(const Proyecto3 *p, Zona panel) {
    int x = panel.x + 10;
    int y = panel.y + 10;
    char linea[48];
    char tam[32];
    const Objeto3 *obj = NULL;
    const Escena3 *esc = NULL;
    int i;

    formatear(linea, sizeof(linea), "INSPECTOR / %s",
              ESTUDIO_PESTANA_TEXTO[g.estudio_pestana]);
    inspector_linea(x, y, linea, TEXTO_UI);
    y += 18;
    dibujar_rect(x, y, zona_derecha(panel) - 10, y + 1, SEPARADOR);
    y += 8;

    if (p && g.estudio_objeto > 0) {
        for (i = 0; i < p->num_objetos; i++)
            if (p->objetos[i].id == g.estudio_objeto) {
                obj = &p->objetos[i];
                break;
            }
    }
    if (p && g.estudio_escena > 0 && !obj) {
        for (i = 0; i < p->num_escenas; i++)
            if (p->escenas[i].id == g.estudio_escena) {
                esc = &p->escenas[i];
                break;
            }
    }

    if (obj) {
        inspector_linea(x, y, "PROPIEDADES", 0x001E88E5u);
        y += 14;
        formatear(linea, sizeof(linea), "id %d  capa %d", obj->id, obj->capa);
        inspector_linea(x, y, linea, ESTUDIO_COLOR_TEXTO);
        y += 12;
        formatear(linea, sizeof(linea), "nombre %s", obj->nombre);
        recortar(tam, sizeof(tam), linea);
        inspector_linea(x, y, tam, ESTUDIO_COLOR_TEXTO);
        y += 12;
        formatear(linea, sizeof(linea), "escena %d", obj->escena);
        inspector_linea(x, y, linea, ESTUDIO_COLOR_TEXTO);
        y += 12;
        formatear(linea, sizeof(linea), "x,y %d,%d", obj->x, obj->y);
        inspector_linea(x, y, linea, ESTUDIO_COLOR_TEXTO);
        y += 12;
        formatear(linea, sizeof(linea), "w,h %d,%d", obj->w, obj->h);
        inspector_linea(x, y, linea, ESTUDIO_COLOR_TEXTO);
        y += 12;
        formatear(linea, sizeof(linea), "visible %s  %s",
                  obj->visible ? "si" : "no",
                  estudio_esta_bloqueado(obj->id) ? "bloqueado" : "libre");
        inspector_linea(x, y, linea, ESTUDIO_COLOR_TEXTO);
        y += 18;

        inspector_linea(x, y, "TRAJES", 0x001E88E5u);
        y += 14;
        formatear(linea, sizeof(linea), "total %d", obj->num_trajes);
        inspector_linea(x, y, linea, ESTUDIO_COLOR_TEXTO);
        y += 12;
        for (i = 0; i < obj->num_trajes && i < 3; i++) {
            formatear(linea, sizeof(linea), "  %d %dx%d", obj->trajes[i].indice,
                      obj->trajes[i].ancho, obj->trajes[i].alto);
            inspector_linea(x, y, linea, ESTUDIO_COLOR_TEXTO);
            y += 12;
        }
        y += 6;

        inspector_linea(x, y, "VARIABLES", 0x001E88E5u);
        y += 14;
        formatear(linea, sizeof(linea), "total %d", obj->num_variables);
        inspector_linea(x, y, linea, ESTUDIO_COLOR_TEXTO);
        y += 12;
        for (i = 0; i < obj->num_variables && i < 3; i++) {
            formatear(linea, sizeof(linea), "  %s", obj->variables[i].nombre);
            recortar(tam, sizeof(tam), linea);
            inspector_linea(x, y, tam, ESTUDIO_COLOR_TEXTO);
            y += 12;
        }
        y += 6;

        inspector_linea(x, y, "EVENTOS", 0x001E88E5u);
        y += 14;
        formatear(linea, sizeof(linea), "total %d", obj->num_eventos);
        inspector_linea(x, y, linea, ESTUDIO_COLOR_TEXTO);
        y += 12;
        for (i = 0; i < obj->num_eventos && i < 4; i++) {
            formatear(linea, sizeof(linea), "  %d %s", obj->eventos[i].id,
                      logic3_evento_texto(obj->eventos[i].tipo));
            inspector_linea(x, y, linea, ESTUDIO_COLOR_TEXTO);
            y += 12;
        }
    } else if (esc) {
        inspector_linea(x, y, "ESCENA", 0x001E88E5u);
        y += 14;
        formatear(linea, sizeof(linea), "id %d", esc->id);
        inspector_linea(x, y, linea, ESTUDIO_COLOR_TEXTO);
        y += 12;
        formatear(linea, sizeof(linea), "nombre %s", esc->nombre);
        recortar(tam, sizeof(tam), linea);
        inspector_linea(x, y, tam, ESTUDIO_COLOR_TEXTO);
        y += 12;
        formatear(linea, sizeof(linea), "mapa %s", esc->mapa);
        recortar(tam, sizeof(tam), linea);
        inspector_linea(x, y, tam, ESTUDIO_COLOR_TEXTO);
        y += 12;
        formatear(linea, sizeof(linea), "tamano %dx%d", esc->ancho, esc->alto);
        inspector_linea(x, y, linea, ESTUDIO_COLOR_TEXTO);
    } else {
        inspector_linea(x, y, "SIN SELECCION", 0x001E88E5u);
        y += 14;
        inspector_linea(x, y, "Crea una escena y objetos", ESTUDIO_COLOR_TEXTO);
        y += 12;
        inspector_linea(x, y, "con los botones del panel", ESTUDIO_COLOR_TEXTO);
    }

    y = zona_abajo(panel) - 26;
    inspector_linea(x, y, "ARCHIVO -> ABRIR LOGIC3", 0x00606060u);
    inspector_linea(x, y + 12, "GUARDAR LOGIC3 para conservar", 0x00606060u);
}

void ui_dibujar_estudio(void) {
    Proyecto3 *p = logic3_modelo_activo();
    Zona izq = estudio_panel_izq_zona();
    Zona centro = estudio_lienzo_zona();
    Zona der = estudio_panel_der_zona();

    dibujar_rect(0, 0, g.ancho, g.alto, FONDO);
    dibujar_rect_z(izq, SIDEBAR);
    dibujar_rect_z(centro, FONDO);
    dibujar_rect_z(der, SIDEBAR);
    dibujar_rect(zona_derecha(izq) - 1, AREA_SUP_ALTO, zona_derecha(izq),
                 g.alto, SEPARADOR);
    dibujar_rect(der.x, AREA_SUP_ALTO, der.x + 1, g.alto, SEPARADOR);

    dibujar_toolbar(centro);
    if (g.estudio_pestana == BLOQUES_TAB) {
        bloques_ui_dibujar();
        return;
    }
    dibujar_lista_estudio(p);
    dibujar_escenario(p, centro);
    dibujar_inspector(p, der);
}

/* ------------------------------------------------------------------ */
/* Interaccion                                                         */
/* ------------------------------------------------------------------ */
static int fila_desde_y(int y) {
    if (y < ESTUDIO_LISTA_Y) return -1;
    return (y - ESTUDIO_LISTA_Y) / ESTUDIO_FILA_ALTO;
}

void estudio_pulsar(int x, int y, int boton) {
    Zona centro, izq, barra;
    int i;
    if (boton != 0 && boton != 1) return;
    izq = estudio_panel_izq_zona();
    centro = estudio_lienzo_zona();
    barra = estudio_barra_zona();
    g.estudio_boton_pulsado = -1;

    if (zona_contiene(barra, x, y)) {
        for (i = 0; i < ESTUDIO_PESTANAS; i++) {
            if (!zona_contiene(estudio_barra_boton_zona(i), x, y)) continue;
            if (i == ESTUDIO_PESTANAS - 1) {
                juego_ventana_abrir();
            } else {
                g.estudio_pestana = i;
                if (i == BLOQUES_TAB) bloques_importar_modelo();
                estudio_guardar_estado();
                app_invalidar();
            }
            return;
        }
    }

    if (g.estudio_pestana == BLOQUES_TAB) {
        bloques_ui_pulsar(x, y, boton);
        return;
    }

    if (zona_contiene(izq, x, y)) {
        for (i = 0; i < ESTUDIO_BOTONES; i++) {
            if (!zona_contiene(estudio_boton_zona(i), x, y)) continue;
            g.estudio_boton_pulsado = i;
            if (boton == 0) {
                if (i == 0) estudio_escena_nueva();
                else if (i == 1) estudio_objeto_nuevo();
                else if (i == 2) estudio_traje_importar();
                else if (i == 3) estudio_borrar_seleccion();
            }
            app_invalidar();
            return;
        }
        {
            int fila = fila_desde_y(y);
            int tipo = -1, indice = -1;
            Proyecto3 *p = logic3_modelo_activo();
            if (fila >= 0 && fila < estudio_filas_totales() &&
                estudio_fila_info(g.estudio_scroll + fila, &tipo, &indice) && p) {
                if (boton == 1 && tipo == 1)
                    estudio_alternar_bloqueo(p->objetos[indice].id);
                else
                    estudio_fila_seleccionar(g.estudio_scroll + fila);
            }
        }
        return;
    }

    if (zona_contiene(centro, x, y)) {
        Proyecto3 *p = logic3_modelo_activo();
        Zona escena = estudio_escena_zona(centro);
        int i;
        if (p && zona_contiene(escena, x, y)) {
            for (i = 0; i < p->num_objetos; i++) {
                const Objeto3 *o = &p->objetos[i];
                if (o->escena != g.estudio_escena || !o->visible) continue;
                if (zona_contiene(objeto_zona_en_escena(o, escena), x, y)) {
                    estudio_seleccionar_objeto(o->id);
                    break;
                }
            }
        }
    }
}

void estudio_soltar(int x, int y, int boton) {
    if (g.estudio_pestana == BLOQUES_TAB) {
        bloques_ui_soltar(x, y, boton);
        return;
    }
    (void)x;
    (void)y;
    (void)boton;
    if (g.estudio_boton_pulsado >= 0) {
        g.estudio_boton_pulsado = -1;
        app_invalidar();
    }
}

void estudio_mover(int x, int y, int botones) {
    Zona izq = estudio_panel_izq_zona();
    Zona barra = estudio_barra_zona();
    int hover = -1, i;
    (void)botones;
    if (g.estudio_pestana == BLOQUES_TAB && !zona_contiene(barra, x, y)) {
        bloques_ui_mover(x, y, botones);
        return;
    }
    if (zona_contiene(barra, x, y)) {
        for (i = 0; i < ESTUDIO_PESTANAS; i++)
            if (zona_contiene(estudio_barra_boton_zona(i), x, y))
                hover = 100 + i;
    } else if (zona_contiene(izq, x, y)) {
        for (i = 0; i < ESTUDIO_BOTONES; i++)
            if (zona_contiene(estudio_boton_zona(i), x, y)) hover = i;
    }
    if (hover != g.estudio_hover) {
        g.estudio_hover = hover;
        app_invalidar();
    }
}

void estudio_rueda(int x, int y, int delta, int ctrl) {
    Zona izq = estudio_panel_izq_zona();
    int maximo = estudio_filas_totales() - estudio_filas_visibles();
    (void)ctrl;
    if (g.estudio_pestana == BLOQUES_TAB) {
        bloques_ui_rueda(x, y, delta);
        return;
    }
    if (!zona_contiene(izq, x, y)) return;
    if (maximo < 0) maximo = 0;
    if (delta > 0) g.estudio_scroll--;
    else if (delta < 0) g.estudio_scroll++;
    if (g.estudio_scroll < 0) g.estudio_scroll = 0;
    if (g.estudio_scroll > maximo) g.estudio_scroll = maximo;
    estudio_guardar_estado();
    app_invalidar();
}

static int fila_de_seleccion(void) {
    Proyecto3 *p = logic3_modelo_activo();
    int i;
    if (!p) return -1;
    if (g.estudio_objeto > 0) {
        for (i = 0; i < p->num_objetos; i++)
            if (p->objetos[i].id == g.estudio_objeto)
                return p->num_escenas + i;
    }
    for (i = 0; i < p->num_escenas; i++)
        if (p->escenas[i].id == g.estudio_escena) return i;
    return -1;
}

void estudio_tecla(int vk, int ctrl, int shift) {
    int fila, total;
    if (g.estudio_pestana == BLOQUES_TAB) {
        bloques_ui_tecla(vk, ctrl, shift);
        return;
    }
    (void)ctrl;
    (void)shift;
    if (vk == VK_DELETE) {
        estudio_borrar_seleccion();
        return;
    }
    if (vk != VK_UP && vk != VK_DOWN) return;
    fila = fila_de_seleccion();
    total = estudio_filas_totales();
    if (fila < 0) fila = vk == VK_DOWN ? 0 : total - 1;
    else fila += vk == VK_DOWN ? 1 : -1;
    if (fila < 0) fila = 0;
    if (fila >= total) fila = total - 1;
    if (fila >= 0) {
        estudio_scroll_ver(fila);
        estudio_fila_seleccionar(fila);
    }
}

void estudio_caracter(int ch) {
    if (g.estudio_pestana == BLOQUES_TAB) {
        bloques_ui_caracter(ch);
        return;
    }
    (void)ch;
}
