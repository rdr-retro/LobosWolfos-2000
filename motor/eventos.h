/* eventos.h - planificador de eventos y cola de mensajes (fase 27).
 *
 * El planificador reparte los scripts de un proyecto LOGIC3 en el orden
 * del capitulo 5.2 del plan maestro:
 *
 *   1. inicio al entrar en escena;
 *   2. entrada (teclado) y flancos;
 *   3. fotograma de las entidades activas, por capa e id;
 *   4. tecla mantenida y tecla pulsada/soltada;
 *   5. cada N;
 *   6. tocar (AABB entre entidades, con espera);
 *   7. recibir de la cola de mensajes;
 *   8. clonar de las entidades creadas ese fotograma;
 *   9. fin de fotograma (bajas y contadores).
 *
 * Cada script tiene su propia LoboMaquina, asi que un `esperar` solo deja
 * esperando a ese script. La cola de mensajes no se vacia a ciegas: si el
 * script que debe recibir esta esperando, el mensaje se queda al frente
 * para el siguiente fotograma. El presupuesto de eventos por fotograma
 * (EVENTOS_PRESUPUESTO) evita que un bucle de mensajes cuelgue el juego.
 *
 * C puro: el modulo no abre ventanas ni usa GDI. Lo enlazan game.exe y el
 * editor; la traza de eventos es opcional (--eventos).
 */
#ifndef EVENTOS_H
#define EVENTOS_H

#include <stdio.h>
#include "lobo.h"
#include "logic3_motor.h"

#define EVENTOS_SCRIPTS_MAX  MOTOR_L3_SCRIPTS_MAX
#define EVENTOS_COLA_MAX     256   /* mensajes pendientes */
#define EVENTOS_TEXTO_MAX    MOTOR_L3_NOMBRE_MAX
#define EVENTOS_PRESUPUESTO  512   /* scripts lanzados por fotograma */
#define EVENTOS_TOCAR_ESPERA 15    /* fotogramas entre dos tocar seguidos */

/* Fase 42: medicion por script. `blits` cuenta los blits de los fotogramas
 * en que el script se disparo (el render corre despues del despacho, asi que
 * se atribuye el fotograma completo al conjunto de scripts activos). */
typedef struct PerfilScript {
    unsigned long instrucciones;
    unsigned long microsegundos;
    unsigned long blits;
    int disparos;
    int activo_foto;
} PerfilScript;

/* Reloj del perfilador: devuelve microsegundos. Lo suministra el huesped
 * (game.exe usa QueryPerformanceCounter y el kernel usa el PIT), para que
 * este modulo siga siendo C puro. */
typedef unsigned long (*EventosReloj)(void *ctx);

/* Tipos de evento: los mismos valores que LOGIC3. `llamada` no se despacha,
 * pero conserva su hueco para que `escena_salir` siga siendo el id 10. */
enum {
    EVENTO_INICIO = 1,
    EVENTO_FOTOGRAMA,
    EVENTO_TECLA,
    EVENTO_RECIBIR,
    EVENTO_TOCAR,
    EVENTO_CADA,
    EVENTO_CLONAR,
    EVENTO_ESCENA,
    EVENTO_LLAMADA,
    EVENTO_ESCENA_SALIR
};

/* Un script vivo: metadatos del evento, programa compilado y maquina. */
typedef struct EventoScript {
    LoboPrograma programa;
    LoboMaquina maquina;
    int bloque;
    int objeto;
    int evento;
    int orden;
    int capa;
    int tipo;
    char nombre[EVENTOS_TEXTO_MAX]; /* tecla, mensaje o tag de tocar */
    int flanco;        /* tecla: 0 mantener, 1 pulsar, 2 soltar */
    int objeto_tocar;  /* tocar: id de objeto o -1 */
    int cada;          /* cada: fotogramas entre disparos */
    int escena;        /* escena: id de la escena */
    int contador;      /* cuenta atras de cada N */
    int cooldown;      /* espera de tocar */
} EventoScript;

