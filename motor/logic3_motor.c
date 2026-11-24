/* logic3_motor.c - lector minimo de LOGIC3 para game.exe (fase 25).
 *
 * No es el lector completo del editor (src/logic3.c): aqui solo se saca
 * lo que el runtime necesita para la fase 25, la cabecera del proyecto,
 * el mapa de la primera escena y los scripts codigo.lobo de los eventos
 * fotograma. El resto de secciones se lee lo justo para no desincronizar
 * el parser y se ignora.
 *
 * El parser es linea a linea y tolerante: una linea es un registro, los
 * textos van entre comillas con los escapes \" \\ \n, y las lineas
 * desconocidas se saltan. Asi el motor puede abrir archivos escritos por
 * versiones futuras del editor sin romperse.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logic3_motor.h"
#include "motor.h" /* fase 45: lectura del LOGIC3 incrustado en el blob */

/* ------------------------------------------------------------------ */
/* Errores                                                            */
/* ------------------------------------------------------------------ */

static char g_error[MOTOR_L3_ERROR_MAX];

const char *motor_l3_error(void) { return g_error; }

static int fallo(int linea, const char *motivo) {
    if (linea > 0)
        _snprintf(g_error, sizeof(g_error), "linea %d: %s", linea, motivo);
    else
        _snprintf(g_error, sizeof(g_error), "%s", motivo);
    g_error[sizeof(g_error) - 1] = '\0';
    return 0;
}

