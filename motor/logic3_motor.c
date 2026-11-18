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
    int bloque_orden;
    int bloque_activo;
    char bloque_tipo[MOTOR_L3_NOMBRE_MAX];
    char bloque_codigo[MOTOR_L3_CODIGO_MAX + 1];
    int bloque_tiene_codigo;

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
    return 0;
}

/* Cierra el bloque en curso: si es codigo.lobo de un evento, se guarda
 * como script del proyecto con los parametros del evento. */
static int cerrar_bloque(L3Lector *l, int linea) {
    MotorL3 *p = l->proy;
    int i, ev = -1;

    if (!l->bloque_activo) return 1;
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
    FILE *f = fopen(ruta, "rb");
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

int motor_l3_cargar(const char *ruta, MotorL3 *p, char *error, int tam_error) {
    char *datos;
    int largo = 0, linea = 0;
    const char *q;
    L3Lector l;
    char salida[MOTOR_L3_CODIGO_MAX * 4 + 32768];
    L3Tok tokens[L3_TOKENS_MAX];

    if (error && tam_error > 0) error[0] = 0;
    if (!ruta || !p) return fallo_en(error, tam_error, 0, "ruta vacia");

    memset(p, 0, sizeof(*p));
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
                } else if (e->tipo == 8) {
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
            l.bloque_orden = 0;
            l.bloque_tipo[0] = '\0';
            l.bloque_codigo[0] = '\0';
            l.bloque_tiene_codigo = 0;
            for (i = 2; i + 1 < nt; i++) {
                if (es(salida, tokens, i, "evento"))
                    l.bloque_evento = entero(salida, tokens, i + 1);
                else if (es(salida, tokens, i, "orden"))
                    l.bloque_orden = entero(salida, tokens, i + 1);
                else if (es(salida, tokens, i, "tipo")) {
                    strncpy(l.bloque_tipo, texto(salida, tokens, i + 1),
                            sizeof(l.bloque_tipo) - 1);
                    l.bloque_tipo[sizeof(l.bloque_tipo) - 1] = '\0';
                }
            }
            l.bloque_activo = 1;
            goto siguiente;
        }
        if (es(salida, tokens, 0, "campo") && nt >= 3 && l.bloque_activo) {
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

    ordenar_scripts(p);
    return 1;
}
