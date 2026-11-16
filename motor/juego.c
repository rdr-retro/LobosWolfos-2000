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
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "motor.h"
#include "lobo.h"
#include "logic3_motor.h"
#include "entidades.h"
#include "eventos.h"

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

/* Ajustes del proyecto aplicados al motor (fase 14). */
static int g_ancho = VENTANA_ANCHO;
static int g_alto = VENTANA_ALTO;
static int g_fps = FPS_OBJETIVO;
static int g_limitar = 1;
static int g_velocidad = SPRITE_VEL;
static u32 g_fondo = 0x00000000u;

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
    if (archivo_existe(mapa)) {
        copiar_texto(dst, tam, mapa);
        return 1;
    }
    _snprintf(prueba, sizeof(prueba), "maps\\%s", mapa);
    prueba[sizeof(prueba) - 1] = '\0';
    if (archivo_existe(prueba)) {
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
    const MotorMapa *m = &juego.mapa;
    int i;
    for (i = 0; i < m->num_celdas; i++) {
        const MotorCelda *c = &m->celdas[i];
        if (motor_aabb(x, y, w, h,
                       c->x * m->lado, c->y * m->lado, m->lado, m->lado))
            return 1;
    }
    return 0;
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

/* Estado compartido por los scripts de un fotograma: posicion del lobo,
 * teclas, memoria de la VM y callbacks de entidades. */
static void estado_preparar(LoboEstado *st) {
    memset(st, 0, sizeof(*st));
    st->x = juego.x;
    st->y = juego.y;
    st->vel = g_velocidad;
    st->fotograma = juego.fotograma;
    st->entrada = juego.entrada; /* LOBO_TECLA_* == MOTOR_ENT_* */
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
}

/* Compila todos los scripts del LOGIC3 en el planificador de eventos:
 * cada bloque codigo.lobo trae su propio texto y su propia maquina,
 * todos contra la misma memoria compartida (globales, listas y mensajes
 * de la fase 25). */
static void scripts_preparar_l3(int con_ventana) {
    int i, n;

    memset(&g_memoria, 0, sizeof(g_memoria));
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
        if (g_eventos.reiniciar) reiniciar_escena_juego(&st);
        if (eventos_cerrar(&g_eventos)) juego.corriendo = 0;
        juego.x = st.x;
        juego.y = st.y;
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
        juego.x = st.x;
        juego.y = st.y;
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

/* Dibuja las entidades vivas en el orden de capa e id. */
static void dibujar_entidades(int cam_x, int cam_y) {
    static const u32 colores[8] = {
        0xFFC84A4Au, 0xFF4AA85Au, 0xFF4A72C8u, 0xFFC89A3Au,
        0xFF9A4AC8u, 0xFF3AA8A8u, 0xFFC85A9Au, 0xFF7A8A3Au
    };
    int orden[ENTIDADES_MAX];
    int n = entidades_orden(orden, ENTIDADES_MAX);
    int i;
    for (i = 0; i < n; i++) {
        Entidad *e = entidad_indice(orden[i]);
        int sx, sy, w, h;
        if (!e || !e->visible) continue;
        sx = e->x - cam_x;
        sy = e->y - cam_y;
        w = e->w > 0 ? e->w : 16;
        h = e->h > 0 ? e->h : 16;
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
    int cam_x, cam_y;

    /* camara centrada en el sprite */
    cam_x = juego.x + SPRITE_LADO / 2 - juego.fb_ancho / 2;
    cam_y = juego.y + SPRITE_LADO / 2 - juego.fb_alto / 2;

    /* Sin mapa el proyecto corre igual: se pinta el fondo y las
     * entidades (fase 26), que no necesitan tiles. */
    if (!juego.mapa_usado[0]) {
        motor_limpiar(juego.fb, juego.fb_ancho, juego.fb_alto, g_fondo);
        dibujar_entidades(cam_x, cam_y);
        return;
    }

    motor_limpiar(juego.fb, juego.fb_ancho, juego.fb_alto, 0x00101826u);
    motor_mapa_render(juego.fb, juego.fb_ancho, juego.fb_alto,
                      cam_x, cam_y, m->lado, m->celdas, m->num_celdas,
                      m->imagenes);
    motor_blit_alpha(juego.fb, juego.fb_ancho, juego.fb_alto,
                     juego.x - cam_x, juego.y - cam_y,
                     juego.sprite, SPRITE_LADO, SPRITE_LADO);
    dibujar_entidades(cam_x, cam_y);
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
        actualizar();
        if (!juego.corriendo) break;
        traza_fotograma();
        renderizar();
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
/* Modo captura (sin ventana, determinista)                            */
/* ------------------------------------------------------------------ */

static int correr_captura(const char *ruta_bmp, int fotogramas) {
    int i;
    juego.fb = (u32 *)malloc((size_t)g_ancho * g_alto * sizeof(u32));
    if (!juego.fb) return 1;
    juego.fb_ancho = g_ancho;
    juego.fb_alto = g_alto;
    if (g_mensaje_ini[0])
        eventos_mensaje(&g_eventos, 0, NULL, g_mensaje_ini);
    for (i = 0; i < fotogramas && juego.corriendo; i++) {
        if (g_tecla_guion) {
            if (g_evento_duracion > 0 && juego.fotograma >= g_evento_duracion)
                juego.entrada = 0;
            else
                juego.entrada = g_tecla_guion;
        }
        actualizar();
        traza_fotograma();
        renderizar();
    }
    if (g_entidades && g_traza)
        fprintf(g_traza, "entidades final: %d avisos %d creadas %d\n",
                entidades_num(), entidades_avisos(), entidades_creadas());
    if (g_eventos_traza && g_traza)
        fprintf(g_traza, "eventos final: %d avisos %d ejecutados %d\n",
                eventos_num(&g_eventos), eventos_avisos(&g_eventos),
                eventos_ejecutados(&g_eventos));
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
    int i;
    if (g_mensaje_ini[0])
        eventos_mensaje(&g_eventos, 0, NULL, g_mensaje_ini);
    for (i = 0; i < fotogramas && juego.corriendo; i++) {
        if (g_tecla_guion) {
            if (g_evento_duracion > 0 && juego.fotograma >= g_evento_duracion)
                juego.entrada = 0;
            else
                juego.entrada = g_tecla_guion;
        }
        actualizar();
        traza_fotograma();
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
    st.memoria = &memoria;
    st.choca = lobo_choca;
    st.ctx = NULL;

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
    }
    fclose(traza);
    lobo_programa_liberar(&programa);
    return resultado;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

static int opcion(const char *nombre) {
    return strcmp(nombre, "--logica") == 0 || strcmp(nombre, "--mapa") == 0 ||
           strcmp(nombre, "--logic3") == 0 || strcmp(nombre, "--vm-test") == 0 ||
           strcmp(nombre, "--captura") == 0 ||
           strcmp(nombre, "--volcar") == 0 ||
           strcmp(nombre, "--fotogramas") == 0 ||
           strcmp(nombre, "--tecla") == 0 || strcmp(nombre, "--traza") == 0 ||
           strcmp(nombre, "--evento") == 0 ||
           strcmp(nombre, "--mensaje") == 0;
}

int WINAPI WinMain(HINSTANCE instancia, HINSTANCE prev, LPSTR cmdline, int show) {
    const char *ruta_logica = NULL, *ruta_mapa = NULL, *ruta_bmp = NULL;
    const char *ruta_logic3 = NULL, *ruta_vm_test = NULL;
    const char *ruta_traza = NULL, *ruta_volcar = NULL;
    int fotogramas = 60;
    unsigned tecla = 0;
    int i;
    int resultado;
    (void)prev; (void)cmdline; (void)show;

    for (i = 1; i < __argc; i++) {
        if (strcmp(__argv[i], "--entidades") == 0) {
            g_entidades = 1;
            continue;
        }
        if (strcmp(__argv[i], "--eventos") == 0) {
            g_eventos_traza = 1;
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

    memset(&juego, 0, sizeof(juego));
    juego.corriendo = 1;
    juego.x = 16;
    juego.y = 16;
    juego.entrada = tecla; /* en captura se mantiene pulsada */
    sprite_crear();

    /* Proyecto incrustado (fase 4): si este exe fue compilado por el
     * editor lleva el proyecto pegado al final. Sin rutas explicitas
     * en la linea de comandos, el proyecto sale del propio exe. */
    if (!ruta_logica && !ruta_mapa && !ruta_logic3 && activo_blob_cargar() > 0)
        ruta_logica = "proyecto.logic";

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

    /* Ajustes del proyecto (fase 14): resolucion, ritmo, velocidad y
     * fondo, leidos del trailer del .logic. El LOGIC3 ya los aplico. */
    if (!g_modo_vm) aplicar_ajustes();

    if (ruta_bmp) {
        buscar_spawn();
        resultado = correr_captura(ruta_bmp, fotogramas);
        if (resultado == 0 && ruta_volcar) resultado = volcar_estado(ruta_volcar);
    } else if (ruta_volcar) {
        buscar_spawn();
        resultado = correr_volcar(ruta_volcar, fotogramas);
    } else {
        buscar_spawn();
        resultado = correr_ventana(instancia);
    }
    if (g_traza) {
        fclose(g_traza);
        g_traza = NULL;
    }
    return resultado;
}