/* Igual que fallo, pero copia el mensaje al buffer del llamante. */
static int fallo_en(char *error, int tam_error, int linea, const char *motivo) {
    fallo(linea, motivo);
    if (error && tam_error > 0) {
        strncpy(error, g_error, (size_t)tam_error - 1);
        error[tam_error - 1] = '\0';
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Tokenizador de una linea                                           */
/* ------------------------------------------------------------------ */

/* Una lista de 256 elementos ocupa 4 tokens de cabecera, el tipo y los
 * 256 valores: el toque de la fase 28 obliga a ampliar el tokenizador. */
#define L3_TOKENS_MAX 288

typedef struct {
    int comillas; /* 1 si venia entre comillas */
    int off;      /* desplazamiento en la salida */
} L3Tok;

/* Copia un caracter desescapado a la salida. Devuelve 1 si cabe. */
static int poner(char *salida, int tam, int *usado, char c) {
    if (*usado >= tam - 1) return 0;
    salida[(*usado)++] = c;
    return 1;
}

/* Parte una linea en tokens. Las comillas agrupan espacios y admiten los
 * escapes \" \\ \n \r y \t, los mismos que escribe el editor (fase 28).
 * Devuelve el numero de tokens o -1 si algo no cabe. */
static int tokenizar(const char *linea, int largo, char *salida, int tam_salida,
                     L3Tok *tokens, int max_tokens, int *usado_total) {
    int i = 0, n = 0, usado = 0;

    while (i < largo) {
        char c = linea[i];
        if (c == ' ' || c == '\t' || c == '\r') { i++; continue; }
        if (n >= max_tokens) return -1;
        tokens[n].off = usado;
        if (c == '"') {
            tokens[n].comillas = 1;
            i++;
            while (i < largo && linea[i] != '"') {
                char ch = linea[i];
                if (ch == '\\' && i + 1 < largo) {
                    char e = linea[i + 1];
                    if (e == '"') ch = '"';
                    else if (e == '\\') ch = '\\';
                    else if (e == 'n') ch = '\n';
                    else if (e == 'r') ch = '\r';
                    else if (e == 't') ch = '\t';
                    else ch = e;
                    i += 2;
                } else {
                    i++;
                }
                if (!poner(salida, tam_salida, &usado, ch)) return -1;
            }
            if (i >= largo) return -1; /* falta la comilla de cierre */
            i++;
        } else {
            tokens[n].comillas = 0;
            while (i < largo && linea[i] != ' ' && linea[i] != '\t' &&
                   linea[i] != '\r') {
                if (!poner(salida, tam_salida, &usado, linea[i])) return -1;
                i++;
            }
        }
        if (!poner(salida, tam_salida, &usado, '\0')) return -1;
        n++;
    }
    *usado_total = usado;
    return n;
}

/* Texto del token i (la salida queda terminada en cero por tokenizar). */
static const char *texto(const char *salida, const L3Tok *tokens, int i) {
    return salida + tokens[i].off;
}

/* Compara el token i con un texto exacto. */
static int es(const char *salida, const L3Tok *tokens, int i, const char *que) {
    return strcmp(texto(salida, tokens, i), que) == 0;
}

/* Entero del token i (0 si no es numero). */
static int entero(const char *salida, const L3Tok *tokens, int i) {
    return (int)strtol(texto(salida, tokens, i), NULL, 10);
}

/* Copia el texto del token i a un buffer con tope. */
static void copiar_token(char *dst, int tam, const char *salida,
                         const L3Tok *tokens, int i) {
    const char *src = texto(salida, tokens, i);
    int j;
    if (tam <= 0) return;
    for (j = 0; j < tam - 1 && src[j]; j++) dst[j] = src[j];
    for (; j < tam; j++) dst[j] = 0;
}

/* ------------------------------------------------------------------ */
/* Tabla de eventos vistos                                            */
/* ------------------------------------------------------------------ */

#define L3_EVENTOS_VISTOS 512

typedef struct {
    int objeto;
    int id;
    int tipo; /* 1 inicio, 2 fotograma, ... (los valores de LOGIC3) */
    int capa;
    char nombre[MOTOR_L3_NOMBRE_MAX];
    int flanco;       /* 0 mantener, 1 pulsar, 2 soltar */
    int objeto_tocar; /* -1 si no aplica */
    int cada;
    int escena;
} EventoVisto;

typedef struct {
    MotorL3 *proy;
    EventoVisto eventos[L3_EVENTOS_VISTOS];
    int num_eventos;

    /* bloque en curso (las lineas campo van despues de su bloque) */
    int bloque_id;
    int bloque_evento;
    int bloque_padre;
    int bloque_orden;
    int bloque_activo;
    char bloque_tipo[MOTOR_L3_NOMBRE_MAX];
    char bloque_codigo[MOTOR_L3_CODIGO_MAX + 1];
    int bloque_tiene_codigo;

    /* Fase 46: piezas `sistema.*` del bloque en curso. */
    int control_actual;        /* jugador del control en curso, 0 si no aplica */
    int accion_actual;         /* indice de la accion en curso, -1 si no hay */
    int regla_actual;          /* indice de la regla en curso, -1 si no hay */
    int bloque_control[MOTOR_L3_CONTROLES_MAX];
    int indice_control[MOTOR_L3_CONTROLES_MAX];
    int num_bloques_control;
    int pieza_actual;

    /* objeto en curso: sus trajes, variables y eventos cuelgan de el */
    int objeto;
    int objeto_indice; /* indice en proy->objetos, -1 si no hay */
    int capa;
    int dentro_funcion;
} L3Lector;

/* Tipo de evento por su nombre de LOGIC3. */
static int tipo_evento(const char *nombre) {
    if (strcmp(nombre, "inicio") == 0) return 1;
    if (strcmp(nombre, "fotograma") == 0) return 2;
    if (strcmp(nombre, "tecla") == 0) return 3;
    if (strcmp(nombre, "recibir") == 0) return 4;
    if (strcmp(nombre, "tocar") == 0) return 5;
    if (strcmp(nombre, "cada") == 0) return 6;
    if (strcmp(nombre, "clonar") == 0) return 7;
    if (strcmp(nombre, "escena") == 0) return 8;
    if (strcmp(nombre, "llamada") == 0) return 9;
    if (strcmp(nombre, "escena_salir") == 0) return 10;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Piezas del modelo de programa (fase 46)                             */
/* ------------------------------------------------------------------ */

static void motor_copiar(char *dst, int tam, const char *src) {
    int i;
    if (!dst || tam <= 0) return;
    for (i = 0; src && src[i] && i < tam - 1; i++) dst[i] = src[i];
    for (; i < tam; i++) dst[i] = 0;
}

static int motor_entero(const char *valor) {
    return valor && *valor ? (int)strtol(valor, NULL, 10) : 0;
}

static MotorL3Control *control_de_jugador(MotorL3Sistema *s, int jugador) {
    int i;
    for (i = 0; i < s->num_controles; i++)
        if (s->controles[i].jugador == jugador) return &s->controles[i];
    return NULL;
}

static int indice_control_de_bloque(L3Lector *l, int bloque) {
    int i;
    for (i = 0; i < l->num_bloques_control; i++)
        if (l->bloque_control[i] == bloque) return l->indice_control[i];
    return -1;
}

static void sistema_preparar_bloque(L3Lector *l) {
    MotorL3Sistema *s = &l->proy->sistema;

    l->control_actual = 0;
    l->accion_actual = -1;
    l->regla_actual = -1;
    l->pieza_actual = -1;

    if (strcmp(l->bloque_tipo, "sistema.opciones") == 0) {
        s->opciones.presente = 1;
        /* Un bloque de opciones incompleto se trata como legacy. */
        s->opciones.jugador_implicito = 1;
    } else if (strcmp(l->bloque_tipo, "sistema.accion") == 0) {
        int control = indice_control_de_bloque(l, l->bloque_padre);
        if (control >= 0 && s->controles[control].num_acciones <
                            MOTOR_L3_ACCIONES_MAX) {
            l->accion_actual = s->controles[control].num_acciones++;
            memset(&s->controles[control].acciones[l->accion_actual], 0,
                   sizeof(MotorL3Accion));
        }
    } else if (strcmp(l->bloque_tipo, "sistema.regla") == 0) {
        if (s->num_reglas < MOTOR_L3_REGLAS_MAX) {
            l->regla_actual = s->num_reglas++;
            memset(&s->reglas[l->regla_actual], 0, sizeof(MotorL3Regla));
        }
    } else if (strcmp(l->bloque_tipo, "sistema.pieza") == 0) {
        MotorL3 *p = l->proy;
        if (p->num_piezas < MOTOR_L3_PIEZAS_MAX) {
            MotorL3Pieza *pieza = &p->piezas[p->num_piezas];
            memset(pieza, 0, sizeof(*pieza));
            pieza->id = l->bloque_id;
            strncpy(pieza->tipo, l->bloque_tipo, sizeof(pieza->tipo) - 1);
            l->pieza_actual = p->num_piezas++;
        }
    }
}

static void sistema_campo(L3Lector *l, const char *clave, const char *valor) {
    MotorL3Sistema *s = &l->proy->sistema;

    if (strcmp(l->bloque_tipo, "sistema.pieza") == 0 &&
        l->pieza_actual >= 0 && l->pieza_actual < l->proy->num_piezas) {
        MotorL3Pieza *p = &l->proy->piezas[l->pieza_actual];
        if (strcmp(clave, "pieza") == 0)
            motor_copiar(p->tipo, sizeof(p->tipo), valor);
        else if (strcmp(clave, "id") == 0)
            p->id = motor_entero(valor);
        else if (strcmp(clave, "enlace") == 0)
            p->enlace = motor_entero(valor);
        else if (strcmp(clave, "xy") == 0)
            sscanf(valor, "%d,%d", &p->x, &p->y);
        else if (strcmp(clave, "nombre") == 0)
            motor_copiar(p->nombre, sizeof(p->nombre), valor);
        else if (strcmp(clave, "valor") == 0)
            motor_copiar(p->valor, sizeof(p->valor), valor);
        else if (strcmp(clave, "valor2") == 0)
            motor_copiar(p->valor2, sizeof(p->valor2), valor);
        return;
    }

    if (strcmp(l->bloque_tipo, "sistema.opciones") == 0) {
        s->opciones.presente = 1;
        if (strcmp(clave, "programa") == 0)
            s->opciones.programa = motor_entero(valor);
        else if (strcmp(clave, "jugador_implicito") == 0)
            s->opciones.jugador_implicito = motor_entero(valor);
        else if (strcmp(clave, "auto_compilar") == 0)
            s->opciones.auto_compilar = motor_entero(valor);
        return;
    }

    if (strcmp(l->bloque_tipo, "sistema.control") == 0) {
        MotorL3Control *c;
        if (strcmp(clave, "jugador") == 0) {
            int jugador = motor_entero(valor);
            if (jugador >= 1 && jugador <= MOTOR_L3_CONTROLES_MAX) {
                l->control_actual = jugador;
                c = control_de_jugador(s, jugador);
                if (!c && s->num_controles < MOTOR_L3_CONTROLES_MAX) {
                    c = &s->controles[s->num_controles++];
                    memset(c, 0, sizeof(*c));
                    c->jugador = jugador;
                }
            }
        }
        c = control_de_jugador(s, l->control_actual);
        if (!c) return;
        if (strcmp(clave, "actor") == 0) c->actor = motor_entero(valor);
        else if (strcmp(clave, "dispositivo") == 0)
            motor_copiar(c->dispositivo, sizeof(c->dispositivo), valor);
        else if (strcmp(clave, "esquema") == 0)
            motor_copiar(c->esquema, sizeof(c->esquema), valor);
        else if (strcmp(clave, "activo") == 0) c->activo = motor_entero(valor);
        return;
    }

    if (strcmp(l->bloque_tipo, "sistema.accion") == 0) {
        int control = indice_control_de_bloque(l, l->bloque_padre);
        MotorL3Accion *a;
        if (control < 0 || l->accion_actual < 0) return;
        a = &s->controles[control].acciones[l->accion_actual];
        if (strcmp(clave, "entrada") == 0)
            motor_copiar(a->entrada, sizeof(a->entrada), valor);
        else if (strcmp(clave, "flanco") == 0) a->flanco = motor_entero(valor);
        else if (strcmp(clave, "funcion") == 0)
            motor_copiar(a->funcion, sizeof(a->funcion), valor);
        return;
    }

    if (strcmp(l->bloque_tipo, "sistema.regla") == 0) {
        MotorL3Regla *r;
        if (l->regla_actual < 0) return;
        r = &s->reglas[l->regla_actual];
        if (strcmp(clave, "evento") == 0) r->evento = motor_entero(valor);
        else if (strcmp(clave, "actor") == 0) r->actor = motor_entero(valor);
        else if (strcmp(clave, "flanco") == 0) r->flanco = motor_entero(valor);
        else if (strcmp(clave, "cada") == 0) r->cada = motor_entero(valor);
        else if (strcmp(clave, "funcion") == 0)
            motor_copiar(r->funcion, sizeof(r->funcion), valor);
        else if (strcmp(clave, "tag") == 0)
            motor_copiar(r->etiqueta, sizeof(r->etiqueta), valor);
    }
}

/* La vista Programa guarda primero piezas visuales para no romper LOGIC3.
 * En el runtime las piezas basicas Actor + Teclado se convierten en un
 * control jugable. Si el editor ya genero bloques canonicos, se conservan. */
static int pieza_es(const MotorL3Pieza *p, const char *tipo) {
    return p && tipo && strcmp(p->tipo, tipo) == 0;
}

static int objeto_tiene_id(const MotorL3 *p, int id) {
    int i;
    for (i = 0; i < p->num_objetos; i++)
        if (p->objetos[i].id == id) return 1;
    return 0;
}

static MotorL3Objeto *objeto_por_id(MotorL3 *p, int id) {
    int i;
    for (i = 0; i < p->num_objetos; i++)
        if (p->objetos[i].id == id) return &p->objetos[i];
    return NULL;
}

static int pieza_actor_runtime_id(int pieza_id) {
    return 100000 + pieza_id;
}

static int control_ya_registrado(const MotorL3 *p, int jugador, int actor) {
    int i;
    for (i = 0; i < p->sistema.num_controles; i++) {
        const MotorL3Control *c = &p->sistema.controles[i];
        if (c->jugador == jugador && c->actor == actor) return 1;
    }
    return 0;
}

/* Un recurso se puede enlazar desde su salida al destino (la forma natural
 * de usar la paleta), pero también aceptamos el enlace inverso para que los
 * proyectos antiguos no queden rotos. */
static int actor_de_recurso(const MotorL3 *p, const MotorL3Pieza *recurso) {
    int i;
    if (!p || !recurso) return 0;
    for (i = 0; i < p->num_piezas; i++) {
        const MotorL3Pieza *actor = &p->piezas[i];
        if (pieza_es(actor, "sistema.actor") &&
            recurso->enlace == actor->id)
            return actor->id;
    }
    for (i = 0; i < p->num_piezas; i++) {
        const MotorL3Pieza *actor = &p->piezas[i];
        if (pieza_es(actor, "sistema.actor") &&
            actor->enlace == recurso->id)
            return actor->id;
    }
    return 0;
}

static void sistema_derivar_recursos(MotorL3 *p) {
    int i, j;
    if (!p) return;

    for (i = 0; i < p->num_piezas; i++) {
        MotorL3Pieza *camara = &p->piezas[i];
        if (!pieza_es(camara, "sistema.camara")) continue;
        p->camara.zoom = atoi(camara->valor);
        if (p->camara.zoom != 2 && p->camara.zoom != 4)
            p->camara.zoom = 1;
        p->camara.definida = 1;
        for (j = 0; j < p->num_piezas; j++) {
            if (pieza_es(&p->piezas[j], "sistema.actor") &&
                p->piezas[j].id == camara->enlace) {
                p->camara.objetivo = pieza_actor_runtime_id(
                    p->piezas[j].id);
                break;
            }
        }
    }

    /* Un escenario visual sin recurso de mapa es válido: permite probar
     * primero la lógica y añadir el mapa después desde la misma paleta. */
    for (i = 0; i < p->num_piezas; i++) {
        MotorL3Pieza *escena = &p->piezas[i];
        if (!pieza_es(escena, "sistema.escena")) continue;
        if (!p->mapa[0] && escena->valor[0] &&
            strcmp(escena->valor, "mapa.mav") != 0)
            motor_copiar(p->mapa, sizeof(p->mapa), escena->valor);
        for (j = 0; j < p->num_piezas; j++) {
            MotorL3Pieza *recurso = &p->piezas[j];
            if (!pieza_es(recurso, "sistema.recurso.mapa") ||
                !recurso->valor[0]) continue;
            if (recurso->enlace == escena->id || escena->enlace == recurso->id) {
                motor_copiar(p->mapa, sizeof(p->mapa), recurso->valor);
                break;
            }
        }
    }

    /* Traje e Imagen son dos formas de aportar la apariencia de un actor.
     * Se convierten al mismo traje runtime para que el render del motor no
     * tenga que conocer la procedencia de la pieza. */
    for (i = 0; i < p->num_piezas; i++) {
        MotorL3Pieza *pieza = &p->piezas[i];
        MotorL3Objeto *objeto;
        MotorL3Traje *traje;
        int actor_id;
        char imagen[MOTOR_L3_RUTA_MAX];
        if (!pieza_es(pieza, "sistema.traje") &&
            !pieza_es(pieza, "sistema.recurso.imagen")) continue;
        actor_id = actor_de_recurso(p, pieza);
        if (!actor_id || !pieza->valor[0]) continue;
        objeto = objeto_por_id(p, pieza_actor_runtime_id(actor_id));
        if (!objeto || objeto->num_trajes >= MOTOR_L3_TRAJES_MAX) continue;
        traje = &objeto->trajes[objeto->num_trajes++];
        memset(traje, 0, sizeof(*traje));
        traje->indice = objeto->num_trajes - 1;
        /* Las imagenes que salen de MODO > Pintar se guardan en `tiles`.
         * Un traje nativo sigue usando `trajes`; una pieza Imagen puede
         * indicar otra carpeta en valor2 y, si solo da un nombre, usa tiles. */
        imagen[0] = 0;
        if (pieza_es(pieza, "sistema.recurso.imagen") &&
            !strchr(pieza->valor, '\\') && !strchr(pieza->valor, '/') &&
            pieza->valor2[0] && strcmp(pieza->valor2, "0") != 0) {
            _snprintf(imagen, sizeof(imagen), "%s\\%s",
                      pieza->valor2, pieza->valor);
            imagen[sizeof(imagen) - 1] = 0;
        } else if (pieza_es(pieza, "sistema.recurso.imagen") &&
                   !strchr(pieza->valor, '\\') &&
                   !strchr(pieza->valor, '/')) {
            _snprintf(imagen, sizeof(imagen), "tiles\\%s", pieza->valor);
            imagen[sizeof(imagen) - 1] = 0;
        } else {
            motor_copiar(imagen, sizeof(imagen), pieza->valor);
        }
        motor_copiar(traje->imagen, sizeof(traje->imagen), imagen);
        traje->cuadros = 1;
        traje->velocidad = 1;
    }
}

static void sistema_derivar_piezas(MotorL3 *p) {
    int i, j;
    if (!p || p->num_piezas <= 0) return;
    if (p->escena_inicial <= 0) p->escena_inicial = 1;

    /* Actor visual sin objeto LOGIC3: se crea una entidad rectangular
     * jugable; cuando el proyecto tenga objeto real, este no se duplica. */
    for (i = 0; i < p->num_piezas; i++) {
        MotorL3Pieza *pieza = &p->piezas[i];
        MotorL3Objeto *o;
        int rid;
        if (!pieza_es(pieza, "sistema.actor")) continue;
        rid = pieza_actor_runtime_id(pieza->id);
        if (objeto_tiene_id(p, rid) || p->num_objetos >= MOTOR_L3_OBJETOS_MAX)
            continue;
        o = &p->objetos[p->num_objetos++];
        memset(o, 0, sizeof(*o));
        o->id = rid;
        strncpy(o->nombre, pieza->valor[0] ? pieza->valor : pieza->nombre,
                sizeof(o->nombre) - 1);
        o->escena = p->escena_inicial ? p->escena_inicial : 1;
        o->x = 16;
        o->y = 16;
        o->w = 24;
        o->h = 24;
        o->visible = 1;
    }

    sistema_derivar_recursos(p);

    /* Cada pieza de teclado representa un perfil de control. La relacion
     * Actor -> Teclado se guarda en el enlace de la pieza Actor. */
    for (i = 0; i < p->num_piezas; i++) {
        MotorL3Pieza *control_pieza = &p->piezas[i];
        MotorL3Control *c;
        int jugador, actor = 0;
        const char *dispositivo = NULL;
        if (pieza_es(control_pieza, "sistema.teclado")) dispositivo = "teclado";
        else if (pieza_es(control_pieza, "sistema.raton")) dispositivo = "raton";
        else if (pieza_es(control_pieza, "sistema.mando")) dispositivo = "mando";
        else if (strncmp(control_pieza->tipo, "sistema.jugador", 15) == 0)
            dispositivo = control_pieza->valor2[0] ? control_pieza->valor2 : "teclado";
        else continue;
        jugador = atoi(control_pieza->valor);
        if (jugador < 1 || jugador > MOTOR_L3_CONTROLES_MAX)
            jugador = 1;
        for (j = 0; j < p->num_piezas; j++) {
            MotorL3Pieza *actor_pieza = &p->piezas[j];
            if (pieza_es(actor_pieza, "sistema.actor") &&
                actor_pieza->enlace == control_pieza->id) {
                actor = pieza_actor_runtime_id(actor_pieza->id);
                break;
            }
        }
        if (!actor && control_pieza->enlace > 0) {
            for (j = 0; j < p->num_piezas; j++) {
                if (pieza_es(&p->piezas[j], "sistema.actor") &&
                    p->piezas[j].id == control_pieza->enlace) {
                    actor = pieza_actor_runtime_id(p->piezas[j].id);
                    break;
                }
            }
        }
        if (!actor || control_ya_registrado(p, jugador, actor) ||
            p->sistema.num_controles >= MOTOR_L3_CONTROLES_MAX)
            continue;
        c = &p->sistema.controles[p->sistema.num_controles++];
        memset(c, 0, sizeof(*c));
        c->jugador = jugador;
        c->actor = actor;
        strncpy(c->dispositivo, dispositivo, sizeof(c->dispositivo) - 1);
        strncpy(c->esquema, "plataformas", sizeof(c->esquema) - 1);
        c->activo = 1;
        for (j = 0; j < p->num_piezas; j++) {
            MotorL3Pieza *accion = &p->piezas[j];
            MotorL3Accion *a;
            if (!pieza_es(accion, "sistema.accion.movimiento") ||
                (control_pieza->enlace != accion->id &&
                 accion->enlace != control_pieza->id) ||
                c->num_acciones >= MOTOR_L3_ACCIONES_MAX)
                continue;
            a = &c->acciones[c->num_acciones++];
            memset(a, 0, sizeof(*a));
            motor_copiar(a->entrada, sizeof(a->entrada), accion->valor);
            motor_copiar(a->funcion, sizeof(a->funcion), accion->valor);
            if (strcmp(accion->valor2, "pulsar") == 0) a->flanco = 1;
            else if (strcmp(accion->valor2, "soltar") == 0) a->flanco = 2;
        }
    }
}

static void sistema_cerrar_bloque(L3Lector *l) {
    if (strcmp(l->bloque_tipo, "sistema.control") != 0) return;
    MotorL3Control *c;
    if (l->control_actual < 1 || l->bloque_id <= 0 ||
        l->num_bloques_control >= MOTOR_L3_CONTROLES_MAX) return;
    c = control_de_jugador(&l->proy->sistema, l->control_actual);
    if (!c) return;
    l->bloque_control[l->num_bloques_control] = l->bloque_id;
    l->indice_control[l->num_bloques_control] = c - l->proy->sistema.controles;
    l->num_bloques_control++;
}

/* Cierra el bloque en curso: si es codigo.lobo de un evento, se guarda
 * como script del proyecto con los parametros del evento. */
static int cerrar_bloque(L3Lector *l, int linea) {
    MotorL3 *p = l->proy;
    int i, ev = -1;

    if (!l->bloque_activo) return 1;
    sistema_cerrar_bloque(l);
    l->bloque_activo = 0;
    if (strcmp(l->bloque_tipo, "codigo.lobo") != 0) return 1;
    if (!l->bloque_tiene_codigo || !l->bloque_codigo[0]) return 1;

    /* El bloque referencia un id de evento; se busca el evento. Los ids
     * son unicos dentro del objeto pero pueden repetirse entre objetos,
     * asi que se prefiere el evento del objeto en curso. */
    for (i = 0; i < l->num_eventos; i++) {
        if (l->eventos[i].id != l->bloque_evento) continue;
        if (l->eventos[i].objeto == l->objeto) { ev = i; break; }
        if (ev < 0) ev = i;
    }
    if (ev < 0) return 1;                /* evento desconocido: se ignora */
    if (l->eventos[ev].tipo == 0 || l->eventos[ev].tipo == 9)
        return 1; /* desconocido o llamada de funcion: no es un evento */

    if (p->num_scripts >= MOTOR_L3_SCRIPTS_MAX)
        return fallo(linea, "demasiados scripts codigo.lobo");

    {
        MotorL3Script *s = &p->scripts[p->num_scripts++];
        const EventoVisto *e = &l->eventos[ev];
        s->bloque = l->bloque_id;
        s->objeto = e->objeto;
        s->evento = e->id;
        s->orden = l->bloque_orden;
        s->capa = e->capa;
        s->tipo = e->tipo;
        memcpy(s->nombre, e->nombre, sizeof(s->nombre));
        s->flanco = e->flanco;
        s->objeto_tocar = e->objeto_tocar;
        s->cada = e->cada;
        s->escena = e->escena;
        memcpy(s->codigo, l->bloque_codigo, strlen(l->bloque_codigo) + 1);
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Lectura del archivo                                                */
/* ------------------------------------------------------------------ */

static char *leer_todo(const char *ruta, int *largo) {
    char *virtual = activo_leer_virtual(ruta, largo);
    FILE *f;
    if (virtual) return virtual;
    f = fopen(ruta, "rb");
    char *datos;
    long n;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n < 0 || n > 64L * 1024 * 1024) { fclose(f); return NULL; }
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

/* Orden estable de los scripts: capa, objeto, evento, orden y bloque. El
 * escritor de LOGIC3 ya es determinista, pero el orden de ejecucion no
 * tiene por que ser el del archivo. */
static void ordenar_scripts(MotorL3 *p) {
    int i, j;
    for (i = 1; i < p->num_scripts; i++) {
        MotorL3Script tmp = p->scripts[i];
        j = i - 1;
        while (j >= 0) {
            MotorL3Script *a = &p->scripts[j];
            int antes = a->capa < tmp.capa ||
                        (a->capa == tmp.capa &&
                         (a->objeto < tmp.objeto ||
                          (a->objeto == tmp.objeto &&
                           (a->evento < tmp.evento ||
                            (a->evento == tmp.evento &&
                             (a->orden < tmp.orden ||
                              (a->orden == tmp.orden &&
                               a->bloque < tmp.bloque)))))));
            if (antes) break;
            p->scripts[j + 1] = *a;
            j--;
        }
        p->scripts[j + 1] = tmp;
    }
}

/* Guarda una escena; la primera del archivo es la que arranca. */
static void agregar_escena(MotorL3 *p, const char *salida,
                           const L3Tok *tokens, int nt) {
    MotorL3Escena *e;
    if (p->num_escenas >= MOTOR_L3_ESCENAS_MAX) return;
    e = &p->escenas[p->num_escenas++];
    memset(e, 0, sizeof(*e));
    e->id = entero(salida, tokens, 1);
    if (nt >= 3) copiar_token(e->nombre, sizeof(e->nombre), salida, tokens, 2);
    if (nt >= 4) copiar_token(e->mapa, sizeof(e->mapa), salida, tokens, 3);
    if (nt >= 5) e->ancho = entero(salida, tokens, 4);
    if (nt >= 6) e->alto = entero(salida, tokens, 5);
    if (nt >= 7) e->fondo = (unsigned int)strtoul(
        texto(salida, tokens, 6), NULL, 16);
    if (p->num_escenas == 1) p->escena_inicial = e->id;
    if (!p->mapa[0] && e->mapa[0])
        memcpy(p->mapa, e->mapa, strlen(e->mapa) + 1);
}

/* Guarda un objeto con sus campos clave-valor. Devuelve su indice o -1. */
static int agregar_objeto(MotorL3 *p, const char *salida,
                          const L3Tok *tokens, int nt) {
    MotorL3Objeto *o;
    int i;
    if (p->num_objetos >= MOTOR_L3_OBJETOS_MAX) return -1;
    o = &p->objetos[p->num_objetos];
    memset(o, 0, sizeof(*o));
    o->id = entero(salida, tokens, 1);
    if (nt >= 3) copiar_token(o->nombre, sizeof(o->nombre), salida, tokens, 2);
    o->visible = 1;
    /* Los siguientes tokens son pares clave valor; las claves que no se
     * conocen se saltan para que el formato pueda crecer. */
    for (i = 3; i + 1 < nt; i += 2) {
        if (es(salida, tokens, i, "escena"))
            o->escena = entero(salida, tokens, i + 1);
        else if (es(salida, tokens, i, "x"))
            o->x = entero(salida, tokens, i + 1);
        else if (es(salida, tokens, i, "y"))
            o->y = entero(salida, tokens, i + 1);
        else if (es(salida, tokens, i, "w"))
            o->w = entero(salida, tokens, i + 1);
        else if (es(salida, tokens, i, "h"))
            o->h = entero(salida, tokens, i + 1);
        else if (es(salida, tokens, i, "visible"))
            o->visible = entero(salida, tokens, i + 1) ? 1 : 0;
        else if (es(salida, tokens, i, "capa"))
            o->capa = entero(salida, tokens, i + 1);
        else if (es(salida, tokens, i, "padre"))
            o->padre = entero(salida, tokens, i + 1);
    }
    if (o->w <= 0) o->w = 16;
    if (o->h <= 0) o->h = 16;
    return p->num_objetos++;
}

/* Codigos de tipo de LOGIC3 (los mismos que en el editor). */
static int tipo_valor(const char *nombre) {
    if (strcmp(nombre, "entero") == 0 || strcmp(nombre, "0") == 0) return 0;
    if (strcmp(nombre, "booleano") == 0 || strcmp(nombre, "1") == 0) return 1;
    if (strcmp(nombre, "texto") == 0 || strcmp(nombre, "2") == 0) return 2;
    return -1;
}

/* Registra un texto en la tabla compartida del proyecto. La tabla es
 * pequena (32 entradas, como los textos runtime de lobo) y se deduplica
 * para que dos variables con el mismo valor no gasten dos huecos. */
static int registrar_texto(MotorL3 *p, const char *cadena) {
    int i;
    for (i = 0; i < p->num_textos; i++)
        if (strcmp(p->textos[i], cadena) == 0) return i;
    if (p->num_textos >= MOTOR_L3_TEXTOS_MAX)
        return MOTOR_L3_TEXTOS_MAX - 1;
    strncpy(p->textos[i], cadena, MOTOR_L3_TEXTO_MAX - 1);
    p->textos[i][MOTOR_L3_TEXTO_MAX - 1] = 0;
    return p->num_textos++;
}

/* Convierte el token i segun el tipo. Los textos se guardan en la tabla
 * compartida y su valor es el indice; ademas se copian a `texto_dst` si
 * se pide (para las variables, que conservan la cadena). */
static int valor_tipado(MotorL3 *p, const char *salida, const L3Tok *tokens,
                        int i, int tipo, char *texto_dst, int tam_texto) {
    const char *src = (i >= 0 && tokens) ? texto(salida, tokens, i) : "";
    if (texto_dst && tam_texto > 0) {
        strncpy(texto_dst, src, (size_t)tam_texto - 1);
        texto_dst[tam_texto - 1] = 0;
    }
    if (i < 0 || !tokens) return 0;
    if (tipo == 2) return registrar_texto(p, src);
    if (tipo == 1) return entero(salida, tokens, i) ? 1 : 0;
    return entero(salida, tokens, i);
}

/* Adjunta una variable al objeto en curso. */
static void agregar_variable_objeto(L3Lector *l, const char *salida,
                                    const L3Tok *tokens, int nt) {
    MotorL3Objeto *o;
    MotorL3Variable *v;
    int tipo = 0;
    if (l->objeto_indice < 0 || l->objeto_indice >= l->proy->num_objetos)
        return;
    if (nt >= 2 && es(salida, tokens, 1, "global")) return; /* otra seccion */
    if (nt < 3) return;
    if (nt >= 4) {
        int t = tipo_valor(texto(salida, tokens, 2));
        if (t >= 0) tipo = t;
    }
    o = &l->proy->objetos[l->objeto_indice];
    if (o->num_variables >= MOTOR_L3_VARS_MAX) return;
    v = &o->variables[o->num_variables++];
    memset(v, 0, sizeof(*v));
    copiar_token(v->nombre, sizeof(v->nombre), salida, tokens, 1);
    v->tipo = tipo;
    v->valor = valor_tipado(l->proy, salida, tokens, nt >= 4 ? 3 : -1,
                            tipo, v->texto, sizeof(v->texto));
}

/* Adjunta una variable global (fase 28). Antes se descartaba. */
static void agregar_variable_global(L3Lector *l, const char *salida,
                                    const L3Tok *tokens, int nt) {
    MotorL3 *p = l->proy;
    MotorL3Variable *v;
    int tipo = 0;
    if (nt < 4 || p->num_variables >= MOTOR_L3_GLOBALES_MAX) return;
    {
        int t = tipo_valor(texto(salida, tokens, 2));
        if (t >= 0) tipo = t;
    }
    v = &p->variables[p->num_variables++];
    memset(v, 0, sizeof(*v));
    copiar_token(v->nombre, sizeof(v->nombre), salida, tokens, 1);
    v->tipo = tipo;
    v->valor = valor_tipado(p, salida, tokens, 3, tipo,
                            v->texto, sizeof(v->texto));
}

/* Adjunta una lista global y sus valores iniciales (fase 28). */
static void agregar_lista_global(L3Lector *l, const char *salida,
                                 const L3Tok *tokens, int nt) {
    MotorL3 *p = l->proy;
    MotorL3Lista *li;
    int tipo = 0, primero = 4, k;
    if (nt < 4 || p->num_listas >= MOTOR_L3_LISTAS_MAX) return;
    if (nt >= 5) {
        int t = tipo_valor(texto(salida, tokens, 4));
        if (t >= 0) { tipo = t; primero = 5; }
    }
    li = &p->listas[p->num_listas++];
    memset(li, 0, sizeof(*li));
    copiar_token(li->nombre, sizeof(li->nombre), salida, tokens, 2);
    li->capacidad = entero(salida, tokens, 3);
    li->tipo = tipo;
    /* valor_tipado registra los textos y devuelve su indice, asi que el
     * mismo array sirve para enteros, booleanos y textos. */
    for (k = primero; k < nt && li->num_elementos < MOTOR_L3_LISTA_ELEMS_MAX;
         k++)
        li->elementos[li->num_elementos++] =
            valor_tipado(p, salida, tokens, k, tipo, NULL, 0);
}

/* Adjunta un traje al objeto en curso. */
static void agregar_traje_objeto(L3Lector *l, const char *salida,
                                 const L3Tok *tokens, int nt) {
    MotorL3Objeto *o;
    MotorL3Traje *t;
    if (l->objeto_indice < 0 || l->objeto_indice >= l->proy->num_objetos)
        return;
    if (nt < 7) return;
    o = &l->proy->objetos[l->objeto_indice];
    if (o->num_trajes >= MOTOR_L3_TRAJES_MAX) return;
    t = &o->trajes[o->num_trajes++];
    memset(t, 0, sizeof(*t));
    t->indice = entero(salida, tokens, 1);
    copiar_token(t->imagen, sizeof(t->imagen), salida, tokens, 2);
    t->ancho = entero(salida, tokens, 3);
    t->alto = entero(salida, tokens, 4);
    t->pivote_x = entero(salida, tokens, 5);
    t->pivote_y = entero(salida, tokens, 6);
    /* Extensiones del formato: pares clave valor despues de los seis
     * campos originales. Los valores por defecto conservan el render
     * clasico, asi un LOGIC3 antiguo no cambia ni un pixel. */
    t->cuadros = 1;
    t->velocidad = 1;
    {
        int i;
        for (i = 7; i + 1 < nt; i += 2) {
            if (es(salida, tokens, i, "cuadros")) {
                int v = entero(salida, tokens, i + 1);
                if (v > 0) t->cuadros = v;
            } else if (es(salida, tokens, i, "cuadro_ancho")) {
                t->cuadro_ancho = entero(salida, tokens, i + 1);
            } else if (es(salida, tokens, i, "cuadro_alto")) {
                t->cuadro_alto = entero(salida, tokens, i + 1);
            } else if (es(salida, tokens, i, "flip_x")) {
                t->flip_x = entero(salida, tokens, i + 1) ? 1 : 0;
            } else if (es(salida, tokens, i, "flip_y")) {
                t->flip_y = entero(salida, tokens, i + 1) ? 1 : 0;
            } else if (es(salida, tokens, i, "rotacion")) {
                int v = entero(salida, tokens, i + 1);
                t->rotacion = (v == 90 || v == 180 || v == 270) ? v : 0;
            } else if (es(salida, tokens, i, "velocidad")) {
                int v = entero(salida, tokens, i + 1);
                if (v > 0) t->velocidad = v;
            }
        }
    }
}

/* Linea opcional de camara (fase 30):
 *   camara objetivo <id> limites <x> <y> <ancho> <alto> zoom <1|2|4>
 * Las claves pueden venir en cualquier orden y las desconocidas se saltan. */
static void agregar_camara(MotorL3 *p, const char *salida,
                           const L3Tok *tokens, int nt) {
    int i;
    p->camara.definida = 1;
    p->camara.objetivo = 0;
    p->camara.x = 0;
    p->camara.y = 0;
    p->camara.ancho = 0;
    p->camara.alto = 0;
    p->camara.zoom = 1;
    for (i = 1; i < nt;) {
        if (es(salida, tokens, i, "objetivo") && i + 1 < nt) {
            int v = entero(salida, tokens, i + 1);
            p->camara.objetivo = v > 0 ? v : 0;
            i += 2;
        } else if (es(salida, tokens, i, "limites") && i + 4 < nt) {
            p->camara.x = entero(salida, tokens, i + 1);
            p->camara.y = entero(salida, tokens, i + 2);
            p->camara.ancho = entero(salida, tokens, i + 3);
            p->camara.alto = entero(salida, tokens, i + 4);
            i += 5;
        } else if (es(salida, tokens, i, "zoom") && i + 1 < nt) {
            int v = entero(salida, tokens, i + 1);
            p->camara.zoom = (v == 2 || v == 4) ? v : 1;
            i += 2;
        } else {
            i++;
        }
    }
    if (p->camara.x < 0) p->camara.x = 0;
    if (p->camara.y < 0) p->camara.y = 0;
    if (p->camara.ancho < 0) p->camara.ancho = 0;
    if (p->camara.alto < 0) p->camara.alto = 0;
}

typedef struct {
    int origen;
    int destino;
    int linea;
} EnlaceMontajeMotor;

static int ruta_es_log_motor(const char *ruta) {
    const char *p = ruta ? strrchr(ruta, '.') : NULL;
    return p && (strcmp(p, ".log") == 0 || strcmp(p, ".LOG") == 0);
}

static int indice_pieza_motor(const MotorL3 *p, int id) {
    int i;
    if (!p || id <= 0) return -1;
    for (i = 0; i < p->num_piezas; i++)
        if (p->piezas[i].id == id) return i;
    return -1;
}

/* Lee la forma textual que usa el editor: un nodo es una pieza y `enlace`
 * es el valor de compatibilidad para proyectos antiguos. Las conexiones
 * explicitas se aplican al final, cuando ya se han leido todos los nodos. */
static int nodo_montaje_motor(MotorL3 *p, const char *salida,
                              const L3Tok *tokens, int nt, int linea) {
    int id, indice, i, x = 0, y = 0, enlace = 0;
    int tiene_x = 0, tiene_y = 0;
    const char *tipo = NULL;
    MotorL3Pieza *pieza;
    if (nt < 4 || (id = entero(salida, tokens, 1)) <= 0)
        return fallo(linea, "nodo sin id valido");
    for (i = 2; i + 1 < nt; i += 2) {
        const char *clave = texto(salida, tokens, i);
        const char *valor = texto(salida, tokens, i + 1);
        if (strcmp(clave, "tipo") == 0) tipo = valor;
        else if (strcmp(clave, "x") == 0) {
            x = motor_entero(valor); tiene_x = 1;
        } else if (strcmp(clave, "y") == 0) {
            y = motor_entero(valor); tiene_y = 1;
        } else if (strcmp(clave, "xy") == 0) {
            if (sscanf(valor, "%d,%d", &x, &y) != 2)
                return fallo(linea, "xy de nodo invalido");
            tiene_x = tiene_y = 1;
        } else if (strcmp(clave, "enlace") == 0) {
            enlace = motor_entero(valor);
        }
    }
    if (!tipo || !tipo[0]) return fallo(linea, "nodo sin tipo");
    indice = indice_pieza_motor(p, id);
    if (indice < 0) {
        if (p->num_piezas >= MOTOR_L3_PIEZAS_MAX)
            return fallo(linea, "demasiados nodos de montaje");
        indice = p->num_piezas++;
        memset(&p->piezas[indice], 0, sizeof(p->piezas[indice]));
        p->piezas[indice].id = id;
    }
    pieza = &p->piezas[indice];
    motor_copiar(pieza->tipo, sizeof(pieza->tipo), tipo);
    for (i = 2; i + 1 < nt; i += 2) {
        const char *clave = texto(salida, tokens, i);
        const char *valor = texto(salida, tokens, i + 1);
        if (strcmp(clave, "nombre") == 0)
            motor_copiar(pieza->nombre, sizeof(pieza->nombre), valor);
        else if (strcmp(clave, "valor") == 0)
            motor_copiar(pieza->valor, sizeof(pieza->valor), valor);
        else if (strcmp(clave, "valor2") == 0)
            motor_copiar(pieza->valor2, sizeof(pieza->valor2), valor);
        else if (strcmp(clave, "enlace") == 0)
            pieza->enlace = enlace;
    }
    if (tiene_x) pieza->x = x;
    if (tiene_y) pieza->y = y;
    if (!pieza->nombre[0]) motor_copiar(pieza->nombre, sizeof(pieza->nombre), tipo);
    return 1;
}

static int conecta_montaje_motor(const char *salida, const L3Tok *tokens,
                                 int nt, int linea,
                                 EnlaceMontajeMotor *enlace) {
    int origen, destino = 0, i;
    if (nt < 3 || (origen = entero(salida, tokens, 1)) <= 0)
        return fallo(linea, "conecta sin nodo origen valido");
    for (i = 2; i < nt; i++) {
        const char *valor = texto(salida, tokens, i);
        char *fin = NULL;
        long n = strtol(valor, &fin, 10);
        if (fin != valor && *fin == 0 && n > 0) {
            destino = (int)n;
            break;
        }
    }
    if (destino <= 0) return fallo(linea, "conecta sin nodo destino valido");
    enlace->origen = origen;
    enlace->destino = destino;
    enlace->linea = linea;
    return 1;
}

int motor_l3_cargar(const char *ruta, MotorL3 *p, char *error, int tam_error) {
    char *datos;
    int largo = 0, linea = 0;
    const char *q;
    L3Lector l;
    char salida[MOTOR_L3_CODIGO_MAX * 4 + 32768];
    L3Tok tokens[L3_TOKENS_MAX];
    EnlaceMontajeMotor enlaces[MOTOR_L3_PIEZAS_MAX];
    int num_enlaces = 0;

    if (error && tam_error > 0) error[0] = 0;
    if (!ruta || !p) return fallo_en(error, tam_error, 0, "ruta vacia");

    memset(p, 0, sizeof(*p));
    /* Sin la pieza `sistema.opciones` el proyecto es legacy. */
    p->sistema.opciones.jugador_implicito = 1;
    p->ancho = 320;
    p->alto = 240;
    p->fps = 60;
    p->velocidad = 4;
    p->limitar_cuadros = 1;
    memset(&l, 0, sizeof(l));
    l.proy = p;
    l.objeto = 0;
    l.objeto_indice = -1;

    datos = leer_todo(ruta, &largo);
    if (!datos) {
        if (error)
            _snprintf(error, tam_error, "no se pudo leer %s", ruta);
        if (error && tam_error > 0) error[tam_error - 1] = '\0';
        return 0;
    }

    q = datos;
    if (strncmp(q, "LOGIC3", 6) != 0) {
        free(datos);
        if (error)
            _snprintf(error, tam_error, "%s no es un LOGIC3", ruta);
        if (error && tam_error > 0) error[tam_error - 1] = '\0';
        return 0;
    }

    while (*q) {
        const char *fin = strchr(q, '\n');
        int n = fin ? (int)(fin - q) : (int)strlen(q);
        int nt, usado = 0;

        if (n > 0 && q[n - 1] == '\r') n--;
        linea++;
        if (n >= (int)sizeof(salida)) {
            free(datos);
            return fallo_en(error, tam_error, linea, "linea demasiado larga");
        }
        nt = tokenizar(q, n, salida, (int)sizeof(salida), tokens,
                       L3_TOKENS_MAX, &usado);
        if (nt < 0) {
            free(datos);
            return fallo_en(error, tam_error, linea, "linea demasiado larga o comilla sin cerrar");
        }
        if (nt == 0) goto siguiente;

        /* --- cabecera --- */
        if (es(salida, tokens, 0, "proyecto") && nt >= 2) {
            int i = 1;
            if (tokens[1].comillas) {
                strncpy(p->nombre, texto(salida, tokens, 1),
                        sizeof(p->nombre) - 1);
                p->nombre[sizeof(p->nombre) - 1] = '\0';
                i = 2;
            }
            if (i + 5 < nt) {
                p->ancho = entero(salida, tokens, i);
                p->alto = entero(salida, tokens, i + 1);
                p->limitar_cuadros = entero(salida, tokens, i + 2);
                p->fps = entero(salida, tokens, i + 3);
                p->velocidad = entero(salida, tokens, i + 4);
                p->fondo = (unsigned int)strtoul(
                    texto(salida, tokens, i + 5), NULL, 16);
            }
            goto siguiente;
        }
        /* --- camara opcional (fase 30) --- */
        if (es(salida, tokens, 0, "camara")) {
            agregar_camara(p, salida, tokens, nt);
            goto siguiente;
        }
        if (es(salida, tokens, 0, "montaje")) {
            if (ruta_es_log_motor(ruta)) p->num_piezas = 0;
            goto siguiente;
        }
        if (es(salida, tokens, 0, "fin_montaje"))
            goto siguiente;
        if (es(salida, tokens, 0, "nodo")) {
            if (!nodo_montaje_motor(p, salida, tokens, nt, linea)) {
                free(datos);
                return fallo_en(error, tam_error, 0, g_error);
            }
            goto siguiente;
        }
        if (es(salida, tokens, 0, "conecta")) {
            if (num_enlaces >= MOTOR_L3_PIEZAS_MAX ||
                !conecta_montaje_motor(salida, tokens, nt, linea,
                                       &enlaces[num_enlaces])) {
                free(datos);
                return fallo_en(error, tam_error, 0, g_error);
            }
            num_enlaces++;
            goto siguiente;
        }
        /* --- escenas: la primera es la que arranca --- */
        if (es(salida, tokens, 0, "escena") && nt >= 3) {
            agregar_escena(p, salida, tokens, nt);
            goto siguiente;
        }
        /* --- objetos: tipo de entidad y dueno de los eventos hijos --- */
        if (es(salida, tokens, 0, "objeto") && nt >= 2) {
            if (!cerrar_bloque(&l, linea)) { free(datos); if (error && tam_error > 0) { strncpy(error, g_error, (size_t)tam_error - 1); error[tam_error - 1] = 0; } return 0; }
            l.objeto = entero(salida, tokens, 1);
            l.objeto_indice = agregar_objeto(p, salida, tokens, nt);
            l.capa = 0;
            l.dentro_funcion = 0;
            if (l.objeto_indice >= 0) l.capa = p->objetos[l.objeto_indice].capa;
            goto siguiente;
        }
        /* --- variables globales y de objeto, y listas globales --- */
        if (es(salida, tokens, 0, "variable") && nt >= 3) {
            if (nt >= 2 && es(salida, tokens, 1, "global"))
                agregar_variable_global(&l, salida, tokens, nt);
            else
                agregar_variable_objeto(&l, salida, tokens, nt);
            goto siguiente;
        }
        if (es(salida, tokens, 0, "lista") && nt >= 4) {
            agregar_lista_global(&l, salida, tokens, nt);
            goto siguiente;
        }
        if (es(salida, tokens, 0, "traje") && nt >= 7) {
            agregar_traje_objeto(&l, salida, tokens, nt);
            goto siguiente;
        }
        /* --- funciones: sus eventos no son scripts de fotograma --- */
        if (es(salida, tokens, 0, "funcion") && nt >= 2) {
            if (!cerrar_bloque(&l, linea)) { free(datos); if (error && tam_error > 0) { strncpy(error, g_error, (size_t)tam_error - 1); error[tam_error - 1] = 0; } return 0; }
            l.objeto = 0;
            l.objeto_indice = -1;
            l.capa = 0;
            l.dentro_funcion = 1;
            goto siguiente;
        }
        /* --- eventos de un objeto --- */
        if (es(salida, tokens, 0, "evento") && nt >= 3) {
            if (l.num_eventos < L3_EVENTOS_VISTOS) {
                EventoVisto *e = &l.eventos[l.num_eventos++];
                e->objeto = l.objeto;
                e->id = entero(salida, tokens, 1);
                e->tipo = tipo_evento(texto(salida, tokens, 2));
                e->capa = l.capa;
                e->flanco = 0;
                e->objeto_tocar = -1;
                if (l.dentro_funcion) e->tipo = 9; /* llamada */
                /* Parametros por tipo; los mismos campos que escribe el
                 * editor en src/logic3.c. El lector es tolerante: si falta
                 * un parametro se deja el valor por defecto. */
                if (e->tipo == 3) {
                    if (nt >= 4) copiar_token(e->nombre, sizeof(e->nombre),
                                              salida, tokens, 3);
                    if (nt >= 5) {
                        const char *f = texto(salida, tokens, 4);
                        if (strcmp(f, "pulsar") == 0) e->flanco = 1;
                        else if (strcmp(f, "soltar") == 0) e->flanco = 2;
                    }
                } else if (e->tipo == 4) {
                    if (nt >= 4) copiar_token(e->nombre, sizeof(e->nombre),
                                              salida, tokens, 3);
                } else if (e->tipo == 5) {
                    if (nt >= 4) e->objeto_tocar = entero(salida, tokens, 3);
                    if (nt >= 5) copiar_token(e->nombre, sizeof(e->nombre),
                                              salida, tokens, 4);
                } else if (e->tipo == 6) {
                    if (nt >= 4) e->cada = entero(salida, tokens, 3);
                } else if (e->tipo == 8 || e->tipo == 10) {
                    if (nt >= 4) e->escena = entero(salida, tokens, 3);
                }
            }
            goto siguiente;
        }
        /* --- bloques y sus campos --- */
        if (es(salida, tokens, 0, "bloque") && nt >= 2) {
            int i;
            if (!cerrar_bloque(&l, linea)) { free(datos); if (error && tam_error > 0) { strncpy(error, g_error, (size_t)tam_error - 1); error[tam_error - 1] = 0; } return 0; }
            p->num_bloques++;
            l.bloque_id = entero(salida, tokens, 1);
            l.bloque_evento = 0;
            l.bloque_padre = 0;
            l.bloque_orden = 0;
            l.bloque_tipo[0] = '\0';
            l.bloque_codigo[0] = '\0';
            l.bloque_tiene_codigo = 0;
            for (i = 2; i + 1 < nt; i++) {
                if (es(salida, tokens, i, "evento"))
                    l.bloque_evento = entero(salida, tokens, i + 1);
                else if (es(salida, tokens, i, "padre"))
                    l.bloque_padre = entero(salida, tokens, i + 1);
                else if (es(salida, tokens, i, "orden"))
                    l.bloque_orden = entero(salida, tokens, i + 1);
                else if (es(salida, tokens, i, "tipo")) {
                    strncpy(l.bloque_tipo, texto(salida, tokens, i + 1),
                            sizeof(l.bloque_tipo) - 1);
                    l.bloque_tipo[sizeof(l.bloque_tipo) - 1] = '\0';
                }
            }
            sistema_preparar_bloque(&l);
            l.bloque_activo = 1;
            goto siguiente;
        }
        if (es(salida, tokens, 0, "campo") && nt >= 3 && l.bloque_activo) {
            sistema_campo(&l, texto(salida, tokens, 1),
                          texto(salida, tokens, 2));
            if (es(salida, tokens, 1, "codigo")) {
                const char *cod = texto(salida, tokens, 2);
                size_t m = strlen(cod);
                if (m > MOTOR_L3_CODIGO_MAX) m = MOTOR_L3_CODIGO_MAX;
                memcpy(l.bloque_codigo, cod, m);
                l.bloque_codigo[m] = '\0';
                l.bloque_tiene_codigo = 1;
            }
            goto siguiente;
        }
        /* --- fin del proyecto --- */
        if (es(salida, tokens, 0, "FIN") || es(salida, tokens, 0, "CRC32"))
            goto siguiente;

    siguiente:
        if (!fin) break;
        q = fin + 1;
    }

    if (!cerrar_bloque(&l, linea)) { free(datos); if (error && tam_error > 0) { strncpy(error, g_error, (size_t)tam_error - 1); error[tam_error - 1] = 0; } return 0; }
    free(datos);

    if (p->ancho < 320 || p->ancho > 4096) p->ancho = 320;
    if (p->alto < 200 || p->alto > 4096) p->alto = 240;
    if (p->fps < 1 || p->fps > 1000) p->fps = 60;
    if (p->velocidad < 1 || p->velocidad > 10) p->velocidad = 4;
    if (!p->camara.definida || p->camara.zoom < 1) p->camara.zoom = 1;

    {
        int i;
        for (i = 0; i < num_enlaces; i++) {
            int origen = indice_pieza_motor(p, enlaces[i].origen);
            int destino = indice_pieza_motor(p, enlaces[i].destino);
            if (origen < 0 || destino < 0) {
                return fallo_en(error, tam_error, enlaces[i].linea,
                                "conecta referencia un nodo inexistente");
            }
            p->piezas[origen].enlace = enlaces[i].destino;
        }
    }

    sistema_derivar_piezas(p);
    ordenar_scripts(p);
    return 1;
}
