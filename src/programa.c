/* ==========================================================================
 * Constructor de programa (fase 47).
 *
 * La vista del gestor tiene dos capas. La capa Compatibilidad sigue usando
 * `g.modulos` y el proyector LOGIC2. La capa Programa usa piezas grandes con
 * paleta, tarjetas, enlaces e inspector. El estado se guarda en bloques
 * `sistema.pieza`; no se anade sintaxis ni se toca el CRC de LOGIC3.
 * ========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "logic3.h"
#include "sistema.h"

typedef struct {
    const char *tipo;
    const char *nombre;
    int grupo;
    unsigned int color;
    const char *campo_nombre[2];
    const char *valor[2];
    const char *puerto[2]; /* 0 = entrada, 1 = salida */
} DefPieza;

typedef struct {
    int num;
    int siguiente_id;
    PiezaPrograma piezas[PROGRAMA_PIEZAS_MAX];
} CopiaPrograma;

static const char *GRUPOS[PROGRAMA_GRUPOS] = {
    "Escenario", "Actores", "Control", "Eventos",
    "Acciones", "Funciones", "Recursos"
};

/* Las cadenas visibles llevan acentos; los comentarios y los tipos no. */
static const DefPieza DEFS[] = {
    { "sistema.escena", "Escenario", 0, 0x004F86C8u,
      { "Mapa", "Fondo" }, { "mapa.mav", "0" }, { "-", "actor" } },
    { "sistema.camara", "Cámara", 0, 0x006C9ADBu,
      { "Zoom", "Borde" }, { "1", "0" }, { "escena", "actor" } },
    { "sistema.transicion", "Transición", 0, 0x0086B0E8u,
      { "Tiempo", "Efecto" }, { "30", "fundido" }, { "escena", "escena" } },

    { "sistema.actor", "Actor", 1, 0x00E07A3Fu,
      { "Nombre", "Traje" }, { "Actor", "lobo" }, { "escena", "control" } },
    { "sistema.traje", "Traje", 1, 0x00EC9B55u,
      { "Imagen", "Capa" }, { "lobo.png", "0" }, { "actor", "actor" } },
    { "sistema.clon", "Clon", 1, 0x00F2B06Eu,
      { "Base", "Total" }, { "Actor", "1" }, { "actor", "actor" } },

    { "sistema.teclado", "Teclado", 2, 0x009B4FC0u,
      { "Jugador", "Dispositivo" }, { "1", "teclado" }, { "-", "accion" } },
    { "sistema.raton", "Ratón", 2, 0x00AC5ED1u,
      { "Jugador", "Dispositivo" }, { "1", "raton" }, { "-", "accion" } },
    { "sistema.mando", "Mando", 2, 0x00BD6DE2u,
      { "Jugador", "Dispositivo" }, { "1", "mando" }, { "-", "accion" } },
    { "sistema.jugador1", "Jugador 1", 2, 0x00CE7DF3u,
      { "Jugador", "Dispositivo" }, { "1", "teclado" }, { "actor", "accion" } },
    { "sistema.jugador2", "Jugador 2", 2, 0x00D68DF5u,
      { "Jugador", "Dispositivo" }, { "2", "teclado" }, { "actor", "accion" } },
    { "sistema.jugador3", "Jugador 3", 2, 0x00DE9DF7u,
      { "Jugador", "Dispositivo" }, { "3", "mando" }, { "actor", "accion" } },
    { "sistema.jugador4", "Jugador 4", 2, 0x00E6ADF9u,
      { "Jugador", "Dispositivo" }, { "4", "mando" }, { "actor", "accion" } },

    { "sistema.evento.inicio", "Inicio", 3, 0x00D94F6Au,
      { "Orden", "Escena" }, { "1", "1" }, { "-", "accion" } },
    { "sistema.evento.tecla", "Tecla", 3, 0x00E05F79u,
      { "Tecla", "Flanco" }, { "espacio", "pulsar" }, { "-", "accion" } },
    { "sistema.evento.tocar", "Tocar", 3, 0x00E76F88u,
      { "Objeto", "Flanco" }, { "Actor", "pulsar" }, { "actor", "accion" } },
    { "sistema.evento.cada", "Cada", 3, 0x00EE7F97u,
      { "Tiempo", "Unidad" }, { "30", "fotogramas" }, { "-", "accion" } },
    { "sistema.evento.recibir", "Recibir", 3, 0x00F58FA6u,
      { "Mensaje", "Desde" }, { "hola", "Actor" }, { "actor", "accion" } },
    { "sistema.evento.escena", "Escena", 3, 0x00FA9FB5u,
      { "Escena", "Fase" }, { "1", "entrar" }, { "escena", "accion" } },

    { "sistema.accion.movimiento", "Movimiento", 4, 0x002E9E7Au,
      { "Entrada", "Flanco" }, { "derecha", "pulsar" }, { "control", "funcion" } },
    { "sistema.accion.apariencia", "Apariencia", 4, 0x0038AE88u,
      { "Traje", "Valor" }, { "lobo", "1" }, { "control", "funcion" } },
    { "sistema.accion.mensaje", "Mensaje", 4, 0x0042BE96u,
      { "Texto", "Destino" }, { "hola", "todos" }, { "control", "funcion" } },
    { "sistema.accion.audio", "Audio", 4, 0x004CCEA4u,
      { "Sonido", "Volumen" }, { "beep.wav", "128" }, { "control", "funcion" } },
    { "sistema.accion.hud", "HUD", 4, 0x0056DEB2u,
      { "Texto", "X,Y" }, { "HUD", "4,4" }, { "control", "funcion" } },
    { "sistema.accion.datos", "Datos", 4, 0x0060EEC0u,
      { "Variable", "Valor" }, { "vida", "1" }, { "control", "funcion" } },

    { "sistema.funcion", "Función", 5, 0x006A5ACBu,
      { "Nombre", "Parámetros" }, { "MiFuncion", "0" }, { "accion", "retorno" } },
    { "sistema.parametro", "Parámetro", 5, 0x007A6ADBu,
      { "Nombre", "Tipo" }, { "arg", "entero" }, { "funcion", "funcion" } },
    { "sistema.retorno", "Retorno", 5, 0x008A7AEBu,
      { "Valor", "Tipo" }, { "0", "entero" }, { "funcion", "-" } },

    { "sistema.recurso.mapa", "Mapa", 6, 0x00808090u,
      { "Archivo", "Capa" }, { "mapa.mav", "0" }, { "-", "escena" } },
    { "sistema.recurso.imagen", "Imagen", 6, 0x009090A0u,
      { "Archivo", "Capa" }, { "imagen.png", "0" }, { "-", "actor" } },
    { "sistema.recurso.sonido", "Sonido", 6, 0x00A0A0B0u,
      { "Archivo", "Canal" }, { "sonido.wav", "1" }, { "-", "accion" } }
};

