/* lobo.c - lenguaje lobo: parser e interprete (fase 8).
 *
 * Dos mitades:
 *
 *   lobo_parsear   compila el texto de un boton FUNCIONES a una lista
 *                  plana de instrucciones (maquina de pila), con
 *                  mensajes de error "linea N: ..." para que el editor
 *                  los pinte en el propio boton.
 *   lobo_ejecutar  recorre la lista contra el estado del juego, una vez
 *                  por fotograma y por elemento del lienzo.
 *
 * C puro sin dependencias (ni windows.h): lo enlazan el editor (para
 * validar al escribir) y game.exe (para correr los scripts), y el
 * kernel loboswolfos lleva el puerto a Rust en kernel/src/lobo.rs con
 * la misma gramatica y la misma numeracion de opcodes.
 *
 * El parser es de dos pasadas: la primera preescanea los nombres que
 * se asignan en el programa (las variables locales del script) para
 * poder rechazar los nombres desconocidos (los errores tipograficos
 * dan error de sintaxis en vez de leer 0 en silencio); la segunda
 * compila linea a linea con descenso recursivo para las expresiones y
 * una pila de bloques abiertos para si/sino/fin y mientras/fin.
 */

#include "lobo.h"

/* ------------------------------------------------------------------ */
/* Utilidades de texto propias (sin libc, para portar igual en Rust)   */
/* ------------------------------------------------------------------ */

/* Copia un nombre con tope y rellena con ceros (como los arrays fijos
 * del kernel). */
static void lobo_copiar(char *dst, const char *src, int tam) {
    int i;
    for (i = 0; i < tam - 1 && src[i]; i++) dst[i] = src[i];
    for (; i < tam; i++) dst[i] = 0;
}

/* Comparacion de cadenas terminadas en cero. */
static int lobo_igual(const char *a, const char *b) {
    int i;
    for (i = 0; a[i] && a[i] == b[i]; i++) {}
    return a[i] == b[i];
}

/* ------------------------------------------------------------------ */
/* Tokens                                                             */
/* ------------------------------------------------------------------ */

#define LOBO_TOKENS_LINEA 48 /* tokens por linea */

typedef enum { TOK_NUM = 0, TOK_ID, TOK_OP, TOK_NADA } LoboTokTipo;

typedef struct {
    int tipo;
    char texto[LOBO_NOMBRE_MAX]; /* identificador u operador */
    int numero;                  /* valor si tipo == TOK_NUM */
} LoboToken;

/* Es espacio en blanco. */
static int lobo_espacio(char c) { return c == ' ' || c == '\t'; }

