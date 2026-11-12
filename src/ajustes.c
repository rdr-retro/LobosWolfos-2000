/* ==========================================================================
 * ajustes.c - propiedades del proyecto (fase 14).
 *
 * El boton AJUSTES DEL PROYECTO de la barra de menus abre una ventana con
 * las propiedades que viajan dentro de la logica: nombre, resolucion de la
 * ventana del juego, limite de cuadros, velocidad base y color de fondo.
 * Los valores se guardan al final del .logic como una linea PROYECTO, de
 * modo que los lectores antiguos la ignoran y el motor la usa al cargar el
 * proyecto. La imagen BIOS mantiene su pantalla de 640x480.
 * ========================================================================== */
#include "app.h"

#define AJ_ANCHO 460
#define AJ_ALTO  340

#define AJ_FONDO     0x00F4F4F4u
#define AJ_BORDE     0x00708090u
#define AJ_CAMPO     0x00FFFFFFu
#define AJ_CAMPO_OFF 0x00E4E4E4u
#define AJ_TITULO    0x003278E0u
#define AJ_TITULO_TX 0x00FFFFFFu
#define AJ_TEXTO     0x00303030u
#define AJ_GRIS      0x00909090u
#define AJ_BOTON     0x00DCDCDCu
#define AJ_BOTON_HOV 0x00C4D4E8u

/* Controles de la ventana. */
enum {
    AJ_NINGUNO = -1,
    AJ_TITULO_BARRA = 0,
    AJ_CERRAR,
    AJ_NOMBRE,
    AJ_RES_MENOS,
    AJ_RES_MAS,
    AJ_LIMITE,
    AJ_FPS_MENOS,
    AJ_FPS_MAS,
    AJ_VEL_MENOS,
    AJ_VEL_MAS,
    AJ_FONDO_0,
    AJ_FONDO_1,
    AJ_FONDO_2,
    AJ_FONDO_3,
    AJ_FONDO_4,
    AJ_FONDO_5,
    AJ_RESTABLECER = 20,
    AJ_CANCELAR,
    AJ_ACEPTAR
};

/* Resoluciones ofrecidas con su relacion de aspecto. */
typedef struct Resolucion {
    int ancho, alto;
    const char *texto;
} Resolucion;

static const Resolucion RESOLUCIONES[] = {
    { 640, 480,  "640 x 480 (4:3)" },
    { 800, 600,  "800 x 600 (4:3)" },
    { 1024, 768, "1024 x 768 (4:3)" },
    { 1280, 720, "1280 x 720 (16:9)" },
    { 1600, 900, "1600 x 900 (16:9)" },
    { 1920, 1080, "1920 x 1080 (16:9)" }
};
#define AJ_NUM_RESOLUCIONES ((int)(sizeof(RESOLUCIONES) / sizeof(RESOLUCIONES[0])))

static const int FRECUENCIAS[] = { 30, 60, 75, 120, 144, 240 };
#define AJ_NUM_FRECUENCIAS ((int)(sizeof(FRECUENCIAS) / sizeof(FRECUENCIAS[0])))

static const u32 FONDOS[] = {
    0x00000000u, /* negro */
    0x00101826u, /* azul noche */
    0x00FFFFFFu, /* blanco */
    0x001E5A2Eu, /* verde */
    0x002E1E5Au, /* morado */
    0x00404040u  /* gris */
};
#define AJ_NUM_FONDOS ((int)(sizeof(FONDOS) / sizeof(FONDOS[0])))

/* ------------------------------------------------------------------ */
/* Valores por defecto y utilidades                                    */
/* ------------------------------------------------------------------ */
static void ajustes_valores_defecto(AjustesProyecto *a) {
    strcpy(a->nombre, "Mi proyecto");
    a->ancho = 640;
    a->alto = 480;
    a->limitar_cuadros = 1;
    a->fps = 60;
    a->velocidad = 3;
    a->fondo = 0x00000000u;
}

