/* ==========================================================================
 * Editor de codigo integrado, con panel izquierdo vacio de 200 px.
 * ========================================================================== */
#include "app.h"

#define EDITOR_BARRA_ALTO 44
#define EDITOR_ESTADO_ALTO 26
#define EDITOR_LINEA_ALTO 18
#define EDITOR_PANEL_ANCHO 200
#define EDITOR_CABECERA_ANCHO 52
#define EDITOR_MARGEN 8
#define EDITOR_SCROLL_ANCHO 12
#define EDITOR_PESTANA 4

ZonasEditor editor_zonas(EditorCodigo *e) {
    ZonasEditor z;
    Zona barra = zona_hacer(0, MENU_BARRA_ALTO, g.ancho, EDITOR_BARRA_ALTO);
    int x = 8;
    int wy = MENU_BARRA_ALTO + EDITOR_BARRA_ALTO;
    int wa = g.alto - wy - EDITOR_ESTADO_ALTO;
    z.barra = barra;
    z.nuevo = zona_hacer(x, barra.y + 8, 72, 28); x += 78;
    z.abrir = zona_hacer(x, barra.y + 8, 72, 28); x += 78;
    z.guardar = zona_hacer(x, barra.y + 8, 78, 28); x += 84;
    z.cerrar = zona_hacer(g.ancho - 90, barra.y + 8, 82, 28);
    z.texto = zona_hacer(EDITOR_PANEL_ANCHO, wy, g.ancho - EDITOR_PANEL_ANCHO, wa < 1 ? 1 : wa);
    z.estado = zona_hacer(0, g.alto - EDITOR_ESTADO_ALTO, g.ancho, EDITOR_ESTADO_ALTO);
    z.scroll = zona_hacer(g.ancho - EDITOR_SCROLL_ANCHO, z.texto.y, EDITOR_SCROLL_ANCHO, z.texto.alto);
    (void)e;
    return z;
}

/* ------------------------------------------------------------------ */
/* Utilidades de texto                                                 */
/* ------------------------------------------------------------------ */
static int editor_linea_de(EditorCodigo *e, int indice) {
    int i, linea = 0;
    if (indice > e->largo) indice = e->largo;
    for (i = 0; i < indice; i++) if (e->texto[i] == '\n') linea++;
    return linea;
}

static int editor_columna_de(EditorCodigo *e, int indice) {
    int i;
    if (indice > e->largo) indice = e->largo;
    for (i = indice - 1; i >= 0; i--) {
        if (e->texto[i] == '\n') return indice - i - 1;
    }
    return indice;
}

static int editor_inicio_linea(EditorCodigo *e, int linea) {
    int i, actual = 0;
    if (linea <= 0) return 0;
    for (i = 0; i < e->largo; i++) {
        if (e->texto[i] == '\n') {
            actual++;
            if (actual == linea) return i + 1;
        }
    }
    return e->largo;
}

static int editor_total_lineas(EditorCodigo *e) {
    int i, n = 1;
    for (i = 0; i < e->largo; i++) if (e->texto[i] == '\n') n++;
    return n;
}

static void editor_reservar(EditorCodigo *e, int nuevo_largo) {
    int capacidad = e->largo + 1;
    char *nuevo;
    if (nuevo_largo + 1 <= capacidad) return;
    capacidad *= 2;
    while (capacidad < nuevo_largo + 1) capacidad *= 2;
    nuevo = (char *)realloc(e->texto, (size_t)capacidad);
    if (nuevo) e->texto = nuevo;
}

void editor_insertar(EditorCodigo *e, const char *texto) {
    int n = (int)strlen(texto);
    if (!e || n <= 0) return;
    if (e->largo + n >= MAX_TEXTO * 32) return;
    editor_reservar(e, e->largo + n);
    if (!e->texto) return;
    memmove(e->texto + e->cursor + n, e->texto + e->cursor, (size_t)(e->largo - e->cursor));
    memcpy(e->texto + e->cursor, texto, (size_t)n);
    e->cursor += n;
    e->largo += n;
    e->texto[e->largo] = '\0';
    e->modificado = 1;
}

