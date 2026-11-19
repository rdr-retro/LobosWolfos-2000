/* ==========================================================================
 * Lienzo de bloques: dibujo e interaccion (fase 32).
 *
 * La paleta ocupa la columna izquierda, el lienzo la central y el inspector
 * la derecha. Solo pinta cajas calculadas por bloques.c; aqui no se toca el
 * formato LOGIC3 ni el escritor de proyectos.
 * ========================================================================== */
#include "app.h"

#define BLOQ_REJILLA       24
#define BLOQ_COLOR_REJILLA 0x001E2A38u
#define BLOQ_COLOR_TEXTO   0x00404040u
#define BLOQ_COLOR_CLARO   0x00F0F0F0u
#define BLOQ_COLOR_BORDE   0x00202020u
#define BLOQ_FILAS_PALETTE 12

enum {
    FORMA_HAT = 0,
    FORMA_PILA,
    FORMA_C,
    FORMA_REPORTERO,
    FORMA_BOOLEANO,
    FORMA_CAP
};

typedef struct {
    int id;
    char slot[BLOQUES_SLOT_MAX];
} DestinoBloque;

static void copiar_texto_local(char *dst, int tam, const char *src) {
    int n = 0;
    if (!src) src = "";
    while (src[n] && n < tam - 1) {
        dst[n] = src[n];
        n++;
    }
    dst[n] = 0;
}

/* ------------------------------------------------------------------ */
/* Zonas                                                               */
/* ------------------------------------------------------------------ */
static Zona zona_lienzo(void) {
    Zona z = estudio_lienzo_zona();
    int y = z.y + ESTUDIO_BARRA_ALTO + 34;
    int alto = g.alto - y - 40;
    if (alto < 80) alto = 80;
    return zona_hacer(z.x + 10, y, z.ancho - 20, alto);
}

static Zona zona_categoria(int indice) {
    return zona_hacer(6, AREA_SUP_ALTO + 8 + indice * 22,
                      ESTUDIO_IZQ_ANCHO - 12, 20);
}

static Zona zona_def_fila(int fila) {
    int y = AREA_SUP_ALTO + 8 + BLOQUES_CATEGORIAS * 22 + 10;
    return zona_hacer(6, y + fila * 22, ESTUDIO_IZQ_ANCHO - 12, 20);
}

static int def_en_fila(int fila, int categoria) {
    int i, n = 0;
    for (i = 0; i < bloques_def_cantidad(); i++) {
        if (bloques_def_categoria(i) != categoria) continue;
        if (n == fila) return i;
        n++;
    }
    return -1;
}