void ajustes_proyecto_iniciar(void) {
    ajustes_valores_defecto(&g.ajustes);
    g.ajustes_ed = g.ajustes;
    g.ajustes_visible = 0;
    g.ajustes_x = -1;   /* posicion automatica, centrada */
    g.ajustes_y = -1;
    g.ajustes_arrastrando = 0;
    g.ajustes_foco = -1;
    g.ajustes_cursor = 0;
    g.ajustes_hover = AJ_NINGUNO;
    g.ajustes_control = AJ_NINGUNO;
    g.ajustes_off_x = 0;
    g.ajustes_off_y = 0;
}

static int resolucion_indice(const AjustesProyecto *a) {
    int i;
    for (i = 0; i < AJ_NUM_RESOLUCIONES; i++)
        if (RESOLUCIONES[i].ancho == a->ancho && RESOLUCIONES[i].alto == a->alto)
            return i;
    return 0;
}

static int frecuencia_indice(const AjustesProyecto *a) {
    int i;
    for (i = 0; i < AJ_NUM_FRECUENCIAS; i++)
        if (FRECUENCIAS[i] == a->fps) return i;
    return 1; /* 60 */
}

static int fondo_indice(const AjustesProyecto *a) {
    int i;
    for (i = 0; i < AJ_NUM_FONDOS; i++)
        if (FONDOS[i] == a->fondo) return i;
    return 0;
}

static void ajustes_normalizar(AjustesProyecto *a) {
    if (a->ancho < 320 || a->ancho > 4096) a->ancho = 640;
    if (a->alto < 200 || a->alto > 4096) a->alto = 480;
    if (a->fps < 1 || a->fps > 1000) a->fps = 60;
    if (a->velocidad < 1) a->velocidad = 1;
    if (a->velocidad > 10) a->velocidad = 10;
    a->limitar_cuadros = a->limitar_cuadros ? 1 : 0;
    if (!a->nombre[0]) strcpy(a->nombre, "Mi proyecto");
}

/* ------------------------------------------------------------------ */
/* Abrir, cerrar y aplicar                                             */
/* ------------------------------------------------------------------ */
void ajustes_proyecto_abrir(void) {
    g.ajustes_ed = g.ajustes;
    g.ajustes_visible = 1;
    g.ajustes_arrastrando = 0;
    g.ajustes_foco = -1;
    g.ajustes_cursor = (int)strlen(g.ajustes_ed.nombre);
    g.ajustes_hover = AJ_NINGUNO;
    g.ajustes_control = AJ_NINGUNO;
    g.menu_abierto = -1;
    g.textura_menu_abierto = 0;
    app_invalidar();
}

void ajustes_proyecto_cerrar(int aplicar) {
    if (!g.ajustes_visible) return;
    if (aplicar) {
        ajustes_normalizar(&g.ajustes_ed);
        g.ajustes = g.ajustes_ed;
        app_estado("Ajustes del proyecto aplicados.");
    }
    g.ajustes_visible = 0;
    g.ajustes_arrastrando = 0;
    g.ajustes_foco = -1;
    g.ajustes_control = AJ_NINGUNO;
    app_invalidar();
}

/* ------------------------------------------------------------------ */
/* Geometria de la ventana                                             */
/* ------------------------------------------------------------------ */
static Zona ajustes_panel(void) {
    Zona z;
    int x, y;
    if (g.ajustes_x < 0 || g.ajustes_y < 0) {
        x = (g.ancho - AJ_ANCHO) / 2;
        y = AREA_SUP_ALTO + (g.alto - AREA_SUP_ALTO - AJ_ALTO) / 2;
    } else {
        x = g.ajustes_x;
        y = g.ajustes_y;
    }
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + AJ_ANCHO > g.ancho) x = g.ancho - AJ_ANCHO;
    if (y + AJ_ALTO > g.alto) y = g.alto - AJ_ALTO;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    z.x = x;
    z.y = y;
    z.ancho = AJ_ANCHO;
    z.alto = AJ_ALTO;
    return z;
}