static void editor_borrar_rango(EditorCodigo *e, int desde, int hasta) {
    if (desde < 0) desde = 0;
    if (hasta > e->largo) hasta = e->largo;
    if (hasta <= desde) return;
    memmove(e->texto + desde, e->texto + hasta, (size_t)(e->largo - hasta));
    e->largo -= hasta - desde;
    e->texto[e->largo] = '\0';
    e->cursor = desde;
    e->modificado = 1;
}

/* ------------------------------------------------------------------ */
/* Inicio / archivos                                                   */
/* ------------------------------------------------------------------ */
void editor_iniciar(EditorCodigo *e) {
    memset(e, 0, sizeof(*e));
    e->texto = (char *)calloc(1, 4096);
    e->largo = 0;
    e->cursor = 0;
    e->sel_ancla = -1;
    e->ruta[0] = '\0';
    recortar_texto(e->titulo, "sin titulo", sizeof(e->titulo));
    e->modificado = 0;
    if (e->texto) {
        const char *bienvenida =
            "-- Texture Paint XP: editor de codigo\n"
            "-- Ctrl+S guardar, Ctrl+O abrir, Ctrl+N nuevo\n"
            "\n";
        int n = (int)strlen(bienvenida);
        memcpy(e->texto, bienvenida, (size_t)n);
        e->largo = n;
        e->cursor = n;
        e->texto[n] = '\0';
    }
}

void editor_terminar(EditorCodigo *e) {
    if (e->texto) free(e->texto);
    e->texto = NULL;
    e->largo = 0;
    e->cursor = 0;
}

void editor_nuevo(EditorCodigo *e) {
    if (e->texto) e->texto[0] = '\0';
    e->largo = 0;
    e->cursor = 0;
    e->sel_ancla = -1;
    e->scroll_x = e->scroll_y = 0;
    e->ruta[0] = '\0';
    recortar_texto(e->titulo, "sin titulo", sizeof(e->titulo));
    e->modificado = 0;
    app_invalidar();
}

void editor_abrir_ruta(EditorCodigo *e, const char *ruta) {
    char *datos = NULL;
    int largo = 0;
    if (!leer_archivo(ruta, &datos, &largo)) {
        dialogo_error("No se pudo abrir el archivo.");
        return;
    }
    if (!e->texto) e->texto = (char *)calloc(1, 4096);
    if (!e->texto) { free(datos); return; }
    if (largo + 1 > 4096) {
        char *nuevo = (char *)realloc(e->texto, (size_t)largo + 1);
        if (nuevo) e->texto = nuevo;
    }
    memcpy(e->texto, datos, (size_t)largo);
    e->texto[largo] = '\0';
    e->largo = largo;
    e->cursor = 0;
    e->sel_ancla = -1;
    e->scroll_x = e->scroll_y = 0;
    e->modificado = 0;
    recortar_texto(e->ruta, ruta, sizeof(e->ruta));
    ruta_nombre(e->titulo, sizeof(e->titulo), ruta);
    free(datos);
    app_estado("Abierto: %s", e->titulo);
    app_invalidar();
}

void editor_abrir(EditorCodigo *e) {
    char ruta[MAX_PATH];
    OPENFILENAMEA ofn;
    ruta[0] = '\0';
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = "Codigo y texto\0*.c;*.h;*.txt;*.lua;*.rs;*.js;*.py;*.json\0Todos los archivos\0*.*\0\0";
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameA(&ofn)) editor_abrir_ruta(e, ruta);
}

void editor_guardar(EditorCodigo *e) {
    if (!e->ruta[0]) {
        editor_guardar_como(e);
        return;
    }
    if (!escribir_archivo(e->ruta, e->texto ? e->texto : "", e->largo)) {
        dialogo_error("No se pudo guardar el archivo.");
        return;
    }
    e->modificado = 0;
    app_estado("Guardado: %s", e->titulo);
    app_invalidar();
}