#define PROGRAMA_DEFS ((int)(sizeof(DEFS) / sizeof(DEFS[0])))

static CopiaPrograma historial[PROGRAMA_HISTORIAL];
static CopiaPrograma futuro[PROGRAMA_HISTORIAL];
static int num_historial = 0;
static int num_futuro = 0;
static int movimiento_capturado = 0;
static int movimiento_hubo = 0;

static void programa_validar(void);

static void copiar_local(char *dst, int tam, const char *src) {
    int i;
    if (!dst || tam <= 0) return;
    for (i = 0; src && src[i] && i < tam - 1; i++) dst[i] = src[i];
    for (; i < tam; i++) dst[i] = 0;
}

static void entero_local(char *dst, int tam, int valor) {
    char buf[32];
    formatear(buf, sizeof(buf), "%d", valor);
    copiar_local(dst, tam, buf);
}

static const char *campo_valor(const Bloque3 *b, const char *clave) {
    int i;
    if (!b) return "";
    for (i = 0; i < b->num_campos; i++)
        if (strcmp(b->campos[i].clave, clave) == 0)
            return b->campos[i].valor;
    return "";
}

static void poner_campo(Bloque3 *b, const char *clave, const char *valor) {
    int i;
    if (!b) return;
    for (i = 0; i < b->num_campos; i++) {
        if (strcmp(b->campos[i].clave, clave) == 0) {
            copiar_local(b->campos[i].valor, L3_TEXTO_MAX, valor);
            return;
        }
    }
    if (b->num_campos >= L3_CAMPOS_MAX) return;
    copiar_local(b->campos[b->num_campos].clave, L3_NOMBRE_MAX, clave);
    copiar_local(b->campos[b->num_campos].valor, L3_TEXTO_MAX, valor);
    b->num_campos++;
}

static Bloque3 *bloque_tipo(Proyecto3 *p, const char *tipo) {
    int i;
    if (!p) return NULL;
    for (i = 0; i < p->num_bloques; i++)
        if (strcmp(p->bloques[i].tipo, tipo) == 0) return &p->bloques[i];
    return NULL;
}

int programa_defs_num(void) { return PROGRAMA_DEFS; }

const char *programa_def_tipo(int def) {
    if (def < 0 || def >= PROGRAMA_DEFS) return "";
    return DEFS[def].tipo;
}

const char *programa_def_nombre(int def) {
    if (def < 0 || def >= PROGRAMA_DEFS) return "";
    return DEFS[def].nombre;
}

const char *programa_def_grupo_nombre(int grupo) {
    if (grupo < 0 || grupo >= PROGRAMA_GRUPOS) return "";
    return GRUPOS[grupo];
}

unsigned int programa_def_color(int def) {
    if (def < 0 || def >= PROGRAMA_DEFS) return 0x00505050u;
    return DEFS[def].color;
}

const char *programa_def_campo_nombre(int def, int campo) {
    if (def < 0 || def >= PROGRAMA_DEFS || campo < 0 || campo > 1) return "";
    return DEFS[def].campo_nombre[campo];
}

const char *programa_def_valor(int def, int campo) {
    if (def < 0 || def >= PROGRAMA_DEFS || campo < 0 || campo > 1) return "";
    return DEFS[def].valor[campo];
}