static Zona ajustes_titulo_zona(void) {
    Zona p = ajustes_panel();
    Zona z;
    z.x = p.x;
    z.y = p.y;
    z.ancho = p.ancho;
    z.alto = 28;
    return z;
}

static Zona ajustes_cerrar_zona(void) {
    Zona p = ajustes_panel();
    Zona z;
    z.x = p.x + p.ancho - 26;
    z.y = p.y + 4;
    z.ancho = 20;
    z.alto = 20;
    return z;
}

static Zona ajustes_nombre_zona(void) {
    Zona p = ajustes_panel();
    Zona z;
    z.x = p.x + 164;
    z.y = p.y + 44;
    z.ancho = p.ancho - 164 - 16;
    z.alto = 24;
    return z;
}

static Zona ajustes_res_zona(void) {
    Zona p = ajustes_panel();
    Zona z;
    z.x = p.x + 164;
    z.y = p.y + 84;
    z.ancho = p.ancho - 164 - 16;
    z.alto = 24;
    return z;
}

static Zona ajustes_fps_zona(void) {
    Zona p = ajustes_panel();
    Zona z;
    z.x = p.x + 164;
    z.y = p.y + 156;
    z.ancho = p.ancho - 164 - 16;
    z.alto = 24;
    return z;
}

static Zona ajustes_vel_zona(void) {
    Zona p = ajustes_panel();
    Zona z;
    z.x = p.x + 164;
    z.y = p.y + 192;
    z.ancho = p.ancho - 164 - 16;
    z.alto = 24;
    return z;
}

static Zona ajustes_limite_zona(void) {
    Zona p = ajustes_panel();
    Zona z;
    z.x = p.x + 164;
    z.y = p.y + 122;
    z.ancho = 18;
    z.alto = 18;
    return z;
}

static Zona ajustes_fondo_zona(int indice) {
    Zona p = ajustes_panel();
    Zona z;
    z.x = p.x + 164 + indice * 34;
    z.y = p.y + 228;
    z.ancho = 24;
    z.alto = 24;
    return z;
}

static Zona ajustes_boton_zona(int control) {
    Zona p = ajustes_panel();
    Zona z;
    z.y = p.y + AJ_ALTO - 46;
    z.alto = 28;
    if (control == AJ_RESTABLECER) {
        z.x = p.x + 16;
        z.ancho = 124;
    } else if (control == AJ_CANCELAR) {
        z.x = p.x + p.ancho - 212;
        z.ancho = 92;
    } else {
        z.x = p.x + p.ancho - 108;
        z.ancho = 92;
    }
    return z;
}

static Zona ajustes_flecha_zona(Zona campo, int derecha) {
    Zona z;
    z.ancho = 24;
    z.alto = campo.alto;
    z.y = campo.y;
    z.x = derecha ? campo.x + campo.ancho - z.ancho : campo.x;
    return z;
}

/* ------------------------------------------------------------------ */
/* Dibujo                                                              */
/* ------------------------------------------------------------------ */
static void ajustes_boton_dibujar(Zona z, const char *texto, int hover,
                                  int activo) {
    u32 fondo = hover ? AJ_BOTON_HOV : AJ_BOTON;
    if (activo) fondo = 0x00B4C8E0u;
    dibujar_rect_z(z, fondo);
    dibujar_marco_z(z, AJ_BORDE);
    dibujar_texto(z.x + (z.ancho - texto_ancho(texto)) / 2,
                  z.y + (z.alto - texto_alto()) / 2, texto, AJ_TEXTO);
}

static void ajustes_campo_dibujar(Zona z, const char *texto, int activo) {
    dibujar_rect_z(z, AJ_CAMPO);
    dibujar_marco_z(z, activo ? AJ_TITULO : AJ_BORDE);
    dibujar_texto(z.x + 8, z.y + (z.alto - texto_alto()) / 2, texto, AJ_TEXTO);
}