/* Es inicial de identificador. */
static int lobo_letra(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/* Es continuacion de identificador. */
static int lobo_alfanum(char c) {
    return lobo_letra(c) || (c >= '0' && c <= '9');
}

/* Parte una linea en tokens. Devuelve el numero de tokens; `op` de dos
 * caracteres se junta en uno (==, !=, <=, >=). El # corta la linea. */
static int lobo_tokenizar(const char *linea, int largo,
                          LoboToken *tokens, int maximo) {
    int n = 0, i = 0;
    while (i < largo && n < maximo) {
        char c = linea[i];
        if (lobo_espacio(c)) { i++; continue; }
        if (c == '#') break; /* comentario hasta el final de la linea */
        if (c >= '0' && c <= '9') {
            int v = 0, digitos = 0;
            while (i < largo && linea[i] >= '0' && linea[i] <= '9') {
                v = v * 10 + (linea[i] - '0');
                i++;
                digitos++;
                if (digitos > 9) { v = 999999999; }
            }
            tokens[n].tipo = TOK_NUM;
            tokens[n].numero = v;
            tokens[n].texto[0] = 0;
            n++;
            continue;
        }
        if (lobo_letra(c)) {
            int j = 0;
            while (i < largo && lobo_alfanum(linea[i])) {
                if (j < LOBO_NOMBRE_MAX - 1) tokens[n].texto[j] = linea[i];
                j++;
                i++;
            }
            if (j >= LOBO_NOMBRE_MAX) return -1; /* nombre demasiado largo */
            tokens[n].texto[j] = 0;
            tokens[n].tipo = TOK_ID;
            tokens[n].numero = 0;
            n++;
            continue;
        }
        /* Operadores de dos caracteres primero. */
        if (i + 1 < largo) {
            char c2 = linea[i + 1];
            if ((c == '=' && c2 == '=') || (c == '!' && c2 == '=') ||
                (c == '<' && c2 == '=') || (c == '>' && c2 == '=')) {
                tokens[n].tipo = TOK_OP;
                tokens[n].texto[0] = c;
                tokens[n].texto[1] = c2;
                tokens[n].texto[2] = 0;
                tokens[n].numero = 0;
                n++;
                i += 2;
                continue;
            }
        }
        if (c == '=' || c == '!' || c == '<' || c == '>' || c == '+' ||
            c == '-' || c == '*' || c == '/' || c == '%' || c == '&' ||
            c == '|' || c == '(' || c == ')' || c == ',') {
            tokens[n].tipo = TOK_OP;
            tokens[n].texto[0] = c;
            tokens[n].texto[1] = 0;
            tokens[n].numero = 0;
            n++;
            i++;
            continue;
        }
        return -2; /* caracter inesperado */
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Parser                                                             */
/* ------------------------------------------------------------------ */

/* Tipos de bloque abierto (si/sino y mientras). */
#define BLOQUE_SI       0 /* si ... (sin sino todavia) */
#define BLOQUE_SINO     1 /* si ... sino ... */
#define BLOQUE_MIENTRAS 2

typedef struct {
    int tipo;
    int linea;           /* linea donde se abrio, para los errores */
    int salto_falso;     /* instruccion SALTO_FALSO a parchear al fin */
    int salto_sino;      /* SALTO del sino a parchear al fin (-1 si no) */
    int mientras_inicio; /* instruccion donde empieza la condicion */
} LoboBloque;

typedef struct {
    LoboPrograma *prog;
    char *error;
    int tam_error;
    int linea;                    /* linea actual (1-based) */
    LoboToken tokens[LOBO_TOKENS_LINEA];
    int num_tokens;
    int pos;                      /* token actual de la linea */
    LoboBloque bloques[16];
    int num_bloques;
    char locales[LOBO_LOCALES_MAX][LOBO_NOMBRE_MAX];
    int num_locales;
} LoboParser;

/* Deja el mensaje de error con el numero de linea (el primero gana). */
static void lobo_error(LoboParser *p, const char *mensaje) {
    if (p->error && p->tam_error > 0 && p->error[0] == 0) {
        int i, j = 0;
        const char prefijo[] = "linea ";
        for (i = 0; prefijo[i] && j < p->tam_error - 1; i++, j++)
            p->error[j] = prefijo[i];
        /* numero de linea a mano (sin sprintf: cero dependencias) */
        {
            char num[12];
            int n = 0, k;
            int v = p->linea;
            if (v <= 0) v = 1;
            do { num[n++] = (char)('0' + v % 10); v /= 10; } while (v);
            for (k = n - 1; k >= 0 && j < p->tam_error - 1; k--)
                p->error[j++] = num[k];
        }
        if (j < p->tam_error - 1) p->error[j++] = ':';
        if (j < p->tam_error - 1) p->error[j++] = ' ';
        for (i = 0; mensaje[i] && j < p->tam_error - 1; i++, j++)
            p->error[j] = mensaje[i];
        p->error[j] = 0;
    }
}

/* Anade una instruccion al programa; 0 si ya no cabe. */
static int lobo_emitir(LoboParser *p, int op, int valor) {
    if (p->prog->largo >= LOBO_INSTS_MAX) {
        lobo_error(p, "el programa es demasiado largo");
        return 0;
    }
    p->prog->insts[p->prog->largo].op = (unsigned char)op;
    p->prog->insts[p->prog->largo].valor = valor;
    p->prog->largo++;
    return 1;
}

/* Cambia el destino de un salto ya emitido (parcheo de bloques). */
static void lobo_parchear(LoboPrograma *prog, int indice, int destino) {
    prog->insts[indice].valor = destino;
}

/* Indice de variable del juego, tecla o funcion integrada que responde
 * a ese nombre; -1 si no es ninguna de las conocidas. */
static int lobo_nombre_conocido(const char *nombre, int *es_tecla,
                                int *es_funcion) {
    *es_tecla = 0;
    *es_funcion = 0;
    if (lobo_igual(nombre, "x")) return LOBO_VAR_X;
    if (lobo_igual(nombre, "y")) return LOBO_VAR_Y;
    if (lobo_igual(nombre, "vel")) return LOBO_VAR_VEL;
    if (lobo_igual(nombre, "fotograma")) return LOBO_VAR_FOTOGRAMA;
    *es_tecla = 1;
    if (lobo_igual(nombre, "tecla_izquierda")) return LOBO_TECLA_IZQUIERDA;
    if (lobo_igual(nombre, "tecla_derecha")) return LOBO_TECLA_DERECHA;
    if (lobo_igual(nombre, "tecla_arriba")) return LOBO_TECLA_ARRIBA;
    if (lobo_igual(nombre, "tecla_abajo")) return LOBO_TECLA_ABAJO;
    if (lobo_igual(nombre, "tecla_salto")) return LOBO_TECLA_SALTO;
    if (lobo_igual(nombre, "tecla_cerrar")) return LOBO_TECLA_CERRAR;
    *es_tecla = 0;
    *es_funcion = 1;
    if (lobo_igual(nombre, "choca")) return 0;
    if (lobo_igual(nombre, "cerrar")) return 0;
    *es_funcion = 0;
    return -1;
}

/* Indice de la local con ese nombre (-1 si no esta preescaneada). */
static int lobo_buscar_local(LoboParser *p, const char *nombre) {
    int i;
    for (i = 0; i < p->num_locales; i++)
        if (lobo_igual(p->locales[i], nombre)) return i;
    return -1;
}

/* Registra un nombre como variable local del script. */
static int lobo_nueva_local(LoboParser *p, const char *nombre) {
    int i = lobo_buscar_local(p, nombre);
    if (i >= 0) return i;
    if (p->num_locales >= LOBO_LOCALES_MAX) {
        lobo_error(p, "demasiadas variables locales (maximo 16)");
        return -1;
    }
    lobo_copiar(p->locales[p->num_locales], nombre, LOBO_NOMBRE_MAX);
    return p->num_locales++;
}

/* ------------------------------------------------------------------ */
/* Expresiones (descenso recursivo sobre los tokens de la linea)       */
/* ------------------------------------------------------------------ */

static int lobo_expresion(LoboParser *p);

/* Token actual ya consumido? */
static int lobo_fin_tokens(LoboParser *p) { return p->pos >= p->num_tokens; }

/* Mira el operador actual sin comerlo; 0 si no hay token. */
static const char *lobo_mirar_op(LoboParser *p) {
    if (lobo_fin_tokens(p) || p->tokens[p->pos].tipo != TOK_OP)
        return 0;
    return p->tokens[p->pos].texto;
}

/* Come el token actual si es ese operador exacto. */
static int lobo_comer_op(LoboParser *p, const char *op) {
    const char *actual = lobo_mirar_op(p);
    if (actual && lobo_igual(actual, op)) {
        p->pos++;
        return 1;
    }
    return 0;
}

/* Come un operador de un conjunto dado en "cadenas"; devuelve el indice
 * o -1. Cada entrada mide 2 chars (una de ellas puede valer ""=fin). */
static int lobo_comer_cualquiera(LoboParser *p, const char *const *ops,
                                 int num) {
    int i;
    for (i = 0; i < num; i++) {
        if (ops[i][0] && lobo_comer_op(p, ops[i])) return i;
    }
    return -1;
}

/* Primario: numero, variable, tecla o llamada a funcion integrada. */
static int lobo_primario(LoboParser *p) {
    if (lobo_fin_tokens(p)) {
        lobo_error(p, "se esperaba una expresion");
        return 0;
    }
    if (p->tokens[p->pos].tipo == TOK_NUM) {
        int v = p->tokens[p->pos].numero;
        p->pos++;
        return lobo_emitir(p, LOBO_NUM, v);
    }
    if (p->tokens[p->pos].tipo == TOK_ID) {
        char nombre[LOBO_NOMBRE_MAX];
        int es_tecla, es_funcion, conocido;
        lobo_copiar(nombre, p->tokens[p->pos].texto, LOBO_NOMBRE_MAX);
        p->pos++;
        conocido = lobo_nombre_conocido(nombre, &es_tecla, &es_funcion);
        if (es_funcion) {
            /* llamada: choca(x, y) o cerrar() */
            if (!lobo_comer_op(p, "(")) {
                lobo_error(p, "se esperaba ( despues del nombre");
                return 0;
            }
            if (lobo_igual(nombre, "cerrar")) {
                if (!lobo_comer_op(p, ")")) {
                    lobo_error(p, "cerrar() no lleva argumentos");
                    return 0;
                }
                return lobo_emitir(p, LOBO_CERRAR, 0);
            }
            if (!lobo_expresion(p)) return 0;
            if (!lobo_comer_op(p, ",")) {
                lobo_error(p, "choca necesita dos argumentos");
                return 0;
            }
            if (!lobo_expresion(p)) return 0;
            if (!lobo_comer_op(p, ")")) {
                lobo_error(p, "se esperaba ) para cerrar la llamada");
                return 0;
            }
            return lobo_emitir(p, LOBO_CHOCAR, 0);
        }
        if (es_tecla) return lobo_emitir(p, LOBO_TECLA, conocido);
        if (conocido >= 0) return lobo_emitir(p, LOBO_VAR, conocido);
        {
            int local = lobo_buscar_local(p, nombre);
            if (local < 0) {
                lobo_error(p, "nombre desconocido");
                return 0;
            }
            return lobo_emitir(p, LOBO_VAR, LOBO_VAR_LOCALES + local);
        }
    }
    if (lobo_comer_op(p, "(")) {
        if (!lobo_expresion(p)) return 0;
        if (!lobo_comer_op(p, ")")) {
            lobo_error(p, "se esperaba )");
            return 0;
        }
        return 1;
    }
    lobo_error(p, "se esperaba una expresion");
    return 0;
}

/* Unario: -expr, no expr, !expr o un primario. */
static int lobo_unario(LoboParser *p) {
    if (lobo_comer_op(p, "-")) {
        if (!lobo_emitir(p, LOBO_NUM, 0)) return 0;
        if (!lobo_unario(p)) return 0;
        return lobo_emitir(p, LOBO_RESTA, 0);
    }
    /* "!" es un operador: comer_op ya avanza el cursor. */
    if (lobo_comer_op(p, "!")) {
        if (!lobo_unario(p)) return 0;
        return lobo_emitir(p, LOBO_NO, 0);
    }
    /* "no" es un identificador: hay que avanzar el cursor a mano. */
    if (p->pos < p->num_tokens && p->tokens[p->pos].tipo == TOK_ID &&
        lobo_igual(p->tokens[p->pos].texto, "no")) {
        p->pos++;
        if (!lobo_unario(p)) return 0;
        return lobo_emitir(p, LOBO_NO, 0);
    }
    return lobo_primario(p);
}

/* Producto: * / % */
static int lobo_producto(LoboParser *p) {
    static const char *const ops[] = { "*", "/", "%" };
    if (!lobo_unario(p)) return 0;
    for (;;) {
        int cual = lobo_comer_cualquiera(p, ops, 3);
        if (cual < 0) return 1;
        if (!lobo_unario(p)) return 0;
        if (!lobo_emitir(p, LOBO_MULT + cual, 0)) return 0;
    }
}

/* Suma: + - */
static int lobo_suma(LoboParser *p) {
    static const char *const ops[] = { "+", "-" };
    if (!lobo_producto(p)) return 0;
    for (;;) {
        int cual = lobo_comer_cualquiera(p, ops, 2);
        if (cual < 0) return 1;
        if (!lobo_producto(p)) return 0;
        if (!lobo_emitir(p, LOBO_SUMA + cual, 0)) return 0;
    }
}

/* Comparacion: == != < > <= >= (una sola, sin encadenar). */
static int lobo_comparacion(LoboParser *p) {
    static const char *const ops[] = { "==", "!=", "<", ">", "<=", ">=" };
    if (!lobo_suma(p)) return 0;
    {
        int cual = lobo_comer_cualquiera(p, ops, 6);
        if (cual < 0) return 1;
        if (!lobo_suma(p)) return 0;
        return lobo_emitir(p, LOBO_IGUAL + cual, 0);
    }
}

/* Y logico: & */
static int lobo_y(LoboParser *p) {
    if (!lobo_comparacion(p)) return 0;
    while (lobo_comer_op(p, "&")) {
        if (!lobo_comparacion(p)) return 0;
        if (!lobo_emitir(p, LOBO_Y, 0)) return 0;
    }
    return 1;
}

/* O logico: | */
static int lobo_o(LoboParser *p) {
    if (!lobo_y(p)) return 0;
    while (lobo_comer_op(p, "|")) {
        if (!lobo_y(p)) return 0;
        if (!lobo_emitir(p, LOBO_O, 0)) return 0;
    }
    return 1;
}

static int lobo_expresion(LoboParser *p) { return lobo_o(p); }

/* ------------------------------------------------------------------ */
/* Pasada de preescaneo: nombres asignados = variables locales        */
/* ------------------------------------------------------------------ */

/* Recorre todas las lineas y registra cada "nombre =" como local. Con
 * eso el parseo puede rechazar nombres desconocidos (errores tipograficos
 * como "telca_derecha" en vez de leer 0 en silencio). */
static int lobo_preescanear(LoboParser *p, const char *texto) {
    const char *q = texto;
    int linea = 1;
    while (*q) {
        const char *fin = q;
        LoboToken tokens[LOBO_TOKENS_LINEA];
        int n, i, largo;
        while (*fin && *fin != '\n') fin++;
        largo = (int)(fin - q);
        /* quitar el \r del final si el texto viene de un archivo CRLF */
        if (largo > 0 && q[largo - 1] == '\r') largo--;
        p->linea = linea;
        n = lobo_tokenizar(q, largo, tokens, LOBO_TOKENS_LINEA);
        if (n < 0) {
            lobo_error(p, n == -1 ? "nombre demasiado largo"
                                  : "caracter inesperado");
            return 0;
        }
        for (i = 0; i + 1 < n; i++) {
            if (tokens[i].tipo == TOK_ID && tokens[i + 1].tipo == TOK_OP &&
                tokens[i + 1].texto[0] == '=' && tokens[i + 1].texto[1] == 0) {
                int es_tecla, es_funcion;
                int conocido =
                    lobo_nombre_conocido(tokens[i].texto, &es_tecla, &es_funcion);
                if (conocido >= 0 && !es_funcion) continue; /* global */
                if (es_tecla) {
                    p->linea = linea;
                    lobo_error(p, "las teclas no se pueden asignar");
                    return 0;
                }
                if (es_funcion) {
                    p->linea = linea;
                    lobo_error(p, "no se puede asignar a una funcion");
                    return 0;
                }
                if (lobo_nueva_local(p, tokens[i].texto) < 0) {
                    p->linea = linea;
                    return 0;
                }
            }
        }
        if (*fin == '\n') fin++;
        q = fin;
        linea++;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Compilacion de una linea                                            */
/* ------------------------------------------------------------------ */

/* Compila la linea actual de p->tokens como sentencia. */
static int lobo_sentencia(LoboParser *p) {
    LoboToken *t = &p->tokens[0];

    if (t->tipo == TOK_ID) {
        /* Palabras del lenguaje. */
        if (lobo_igual(t->texto, "si")) {
            LoboBloque *b;
            p->pos = 1;
            if (!lobo_expresion(p)) return 0;
            if (!lobo_fin_tokens(p)) {
                lobo_error(p, "sobran datos tras la condicion del si");
                return 0;
            }
            if (p->num_bloques >= 16) {
                lobo_error(p, "demasiados bloques anidados");
                return 0;
            }
            b = &p->bloques[p->num_bloques++];
            b->tipo = BLOQUE_SI;
            b->linea = p->linea;
            b->salto_sino = -1;
            b->mientras_inicio = -1;
            b->salto_falso = p->prog->largo;
            return lobo_emitir(p, LOBO_SALTO_FALSO, 0);
        }
        if (lobo_igual(t->texto, "sino")) {
            LoboBloque *b;
            if (p->num_bloques == 0 || p->bloques[p->num_bloques - 1].tipo != BLOQUE_SI) {
                lobo_error(p, "sino sin un si abierto");
                return 0;
            }
            if (p->num_tokens != 1) {
                lobo_error(p, "sino va solo en la linea");
                return 0;
            }
            b = &p->bloques[p->num_bloques - 1];
            b->salto_sino = p->prog->largo;
            if (!lobo_emitir(p, LOBO_SALTO, 0)) return 0;
            /* el cuerpo del si salta aqui (al principio del sino) */
            lobo_parchear(p->prog, b->salto_falso, p->prog->largo);
            b->tipo = BLOQUE_SINO;
            return 1;
        }
        if (lobo_igual(t->texto, "fin")) {
            LoboBloque *b;
            if (p->num_bloques == 0) {
                lobo_error(p, "fin sin bloque abierto");
                return 0;
            }
            if (p->num_tokens != 1) {
                lobo_error(p, "fin va solo en la linea");
                return 0;
            }
            b = &p->bloques[p->num_bloques - 1];
            if (b->tipo == BLOQUE_MIENTRAS) {
                if (!lobo_emitir(p, LOBO_SALTO, b->mientras_inicio)) return 0;
                lobo_parchear(p->prog, b->salto_falso, p->prog->largo);
            } else {
                /* si a pelo: el falso salta aqui; con sino: el salto
                 * del sino apunta aqui. */
                if (b->salto_sino >= 0)
                    lobo_parchear(p->prog, b->salto_sino, p->prog->largo);
                else
                    lobo_parchear(p->prog, b->salto_falso, p->prog->largo);
            }
            p->num_bloques--;
            return 1;
        }
        if (lobo_igual(t->texto, "mientras")) {
            LoboBloque *b;
            p->pos = 1;
            if (p->num_bloques >= 16) {
                lobo_error(p, "demasiados bloques anidados");
                return 0;
            }
            b = &p->bloques[p->num_bloques++];
            b->tipo = BLOQUE_MIENTRAS;
            b->linea = p->linea;
            b->salto_sino = -1;
            b->mientras_inicio = p->prog->largo;
            if (!lobo_expresion(p)) return 0;
            if (!lobo_fin_tokens(p)) {
                lobo_error(p, "sobran datos tras la condicion del mientras");
                return 0;
            }
            b->salto_falso = p->prog->largo;
            return lobo_emitir(p, LOBO_SALTO_FALSO, 0);
        }
        /* Asignacion: nombre = expresion. */
        if (p->num_tokens >= 2 && p->tokens[1].tipo == TOK_OP &&
            p->tokens[1].texto[0] == '=' && p->tokens[1].texto[1] == 0) {
            int es_tecla, es_funcion;
            int conocido =
                lobo_nombre_conocido(t->texto, &es_tecla, &es_funcion);
            int destino = -1;
            if (es_tecla) {
                lobo_error(p, "las teclas no se pueden asignar");
                return 0;
            }
            if (es_funcion) {
                lobo_error(p, "no se puede asignar a una funcion");
                return 0;
            }
            if (conocido >= 0) destino = conocido;
            else {
                int local = lobo_buscar_local(p, t->texto);
                if (local < 0) {
                    lobo_error(p, "nombre desconocido");
                    return 0;
                }
                destino = LOBO_VAR_LOCALES + local;
            }
            p->pos = 2;
            if (!lobo_expresion(p)) return 0;
            if (!lobo_fin_tokens(p)) {
                lobo_error(p, "sobran datos tras la asignacion");
                return 0;
            }
            return lobo_emitir(p, LOBO_ASIGNA, destino);
        }
    }

    /* Expresion suelta (p. ej. cerrar()): se evalua y se descarta. */
    p->pos = 0;
    if (!lobo_expresion(p)) return 0;
    if (!lobo_fin_tokens(p)) {
        lobo_error(p, "sobran datos al final de la linea");
        return 0;
    }
    return lobo_emitir(p, LOBO_POP, 0);
}

/* ------------------------------------------------------------------ */
/* lobo_parsear                                                        */
/* ------------------------------------------------------------------ */

int lobo_parsear(const char *texto, LoboPrograma *programa,
                 char *error, int tam_error) {
    LoboParser p;
    const char *q = texto;
    int linea = 1;

    if (error && tam_error > 0) error[0] = 0;
    if (!texto || !programa) {
        if (error && tam_error > 0)
            lobo_copiar(error, "programa vacio", tam_error);
        return 0;
    }
    programa->largo = 0;

    {
        int i;
        for (i = 0; i < LOBO_INSTS_MAX; i++) {
            programa->insts[i].op = 0;
            programa->insts[i].valor = 0;
        }
    }

    p.prog = programa;
    p.error = error;
    p.tam_error = tam_error;
    p.linea = 1;
    p.num_tokens = 0;
    p.pos = 0;
    p.num_bloques = 0;
    p.num_locales = 0;
    {
        int i;
        for (i = 0; i < LOBO_LOCALES_MAX; i++) p.locales[i][0] = 0;
    }

    /* 1. Preescaneo de locales. */
    if (!lobo_preescanear(&p, texto)) return 0;
    if (error && tam_error > 0 && error[0]) return 0;

    /* 2. Compilacion linea a linea. */
    while (*q) {
        const char *fin = q;
        int n;
        while (*fin && *fin != '\n') fin++;
        /* quitar el \r del final si viene de un archivo de Windows */
        {
            int largo = (int)(fin - q);
            if (largo > 0 && q[largo - 1] == '\r') largo--;
            p.linea = linea;
            n = lobo_tokenizar(q, largo, p.tokens, LOBO_TOKENS_LINEA);
            if (n < 0) {
                lobo_error(&p, n == -1 ? "nombre demasiado largo"
                                      : "caracter inesperado");
                return 0;
            }
            p.num_tokens = n;
            p.pos = 0;
            if (n > 0) {
                if (!lobo_sentencia(&p)) return 0;
                if (error && tam_error > 0 && error[0]) return 0;
            }
        }
        if (*fin == '\n') fin++;
        q = fin;
        linea++;
    }

    if (p.num_bloques > 0) {
        int linea_bloque = p.bloques[p.num_bloques - 1].linea;
        p.linea = linea_bloque;
        lobo_error(&p, "falta el fin del bloque");
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Interprete                                                          */
/* ------------------------------------------------------------------ */

/* Lee una variable (global del juego o local del script). */
static int lobo_leer_var(const LoboEstado *st, const LoboMaquina *m,
                         int var) {
    switch (var) {
    case LOBO_VAR_X: return st->x;
    case LOBO_VAR_Y: return st->y;
    case LOBO_VAR_VEL: return st->vel;
    case LOBO_VAR_FOTOGRAMA: return st->fotograma;
    default:
        if (var >= LOBO_VAR_LOCALES && var < LOBO_VAR_LOCALES + LOBO_LOCALES_MAX)
            return m->locales[var - LOBO_VAR_LOCALES];
        return 0;
    }
}

/* Escribe una variable. */
static void lobo_escribir_var(LoboEstado *st, LoboMaquina *m, int var,
                              int valor) {
    switch (var) {
    case LOBO_VAR_X: st->x = valor; return;
    case LOBO_VAR_Y: st->y = valor; return;
    case LOBO_VAR_VEL: st->vel = valor; return;
    case LOBO_VAR_FOTOGRAMA: st->fotograma = valor; return;
    default:
        if (var >= LOBO_VAR_LOCALES && var < LOBO_VAR_LOCALES + LOBO_LOCALES_MAX)
            m->locales[var - LOBO_VAR_LOCALES] = valor;
    }
}

/* Entero mas pequeno posible, sin limits.h. */
#define LOBO_INT_MIN (-2147483647 - 1)

void lobo_ejecutar(const LoboPrograma *programa, LoboEstado *estado,
                   LoboMaquina *maquina) {
    int pila[LOBO_PILA_MAX];
    int np = 0;       /* valores apilados */
    long pasos = LOBO_PASOS_MAX;
    int i = 0;

    maquina->cerrar = 0;
    maquina->agotado = 0;

    while (i < programa->largo) {
        const LoboInst *in = &programa->insts[i];
        int a, b;
        if (--pasos < 0) {
            maquina->agotado = 1;
            return;
        }
        switch (in->op) {
        case LOBO_NUM:
            if (np >= LOBO_PILA_MAX) { maquina->agotado = 1; return; }
            pila[np++] = in->valor;
            break;
        case LOBO_VAR:
            if (np >= LOBO_PILA_MAX) { maquina->agotado = 1; return; }
            pila[np++] = lobo_leer_var(estado, maquina, in->valor);
            break;
        case LOBO_TECLA:
            if (np >= LOBO_PILA_MAX) { maquina->agotado = 1; return; }
            pila[np++] = (estado->entrada & (unsigned)in->valor) != 0;
            break;
        case LOBO_CHOCAR:
            if (np < 2) { maquina->agotado = 1; return; }
            b = pila[--np];
            a = pila[--np];
            if (np >= LOBO_PILA_MAX) { maquina->agotado = 1; return; }
            if (estado->choca)
                pila[np++] = estado->choca(estado->ctx, a, b) != 0;
            else
                pila[np++] = 0;
            break;
        case LOBO_CERRAR:
            maquina->cerrar = 1;
            if (np >= LOBO_PILA_MAX) { maquina->agotado = 1; return; }
            pila[np++] = 0;
            break;
        case LOBO_SUMA:
            if (np < 2) { maquina->agotado = 1; return; }
            b = pila[--np]; a = pila[--np];
            pila[np++] = (int)((unsigned)a + (unsigned)b);
            break;
        case LOBO_RESTA:
            if (np < 2) { maquina->agotado = 1; return; }
            b = pila[--np]; a = pila[--np];
            pila[np++] = (int)((unsigned)a - (unsigned)b);
            break;
        case LOBO_MULT:
            if (np < 2) { maquina->agotado = 1; return; }
            b = pila[--np]; a = pila[--np];
            pila[np++] = (int)((unsigned)a * (unsigned)b);
            break;
        case LOBO_DIV:
            if (np < 2) { maquina->agotado = 1; return; }
            b = pila[--np]; a = pila[--np];
            pila[np++] = (b == 0 || (a == LOBO_INT_MIN && b == -1)) ? 0 : a / b;
            break;
        case LOBO_MOD:
            if (np < 2) { maquina->agotado = 1; return; }
            b = pila[--np]; a = pila[--np];
            pila[np++] = (b == 0 || (a == LOBO_INT_MIN && b == -1)) ? 0 : a % b;
            break;
        case LOBO_IGUAL:
        case LOBO_DISTINTO:
        case LOBO_MENOR:
        case LOBO_MAYOR:
        case LOBO_MENOR_IG:
        case LOBO_MAYOR_IG:
            if (np < 2) { maquina->agotado = 1; return; }
            b = pila[--np]; a = pila[--np];
            switch (in->op) {
            case LOBO_IGUAL: a = a == b; break;
            case LOBO_DISTINTO: a = a != b; break;
            case LOBO_MENOR: a = a < b; break;
            case LOBO_MAYOR: a = a > b; break;
            case LOBO_MENOR_IG: a = a <= b; break;
            default: a = a >= b; break;
            }
            pila[np++] = a;
            break;
        case LOBO_Y:
        case LOBO_O:
            if (np < 2) { maquina->agotado = 1; return; }
            b = pila[--np]; a = pila[--np];
            pila[np++] = in->op == LOBO_Y ? (a != 0 && b != 0)
                                          : (a != 0 || b != 0);
            break;
        case LOBO_NO:
            if (np < 1) { maquina->agotado = 1; return; }
            a = pila[np - 1];
            pila[np - 1] = a == 0;
            break;
        case LOBO_ASIGNA:
            if (np < 1) { maquina->agotado = 1; return; }
            lobo_escribir_var(estado, maquina, in->valor, pila[--np]);
            break;
        case LOBO_SALTO:
            i = in->valor;
            continue;
        case LOBO_SALTO_FALSO:
            if (np < 1) { maquina->agotado = 1; return; }
            a = pila[--np];
            if (a == 0) {
                i = in->valor;
                continue;
            }
            break;
        case LOBO_POP:
            if (np > 0) np--;
            break;
        default:
            maquina->agotado = 1;
            return;
        }
        i++;
    }
}

void lobo_maquina_reiniciar(LoboMaquina *maquina) {
    int i;
    for (i = 0; i < LOBO_LOCALES_MAX; i++) maquina->locales[i] = 0;
    maquina->cerrar = 0;
    maquina->agotado = 0;
}
