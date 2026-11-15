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

#define L3_TOKENS_MAX 24

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
 * escapes \" \\ y \n; el resto de escapes se conservan tal cual (lector
 * tolerante). Devuelve el numero de tokens o -1 si algo no cabe. */
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

/* ------------------------------------------------------------------ */
/* Tabla de eventos vistos                                            */
/* ------------------------------------------------------------------ */

#define L3_EVENTOS_VISTOS 512

typedef struct {
    int objeto;
    int id;
    int tipo; /* 1 inicio, 2 fotograma, ... (los valores de LOGIC3) */
    int capa;
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

    /* objeto en curso: los eventos cuelgan de el */
    int objeto;
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

/* Cierra el bloque en curso: si es codigo.lobo de un evento fotograma,
 * se guarda como script del proyecto. */
static int cerrar_bloque(L3Lector *l, int linea) {
    MotorL3 *p = l->proy;
    int i, ev = -1;

    if (!l->bloque_activo) return 1;
    l->bloque_activo = 0;
    if (strcmp(l->bloque_tipo, "codigo.lobo") != 0) return 1;
    if (!l->bloque_tiene_codigo || !l->bloque_codigo[0]) return 1;

    /* El bloque referencia un id de evento; se busca el evento. Si hay
     * varios con el mismo id (ids por objeto), gana el fotograma. */
    for (i = 0; i < l->num_eventos; i++) {
        if (l->eventos[i].id != l->bloque_evento) continue;
        if (ev < 0 || l->eventos[i].tipo == 2) ev = i;
        if (l->eventos[i].tipo == 2) break;
    }
    if (ev < 0) return 1;                /* evento desconocido: se ignora */
    if (l->eventos[ev].tipo != 2) return 1; /* solo fotograma en la fase 25 */

    if (p->num_scripts >= MOTOR_L3_SCRIPTS_MAX)
        return fallo(linea, "demasiados scripts codigo.lobo");

    {
        MotorL3Script *s = &p->scripts[p->num_scripts++];
        s->bloque = l->bloque_id;
        s->objeto = l->eventos[ev].objeto;
        s->evento = l->eventos[ev].id;
        s->orden = l->bloque_orden;
        s->capa = l->eventos[ev].capa;
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

int motor_l3_cargar(const char *ruta, MotorL3 *p, char *error, int tam_error) {
    char *datos;
    int largo = 0, linea = 0;
    const char *q;
    L3Lector l;
    char salida[MOTOR_L3_CODIGO_MAX * 4 + 1024];
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
        /* --- escenas: se queda el mapa de la primera con mapa --- */
        if (es(salida, tokens, 0, "escena") && nt >= 3) {
            p->num_escenas++;
            if (!p->mapa[0] && nt >= 4 && tokens[3].comillas &&
                texto(salida, tokens, 3)[0]) {
                strncpy(p->mapa, texto(salida, tokens, 3),
                        sizeof(p->mapa) - 1);
                p->mapa[sizeof(p->mapa) - 1] = '\0';
            }
            goto siguiente;
        }
        /* --- objetos: id, capa y eventos hijos --- */
        if (es(salida, tokens, 0, "objeto") && nt >= 2) {
            int i;
            if (!cerrar_bloque(&l, linea)) { free(datos); if (error && tam_error > 0) { strncpy(error, g_error, (size_t)tam_error - 1); error[tam_error - 1] = 0; } return 0; }
            p->num_objetos++;
            l.objeto = entero(salida, tokens, 1);
            l.capa = 0;
            l.dentro_funcion = 0;
            for (i = 2; i + 1 < nt; i++) {
                if (es(salida, tokens, i, "capa"))
                    l.capa = entero(salida, tokens, i + 1);
            }
            goto siguiente;
        }
        /* --- funciones: sus eventos no son scripts de fotograma --- */
        if (es(salida, tokens, 0, "funcion") && nt >= 2) {
            if (!cerrar_bloque(&l, linea)) { free(datos); if (error && tam_error > 0) { strncpy(error, g_error, (size_t)tam_error - 1); error[tam_error - 1] = 0; } return 0; }
            l.objeto = 0;
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
                if (l.dentro_funcion) e->tipo = 9; /* llamada */
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

    ordenar_scripts(p);
    return 1;
}