static void ajustes_flechas_dibujar(Zona campo, int hover_menos,
                                    int hover_mas, int activo) {
    Zona izq = ajustes_flecha_zona(campo, 0);
    Zona der = ajustes_flecha_zona(campo, 1);
    u32 color = activo ? AJ_TEXTO : AJ_GRIS;
    if (hover_menos) dibujar_rect_z(izq, AJ_BOTON_HOV);
    if (hover_mas) dibujar_rect_z(der, AJ_BOTON_HOV);
    dibujar_marco_z(izq, AJ_BORDE);
    dibujar_marco_z(der, AJ_BORDE);
    dibujar_texto(izq.x + (izq.ancho - texto_ancho("<")) / 2,
                  izq.y + (izq.alto - texto_alto()) / 2, "<", color);
    dibujar_texto(der.x + (der.ancho - texto_ancho(">")) / 2,
                  der.y + (der.alto - texto_alto()) / 2, ">", color);
}

static void ajustes_marco_dibujar(void) {
    Zona p = ajustes_panel();
    /* Sombra y marco exterior. */
    dibujar_rect(p.x + 4, p.y + 4, p.x + p.ancho + 4, p.y + p.alto + 4,
                 0x00000000u);
    dibujar_rect_z(p, AJ_FONDO);
    dibujar_marco_z(p, AJ_BORDE);
    dibujar_rect_z(ajustes_titulo_zona(), AJ_TITULO);
    dibujar_texto(p.x + 12, p.y + (28 - texto_alto()) / 2,
                  "AJUSTES DEL PROYECTO", AJ_TITULO_TX);
    ajustes_boton_dibujar(ajustes_cerrar_zona(), "X",
                          g.ajustes_hover == AJ_CERRAR,
                          g.ajustes_control == AJ_CERRAR);
}