/* Un mensaje pendiente. `destino` vacio = a todos los objetos. */
typedef struct EventoMensaje {
    char destino[EVENTOS_TEXTO_MAX];
    char texto[EVENTOS_TEXTO_MAX];
    int origen;
} EventoMensaje;

/* Estado del planificador. */
typedef struct Eventos {
    EventoScript scripts[EVENTOS_SCRIPTS_MAX];
    int num_scripts;
    EventoMensaje cola[EVENTOS_COLA_MAX];
    int cola_ini;
    int cola_num;
    int fotograma;
    int eventos_fotograma;
    int ejecutados;      /* total acumulado (para --volcar) */
    int avisos;
    int avisado;         /* ya se aviso del presupuesto en este fotograma */
    int detener_todo;
    int reiniciar;
    int cerrar;
    int iniciado;
    int errores;
    int bloque_error;
    char error[LOBO_ERROR_MAX];
    int escena;
    FILE *traza;   /* --traza; puede ser NULL */
    int detalle;   /* 1 = volcar eventos ejecutados (--eventos) */
    /* --- perfilador (fase 42) --- */
    PerfilScript perfil[EVENTOS_SCRIPTS_MAX];
    int perfil_activo;
    EventosReloj reloj;
    void *reloj_ctx;
} Eventos;

/* Deja el planificador limpio (no libera programas: para eso esta
 * eventos_liberar). */
void eventos_iniciar(Eventos *ev, FILE *traza, int detalle);

/* Compila los scripts de todos los eventos del proyecto. `remapear` es
 * opcional y se llama tras compilar para traducir los indices locales de
 * variables de entidad a los slots globales del juego. Devuelve cuantos
 * scripts quedaron activos. Los que no compilan se avisan en la traza y se
 * saltan, como en la fase 25. */
typedef void (*EventosRemapear)(void *ctx, LoboPrograma *programa);
int eventos_cargar(Eventos *ev, const MotorL3 *proy,
                   EventosRemapear remapear, void *ctx);

/* Suelta los programas compilados. */
void eventos_liberar(Eventos *ev);

/* Arranca (o reinicia) la escena: maquinas a cero, contadores a su valor y
 * eventos inicio/escena de la escena actual. */
void eventos_escena_iniciar(Eventos *ev, LoboEstado *estado);

/* Dispara escena_salir de la escena indicada con las entidades todavia
 * vivas. Debe llamarse antes de vaciar la escena. */
void eventos_escena_salir(Eventos *ev, LoboEstado *estado,
                          int fotograma, int escena);

/* Despacha un fotograma completo. `entrada` son los bits actuales y
 * `anterior` los del fotograma previo (para los flancos). */
void eventos_fotograma(Eventos *ev, LoboEstado *estado, int fotograma,
                       unsigned entrada, unsigned anterior);

/* Encola un mensaje. `destino` puede ser NULL o "" para todos. */
void eventos_mensaje(Eventos *ev, int origen, const char *destino,
                     const char *texto);

/* Callback para LoboEstado: enviar() y enviar_a() de lobo. */
void eventos_enviar_cb(void *ctx, int origen, const char *destino,
                       const char *texto);

/* Callback para LoboEstado: reiniciar_escena() de lobo. */
void eventos_reiniciar_cb(void *ctx);

/* Accesores para la traza y el volcado. */
int eventos_num(const Eventos *ev);
const EventoScript *eventos_script(const Eventos *ev, int i);
int eventos_avisos(const Eventos *ev);
int eventos_ejecutados(const Eventos *ev);
int eventos_cerrar(const Eventos *ev);
unsigned eventos_entrada(const Eventos *ev, const char *nombre);

/* Perfilador (fase 42). Sin eventos_perfil_activar todo sigue como antes:
 * no se llama al reloj ni se cuentan instrucciones. */
void eventos_perfil_activar(Eventos *ev, EventosReloj reloj, void *ctx);
int eventos_perfil_activo(const Eventos *ev);
const PerfilScript *eventos_perfil_script(const Eventos *ev, int i);
void eventos_perfil_blits(Eventos *ev, int blits);

#endif /* EVENTOS_H */