const char *programa_def_puerto(int def, int lado) {
    if (def < 0 || def >= PROGRAMA_DEFS || lado < 0 || lado > 1) return "";
    return DEFS[def].puerto[lado];
}

int programa_modo_actual(void) { return g.programa_modo ? 1 : 0; }

int programa_historial_num(void) { return num_historial; }
int programa_futuro_num(void) { return num_futuro; }

static void guardar_copia(CopiaPrograma *c) {
    c->num = g.num_programa_piezas;
    c->siguiente_id = g.programa_siguiente_id;
    memcpy(c->piezas, g.programa_piezas,
           sizeof(PiezaPrograma) * (size_t)g.num_programa_piezas);
}

static void restaurar_copia(const CopiaPrograma *c) {
    g.num_programa_piezas = c->num;
    g.programa_siguiente_id = c->siguiente_id;
    memcpy(g.programa_piezas, c->piezas,
           sizeof(PiezaPrograma) * (size_t)c->num);
    if (g.programa_sel >= g.num_programa_piezas)
        g.programa_sel = g.num_programa_piezas - 1;
    programa_validar();
}

static void programa_capturar(void) {
    if (num_historial >= PROGRAMA_HISTORIAL) {
        memmove(&historial[0], &historial[1],
                sizeof(historial[0]) * (size_t)(PROGRAMA_HISTORIAL - 1));
        num_historial = PROGRAMA_HISTORIAL - 1;
    }
    guardar_copia(&historial[num_historial++]);
    num_futuro = 0;
}

static void programa_descartar_captura(void) {
    if (num_historial > 0) num_historial--;
    num_futuro = 0;
}

static void programa_deshacer(void) {
    if (num_historial <= 0) {
        app_estado("programa: no hay cambios que deshacer");
        return;
    }
    if (num_futuro >= PROGRAMA_HISTORIAL) {
        memmove(&futuro[0], &futuro[1],
                sizeof(futuro[0]) * (size_t)(PROGRAMA_HISTORIAL - 1));
        num_futuro = PROGRAMA_HISTORIAL - 1;
    }
    guardar_copia(&futuro[num_futuro++]);
    restaurar_copia(&historial[--num_historial]);
    app_estado("programa: deshecho");
}

static void programa_rehacer(void) {
    if (num_futuro <= 0) {
        app_estado("programa: no hay cambios que rehacer");
        return;
    }
    if (num_historial >= PROGRAMA_HISTORIAL) {
        memmove(&historial[0], &historial[1],
                sizeof(historial[0]) * (size_t)(PROGRAMA_HISTORIAL - 1));
        num_historial = PROGRAMA_HISTORIAL - 1;
    }
    guardar_copia(&historial[num_historial++]);
    restaurar_copia(&futuro[--num_futuro]);
    app_estado("programa: rehecho");
}

static void programa_reiniciar(void) {
    memset(g.programa_piezas, 0, sizeof(g.programa_piezas));
    g.num_programa_piezas = 0;
    g.programa_sel = -1;
    g.programa_siguiente_id = 1;
    memset(g.programa_grupo, 0, sizeof(g.programa_grupo));
    g.programa_paleta_sel = -1;
    g.programa_paleta_pulsada = -1;
    g.programa_scroll = 0;
    g.programa_arrastrando = 0;
    g.programa_drag_i = -1;
    g.programa_enlace_desde = -1;
    g.programa_snap_x = -1;
    g.programa_snap_y = -1;
    num_historial = 0;
    num_futuro = 0;
    movimiento_capturado = 0;
    movimiento_hubo = 0;
}

/* Una pieza con puerto de salida necesita un destino. Es la validacion en
 * vivo minima: el inspector y la insignia roja lo reflejan al instante. */
static void programa_validar(void) {
    int i;
    for (i = 0; i < g.num_programa_piezas; i++) {
        PiezaPrograma *p = &g.programa_piezas[i];
        int salida = 0;
        if (p->def >= 0 && p->def < PROGRAMA_DEFS)
            salida = strcmp(DEFS[p->def].puerto[1], "-") != 0;
        p->valido = !salida || p->enlace > 0;
    }
}