void ui_dibujar_ajustes(void) {
    Zona p, z;
    char texto[64];
    int i, hover;

    if (!g.ajustes_visible) return;
    p = ajustes_panel();
    ajustes_marco_dibujar();

    /* Nombre del proyecto. */
    dibujar_texto(p.x + 16, p.y + 44 + (24 - texto_alto()) / 2,
                  "Nombre del proyecto", AJ_TEXTO);
    z = ajustes_nombre_zona();
    ajustes_campo_dibujar(z, g.ajustes_ed.nombre, g.ajustes_foco == 0);
    if (g.ajustes_foco == 0) {
        /* Cursor de texto. */
        char parcial[AJUSTES_NOMBRE_MAX + 1];
        int n = g.ajustes_cursor;
        if (n < 0) n = 0;
        if (n > (int)strlen(g.ajustes_ed.nombre)) n = (int)strlen(g.ajustes_ed.nombre);
        memcpy(parcial, g.ajustes_ed.nombre, (size_t)n);
        parcial[n] = 0;
        dibujar_rect(z.x + 8 + texto_ancho(parcial), z.y + 4,
                     z.x + 9 + texto_ancho(parcial), z.y + z.alto - 4,
                     AJ_TEXTO);
    }

    /* Resolucion. */
    dibujar_texto(p.x + 16, p.y + 84 + (24 - texto_alto()) / 2,
                  "Resolucion", AJ_TEXTO);
    z = ajustes_res_zona();
    dibujar_rect_z(z, AJ_CAMPO);
    dibujar_marco_z(z, AJ_BORDE);
    sprintf(texto, "%s", RESOLUCIONES[resolucion_indice(&g.ajustes_ed)].texto);
    dibujar_texto(z.x + (z.ancho - texto_ancho(texto)) / 2,
                  z.y + (z.alto - texto_alto()) / 2, texto, AJ_TEXTO);
    ajustes_flechas_dibujar(z, g.ajustes_hover == AJ_RES_MENOS,
                            g.ajustes_hover == AJ_RES_MAS,
                            g.ajustes_control == AJ_RES_MENOS ||
                            g.ajustes_control == AJ_RES_MAS);

    /* Limitar cuadros. */
    z = ajustes_limite_zona();
    dibujar_rect_z(z, AJ_CAMPO);
    dibujar_marco_z(z, AJ_BORDE);
    if (g.ajustes_ed.limitar_cuadros) {
        dibujar_linea(z.x + 4, z.y + 9, z.x + 8, z.y + 13, AJ_TITULO);
        dibujar_linea(z.x + 8, z.y + 13, z.x + 14, z.y + 5, AJ_TITULO);
    }
    dibujar_texto(z.x + 26, z.y + (z.alto - texto_alto()) / 2,
                  "Limitar cuadros", AJ_TEXTO);

    z = ajustes_fps_zona();
    dibujar_texto(p.x + 16, z.y + (z.alto - texto_alto()) / 2,
                  "Cuadros por segundo", AJ_TEXTO);
    if (g.ajustes_ed.limitar_cuadros) {
        dibujar_rect_z(z, AJ_CAMPO);
        dibujar_marco_z(z, AJ_BORDE);
        sprintf(texto, "%d FPS", g.ajustes_ed.fps);
        dibujar_texto(z.x + (z.ancho - texto_ancho(texto)) / 2,
                      z.y + (z.alto - texto_alto()) / 2, texto, AJ_TEXTO);
        ajustes_flechas_dibujar(z, g.ajustes_hover == AJ_FPS_MENOS,
                                g.ajustes_hover == AJ_FPS_MAS,
                                g.ajustes_control == AJ_FPS_MENOS ||
                                g.ajustes_control == AJ_FPS_MAS);
    } else {
        dibujar_rect_z(z, AJ_CAMPO_OFF);
        dibujar_marco_z(z, AJ_BORDE);
        dibujar_texto(z.x + (z.ancho - texto_ancho("Sin limite")) / 2,
                      z.y + (z.alto - texto_alto()) / 2, "Sin limite", AJ_GRIS);
    }

    /* Velocidad del jugador. */
    dibujar_texto(p.x + 16, p.y + 192 + (24 - texto_alto()) / 2,
                  "Velocidad del jugador", AJ_TEXTO);
    z = ajustes_vel_zona();
    dibujar_rect_z(z, AJ_CAMPO);
    dibujar_marco_z(z, AJ_BORDE);
    sprintf(texto, "%d", g.ajustes_ed.velocidad);
    dibujar_texto(z.x + (z.ancho - texto_ancho(texto)) / 2,
                  z.y + (z.alto - texto_alto()) / 2, texto, AJ_TEXTO);
    ajustes_flechas_dibujar(z, g.ajustes_hover == AJ_VEL_MENOS,
                            g.ajustes_hover == AJ_VEL_MAS,
                            g.ajustes_control == AJ_VEL_MENOS ||
                            g.ajustes_control == AJ_VEL_MAS);

    /* Color de fondo. */
    dibujar_texto(p.x + 16, p.y + 228 + (24 - texto_alto()) / 2,
                  "Color de fondo", AJ_TEXTO);
    for (i = 0; i < AJ_NUM_FONDOS; i++) {
        Zona f = ajustes_fondo_zona(i);
        int sel = (fondo_indice(&g.ajustes_ed) == i);
        hover = (g.ajustes_hover == AJ_FONDO_0 + i);
        dibujar_rect_z(f, FONDOS[i]);
        dibujar_marco_z(f, sel ? AJ_TITULO : AJ_BORDE);
        if (hover) {
            dibujar_marco(f.x - 1, f.y - 1, f.x + f.ancho, f.y + f.alto,
                          AJ_BOTON_HOV);
        }
        if (sel) {
            /* Segundo marco interior para marcar el color elegido. */
            dibujar_marco(f.x + 2, f.y + 2, f.x + f.ancho - 2,
                          f.y + f.alto - 2, AJ_TITULO_TX);
        }
    }

    /* Notas. */
    dibujar_texto(p.x + 16, p.y + 266, "Se guarda con la logica del proyecto.",
                  AJ_GRIS);
    dibujar_texto(p.x + 16, p.y + 282, "La imagen BIOS usa 640 x 480.",
                  AJ_GRIS);

    /* Botones. */
    ajustes_boton_dibujar(ajustes_boton_zona(AJ_RESTABLECER), "RESTABLECER",
                          g.ajustes_hover == AJ_RESTABLECER,
                          g.ajustes_control == AJ_RESTABLECER);
    ajustes_boton_dibujar(ajustes_boton_zona(AJ_CANCELAR), "CANCELAR",
                          g.ajustes_hover == AJ_CANCELAR,
                          g.ajustes_control == AJ_CANCELAR);
    ajustes_boton_dibujar(ajustes_boton_zona(AJ_ACEPTAR), "ACEPTAR",
                          g.ajustes_hover == AJ_ACEPTAR,
                          g.ajustes_control == AJ_ACEPTAR);
}