void editor_guardar_como(EditorCodigo *e) {
    char ruta[MAX_PATH];
    OPENFILENAMEA ofn;
    recortar_texto(ruta, e->ruta[0] ? e->ruta : "codigo.txt", sizeof(ruta));
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = "Codigo y texto\0*.c;*.h;*.txt;*.lua;*.rs;*.js;*.py;*.json\0Todos los archivos\0*.*\0\0";
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "txt";
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameA(&ofn)) {
        recortar_texto(e->ruta, ruta, sizeof(e->ruta));
        ruta_nombre(e->titulo, sizeof(e->titulo), ruta);
        editor_guardar(e);
    }
}

void editor_cerrar(EditorCodigo *e) {
    if (e->modificado) {
        if (!dialogo_confirmar("Cerrar editor", "El documento tiene cambios sin guardar.\n¿Cerrarlo de todos modos?"))
            return;
    }
    e->activo = 0;
    app_cerrar_editor_codigo();
}

/* ------------------------------------------------------------------ */
/* Dibujo                                                              */
/* ------------------------------------------------------------------ */
static void dibujar_boton_editor(Zona z, const char *texto) {
    int hover = zona_contiene(z, g.raton_x, g.raton_y);
    dibujar_rect_z(z, hover ? 0x004A4A4Au : EDITOR_BOTON);
    dibujar_marco_z(z, EDITOR_BOTON_BORDE);
    dibujar_texto_fuente(z.x + (z.ancho - texto_ancho_fuente(texto, g.fuente_peq)) / 2,
                         z.y + (z.alto - 12) / 2, texto, EDITOR_TEXTO, g.fuente_peq);
}