void programa_cargar_modelo(void) {
    Proyecto3 *p = logic3_modelo_activo();
    int i;

    programa_reiniciar();
    if (!p) {
        g.programa_modo = 1;
        return;
    }
    for (i = 0; i < p->num_bloques; i++) {
        const Bloque3 *b = &p->bloques[i];
        PiezaPrograma *pieza;
        const char *tipo;
        int id, xy[2] = { 0, 0 };
        int def = -1, d;

        if (strcmp(b->tipo, "sistema.pieza") != 0) continue;
        if (g.num_programa_piezas >= PROGRAMA_PIEZAS_MAX) break;
        tipo = campo_valor(b, "pieza");
        for (d = 0; d < PROGRAMA_DEFS; d++)
            if (strcmp(DEFS[d].tipo, tipo) == 0) { def = d; break; }
        if (def < 0) continue;
        id = atoi(campo_valor(b, "id"));
        if (id <= 0) continue;
        (void)sscanf(campo_valor(b, "xy"), "%d,%d", &xy[0], &xy[1]);
        pieza = &g.programa_piezas[g.num_programa_piezas++];
        memset(pieza, 0, sizeof(*pieza));
        pieza->id = id;
        pieza->def = def;
        pieza->x = xy[0];
        pieza->y = xy[1];
        pieza->ancho = PROGRAMA_TARJETA_ANCHO;
        pieza->alto = PROGRAMA_TARJETA_ALTO;
        pieza->enlace = atoi(campo_valor(b, "enlace"));
        copiar_local(pieza->nombre, sizeof(pieza->nombre),
                     campo_valor(b, "nombre"));
        copiar_local(pieza->valor, sizeof(pieza->valor),
                     campo_valor(b, "valor"));
        copiar_local(pieza->valor2, sizeof(pieza->valor2),
                     campo_valor(b, "valor2"));
        if (!pieza->nombre[0])
            copiar_local(pieza->nombre, sizeof(pieza->nombre),
                         DEFS[def].nombre);
        if (pieza->id >= g.programa_siguiente_id)
            g.programa_siguiente_id = pieza->id + 1;
    }
    programa_validar();
    /* El gestor abre siempre en Programa para que la paleta sea visible desde
     * el primer momento. El campo guardado sistema.opciones.programa decide
     * como ejecuta el runtime; no es el interruptor de la vista del editor. */
    g.programa_modo = 1;
}

void programa_guardar_modelo(void *modelo) {
    Proyecto3 *p = (Proyecto3 *)modelo;
    Bloque3 *op;
    int i, j, orden = 0;
    int usar_programa = g.num_programa_piezas > 0;
    char buf[64];

    if (!p) return;
    /* Entrar al gestor no convierte un proyecto legacy en proyecto programa:
     * solo se marca como programa cuando el lienzo tiene piezas guardadas. */
    if (usar_programa && !bloque_tipo(p, SISTEMA_TIPO_OPCIONES))
        sistema_opciones_asegurar(p);
    op = bloque_tipo(p, SISTEMA_TIPO_OPCIONES);
    if (op && usar_programa)
        poner_campo(op, "programa", "1");

    for (i = p->num_bloques - 1; i >= 0; i--) {
        if (strcmp(p->bloques[i].tipo, "sistema.pieza") != 0) continue;
        if (i < p->num_bloques - 1)
            memmove(&p->bloques[i], &p->bloques[i + 1],
                    sizeof(Bloque3) * (size_t)(p->num_bloques - i - 1));
        p->num_bloques--;
    }

    for (i = 0; i < g.num_programa_piezas; i++) {
        const PiezaPrograma *pieza = &g.programa_piezas[i];
        Bloque3 *b = NULL;
        if (!logic3_bloque_nuevo(p, 1, 0, orden++, "sistema.pieza", &b)) break;
        if (!b) break;
        poner_campo(b, "ui", "1");
        poner_campo(b, "pieza", programa_def_tipo(pieza->def));
        entero_local(buf, sizeof(buf), pieza->id);
        poner_campo(b, "id", buf);
        formatear(buf, sizeof(buf), "%d,%d", pieza->x, pieza->y);
        poner_campo(b, "xy", buf);
        poner_campo(b, "nombre", pieza->nombre);
        entero_local(buf, sizeof(buf), pieza->enlace);
        poner_campo(b, "enlace", buf);
        poner_campo(b, "valor", pieza->valor);
        poner_campo(b, "valor2", pieza->valor2);
        for (j = 0; j < b->num_ranuras; j++) b->ranuras[j].bloque = 0;
        b->num_ranuras = 0;
    }
}

void programa_volcar(const char *ruta) {
    FILE *f;
    int i;
    if (!ruta || !ruta[0]) return;
    f = fopen(ruta, "w");
    if (!f) return;
    fprintf(f,
            "programa_modo=%d programa_piezas=%d programa_sel=%d "
            "programa_paleta=%d programa_grupos=%d%d%d%d%d%d%d "
            "programa_undo=%d programa_redo=%d\n",
            g.programa_modo ? 1 : 0, g.num_programa_piezas,
            g.programa_sel, g.programa_paleta_sel,
            g.programa_grupo[0], g.programa_grupo[1], g.programa_grupo[2],
            g.programa_grupo[3], g.programa_grupo[4], g.programa_grupo[5],
            g.programa_grupo[6], num_historial, num_futuro);
    for (i = 0; i < g.num_programa_piezas; i++) {
        const PiezaPrograma *p = &g.programa_piezas[i];
        char valor[PROGRAMA_VALOR_MAX];
        char valor2[PROGRAMA_VALOR_MAX];
        int n;
        copiar_local(valor, sizeof(valor), p->valor);
        copiar_local(valor2, sizeof(valor2), p->valor2);
        for (n = 0; valor[n]; n++) if (valor[n] == ' ') valor[n] = '_';
        for (n = 0; valor2[n]; n++) if (valor2[n] == ' ') valor2[n] = '_';
        fprintf(f,
                "programa_pieza=%d:%s:%d,%d:%d:%s:%s\n",
                p->id, programa_def_tipo(p->def), p->x, p->y,
                p->enlace, valor, valor2);
    }
    fclose(f);
}