static int hijo_en_slot(int padre, const char *slot) {
    int i, id;
    for (i = 0; i < bloques_cantidad(); i++) {
        id = bloques_nodo_id(i);
        if (bloques_nodo_padre(id) == padre &&
            strcmp(bloques_nodo_slot(id), slot) == 0) return id;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Dibujo                                                              */
/* ------------------------------------------------------------------ */
static void dibujar_bloque(const BloqueCaja *c) {
    int def = bloques_nodo_def(c->id);
    int forma = bloques_def_forma(def);
    int seleccionado = g.bloques_sel == c->id;
    u32 color = bloques_def_color(def);
    u32 tinta = forma == FORMA_REPORTERO ? 0x00202020u : 0x00FFFFFFu;
    const char *campo;
    char etiqueta[40];
    int i, id;

    dibujar_rect_z(c->z, color);
    dibujar_marco_z(c->z, seleccionado ? 0x00FFFFFFu : BLOQ_COLOR_BORDE);
    if (seleccionado)
        dibujar_rect(c->z.x + 1, c->z.y + 1, c->z.x + 3, zona_abajo(c->z) - 1,
                     0x00FFFFFFu);

    fuente_dibujar(c->z.x + 6, c->z.y + 7, bloques_nombre_def(def), tinta);
    campo = bloques_nodo_campo(c->id, 0);
    if (campo && campo[0]) {
        formatear(etiqueta, sizeof(etiqueta), "= %s", campo);
        fuente_dibujar(c->z.x + c->z.ancho - fuente_texto_ancho(etiqueta) - 6,
                       c->z.y + 7, etiqueta, tinta);
    }

    for (i = 0; i < c->num_slots; i++) {
        Zona s = c->slots[i];
        dibujar_rect_z(s, BLOQ_COLOR_CLARO);
        dibujar_marco_z(s, BLOQ_COLOR_BORDE);
        id = hijo_en_slot(c->id, c->nombres[i]);
        if (id > 0)
            fuente_dibujar(s.x + 3, s.y + 3, bloques_nombre_def(bloques_nodo_def(id)),
                           BLOQ_COLOR_TEXTO);
        else
            fuente_dibujar(s.x + 3, s.y + 3, c->nombres[i], 0x00606060u);
    }

    if (forma == FORMA_C) {
        fuente_dibujar(zona_derecha(c->z) - 16, zona_abajo(c->z) - 12,
                       g.bloques_sel == c->id ? "v" : "^", tinta);
    }
}

static void dibujar_paleta(void) {
    Zona izq = estudio_panel_izq_zona();
    int i, fila;
    dibujar_rect(zona_derecha(izq) - 1, AREA_SUP_ALTO, zona_derecha(izq),
                 g.alto, SEPARADOR);
    fuente_dibujar(6, AREA_SUP_ALTO + 60, "CATEGORIAS", TEXTO_UI);
    for (i = 0; i < BLOQUES_CATEGORIAS; i++) {
        Zona z = zona_categoria(i);
        int activo = g.bloques_categoria == i;
        dibujar_rect_z(z, activo ? BOTON_SELECCION : BARRA);
        dibujar_marco_z(z, activo ? BOTON_SELECCION : BARRA_BORDE);
        fuente_dibujar(z.x + 6, z.y + 6, bloques_categoria_nombre(i),
                       activo ? 0x00FFFFFFu : TEXTO_UI);
    }
    for (fila = 0; fila < BLOQ_FILAS_PALETTE; fila++) {
        Zona z = zona_def_fila(fila);
        int def = def_en_fila(fila, g.bloques_categoria);
        if (def < 0) break;
        dibujar_rect_z(z, g.bloques_paleta_sel == def ? BOTON_SELECCION : BARRA);
        dibujar_marco_z(z, BARRA_BORDE);
        dibujar_rect(z.x + 3, z.y + 5, z.x + 11, z.y + 14,
                     bloques_def_color(def));
        fuente_dibujar(z.x + 16, z.y + 6, bloques_nombre_def(def),
                       g.bloques_paleta_sel == def ? 0x00FFFFFFu : TEXTO_UI);
    }
    {
        char aviso[48];
        formatear(aviso, sizeof(aviso), "BLOQUES: %d", bloques_cantidad());
        fuente_dibujar(6, zona_abajo(izq) - 20, aviso, 0x00606060u);
    }
}

static void dibujar_lienzo(void) {
    Zona c = zona_lienzo();
    BloqueCaja cajas[BLOQUES_MAX];
    int num = bloques_layout(c.x, c.y, c.ancho, c.alto, cajas, BLOQUES_MAX);
    int i, x, y;
    char titulo[64];

    formatear(titulo, sizeof(titulo), "LIENZO DE BLOQUES / %s",
              bloques_categoria_nombre(g.bloques_categoria));
    fuente_dibujar(estudio_lienzo_zona().x + 12,
                   estudio_lienzo_zona().y + ESTUDIO_BARRA_ALTO + 10,
                   titulo, TEXTO_UI);
    for (x = c.x + BLOQ_REJILLA; x < zona_derecha(c); x += BLOQ_REJILLA)
        dibujar_rect(x, c.y, x + 1, zona_abajo(c), BLOQ_COLOR_REJILLA);
    for (y = c.y + BLOQ_REJILLA; y < zona_abajo(c); y += BLOQ_REJILLA)
        dibujar_rect(c.x, y, zona_derecha(c), y + 1, BLOQ_COLOR_REJILLA);
    for (i = 0; i < num; i++) dibujar_bloque(&cajas[i]);

    if (num == 0) {
        fuente_dibujar(c.x + 16, c.y + 16,
                       "ARRASTRA UN BLOQUE DE LA PALETA AQUI", 0x00606060u);
        fuente_dibujar(c.x + 16, c.y + 30,
                       "TAB cambia de panel; INTRODUCE anade", 0x00606060u);
    }
    {
        char estado[64];
        formatear(estado, sizeof(estado),
                  "NODOS %d  RANURAS %d  CUERPO %d  ZOOM %d",
                  bloques_cantidad(), bloques_num_ranuras(),
                  bloques_num_hijos(), g.bloques_zoom);
        fuente_dibujar(c.x + 4, zona_abajo(c) + 8, estado, 0x00606060u);
    }
}

static void dibujar_inspector_bloques(void) {
    Zona p = estudio_panel_der_zona();
    int x = p.x + 10, y = p.y + 10;
    int id = g.bloques_sel;
    char linea[64];

    fuente_dibujar(x, y, "INSPECTOR / BLOQUES", TEXTO_UI);
    y += 18;
    dibujar_rect(x, y, zona_derecha(p) - 10, y + 1, SEPARADOR);
    y += 10;
    if (id <= 0) {
        fuente_dibujar(x, y, "SIN BLOQUE SELECCIONADO", 0x001E88E5u);
        y += 14;
        fuente_dibujar(x, y, "Arrastra desde la paleta", BLOQ_COLOR_TEXTO);
        y += 12;
        fuente_dibujar(x, y, "o pulsa INTRODUCE", BLOQ_COLOR_TEXTO);
    } else {
        int def = bloques_nodo_def(id);
        formatear(linea, sizeof(linea), "id %d", id);
        fuente_dibujar(x, y, linea, 0x001E88E5u);
        y += 14;
        formatear(linea, sizeof(linea), "tipo %s", bloques_nombre_def(def));
        fuente_dibujar(x, y, linea, BLOQ_COLOR_TEXTO);
        y += 12;
        formatear(linea, sizeof(linea), "padre %d  slot %s",
                  bloques_nodo_padre(id), bloques_nodo_slot(id));
        fuente_dibujar(x, y, linea, BLOQ_COLOR_TEXTO);
        y += 12;
        formatear(linea, sizeof(linea), "campo %s",
                  bloques_nodo_campo(id, 0));
        fuente_dibujar(x, y, linea, BLOQ_COLOR_TEXTO);
        y += 18;
        fuente_dibujar(x, y, "ATAJOS", 0x001E88E5u);
        y += 14;
        fuente_dibujar(x, y, "TAB     paleta / lienzo", BLOQ_COLOR_TEXTO);
        y += 12;
        fuente_dibujar(x, y, "ARRIBA/ABAJO  seleccion", BLOQ_COLOR_TEXTO);
        y += 12;
        fuente_dibujar(x, y, "INTRODUCE  anadir bloque", BLOQ_COLOR_TEXTO);
        y += 12;
        fuente_dibujar(x, y, "SUPR  borrar  CTRL+Z", BLOQ_COLOR_TEXTO);
        y += 12;
        fuente_dibujar(x, y, "CTRL+Y rehacer", BLOQ_COLOR_TEXTO);
    }
    fuente_dibujar(x, zona_abajo(p) - 26, "ARCHIVO -> GUARDAR LOGIC3",
                   0x00606060u);
    fuente_dibujar(x, zona_abajo(p) - 14, "para conservar los bloques",
                   0x00606060u);
}

void bloques_ui_dibujar(void) {
    Zona der = estudio_panel_der_zona();
    dibujar_rect(der.x, AREA_SUP_ALTO, der.x + 1, g.alto, SEPARADOR);
    dibujar_paleta();
    dibujar_lienzo();
    dibujar_inspector_bloques();
}

/* ------------------------------------------------------------------ */
/* Apoyo a la interaccion                                              */
/* ------------------------------------------------------------------ */
static DestinoBloque destino_en(int x, int y) {
    Zona c = zona_lienzo();
    BloqueCaja cajas[BLOQUES_MAX];
    DestinoBloque r;
    int num = bloques_layout(c.x, c.y, c.ancho, c.alto, cajas, BLOQUES_MAX);
    int i, j, def;
    r.id = 0;
    r.slot[0] = 0;
    if (!zona_contiene(c, x, y)) return r;
    for (i = num - 1; i >= 0; i--) {
        for (j = 0; j < cajas[i].num_slots; j++)
            if (zona_contiene(cajas[i].slots[j], x, y)) {
                r.id = cajas[i].id;
                copiar_texto_local(r.slot, sizeof(r.slot), cajas[i].nombres[j]);
                return r;
            }
    }
    for (i = num - 1; i >= 0; i--) {
        if (!zona_contiene(cajas[i].z, x, y)) continue;
        def = bloques_nodo_def(cajas[i].id);
        r.id = cajas[i].id;
        if (bloques_def_slot(def, 0)[0])
            copiar_texto_local(r.slot, sizeof(r.slot), bloques_def_slot(def, 0));
        return r;
    }
    return r;
}

static int bloque_en(int x, int y) {
    Zona c = zona_lienzo();
    BloqueCaja cajas[BLOQUES_MAX];
    int num = bloques_layout(c.x, c.y, c.ancho, c.alto, cajas, BLOQUES_MAX);
    int i;
    if (!zona_contiene(c, x, y)) return 0;
    for (i = num - 1; i >= 0; i--)
        if (zona_contiene(cajas[i].z, x, y)) return cajas[i].id;
    return 0;
}

void bloques_ui_pulsar(int x, int y, int boton) {
    Zona barra = estudio_barra_zona();
    Zona c = zona_lienzo();
    int i, fila, id, def;
    if (boton != 0 && boton != 1) return;
    if (zona_contiene(barra, x, y)) return;
    for (i = 0; i < BLOQUES_CATEGORIAS; i++) {
        if (!zona_contiene(zona_categoria(i), x, y)) continue;
        g.bloques_categoria = i;
        g.bloques_paleta_sel = 0;
        g.bloques_foco = 0;
        app_invalidar();
        return;
    }
    for (fila = 0; fila < BLOQ_FILAS_PALETTE; fila++) {
        def = def_en_fila(fila, g.bloques_categoria);
        if (def < 0) break;
        if (!zona_contiene(zona_def_fila(fila), x, y)) continue;
        g.bloques_paleta_sel = def;
        g.bloques_foco = 0;
        if (boton == 0) {
            g.bloques_desde_paleta = def;
            g.bloques_arrastre = -1;
            g.bloques_drag_x = x;
            g.bloques_drag_y = y;
        } else {
            bloques_crear(def, c.x + 40, c.y + 40 + bloques_cantidad() * 30);
            g.bloques_desde_paleta = -1;
        }
        app_invalidar();
        return;
    }
    if (zona_contiene(c, x, y)) {
        id = bloque_en(x, y);
        if (boton == 1) {
            if (id > 0) bloques_duplicar(id);
            app_invalidar();
            return;
        }
        g.bloques_foco = 1;
        if (id > 0) {
            bloques_seleccionar(id);
            g.bloques_arrastre = id;
            g.bloques_desde_paleta = -1;
            g.bloques_drag_x = x;
            g.bloques_drag_y = y;
        } else {
            bloques_seleccionar(0);
            g.bloques_arrastre = -1;
        }
        app_invalidar();
    }
}

void bloques_ui_soltar(int x, int y, int boton) {
    Zona c = zona_lienzo();
    DestinoBloque dest;
    int id;
    (void)boton;
    if (g.bloques_desde_paleta >= 0) {
        id = g.bloques_desde_paleta;
        g.bloques_desde_paleta = -1;
        if (zona_contiene(c, x, y)) {
            int nuevo = bloques_crear(id, x, y);
            dest = destino_en(x, y);
            if (nuevo > 0 && dest.id > 0 && dest.id != nuevo && dest.slot[0])
                bloques_acoplar(nuevo, dest.id, dest.slot);
        }
        app_invalidar();
        return;
    }
    if (g.bloques_arrastre > 0) {
        id = g.bloques_arrastre;
        g.bloques_arrastre = -1;
        if (zona_contiene(c, x, y)) {
            dest = destino_en(x, y);
            if (dest.id > 0 && dest.id != id && dest.slot[0])
                bloques_acoplar(id, dest.id, dest.slot);
            else if (bloques_nodo_padre(id) == 0)
                bloques_mover_a(id, x, y);
        }
        app_invalidar();
    }
}

void bloques_ui_mover(int x, int y, int botones) {
    int hover;
    (void)botones;
    g.bloques_drag_x = x;
    g.bloques_drag_y = y;
    hover = bloque_en(x, y);
    if (hover != g.bloques_hover) {
        g.bloques_hover = hover;
        app_invalidar();
    }
}

void bloques_ui_rueda(int x, int y, int delta) {
    Zona c = zona_lienzo();
    if (!zona_contiene(c, x, y)) return;
    g.bloques_pan_y += delta > 0 ? -12 : 12;
    if (g.bloques_pan_y > 400) g.bloques_pan_y = 400;
    if (g.bloques_pan_y < -400) g.bloques_pan_y = -400;
    app_invalidar();
}

void bloques_ui_tecla(int vk, int ctrl, int shift) {
    static int portapapeles = 0;
    Zona c = zona_lienzo();
    int i, def, n = 0, actual = -1;
    (void)shift;
    if (ctrl) {
        if (vk == 'Z') bloques_deshacer();
        else if (vk == 'Y') bloques_rehacer();
        else if (vk == 'C') portapapeles = g.bloques_sel;
        else if (vk == 'V' && portapapeles > 0) bloques_duplicar(portapapeles);
        app_invalidar();
        return;
    }
    if (vk == VK_TAB) {
        g.bloques_foco = g.bloques_foco ? 0 : 1;
        app_invalidar();
        return;
    }
    if (vk == VK_LEFT || vk == VK_RIGHT) {
        g.bloques_categoria += vk == VK_RIGHT ? 1 : -1;
        if (g.bloques_categoria < 0) g.bloques_categoria = BLOQUES_CATEGORIAS - 1;
        if (g.bloques_categoria >= BLOQUES_CATEGORIAS) g.bloques_categoria = 0;
        g.bloques_paleta_sel = 0;
        g.bloques_foco = 0;
        app_invalidar();
        return;
    }
    if (vk == VK_UP || vk == VK_DOWN) {
        if (g.bloques_foco == 0) {
            for (i = 0; i < bloques_def_cantidad(); i++)
                if (bloques_def_categoria(i) == g.bloques_categoria) {
                    if (i == g.bloques_paleta_sel) actual = n;
                    n++;
                }
            if (n > 0) {
                actual += vk == VK_DOWN ? 1 : -1;
                if (actual < 0) actual = n - 1;
                if (actual >= n) actual = 0;
                g.bloques_paleta_sel = def_en_fila(actual, g.bloques_categoria);
            }
        } else if (bloques_cantidad() > 0) {
            for (i = 0; i < bloques_cantidad(); i++)
                if (bloques_nodo_id(i) == g.bloques_sel) actual = i;
            if (actual < 0) actual = 0;
            else actual += vk == VK_DOWN ? 1 : -1;
            if (actual < 0) actual = bloques_cantidad() - 1;
            if (actual >= bloques_cantidad()) actual = 0;
            bloques_seleccionar(bloques_nodo_id(actual));
        }
        app_invalidar();
        return;
    }
    if (vk == VK_RETURN) {
        if (g.bloques_foco == 0) {
            def = g.bloques_paleta_sel >= 0 ? g.bloques_paleta_sel :
                  def_en_fila(0, g.bloques_categoria);
            if (def >= 0)
                bloques_crear(def, c.x + 30,
                              c.y + 20 + bloques_cantidad() * 30);
        }
        app_invalidar();
        return;
    }
    if (vk == VK_DELETE) {
        bloques_borrar(g.bloques_sel);
        app_invalidar();
    }
}

void bloques_ui_caracter(int ch) {
    if (g.bloques_sel <= 0) return;
    bloques_campo_anadir(g.bloques_sel, 0, ch);
    app_invalidar();
}

void bloques_ui_arrastrar(int x0, int y0, int x1, int y1) {
    int paso;
    app_mover(x0, y0, 0);
    g.boton_izq = 1;
    app_pulsar(x0, y0, 0);
    for (paso = 1; paso <= 12; paso++) {
        int mx = x0 + (x1 - x0) * paso / 12;
        int my = y0 + (y1 - y0) * paso / 12;
        app_mover(mx, my, 1);
    }
    g.boton_izq = 0;
    app_soltar(x1, y1, 0);
}