void editor_dibujar(EditorCodigo *e) {
    ZonasEditor z = editor_zonas(e);
    int linea_actual, columna_actual;
    int total_lineas;
    int primera, ultima, i;
    int y;
    int ancho_lineas;
    char texto[64];

    /* Fondo general. */
    dibujar_rect(0, 0, g.ancho, g.alto, EDITOR_FONDO);

    /* Barra superior. */
    dibujar_rect_z(z.barra, EDITOR_BARRA);
    dibujar_rect(0, zona_abajo(z.barra) - 1, g.ancho, zona_abajo(z.barra), EDITOR_BARRA_BORDE);
    dibujar_boton_editor(z.nuevo, "Nuevo");
    dibujar_boton_editor(z.abrir, "Abrir");
    dibujar_boton_editor(z.guardar, "Guardar");
    dibujar_boton_editor(z.cerrar, "Volver");

    /* Titulo del documento, centrado en la barra si cabe. */
    {
        char titulo[192];
        formatear(titulo, sizeof(titulo), "%s%s", e->titulo, e->modificado ? " *" : "");
        if (texto_ancho_fuente(titulo, g.fuente_peq) < g.ancho - 700)
            dibujar_texto_fuente((g.ancho - texto_ancho_fuente(titulo, g.fuente_peq)) / 2,
                                 z.barra.y + 14, titulo, EDITOR_TENUE, g.fuente_peq);
    }

    /* Panel izquierdo vacio de 200 px. */
    dibujar_rect(0, z.texto.y, EDITOR_PANEL_ANCHO, zona_abajo(z.texto), EDITOR_PANEL);
    dibujar_rect(EDITOR_PANEL_ANCHO - 1, z.texto.y, EDITOR_PANEL_ANCHO, zona_abajo(z.texto), EDITOR_PANEL_SEP);

    /* Area de texto. */
    dibujar_rect_z(z.texto, EDITOR_FONDO);
    dibujar_rect(z.texto.x, z.texto.y, g.ancho, z.texto.y + 1, EDITOR_BARRA_BORDE);

    /* Cursor y linea. */
    linea_actual = editor_linea_de(e, e->cursor);
    columna_actual = editor_columna_de(e, e->cursor);
    total_lineas = editor_total_lineas(e);

    /* Ancho de la columna de numeros. */
    formatear(texto, sizeof(texto), "%d", total_lineas);
    ancho_lineas = texto_ancho_fuente(texto, g.fuente_codigo) + 18;
    if (ancho_lineas < EDITOR_CABECERA_ANCHO) ancho_lineas = EDITOR_CABECERA_ANCHO;

    primera = e->scroll_y / EDITOR_LINEA_ALTO;
    ultima = primera + z.texto.alto / EDITOR_LINEA_ALTO + 2;
    if (primera < 0) primera = 0;

    for (i = primera; i <= ultima && i < total_lineas; i++) {
        int inicio = editor_inicio_linea(e, i);
        int fin = editor_inicio_linea(e, i + 1);
        int largo_linea;
        int ly = z.texto.y + i * EDITOR_LINEA_ALTO - e->scroll_y;
        char numero[32];
        char linea[4096];
        int n;
        if (ly + EDITOR_LINEA_ALTO < z.texto.y || ly > zona_abajo(z.texto)) continue;
        if (fin > inicio && e->texto[fin - 1] == '\n') fin--;

        if (i == linea_actual) {
            dibujar_rect(z.texto.x, ly, g.ancho, ly + EDITOR_LINEA_ALTO, EDITOR_LINEA);
        }

        formatear(numero, sizeof(numero), "%d", i + 1);
        dibujar_texto_fuente(z.texto.x + ancho_lineas - 8 - texto_ancho_fuente(numero, g.fuente_codigo),
                             ly + 2, numero, EDITOR_NUMERO, g.fuente_codigo);

        largo_linea = fin - inicio;
        if (largo_linea > (int)sizeof(linea) - 1) largo_linea = (int)sizeof(linea) - 1;
        n = 0;
        while (n < largo_linea) {
            if ((unsigned char)e->texto[inicio + n] == '\t') {
                int k;
                for (k = 0; k < EDITOR_PESTANA; k++) linea[n] = ' ';
                n++;
            } else {
                linea[n] = e->texto[inicio + n];
                n++;
            }
        }
        linea[n] = '\0';
        dibujar_texto_fuente(z.texto.x + ancho_lineas - e->scroll_x, ly + 2, linea, EDITOR_TEXTO, g.fuente_codigo);

        /* Cursor. */
        if (i == linea_actual) {
            int cx;
            char antes[4096];
            int nc = columna_actual;
            if (nc > (int)sizeof(antes) - 1) nc = (int)sizeof(antes) - 1;
            memcpy(antes, linea, (size_t)nc);
            antes[nc] = '\0';
            cx = z.texto.x + ancho_lineas - e->scroll_x + texto_ancho_fuente(antes, g.fuente_codigo);
            if ((GetTickCount() / 500) % 2 == 0 || g.boton_izq) {
                dibujar_rect(cx, ly + 1, cx + 2, ly + EDITOR_LINEA_ALTO - 1, EDITOR_CURSOR);
            }
        }
    }

    /* Barra de scroll vertical, si hace falta. */
    {
        int total_px = total_lineas * EDITOR_LINEA_ALTO;
        if (total_px > z.texto.alto) {
            int alto_barra = z.texto.alto * z.texto.alto / total_px;
            int y0;
            if (alto_barra < 20) alto_barra = 20;
            y0 = z.texto.y + (z.texto.alto - alto_barra) * e->scroll_y / (total_px - z.texto.alto + 1);
            dibujar_rect(z.scroll.x, z.texto.y, zona_derecha(z.scroll), zona_abajo(z.texto), EDITOR_FONDO);
            dibujar_rect(z.scroll.x + 3, y0, zona_derecha(z.scroll) - 3, y0 + alto_barra, EDITOR_PULGAR);
        }
    }

    /* Barra de estado. */
    dibujar_rect_z(z.estado, EDITOR_BARRA);
    dibujar_rect(0, z.estado.y, g.ancho, z.estado.y + 1, EDITOR_BARRA_BORDE);
    formatear(texto, sizeof(texto), "Ln %d, Col %d   %d caracteres", linea_actual + 1,
              columna_actual + 1, e->largo);
    dibujar_texto_fuente(10, z.estado.y + 5, texto, EDITOR_TENUE, g.fuente_peq);
    if (e->ruta[0]) {
        dibujar_texto_fuente(280, z.estado.y + 5, e->ruta, EDITOR_TENUE, g.fuente_peq);
    }
    (void)y;
}

