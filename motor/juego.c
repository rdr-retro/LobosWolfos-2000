/* juego.c - game.exe, el juego montado sobre el motor egine (fase 3).
 *
 * Abre una ventana, carga el proyecto (un .logic del gestor de logica, un
 * .logic3 de los juegos complejos o directamente un .mav con sus tiles
 * PNG), dibuja el mapa con el nucleo en ensamblador
 * (motor_mapa_render) y mueve un sprite con el teclado colisionando
 * contra las celdas del mapa (motor_aabb).
 *
 * Flags de prueba (mismo estilo que el editor):
 *   --logica ruta     proyecto .logic del gestor (busca el mapa en maps/)
 *   --logic3 ruta     proyecto LOGIC3: scripts codigo.lobo con la VM
 *                     resumible de la fase 25 (esperar no bloquea)
 *   --mapa ruta       mapa .mav directo (gana al proyecto)
 *   --captura ruta    corre N fotogramas sin ventana y guarda BMP; sale
 *   --fotogramas N    fotogramas de la captura (60 por defecto)
 *   --tecla nombre    tecla mantenida durante la captura
 *                     (izquierda/derecha/arriba/abajo/saltar)
 *   --traza ruta      escribe en el archivo una linea por fotograma con
 *                     la posicion del lobo (y una cabecera lobo: N);
 *                     en modo --logic3 anade una linea "vm ..." por
 *                     fotograma con el estado de cada script
 *   --vm-test ruta    compila un .lobo en modo VM y volca su estado paso
 *                     a paso (para depurar la maquina sin proyecto)
 *   --hud             muestra vidas, puntuacion y dialogo sobre el mundo
 *   --vidas N         vidas iniciales del HUD (activa el HUD)
 *   --puntos N        puntuacion inicial del HUD (activa el HUD)
 *   --dialogo a|b|c   tres lineas del dialogo (activa el HUD)
 *   --volcar-hud ruta volca el estado final del HUD
 *   --audio           activa sonar(), musica() y volumen() en los scripts
 *   --audio-sonido n=ruta  asocia el nombre de un sonido a un WAV
 *   --volumen N       volumen maestro inicial (0..100, activa el audio)
 *   --volcar-audio ruta  volca el estado final del mezclador
 *   --saves ruta      carpeta de saves (activa guardar/cargar/record)
 *   --volcar-saves ruta  volca el estado final del guardado
 *   --escenas         activa escenas, transiciones, camara y efectos
 *   --volcar-escenas ruta  volca el estado final de escenas y efectos
 *   --perfil          mide instrucciones, microsegundos y blits por script
 *   --presupuesto N   microsegundos por fotograma (16667 = 60 FPS); con
 *                     --perfil avisa y degrada si se pasa
 *   --volcar-perfil ruta  informe final de perfil y memoria del blob
 *   --estres N,M      N entidades y M bloques sinteticos para pruebas
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "motor.h"
#include "lobo.h"
#include "entrada.h"
#include "logic3_motor.h"
#include "entidades.h"
#include "eventos.h"
#include "colisiones.h"
#include "render.h"
#include "depurador.h"
#include "panel_debug.h"
#include "hud.h"
#include "audio.h"
#include "save.h"

#define VENTANA_ANCHO 640
#define VENTANA_ALTO  480
#define FPS_OBJETIVO  60
#define MS_POR_FOTO   (1000 / FPS_OBJETIVO)
#define SPRITE_LADO   24
#define SPRITE_VEL    3

typedef struct Juego {
    MotorMapa mapa;
    char mapa_usado[MAX_PATH];
    u32 *fb;                 /* framebuffer: DIB en ventana, malloc en captura */
    int fb_ancho, fb_alto;
    u32 sprite[SPRITE_LADO * SPRITE_LADO]; /* sprite generado, 24x24 */
    int x, y;                /* posicion del sprite en el mundo */
    unsigned entrada;        /* bits MOTOR_ENT_* */
    unsigned entrada_anterior; /* fotograma previo, para los flancos */
    int fotograma;           /* contador de actualizaciones */
    int corriendo;
} Juego;

static Juego juego;

/* Fase 37: entrada extendida. Solo se expone a los scripts cuando se ha
 * pedido de forma explicita en la linea de comandos, para no cambiar las
 * capturas ni las trazas de las suites anteriores. */
static MotorEntrada g_entrada_ext;
static int g_entrada_extactiva;
static const char *g_ruta_volcar_entrada;
static HWND g_ventana_ext;
static int g_rueda_acum;

/* Fase 38: HUD. Igual que la entrada extendida, no se dibuja ni se volca
 * nada hasta pedir --hud o --volcar-hud, para que las capturas antiguas
 * sigan siendo byte a byte identicas. */
static int g_hud_activo;
static const char *g_ruta_volcar_hud;

/* Fase 39: audio. Igual que el HUD, no se carga ningun WAV ni se volca nada
 * hasta pedir --audio, --audio-sonido, --volumen o --volcar-audio. */
static int g_audio_activo;
static const char *g_ruta_volcar_audio;

/* Fase 40: guardado. Sin --saves no se abre ningun .sav y las salidas de
 * las suites anteriores siguen byte a byte identicas. */
static int g_saves_activo;
static const char *g_ruta_volcar_saves;

/* Ajustes del proyecto aplicados al motor (fase 14). */
static int g_ancho = VENTANA_ANCHO;
static int g_alto = VENTANA_ALTO;
static int g_fps = FPS_OBJETIVO;
static int g_limitar = 1;
static int g_velocidad = SPRITE_VEL;
static u32 g_fondo = 0x00000000u;
/* Fisica de la fase 29. Cero por defecto para no alterar el juego clasico;
 * los scripts la activan escribiendo vel_x, vel_y o gravedad. */
static struct {
    int vel_x, vel_y, gravedad, rebote, friccion, en_suelo;
} g_fisica;

/* Depurador (fase 34). Todas las rutas son NULL hasta que se pasan por
 * linea de comandos; asi las trazas de las fases anteriores no cambian. */
static FILE *g_ordenes = NULL;
static const char *g_ruta_debug = NULL;
static const char *g_grabar_replay = NULL;
static const char *g_comparar_a = NULL;
static const char *g_comparar_b = NULL;
static int g_semilla = 12345;

static void aplicar_ajustes(void) {
    g_ancho = activo_proyecto_ancho();
    g_alto = activo_proyecto_alto();
    g_fps = activo_proyecto_fps();
    g_limitar = activo_proyecto_limitar_cuadros();
    g_velocidad = activo_proyecto_velocidad();
    g_fondo = (u32)activo_proyecto_fondo();
    if (g_ancho < 320 || g_ancho > 4096) g_ancho = VENTANA_ANCHO;
    if (g_alto < 200 || g_alto > 4096) g_alto = VENTANA_ALTO;
    if (g_fps < 1 || g_fps > 1000) g_fps = FPS_OBJETIVO;
    if (g_velocidad < 1 || g_velocidad > 10) g_velocidad = SPRITE_VEL;
}

/* Los ajustes del proyecto LOGIC3 vienen del lector del motor (no de
 * activo.c, que solo entiende LOGIC2). */
static void aplicar_ajustes_l3(const MotorL3 *p) {
    g_ancho = p->ancho;
    g_alto = p->alto;
    g_fps = p->fps;
    g_limitar = p->limitar_cuadros;
    g_velocidad = p->velocidad;
    g_fondo = (u32)p->fondo;
    if (g_ancho < 320 || g_ancho > 4096) g_ancho = VENTANA_ANCHO;
    if (g_alto < 200 || g_alto > 4096) g_alto = VENTANA_ALTO;
    if (g_fps < 1 || g_fps > 1000) g_fps = FPS_OBJETIVO;
    if (g_velocidad < 1 || g_velocidad > 10) g_velocidad = SPRITE_VEL;
}