static int def_indice(const char *tipo) {
    int d;
    for (d = 0; d < PROGRAMA_DEFS; d++)
        if (strcmp(DEFS[d].tipo, tipo) == 0) return d;
    return -1;
}

/* Prepara la vista al entrar al gestor. Sin modelo cargado los contadores
 * arrancan a cero, asi que hay que dejar la seleccion en -1 para que no
 * aparezca resaltada la primera pieza de la paleta. */
void programa_vista_preparar(int modo) {
    g.programa_modo = modo ? 1 : 0;
    if (g.num_programa_piezas <= 0) {
        g.programa_sel = -1;
        g.programa_paleta_sel = -1;
        g.programa_paleta_pulsada = -1;
    }
}

void programa_modo_poner(int modo) {
    modo = modo ? 1 : 0;
    if (modo == g.programa_modo) return;
    /* Solo cambia la vista del gestor. El proyecto no se toca: el modo de
     * ejecucion se escribe al guardar, segun las piezas que tenga el lienzo. */
    programa_vista_preparar(modo);
    g.programa_arrastrando = 0;
    g.programa_drag_i = -1;
    g.programa_enlace_desde = -1;
    g.programa_paleta_pulsada = -1;
    app_estado(modo ? "programa: arrastra una pieza de la paleta al lienzo"
                    : "compatibilidad: vista LOGIC2 del gestor");
    app_invalidar();
}

/* ------------------------------------------------------------------ */
/* Geometria                                                           */
/* ------------------------------------------------------------------ */
Zona programa_paleta_zona(void) {
    int panel = sidebar_derecho_ancho();
    int y0 = AREA_SUP_ALTO + 30;
    int alto = g.alto - y0 - 36;
    if (alto < PROGRAMA_FILA_ALTO) alto = PROGRAMA_FILA_ALTO;
    return zona_hacer(SIDEBAR_MARGEN, y0,
                      panel - SIDEBAR_MARGEN * 2, alto);
}

Zona programa_inspector_zona(void) {
    int panel = sidebar_derecho_ancho() + MODULOS_PANEL_DER_EXTRA;
    int x0 = g.ancho - panel;
    /* El titulo INSPECTOR se dibuja 14 px por encima de la zona. Con 8 px de
     * margen caia dentro de la barra de menus, que se pinta despues y lo
     * ocultaba. El mismo margen que la paleta deja el titulo visible. */
    int y0 = AREA_SUP_ALTO + 30;
    int alto = g.alto - y0 - BARRA_ABAJO_ALTO - 8;
    if (alto < 40) alto = 40;
    return zona_hacer(x0 + SIDEBAR_MARGEN, y0,
                      panel - SIDEBAR_MARGEN * 2, alto);
}

Zona programa_boton_zona(int cual) {
    int panel = sidebar_derecho_ancho();
    int ancho = g.ancho - panel - (panel + MODULOS_PANEL_DER_EXTRA);
    int x, y;
    if (ancho < MIN_LADO) ancho = MIN_LADO;
    x = panel + 8;
    y = g.alto - BARRA_ABAJO_ALTO + 6;
    if (cual == 0) return zona_hacer(x, y, 78, 20);
    if (cual == 1) return zona_hacer(x + 84, y, 104, 20);
    if (cual == 2) return zona_hacer(x + 194, y, 30, 20);
    if (cual == 3) return zona_hacer(x + 228, y, 30, 20);
    return zona_hacer(x + 264, y, 94, 20);
}

Zona programa_fila_zona(int fila) {
    Zona z = programa_paleta_zona();
    return zona_hacer(z.x, z.y + (fila - g.programa_scroll) * PROGRAMA_FILA_ALTO,
                      z.ancho, PROGRAMA_FILA_ALTO);
}

Zona programa_campo_zona(int campo) {
    Zona z = programa_inspector_zona();
    return zona_hacer(z.x + 10, z.y + 96 + campo * 46,
                      z.ancho - 20, 36);
}

Zona programa_scripts_zona(void) {
    Zona z = programa_inspector_zona();
    return zona_hacer(z.x + 10, zona_abajo(z) - 34, z.ancho - 20, 24);
}

Zona programa_tarjeta_zona(int indice) {
    Zona lienzo = modulos_lienzo_zona();
    const PiezaPrograma *p;
    if (indice < 0 || indice >= g.num_programa_piezas)
        return zona_hacer(0, 0, 0, 0);
    p = &g.programa_piezas[indice];
    return zona_hacer(lienzo.x + g.pan_x + p->x,
                      lienzo.y + g.pan_y + p->y, p->ancho, p->alto);
}

int programa_fila_total(void) {
    int total = 0, gidx, d;
    for (gidx = 0; gidx < PROGRAMA_GRUPOS; gidx++) {
        total++;
        if (g.programa_grupo[gidx]) continue;
        for (d = 0; d < PROGRAMA_DEFS; d++)
            if (DEFS[d].grupo == gidx) total++;
    }
    return total;
}