/* ------------------------------------------------------------------ */
/* Entrada                                                             */
/* ------------------------------------------------------------------ */
static int ajustes_control_en(int x, int y) {
    Zona p = ajustes_panel();
    int i;
    if (!zona_contiene(p, x, y)) return AJ_NINGUNO;
    if (zona_contiene(ajustes_cerrar_zona(), x, y)) return AJ_CERRAR;
    if (zona_contiene(ajustes_titulo_zona(), x, y)) return AJ_TITULO_BARRA;
    if (zona_contiene(ajustes_nombre_zona(), x, y)) return AJ_NOMBRE;

    if (zona_contiene(ajustes_flecha_zona(ajustes_res_zona(), 0), x, y))
        return AJ_RES_MENOS;
    if (zona_contiene(ajustes_flecha_zona(ajustes_res_zona(), 1), x, y))
        return AJ_RES_MAS;

    if (zona_contiene(ajustes_limite_zona(), x, y)) return AJ_LIMITE;
    if (zona_contiene(ajustes_fps_zona(), x, y)) {
        if (g.ajustes_ed.limitar_cuadros) {
            if (zona_contiene(ajustes_flecha_zona(ajustes_fps_zona(), 0), x, y))
                return AJ_FPS_MENOS;
            if (zona_contiene(ajustes_flecha_zona(ajustes_fps_zona(), 1), x, y))
                return AJ_FPS_MAS;
        }
        return AJ_NINGUNO;
    }

    if (zona_contiene(ajustes_flecha_zona(ajustes_vel_zona(), 0), x, y))
        return AJ_VEL_MENOS;
    if (zona_contiene(ajustes_flecha_zona(ajustes_vel_zona(), 1), x, y))
        return AJ_VEL_MAS;

    for (i = 0; i < AJ_NUM_FONDOS; i++)
        if (zona_contiene(ajustes_fondo_zona(i), x, y)) return AJ_FONDO_0 + i;

    if (zona_contiene(ajustes_boton_zona(AJ_RESTABLECER), x, y))
        return AJ_RESTABLECER;
    if (zona_contiene(ajustes_boton_zona(AJ_CANCELAR), x, y))
        return AJ_CANCELAR;
    if (zona_contiene(ajustes_boton_zona(AJ_ACEPTAR), x, y))
        return AJ_ACEPTAR;
    return AJ_NINGUNO;
}

/* Indice del nombre mas cercano a una x del campo. */
static int ajustes_indice_x(int x) {
    Zona z = ajustes_nombre_zona();
    char parcial[AJUSTES_NOMBRE_MAX + 1];
    int i, n = (int)strlen(g.ajustes_ed.nombre);
    for (i = 0; i <= n; i++) {
        memcpy(parcial, g.ajustes_ed.nombre, (size_t)i);
        parcial[i] = 0;
        if (z.x + 8 + texto_ancho(parcial) >= x) return i;
    }
    return n;
}