/* ------------------------------------------------------------------ */
/* Entrada                                                             */
/* ------------------------------------------------------------------ */
static int editor_pos_en(EditorCodigo *e, int x, int y) {
    ZonasEditor z = editor_zonas(e);
    int total_lineas = editor_total_lineas(e);
    int linea, i, mejor = 0;
    int ancho_lineas;
    char texto[32];
    formatear(texto, sizeof(texto), "%d", total_lineas);
    ancho_lineas = texto_ancho_fuente(texto, g.fuente_codigo) + 18;
    if (ancho_lineas < EDITOR_CABECERA_ANCHO) ancho_lineas = EDITOR_CABECERA_ANCHO;

    linea = (y - z.texto.y + e->scroll_y) / EDITOR_LINEA_ALTO;
    if (linea < 0) linea = 0;
    if (linea >= total_lineas) linea = total_lineas - 1;

    {
        int inicio = editor_inicio_linea(e, linea);
        int fin = editor_inicio_linea(e, linea + 1);
        int objetivo = x - (z.texto.x + ancho_lineas) + e->scroll_x;
        char buf[4096];
        int n = 0;
        if (fin > inicio && e->texto[fin - 1] == '\n') fin--;
        while (inicio + n < fin && n < (int)sizeof(buf) - 1) {
            buf[n] = e->texto[inicio + n];
            n++;
        }
        buf[n] = '\0';
        mejor = 0;
        for (i = 0; i <= n; i++) {
            char corte[4096];
            memcpy(corte, buf, (size_t)i);
            corte[i] = '\0';
            if (texto_ancho_fuente(corte, g.fuente_codigo) <= objetivo) mejor = i;
            else break;
        }
        return inicio + mejor;
    }
}

void editor_scroll_visible(EditorCodigo *e) {
    ZonasEditor z = editor_zonas(e);
    int linea = editor_linea_de(e, e->cursor);
    int y = linea * EDITOR_LINEA_ALTO;
    if (y < e->scroll_y) e->scroll_y = y;
    if (y + EDITOR_LINEA_ALTO > e->scroll_y + z.texto.alto)
        e->scroll_y = y + EDITOR_LINEA_ALTO - z.texto.alto;
    if (e->scroll_y < 0) e->scroll_y = 0;
}

void editor_pulsar(EditorCodigo *e, int x, int y, int boton) {
    ZonasEditor z = editor_zonas(e);
    (void)boton;
    if (zona_contiene(z.nuevo, x, y)) { editor_nuevo(e); return; }
    if (zona_contiene(z.abrir, x, y)) { editor_abrir(e); return; }
    if (zona_contiene(z.guardar, x, y)) { editor_guardar(e); return; }
    if (zona_contiene(z.cerrar, x, y)) { editor_cerrar(e); return; }
    if (zona_contiene(z.texto, x, y) || zona_contiene(z.scroll, x, y)) {
        e->cursor = editor_pos_en(e, x, y);
        e->sel_ancla = -1;
        app_invalidar();
    }
}

void editor_soltar(EditorCodigo *e, int x, int y, int boton) {
    (void)e; (void)x; (void)y; (void)boton;
}

void editor_mover(EditorCodigo *e, int x, int y, int botones) {
    (void)e; (void)x; (void)y; (void)botones;
}

void editor_rueda(EditorCodigo *e, int x, int y, int delta) {
    ZonasEditor z = editor_zonas(e);
    (void)x; (void)y;
    e->scroll_y += delta > 0 ? -EDITOR_LINEA_ALTO * 3 : EDITOR_LINEA_ALTO * 3;
    {
        int total = editor_total_lineas(e) * EDITOR_LINEA_ALTO;
        int maximo = total - z.texto.alto;
        if (maximo < 0) maximo = 0;
        if (e->scroll_y > maximo) e->scroll_y = maximo;
    }
    if (e->scroll_y < 0) e->scroll_y = 0;
    app_invalidar();
}