int programa_fila_info(int fila, int *grupo, int *def) {
    int actual = 0, gidx, d;
    if (grupo) *grupo = -1;
    if (def) *def = -1;
    if (fila < 0) return 0;
    for (gidx = 0; gidx < PROGRAMA_GRUPOS; gidx++) {
        if (actual == fila) {
            if (grupo) *grupo = gidx;
            return 1;
        }
        actual++;
        if (g.programa_grupo[gidx]) continue;
        for (d = 0; d < PROGRAMA_DEFS; d++) {
            if (DEFS[d].grupo != gidx) continue;
            if (actual == fila) {
                if (grupo) *grupo = gidx;
                if (def) *def = d;
                return 1;
            }
            actual++;
        }
    }
    return 0;
}

int programa_fila_en(int x, int y) {
    Zona z = programa_paleta_zona();
    int fila;
    if (!zona_contiene(z, x, y)) return -1;
    fila = (y - z.y) / PROGRAMA_FILA_ALTO + g.programa_scroll;
    if (fila < 0 || fila >= programa_fila_total()) return -1;
    return fila;
}

int programa_pieza_en(int x, int y) {
    int i;
    for (i = g.num_programa_piezas - 1; i >= 0; i--) {
        Zona z = programa_tarjeta_zona(i);
        if (zona_contiene(z, x, y)) return i;
    }
    return -1;
}