int ajustes_pulsar(int x, int y, int boton) {
    int c;
    if (!g.ajustes_visible) return 0;
    if (boton != 0) return 1;
    c = ajustes_control_en(x, y);
    g.ajustes_control = c;
    if (c == AJ_TITULO_BARRA) {
        Zona p = ajustes_panel();
        g.ajustes_arrastrando = 1;
        g.ajustes_off_x = x - p.x;
        g.ajustes_off_y = y - p.y;
    } else if (c == AJ_CERRAR || c == AJ_CANCELAR) {
        ajustes_proyecto_cerrar(0);
    } else if (c == AJ_ACEPTAR) {
        ajustes_proyecto_cerrar(1);
    } else if (c == AJ_RESTABLECER) {
        ajustes_valores_defecto(&g.ajustes_ed);
        g.ajustes_cursor = (int)strlen(g.ajustes_ed.nombre);
    } else if (c == AJ_NOMBRE) {
        g.ajustes_foco = 0;
        g.ajustes_cursor = ajustes_indice_x(x);
    } else if (c == AJ_RES_MENOS || c == AJ_RES_MAS) {
        int i = resolucion_indice(&g.ajustes_ed);
        i += (c == AJ_RES_MAS) ? 1 : -1;
        if (i < 0) i = AJ_NUM_RESOLUCIONES - 1;
        if (i >= AJ_NUM_RESOLUCIONES) i = 0;
        g.ajustes_ed.ancho = RESOLUCIONES[i].ancho;
        g.ajustes_ed.alto = RESOLUCIONES[i].alto;
    } else if (c == AJ_LIMITE) {
        g.ajustes_ed.limitar_cuadros = !g.ajustes_ed.limitar_cuadros;
    } else if (c == AJ_FPS_MENOS || c == AJ_FPS_MAS) {
        int i = frecuencia_indice(&g.ajustes_ed);
        i += (c == AJ_FPS_MAS) ? 1 : -1;
        if (i < 0) i = AJ_NUM_FRECUENCIAS - 1;
        if (i >= AJ_NUM_FRECUENCIAS) i = 0;
        g.ajustes_ed.fps = FRECUENCIAS[i];
    } else if (c == AJ_VEL_MENOS || c == AJ_VEL_MAS) {
        g.ajustes_ed.velocidad += (c == AJ_VEL_MAS) ? 1 : -1;
        if (g.ajustes_ed.velocidad < 1) g.ajustes_ed.velocidad = 10;
        if (g.ajustes_ed.velocidad > 10) g.ajustes_ed.velocidad = 1;
    } else if (c >= AJ_FONDO_0 && c <= AJ_FONDO_5) {
        g.ajustes_ed.fondo = FONDOS[c - AJ_FONDO_0];
    }
    if (c != AJ_NOMBRE) g.ajustes_foco = -1;
    app_invalidar();
    return 1;
}

void ajustes_soltar(int x, int y) {
    if (!g.ajustes_visible) return;
    g.ajustes_arrastrando = 0;
    g.ajustes_control = AJ_NINGUNO;
    app_invalidar();
}

void ajustes_mover(int x, int y) {
    int c;
    if (!g.ajustes_visible) return;
    if (g.ajustes_arrastrando) {
        Zona p = ajustes_panel();
        g.ajustes_x = x - g.ajustes_off_x;
        g.ajustes_y = y - g.ajustes_off_y;
        p = ajustes_panel();
        g.ajustes_x = p.x;
        g.ajustes_y = p.y;
        app_invalidar();
        return;
    }
    c = ajustes_control_en(x, y);
    if (c != g.ajustes_hover) {
        g.ajustes_hover = c;
        app_invalidar();
    }
}