void editor_tecla(EditorCodigo *e, int vk, int ctrl, int shift) {
    if (ctrl && vk == 'S') { editor_guardar(e); return; }
    if (ctrl && vk == 'O') { editor_abrir(e); return; }
    if (ctrl && vk == 'N') { editor_nuevo(e); return; }
    if (ctrl && vk == 'A') { e->sel_ancla = 0; e->cursor = e->largo; app_invalidar(); return; }

    if (vk == VK_LEFT) {
        if (e->cursor > 0) e->cursor--;
        editor_scroll_visible(e);
        app_invalidar();
        return;
    }
    if (vk == VK_RIGHT) {
        if (e->cursor < e->largo) e->cursor++;
        editor_scroll_visible(e);
        app_invalidar();
        return;
    }
    if (vk == VK_UP || vk == VK_DOWN) {
        int linea = editor_linea_de(e, e->cursor);
        int columna = editor_columna_de(e, e->cursor);
        int destino = linea + (vk == VK_UP ? -1 : 1);
        int total = editor_total_lineas(e);
        int inicio;
        if (destino < 0) destino = 0;
        if (destino >= total) destino = total - 1;
        inicio = editor_inicio_linea(e, destino);
        e->cursor = inicio;
        while (e->cursor < e->largo && e->texto[e->cursor] != '\n' && columna > 0) {
            e->cursor++;
            columna--;
        }
        editor_scroll_visible(e);
        app_invalidar();
        return;
    }
    if (vk == VK_HOME) {
        e->cursor = editor_inicio_linea(e, editor_linea_de(e, e->cursor));
        app_invalidar();
        return;
    }
    if (vk == VK_END) {
        int linea = editor_linea_de(e, e->cursor);
        int fin = editor_inicio_linea(e, linea + 1);
        if (fin > 0 && e->texto[fin - 1] == '\n') fin--;
        e->cursor = fin;
        app_invalidar();
        return;
    }
    if (vk == VK_PRIOR || vk == VK_NEXT) {
        ZonasEditor z = editor_zonas(e);
        int filas = z.texto.alto / EDITOR_LINEA_ALTO;
        int linea = editor_linea_de(e, e->cursor);
        int columna = editor_columna_de(e, e->cursor);
        int destino = linea + (vk == VK_PRIOR ? -filas : filas);
        int total = editor_total_lineas(e);
        int inicio;
        if (destino < 0) destino = 0;
        if (destino >= total) destino = total - 1;
        inicio = editor_inicio_linea(e, destino);
        e->cursor = inicio;
        while (e->cursor < e->largo && e->texto[e->cursor] != '\n' && columna > 0) {
            e->cursor++;
            columna--;
        }
        editor_scroll_visible(e);
        app_invalidar();
        return;
    }
    if (vk == VK_BACK) {
        if (e->sel_ancla >= 0 && e->sel_ancla != e->cursor) {
            int a = e->sel_ancla < e->cursor ? e->sel_ancla : e->cursor;
            int b = e->sel_ancla < e->cursor ? e->cursor : e->sel_ancla;
            editor_borrar_rango(e, a, b);
            e->sel_ancla = -1;
        } else if (e->cursor > 0) {
            editor_borrar_rango(e, e->cursor - 1, e->cursor);
        }
        editor_scroll_visible(e);
        app_invalidar();
        return;
    }
    if (vk == VK_DELETE) {
        if (e->cursor < e->largo) editor_borrar_rango(e, e->cursor, e->cursor + 1);
        app_invalidar();
        return;
    }
    if (vk == VK_RETURN) {
        editor_insertar(e, "\n");
        editor_scroll_visible(e);
        app_invalidar();
        return;
    }
    if (vk == VK_TAB) {
        editor_insertar(e, "    ");
        app_invalidar();
        return;
    }
    (void)shift;
}

void editor_caracter(EditorCodigo *e, int ch) {
    char texto[2];
    if (ch < 32 || ch > 255) return;
    texto[0] = (char)ch;
    texto[1] = '\0';
    editor_insertar(e, texto);
    editor_scroll_visible(e);
    app_invalidar();
}
