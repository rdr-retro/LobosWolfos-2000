/* sistema.c - lectura y validacion del modelo de programa (fase 46). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sistema.h"

static void copiar(char *dst, int tam, const char *src) {
    int i;
    if (!dst || tam <= 0) return;
    for (i = 0; src && src[i] && i < tam - 1; i++) dst[i] = src[i];
    for (; i < tam; i++) dst[i] = 0;
}

static int entero_valido(const char *texto, int *valor) {
    char *fin = NULL;
    long n;
    if (!texto || !*texto) return 0;
    n = strtol(texto, &fin, 10);
    if (!fin || *fin) return 0;
    if (n < -2147483647L || n > 2147483647L) return 0;
    *valor = (int)n;
    return 1;
}

static const char *campo_valor(const Bloque3 *b, const char *clave) {
    int i;
    for (i = 0; i < b->num_campos; i++)
        if (strcmp(b->campos[i].clave, clave) == 0)
            return b->campos[i].valor;
    return "";
}

static int campo_entero(const Bloque3 *b, const char *clave, int *valor) {
    return entero_valido(campo_valor(b, clave), valor);
}

static int es_binario(int n) { return n == 0 || n == 1; }

static int dispositivo_de(const char *nombre) {
    if (!nombre || !*nombre) return SISTEMA_DISPOSITIVO_TECLADO;
    if (strcmp(nombre, "raton") == 0) return SISTEMA_DISPOSITIVO_RATON;
    if (strcmp(nombre, "mando") == 0) return SISTEMA_DISPOSITIVO_MANDO;
    if (strcmp(nombre, "teclado") == 0) return SISTEMA_DISPOSITIVO_TECLADO;
    return -1;
}

static int esquema_conocido(const char *nombre) {
    return strcmp(nombre, "plataformas") == 0 ||
           strcmp(nombre, "superior") == 0 ||
           strcmp(nombre, "personalizado") == 0 ||
           strcmp(nombre, "mando_1") == 0 ||
           strcmp(nombre, "mando_2") == 0;
}

static int evento_conocido(int evento) {
    return evento >= 1 && evento <= 9;
}

static int funcion_existe(const Proyecto3 *p, const char *nombre) {
    int i;
    if (!nombre || !*nombre) return 0;
    for (i = 0; i < p->num_funciones; i++)
        if (strcmp(p->funciones[i].nombre, nombre) == 0) return 1;
    return 0;
}

int sistema_es_bloque(const Bloque3 *b) {
    return b && strncmp(b->tipo, "sistema.", 8) == 0;
}

int sistema_jugador_implicito(const SistemaPrograma *s) {
    if (!s || !s->opciones.presente) return 1;
    return s->opciones.jugador_implicito ? 1 : 0;
}

int sistema_opciones_asegurar(Proyecto3 *p) {
    Bloque3 *b = NULL;
    int i;

    if (!p) return 0;
    for (i = 0; i < p->num_bloques; i++) {
        if (strcmp(p->bloques[i].tipo, SISTEMA_TIPO_OPCIONES) == 0) return 1;
    }
    if (!logic3_bloque_nuevo(p, 1, 0, 0, SISTEMA_TIPO_OPCIONES, &b)) return 0;
    if (!b || b->num_campos + 3 > L3_CAMPOS_MAX) return 0;
    copiar(b->campos[b->num_campos].clave, L3_NOMBRE_MAX, "programa");
    copiar(b->campos[b->num_campos].valor, L3_TEXTO_MAX, "1");
    b->num_campos++;
    copiar(b->campos[b->num_campos].clave, L3_NOMBRE_MAX, "jugador_implicito");
    copiar(b->campos[b->num_campos].valor, L3_TEXTO_MAX, "0");
    b->num_campos++;
    copiar(b->campos[b->num_campos].clave, L3_NOMBRE_MAX, "auto_compilar");
    copiar(b->campos[b->num_campos].valor, L3_TEXTO_MAX, "1");
    b->num_campos++;
    return 1;
}

int sistema_leer(const Proyecto3 *p, SistemaPrograma *s,
                 char *error, int tam_error) {
    int bloque_control[SISTEMA_JUGADORES_MAX];
    int indice_control[SISTEMA_JUGADORES_MAX];
    int num_bloques_control = 0;
    int i, j;

    if (error && tam_error > 0) error[0] = 0;
    if (!p || !s) return 0;
    memset(s, 0, sizeof(*s));
    s->opciones.jugador_implicito = 1; /* valor legacy si no hay opciones */

    for (i = 0; i < p->num_bloques; i++) {
        const Bloque3 *b = &p->bloques[i];
        if (strcmp(b->tipo, SISTEMA_TIPO_OPCIONES) == 0) {
            SistemaOpciones *o = &s->opciones;
            if (o->presente) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "linea %d: opciones de programa duplicadas",
                                    b->linea);
                return 0;
            }
            memset(o, 0, sizeof(*o));
            o->presente = 1;
            o->linea = b->linea;
            o->jugador_implicito = 1;
            if (!campo_entero(b, "programa", &o->programa) ||
                !campo_entero(b, "jugador_implicito", &o->jugador_implicito) ||
                !campo_entero(b, "auto_compilar", &o->auto_compilar)) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "linea %d: opciones de programa incompletas",
                                    b->linea);
                return 0;
            }
            if (!es_binario(o->programa) ||
                !es_binario(o->jugador_implicito) ||
                !es_binario(o->auto_compilar)) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "linea %d: opciones de programa invalidas",
                                    b->linea);
                return 0;
            }
        } else if (strcmp(b->tipo, SISTEMA_TIPO_CONTROL) == 0) {
            SistemaControl *c;
            int jugador = 0, dispositivo;
            int ya = 0;
            if (!campo_entero(b, "jugador", &jugador) ||
                jugador < 1 || jugador > SISTEMA_JUGADORES_MAX) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "linea %d: jugador de control invalido",
                                    b->linea);
                return 0;
            }
            for (j = 0; j < s->num_controles; j++)
                if (s->controles[j].jugador == jugador) ya = 1;
            if (ya) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "linea %d: jugador duplicado", b->linea);
                return 0;
            }
            dispositivo = dispositivo_de(campo_valor(b, "dispositivo"));
            if (dispositivo < 0) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "linea %d: dispositivo desconocido", b->linea);
                return 0;
            }
            c = &s->controles[s->num_controles++];
            memset(c, 0, sizeof(*c));
            c->jugador = jugador;
            c->linea = b->linea;
            c->dispositivo = dispositivo;
            copiar(c->esquema, sizeof(c->esquema), campo_valor(b, "esquema"));
            c->esquema_activo = esquema_conocido(c->esquema);
            c->activo = 1;
            if (!campo_entero(b, "actor", &c->actor) || c->actor <= 0 ||
                !campo_entero(b, "activo", &c->activo) ||
                !es_binario(c->activo)) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "linea %d: control incompleto", b->linea);
                return 0;
            }
            if (!c->esquema_activo) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "linea %d: esquema desconocido", b->linea);
                return 0;
            }
            if (num_bloques_control < SISTEMA_JUGADORES_MAX) {
                bloque_control[num_bloques_control] = b->id;
                indice_control[num_bloques_control] = s->num_controles - 1;
                num_bloques_control++;
            }
        }
    }

    for (i = 0; i < p->num_bloques; i++) {
        const Bloque3 *b = &p->bloques[i];
        if (strcmp(b->tipo, SISTEMA_TIPO_ACCION) == 0) {
            SistemaAccion *a;
            int control = -1, flanco = 0;
            for (j = 0; j < num_bloques_control; j++)
                if (bloque_control[j] == b->padre)
                    control = indice_control[j];
            if (control < 0) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "linea %d: accion sin control padre", b->linea);
                return 0;
            }
            if (!campo_entero(b, "flanco", &flanco) ||
                flanco < SISTEMA_FLANCO_MANTENER ||
                flanco > SISTEMA_FLANCO_SOLTAR) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "linea %d: flanco de accion invalido", b->linea);
                return 0;
            }
            if (s->num_acciones >= SISTEMA_ACCIONES_MAX) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "demasiadas acciones de programa");
                return 0;
            }
            a = &s->acciones[s->num_acciones++];
            memset(a, 0, sizeof(*a));
            a->linea = b->linea;
            a->control = control;
            a->flanco = flanco;
            copiar(a->entrada, sizeof(a->entrada), campo_valor(b, "entrada"));
            copiar(a->funcion, sizeof(a->funcion), campo_valor(b, "funcion"));
            copiar(a->argumentos, sizeof(a->argumentos),
                   campo_valor(b, "argumentos"));
            if (!a->entrada[0] || !funcion_existe(p, a->funcion)) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "linea %d: accion sin funcion valida", b->linea);
                return 0;
            }
        } else if (strcmp(b->tipo, SISTEMA_TIPO_REGLA) == 0) {
            SistemaRegla *r;
            int evento = 0, flanco = 0, cada = 0;
            if (!campo_entero(b, "evento", &evento) ||
                !evento_conocido(evento)) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "linea %d: evento de regla invalido", b->linea);
                return 0;
            }
            if (s->num_reglas >= SISTEMA_REGLAS_MAX) {
                if (error) snprintf(error, (size_t)tam_error,
                                    "demasiadas reglas de programa");
                return 0;
            }
            r = &s->reglas[s->num_reglas++];
            memset(r, 0, sizeof(*r));
            r->linea = b->linea;
            r->evento = evento;
            campo_entero(b, "actor", &r->actor);
            campo_entero(b, "flanco", &flanco);
            campo_entero(b, "cada", &cada);
            r->flanco = flanco;
            r->cada = cada;
            copiar(r->funcion, sizeof(r->funcion), campo_valor(b, "funcion"));
            copiar(r->etiqueta, sizeof(r->etiqueta), campo_valor(b, "tag"));
        }
    }
    return 1;
}