/* Archivo legible (mismo criterio que activo.c). */
static int archivo_existe(const char *ruta) {
    FILE *f;
    if (!ruta || !ruta[0]) return 0;
    f = fopen(ruta, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* Copia de texto con tope (siempre termina en cero). */
static void copiar_texto(char *dst, int tam, const char *src) {
    int i;
    if (tam <= 0) return;
    for (i = 0; i < tam - 1 && src && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

/* Reparte `--dialogo linea1|linea2|linea3` en las tres lineas del HUD. */
static void fijar_dialogo_cli(const char *valor) {
    char linea[HUD_TEXTO_MAX];
    int n = 0, i = 0;
    if (!valor) return;
    for (;;) {
        int j = 0;
        while (valor[i] && valor[i] != '|' && j < HUD_TEXTO_MAX - 1)
            linea[j++] = valor[i++];
        linea[j] = '\0';
        if (n < HUD_DIALOGO_LINEAS) motor_hud_dialogo(n, linea);
        n++;
        if (!valor[i]) break;
        i++; /* barra vertical */
    }
}

/* Carpeta de una ruta (sin la barra final). */
static void ruta_carpeta(const char *ruta, char *dst, int tam) {
    int i, corte = -1;
    for (i = 0; ruta && ruta[i]; i++)
        if (ruta[i] == '\\' || ruta[i] == '/') corte = i;
    if (corte < 0) {
        if (tam > 0) dst[0] = '\0';
        return;
    }
    if (corte >= tam) corte = tam - 1;
    for (i = 0; i < corte; i++) dst[i] = ruta[i];
    dst[corte] = '\0';
}

/* Resuelve el mapa de la primera escena del LOGIC3: tal cual, en maps\
 * junto al ejecutable, o junto al propio .logic3 (con y sin maps\). */
static int resolver_mapa_l3(const char *ruta_logic3, const char *mapa,
                            char *dst, int tam) {
    char carpeta[MAX_PATH], prueba[MAX_PATH];

    if (!mapa || !mapa[0]) return 0;
    /* Fase 45: el juego compilado lleva el mapa dentro del blob, no en
     * una carpeta. activo_cargar_mapa lo busca por nombre base. */
    if (activo_existe_virtual(mapa)) {
        copiar_texto(dst, tam, mapa);
        return 1;
    }
    if (archivo_existe(mapa)) {
        copiar_texto(dst, tam, mapa);
        return 1;
    }
    _snprintf(prueba, sizeof(prueba), "maps\\%s", mapa);
    prueba[sizeof(prueba) - 1] = '\0';
    if (activo_existe_virtual(prueba) || archivo_existe(prueba)) {
        copiar_texto(dst, tam, prueba);
        return 1;
    }
    ruta_carpeta(ruta_logic3, carpeta, sizeof(carpeta));
    if (carpeta[0]) {
        _snprintf(prueba, sizeof(prueba), "%s\\%s", carpeta, mapa);
        prueba[sizeof(prueba) - 1] = '\0';
        if (archivo_existe(prueba)) {
            copiar_texto(dst, tam, prueba);
            return 1;
        }
        _snprintf(prueba, sizeof(prueba), "%s\\maps\\%s", carpeta, mapa);
        prueba[sizeof(prueba) - 1] = '\0';
        if (archivo_existe(prueba)) {
            copiar_texto(dst, tam, prueba);
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Sprite del jugador: un lobito azul de 24x24 dibujado a mano         */
/* ------------------------------------------------------------------ */

static void sprite_crear(void) {
    int px, py;
    for (py = 0; py < SPRITE_LADO; py++) {
        for (px = 0; px < SPRITE_LADO; px++) {
            u32 c = 0xFF2456A6u;                    /* cuerpo */
            int borde = px == 0 || px == SPRITE_LADO - 1 ||
                        py == 0 || py == SPRITE_LADO - 1;
            int esquina = (px < 2 || px > SPRITE_LADO - 3) &&
                         (py < 2 || py > SPRITE_LADO - 3);
            int oreja = (px >= 1 && px <= 6 && py <= 7) ||
                        (px >= SPRITE_LADO - 7 && px <= SPRITE_LADO - 2 && py <= 7);
            int ojo = (py >= 8 && py <= 10) &&
                      ((px >= 5 && px <= 7) || (px >= 16 && px <= 18));
            int hocico = (py >= 14 && py <= 19) &&
                        (px >= 9 && px <= 14);
            if (esquina) { c = 0; }
            else if (oreja) c = 0xFF16325Eu;         /* orejas oscuras */
            else if (borde) c = 0xFF16325Eu;
            else if (ojo) c = 0xFFFFFFFFu;            /* ojos */
            else if (hocico) c = 0xFFD8E6F8u;         /* hocico claro */
            juego.sprite[py * SPRITE_LADO + px] = c;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Colision del sprite contra las celdas solidas del mapa             */
/* ------------------------------------------------------------------ */

static int choca_con_mapa_wh(int x, int y, int w, int h) {
    return mapa_caja_bloqueada(&juego.mapa, x, y, w, h, 0);
}

static int choca_con_mapa(int x, int y) {
    return choca_con_mapa_wh(x, y, SPRITE_LADO, SPRITE_LADO);
}

/* Busca un punto libre para aparecer: en espiral desde la posicion
 * inicial hasta encontrar un hueco con margen (el sprite mas 8 pixeles
 * de holgura en cada direccion), para no nacer pegado a un muro. */
static void buscar_spawn(void) {
    const int margen = 8;
    int r, x, y;
    if (!choca_con_mapa_wh(juego.x - margen, juego.y - margen,
                           SPRITE_LADO + 2 * margen,
                           SPRITE_LADO + 2 * margen))
        return;
    for (r = 16; r <= 4096; r += 16) {
        for (y = -r; y <= r; y += 8) {
            int paso = (y == -r || y == r) ? 8 : 2 * r;
            for (x = -r; x <= r; x += paso) {
                if (!choca_con_mapa_wh(x - margen, y - margen,
                                       SPRITE_LADO + 2 * margen,
                                       SPRITE_LADO + 2 * margen)) {
                    juego.x = x;
                    juego.y = y;
                    return;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Scripts lobo: el codigo de los botones FUNCIONES (fase 8)           */
/* ------------------------------------------------------------------ */

/* Un script por elemento del lienzo con funcion de codigo: todos ven
 * el mismo estado del juego (la posicion del lobo, las teclas) y cada
 * uno guarda su maquina (sus variables locales). Los elementos que
 * repiten funcion comparten el programa compilado. */
static LoboPrograma g_programas[ACTIVO_FUNCIONES_MAX];
static int g_prog_id[ACTIVO_FUNCIONES_MAX]; /* id de funcion de cada programa */
static int g_num_programas;

static struct {
    int prog; /* indice en g_programas */
    LoboMaquina maquina;
} g_scripts[ACTIVO_USADAS_MAX];
static int g_num_scripts;

static FILE *g_traza; /* --traza: una linea por fotograma */

/* --- modo VM (fase 25): un programa y una maquina por script --- */
static MotorL3 g_l3;
static Eventos g_eventos;       /* planificador de eventos (fase 27) */
static int g_modo_vm;
static LoboMemoria g_memoria; /* globales, listas y mensajes compartidos */

/* --- eventos (fase 27) --- */
static int g_evento_duracion;   /* --evento N: fotogramas que dura la tecla */
static int g_eventos_traza;     /* --eventos: volcar eventos ejecutados */
static unsigned g_tecla_guion;  /* tecla del guion de captura */
static char g_mensaje_ini[LOBO_TEXTO_MAX]; /* --mensaje: mensaje inicial */

/* --- entidades (fase 26) --- */
static int g_entidades; /* --entidades: detalle en traza y volcado */
static int g_colisiones_traza; /* --colisiones: triggers, dano y suelo */

/* --- render y camara (fase 30) --- */
static int g_render_traza;              /* --render: traza de trajes y blits */
static RenderCamara g_camara;
static RenderInfo g_render_info;
static int g_camara_objetivo = -1;     /* --camara: -1 = no cambiar */
static int g_zoom_override;            /* --zoom: 0 = no cambiar */
static int g_limites[4];
static int g_limites_override;
#define PIXEL_SONDAS_MAX 16
static int g_pixels[PIXEL_SONDAS_MAX][2];
static int g_num_pixels;
static char g_ent_nombres[LOBO_ENTIDAD_NOMBRES_MAX][LOBO_TEXTO_MAX];
static int g_num_ent_nombres;

/* Callback de choca() del lenguaje: la caja del sprite contra el
 * mapa solido. */
static int lobo_choca(void *ctx, int x, int y) {
    (void)ctx;
    return choca_con_mapa(x, y);
}

/* Compila los scripts del proyecto y prepara una maquina por elemento.
 * Las funciones vacias o borradas se saltan; las que no compilan se
 * avisan (cuadro en modo ventana, linea en la traza) y tambien se
 * saltan: el resto del juego sigue corriendo. */
static void scripts_preparar(int con_ventana) {
    int ids[ACTIVO_USADAS_MAX];
    int n = activo_funciones_usadas(ids, ACTIVO_USADAS_MAX);
    int i, j;

    g_num_programas = 0;
    g_num_scripts = 0;
    for (i = 0; i < n; i++) {
        const char *codigo = activo_funcion_codigo(ids[i]);
        char error[LOBO_ERROR_MAX];
        int prog = -1;

        if (!codigo || !codigo[0]) continue;

        for (j = 0; j < g_num_programas; j++)
            if (g_prog_id[j] == ids[i]) { prog = j; break; }
        if (prog < 0) {
            if (!lobo_parsear(codigo, &g_programas[g_num_programas],
                              error, sizeof(error))) {
                if (g_traza)
                    fprintf(g_traza, "lobo: error %d %s\n", ids[i], error);
                if (con_ventana) {
                    char msg[256];
                    _snprintf(msg, sizeof(msg),
                              "La funcion %d no compila:\n%s", ids[i], error);
                    msg[sizeof(msg) - 1] = '\0';
                    MessageBoxA(NULL, msg, "egine", MB_ICONWARNING);
                }
                continue;
            }
            g_prog_id[g_num_programas] = ids[i];
            prog = g_num_programas++;
        }
        g_scripts[g_num_scripts].prog = prog;
        lobo_maquina_reiniciar(&g_scripts[g_num_scripts].maquina);
        g_num_scripts++;
    }
    if (g_traza) fprintf(g_traza, "lobo: %d scripts\n", g_num_scripts);
}

/* ------------------------------------------------------------------ */
/* Entidades (fase 26)                                                */
/* ------------------------------------------------------------------ */

/* Slot global del nombre de una variable de entidad (lo crea si hace
 * falta). Todos los programas del proyecto comparten estos slots. */
static int ent_nombre_slot(const char *nombre) {
    int i;
    if (!nombre) return -1;
    for (i = 0; i < g_num_ent_nombres; i++)
        if (strcmp(g_ent_nombres[i], nombre) == 0) return i;
    if (g_num_ent_nombres >= LOBO_ENTIDAD_NOMBRES_MAX) {
        entidades_aviso();
        return -1;
    }
    copiar_texto(g_ent_nombres[g_num_ent_nombres], LOBO_TEXTO_MAX, nombre);
    return g_num_ent_nombres++;
}

/* Traduce un campo de lobo (LOBO_MI_*) al campo del modulo de entidades. */
static int campo_entidad_lobo(int campo) {
    switch (campo) {
    case LOBO_MI_X: return ENTIDAD_CAMPO_X;
    case LOBO_MI_Y: return ENTIDAD_CAMPO_Y;
    case LOBO_MI_VIDA: return ENTIDAD_CAMPO_VIDA;
    case LOBO_MI_VISIBLE: return ENTIDAD_CAMPO_VISIBLE;
    case LOBO_MI_TRAJE: return ENTIDAD_CAMPO_TRAJE;
    case LOBO_MI_ID: return ENTIDAD_CAMPO_ID;
    case LOBO_MI_ESCENA: return ENTIDAD_CAMPO_ESCENA;
    case LOBO_MI_ANCHO: return ENTIDAD_CAMPO_W;
    case LOBO_MI_ALTO: return ENTIDAD_CAMPO_H;
    case LOBO_MI_CAPA: return ENTIDAD_CAMPO_CAPA;
    default: return -1;
    }
}

static int ent_leer(void *ctx, int id, int campo) {
    (void)ctx;
    if (campo >= LOBO_MI_VARS)
        return entidades_leer_var(id, campo - LOBO_MI_VARS);
    campo = campo_entidad_lobo(campo);
    return campo < 0 ? 0 : entidades_leer_campo(id, campo);
}

static int ent_escribir(void *ctx, int id, int campo, int valor) {
    (void)ctx;
    if (campo >= LOBO_MI_VARS)
        return entidades_escribir_var(id, campo - LOBO_MI_VARS, valor);
    campo = campo_entidad_lobo(campo);
    if (campo < 0 || campo == ENTIDAD_CAMPO_ID ||
        campo == ENTIDAD_CAMPO_TIPO)
        return 0;
    return entidades_escribir_campo(id, campo, valor);
}

static int ent_crear(void *ctx, const char *tag, int x, int y) {
    int tipo;
    (void)ctx;
    tipo = entidades_tipo_por_tag(tag);
    if (tipo < 0) {
        /* Tag desconocido: se avisa y la llamada devuelve -1. */
        entidades_aviso();
        return -1;
    }
    return entidad_crear(tipo, x, y);
}

static int ent_borrar(void *ctx, int id) {
    (void)ctx;
    return entidad_borrar(id);
}

static int ent_clonar(void *ctx, int id) {
    (void)ctx;
    return entidades_clonar(id);
}

static int ent_mover(void *ctx, int id, int x, int y) {
    (void)ctx;
    return entidad_mover(id, x, y);
}

/* --- colisiones entidad-entidad (fase 29) --- */
static int col_toca(void *ctx, int entidad, int id) {
    (void)ctx;
    return entidades_tocan(entidad, id);
}

static int col_toca_tag(void *ctx, int entidad, const char *tag) {
    (void)ctx;
    return entidades_toca_tag(entidad, tag);
}

static int col_distancia(void *ctx, int entidad, int id) {
    (void)ctx;
    return entidades_distancia(entidad, id);
}

static int col_caja_de(void *ctx, int entidad, int id, int indice) {
    (void)ctx;
    (void)entidad; /* la caja consultada es la del id, no la del emisor */
    return entidades_caja(id, indice);
}


/* Instancia los objetos de una escena (arranque y reinicio de escena). */
static void instanciar_escena(int escena) {
    int i;
    for (i = 0; i < g_l3.num_objetos; i++) {
        MotorL3Objeto *o = &g_l3.objetos[i];
        int tipo;
        if (o->escena != escena) continue;
        tipo = entidades_tipo_por_objeto(o->id);
        if (tipo >= 0) entidad_crear(tipo, o->x, o->y);
    }
}

/* Registra tipos y variables del LOGIC3 e instancia los objetos de la
 * escena inicial. Los objetos creados al arrancar no gastan el cupo de
 * altas por fotograma. */
static void entidades_preparar(void) {
    int i, j;
    entidades_reiniciar();
    g_num_ent_nombres = 0;
    memset(g_ent_nombres, 0, sizeof(g_ent_nombres));

    for (i = 0; i < g_l3.num_objetos; i++) {
        MotorL3Objeto *o = &g_l3.objetos[i];
        int tipo = entidades_tipo_agregar(o->id, o->nombre, o->escena,
                                          o->x, o->y, o->w, o->h,
                                          o->visible, o->capa);
        if (tipo < 0) continue;
        for (j = 0; j < o->num_variables; j++) {
            int slot = ent_nombre_slot(o->variables[j].nombre);
            if (slot >= 0)
                entidades_tipo_var(tipo, slot, o->variables[j].nombre,
                                   o->variables[j].valor);
        }
    }
    instanciar_escena(g_l3.escena_inicial);
    /* El cupo de altas por fotograma empieza limpio en el fotograma 1. */
    entidades_fin_fotograma();
}

/* Remapea los indices locales de variables de entidad de un programa a
 * los slots globales del proyecto (los de @nombre y mi.variable). */
static void lobo_remap_programa(LoboPrograma *p) {
    int mapa[LOBO_ENTIDAD_NOMBRES_MAX];
    int i;
    for (i = 0; i < p->num_ent_nombres; i++)
        mapa[i] = ent_nombre_slot(p->entidad_nombres[i]);
    for (i = 0; i < p->largo; i++) {
        LoboInst *in = &p->insts[i];
        if (in->op == LOBO_CARGAR_ENTIDAD || in->op == LOBO_GUARDAR_ENTIDAD) {
            if (in->valor >= 0 && in->valor < p->num_ent_nombres &&
                mapa[in->valor] >= 0)
                in->valor = mapa[in->valor];
        } else if (in->op == LOBO_CARGAR_MI || in->op == LOBO_GUARDAR_MI) {
            if (in->valor >= LOBO_MI_VARS) {
                int slot = in->valor - LOBO_MI_VARS;
                if (slot >= 0 && slot < p->num_ent_nombres &&
                    mapa[slot] >= 0)
                    in->valor = LOBO_MI_VARS + mapa[slot];
            }
        }
    }
}

/* Envoltorio para eventos_cargar (que pasa un contexto opcional). */
static void remapear_programa_cb(void *ctx, LoboPrograma *p) {
    (void)ctx;
    lobo_remap_programa(p);
}

/* --- fase 38: callbacks del HUD ------------------------------------ */

static int cb_hud_vidas(void *ctx, int valor) {
    int anterior = motor_hud_leer_vidas();
    (void)ctx;
    motor_hud_vidas(valor);
    return anterior;
}

static int cb_hud_puntos(void *ctx, int valor) {
    int anterior = motor_hud_leer_puntos();
    (void)ctx;
    motor_hud_puntos(valor);
    return anterior;
}

static int cb_hud_dialogo(void *ctx, int linea, const char *texto) {
    (void)ctx;
    motor_hud_dialogo(linea, texto);
    return 0;
}

static int cb_hud_texto(void *ctx, int x, int y, const char *texto,
                        unsigned color, int escala) {
    (void)ctx;
    return motor_hud_texto(x, y, texto, (u32)color, escala);
}

static int cb_hud_rect(void *ctx, int x, int y, int w, int h,
                       unsigned color) {
    (void)ctx;
    return motor_hud_rect(x, y, w, h, (u32)color);
}

static int cb_hud_barra(void *ctx, int x, int y, int w, int h, int valor,
                        int max, unsigned color) {
    (void)ctx;
    return motor_hud_barra(x, y, w, h, valor, max, (u32)color);
}

/* La llamada del script solo registra la imagen; el dibujo se resuelve
 * desde motor_hud_dibujar, cuando el framebuffer ya esta listo. */
static int cb_hud_imagen(void *ctx, int id, int x, int y) {
    (void)ctx;
    return motor_hud_imagen(id, x, y);
}

static int cb_hud_imagen_dibujar(void *ctx, int id, int x, int y) {
    Entidad *e = entidad_buscar(id);
    RenderSeleccion sel;
    RenderInfo info;
    (void)ctx;
    if (!e || !e->visible || !juego.fb) return 0;
    memset(&info, 0, sizeof(info));
    if (!render_seleccionar(&g_l3, e, juego.fotograma, &sel)) return 0;
    return render_dibujar(juego.fb, juego.fb_ancho, juego.fb_alto,
                          &sel, x, y, 1, &info);
}

static int cb_hud_boton(void *ctx, int id, int x, int y, int w, int h,
                        const char *etiqueta) {
    (void)ctx;
    return motor_hud_boton(id, x, y, w, h, etiqueta);
}

static int cb_hud_plantilla(void *ctx, int cual) {
    (void)ctx;
    motor_hud_plantilla(cual);
    return 0;
}

/* --- fase 39: callbacks del mezclador ----------------------------- */

static int cb_sonar(void *ctx, const char *nombre) {
    (void)ctx;
    return motor_audio_sonar(nombre);
}

static int cb_musica(void *ctx, const char *nombre) {
    (void)ctx;
    return motor_audio_musica(nombre);
}

static int cb_detener_sonido(void *ctx) {
    (void)ctx;
    return motor_audio_detener();
}

static int cb_volumen(void *ctx, int valor) {
    (void)ctx;
    return motor_audio_volumen(valor);
}

static int cb_volumen_actual(void *ctx) {
    (void)ctx;
    return motor_audio_volumen_actual();
}

/* --- fase 40: callbacks del guardado ------------------------------- */

static int cb_guardar(void *ctx, const char *clave, int valor) {
    (void)ctx;
    return motor_save_guardar(clave, valor);
}

static int cb_cargar(void *ctx, const char *clave) {
    (void)ctx;
    return motor_save_cargar(clave);
}

static int cb_borrar_save(void *ctx) {
    (void)ctx;
    return motor_save_borrar();
}

static int cb_record(void *ctx, const char *clave, int valor) {
    (void)ctx;
    return motor_save_record(clave, valor);
}

static int cb_record_actual(void *ctx, const char *clave) {
    (void)ctx;
    return motor_save_record_actual(clave);
}

static int cb_ajuste(void *ctx, const char *nombre, int valor) {
    (void)ctx;
    return motor_save_ajuste(nombre, valor);
}

static int cb_ajuste_actual(void *ctx, const char *nombre) {
    (void)ctx;
    return motor_save_ajuste_actual(nombre);
}

/* --- fase 41: escenas, transiciones, camara y efectos --------------- */
typedef struct EscenasRuntime {
    int activo;            /* 1 con --escenas, 0 sin callbacks */
    int inicial;           /* id de la escena que arranco */
    int actual;            /* id de la escena visible */
    int cambios;           /* cambios de escena aplicados */
    int reinicios;         /* reiniciar_escena() vistos */
    int pendiente;         /* id pedido por cambiar_escena(), 0 = ninguno */
    char ruta[MAX_PATH];   /* .logic3 del que salen los mapas */
    int objetivo;          /* objetivo de camara anterior */
    int zoom;              /* zoom anterior */
    int limites[4];        /* limites anteriores */
    int suave;             /* pasos de suavizado */
    int cam_listo;         /* camara suavizada ya inicializada */
    int cam_x, cam_y;      /* camara suavizada entera */
    struct {
        int activo;
        int tipo;          /* 0 ninguna, 1 negro, 2 blanco */
        int duracion;
        int transcurrido;
        int fase;          /* 1 cubriendo, 2 descubriendo */
    } trans;
    int parpadeo_id;
    int parpadeo_periodo;
    int temblor_intensidad;
    int temblor_restantes;
    int flash_color;
    int flash_restantes;
    int flash_duracion;
    int fade_color;
    int fade_alpha;
    int fade_restantes;
} EscenasRuntime;

static EscenasRuntime g_escenas;
static int g_escenas_activo;
static const char *g_ruta_volcar_escenas;

/* --- perfilador, presupuestos y estres (fase 42) --- */
static int g_perfil;
static const char *g_ruta_volcar_perfil;
static unsigned long g_presupuesto_us = 16667; /* 60 FPS */
static int g_estres_entidades;
static int g_estres_bloques;
static LARGE_INTEGER g_perfil_frecuencia;
static unsigned long g_perfil_t0;
static unsigned long g_perfil_ins0;
static unsigned long g_perfil_ins_foto;
static unsigned long g_perfil_us_foto;
static unsigned long g_perfil_us_total;
static unsigned long g_perfil_us_max;
static int g_perfil_blits_foto;
static int g_perfil_fotogramas;
static int g_perfil_avisos;
static int g_perfil_excesos;
static int g_perfil_limite_blits = RENDER_BLITS_MAX;
static void perfil_iniciar(void);
static void perfil_fin(void);
static void perfil_cache_render(void);

/* Devuelve el indice de la escena con ese id, o -1 si no existe. */
static int escena_indice(int id) {
    int i;
    for (i = 0; i < g_l3.num_escenas; i++)
        if (g_l3.escenas[i].id == id) return i;
    return -1;
}

/* Aplica el cambio pedido por un script. Se llama al final del fotograma,
 * nunca desde la VM: recargar el mapa a mitad de un evento desordenaria
 * los scripts que aun estan leyendo las entidades de la escena vieja.
 * La memoria de la VM (g_memoria) no se toca: el progreso se conserva. */
static void aplicar_cambio_escena(int id) {
    int indice = escena_indice(id);
    char ruta[MAX_PATH];

    if (indice < 0) return;
    g_eventos.escena = id;
    g_escenas.actual = id;
    g_escenas.cambios++;
    if (g_l3.escenas[indice].fondo)
        g_fondo = (u32)g_l3.escenas[indice].fondo;

    motor_mapa_liberar(&juego.mapa);
    if (g_l3.escenas[indice].mapa[0] &&
        resolver_mapa_l3(g_escenas.ruta, g_l3.escenas[indice].mapa,
                         ruta, sizeof(ruta)) &&
        activo_cargar_mapa(ruta, &juego.mapa)) {
        copiar_texto(juego.mapa_usado, sizeof(juego.mapa_usado), ruta);
    } else {
        juego.mapa_usado[0] = '\0';
    }

    entidades_vaciar();
    instanciar_escena(id);
    entidades_fin_fotograma();
    eventos_escena_iniciar(&g_eventos, NULL);

    /* Los efectos son de la escena que los pidio; la transicion sigue
     * activa para descubrir la nueva. */
    g_escenas.parpadeo_id = 0;
    g_escenas.parpadeo_periodo = 0;
    g_escenas.temblor_intensidad = 0;
    g_escenas.temblor_restantes = 0;
    g_escenas.flash_restantes = 0;
    g_escenas.fade_restantes = 0;
    g_escenas.cam_listo = 0;
}

/* Callbacks de LoboEstado. Sin --escenas quedan a NULL y la VM devuelve
 * valores neutros, igual que pasa con el audio o el guardado. */
static int cb_cambiar_escena(void *ctx, int id) {
    (void)ctx;
    if (escena_indice(id) < 0) return 0;
    g_escenas.pendiente = id;
    return 1;
}

static int cb_escena_actual(void *ctx) {
    (void)ctx;
    return g_escenas.actual;
}

static int cb_camara(void *ctx, int campo, int a, int b, int c, int d) {
    int anterior;
    (void)ctx;
    (void)d;
    switch (campo) {
    case 0:
        anterior = g_camara.objetivo;
        g_camara.objetivo = a;
        g_escenas.objetivo = a;
        return anterior;
    case 1:
        anterior = g_camara.zoom;
        g_camara.zoom = (a == 2 || a == 4) ? a : 1;
        g_escenas.zoom = g_camara.zoom;
        return anterior;
    case 2:
        g_camara.x = a;
        g_camara.y = b;
        g_camara.ancho = c;
        g_camara.alto = d;
        g_escenas.limites[0] = a;
        g_escenas.limites[1] = b;
        g_escenas.limites[2] = c;
        g_escenas.limites[3] = d;
        return 0;
    case 3:
        anterior = g_escenas.suave;
        g_escenas.suave = a > 0 ? a : 0;
        g_escenas.cam_listo = 0;
        return anterior;
    default:
        return 0;
    }
}

static int cb_efecto(void *ctx, int campo, int a, int b, int c, int d) {
    int n;
    (void)ctx;
    (void)d;
    switch (campo) {
    case 0:
        n = b > 0 ? b : 0;
        if (n > 100000) n = 100000;
        g_escenas.trans.activo = n > 0;
        g_escenas.trans.tipo = a;
        g_escenas.trans.duracion = n;
        g_escenas.trans.transcurrido = 0;
        g_escenas.trans.fase = 1;
        break;
    case 1:
        g_escenas.parpadeo_id = a;
        g_escenas.parpadeo_periodo = b > 0 ? b : 1;
        break;
    case 2:
        n = b > 0 ? b : 0;
        if (n > 100000) n = 100000;
        g_escenas.temblor_intensidad = a > 0 ? a : 0;
        g_escenas.temblor_restantes = n;
        break;
    case 3:
        n = b > 0 ? b : 0;
        if (n > 100000) n = 100000;
        g_escenas.flash_color = a;
        g_escenas.flash_duracion = n;
        g_escenas.flash_restantes = n;
        break;
    case 4:
        n = c > 0 ? c : 0;
        if (n > 100000) n = 100000;
        g_escenas.fade_color = a;
        g_escenas.fade_alpha = b > 0 ? (b < 255 ? b : 255) : 0;
        g_escenas.fade_restantes = n;
        break;
    default:
        break;
    }
    return 0;
}

/* Prepara el estado de escenas despues de cargar el LOGIC3 y la camara. */
static void escenas_preparar(const MotorL3 *p, const char *ruta) {
    memset(&g_escenas, 0, sizeof(g_escenas));
    g_escenas.activo = g_escenas_activo;
    g_escenas.inicial = p->escena_inicial;
    g_escenas.actual = p->escena_inicial;
    copiar_texto(g_escenas.ruta, sizeof(g_escenas.ruta),
                 ruta ? ruta : "");
    g_escenas.objetivo = g_camara.objetivo;
    g_escenas.zoom = g_camara.zoom;
    g_escenas.limites[0] = g_camara.x;
    g_escenas.limites[1] = g_camara.y;
    g_escenas.limites[2] = g_camara.ancho;
    g_escenas.limites[3] = g_camara.alto;
}

/* Avanza transiciones y efectos y aplica el cambio de escena pendiente.
 * Con transicion el cambio ocurre a mitad de la cobertura. */
static void escenas_fotograma(void) {
    int mitad, id;

    if (g_escenas.temblor_restantes > 0) g_escenas.temblor_restantes--;
    if (g_escenas.flash_restantes > 0) g_escenas.flash_restantes--;
    if (g_escenas.fade_restantes > 0) g_escenas.fade_restantes--;

    if (!g_escenas.trans.activo) {
        id = g_escenas.pendiente;
        g_escenas.pendiente = 0;
        if (id) aplicar_cambio_escena(id);
        return;
    }
    if (g_escenas.trans.duracion <= 0) {
        g_escenas.trans.activo = 0;
        return;
    }
    g_escenas.trans.transcurrido++;
    mitad = (g_escenas.trans.duracion + 1) / 2;
    if (g_escenas.trans.fase == 1 &&
        g_escenas.trans.transcurrido >= mitad) {
        id = g_escenas.pendiente;
        g_escenas.pendiente = 0;
        g_escenas.trans.fase = 2;
        if (id) aplicar_cambio_escena(id);
    }
    if (g_escenas.trans.transcurrido >= g_escenas.trans.duracion)
        g_escenas.trans.activo = 0;
}

/* Traza opcional de escenas: una linea por fotograma, detras de las
 * trazas clasicas, solo cuando se ha pedido --escenas. */
static void traza_escenas(void) {
    if (!g_traza || !g_escenas_activo) return;
    fprintf(g_traza,
            "escena %d camara %d %d zoom %d suave %d "
            "trans %d %d %d parpadeo %d %d temblor %d %d "
            "flash %d %d fade %d %d %d cambios %d reinicios %d\n",
            g_escenas.actual, g_escenas.cam_x, g_escenas.cam_y,
            g_camara.zoom, g_escenas.suave,
            g_escenas.trans.tipo, g_escenas.trans.duracion,
            g_escenas.trans.transcurrido,
            g_escenas.parpadeo_id, g_escenas.parpadeo_periodo,
            g_escenas.temblor_intensidad, g_escenas.temblor_restantes,
            g_escenas.flash_color, g_escenas.flash_restantes,
            g_escenas.fade_color, g_escenas.fade_alpha,
            g_escenas.fade_restantes,
            g_escenas.cambios, g_escenas.reinicios);
}

/* Volcado propio de la fase 41 (--volcar-escenas). */
static void escenas_volcar(const char *ruta) {
    FILE *f = fopen(ruta, "wt");
    int i;
    if (!f) return;
    fprintf(f, "escenas: activo %d\n", g_escenas.activo);
    fprintf(f, "escenas: inicial %d\n", g_escenas.inicial);
    fprintf(f, "escenas: actual %d\n", g_escenas.actual);
    fprintf(f, "escenas: numero %d\n", g_l3.num_escenas);
    for (i = 0; i < g_l3.num_escenas; i++) {
        const MotorL3Escena *e = &g_l3.escenas[i];
        fprintf(f, "escena %d \"%s\" mapa %s fondo %08X\n",
                e->id, e->nombre, e->mapa, e->fondo);
    }
    fprintf(f, "escenas: mapa %s\n",
            juego.mapa_usado[0] ? juego.mapa_usado : "-");
    fprintf(f, "escenas: camara objetivo %d zoom %d "
            "limites %d,%d,%d,%d suavizar %d\n",
            g_camara.objetivo, g_camara.zoom,
            g_camara.x, g_camara.y, g_camara.ancho, g_camara.alto,
            g_escenas.suave);
    fprintf(f, "escenas: transicion tipo %d duracion %d transcurrido %d fase %d\n",
            g_escenas.trans.tipo, g_escenas.trans.duracion,
            g_escenas.trans.transcurrido, g_escenas.trans.fase);
    fprintf(f, "escenas: parpadeo id %d periodo %d\n",
            g_escenas.parpadeo_id, g_escenas.parpadeo_periodo);
    fprintf(f, "escenas: temblor intensidad %d restantes %d\n",
            g_escenas.temblor_intensidad, g_escenas.temblor_restantes);
    fprintf(f, "escenas: flash color %d restantes %d\n",
            g_escenas.flash_color, g_escenas.flash_restantes);
    fprintf(f, "escenas: fade color %d alpha %d restantes %d\n",
            g_escenas.fade_color, g_escenas.fade_alpha,
            g_escenas.fade_restantes);
    fprintf(f, "escenas: cambios %d reinicios %d\n",
            g_escenas.cambios, g_escenas.reinicios);
    fclose(f);
}

/* Estado compartido por los scripts de un fotograma: posicion del lobo,
 * teclas, memoria de la VM y callbacks de entidades. */
static void estado_preparar(LoboEstado *st) {
    memset(st, 0, sizeof(*st));
    st->x = juego.x;
    st->y = juego.y;
    st->vel = g_velocidad;
    st->fotograma = juego.fotograma;
    st->entrada = juego.entrada; /* LOBO_TECLA_* == MOTOR_ENT_* */
    st->entrada_ext = g_entrada_extactiva ? &g_entrada_ext : NULL;
    st->memoria = &g_memoria;
    st->choca = lobo_choca;
    st->ctx = NULL;
    st->entidad_crear = ent_crear;
    st->entidad_borrar = ent_borrar;
    st->entidad_clonar = ent_clonar;
    st->entidad_mover = ent_mover;
    st->entidad_leer = ent_leer;
    st->entidad_escribir = ent_escribir;
    st->ctx_entidades = NULL;
    st->enviar = eventos_enviar_cb;
    st->reiniciar_escena = eventos_reiniciar_cb;
    st->ctx_eventos = &g_eventos;
    st->toca = col_toca;
    st->toca_tag = col_toca_tag;
    st->distancia = col_distancia;
    st->caja_de = col_caja_de;
    st->ctx_colisiones = NULL;
    st->vel_x = g_fisica.vel_x;
    st->vel_y = g_fisica.vel_y;
    st->gravedad = g_fisica.gravedad;
    st->rebote = g_fisica.rebote;
    st->friccion = g_fisica.friccion;
    st->en_suelo = g_fisica.en_suelo;
    if (g_hud_activo) {
        st->hud_vidas = cb_hud_vidas;
        st->hud_puntos = cb_hud_puntos;
        st->hud_dialogo = cb_hud_dialogo;
        st->hud_texto = cb_hud_texto;
        st->hud_rect = cb_hud_rect;
        st->hud_barra = cb_hud_barra;
        st->hud_imagen = cb_hud_imagen;
        st->hud_boton = cb_hud_boton;
        st->hud_plantilla = cb_hud_plantilla;
        st->ctx_hud = NULL;
    }
    if (g_audio_activo) {
        st->sonar = cb_sonar;
        st->musica = cb_musica;
        st->detener_sonido = cb_detener_sonido;
        st->volumen = cb_volumen;
        st->volumen_actual = cb_volumen_actual;
        st->ctx_audio = NULL;
    }
    if (g_saves_activo) {
        st->guardar = cb_guardar;
        st->cargar = cb_cargar;
        st->borrar_save = cb_borrar_save;
        st->record = cb_record;
        st->record_actual = cb_record_actual;
        st->ajuste = cb_ajuste;
        st->ajuste_actual = cb_ajuste_actual;
        st->ctx_save = NULL;
    }
    if (g_escenas_activo) {
        st->cambiar_escena = cb_cambiar_escena;
        st->escena_actual = cb_escena_actual;
        st->camara = cb_camara;
        st->efecto = cb_efecto;
        st->ctx_escenas = NULL;
    }
}

/* Copia la fisica escrita por los scripts al estado global y da un paso
 * contra el mapa. Solo se activa si el script ha tocado alguna variable de
 * fisica; con todo a cero el comportamiento clasico queda intacto. */
static void aplicar_fisica(LoboEstado *st) {
    int activo;
    g_fisica.vel_x = st->vel_x;
    g_fisica.vel_y = st->vel_y;
    g_fisica.gravedad = st->gravedad;
    g_fisica.rebote = st->rebote;
    g_fisica.friccion = st->friccion;
    g_fisica.en_suelo = st->en_suelo;
    activo = g_fisica.vel_x || g_fisica.vel_y || g_fisica.gravedad ||
             g_fisica.rebote || g_fisica.friccion;
    if (activo) {
        /* La caja del lobo es el sprite de 24 pixeles. */
        g_fisica.en_suelo = fisica_paso(&juego.mapa, &juego.x, &juego.y,
                                        SPRITE_LADO, SPRITE_LADO,
                                        &g_fisica.vel_x, &g_fisica.vel_y,
                                        g_fisica.gravedad, g_fisica.rebote,
                                        g_fisica.friccion,
                                        &g_fisica.en_suelo);
        st->vel_x = g_fisica.vel_x;
        st->vel_y = g_fisica.vel_y;
        st->en_suelo = g_fisica.en_suelo;
        /* El paso ha movido al lobo en el mundo: devuelve la posicion al
         * estado que leen los scripts y la traza. */
        st->x = juego.x;
        st->y = juego.y;
    }
}

/* Siembra la memoria de la VM con las variables y listas globales del
 * LOGIC3 (fase 28). Los indices son los de declaracion: la lista "inventario"
 * primera del archivo es la lista 0 que ve el script. Las variables de texto
 * se registran como textos runtime, detras de los textos del programa. */
static void memoria_preparar_l3(void) {
    int i, j;
    memset(&g_memoria, 0, sizeof(g_memoria));
    for (i = 0; i < g_l3.num_textos && i < LOBO_TEXTOS_RUNTIME_MAX; i++) {
        strncpy(g_memoria.textos[i], g_l3.textos[i], LOBO_TEXTO_MAX - 1);
        g_memoria.textos[i][LOBO_TEXTO_MAX - 1] = 0;
    }
    g_memoria.num_textos = g_l3.num_textos;
    for (i = 0; i < g_l3.num_variables && i < LOBO_GLOBALES_MAX; i++) {
        const MotorL3Variable *v = &g_l3.variables[i];
        int valor = v->valor;
        if (v->tipo == 2 && valor >= 0 && valor < g_l3.num_textos)
            valor = (int)(LOBO_TEXTO_MARCA |
                          (unsigned)(LOBO_TEXTOS_MAX + valor));
        g_memoria.globales[i] = valor;
    }
    for (i = 0; i < g_l3.num_listas && i < LOBO_LISTAS_MAX; i++) {
        const MotorL3Lista *li = &g_l3.listas[i];
        LoboLista *dst = &g_memoria.listas[i];
        int n = li->num_elementos;
        if (n > li->capacidad) n = li->capacidad;
        if (n > LOBO_LISTA_ELEMENTOS_MAX) n = LOBO_LISTA_ELEMENTOS_MAX;
        for (j = 0; j < n; j++) {
            int valor = li->elementos[j];
            if (li->tipo == 2 && valor >= 0 && valor < g_l3.num_textos)
                valor = (int)(LOBO_TEXTO_MARCA |
                              (unsigned)(LOBO_TEXTOS_MAX + valor));
            dst->elem[j] = valor;
        }
        dst->n = n;
    }
}

/* Compila todos los scripts del LOGIC3 en el planificador de eventos:
 * cada bloque codigo.lobo trae su propio texto y su propia maquina,
 * todos contra la misma memoria compartida (globales, listas y mensajes
 * de la fase 25). */
static void scripts_preparar_l3(int con_ventana) {
    int i, n;

    memoria_preparar_l3();
    eventos_iniciar(&g_eventos, g_traza, g_eventos_traza);
    g_eventos.escena = g_l3.escena_inicial;
    n = eventos_cargar(&g_eventos, &g_l3, remapear_programa_cb, NULL);
    if (g_eventos.errores > 0 && con_ventana) {
        char msg[256];
        _snprintf(msg, sizeof(msg),
                  "El script del bloque %d no compila:\n%s",
                  g_eventos.bloque_error, g_eventos.error);
        msg[sizeof(msg) - 1] = '\0';
        MessageBoxA(NULL, msg, "egine", MB_ICONWARNING);
    }
    eventos_escena_iniciar(&g_eventos, NULL);

    if (g_traza) {
        fprintf(g_traza, "vm: %d scripts\n", n);
        for (i = 0; i < n; i++) {
            const EventoScript *s = eventos_script(&g_eventos, i);
            fprintf(g_traza, "vm: script %d bloque %d objeto %d evento %d\n", i,
                    s->bloque, s->objeto, s->evento);
        }
        if (g_entidades)
            fprintf(g_traza, "entidades: %d iniciales avisos %d\n",
                    entidades_num(), entidades_avisos());
    }
}

/* Detalle de entidades de un fotograma (--entidades): id, x e y, en el
 * orden de dibujo (capa y luego id). */
static void traza_entidades(void) {
    int orden[ENTIDADES_MAX];
    int n, i;
    if (!g_traza || !g_entidades) return;
    n = entidades_orden(orden, ENTIDADES_MAX);
    for (i = 0; i < n; i++) {
        Entidad *e = entidad_indice(orden[i]);
        if (e) fprintf(g_traza, "ent %d %d %d\n", e->id, e->x, e->y);
    }
}

/* Una linea de traza por fotograma: contador, x e y del lobo. En modo VM
 * se anade una linea con el estado de cada script (indice:estado:pc:
 * espera), que es lo que demuestra que un script esperando no bloquea a
 * los demas. */
static void traza_fotograma(void) {
    int i, n;
    if (!g_traza) return;
    fprintf(g_traza, "%d %d %d\n", juego.fotograma, juego.x, juego.y);
    if (g_colisiones_traza) {
        int tx = juego.x, ty = juego.y;
        int trig[8], ntrig = 0, dano = 0, k;
        colisionar_caja_mapa(&juego.mapa, &tx, &ty, SPRITE_LADO, SPRITE_LADO,
                             0, 0, trig, 8, &ntrig, &dano);
        fprintf(g_traza, "colision suelo %d dano %d triggers",
                g_fisica.en_suelo, dano);
        for (k = 0; k < ntrig; k++) fprintf(g_traza, " %d", trig[k]);
        fprintf(g_traza, "\n");
    }
    if (!g_modo_vm) return;
    n = eventos_num(&g_eventos);
    fprintf(g_traza, "vm");
    for (i = 0; i < n; i++) {
        const EventoScript *s = eventos_script(&g_eventos, i);
        fprintf(g_traza, " %d:%d:%d:%d", i, s->maquina.estado,
                s->maquina.pc, s->maquina.espera);
    }
    fprintf(g_traza, "\n");
    traza_entidades();
    traza_escenas();
}

/* ------------------------------------------------------------------ */
/* Actualizar y renderizar un fotograma                                */
/* ------------------------------------------------------------------ */

/* Reinicia la escena actual: borra las entidades, instancia otra vez los
 * objetos de la escena y deja los scripts a cero. El proximo fotograma
 * vuelve a disparar inicio/escena. */
static void reiniciar_escena_juego(LoboEstado *st) {
    g_eventos.reiniciar = 0;
    entidades_vaciar();
    instanciar_escena(g_eventos.escena);
    entidades_fin_fotograma();
    eventos_escena_iniciar(&g_eventos, st);
}

static void actualizar(void) {
    unsigned e = juego.entrada;

    juego.fotograma++;

    /* Escape cierra siempre, alla donde se mueva el lobo. */
    if (e & MOTOR_ENT_CERRAR) juego.corriendo = 0;

    if (g_modo_vm && eventos_num(&g_eventos) > 0) {
        /* Modo VM (fases 25 a 27): el planificador reparte los eventos;
         * cada script conserva su pc, su pila y su espera, y un script
         * esperando no frena a los demas. */
        LoboEstado st;
        estado_preparar(&st);
        eventos_fotograma(&g_eventos, &st, juego.fotograma, juego.entrada,
                          juego.entrada_anterior);
        juego.entrada_anterior = juego.entrada;
        /* Las bajas se aplican cuando ya no queda ningun script del
         * fotograma mirando la entidad. */
        entidades_fin_fotograma();
        if (g_eventos.reiniciar) {
            g_escenas.reinicios++;
            reiniciar_escena_juego(&st);
        }
        if (eventos_cerrar(&g_eventos)) juego.corriendo = 0;
        aplicar_fisica(&st);
        juego.x = st.x;
        juego.y = st.y;
        escenas_fotograma();
        return;
    }

    if (g_num_scripts > 0) {
        /* Modo scripts (fase 8): el codigo lobo de los botones
         * FUNCIONES manda y el movimiento por defecto queda fuera. */
        LoboEstado st;
        int i;
        estado_preparar(&st);
        for (i = 0; i < g_num_scripts; i++) {
            lobo_ejecutar(&g_programas[g_scripts[i].prog], &st,
                          &g_scripts[i].maquina);
            if (g_scripts[i].maquina.cerrar) juego.corriendo = 0;
        }
        aplicar_fisica(&st);
        juego.x = st.x;
        juego.y = st.y;
        escenas_fotograma();
        return;
    }

    /* Movimiento por defecto (sin scripts en el proyecto). */
    {
        int vx = 0, vy = 0;
        if (e & MOTOR_ENT_IZQUIERDA) vx -= g_velocidad;
        if (e & MOTOR_ENT_DERECHA)   vx += g_velocidad;
        if (e & MOTOR_ENT_ARRIBA)    vy -= g_velocidad;
        if (e & MOTOR_ENT_ABAJO)     vy += g_velocidad;

        /* movimiento separado por eje para deslizar contra los tiles */
        if (vx) {
            juego.x += vx;
            if (choca_con_mapa(juego.x, juego.y)) juego.x -= vx;
        }
        if (vy) {
            juego.y += vy;
            if (choca_con_mapa(juego.x, juego.y)) juego.y -= vy;
        }
    }
    escenas_fotograma();
}

/* ------------------------------------------------------------------ */
/* Render provisional de entidades (rectangulo de color con su id)    */
/* ------------------------------------------------------------------ */

/* Fuente 3x5 de digitos (una fila por byte, 3 bits de ancho). */
static const unsigned char g_digitos[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
    {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 2, 2},
    {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}
};

/* Rellena un rectangulo con recorte por los bordes del framebuffer. */
static void rellenar_rect(int x, int y, int w, int h, u32 color) {
    int i, j;
    for (j = 0; j < h; j++) {
        int py = y + j;
        if (py < 0 || py >= juego.fb_alto) continue;
        for (i = 0; i < w; i++) {
            int px = x + i;
            if (px < 0 || px >= juego.fb_ancho) continue;
            juego.fb[py * juego.fb_ancho + px] = color;
        }
    }
}

/* Mezcla entera de dos colores 0xAARRGGBB; el alpha entrante va aparte. */
static u32 mezclar_color(u32 dst, u32 src, int alpha) {
    int a = alpha, inv, r, g, b;
    if (a < 0) a = 0;
    if (a > 255) a = 255;
    inv = 255 - a;
    r = (((int)((dst >> 16) & 0xFFu) * inv +
          (int)((src >> 16) & 0xFFu) * a) / 255) & 0xFF;
    g = (((int)((dst >> 8) & 0xFFu) * inv +
          (int)((src >> 8) & 0xFFu) * a) / 255) & 0xFF;
    b = (((int)(dst & 0xFFu) * inv +
          (int)(src & 0xFFu) * a) / 255) & 0xFF;
    return 0xFF000000u | ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}

static void rellenar_rect_alpha(int x, int y, int w, int h, u32 color,
                                int alpha) {
    int i, j;
    if (alpha <= 0) return;
    if (alpha > 255) alpha = 255;
    for (j = 0; j < h; j++) {
        int py = y + j;
        if (py < 0 || py >= juego.fb_alto) continue;
        for (i = 0; i < w; i++) {
            int px = x + i, indice;
            if (px < 0 || px >= juego.fb_ancho) continue;
            indice = py * juego.fb_ancho + px;
            juego.fb[indice] = mezclar_color(juego.fb[indice], color, alpha);
        }
    }
}

/* Opacidad de la transicion: sube mientras cubre y baja al descubrir. */
static int transicion_alpha(void) {
    int mitad, parte;
    if (!g_escenas.trans.activo || g_escenas.trans.duracion <= 0) return 0;
    if (g_escenas.trans.fase == 1) {
        mitad = (g_escenas.trans.duracion + 1) / 2;
        return 255 * g_escenas.trans.transcurrido / mitad;
    }
    mitad = (g_escenas.trans.duracion + 1) / 2;
    parte = g_escenas.trans.duracion - mitad;
    if (parte <= 0) return 0;
    return 255 * (g_escenas.trans.duracion -
                  g_escenas.trans.transcurrido) / parte;
}

/* Capas de la fase 41 sobre el fotograma ya terminado. Sin --escenas los
 * efectos estan a cero y esta funcion no escribe ni un pixel. */
static void aplicar_efectos(void) {
    int a;
    u32 color;
    if (!g_escenas_activo || !juego.fb) return;
    if (g_escenas.trans.activo) {
        a = transicion_alpha();
        color = g_escenas.trans.tipo == 2 ? 0x00FFFFFFu : 0x00000000u;
        rellenar_rect_alpha(0, 0, juego.fb_ancho, juego.fb_alto, color, a);
    }
    if (g_escenas.flash_restantes > 0 && g_escenas.flash_duracion > 0) {
        a = 255 * g_escenas.flash_restantes / g_escenas.flash_duracion;
        rellenar_rect_alpha(0, 0, juego.fb_ancho, juego.fb_alto,
                            g_escenas.flash_color, a);
    }
    if (g_escenas.fade_restantes > 0 && g_escenas.fade_alpha > 0)
        rellenar_rect_alpha(0, 0, juego.fb_ancho, juego.fb_alto,
                            g_escenas.fade_color, g_escenas.fade_alpha);
}

/* Digito de 3x5 a escala 2 (6x10 pixeles). */
static void dibujar_digito(int x, int y, int d) {
    int fy, fx;
    if (d < 0 || d > 9) return;
    for (fy = 0; fy < 5; fy++)
        for (fx = 0; fx < 3; fx++)
            if (g_digitos[d][fy] & (1 << (2 - fx)))
                rellenar_rect(x + fx * 2, y + fy * 2, 2, 2, 0xFFFFFFFFu);
}

/* Id de la entidad encima del rectangulo (digitos en blanco). */
static void dibujar_id(int x, int y, int id) {
    char num[12];
    int n = 0, i;
    if (id < 0) return;
    do {
        num[n++] = (char)('0' + id % 10);
        id /= 10;
    } while (id && n < (int)sizeof(num));
    for (i = 0; i < n; i++)
        dibujar_digito(x + i * 8, y, num[n - 1 - i] - '0');
}

/* Dibuja las entidades vivas en el orden de capa e id. Con traje cargado
 * se usa el render de la fase 30; sin el se mantiene el rectangulo de
 * color con el id de las fases anteriores. */
static void dibujar_entidades(int cam_x, int cam_y, int zoom) {
    static const u32 colores[8] = {
        0xFFC84A4Au, 0xFF4AA85Au, 0xFF4A72C8u, 0xFFC89A3Au,
        0xFF9A4AC8u, 0xFF3AA8A8u, 0xFFC85A9Au, 0xFF7A8A3Au
    };
    int orden[ENTIDADES_MAX];
    int n = entidades_orden(orden, ENTIDADES_MAX);
    int i;
    for (i = 0; i < n; i++) {
        Entidad *e = entidad_indice(orden[i]);
        RenderSeleccion sel;
        int sx, sy, w, h;
        if (!e || !e->visible) continue;
        if (g_escenas_activo && g_escenas.parpadeo_id == e->id &&
            g_escenas.parpadeo_periodo > 0 &&
            (juego.fotograma / g_escenas.parpadeo_periodo) % 2 == 1)
            continue;
        sx = (e->x - cam_x) * zoom;
        sy = (e->y - cam_y) * zoom;

        if (render_seleccionar(&g_l3, e, juego.fotograma, &sel)) {
            if (g_render_traza && g_traza)
                fprintf(g_traza,
                        "render entidad %d traje %d cuadro %d capa %d "
                        "x %d y %d\n",
                        e->id, sel.indice, sel.cuadro, sel.capa, sx, sy);
            render_dibujar(juego.fb, juego.fb_ancho, juego.fb_alto,
                           &sel, sx, sy, zoom, &g_render_info);
            continue;
        }

        w = (e->w > 0 ? e->w : 16) * zoom;
        h = (e->h > 0 ? e->h : 16) * zoom;
        rellenar_rect(sx, sy, w, h, colores[(e->id - 1) & 7]);
        /* borde oscuro para separar entidades contiguas */
        rellenar_rect(sx, sy, w, 1, 0xFF101010u);
        rellenar_rect(sx, sy + h - 1, w, 1, 0xFF101010u);
        rellenar_rect(sx, sy, 1, h, 0xFF101010u);
        rellenar_rect(sx + w - 1, sy, 1, h, 0xFF101010u);
        dibujar_id(sx + 2, sy + 2, e->id);
    }
}

static void renderizar(void) {
    MotorMapa *m = &juego.mapa;
    int cam_x = 0, cam_y = 0;
    int zoom = g_camara.zoom;
    MotorImagen sprite;
    int dx, dy, paso, radio;

    if (zoom < 1) zoom = 1;
    render_camara_calcular(&g_camara, juego.fb_ancho, juego.fb_alto,
                           juego.x + SPRITE_LADO / 2,
                           juego.y + SPRITE_LADO / 2, &cam_x, &cam_y);
    /* Suavizado entero de camara: ni un solo pixel de diferencia en las
     * capturas antiguas porque solo se activa con --escenas. */
    if (g_escenas_activo && g_escenas.suave > 1) {
        if (!g_escenas.cam_listo) {
            g_escenas.cam_x = cam_x;
            g_escenas.cam_y = cam_y;
            g_escenas.cam_listo = 1;
        } else {
            dx = cam_x - g_escenas.cam_x;
            dy = cam_y - g_escenas.cam_y;
            if (dx != 0) {
                paso = (dx > 0 ? dx : -dx) / g_escenas.suave;
                if ((dx > 0 ? dx : -dx) % g_escenas.suave) paso++;
                if (paso < 1) paso = 1;
                g_escenas.cam_x += dx > 0 ? paso : -paso;
                if ((dx > 0 && g_escenas.cam_x > cam_x) ||
                    (dx < 0 && g_escenas.cam_x < cam_x))
                    g_escenas.cam_x = cam_x;
            }
            if (dy != 0) {
                paso = (dy > 0 ? dy : -dy) / g_escenas.suave;
                if ((dy > 0 ? dy : -dy) % g_escenas.suave) paso++;
                if (paso < 1) paso = 1;
                g_escenas.cam_y += dy > 0 ? paso : -paso;
                if ((dy > 0 && g_escenas.cam_y > cam_y) ||
                    (dy < 0 && g_escenas.cam_y < cam_y))
                    g_escenas.cam_y = cam_y;
            }
        }
        cam_x = g_escenas.cam_x;
        cam_y = g_escenas.cam_y;
    }
    if (g_escenas_activo && g_escenas.temblor_restantes > 0 &&
        g_escenas.temblor_intensidad > 0) {
        radio = g_escenas.temblor_intensidad;
        if (radio > 32) radio = 32;
        cam_x += ((juego.fotograma * 37 + 11) % (2 * radio + 1)) - radio;
        cam_y += ((juego.fotograma * 53 + 7) % (2 * radio + 1)) - radio;
    }
    memset(&g_render_info, 0, sizeof(g_render_info));
    g_render_info.cam_x = cam_x;
    g_render_info.cam_y = cam_y;

    /* Sin mapa el proyecto corre igual: se pinta el fondo y las
     * entidades (fase 26), que no necesitan tiles. */
    if (!juego.mapa_usado[0]) {
        motor_limpiar(juego.fb, juego.fb_ancho, juego.fb_alto, g_fondo);
        dibujar_entidades(cam_x, cam_y, zoom);
    } else {
        motor_limpiar(juego.fb, juego.fb_ancho, juego.fb_alto, 0x00101826u);
        /* Escalar el lado del tile y la camara por el zoom entero es
         * equivalente a dibujar el mapa y ampliarlo en pantalla. */
        motor_mapa_render(juego.fb, juego.fb_ancho, juego.fb_alto,
                          cam_x * zoom, cam_y * zoom, m->lado * zoom,
                          m->dibujo_celdas ? m->dibujo_celdas : m->celdas,
                          m->dibujo_celdas ? m->num_dibujo : m->num_celdas,
                          m->imagenes);
    }

    sprite.pixeles = juego.sprite;
    sprite.ancho = SPRITE_LADO;
    sprite.alto = SPRITE_LADO;
    /* El sprite clasico solo se pinta cuando hay mapa, igual que en las
     * capturas de las fases anteriores: las pruebas VM sin mapa esperan
     * unicamente el fondo y las entidades. */
    if (juego.mapa_usado[0]) {
        if (zoom == 1) {
            motor_blit_alpha(juego.fb, juego.fb_ancho, juego.fb_alto,
                             juego.x - cam_x, juego.y - cam_y,
                             juego.sprite, SPRITE_LADO, SPRITE_LADO);
        } else {
            render_blit(juego.fb, juego.fb_ancho, juego.fb_alto,
                        juego.x - cam_x, juego.y - cam_y,
                        &sprite, 0, 0, SPRITE_LADO, SPRITE_LADO,
                        0, 0, 0, 0, 0, zoom);
        }
    }
    if (juego.mapa_usado[0]) dibujar_entidades(cam_x, cam_y, zoom);

    /* Panel del depurador sobre la vista Play. Solo con --panel-debug,
     * para que las capturas de las suites anteriores no cambien. */
    if (panel_debug_esta_activo())
        panel_debug_dibujar(juego.fb, juego.fb_ancho, juego.fb_alto,
                            &g_memoria, &g_l3);

    /* HUD sobre todo lo demas: la franja superior y el dialogo inferior
     * dejan el mundo visible en el centro de la pantalla (fase 38). */
    if (g_hud_activo)
        motor_hud_dibujar(juego.fb, juego.fb_ancho, juego.fb_alto);

    /* Los efectos de la fase 41 van encima de todo, pero solo existen
     * cuando un script los ha pedido con --escenas. */
    aplicar_efectos();

    perfil_cache_render();
    if (g_render_traza && g_traza)
        fprintf(g_traza,
                "render cam %d %d zoom %d blits %d recortados %d "
                "saltados %d\n",
                cam_x, cam_y, zoom, g_render_info.blits,
                g_render_info.recortados, g_render_info.saltados);
}

/* ------------------------------------------------------------------ */
/* Modo ventana                                                        */
/* ------------------------------------------------------------------ */

static HDC memdc;
static HBITMAP dib;
static void *dib_pixeles;
static HBITMAP dib_anterior;

static LRESULT CALLBACK proc_ventana(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) DestroyWindow(hwnd);
        return 0;
    case WM_MOUSEWHEEL:
        /* Fase 37: acumula pasos de rueda hasta el siguiente fotograma. */
        g_rueda_acum += (int)(short)HIWORD(wp) / WHEEL_DELTA;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static unsigned leer_teclado(void) {
    unsigned e = 0;
    if (GetAsyncKeyState(VK_LEFT) & 0x8000)  e |= MOTOR_ENT_IZQUIERDA;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) e |= MOTOR_ENT_DERECHA;
    if (GetAsyncKeyState(VK_UP) & 0x8000)    e |= MOTOR_ENT_ARRIBA;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)  e |= MOTOR_ENT_ABAJO;
    if (GetAsyncKeyState('W') & 0x8000) e |= MOTOR_ENT_ARRIBA;
    if (GetAsyncKeyState('S') & 0x8000) e |= MOTOR_ENT_ABAJO;
    if (GetAsyncKeyState('A') & 0x8000) e |= MOTOR_ENT_IZQUIERDA;
    if (GetAsyncKeyState('D') & 0x8000) e |= MOTOR_ENT_DERECHA;
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) e |= MOTOR_ENT_SALTO;
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) e |= MOTOR_ENT_CERRAR;
    return e;
}

/* Fase 37: rellena las 32 ranuras y el raton desde Win32. Solo corre cuando
 * se pidio la entrada extendida, para no alterar ningun juego antiguo. */
static void leer_entrada_ext(void) {
    MotorEntrada *e = &g_entrada_ext;
    POINT pt;
    int i, nx, ny;

    if (!g_entrada_extactiva) return;
    pt.x = 0;
    pt.y = 0;
    motor_entrada_poner(e, ENT_IZQUIERDA, GetAsyncKeyState(VK_LEFT) & 0x8000);
    motor_entrada_poner(e, ENT_DERECHA,  GetAsyncKeyState(VK_RIGHT) & 0x8000);
    motor_entrada_poner(e, ENT_ARRIBA,   GetAsyncKeyState(VK_UP) & 0x8000);
    motor_entrada_poner(e, ENT_ABAJO,    GetAsyncKeyState(VK_DOWN) & 0x8000);
    motor_entrada_poner(e, ENT_ESPACIO,  GetAsyncKeyState(VK_SPACE) & 0x8000);
    motor_entrada_poner(e, ENT_ENTER,    GetAsyncKeyState(VK_RETURN) & 0x8000);
    for (i = ENT_A; i < MOTOR_TECLAS_MAX; i++)
        motor_entrada_poner(e, i,
                            GetAsyncKeyState('A' + (i - ENT_A)) & 0x8000);

    nx = e->raton_x;
    ny = e->raton_y;
    if (g_ventana_ext && GetCursorPos(&pt)) {
        ScreenToClient(g_ventana_ext, &pt);
        nx = (int)pt.x;
        ny = (int)pt.y;
    }
    e->raton_dx = nx - e->raton_x;
    e->raton_dy = ny - e->raton_y;
    e->raton_x = nx;
    e->raton_y = ny;
    e->raton_botones = 0;
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) e->raton_botones |= 1u;
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) e->raton_botones |= 2u;
    if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) e->raton_botones |= 4u;
    e->raton_rueda = g_rueda_acum;
    g_rueda_acum = 0;
    if (g_ventana_ext) {
        RECT rc;
        e->raton_dentro = GetClientRect(g_ventana_ext, &rc) &&
                          pt.x >= rc.left && pt.x < rc.right &&
                          pt.y >= rc.top && pt.y < rc.bottom;
    }
}

static int correr_ventana(HINSTANCE instancia) {
    WNDCLASSA clase;
    HWND hwnd;
    MSG msg;
    BITMAPINFO bmi;
    HDC pantalla;
    RECT rect;
    char titulo[MAX_PATH + 64];
    DWORD t0;

    memset(&clase, 0, sizeof(clase));
    clase.lpfnWndProc = proc_ventana;
    clase.hInstance = instancia;
    clase.lpszClassName = "EGINE_JUEGO";
    clase.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassA(&clase)) return 1;

    rect.left = 0; rect.top = 0;
    rect.right = g_ancho; rect.bottom = g_alto;
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    _snprintf(titulo, sizeof(titulo), "egine - LobosWolfos 2000 - %s - %s",
              activo_proyecto_nombre(),
              juego.mapa_usado[0] ? juego.mapa_usado : "sin mapa");
    titulo[sizeof(titulo) - 1] = '\0';
    hwnd = CreateWindowA("EGINE_JUEGO", titulo, WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         rect.right - rect.left, rect.bottom - rect.top,
                         NULL, NULL, instancia, NULL);
    if (!hwnd) return 1;
    g_ventana_ext = hwnd;

    /* framebuffer: DIB de 32 bits top-down, igual que el editor */
    pantalla = GetDC(NULL);
    memdc = CreateCompatibleDC(pantalla);
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_ancho;
    bmi.bmiHeader.biHeight = -g_alto;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    dib = CreateDIBSection(pantalla, &bmi, DIB_RGB_COLORS, &dib_pixeles,
                           NULL, 0);
    ReleaseDC(NULL, pantalla);
    if (!memdc || !dib) return 1;
    dib_anterior = SelectObject(memdc, dib);
    juego.fb = (u32 *)dib_pixeles;
    juego.fb_ancho = g_ancho;
    juego.fb_alto = g_alto;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    juego.corriendo = 1;
    while (juego.corriendo) {
        t0 = GetTickCount();
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) juego.corriendo = 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        juego.entrada = leer_teclado();
        leer_entrada_ext();
        motor_hud_fotograma(g_entrada_extactiva ? &g_entrada_ext : NULL);
        motor_audio_fotograma();
        perfil_iniciar();
        actualizar();
        if (g_entrada_extactiva) motor_entrada_siguiente(&g_entrada_ext);
        if (!juego.corriendo) break;
        traza_fotograma();
        renderizar();
        perfil_cache_render();
        perfil_fin();
        {
            HDC hdc = GetDC(hwnd);
            BitBlt(hdc, 0, 0, g_ancho, g_alto, memdc, 0, 0, SRCCOPY);
            ReleaseDC(hwnd, hdc);
        }
        /* Fotograma sincronizado al ritmo del proyecto (fase 14). Sin
         * limite, solo se cede el turno para no quemar la CPU. */
        if (g_limitar) {
            DWORD por_foto = (DWORD)(1000 / g_fps);
            if (por_foto < 1) por_foto = 1;
            while (GetTickCount() - t0 < por_foto) Sleep(1);
        } else {
            Sleep(0);
        }
    }

    SelectObject(memdc, dib_anterior);
    DeleteObject(dib);
    DeleteDC(memdc);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Depurador: hooks, ordenes y replay (fase 34)                         */
/* ------------------------------------------------------------------ */

static void cb_depurador_instruccion(int pc, int error) {
    depurador_instruccion(pc, error);
}

static int cb_depurador_parada(void) {
    return depurador_cortar_instrucciones();
}

/* Aplica la semilla del depurador a todas las maquinas del proyecto. */
static void aplicar_semilla_debug(int valor) {
    int i, n = eventos_num(&g_eventos);
    for (i = 0; i < n && i < EVENTOS_SCRIPTS_MAX; i++)
        g_eventos.scripts[i].maquina.semilla = valor;
}

/* Ejecuta la siguiente orden del guion. Devuelve 0 al agotarse. */
static int procesar_orden_debug(void) {
    char linea[DEPURADOR_ORDEN_MAX];
    char *p, *fin;
    int n;

    if (!g_ordenes) return 0;
    while (fgets(linea, sizeof(linea), g_ordenes)) {
        p = linea;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '\r' || *p == '\n' || *p == '#') continue;
        fin = p + strlen(p);
        while (fin > p && (fin[-1] == '\r' || fin[-1] == '\n')) *--fin = '\0';
        if (strncmp(p, "pausar", 6) == 0) {
            depurador_pausar();
        } else if (strncmp(p, "continuar", 9) == 0) {
            depurador_paso(DEPURADOR_PASO_LIBRE, 0);
            depurador_reanudar();
        } else if (strncmp(p, "paso_instruccion", 16) == 0) {
            n = 1;
            sscanf(p + 16, "%d", &n);
            depurador_paso(DEPURADOR_PASO_INSTRUCCION, n);
            depurador_reanudar();
        } else if (strncmp(p, "paso", 4) == 0) {
            depurador_paso(DEPURADOR_PASO_BLOQUE, 1);
            depurador_reanudar();
        } else if (strncmp(p, "observar", 8) == 0) {
            p += 8;
            while (*p == ' ' || *p == '\t') p++;
            depurador_observar(p);
        } else if (strncmp(p, "volcar", 6) == 0) {
            depurador_volcar(g_ruta_debug, &g_memoria, &g_l3, 0);
        } else if (strncmp(p, "break_bloque", 12) == 0) {
            n = 0;
            sscanf(p + 12, "%d", &n);
            depurador_break_bloque(n);
        } else if (strncmp(p, "break_evento", 12) == 0) {
            n = 0;
            sscanf(p + 12, "%d", &n);
            depurador_break_evento(n);
        } else if (strncmp(p, "semilla", 7) == 0) {
            n = 12345;
            sscanf(p + 7, "%d", &n);
            depurador_semilla(n);
            aplicar_semilla_debug(n);
        } else {
            depurador_consola(p);
        }
        return 1;
    }
    return 0;
}

/* Ejecuta las ordenes previas al primer fotograma hasta encontrar una
 * pausa. Sin esto, la orden `pausar` no tendria efecto hasta el segundo
 * fotograma y no se podria observar el estado inicial. */
static void ordenes_iniciales_debug(void) {
    if (!depurador_esta_activo() || !g_ordenes) return;
    while (!depurador_esta_pausado() && procesar_orden_debug()) {}
}

/* Mantiene el depurador detenido mientras haya ordenes pendientes. */
static void atender_pausa_debug(void) {
    if (!depurador_esta_activo() || !depurador_esta_pausado()) return;
    if (procesar_orden_debug()) {
        if (depurador_esta_pausado()) return;
    } else {
        /* Sin guion: volcar el break y seguir ejecutando. */
        depurador_volcar(g_ruta_debug, &g_memoria, &g_l3, 0);
        depurador_paso(DEPURADOR_PASO_LIBRE, 0);
        depurador_reanudar();
    }
}

/* ------------------------------------------------------------------ */
/* Perfilador, presupuestos y estres (fase 42)                         */
/* ------------------------------------------------------------------ */

/* Reloj en microsegundos. QueryPerformanceCounter existe en Windows
 * 2000; si no da frecuencia se cae al reloj de milisegundos de Win32. */
static unsigned long perfil_reloj(void *ctx) {
    LARGE_INTEGER t;
    (void)ctx;
    if (g_perfil_frecuencia.QuadPart > 0 && QueryPerformanceCounter(&t))
        return (unsigned long)((t.QuadPart * (LONGLONG)1000000) /
                               g_perfil_frecuencia.QuadPart);
    return GetTickCount() * 1000u;
}

/* Marca el principio del fotograma, antes de despachar los scripts. */
static void perfil_iniciar(void) {
    if (!g_perfil) return;
    g_perfil_t0 = perfil_reloj(NULL);
    g_perfil_ins0 = lobo_perfil_instrucciones();
}

/* Cierra el fotograma: totales, aviso de presupuesto y degradacion. La
 * degradacion solo baja el numero de blits; nunca duerme mas ni bloquea
 * el bucle, asi que la ventana sigue respondiendo. */
static void perfil_fin(void) {
    unsigned long us;
    if (!g_perfil) return;
    us = perfil_reloj(NULL);
    g_perfil_ins_foto = lobo_perfil_instrucciones() - g_perfil_ins0;
    g_perfil_us_foto = us >= g_perfil_t0 ? us - g_perfil_t0 : 0;
    g_perfil_fotogramas++;
    g_perfil_us_total += g_perfil_us_foto;
    if (g_perfil_us_foto > g_perfil_us_max) g_perfil_us_max = g_perfil_us_foto;
    g_perfil_blits_foto = g_render_info.blits;
    eventos_perfil_blits(&g_eventos, g_perfil_blits_foto);

    if (g_perfil_us_foto > g_presupuesto_us) {
        g_perfil_excesos++;
        g_perfil_avisos++;
        if (g_perfil_excesos >= 3 && g_perfil_limite_blits > 1) {
            g_perfil_limite_blits /= 2;
            render_blits_limite(g_perfil_limite_blits);
        }
        if (g_traza)
            fprintf(g_traza,
                    "presupuesto: excedido en el fotograma %d %lu us "
                    "(limite %lu)\n",
                    juego.fotograma, g_perfil_us_foto, g_presupuesto_us);
        fprintf(stderr,
                "egine: presupuesto excedido: %lu us en el fotograma %d\n",
                g_perfil_us_foto, juego.fotograma);
    } else if (g_perfil_excesos > 0) {
        g_perfil_excesos--;
        if (g_perfil_excesos == 0) {
            g_perfil_limite_blits = RENDER_BLITS_MAX;
            render_blits_limite(g_perfil_limite_blits);
        }
    }

    if (g_traza)
        fprintf(g_traza,
                "perfil %d instrucciones %lu us %lu blits %d "
                "cache %d/%d limite %d\n",
                juego.fotograma, g_perfil_ins_foto, g_perfil_us_foto,
                g_perfil_blits_foto, g_render_info.cache_aciertos,
                g_render_info.cache_cargas, g_perfil_limite_blits);
}

/* Copia los contadores del cache al RenderInfo del fotograma para que la
 * traza nueva los vea sin cambiar la traza --render de las suites viejas. */
static void perfil_cache_render(void) {
    if (!g_perfil) return;
    render_cache_info(&g_render_info.cache_aciertos,
                      &g_render_info.cache_cargas,
                      &g_render_info.cache_bytes);
}

/* Informe final: totales, memoria del blob/runtime y un renglon por script
 * con instrucciones, microsegundos, blits y disparos. */
static void perfil_volcar(const char *ruta) {
    FILE *f;
    unsigned long blob_bytes = 0, cache_bytes = 0, runtime = 0;
    int archivos = 0, aciertos = 0, cargas = 0, i, n;
    if (!g_perfil || !ruta) return;
    f = fopen(ruta, "wt");
    if (!f) return;

    fprintf(f, "perfil: fotogramas %d us totales %lu us medios %lu us "
            "maximos %lu\n",
            g_perfil_fotogramas, g_perfil_us_total,
            g_perfil_fotogramas ? g_perfil_us_total /
                                  (unsigned long)g_perfil_fotogramas : 0,
            g_perfil_us_max);
    fprintf(f, "perfil: presupuesto %lu avisos %d blits por fotograma %d\n",
            g_presupuesto_us, g_perfil_avisos, g_perfil_blits_foto);

    activo_memoria_blob(&blob_bytes, &archivos);
    render_cache_info(&aciertos, &cargas, &cache_bytes);
    runtime = blob_bytes + cache_bytes + sizeof(Juego) + sizeof(Eventos) +
              sizeof(EscenasRuntime);
    runtime += (unsigned long)entidades_num() * sizeof(Entidad);
    runtime += (unsigned long)juego.mapa.num_celdas * sizeof(MotorCelda);
    runtime += (unsigned long)juego.mapa.num_celdas; /* flags */
    runtime += (unsigned long)juego.mapa.num_celdas * MOTOR_CELDA_TAG_LEN;
    runtime += (unsigned long)juego.mapa.num_imagenes * sizeof(MotorImagen);
    if (juego.mapa.dibujo_celdas != juego.mapa.celdas)
        runtime += (unsigned long)juego.mapa.num_dibujo * sizeof(MotorCelda);
    fprintf(f, "memoria: blob %lu archivos %d trajes %d cargas %d "
            "imagenes %lu runtime %lu\n",
            blob_bytes, archivos, aciertos, cargas, cache_bytes, runtime);
    fprintf(f, "memoria: mapa celdas %d entidades %d tiles %d\n",
            juego.mapa.num_celdas, entidades_num(), juego.mapa.num_imagenes);

    n = eventos_num(&g_eventos);
    for (i = 0; i < n; i++) {
        const PerfilScript *p = eventos_perfil_script(&g_eventos, i);
        const EventoScript *s = eventos_script(&g_eventos, i);
        if (!p) continue;
        fprintf(f, "script %d evento %d tipo %d instrucciones %lu "
                "microsegundos %lu blits %lu disparos %d\n",
                i, s ? s->evento : 0, s ? s->tipo : 0, p->instrucciones,
                p->microsegundos, p->blits, p->disparos);
    }
    fclose(f);
}

/* Genera la carga sintetica de la prueba de estres: entidades repartidas
 * por la pantalla y una rejilla de bloques con una imagen de un solo
 * color. Solo se llama con --estres, nunca en las suites antiguas. */
static void estres_preparar(int entidades, int bloques) {
    MotorMapa *m = &juego.mapa;
    int tipo = -1, i, columnas;

    if (entidades > 0 && g_l3.num_objetos > 0)
        tipo = entidades_tipo_por_objeto(g_l3.objetos[0].id);
    if (tipo < 0) tipo = entidades_tipo_agregar(-1, "estres", g_l3.escena_inicial,
                                                0, 0, 12, 12, 1, 0);
    if (tipo >= 0) {
        for (i = 0; i < entidades && i < ENTIDADES_MAX; i++) {
            /* El cupo de altas por fotograma es 32: se reinicia en lotes
             * para que la prueba de estres alcance las 200 entidades. */
            if (i > 0 && i % ENTIDADES_NUEVAS_FOTO == 0)
                entidades_fin_fotograma();
            entidad_crear(tipo, (i * 37) % (g_ancho > 16 ? g_ancho - 16 : 1),
                          (i * 53) % (g_alto > 16 ? g_alto - 16 : 1));
        }
        entidades_fin_fotograma();
    }

    if (bloques <= 0) return;
    if (bloques > 8192) bloques = 8192;
    columnas = 100;
    if (columnas > bloques) columnas = bloques;
    m->num_celdas = bloques;
    m->lado = 16;
    m->celdas = (MotorCelda *)calloc((size_t)bloques, sizeof(MotorCelda));
    m->flags = (unsigned char *)calloc((size_t)bloques, 1);
    m->tags = (char *)calloc((size_t)bloques, MOTOR_CELDA_TAG_LEN);
    m->imagenes = (MotorImagen *)calloc(1, sizeof(MotorImagen));
    if (!m->celdas || !m->flags || !m->tags || !m->imagenes) {
        motor_mapa_liberar(m);
        return;
    }
    m->num_imagenes = 1;
    m->imagenes[0].ancho = 16;
    m->imagenes[0].alto = 16;
    m->imagenes[0].pixeles = (u32 *)malloc(16 * 16 * sizeof(u32));
    if (!m->imagenes[0].pixeles) {
        motor_mapa_liberar(m);
        return;
    }
    for (i = 0; i < 16 * 16; i++) m->imagenes[0].pixeles[i] = 0xFF203840u;
    for (i = 0; i < bloques; i++) {
        m->celdas[i].x = i % columnas;
        m->celdas[i].y = i / columnas;
        m->celdas[i].imagen = 0;
        m->flags[i] = MOTOR_CELDA_SOLIDO;
    }
    m->dibujo_celdas = m->celdas;
    m->num_dibujo = bloques;
    strcpy(juego.mapa_usado, "estres");
}

/* ------------------------------------------------------------------ */
/* Modo captura (sin ventana, determinista)                            */
/* ------------------------------------------------------------------ */

static int correr_captura(const char *ruta_bmp, int fotogramas) {
    int i = 0;
    juego.fb = (u32 *)malloc((size_t)g_ancho * g_alto * sizeof(u32));
    if (!juego.fb) return 1;
    juego.fb_ancho = g_ancho;
    juego.fb_alto = g_alto;
    if (g_mensaje_ini[0])
        eventos_mensaje(&g_eventos, 0, NULL, g_mensaje_ini);
    while (i < fotogramas && juego.corriendo) {
        unsigned entrada;
        atender_pausa_debug();
        if (depurador_esta_pausado()) continue;
        depurador_fotograma(juego.fotograma);
        entrada = g_tecla_guion;
        if (entrada && g_evento_duracion > 0 &&
            juego.fotograma >= g_evento_duracion)
            entrada = 0;
        if (depurador_tiene_replay()) entrada = depurador_entrada(i, entrada);
        juego.entrada = entrada;
        if (g_grabar_replay) depurador_grabar_entrada(i, entrada);
        /* En captura la CLI fija la entrada: no se sondea Win32. */
        motor_hud_fotograma(g_entrada_extactiva ? &g_entrada_ext : NULL);
        motor_audio_fotograma();
        perfil_iniciar();
        actualizar();
        if (g_entrada_extactiva) motor_entrada_siguiente(&g_entrada_ext);
        traza_fotograma();
        renderizar();
        perfil_cache_render();
        perfil_fin();
        i++; /* las pausas no consumen fotogramas del guion */
    }
    if (g_entidades && g_traza)
        fprintf(g_traza, "entidades final: %d avisos %d creadas %d\n",
                entidades_num(), entidades_avisos(), entidades_creadas());
    if (g_eventos_traza && g_traza)
        fprintf(g_traza, "eventos final: %d avisos %d ejecutados %d\n",
                eventos_num(&g_eventos), eventos_avisos(&g_eventos),
                eventos_ejecutados(&g_eventos));
    /* Sondas de pixel del ultimo fotograma: permiten comprobar posicion,
     * capa, flip y rotacion sin depender solo del hash del BMP. */
    if (g_traza) {
        for (i = 0; i < g_num_pixels; i++) {
            int px = g_pixels[i][0], py = g_pixels[i][1];
            if (px < 0 || py < 0 || px >= juego.fb_ancho ||
                py >= juego.fb_alto)
                fprintf(g_traza, "pixel %d %d fuera\n", px, py);
            else
                fprintf(g_traza, "pixel %d %d %08X\n", px, py,
                        juego.fb[py * juego.fb_ancho + px]);
        }
    }
    if (!activo_guardar_bmp(ruta_bmp, juego.fb, juego.fb_ancho, juego.fb_alto))
        return 1;
    free(juego.fb);
    juego.fb = NULL;
    return 0;
}

/* Volcado de estado final sin ventana (--volcar). Con --entidades
 * detalla cada entidad viva. */
static int volcar_estado(const char *ruta) {
    FILE *f = fopen(ruta, "wt");
    int orden[ENTIDADES_MAX];
    int n, i;
    if (!f) return 1;
    fprintf(f, "fotogramas %d\n", juego.fotograma);
    fprintf(f, "lobo %d %d\n", juego.x, juego.y);
    fprintf(f, "entidades %d avisos %d creadas %d\n", entidades_num(),
            entidades_avisos(), entidades_creadas());
    if (g_modo_vm && g_eventos_traza)
        fprintf(f, "eventos %d avisos %d ejecutados %d\n",
                eventos_num(&g_eventos), eventos_avisos(&g_eventos),
                eventos_ejecutados(&g_eventos));
    if (g_entidades) {
        n = entidades_orden(orden, ENTIDADES_MAX);
        for (i = 0; i < n; i++) {
            Entidad *e = entidad_indice(orden[i]);
            int j, vars;
            if (!e) continue;
            fprintf(f,
                    "entidad %d escena %d x %d y %d w %d h %d visible %d "
                    "capa %d vida %d\n",
                    e->id, e->escena, e->x, e->y, e->w, e->h, e->visible,
                    e->capa, e->vida);
            vars = entidades_tipo_vars(e->tipo);
            for (j = 0; j < vars; j++)
                fprintf(f, "  var \"%s\" %d\n",
                        entidades_tipo_var_nombre(e->tipo, j), e->vars[j]);
        }
    }
    fclose(f);
    return 0;
}

/* Corre N fotogramas sin ventana y volca el estado final (--volcar sin
 * --captura). */
static int correr_volcar(const char *ruta, int fotogramas) {
    int i = 0;
    if (g_mensaje_ini[0])
        eventos_mensaje(&g_eventos, 0, NULL, g_mensaje_ini);
    while (i < fotogramas && juego.corriendo) {
        unsigned entrada;
        atender_pausa_debug();
        if (depurador_esta_pausado()) continue;
        depurador_fotograma(juego.fotograma);
        entrada = g_tecla_guion;
        if (entrada && g_evento_duracion > 0 &&
            juego.fotograma >= g_evento_duracion)
            entrada = 0;
        if (depurador_tiene_replay()) entrada = depurador_entrada(i, entrada);
        juego.entrada = entrada;
        if (g_grabar_replay) depurador_grabar_entrada(i, entrada);
        /* En --volcar tampoco hay ventana: respeta la entrada de la CLI. */
        motor_hud_fotograma(g_entrada_extactiva ? &g_entrada_ext : NULL);
        motor_audio_fotograma();
        perfil_iniciar();
        actualizar();
        if (g_entrada_extactiva) motor_entrada_siguiente(&g_entrada_ext);
        traza_fotograma();
        perfil_fin();
        i++;
    }
    if (g_entidades && g_traza)
        fprintf(g_traza, "entidades final: %d avisos %d creadas %d\n",
                entidades_num(), entidades_avisos(), entidades_creadas());
    if (g_eventos_traza && g_traza)
        fprintf(g_traza, "eventos final: %d avisos %d ejecutados %d\n",
                eventos_num(&g_eventos), eventos_avisos(&g_eventos),
                eventos_ejecutados(&g_eventos));
    return volcar_estado(ruta);
}

/* ------------------------------------------------------------------ */
/* Banco de pruebas de la VM (--vm-test)                              */
/* ------------------------------------------------------------------ */

/* Lee un archivo de texto entero (NULL si no se puede). */
static char *leer_texto(const char *ruta, int *largo) {
    FILE *f = fopen(ruta, "rb");
    char *datos;
    long n;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n < 0 || n > 1024L * 1024) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    datos = (char *)malloc((size_t)n + 1);
    if (!datos) { fclose(f); return NULL; }
    if (n > 0 && fread(datos, 1, (size_t)n, f) != (size_t)n) {
        free(datos);
        fclose(f);
        return NULL;
    }
    datos[n] = '\0';
    *largo = (int)n;
    fclose(f);
    return datos;
}

/* Compila un .lobo en modo VM y ejecuta N fotogramas, volcando en la
 * traza el estado de la maquina despues de cada uno. Sin ventana y sin
 * proyecto: sirve para depurar opcodes y el planificador. */
static int correr_vm_test(const char *ruta_lobo, const char *ruta_traza,
                          int fotogramas, unsigned tecla) {
    char *texto;
    int largo = 0, i, resultado = 0;
    LoboPrograma programa;
    LoboMaquina maquina;
    LoboMemoria memoria;
    LoboEstado st;
    char error[LOBO_ERROR_MAX];
    FILE *traza;

    texto = leer_texto(ruta_lobo, &largo);
    if (!texto) return 1;
    traza = fopen(ruta_traza, "wt");
    if (!traza) { free(texto); return 1; }

    lobo_programa_iniciar(&programa);
    if (!lobo_compilar(texto, &programa, LOBO_MODO_VM, error, sizeof(error))) {
        fprintf(traza, "vm: error %s\n", error);
        fclose(traza);
        free(texto);
        lobo_programa_liberar(&programa);
        return 1;
    }
    free(texto);

    memset(&memoria, 0, sizeof(memoria));
    lobo_maquina_reiniciar(&maquina);
    memset(&st, 0, sizeof(st));
    st.x = 16;
    st.y = 16;
    st.vel = SPRITE_VEL;
    st.entrada = tecla;
    st.entrada_ext = g_entrada_extactiva ? &g_entrada_ext : NULL;
    st.memoria = &memoria;
    st.choca = lobo_choca;
    st.ctx = NULL;
    memset(&g_fisica, 0, sizeof(g_fisica));

    fprintf(traza, "vm: %d instrucciones %d funciones\n", programa.largo,
            programa.num_funciones);
    for (i = 0; i < fotogramas; i++) {
        st.fotograma = i + 1;
        lobo_vm_fotograma(&programa, &st, &maquina);
        fprintf(traza,
                "vm %d estado %d pc %d np %d espera %d x %d y %d msg %d "
                "clones %d err %d\n",
                i + 1, maquina.estado, maquina.pc, maquina.np, maquina.espera,
                st.x, st.y, memoria.mensajes, memoria.clones, maquina.agotado);
        if (st.vel_x || st.vel_y || st.gravedad || st.rebote || st.friccion) {
            /* El banco de pruebas no tiene mapa: la caja parte de (16,16)
             * en cada fotograma para que la traza sea reproducible. */
            st.x = 16;
            st.y = 16;
            int suelo;
            suelo = fisica_paso(NULL, &st.x, &st.y, SPRITE_LADO, SPRITE_LADO,
                                &st.vel_x, &st.vel_y, st.gravedad,
                                st.rebote, st.friccion, &st.en_suelo);
            fprintf(traza,
                    "fisica %d vel_x %d vel_y %d x %d y %d suelo %d\n",
                    i + 1, st.vel_x, st.vel_y, st.x, st.y, suelo);
            (void)suelo;
        }
        if (g_entrada_extactiva) motor_entrada_siguiente(&g_entrada_ext);
    }
    fclose(traza);
    if (g_ruta_volcar_entrada)
        motor_entrada_volcar(&g_entrada_ext, g_ruta_volcar_entrada);
    lobo_programa_liberar(&programa);
    return resultado;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

static int opcion(const char *nombre) {
    return strcmp(nombre, "--colisiones") == 0 ||
           strcmp(nombre, "--logica") == 0 || strcmp(nombre, "--mapa") == 0 ||
           strcmp(nombre, "--logic3") == 0 || strcmp(nombre, "--vm-test") == 0 ||
           strcmp(nombre, "--captura") == 0 ||
           strcmp(nombre, "--volcar") == 0 ||
           strcmp(nombre, "--fotogramas") == 0 ||
           strcmp(nombre, "--tecla") == 0 || strcmp(nombre, "--traza") == 0 ||
           strcmp(nombre, "--evento") == 0 ||
           strcmp(nombre, "--mensaje") == 0 ||
           strcmp(nombre, "--render") == 0 ||
           strcmp(nombre, "--camara") == 0 ||
           strcmp(nombre, "--zoom") == 0 ||
           strcmp(nombre, "--limites") == 0 ||
           strcmp(nombre, "--pixel") == 0 ||
           strcmp(nombre, "--ordenes") == 0 ||
           strcmp(nombre, "--paso") == 0 ||
           strcmp(nombre, "--break") == 0 ||
           strcmp(nombre, "--observar") == 0 ||
           strcmp(nombre, "--volcar-debug") == 0 ||
           strcmp(nombre, "--replay") == 0 ||
           strcmp(nombre, "--grabar-replay") == 0 ||
           strcmp(nombre, "--semilla") == 0 ||
           strcmp(nombre, "--entrada-teclas") == 0 ||
           strcmp(nombre, "--entrada-raton") == 0 ||
           strcmp(nombre, "--volcar-entrada") == 0 ||
           strcmp(nombre, "--vidas") == 0 ||
           strcmp(nombre, "--puntos") == 0 ||
           strcmp(nombre, "--dialogo") == 0 ||
           strcmp(nombre, "--volcar-hud") == 0 ||
           strcmp(nombre, "--audio-sonido") == 0 ||
           strcmp(nombre, "--volumen") == 0 ||
           strcmp(nombre, "--volcar-audio") == 0 ||
            strcmp(nombre, "--saves") == 0 ||
            strcmp(nombre, "--volcar-saves") == 0 ||
             strcmp(nombre, "--volcar-escenas") == 0 ||
             strcmp(nombre, "--presupuesto") == 0 ||
             strcmp(nombre, "--volcar-perfil") == 0 ||
             strcmp(nombre, "--estres") == 0;
}

int WINAPI WinMain(HINSTANCE instancia, HINSTANCE prev, LPSTR cmdline, int show) {
    const char *ruta_logica = NULL, *ruta_mapa = NULL, *ruta_bmp = NULL;
    const char *ruta_logic3 = NULL, *ruta_vm_test = NULL;
    const char *ruta_traza = NULL, *ruta_volcar = NULL;
    const char *ruta_ordenes = NULL, *ruta_replay = NULL;
    int fotogramas = 60;
    unsigned tecla = 0;
    int i;
    int resultado;
    (void)prev; (void)cmdline; (void)show;

    depurador_iniciar();
    motor_entrada_iniciar(&g_entrada_ext);
    motor_hud_limpiar();
    motor_audio_limpiar();
    motor_save_limpiar();
    for (i = 1; i < __argc; i++) {
        if (strcmp(__argv[i], "--entidades") == 0) {
            g_entidades = 1;
            continue;
        }
        if (strcmp(__argv[i], "--colisiones") == 0) {
            g_colisiones_traza = 1;
            continue;
        }
        if (strcmp(__argv[i], "--render") == 0) {
            g_render_traza = 1;
            continue;
        }
        if (strcmp(__argv[i], "--eventos") == 0) {
            g_eventos_traza = 1;
            continue;
        }
        if (strcmp(__argv[i], "--depurar") == 0) {
            depurador_activar(1);
            continue;
        }
        if (strcmp(__argv[i], "--panel-debug") == 0) {
            panel_debug_activar(1);
            depurador_activar(1);
            continue;
        }
        if (strcmp(__argv[i], "--hud") == 0) {
            g_hud_activo = 1;
            motor_hud_activar(1);
            continue;
        }
        if (strcmp(__argv[i], "--audio") == 0) {
            g_audio_activo = 1;
            motor_audio_activar(1);
            continue;
        }
        if (strcmp(__argv[i], "--escenas") == 0) {
            g_escenas_activo = 1;
            continue;
        }
        if (strcmp(__argv[i], "--perfil") == 0) {
            g_perfil = 1;
            continue;
        }
        if (strcmp(__argv[i], "--comparar") == 0 && i + 2 < __argc) {
            g_comparar_a = __argv[++i];
            g_comparar_b = __argv[++i];
            continue;
        }
        if (opcion(__argv[i]) && i + 1 < __argc) {
            const char *v = __argv[++i];
            if (strcmp(__argv[i - 1], "--logica") == 0) ruta_logica = v;
            else if (strcmp(__argv[i - 1], "--logic3") == 0) ruta_logic3 = v;
            else if (strcmp(__argv[i - 1], "--vm-test") == 0) ruta_vm_test = v;
            else if (strcmp(__argv[i - 1], "--mapa") == 0) ruta_mapa = v;
            else if (strcmp(__argv[i - 1], "--captura") == 0) ruta_bmp = v;
            else if (strcmp(__argv[i - 1], "--volcar") == 0) ruta_volcar = v;
            else if (strcmp(__argv[i - 1], "--traza") == 0) ruta_traza = v;
            else if (strcmp(__argv[i - 1], "--mensaje") == 0) {
                copiar_texto(g_mensaje_ini, sizeof(g_mensaje_ini), v);
            } else if (strcmp(__argv[i - 1], "--evento") == 0) {
                g_evento_duracion = atoi(v);
            } else if (strcmp(__argv[i - 1], "--fotogramas") == 0)
                fotogramas = atoi(v);
            else if (strcmp(__argv[i - 1], "--tecla") == 0) {
                if (strcmp(v, "izquierda") == 0) tecla = MOTOR_ENT_IZQUIERDA;
                else if (strcmp(v, "derecha") == 0) tecla = MOTOR_ENT_DERECHA;
                else if (strcmp(v, "arriba") == 0) tecla = MOTOR_ENT_ARRIBA;
                else if (strcmp(v, "abajo") == 0) tecla = MOTOR_ENT_ABAJO;
                else if (strcmp(v, "saltar") == 0) tecla = MOTOR_ENT_SALTO;
                else if (strcmp(v, "cerrar") == 0) tecla = MOTOR_ENT_CERRAR;
            } else if (strcmp(__argv[i - 1], "--camara") == 0) {
                g_camara_objetivo = atoi(v);
            } else if (strcmp(__argv[i - 1], "--zoom") == 0) {
                int z = atoi(v);
                if (z == 1 || z == 2 || z == 4) g_zoom_override = z;
            } else if (strcmp(__argv[i - 1], "--limites") == 0) {
                int a = 0, b = 0, c = 0, d = 0;
                if (sscanf(v, "%d,%d,%d,%d", &a, &b, &c, &d) == 4) {
                    g_limites[0] = a; g_limites[1] = b;
                    g_limites[2] = c; g_limites[3] = d;
                    g_limites_override = 1;
                }
            } else if (strcmp(__argv[i - 1], "--pixel") == 0) {
                int px = 0, py = 0;
                if (g_num_pixels < PIXEL_SONDAS_MAX &&
                    sscanf(v, "%d,%d", &px, &py) == 2) {
                    g_pixels[g_num_pixels][0] = px;
                    g_pixels[g_num_pixels][1] = py;
                    g_num_pixels++;
                }
            } else if (strcmp(__argv[i - 1], "--ordenes") == 0) {
                ruta_ordenes = v;
            } else if (strcmp(__argv[i - 1], "--volcar-debug") == 0) {
                g_ruta_debug = v;
            } else if (strcmp(__argv[i - 1], "--replay") == 0) {
                ruta_replay = v;
            } else if (strcmp(__argv[i - 1], "--grabar-replay") == 0) {
                g_grabar_replay = v;
            } else if (strcmp(__argv[i - 1], "--semilla") == 0) {
                g_semilla = atoi(v);
            } else if (strcmp(__argv[i - 1], "--entrada-teclas") == 0) {
                g_entrada_extactiva = 1;
                motor_entrada_fijar(&g_entrada_ext, v);
            } else if (strcmp(__argv[i - 1], "--entrada-raton") == 0) {
                int rx = 0, ry = 0, bot = 0, rueda = 0, dentro = 0;
                g_entrada_extactiva = 1;
                if (sscanf(v, "%d,%d,%d,%d,%d", &rx, &ry, &bot, &rueda,
                           &dentro) >= 2) {
                    g_entrada_ext.raton_x = rx;
                    g_entrada_ext.raton_y = ry;
                    g_entrada_ext.raton_botones = (unsigned)bot;
                    g_entrada_ext.raton_rueda = rueda;
                    g_entrada_ext.raton_dentro = dentro;
                }
            } else if (strcmp(__argv[i - 1], "--volcar-entrada") == 0) {
                g_ruta_volcar_entrada = v;
            } else if (strcmp(__argv[i - 1], "--vidas") == 0) {
                g_hud_activo = 1;
                motor_hud_activar(1);
                motor_hud_vidas(atoi(v));
            } else if (strcmp(__argv[i - 1], "--puntos") == 0) {
                g_hud_activo = 1;
                motor_hud_activar(1);
                motor_hud_puntos(atoi(v));
            } else if (strcmp(__argv[i - 1], "--dialogo") == 0) {
                g_hud_activo = 1;
                motor_hud_activar(1);
                fijar_dialogo_cli(v);
            } else if (strcmp(__argv[i - 1], "--volcar-hud") == 0) {
                g_ruta_volcar_hud = v;
            } else if (strcmp(__argv[i - 1], "--audio-sonido") == 0) {
                char nombre[AUDIO_NOMBRE_MAX];
                const char *ruta = strchr(v, '=');
                g_audio_activo = 1;
                motor_audio_activar(1);
                if (ruta && ruta > v) {
                    int n = (int)(ruta - v);
                    if (n >= AUDIO_NOMBRE_MAX) n = AUDIO_NOMBRE_MAX - 1;
                    memcpy(nombre, v, (size_t)n);
                    nombre[n] = '\0';
                    motor_audio_cargar(nombre, ruta + 1);
                }
            } else if (strcmp(__argv[i - 1], "--volumen") == 0) {
                g_audio_activo = 1;
                motor_audio_activar(1);
                motor_audio_volumen(atoi(v));
            } else if (strcmp(__argv[i - 1], "--volcar-audio") == 0) {
                g_ruta_volcar_audio = v;
            } else if (strcmp(__argv[i - 1], "--saves") == 0) {
                g_saves_activo = 1;
                motor_save_activar(1);
                motor_save_directorio(v);
            } else if (strcmp(__argv[i - 1], "--volcar-saves") == 0) {
                g_ruta_volcar_saves = v;
            } else if (strcmp(__argv[i - 1], "--volcar-escenas") == 0) {
                g_ruta_volcar_escenas = v;
            } else if (strcmp(__argv[i - 1], "--presupuesto") == 0) {
                int us = atoi(v);
                if (us > 0) g_presupuesto_us = (unsigned long)us;
                g_perfil = 1;
            } else if (strcmp(__argv[i - 1], "--volcar-perfil") == 0) {
                g_ruta_volcar_perfil = v;
                g_perfil = 1;
            } else if (strcmp(__argv[i - 1], "--estres") == 0) {
                int ne = 0, nb = 0;
                if (sscanf(v, "%d,%d", &ne, &nb) >= 1) {
                    g_estres_entidades = ne > 0 ? ne : 0;
                    g_estres_bloques = nb > 0 ? nb : 0;
                }
            } else if (strcmp(__argv[i - 1], "--observar") == 0) {
                depurador_observar(v);
            } else if (strcmp(__argv[i - 1], "--break") == 0) {
                int n = 0;
                if (v[0] == 'b' || v[0] == 'B')
                    sscanf(v + 1, ":%d", &n), depurador_break_bloque(n);
                else if (v[0] == 'e' || v[0] == 'E')
                    sscanf(v + 1, ":%d", &n), depurador_break_evento(n);
            } else if (strcmp(__argv[i - 1], "--paso") == 0) {
                int n = 1;
                if (strncmp(v, "instruccion", 11) == 0) {
                    sscanf(v + 11, ":%d", &n);
                    depurador_paso(DEPURADOR_PASO_INSTRUCCION, n);
                } else {
                    depurador_paso(DEPURADOR_PASO_BLOQUE, 1);
                }
            }
        }
    }
    if (fotogramas < 1) fotogramas = 1;
    if (fotogramas > 100000) fotogramas = 100000;
    if (g_evento_duracion < 0) g_evento_duracion = 0;
    if (g_evento_duracion > 100000) g_evento_duracion = 100000;
    g_tecla_guion = tecla;
    if (ruta_traza) {
        g_traza = fopen(ruta_traza, "wt"); /* si falla: sin traza */
    }

    /* Depurador: la grabacion y el replay activan el modo depuracion. */
    if (ruta_replay) {
        char msg[DEPURADOR_NOMBRE_MAX];
        int sem = g_semilla;
        msg[0] = '\0';
        if (depurador_cargar_replay(ruta_replay, msg, sizeof(msg), &sem) == 0) {
            depurador_activar(1);
            if (msg[0]) copiar_texto(g_mensaje_ini, sizeof(g_mensaje_ini), msg);
            g_semilla = sem;
        }
    }
    depurador_semilla(g_semilla);
    if (ruta_ordenes) {
        g_ordenes = fopen(ruta_ordenes, "rt");
        if (g_ordenes) depurador_activar(1);
    }
    if (g_grabar_replay) {
        depurador_replay_grabar(1);
        depurador_activar(1);
    }
    if (g_ruta_debug) depurador_activar(1);

    /* Comparacion de dos trazas de replay, sin GDI+ ni proyecto. */
    if (g_comparar_a && g_comparar_b) {
        const char *salida = ruta_traza ? ruta_traza : "comparar.txt";
        int r;
        if (g_traza) {
            fclose(g_traza);
            g_traza = NULL;
        }
        r = depurador_comparar(g_comparar_a, g_comparar_b, salida);
        depurador_terminar();
        return r == 2 ? 1 : 0;
    }

    /* Banco de la VM: no necesita GDI+ ni proyecto. */
    if (ruta_vm_test) {
        const char *salida = ruta_traza ? ruta_traza : "vm_test.traza";
        if (g_traza) {
            fclose(g_traza);
            g_traza = NULL;
        }
        return correr_vm_test(ruta_vm_test, salida, fotogramas, tecla);
    }

    if (!activo_gdiplus_iniciar()) {
        MessageBoxA(NULL, "No se pudo iniciar GDI+ (gdiplus.dll)",
                    "egine", MB_ICONERROR);
        return 1;
    }
    render_iniciar(ruta_logic3 ? ruta_logic3 : "");
    if (g_perfil) {
        QueryPerformanceFrequency(&g_perfil_frecuencia);
        render_rapido(1);
    }
    motor_hud_imagen_callback(cb_hud_imagen_dibujar, NULL);

    memset(&juego, 0, sizeof(juego));
    juego.corriendo = 1;
    juego.x = 16;
    juego.y = 16;
    juego.entrada = tecla; /* en captura se mantiene pulsada */
    sprite_crear();

    /* Proyecto incrustado (fase 4): si este exe fue compilado por el
     * editor lleva el proyecto pegado al final. Sin rutas explicitas
     * en la linea de comandos, el proyecto sale del propio exe. */
    if (!ruta_logica && !ruta_mapa && !ruta_logic3 && activo_blob_cargar() > 0) {
        /* Fase 45: si el blob trae el LOGIC3 nativo se ejecutan sus bloques
         * con la VM; si no, se mantiene el proyecto LOGIC2 de siempre. */
        if (activo_existe_virtual("proyecto.logic3"))
            ruta_logic3 = "proyecto.logic3";
        else
            ruta_logica = "proyecto.logic";
    }

    /* Fase 39: los nombres de sonar()/musica() sin --audio-sonido se buscan
     * como sonidos\ nombre.wav junto al proyecto. */
    if (g_audio_activo)
        motor_audio_base(ruta_logic3 ? ruta_logic3
                                     : (ruta_logica ? ruta_logica : ""));

    /* Fase 40: el nombre del save sale del mismo proyecto. Sin --saves no
     * se abre ningun archivo, aunque se calcule la ruta para el volcado. */
    motor_save_base(ruta_logic3 ? ruta_logic3
                                : (ruta_logica ? ruta_logica : ""));

    if (ruta_logic3) {
        char error[MOTOR_L3_ERROR_MAX];
        char ruta_mapa_l3[MAX_PATH];

        if (!motor_l3_cargar(ruta_logic3, &g_l3, error, sizeof(error))) {
            char msg[512];
            _snprintf(msg, sizeof(msg), "No se pudo cargar el LOGIC3:\n%s",
                      error);
            msg[sizeof(msg) - 1] = '\0';
            MessageBoxA(NULL, msg, "egine", MB_ICONERROR);
            if (g_traza) fclose(g_traza);
            return 1;
        }
        g_modo_vm = 1;
        aplicar_ajustes_l3(&g_l3);

        if (g_l3.mapa[0]) {
            if (!resolver_mapa_l3(ruta_logic3, g_l3.mapa, ruta_mapa_l3,
                                  sizeof(ruta_mapa_l3)) ||
                !activo_cargar_mapa(ruta_mapa_l3, &juego.mapa)) {
                char msg[512];
                _snprintf(msg, sizeof(msg),
                          "No se pudo cargar el mapa del LOGIC3:\n%s",
                          g_l3.mapa);
                msg[sizeof(msg) - 1] = '\0';
                MessageBoxA(NULL, msg, "egine", MB_ICONERROR);
                if (g_traza) fclose(g_traza);
                return 1;
            }
            copiar_texto(juego.mapa_usado, sizeof(juego.mapa_usado),
                         ruta_mapa_l3);
        }
        /* entidades de la escena inicial y scripts codigo.lobo de los
         * eventos fotograma: un script cada uno, con la VM resumible */
        entidades_preparar();
        scripts_preparar_l3(ruta_bmp == NULL && ruta_volcar == NULL);
        if (depurador_esta_activo()) {
            lobo_depurador_instruccion(cb_depurador_instruccion);
            lobo_depurador_parada(cb_depurador_parada);
            aplicar_semilla_debug(g_semilla);
            depurador_consola("depurador listo");
        }
    } else if (ruta_mapa) {
        if (!activo_cargar_mapa(ruta_mapa, &juego.mapa)) {
            char msg[512];
            _snprintf(msg, sizeof(msg), "No se pudo cargar el mapa:\n%s",
                      ruta_mapa);
            msg[sizeof(msg) - 1] = '\0';
            MessageBoxA(NULL, msg, "egine", MB_ICONERROR);
            if (g_traza) fclose(g_traza);
            return 1;
        }
        strncpy(juego.mapa_usado, ruta_mapa, sizeof(juego.mapa_usado) - 1);
    } else {
        if (!activo_cargar_proyecto(ruta_logica, &juego.mapa,
                                    juego.mapa_usado,
                                    sizeof(juego.mapa_usado))) {
            MessageBoxA(NULL,
                        "No se pudo cargar el proyecto ni un mapa .mav\n"
                        "(busca un .logic o maps\\*.mav junto al juego)",
                        "egine", MB_ICONERROR);
            if (g_traza) fclose(g_traza);
            return 1;
        }
        /* los botones FUNCIONES con codigo: un script por elemento */
        scripts_preparar(ruta_bmp == NULL);
    }

    /* Fase 42: eventos_cargar reinicia el planificador, asi que el
     * perfilador se engancha despues de compilar los scripts. */
    if (g_perfil) eventos_perfil_activar(&g_eventos, perfil_reloj, NULL);

    /* Fase 42: la carga sintetica se anade despues del proyecto para que
     * los tipos y el mapa reales sigan intactos. */
    estres_preparar(g_estres_entidades, g_estres_bloques);

    /* Ajustes del proyecto (fase 14): resolucion, ritmo, velocidad y
     * fondo, leidos del trailer del .logic. El LOGIC3 ya los aplico. */
    if (!g_modo_vm) aplicar_ajustes();

    /* Camara de la fase 30: la del LOGIC3, con las opciones de linea de
     * comandos por encima. Sin LOGIC3 se mantiene la camara sin limites
     * de siempre (sigue al sprite). */
    render_camara_iniciar(g_modo_vm ? &g_l3 : NULL, &g_camara);
    render_camara_aplicar(&g_camara, g_camara_objetivo, g_zoom_override,
                          g_limites_override ? g_limites : NULL);
    if (g_modo_vm) escenas_preparar(&g_l3, ruta_logic3);

    ordenes_iniciales_debug();
    if (ruta_bmp) {
        buscar_spawn();
        resultado = correr_captura(ruta_bmp, fotogramas);
        if (resultado == 0 && ruta_volcar) resultado = volcar_estado(ruta_volcar);
    } else if (ruta_volcar) {
        buscar_spawn();
        resultado = correr_volcar(ruta_volcar, fotogramas);
    } else {
        buscar_spawn();
        /* Solo la ventana abre waveOut: las capturas son headless. */
        if (g_audio_activo) motor_audio_dispositivo(1);
        resultado = correr_ventana(instancia);
    }
    /* Fase 37: el volcado de entrada se escribe al final del guion, tanto en
     * captura como en --volcar. --vm-test ya lo hace en su propio banco. */
    if (g_ruta_volcar_entrada)
        motor_entrada_volcar(&g_entrada_ext, g_ruta_volcar_entrada);
    if (g_ruta_volcar_hud) motor_hud_volcar(g_ruta_volcar_hud);
    if (g_ruta_volcar_audio) motor_audio_volcar(g_ruta_volcar_audio);
    if (g_ruta_volcar_saves) motor_save_volcar(g_ruta_volcar_saves);
    if (g_ruta_volcar_escenas) escenas_volcar(g_ruta_volcar_escenas);
    perfil_volcar(g_ruta_volcar_perfil);
    if (g_grabar_replay) depurador_guardar_replay(g_grabar_replay);
    if (g_ruta_debug) depurador_volcar(g_ruta_debug, &g_memoria, &g_l3, 1);
    if (g_ordenes) {
        fclose(g_ordenes);
        g_ordenes = NULL;
    }
    if (g_traza) {
        fclose(g_traza);
        g_traza = NULL;
    }
    depurador_terminar();
    render_terminar();
    motor_mapa_liberar(&juego.mapa);
    activo_gdiplus_terminar();
    return resultado;
}