int programa_puerto_en(int x, int y, int *pieza, int *lado) {
    int i;
    for (i = g.num_programa_piezas - 1; i >= 0; i--) {
        Zona z = programa_tarjeta_zona(i);
        int cy = z.y + z.alto / 2;
        int dxr = x - zona_derecha(z);
        int dxl = x - z.x;
        int dy = y - cy;
        if (dxr * dxr + dy * dy <= 81) {
            if (pieza) *pieza = i;
            if (lado) *lado = 1;
            return 1;
        }
        if (dxl * dxl + dy * dy <= 81) {
            if (pieza) *pieza = i;
            if (lado) *lado = 0;
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Mutaciones                                                          */
/* ------------------------------------------------------------------ */
static int programa_insertar(int def, int x, int y) {
    PiezaPrograma *p;
    Zona lienzo = modulos_lienzo_zona();
    int wx, wy;
    int idx;

    if (def < 0 || def >= PROGRAMA_DEFS) return -1;
    if (g.num_programa_piezas >= PROGRAMA_PIEZAS_MAX) {
        app_estado("programa: lienzo lleno");
        return -1;
    }
    if (!zona_contiene(lienzo, x, y)) return -1;
    wx = x - lienzo.x - g.pan_x - PROGRAMA_TARJETA_ANCHO / 2;
    wy = y - lienzo.y - g.pan_y - PROGRAMA_TARJETA_ALTO / 2;
    modulos_acoplar(&wx, &wy, PROGRAMA_TARJETA_ANCHO, PROGRAMA_TARJETA_ALTO,
                    -1, &g.programa_snap_x, &g.programa_snap_y);
    programa_capturar();
    idx = g.num_programa_piezas++;
    p = &g.programa_piezas[idx];
    memset(p, 0, sizeof(*p));
    p->id = g.programa_siguiente_id++;
    p->def = def;
    p->x = wx;
    p->y = wy;
    p->ancho = PROGRAMA_TARJETA_ANCHO;
    p->alto = PROGRAMA_TARJETA_ALTO;
    copiar_local(p->nombre, sizeof(p->nombre), DEFS[def].nombre);
    copiar_local(p->valor, sizeof(p->valor), DEFS[def].valor[0]);
    copiar_local(p->valor2, sizeof(p->valor2), DEFS[def].valor[1]);
    programa_validar();
    g.programa_sel = idx;
    app_estado("programa: añadida %s", DEFS[def].nombre);
    app_invalidar();
    return idx;
}

static void programa_borrar(int idx) {
    int id, i;
    if (idx < 0 || idx >= g.num_programa_piezas) return;
    programa_capturar();
    id = g.programa_piezas[idx].id;
    for (i = idx; i < g.num_programa_piezas - 1; i++)
        g.programa_piezas[i] = g.programa_piezas[i + 1];
    g.num_programa_piezas--;
    for (i = 0; i < g.num_programa_piezas; i++)
        if (g.programa_piezas[i].enlace == id)
            g.programa_piezas[i].enlace = 0;
    programa_validar();
    g.programa_sel = -1;
    app_estado("programa: pieza borrada");
    app_invalidar();
}

static void programa_enlazar(int desde, int hacia) {
    if (desde < 0 || hacia < 0 || desde == hacia) return;
    if (desde >= g.num_programa_piezas || hacia >= g.num_programa_piezas) return;
    programa_capturar();
    g.programa_piezas[desde].enlace = g.programa_piezas[hacia].id;
    programa_validar();
    g.programa_sel = desde;
    app_estado("programa: pieza %d enlazada a %d",
               g.programa_piezas[desde].id, g.programa_piezas[hacia].id);
    app_invalidar();
}

static void programa_ciclar(int idx, int campo) {
    PiezaPrograma *p;
    const char *opciones[4];
    char *destino;
    int n = 0, actual = 0, i;

    if (idx < 0 || idx >= g.num_programa_piezas || campo < 0 || campo > 1)
        return;
    p = &g.programa_piezas[idx];
    if (p->def < 0 || p->def >= PROGRAMA_DEFS) return;

    if (DEFS[p->def].grupo == 2 && campo == 0) {
        opciones[n++] = "1";
        opciones[n++] = "2";
        opciones[n++] = "3";
        opciones[n++] = "4";
    } else if (DEFS[p->def].grupo == 2 && campo == 1) {
        opciones[n++] = "teclado";
        opciones[n++] = "raton";
        opciones[n++] = "mando";
    } else if (DEFS[p->def].grupo == 4 && campo == 0) {
        opciones[n++] = "derecha";
        opciones[n++] = "izquierda";
        opciones[n++] = "arriba";
        opciones[n++] = "abajo";
    } else if ((DEFS[p->def].grupo == 4 && campo == 1) ||
               p->def == def_indice("sistema.evento.tecla")) {
        opciones[n++] = "mantener";
        opciones[n++] = "pulsar";
        opciones[n++] = "soltar";
    } else {
        app_estado("programa: campo de solo lectura");
        return;
    }
    destino = campo == 0 ? p->valor : p->valor2;
    for (i = 0; i < n; i++)
        if (strcmp(destino, opciones[i]) == 0) actual = i + 1;
    programa_capturar();
    copiar_local(destino, sizeof(p->valor), opciones[actual % n]);
    app_estado("programa: %s = %s", DEFS[p->def].campo_nombre[campo],
               opciones[actual % n]);
    app_invalidar();
}

/* ------------------------------------------------------------------ */
/* Interaccion                                                         */
/* ------------------------------------------------------------------ */
int programa_pulsar(int x, int y, int boton) {
    Zona lienzo = modulos_lienzo_zona();
    int cual;
    int fila, grupo = -1, def = -1;
    int pieza = -1, lado = -1;
    int idx;

    for (cual = 0; cual < 5; cual++) {
        if (!zona_contiene(programa_boton_zona(cual), x, y)) continue;
        if (boton != 0) return 1;
        if (cual == 0) programa_modo_poner(1);
        else if (cual == 1) programa_modo_poner(0);
        else if (cual == 2) programa_deshacer();
        else if (cual == 3) programa_rehacer();
        else {
            if (g.vista != VISTA_ESTUDIO) app_alternar_estudio();
            g.estudio_pestana = BLOQUES_TAB;
            app_estado("programa: bloques del actor o función seleccionados");
        }
        app_invalidar();
        return 1;
    }

    if (!g.programa_modo) return 0;

    if (zona_contiene(programa_scripts_zona(), x, y)) {
        if (boton == 0) {
            if (g.vista != VISTA_ESTUDIO) app_alternar_estudio();
            g.estudio_pestana = BLOQUES_TAB;
            app_estado("programa: bloques del actor o función seleccionados");
            app_invalidar();
        }
        return 1;
    }

    if (boton == 1) {
        idx = programa_pieza_en(x, y);
        if (idx >= 0) programa_borrar(idx);
        return 1;
    }

    if (g.programa_sel >= 0) {
        for (cual = 0; cual < 2; cual++) {
            if (!zona_contiene(programa_campo_zona(cual), x, y)) continue;
            programa_ciclar(g.programa_sel, cual);
            return 1;
        }
    }

    fila = programa_fila_en(x, y);
    if (fila >= 0) {
        if (!programa_fila_info(fila, &grupo, &def)) return 1;
        if (def < 0) {
            g.programa_grupo[grupo] = !g.programa_grupo[grupo];
            if (g.programa_scroll > programa_fila_total() - 1)
                g.programa_scroll = programa_fila_total() - 1;
            if (g.programa_scroll < 0) g.programa_scroll = 0;
            app_invalidar();
            return 1;
        }
        g.programa_paleta_sel = def;
        g.programa_paleta_pulsada = def;
        g.programa_paleta_x = x;
        g.programa_paleta_y = y;
        g.programa_paleta_movido = 0;
        g.programa_arrastrando = 1;
        app_estado("programa: suelta %s en el lienzo", DEFS[def].nombre);
        app_invalidar();
        return 1;
    }

    if (zona_contiene(programa_inspector_zona(), x, y)) return 1;
    if (!zona_contiene(lienzo, x, y)) return 1;

    if (boton == 0 && programa_puerto_en(x, y, &pieza, &lado) && lado == 1) {
        g.programa_enlace_desde = pieza;
        g.programa_arrastrando = 4;
        app_estado("programa: suelta sobre la pieza destino");
        app_invalidar();
        return 1;
    }

    idx = programa_pieza_en(x, y);
    if (idx >= 0 && boton == 0) {
        Zona z = programa_tarjeta_zona(idx);
        g.programa_sel = idx;
        programa_capturar();
        movimiento_capturado = 1;
        movimiento_hubo = 0;
        g.programa_arrastrando = 3;
        g.programa_drag_i = idx;
        g.programa_off_x = x - z.x;
        g.programa_off_y = y - z.y;
        g.programa_snap_x = -1;
        g.programa_snap_y = -1;
        app_invalidar();
        return 1;
    }

    /* Clic en zona vacia: coloca la pieza elegida en la paleta. Asi tambien
     * se puede componer sin arrastrar. */
    if (boton == 0 && g.programa_paleta_sel >= 0) {
        int def = g.programa_paleta_sel;
        g.programa_paleta_sel = -1;
        programa_insertar(def, x, y);
        return 1;
    }

    if (boton == 0 || boton == 2) {
        g.programa_arrastrando = 2;
        g.programa_drag_i = x - g.pan_x;
        g.programa_off_y = y - g.pan_y;
        app_invalidar();
    }
    return 1;
}

void programa_soltar(int x, int y) {
    if (!g.programa_modo) return;
    if (g.programa_arrastrando == 1) {
        int def = g.programa_paleta_pulsada;
        g.programa_arrastrando = 0;
        g.programa_paleta_pulsada = -1;
        g.programa_paleta_movido = 0;
        g.programa_snap_x = -1;
        g.programa_snap_y = -1;
        if (def >= 0 && zona_contiene(modulos_lienzo_zona(), x, y))
            programa_insertar(def, x, y);
        app_invalidar();
        return;
    }
    if (g.programa_arrastrando == 3) {
        if (movimiento_capturado && !movimiento_hubo)
            programa_descartar_captura();
        movimiento_capturado = 0;
        movimiento_hubo = 0;
        g.programa_arrastrando = 0;
        g.programa_snap_x = -1;
        g.programa_snap_y = -1;
        app_invalidar();
        return;
    }
    if (g.programa_arrastrando == 4) {
        int desde = g.programa_enlace_desde;
        int hacia = programa_pieza_en(x, y);
        g.programa_arrastrando = 0;
        g.programa_enlace_desde = -1;
        if (hacia >= 0 && hacia != desde) programa_enlazar(desde, hacia);
        else app_estado("programa: enlace cancelado");
        app_invalidar();
        return;
    }
    if (g.programa_arrastrando == 2) {
        g.programa_arrastrando = 0;
        app_invalidar();
    }
}

void programa_mover(int x, int y) {
    Zona lienzo;
    int dx, dy;

    if (!g.programa_modo) return;
    if (g.programa_arrastrando == 1) {
        dx = x - g.programa_paleta_x;
        dy = y - g.programa_paleta_y;
        if (!g.programa_paleta_movido && dx * dx + dy * dy >= 16)
            g.programa_paleta_movido = 1;
        app_invalidar();
        return;
    }
    if (g.programa_arrastrando == 2) {
        g.pan_x = x - g.programa_drag_i;
        g.pan_y = y - g.programa_off_y;
        app_invalidar();
        return;
    }
    if (g.programa_arrastrando == 3) {
        PiezaPrograma *p;
        int wx, wy;
        if (g.programa_drag_i < 0 || g.programa_drag_i >= g.num_programa_piezas)
            return;
        p = &g.programa_piezas[g.programa_drag_i];
        lienzo = modulos_lienzo_zona();
        wx = x - lienzo.x - g.pan_x - g.programa_off_x;
        wy = y - lienzo.y - g.pan_y - g.programa_off_y;
        modulos_acoplar(&wx, &wy, p->ancho, p->alto, g.programa_drag_i,
                        &g.programa_snap_x, &g.programa_snap_y);
        if (wx != p->x || wy != p->y) movimiento_hubo = 1;
        p->x = wx;
        p->y = wy;
        app_invalidar();
        return;
    }
    if (g.programa_arrastrando == 4) app_invalidar();
}

void programa_rueda(int x, int y, int delta, int ctrl) {
    Zona paleta = programa_paleta_zona();
    Zona lienzo = modulos_lienzo_zona();
    int maximo;

    (void)ctrl;
    if (!g.programa_modo) return;
    if (zona_contiene(paleta, x, y)) {
        maximo = programa_fila_total() - paleta.alto / PROGRAMA_FILA_ALTO;
        if (maximo < 0) maximo = 0;
        g.programa_scroll += delta > 0 ? -1 : 1;
        if (g.programa_scroll < 0) g.programa_scroll = 0;
        if (g.programa_scroll > maximo) g.programa_scroll = maximo;
        app_invalidar();
        return;
    }
    if (zona_contiene(lienzo, x, y)) {
        g.pan_y += delta > 0 ? 32 : -32;
        app_invalidar();
    }
}

int programa_tecla(int vk, int ctrl, int shift) {
    (void)shift;
    if (!g.programa_modo) return 0;
    if (ctrl && (vk == 'Z' || vk == 'z')) {
        programa_deshacer();
        return 1;
    }
    if (ctrl && (vk == 'Y' || vk == 'y')) {
        programa_rehacer();
        return 1;
    }
    if (vk == VK_DELETE && g.programa_sel >= 0) {
        programa_borrar(g.programa_sel);
        return 1;
    }
    if (vk == VK_ESCAPE && g.programa_arrastrando) {
        g.programa_arrastrando = 0;
        g.programa_paleta_pulsada = -1;
        g.programa_enlace_desde = -1;
        app_invalidar();
        return 1;
    }
    return 0;
}