int ajustes_tecla(int vk, int ctrl) {
    int n;
    if (!g.ajustes_visible) return 0;
    if (vk == VK_ESCAPE) {
        ajustes_proyecto_cerrar(0);
        return 1;
    }
    if (vk == VK_RETURN) {
        ajustes_proyecto_cerrar(1);
        return 1;
    }
    if (g.ajustes_foco != 0) return 1;
    n = (int)strlen(g.ajustes_ed.nombre);
    if (g.ajustes_cursor < 0) g.ajustes_cursor = 0;
    if (g.ajustes_cursor > n) g.ajustes_cursor = n;
    switch (vk) {
    case VK_LEFT:
        if (g.ajustes_cursor > 0) g.ajustes_cursor--;
        break;
    case VK_RIGHT:
        if (g.ajustes_cursor < n) g.ajustes_cursor++;
        break;
    case VK_HOME:
        g.ajustes_cursor = 0;
        break;
    case VK_END:
        g.ajustes_cursor = n;
        break;
    case VK_BACK:
        if (g.ajustes_cursor > 0) {
            memmove(g.ajustes_ed.nombre + g.ajustes_cursor - 1,
                    g.ajustes_ed.nombre + g.ajustes_cursor,
                    (size_t)(n - g.ajustes_cursor + 1));
            g.ajustes_cursor--;
        }
        break;
    case VK_DELETE:
        if (g.ajustes_cursor < n) {
            memmove(g.ajustes_ed.nombre + g.ajustes_cursor,
                    g.ajustes_ed.nombre + g.ajustes_cursor + 1,
                    (size_t)(n - g.ajustes_cursor));
        }
        break;
    default:
        break;
    }
    app_invalidar();
    return 1;
}

int ajustes_caracter(int ch) {
    int n;
    if (!g.ajustes_visible) return 0;
    if (g.ajustes_foco != 0) return 1;
    if (ch < 32 || ch > 126 || ch == '"') return 1;
    n = (int)strlen(g.ajustes_ed.nombre);
    if (n >= AJUSTES_NOMBRE_MAX - 1) return 1;
    if (g.ajustes_cursor < 0) g.ajustes_cursor = 0;
    if (g.ajustes_cursor > n) g.ajustes_cursor = n;
    memmove(g.ajustes_ed.nombre + g.ajustes_cursor + 1,
            g.ajustes_ed.nombre + g.ajustes_cursor,
            (size_t)(n - g.ajustes_cursor + 1));
    g.ajustes_ed.nombre[g.ajustes_cursor] = (char)ch;
    g.ajustes_cursor++;
    app_invalidar();
    return 1;
}

/* ------------------------------------------------------------------ */
/* Serializacion en el .logic                                          */
/* ------------------------------------------------------------------ */
int ajustes_proyecto_linea(char *dst, int tam) {
    return snprintf(dst, (size_t)tam,
                    "PROYECTO \"%s\" %d %d %d %d %d %08X\n",
                    g.ajustes.nombre, g.ajustes.ancho, g.ajustes.alto,
                    g.ajustes.limitar_cuadros, g.ajustes.fps,
                    g.ajustes.velocidad, (unsigned int)g.ajustes.fondo);
}

/* Acepta la linea PROYECTO del .logic. Devuelve 1 si la ha leido. */
int ajustes_proyecto_cargar_linea(const char *linea) {
    const char *q1, *q2, *p;
    AjustesProyecto a;
    unsigned int fondo = 0;
    if (!linea || strncmp(linea, "PROYECTO", 8) != 0) return 0;
    q1 = strchr(linea, '"');
    if (!q1) return 0;
    q2 = strchr(q1 + 1, '"');
    if (!q2) return 0;
    a = g.ajustes;
    a.nombre[0] = 0;
    {
        int n = (int)(q2 - q1 - 1);
        if (n >= AJUSTES_NOMBRE_MAX) n = AJUSTES_NOMBRE_MAX - 1;
        if (n > 0) memcpy(a.nombre, q1 + 1, (size_t)n);
        a.nombre[n] = 0;
    }
    p = q2 + 1;
    if (sscanf(p, "%d %d %d %d %d %X", &a.ancho, &a.alto,
               &a.limitar_cuadros, &a.fps, &a.velocidad, &fondo) != 6)
        return 0;
    a.fondo = (u32)fondo;
    ajustes_normalizar(&a);
    g.ajustes = a;
    return 1;
}
