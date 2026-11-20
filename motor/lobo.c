/* lobo.c - lenguaje lobo: parser, interprete clasico y VM resumible.
 *
 * Tres mitades:
 *
 *   lobo_compilar   compila el texto de un boton FUNCIONES (modo clasico,
 *                   fase 8) o de un bloque codigo.lobo (modo VM, fase
 *                   25) a una lista plana de instrucciones (maquina de
 *                   pila), con mensajes "linea N: ..." para que el
 *                   editor los pinte en el propio boton.
 *   lobo_ejecutar   recorre la lista de una vez, una vez por fotograma y
 *                   por elemento del lienzo (modo clasico de LOGIC2).
 *   lobo_vm_*       ejecuta la misma lista con estado persistente entre
 *                   fotogramas: pc, pila, frames de llamada y espera
 *                   (lobo 3). `esperar` no bloquea a los demas scripts.
 *
 * C puro (solo malloc/realloc/free del runtime de Windows; nada de
 * windows.h): lo enlazan el editor (para validar al escribir) y
 * game.exe (para correr los scripts), y el kernel loboswolfos lleva el
 * puerto a Rust en kernel/src/lobo.rs con la misma gramatica y la misma
 * numeracion de opcodes (los de la fase 25 van al final para no
 * romperla).
 *
 * El parser es de dos pasadas: la primera preescanea los nombres que
 * se asignan en el programa (las variables locales del script, los
 * globales $, las variables de entidad @, las listas y las firmas de
 * las funciones) para poder rechazar los nombres desconocidos (los
 * errores tipograficos dan error de sintaxis en vez de leer 0 en
 * silencio); la segunda compila linea a linea con descenso recursivo
 * para las expresiones y una pila de bloques abiertos para
 * si/sino/fin, mientras/fin y funcion/fin.
 */

#include "lobo.h"
#include "entrada.h"
#include <stdlib.h>

/* Hooks del depurador (fase 34). Son opcionales: mientras no se
 * registran, las llamadas de abajo no cuestan nada y la VM se comporta
 * exactamente como en las fases 25 a 33. */
static void (*g_hook_instruccion)(int pc, int error) = NULL;
static int (*g_hook_debe_parar)(void) = NULL;

void lobo_depurador_instruccion(void (*fn)(int pc, int error)) {
    g_hook_instruccion = fn;
}

void lobo_depurador_parada(int (*fn)(void)) {
    g_hook_debe_parar = fn;
}

/* Fase 37: valor de una instruccion de entrada extendida. Con `entrada_ext`
 * a NULL (motores sin raton, --vm-test antiguo) todo devuelve 0, asi que
 * los programas que no usan estas teclas no cambian. */
static int lobo_entrada_valor(const LoboEstado *estado, int op, int valor) {
    const MotorEntrada *e = estado ? estado->entrada_ext : NULL;
    switch (op) {
    case LOBO_TECLA_PULSADA:   return motor_entrada_pulsada(e, valor);
    case LOBO_TECLA_MANTENIDA: return motor_entrada_mantenida(e, valor);
    case LOBO_TECLA_SOLTADA:   return motor_entrada_soltada(e, valor);
    case LOBO_RATON_BOTON:     return motor_entrada_raton_boton(e, valor);
    case LOBO_RATON_X:         return e ? e->raton_x : 0;
    case LOBO_RATON_Y:         return e ? e->raton_y : 0;
    case LOBO_RATON_RUEDA:     return e ? e->raton_rueda : 0;
    case LOBO_RATON_DENTRO:    return e ? e->raton_dentro : 0;
    default: return 0;
    }
}

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

typedef enum { TOK_NUM = 0, TOK_ID, TOK_OP, TOK_TXT, TOK_NADA } LoboTokTipo;

typedef struct {
    int tipo;
    char texto[LOBO_TEXTO_MAX]; /* identificador, operador o texto */
    int numero;                 /* valor si tipo == TOK_NUM */
} LoboToken;

/* Codigos de error del tokenizador. */
#define TOK_ERR_NOMBRE -1  /* identificador demasiado largo */
#define TOK_ERR_CARACTER -2 /* caracter inesperado */
#define TOK_ERR_COMILLAS -3 /* falta cerrar las comillas */
#define TOK_ERR_TEXTO -4    /* texto demasiado largo */

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
 * caracteres se junta en uno (==, !=, <=, >=). El # corta la linea.
 * Los textos entre comillas y los nombres con $ o @ solo existen en el
 * modo VM: el modo clasico los rechaza con el mismo error de siempre. */
static int lobo_tokenizar(const char *linea, int largo, LoboToken *tokens,
                          int maximo, int vm) {
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
            if (j >= LOBO_NOMBRE_MAX) return TOK_ERR_NOMBRE;
            tokens[n].texto[j] = 0;
            tokens[n].tipo = TOK_ID;
            tokens[n].numero = 0;
            n++;
            continue;
        }
        /* Variables globales ($) y de entidad (@): el sigilo forma
         * parte del nombre, asi el resto del parser no cambia. */
        if (vm && (c == '$' || c == '@') && i + 1 < largo &&
            lobo_letra(linea[i + 1])) {
            int j = 0;
            tokens[n].texto[j++] = c;
            i++;
            while (i < largo && lobo_alfanum(linea[i])) {
                if (j < LOBO_NOMBRE_MAX - 1) tokens[n].texto[j] = linea[i];
                j++;
                i++;
            }
            if (j >= LOBO_NOMBRE_MAX) return TOK_ERR_NOMBRE;
            tokens[n].texto[j] = 0;
            tokens[n].tipo = TOK_ID;
            tokens[n].numero = 0;
            n++;
            continue;
        }
        /* Textos entre comillas con escapes \" \\ y \n (modo VM). */
        if (vm && c == '"') {
            int j = 0;
            i++;
            while (i < largo && linea[i] != '"') {
                char ch = linea[i];
                if (ch == '\\' && i + 1 < largo) {
                    char e = linea[i + 1];
                    if (e == '"') ch = '"';
                    else if (e == '\\') ch = '\\';
                    else if (e == 'n') ch = '\n';
                    else return TOK_ERR_CARACTER;
                    i += 2;
                } else {
                    i++;
                }
                /* Los textos se cortan a 64 bytes (63 + el cero final)
                 * en vez de fallar: es el toque de la fase 28. */
                if (j < LOBO_TEXTO_MAX - 1) tokens[n].texto[j++] = ch;
            }
            if (i >= largo) return TOK_ERR_COMILLAS;
            i++; /* comilla de cierre */
            tokens[n].texto[j] = 0;
            tokens[n].tipo = TOK_TXT;
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
            c == '|' || c == '(' || c == ')' || c == ',' ||
            (vm && c == '.')) {
            tokens[n].tipo = TOK_OP;
            tokens[n].texto[0] = c;
            tokens[n].texto[1] = 0;
            tokens[n].numero = 0;
            n++;
            i++;
            continue;
        }
        return TOK_ERR_CARACTER; /* caracter inesperado */
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Parser                                                             */
/* ------------------------------------------------------------------ */

/* Tipos de bloque abierto (si/sino, mientras, funcion y bucles de la
 * fase 28). */
#define BLOQUE_SI         0 /* si ... (sin sino todavia) */
#define BLOQUE_SINO       1 /* si ... sino ... */
#define BLOQUE_MIENTRAS   2
#define BLOQUE_FUNCION    3
#define BLOQUE_REPETIR    4
#define BLOQUE_PARA_CADA  5

#define LOBO_ROMPER_MAX    8 /* romper/continuar por bucle */
#define LOBO_CONTINUAR_MAX 8

typedef struct {
    int tipo;
    int linea;           /* linea donde se abrio, para los errores */
    int salto_falso;     /* instruccion SALTO_FALSO a parchear al fin */
    int salto_sino;      /* SALTO del sino a parchear al fin (-1 si no) */
    int mientras_inicio; /* instruccion donde empieza la condicion */
    int salto_funcion;   /* SALTO que saltaba el cuerpo al definir (-1 si no) */
    int continuar_inicio;/* donde sigue un continuar del bucle */
    int contador;        /* local oculto del repetir */
    int indice;          /* local oculto del para cada */
    int lista;           /* lista recorrida por el para cada */
    int variable;        /* local que recibe el elemento del para cada */
    int encadenado;      /* 1 si este si viene de un "sino si" */
    int salto_romper[LOBO_ROMPER_MAX];
    int n_romper;
    int salto_continuar[LOBO_CONTINUAR_MAX];
    int n_continuar;
} LoboBloque;

typedef struct {
    LoboPrograma *prog;
    char *error;
    int tam_error;
    int linea;                 /* linea actual (1-based) */
    int modo;                  /* LOBO_MODO_* */
    int limite_insts;          /* topes del modo */
    int limite_locales;
    LoboToken tokens[LOBO_TOKENS_LINEA];
    int num_tokens;
    int pos;                   /* token actual de la linea */
    LoboBloque bloques[16];
    int num_bloques;
    int funcion_actual;        /* indice de la funcion abierta (-1 si no) */
    char locales[LOBO_LOCALES_MAX][LOBO_NOMBRE_MAX];
    int num_locales;
    char globales[LOBO_GLOBALES_MAX][LOBO_NOMBRE_MAX];
    int num_globales;
    char listas[LOBO_LISTAS_MAX][LOBO_NOMBRE_MAX];
    int num_listas;
    /* Tipos declarados (fase 28) y contador de locales ocultos. */
    char tipos_locales[LOBO_LOCALES_MAX];
    char tipos_globales[LOBO_GLOBALES_MAX];
    char tipos_entidad[LOBO_ENTIDAD_VARS_MAX];
    char tipos_listas[LOBO_LISTAS_MAX];
    int num_temporales;
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

/* Traduce el codigo del tokenizador al error del parser. */
static void lobo_error_token(LoboParser *p, int codigo) {
    if (codigo == TOK_ERR_NOMBRE) lobo_error(p, "nombre demasiado largo");
    else if (codigo == TOK_ERR_COMILLAS) lobo_error(p, "falta cerrar las comillas");
    else if (codigo == TOK_ERR_TEXTO) lobo_error(p, "texto demasiado largo");
    else lobo_error(p, "caracter inesperado");
}

/* Mensaje de limite de locales segun el modo (el clasico conserva el
 * texto exacto de la fase 8). */
static void lobo_error_locales(LoboParser *p) {
    if (p->limite_locales == LOBO_LOCALES_CLASICO)
        lobo_error(p, "demasiadas variables locales (maximo 16)");
    else
        lobo_error(p, "demasiadas variables locales (maximo 64)");
}

/* Declaraciones adelantadas: los ayudantes de tipos los usan antes de
 * que llegue su definicion en el bloque de simbolos. */
static int lobo_nueva_local(LoboParser *p, const char *nombre);
static int lobo_buscar_local(LoboParser *p, const char *nombre);
static int lobo_buscar_global(LoboParser *p, const char *nombre);
static int lobo_buscar_entidad(LoboParser *p, const char *nombre);

/* Codigo de tipo de una palabra de la fase 28; -1 si no es un tipo. */
static int lobo_tipo_codigo(const char *nombre) {
    if (lobo_igual(nombre, "entero")) return LOBO_TIPO_ENTERO;
    if (lobo_igual(nombre, "booleano")) return LOBO_TIPO_BOOLEANO;
    if (lobo_igual(nombre, "texto")) return LOBO_TIPO_TEXTO;
    if (lobo_igual(nombre, "lista")) return LOBO_TIPO_LISTA;
    return -1;
}

/* Deja un bloque nuevo a cero. Todos los campos de salto empiezan en -1
 * salvo los que cada sentencia rellena a continuacion. */
static void lobo_bloque_iniciar(LoboBloque *b, int tipo, int linea) {
    int i;
    b->tipo = tipo;
    b->linea = linea;
    b->salto_falso = -1;
    b->salto_sino = -1;
    b->mientras_inicio = -1;
    b->salto_funcion = -1;
    b->continuar_inicio = -1;
    b->contador = -1;
    b->indice = -1;
    b->lista = -1;
    b->variable = -1;
    b->encadenado = 0;
    b->n_romper = 0;
    b->n_continuar = 0;
    for (i = 0; i < LOBO_ROMPER_MAX; i++) b->salto_romper[i] = -1;
    for (i = 0; i < LOBO_CONTINUAR_MAX; i++) b->salto_continuar[i] = -1;
}

/* Local oculto para los contadores de repetir y para cada. El nombre
 * `__tN` no puede chocar con una variable escrita a mano porque se
 * comprueba contra la tabla de locales antes de usarlo. */
static int lobo_local_temporal(LoboParser *p) {
    char nombre[LOBO_NOMBRE_MAX];
    char num[12];
    int i, n = 0, v = p->num_temporales++;
    do { num[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    for (i = 0; i < 2; i++) nombre[i] = '_';
    for (i = 0; n > 0 && i < 8; i++, n--) nombre[2 + i] = num[n - 1];
    nombre[2 + i] = 0;
    return lobo_nueva_local(p, nombre);
}

/* Marca el tipo de una variable ya registrada. */
static void lobo_marcar_tipo(LoboParser *p, const char *nombre, int tipo) {
    char codigo = (char)tipo;
    int i;
    if (nombre[0] == '$') {
        i = lobo_buscar_global(p, nombre);
        if (i >= 0) p->tipos_globales[i] = codigo;
    } else if (nombre[0] == '@') {
        i = lobo_buscar_entidad(p, nombre);
        if (i >= 0) p->tipos_entidad[i] = codigo;
    } else {
        i = lobo_buscar_local(p, nombre);
        if (i >= 0) p->tipos_locales[i] = codigo;
    }
}

/* Reserva capacidad en el programa para `necesita` instrucciones sin
 * pasarse nunca de LOBO_INSTS_MAX. Devuelve 0 sin memoria. */
static int lobo_programa_reservar(LoboPrograma *prog, int necesita) {
    int nueva;
    LoboInst *nuevo;
    if (prog->capacidad >= necesita) return 1;
    nueva = prog->capacidad > 0 ? prog->capacidad : 64;
    while (nueva < necesita) nueva *= 2;
    if (nueva > LOBO_INSTS_MAX) nueva = LOBO_INSTS_MAX;
    if (nueva < necesita) return 0;
    nuevo = (LoboInst *)realloc(prog->insts, (size_t)nueva * sizeof(LoboInst));
    if (!nuevo) return 0;
    prog->insts = nuevo;
    prog->capacidad = nueva;
    return 1;
}

/* Anade una instruccion al programa con uno o dos operandos; 0 si ya no
 * cabe. */
static int lobo_emitir2(LoboParser *p, int op, int valor, int extra) {
    if (p->prog->largo >= p->limite_insts) {
        lobo_error(p, "el programa es demasiado largo");
        return 0;
    }
    if (!lobo_programa_reservar(p->prog, p->prog->largo + 1)) {
        lobo_error(p, "sin memoria para el programa");
        return 0;
    }
    p->prog->insts[p->prog->largo].op = (unsigned char)op;
    p->prog->insts[p->prog->largo].valor = valor;
    p->prog->insts[p->prog->largo].extra = extra;
    p->prog->largo++;
    return 1;
}

static int lobo_emitir(LoboParser *p, int op, int valor) {
    return lobo_emitir2(p, op, valor, 0);
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
    if (lobo_igual(nombre, "vel_x")) return LOBO_VAR_VEL_X;
    if (lobo_igual(nombre, "vel_y")) return LOBO_VAR_VEL_Y;
    if (lobo_igual(nombre, "gravedad")) return LOBO_VAR_GRAVEDAD;
    if (lobo_igual(nombre, "rebote")) return LOBO_VAR_REBOTE;
    if (lobo_igual(nombre, "friccion")) return LOBO_VAR_FRICCION;
    if (lobo_igual(nombre, "en_suelo")) return LOBO_VAR_EN_SUELO;
    *es_tecla = 1;
    if (lobo_igual(nombre, "tecla_izquierda")) return LOBO_TECLA_IZQUIERDA;
    if (lobo_igual(nombre, "tecla_derecha")) return LOBO_TECLA_DERECHA;
    if (lobo_igual(nombre, "tecla_arriba")) return LOBO_TECLA_ARRIBA;
    if (lobo_igual(nombre, "tecla_abajo")) return LOBO_TECLA_ABAJO;
    if (lobo_igual(nombre, "tecla_salto")) return LOBO_TECLA_SALTO;
    if (lobo_igual(nombre, "tecla_cerrar")) return LOBO_TECLA_CERRAR;
    *es_tecla = 0;
    *es_funcion = 1;
    if (lobo_igual(nombre, "choca")) return LOBO_CHOCAR;
    if (lobo_igual(nombre, "cerrar")) return LOBO_CERRAR;
    /* Fase 37: entradas extendidas. El valor devuelto es el opcode que
     * debe emitir el parser; las teclas con flanco resuelven su ranura
     * en compilacion a partir del texto entre comillas. */
    if (lobo_igual(nombre, "tecla_pulsada")) return LOBO_TECLA_PULSADA;
    if (lobo_igual(nombre, "tecla_mantenida")) return LOBO_TECLA_MANTENIDA;
    if (lobo_igual(nombre, "tecla_soltada")) return LOBO_TECLA_SOLTADA;
    if (lobo_igual(nombre, "raton_boton")) return LOBO_RATON_BOTON;
    if (lobo_igual(nombre, "raton_x")) return LOBO_RATON_X;
    if (lobo_igual(nombre, "raton_y")) return LOBO_RATON_Y;
    if (lobo_igual(nombre, "raton_rueda")) return LOBO_RATON_RUEDA;
    if (lobo_igual(nombre, "raton_dentro")) return LOBO_RATON_DENTRO;
    /* Fase 38: HUD. El valor devuelto es el opcode que debe emitir el
     * parser; el numero de argumentos se deduce de el. */
    if (lobo_igual(nombre, "hud_vidas")) return LOBO_HUD_VIDAS;
    if (lobo_igual(nombre, "hud_puntos")) return LOBO_HUD_PUNTOS;
    if (lobo_igual(nombre, "hud_dialogo")) return LOBO_HUD_DIALOGO;
    if (lobo_igual(nombre, "hud_texto")) return LOBO_HUD_TEXTO;
    if (lobo_igual(nombre, "hud_rect")) return LOBO_HUD_RECT;
    if (lobo_igual(nombre, "hud_barra")) return LOBO_HUD_BARRA;
    if (lobo_igual(nombre, "hud_imagen")) return LOBO_HUD_IMAGEN;
    if (lobo_igual(nombre, "hud_boton")) return LOBO_HUD_BOTON;
    if (lobo_igual(nombre, "hud_plantilla")) return LOBO_HUD_PLANTILLA;
    /* Fase 39: sonido, musica y volumen. Igual que el HUD, el valor es el
     * opcode y el numero de argumentos se deduce de el. */
    if (lobo_igual(nombre, "sonar")) return LOBO_SONAR;
    if (lobo_igual(nombre, "musica")) return LOBO_MUSICA;
    if (lobo_igual(nombre, "detener_sonido")) return LOBO_DETENER_SONIDO;
    if (lobo_igual(nombre, "volumen")) return LOBO_VOLUMEN;
    if (lobo_igual(nombre, "volumen_actual")) return LOBO_VOLUMEN_ACTUAL;
    /* Fase 40: guardado, records y ajustes. Como el audio, el valor
     * devuelto es el opcode y la aridad se deduce de el. */
    if (lobo_igual(nombre, "guardar")) return LOBO_GUARDAR;
    if (lobo_igual(nombre, "cargar")) return LOBO_CARGAR;
    if (lobo_igual(nombre, "borrar_save")) return LOBO_BORRAR_SAVE;
    if (lobo_igual(nombre, "record")) return LOBO_RECORD;
    if (lobo_igual(nombre, "record_actual")) return LOBO_RECORD_ACTUAL;
    if (lobo_igual(nombre, "ajuste")) return LOBO_AJUSTE;
    if (lobo_igual(nombre, "ajuste_actual")) return LOBO_AJUSTE_ACTUAL;
    /* Fase 41: escenas, transiciones, camara y efectos. Como en la fase 40,
     * el valor devuelto es el opcode y la aridad se deduce de el. */
    if (lobo_igual(nombre, "cambiar_escena")) return LOBO_CAMBIAR_ESCENA;
    if (lobo_igual(nombre, "escena_actual")) return LOBO_ESCENA_ACTUAL;
    if (lobo_igual(nombre, "transicion")) return LOBO_TRANSICION;
    if (lobo_igual(nombre, "reiniciar_escena")) return LOBO_REINICIAR_ESCENA;
    if (lobo_igual(nombre, "camara_objetivo")) return LOBO_CAMARA_OBJETIVO;
    if (lobo_igual(nombre, "camara_zoom")) return LOBO_CAMARA_ZOOM;
    if (lobo_igual(nombre, "camara_limites")) return LOBO_CAMARA_LIMITES;
    if (lobo_igual(nombre, "camara_suavizar")) return LOBO_CAMARA_SUAVIZAR;
    if (lobo_igual(nombre, "parpadeo")) return LOBO_PARPADEO;
    if (lobo_igual(nombre, "temblor")) return LOBO_TEMBLOR;
    if (lobo_igual(nombre, "flash")) return LOBO_FLASH;
    if (lobo_igual(nombre, "fade")) return LOBO_FADE;
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
    if (p->num_locales >= p->limite_locales) {
        lobo_error_locales(p);
        return -1;
    }
    lobo_copiar(p->locales[p->num_locales], nombre, LOBO_NOMBRE_MAX);
    return p->num_locales++;
}

/* Indice de la global con ese nombre completo (con el $). */
static int lobo_buscar_global(LoboParser *p, const char *nombre) {
    int i;
    for (i = 0; i < p->num_globales; i++)
        if (lobo_igual(p->globales[i], nombre)) return i;
    return -1;
}

static int lobo_nueva_global(LoboParser *p, const char *nombre) {
    int i = lobo_buscar_global(p, nombre);
    if (i >= 0) return i;
    if (p->num_globales >= LOBO_GLOBALES_MAX) {
        lobo_error(p, "demasiadas variables globales (maximo 128)");
        return -1;
    }
    lobo_copiar(p->globales[p->num_globales], nombre, LOBO_NOMBRE_MAX);
    return p->num_globales++;
}

/* Indice de la variable de entidad con ese nombre. @nombre y
 * mi.variable("nombre") comparten la tabla del programa; el @ del
 * tokenizador no forma parte del nombre guardado. */
static int lobo_buscar_entidad(LoboParser *p, const char *nombre) {
    int i;
    if (nombre[0] == '@') nombre++;
    for (i = 0; i < p->prog->num_ent_nombres; i++)
        if (lobo_igual(p->prog->entidad_nombres[i], nombre)) return i;
    return -1;
}

static int lobo_nueva_entidad(LoboParser *p, const char *nombre) {
    int i = lobo_buscar_entidad(p, nombre);
    if (i >= 0) return i;
    if (p->prog->num_ent_nombres >= LOBO_ENTIDAD_NOMBRES_MAX) {
        lobo_error(p, "demasiadas variables de entidad (maximo 64)");
        return -1;
    }
    if (nombre[0] == '@') nombre++;
    lobo_copiar(p->prog->entidad_nombres[p->prog->num_ent_nombres],
                nombre, LOBO_TEXTO_MAX);
    return p->prog->num_ent_nombres++;
}

/* Indice de la lista con ese nombre. */
static int lobo_buscar_lista(LoboParser *p, const char *nombre) {
    int i;
    for (i = 0; i < p->num_listas; i++)
        if (lobo_igual(p->listas[i], nombre)) return i;
    return -1;
}

static int lobo_nueva_lista(LoboParser *p, const char *nombre) {
    int i = lobo_buscar_lista(p, nombre);
    if (i >= 0) return i;
    if (p->num_listas >= LOBO_LISTAS_MAX) {
        lobo_error(p, "demasiadas listas (maximo 32)");
        return -1;
    }
    lobo_copiar(p->listas[p->num_listas], nombre, LOBO_NOMBRE_MAX);
    return p->num_listas++;
}

/* Indice de la funcion de usuario con ese nombre. */
static int lobo_buscar_funcion(LoboParser *p, const char *nombre) {
    int i;
    for (i = 0; i < p->prog->num_funciones; i++)
        if (lobo_igual(p->prog->funciones[i].nombre, nombre)) return i;
    return -1;
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

/* Guarda un texto en el programa y devuelve su indice (-1 si no cabe). */
static int lobo_agregar_texto(LoboParser *p, const char *texto) {
    int i;
    for (i = 0; i < p->prog->num_textos; i++)
        if (lobo_igual(p->prog->textos[i], texto)) return i;
    if (p->prog->num_textos >= LOBO_TEXTOS_MAX) {
        lobo_error(p, "demasiados textos (maximo 64)");
        return -1;
    }
    lobo_copiar(p->prog->textos[p->prog->num_textos], texto, LOBO_TEXTO_MAX);
    return p->prog->num_textos++;
}

/* Nombres de las funciones de lista que se llaman como una funcion. */
static int lobo_es_lista_funcion(const char *nombre) {
    return lobo_igual(nombre, "largo") || lobo_igual(nombre, "elemento") ||
           lobo_igual(nombre, "contiene") || lobo_igual(nombre, "tiene") ||
           lobo_igual(nombre, "fijar") || lobo_igual(nombre, "anadir") ||
           lobo_igual(nombre, "agregar") || lobo_igual(nombre, "insertar") ||
           lobo_igual(nombre, "borrar") || lobo_igual(nombre, "limpiar") ||
           lobo_igual(nombre, "vaciar") || lobo_igual(nombre, "ordenar");
}

/* Compila una llamada a las funciones de lista. El nombre ya se comio y
 * el parentesis de apertura tambien. */
static int lobo_lista_builtin(LoboParser *p, const char *nombre) {
    static const struct {
        const char *nombre;
        int op;
        int args; /* argumentos despues de la lista */
    } tabla[] = {
        { "largo", LOBO_LISTA_LARGO, 0 },
        { "elemento", LOBO_LISTA_ELEMENTO, 1 },
        { "contiene", LOBO_LISTA_TIENE, 1 },
        { "tiene", LOBO_LISTA_TIENE, 1 },
        { "fijar", LOBO_LISTA_FIJAR, 2 },
        { "anadir", LOBO_LISTA_ANADIR, 1 },
        { "agregar", LOBO_LISTA_ANADIR, 1 },
        { "insertar", LOBO_LISTA_INSERTAR, 2 },
        { "borrar", LOBO_LISTA_BORRAR, 1 },
        { "limpiar", LOBO_LISTA_LIMPIAR, 0 },
        { "vaciar", LOBO_LISTA_LIMPIAR, 0 },
        { "ordenar", LOBO_LISTA_ORDENAR, 0 }
    };
    int i, lista = -1, args = 0, op = 0, esperados = 0;
    for (i = 0; i < (int)(sizeof(tabla) / sizeof(tabla[0])); i++) {
        if (lobo_igual(nombre, tabla[i].nombre)) {
            op = tabla[i].op;
            esperados = tabla[i].args;
            break;
        }
    }
    if (lobo_fin_tokens(p) || p->tokens[p->pos].tipo != TOK_ID) {
        lobo_error(p, "se esperaba el nombre de una lista");
        return 0;
    }
    lista = lobo_buscar_lista(p, p->tokens[p->pos].texto);
    if (lista < 0) {
        lobo_error(p, "lista desconocida");
        return 0;
    }
    p->pos++;
    for (i = 0; i < esperados; i++) {
        if (!lobo_comer_op(p, ",")) {
            lobo_error(p, "faltan argumentos de la lista");
            return 0;
        }
        if (!lobo_expresion(p)) return 0;
        args++;
    }
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "sobran argumentos o falta ) en la lista");
        return 0;
    }
    (void)args;
    return lobo_emitir(p, op, lista);
}

/* Compila los argumentos de una llamada. El parentesis de apertura ya
 * esta comido; lo comparten llamar("nombre", ...) y nombre(...). */
static int lobo_llamada_usuario_args(LoboParser *p, int fi) {
    int dados = 0;
    LoboFuncion *f = &p->prog->funciones[fi];
    /* Con llamar("f", x) el cursor esta en la coma que sigue al nombre;
     * con f(x) esta en el primer argumento. Se come esa coma si existe y
     * luego se leen todos los argumentos hasta cerrar el parentesis. */
    if (!lobo_fin_tokens(p) && p->tokens[p->pos].tipo == TOK_OP &&
        lobo_igual(p->tokens[p->pos].texto, ","))
        p->pos++;
    while (!lobo_fin_tokens(p) &&
           !(p->tokens[p->pos].tipo == TOK_OP &&
             lobo_igual(p->tokens[p->pos].texto, ")"))) {
        if (dados >= LOBO_PARAMS_MAX) {
            lobo_error(p, "demasiados argumentos en la llamada");
            return 0;
        }
        if (!lobo_expresion(p)) return 0;
        dados++;
        if (!lobo_comer_op(p, ",")) break;
    }
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar la llamada");
        return 0;
    }
    if (dados != f->params) {
        lobo_error(p, "numero de argumentos distinto de los parametros");
        return 0;
    }
    return lobo_emitir(p, LOBO_LLAMAR, fi);
}

/* llamar("nombre", ...). */
static int lobo_llamada_usuario(LoboParser *p) {
    int fi;
    char nombre[LOBO_NOMBRE_MAX];
    if (lobo_fin_tokens(p) || p->tokens[p->pos].tipo != TOK_TXT) {
        lobo_error(p, "llamar necesita el nombre de la funcion entre comillas");
        return 0;
    }
    lobo_copiar(nombre, p->tokens[p->pos].texto, LOBO_NOMBRE_MAX);
    p->pos++;
    fi = lobo_buscar_funcion(p, nombre);
    if (fi < 0) {
        lobo_error(p, "funcion desconocida");
        return 0;
    }
    return lobo_llamada_usuario_args(p, fi);
}

/* Campo de `mi` por nombre; -1 si no es un campo conocido. */
static int lobo_campo_mi(const char *nombre) {
    if (lobo_igual(nombre, "x")) return LOBO_MI_X;
    if (lobo_igual(nombre, "y")) return LOBO_MI_Y;
    if (lobo_igual(nombre, "vida")) return LOBO_MI_VIDA;
    if (lobo_igual(nombre, "visible")) return LOBO_MI_VISIBLE;
    if (lobo_igual(nombre, "traje")) return LOBO_MI_TRAJE;
    if (lobo_igual(nombre, "id")) return LOBO_MI_ID;
    if (lobo_igual(nombre, "escena")) return LOBO_MI_ESCENA;
    if (lobo_igual(nombre, "ancho")) return LOBO_MI_ANCHO;
    if (lobo_igual(nombre, "alto")) return LOBO_MI_ALTO;
    if (lobo_igual(nombre, "capa")) return LOBO_MI_CAPA;
    return -1;
}

/* Primario detras de "mi." (el punto ya comido). */
static int lobo_primario_mi(LoboParser *p) {
    char nombre[LOBO_NOMBRE_MAX];
    int campo;

    if (lobo_fin_tokens(p) || p->tokens[p->pos].tipo != TOK_ID) {
        lobo_error(p, "se esperaba un campo de mi");
        return 0;
    }
    lobo_copiar(nombre, p->tokens[p->pos].texto, LOBO_NOMBRE_MAX);
    p->pos++;
    if (lobo_igual(nombre, "variable")) {
        int slot;
        if (!lobo_comer_op(p, "(")) {
            lobo_error(p, "mi.variable necesita parentesis");
            return 0;
        }
        if (lobo_fin_tokens(p) || p->tokens[p->pos].tipo != TOK_TXT) {
            lobo_error(p, "mi.variable necesita el nombre entre comillas");
            return 0;
        }
        slot = lobo_buscar_entidad(p, p->tokens[p->pos].texto);
        if (slot < 0) {
            lobo_error(p, "variable de entidad desconocida");
            return 0;
        }
        p->pos++;
        if (!lobo_comer_op(p, ")")) {
            lobo_error(p, "falta ) tras mi.variable");
            return 0;
        }
        return lobo_emitir(p, LOBO_CARGAR_MI, LOBO_MI_VARS + slot);
    }
    campo = lobo_campo_mi(nombre);
    if (campo < 0) {
        lobo_error(p, "campo de mi desconocido");
        return 0;
    }
    return lobo_emitir(p, LOBO_CARGAR_MI, campo);
}

/* crear("tag", x, y): deja el id de la entidad nueva (o -1) en la pila. */
static int lobo_llamada_crear(LoboParser *p) {
    int idx;
    if (lobo_fin_tokens(p) || p->tokens[p->pos].tipo != TOK_TXT) {
        lobo_error(p, "crear necesita el nombre del objeto entre comillas");
        return 0;
    }
    idx = lobo_agregar_texto(p, p->tokens[p->pos].texto);
    if (idx < 0) return 0;
    p->pos++;
    if (!lobo_comer_op(p, ",")) {
        lobo_error(p, "crear necesita x e y");
        return 0;
    }
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ",")) {
        lobo_error(p, "crear necesita x e y");
        return 0;
    }
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar crear");
        return 0;
    }
    return lobo_emitir(p, LOBO_CREAR, idx);
}

/* mover(id, x, y). */
static int lobo_llamada_mover(LoboParser *p) {
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ",")) {
        lobo_error(p, "mover necesita id, x e y");
        return 0;
    }
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ",")) {
        lobo_error(p, "mover necesita id, x e y");
        return 0;
    }
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar mover");
        return 0;
    }
    return lobo_emitir(p, LOBO_MOVER_ENTIDAD, 0);
}

/* borrar(): la entidad actual; borrar(id): esa entidad; y
 * borrar(lista, i) sigue siendo la funcion de lista de la fase 25. */
static int lobo_llamada_borrar(LoboParser *p) {
    if (lobo_comer_op(p, ")"))
        return lobo_emitir(p, LOBO_BORRAR_ENTIDAD, 0);
    if (!lobo_fin_tokens(p) && p->tokens[p->pos].tipo == TOK_ID &&
        lobo_buscar_lista(p, p->tokens[p->pos].texto) >= 0)
        return lobo_lista_builtin(p, "borrar");
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar borrar");
        return 0;
    }
    return lobo_emitir(p, LOBO_BORRAR_ENTIDAD, 1);
}

/* enviar("mensaje"): a todos los objetos (fase 27). */
static int lobo_llamada_enviar(LoboParser *p) {
    int idx;
    if (lobo_fin_tokens(p) || p->tokens[p->pos].tipo != TOK_TXT) {
        lobo_error(p, "enviar necesita un texto entre comillas");
        return 0;
    }
    idx = lobo_agregar_texto(p, p->tokens[p->pos].texto);
    if (idx < 0) return 0;
    p->pos++;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar enviar");
        return 0;
    }
    return lobo_emitir(p, LOBO_ENVIAR, idx);
}

/* enviar_a("objeto", "mensaje"): al objeto con ese tag. */
static int lobo_llamada_enviar_a(LoboParser *p) {
    int destino, texto;
    if (lobo_fin_tokens(p) || p->tokens[p->pos].tipo != TOK_TXT) {
        lobo_error(p, "enviar_a necesita el objeto entre comillas");
        return 0;
    }
    destino = lobo_agregar_texto(p, p->tokens[p->pos].texto);
    if (destino < 0) return 0;
    p->pos++;
    if (!lobo_comer_op(p, ",")) {
        lobo_error(p, "enviar_a necesita objeto y mensaje");
        return 0;
    }
    if (lobo_fin_tokens(p) || p->tokens[p->pos].tipo != TOK_TXT) {
        lobo_error(p, "enviar_a necesita el mensaje entre comillas");
        return 0;
    }
    texto = lobo_agregar_texto(p, p->tokens[p->pos].texto);
    if (texto < 0) return 0;
    p->pos++;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar enviar_a");
        return 0;
    }
    return lobo_emitir2(p, LOBO_ENVIAR_A, texto, destino);
}

/* Built-ins numericos y de texto de la fase 28. Todos toman expresiones
 * (no solo literales) para poder trabajar con variables y parametros. */
static int lobo_llamada_abs(LoboParser *p) {
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar abs");
        return 0;
    }
    return lobo_emitir(p, LOBO_ABS, 0);
}

/* min(a, b) / max(a, b). */
static int lobo_llamada_min_max(LoboParser *p, int op) {
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ",")) {
        lobo_error(p, "se esperaba , entre los argumentos");
        return 0;
    }
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar la funcion");
        return 0;
    }
    return lobo_emitir(p, op, 0);
}

/* limitar(v, lo, hi). */
static int lobo_llamada_limitar(LoboParser *p) {
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ",")) {
        lobo_error(p, "limitar necesita v, lo e hi");
        return 0;
    }
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ",")) {
        lobo_error(p, "limitar necesita v, lo e hi");
        return 0;
    }
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar limitar");
        return 0;
    }
    return lobo_emitir(p, LOBO_LIMITAR, 0);
}

/* redondear(v). Con enteros el valor ya esta redondeado: la operacion
 * existe para que el codigo escrito para decimales siga siendo valido. */
static int lobo_llamada_redondear(LoboParser *p) {
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar redondear");
        return 0;
    }
    return lobo_emitir(p, LOBO_REDONDEAR, 0);
}

/* Fase 38: numero de argumentos de cada built-in del HUD. */
static int lobo_hud_argumentos(int op) {
    switch (op) {
    case LOBO_HUD_VIDAS:
    case LOBO_HUD_PUNTOS:
    case LOBO_HUD_PLANTILLA: return 1;
    case LOBO_HUD_DIALOGO: return 2;
    case LOBO_HUD_IMAGEN: return 3;
    case LOBO_HUD_TEXTO:
    case LOBO_HUD_RECT: return 5;
    case LOBO_HUD_BOTON: return 6;
    case LOBO_HUD_BARRA: return 7;
    default: return 0;
    }
}

/* Lee n expresiones separadas por comas y cierra el parentesis. Los valores
 * quedan apilados de izquierda a derecha; la instruccion los consume al
 * reverso, que es el orden que esperan los callbacks del HUD. */
static int lobo_llamada_hud(LoboParser *p, int op, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (i && !lobo_comer_op(p, ",")) {
            lobo_error(p, "se esperaba , entre argumentos del HUD");
            return 0;
        }
        if (!lobo_expresion(p)) return 0;
    }
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar la llamada del HUD");
        return 0;
    }
    return lobo_emitir(p, op, 0);
}

/* Fase 39: numero de argumentos de cada built-in de audio. */
static int lobo_audio_argumentos(int op) {
    switch (op) {
    case LOBO_SONAR:
    case LOBO_MUSICA:
    case LOBO_VOLUMEN: return 1;
    default: return 0;
    }
}

/* Igual que lobo_llamada_hud: lee los argumentos de izquierda a derecha y
 * deja la instruccion lista para consumirlos en orden inverso. */
static int lobo_llamada_audio(LoboParser *p, int op, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (i && !lobo_comer_op(p, ",")) {
            lobo_error(p, "se esperaba , entre argumentos de audio");
            return 0;
        }
        if (!lobo_expresion(p)) return 0;
    }
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar la llamada de audio");
        return 0;
    }
    return lobo_emitir(p, op, 0);
}

/* Fase 40: numero de argumentos de cada builtin de persistencia. */
static int lobo_save_argumentos(int op) {
    switch (op) {
    case LOBO_GUARDAR:
    case LOBO_RECORD:
    case LOBO_AJUSTE: return 2;
    case LOBO_CARGAR:
    case LOBO_RECORD_ACTUAL:
    case LOBO_AJUSTE_ACTUAL: return 1;
    default: return 0;
    }
}

/* Igual que lobo_llamada_audio: lee los argumentos de izquierda a derecha
 * y deja la instruccion lista para consumirlos en orden inverso. */
static int lobo_llamada_save(LoboParser *p, int op, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (i && !lobo_comer_op(p, ",")) {
            lobo_error(p, "se esperaba , entre argumentos del guardado");
            return 0;
        }
        if (!lobo_expresion(p)) return 0;
    }
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar la llamada de guardado");
        return 0;
    }
    return lobo_emitir(p, op, 0);
}

/* Fase 41: numero de argumentos de los builtins de escenas y efectos. */
static int lobo_escena_argumentos(int op) {
    switch (op) {
    case LOBO_CAMBIAR_ESCENA:
    case LOBO_CAMARA_OBJETIVO:
    case LOBO_CAMARA_ZOOM:
    case LOBO_CAMARA_SUAVIZAR: return 1;
    case LOBO_TRANSICION:
    case LOBO_PARPADEO:
    case LOBO_TEMBLOR:
    case LOBO_FLASH: return 2;
    case LOBO_FADE: return 3;
    case LOBO_CAMARA_LIMITES: return 4;
    case LOBO_ESCENA_ACTUAL:
    case LOBO_REINICIAR_ESCENA: return 0;
    default: return 0;
    }
}

/* Igual que lobo_llamada_save: lee los argumentos de izquierda a derecha
 * y deja la instruccion lista para consumirlos en orden inverso. */
static int lobo_llamada_escena(LoboParser *p, int op, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (i && !lobo_comer_op(p, ",")) {
            lobo_error(p, "se esperaba , entre argumentos de escena");
            return 0;
        }
        if (!lobo_expresion(p)) return 0;
    }
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar la llamada de escena");
        return 0;
    }
    return lobo_emitir(p, op, 0);
}

/* aleatorio(a, b): extremos incluidos, semilla por maquina. */
static int lobo_llamada_aleatorio(LoboParser *p) {
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ",")) {
        lobo_error(p, "aleatorio necesita dos argumentos");
        return 0;
    }
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar aleatorio");
        return 0;
    }
    return lobo_emitir(p, LOBO_ALEATORIO, 0);
}

/* longitud(texto): un argumento que puede ser un handle de texto. */
static int lobo_llamada_texto_uno(LoboParser *p, int op) {
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar la funcion de texto");
        return 0;
    }
    return lobo_emitir(p, op, 0);
}

/* letra(texto, i): el caracter i devuelve su codigo (0 si no existe). */
static int lobo_llamada_letra(LoboParser *p) {
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ",")) {
        lobo_error(p, "letra necesita un texto y un indice");
        return 0;
    }
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar letra");
        return 0;
    }
    return lobo_emitir(p, LOBO_LETRA, 0);
}

/* unir(a, b): concatena dos textos y recorta el resultado a 64 bytes. */
static int lobo_llamada_unir(LoboParser *p) {
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ",")) {
        lobo_error(p, "unir necesita dos textos");
        return 0;
    }
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar unir");
        return 0;
    }
    return lobo_emitir(p, LOBO_UNIR, 0);
}

/* Built-ins de colisiones de la fase 29. toca() y distancia() toman un id
 * de entidad; caja_de() toma id e indice (0=x, 1=y, 2=w, 3=h). */
static int lobo_llamada_colision_uno(LoboParser *p, int op,
                                     const char *nombre) {
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar la funcion");
        (void)nombre;
        return 0;
    }
    return lobo_emitir(p, op, 0);
}

static int lobo_llamada_caja_de(LoboParser *p) {
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ",")) {
        lobo_error(p, "caja_de necesita id e indice");
        return 0;
    }
    if (!lobo_expresion(p)) return 0;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar caja_de");
        return 0;
    }
    return lobo_emitir(p, LOBO_CAJA_DE, 0);
}

/* toca_tag("nombre"): el tag se guarda como texto del programa, igual que
 * en crear() o enviar_a(). */
static int lobo_llamada_toca_tag(LoboParser *p) {
    int idx;
    if (lobo_fin_tokens(p) || p->tokens[p->pos].tipo != TOK_TXT) {
        lobo_error(p, "toca_tag necesita el tag entre comillas");
        return 0;
    }
    idx = lobo_agregar_texto(p, p->tokens[p->pos].texto);
    if (idx < 0) return 0;
    p->pos++;
    if (!lobo_comer_op(p, ")")) {
        lobo_error(p, "se esperaba ) para cerrar toca_tag");
        return 0;
    }
    return lobo_emitir2(p, LOBO_TOCA_TAG, idx, 0);
}

/* Primario: numero, variable, tecla, texto, lista o llamada. */
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
    /* Un texto en una expresion es un handle con marca alta (fase 28). */
    if (p->tokens[p->pos].tipo == TOK_TXT) {
        int idx = lobo_agregar_texto(p, p->tokens[p->pos].texto);
        if (idx < 0) return 0;
        p->pos++;
        return lobo_emitir(p, LOBO_TEXTO, idx);
    }
    if (p->tokens[p->pos].tipo == TOK_ID) {
        char nombre[LOBO_NOMBRE_MAX];
        int es_tecla, es_funcion, conocido;
        lobo_copiar(nombre, p->tokens[p->pos].texto, LOBO_NOMBRE_MAX);
        p->pos++;
        if (p->modo == LOBO_MODO_VM) {
            if (nombre[0] == '$') {
                int g = lobo_buscar_global(p, nombre);
                if (g < 0) { lobo_error(p, "global desconocida"); return 0; }
                return lobo_emitir(p, LOBO_CARGAR_GLOBAL, g);
            }
            if (nombre[0] == '@') {
                int g = lobo_buscar_entidad(p, nombre);
                if (g < 0) { lobo_error(p, "variable de entidad desconocida"); return 0; }
                return lobo_emitir(p, LOBO_CARGAR_ENTIDAD, g);
            }
            /* mi.campo y mi.variable("nombre") */
            if (lobo_igual(nombre, "mi") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), ".")) {
                p->pos++;
                return lobo_primario_mi(p);
            }
            /* Built-ins numericos y de texto (fase 28). */
            if (lobo_igual(nombre, "abs") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_abs(p);
            }
            if ((lobo_igual(nombre, "min") || lobo_igual(nombre, "max")) &&
                lobo_mirar_op(p) && lobo_igual(lobo_mirar_op(p), "(")) {
                int op = lobo_igual(nombre, "min") ? LOBO_MIN : LOBO_MAX;
                p->pos++;
                return lobo_llamada_min_max(p, op);
            }
            if (lobo_igual(nombre, "limitar") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_limitar(p);
            }
            if (lobo_igual(nombre, "redondear") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_redondear(p);
            }
            if (lobo_igual(nombre, "aleatorio") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_aleatorio(p);
            }
            if (lobo_igual(nombre, "longitud") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_texto_uno(p, LOBO_LONGITUD);
            }
            if (lobo_igual(nombre, "letra") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_letra(p);
            }
            if (lobo_igual(nombre, "unir") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_unir(p);
            }
            /* Colisiones y fisica (fase 29). */
            if ((lobo_igual(nombre, "toca") || lobo_igual(nombre, "distancia")) &&
                lobo_mirar_op(p) && lobo_igual(lobo_mirar_op(p), "(")) {
                int op = lobo_igual(nombre, "toca") ? LOBO_TOCA : LOBO_DISTANCIA;
                p->pos++;
                return lobo_llamada_colision_uno(p, op, nombre);
            }
            if (lobo_igual(nombre, "toca_tag") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_toca_tag(p);
            }
            if (lobo_igual(nombre, "caja_de") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_caja_de(p);
            }
            /* Llamada directa: danar(5) equivale a llamar("danar", 5). */
            {
                int fi = lobo_buscar_funcion(p, nombre);
                if (fi >= 0 && lobo_mirar_op(p) &&
                    lobo_igual(lobo_mirar_op(p), "(")) {
                    p->pos++;
                    return lobo_llamada_usuario_args(p, fi);
                }
            }
            /* crear(), mover() y borrar() (fase 26) */
            if (lobo_igual(nombre, "crear") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_crear(p);
            }
            if (lobo_igual(nombre, "mover") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_mover(p);
            }
            if (lobo_igual(nombre, "borrar") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_borrar(p);
            }
            /* enviar() y enviar_a() (fase 27) */
            if (lobo_igual(nombre, "enviar") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_enviar(p);
            }
            if (lobo_igual(nombre, "enviar_a") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_enviar_a(p);
            }
            /* llamar("funcion", ...) */
            if (lobo_igual(nombre, "llamar") && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_llamada_usuario(p);
            }
            /* Funciones de lista: largo(l), elemento(l, i), ... */
            if (lobo_es_lista_funcion(nombre) && lobo_mirar_op(p) &&
                lobo_igual(lobo_mirar_op(p), "(")) {
                p->pos++;
                return lobo_lista_builtin(p, nombre);
            }
        }
        conocido = lobo_nombre_conocido(nombre, &es_tecla, &es_funcion);
        if (es_funcion) {
            /* llamada: choca(x, y), cerrar() o una entrada de la fase 37 */
            if (!lobo_comer_op(p, "(")) {
                lobo_error(p, "se esperaba ( despues del nombre");
                return 0;
            }
            if (conocido == LOBO_CERRAR) {
                if (!lobo_comer_op(p, ")")) {
                    lobo_error(p, "cerrar() no lleva argumentos");
                    return 0;
                }
                return lobo_emitir(p, LOBO_CERRAR, 0);
            }
            /* tecla_pulsada("a") y hermanas: nombre entre comillas, resuelto
             * a una ranura de 0 a 31 antes de emitir la instruccion. */
            if (conocido == LOBO_TECLA_PULSADA ||
                conocido == LOBO_TECLA_MANTENIDA ||
                conocido == LOBO_TECLA_SOLTADA) {
                int slot = -1;
                if (p->pos < p->num_tokens &&
                    p->tokens[p->pos].tipo == TOK_TXT) {
                    slot = motor_entrada_slot(p->tokens[p->pos].texto);
                    p->pos++;
                }
                if (slot < 0) {
                    lobo_error(p, "la tecla debe ser un nombre entre comillas");
                    return 0;
                }
                if (!lobo_comer_op(p, ")")) {
                    lobo_error(p, "se esperaba ) para cerrar la llamada");
                    return 0;
                }
                return lobo_emitir(p, conocido, slot);
            }
            /* raton_x(), raton_y(), raton_rueda(), raton_dentro() */
            if (conocido == LOBO_RATON_X || conocido == LOBO_RATON_Y ||
                conocido == LOBO_RATON_RUEDA ||
                conocido == LOBO_RATON_DENTRO) {
                if (!lobo_comer_op(p, ")")) {
                    lobo_error(p, "esta funcion no lleva argumentos");
                    return 0;
                }
                return lobo_emitir(p, conocido, 0);
            }
            /* raton_boton(n): el numero se apila y la instruccion lo consume. */
            if (conocido == LOBO_RATON_BOTON) {
                if (!lobo_expresion(p)) return 0;
                if (!lobo_comer_op(p, ")")) {
                    lobo_error(p, "se esperaba ) para cerrar la llamada");
                    return 0;
                }
                return lobo_emitir(p, LOBO_RATON_BOTON, 0);
            }
            /* Fase 38: vidas, puntos, dialogo y elementos del HUD. */
            if (conocido >= LOBO_HUD_VIDAS &&
                conocido <= LOBO_HUD_PLANTILLA)
                return lobo_llamada_hud(p, conocido,
                                        lobo_hud_argumentos(conocido));
            /* Fase 39: sonar, musica, detener_sonido y volumen. */
            if (conocido >= LOBO_SONAR &&
                conocido <= LOBO_VOLUMEN_ACTUAL)
                return lobo_llamada_audio(p, conocido,
                                          lobo_audio_argumentos(conocido));
            /* Fase 40: guardar, cargar, borrar_save y records. */
            if (conocido >= LOBO_GUARDAR &&
                conocido <= LOBO_AJUSTE_ACTUAL)
                return lobo_llamada_save(p, conocido,
                                         lobo_save_argumentos(conocido));
            /* Fase 41: escenas, transiciones, camara y efectos. */
            if (conocido >= LOBO_CAMBIAR_ESCENA &&
                conocido <= LOBO_FADE)
                return lobo_llamada_escena(
                    p, conocido, lobo_escena_argumentos(conocido));
            if (conocido == LOBO_REINICIAR_ESCENA)
                return lobo_llamada_escena(p, conocido, 0);
            if (conocido != LOBO_CHOCAR) return 0;
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
/* Pasada de preescaneo                                               */
/* ------------------------------------------------------------------ */

/* Registra una firma: funcion nombre(a, b). Los parametros se guardan
 * como locales para que el cuerpo los vea por su nombre. */
static int lobo_pre_funcion(LoboParser *p, LoboToken *t, int n) {
    int i, params = 0, fi;
    LoboFuncion *f;
    if (n < 4) {
        lobo_error(p, "faltan los parentesis de la funcion");
        return 0;
    }
    if (t[1].tipo != TOK_ID || t[1].texto[0] == '$' || t[1].texto[0] == '@') {
        lobo_error(p, "se esperaba el nombre de la funcion");
        return 0;
    }
    if (t[2].tipo != TOK_OP || !lobo_igual(t[2].texto, "(")) {
        lobo_error(p, "se esperaba ( tras el nombre de la funcion");
        return 0;
    }
    if (lobo_buscar_funcion(p, t[1].texto) >= 0) {
        lobo_error(p, "funcion repetida");
        return 0;
    }
    if (p->prog->num_funciones >= LOBO_FUNCIONES_MAX) {
        lobo_error(p, "demasiadas funciones (maximo 64)");
        return 0;
    }
    fi = p->prog->num_funciones++;
    f = &p->prog->funciones[fi];
    lobo_copiar(f->nombre, t[1].texto, LOBO_NOMBRE_MAX);
    f->inicio = -1;
    f->params = 0;
    i = 3;
    while (i < n && !(t[i].tipo == TOK_OP && lobo_igual(t[i].texto, ")"))) {
        int local, es_tecla, es_funcion;
        if (params > 0) {
            if (!(t[i].tipo == TOK_OP && lobo_igual(t[i].texto, ","))) {
                lobo_error(p, "se esperaba , entre parametros");
                return 0;
            }
            i++;
        }
        if (i >= n || t[i].tipo != TOK_ID || t[i].texto[0] == '$' ||
            t[i].texto[0] == '@') {
            lobo_error(p, "se esperaba el nombre de un parametro");
            return 0;
        }
        if (params >= LOBO_PARAMS_MAX) {
            lobo_error(p, "demasiados parametros (maximo 8)");
            return 0;
        }
        if (lobo_nombre_conocido(t[i].texto, &es_tecla, &es_funcion) >= 0 ||
            es_tecla || es_funcion) {
            lobo_error(p, "parametro con nombre reservado");
            return 0;
        }
        local = lobo_nueva_local(p, t[i].texto);
        if (local < 0) return 0;
        f->param_local[params++] = local;
        i++;
    }
    if (i >= n || !(t[i].tipo == TOK_OP && lobo_igual(t[i].texto, ")"))) {
        lobo_error(p, "falta ) en la firma de la funcion");
        return 0;
    }
    if (i + 1 != n) {
        lobo_error(p, "sobran datos tras la firma de la funcion");
        return 0;
    }
    f->params = params;
    return 1;
}

/* Recorre todas las lineas y registra los nombres del programa. Con eso
 * el parseo puede rechazar nombres desconocidos (errores tipograficos
 * como "telca_derecha" en vez de leer 0 en silencio). */
static int lobo_preescanear(LoboParser *p, const char *texto) {
    const char *q = texto;
    int linea = 1;
    int vm = p->modo == LOBO_MODO_VM;
    while (*q) {
        const char *fin = q;
        LoboToken tokens[LOBO_TOKENS_LINEA];
        int n, i, largo;
        while (*fin && *fin != '\n') fin++;
        largo = (int)(fin - q);
        /* quitar el \r del final si el texto viene de un archivo CRLF */
        if (largo > 0 && q[largo - 1] == '\r') largo--;
        p->linea = linea;
        n = lobo_tokenizar(q, largo, tokens, LOBO_TOKENS_LINEA, vm);
        if (n < 0) {
            lobo_error_token(p, n);
            return 0;
        }
        if (vm && n >= 1 && tokens[0].tipo == TOK_ID &&
            lobo_igual(tokens[0].texto, "funcion")) {
            if (!lobo_pre_funcion(p, tokens, n)) return 0;
            goto siguiente;
        }
        if (vm && n >= 2 && tokens[0].tipo == TOK_ID &&
            lobo_igual(tokens[0].texto, "lista")) {
            int li = -1, tipo = LOBO_TIPO_ENTERO, nombre_en = 1;
            if (n >= 3 && tokens[1].tipo == TOK_ID &&
                lobo_tipo_codigo(tokens[1].texto) >= 0 &&
                tokens[2].tipo == TOK_ID) {
                tipo = lobo_tipo_codigo(tokens[1].texto);
                nombre_en = 2;
            }
            if (tokens[nombre_en].tipo == TOK_ID) {
                li = lobo_nueva_lista(p, tokens[nombre_en].texto);
                if (li < 0) return 0;
                p->tipos_listas[li] = (char)tipo;
            }
        }
        /* Declaraciones con tipo: entero x, booleano $listo,
         * texto @nombre. El tipo por defecto sigue siendo entero. */
        if (vm && n >= 2 && tokens[0].tipo == TOK_ID) {
            int tipo = lobo_tipo_codigo(tokens[0].texto);
            if (tipo >= 0 && tipo != LOBO_TIPO_LISTA &&
                tokens[1].tipo == TOK_ID) {
                const char *nombre = tokens[1].texto;
                if (nombre[0] == '$') {
                    int g = lobo_nueva_global(p, nombre);
                    if (g < 0) return 0;
                    p->tipos_globales[g] = (char)tipo;
                } else if (nombre[0] == '@') {
                    int e = lobo_nueva_entidad(p, nombre);
                    if (e < 0) return 0;
                    p->tipos_entidad[e] = (char)tipo;
                } else {
                    int l = lobo_nueva_local(p, nombre);
                    if (l < 0) return 0;
                    p->tipos_locales[l] = (char)tipo;
                }
            }
        }
        /* para cada x en lista: x es una local aunque no haya `=`. */
        if (vm && n >= 4 && tokens[0].tipo == TOK_ID &&
            lobo_igual(tokens[0].texto, "para") &&
            tokens[1].tipo == TOK_ID &&
            lobo_igual(tokens[1].texto, "cada") &&
            tokens[2].tipo == TOK_ID) {
            if (lobo_nueva_local(p, tokens[2].texto) < 0) {
                p->linea = linea;
                return 0;
            }
        }
        for (i = 0; i < n; i++) {
            if (tokens[i].tipo == TOK_ID && tokens[i].texto[0] == '$') {
                if (lobo_nueva_global(p, tokens[i].texto) < 0) return 0;
            } else if (tokens[i].tipo == TOK_ID && tokens[i].texto[0] == '@') {
                if (lobo_nueva_entidad(p, tokens[i].texto) < 0) return 0;
            }
        }
        /* mi.variable("nombre"): comparte la tabla con @nombre. */
        for (i = 0; i + 4 < n; i++) {
            if (tokens[i].tipo == TOK_ID && lobo_igual(tokens[i].texto, "mi") &&
                tokens[i + 1].tipo == TOK_OP &&
                lobo_igual(tokens[i + 1].texto, ".") &&
                tokens[i + 2].tipo == TOK_ID &&
                lobo_igual(tokens[i + 2].texto, "variable") &&
                tokens[i + 3].tipo == TOK_OP &&
                lobo_igual(tokens[i + 3].texto, "(") &&
                tokens[i + 4].tipo == TOK_TXT) {
                if (lobo_nueva_entidad(p, tokens[i + 4].texto) < 0) return 0;
            }
        }
        for (i = 0; i + 1 < n; i++) {
            if (tokens[i].tipo == TOK_ID && tokens[i + 1].tipo == TOK_OP &&
                tokens[i + 1].texto[0] == '=' && tokens[i + 1].texto[1] == 0) {
                int es_tecla, es_funcion;
                int conocido;
                if (tokens[i].texto[0] == '$' || tokens[i].texto[0] == '@')
                    continue; /* ya registradas arriba */
                conocido =
                    lobo_nombre_conocido(tokens[i].texto, &es_tecla, &es_funcion);
                if (conocido >= 0 && !es_funcion) continue; /* global del juego */
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
    siguiente:
        if (*fin == '\n') fin++;
        q = fin;
        linea++;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Compilacion de una linea                                           */
/* ------------------------------------------------------------------ */

/* Cierra el bloque de funcion: retorno implicito y salto del cuerpo. */
static int lobo_cerrar_funcion(LoboParser *p, LoboBloque *b) {
    if (!lobo_emitir(p, LOBO_NUM, 0)) return 0;
    if (!lobo_emitir(p, LOBO_RETORNAR, 0)) return 0;
    lobo_parchear(p->prog, b->salto_funcion, p->prog->largo);
    p->funcion_actual = -1;
    return 1;
}

/* Resuelve el destino de una asignacion o declaracion. Comparte las
 * comprobaciones de nombres reservados entre ambos caminos. */
static int lobo_resolver_destino(LoboParser *p, const char *nombre,
                                 int *es_global, int *es_entidad) {
    int es_tecla, es_funcion, conocido, destino = -1;
    *es_global = 0;
    *es_entidad = 0;
    if (p->modo == LOBO_MODO_VM && nombre[0] == '$') {
        *es_global = 1;
        destino = lobo_buscar_global(p, nombre);
        if (destino < 0) lobo_error(p, "global desconocida");
        return destino;
    }
    if (p->modo == LOBO_MODO_VM && nombre[0] == '@') {
        *es_entidad = 1;
        destino = lobo_buscar_entidad(p, nombre);
        if (destino < 0) lobo_error(p, "variable de entidad desconocida");
        return destino;
    }
    conocido = lobo_nombre_conocido(nombre, &es_tecla, &es_funcion);
    if (es_tecla) {
        lobo_error(p, "las teclas no se pueden asignar");
        return -1;
    }
    if (es_funcion) {
        lobo_error(p, "no se puede asignar a una funcion");
        return -1;
    }
    if (conocido >= 0) return conocido;
    destino = lobo_buscar_local(p, nombre);
    if (destino < 0) {
        lobo_error(p, "nombre desconocido");
        return -1;
    }
    return LOBO_VAR_LOCALES + destino;
}

/* Busca el bucle mas interno abierto para romper/continuar. */
static int lobo_bloque_bucle(LoboParser *p) {
    int i;
    for (i = p->num_bloques - 1; i >= 0; i--) {
        int tipo = p->bloques[i].tipo;
        if (tipo == BLOQUE_MIENTRAS || tipo == BLOQUE_REPETIR ||
            tipo == BLOQUE_PARA_CADA)
            return i;
    }
    return -1;
}

/* Compila la linea actual de p->tokens como sentencia. */
static int lobo_sentencia(LoboParser *p) {
    LoboToken *t = &p->tokens[0];
    int vm = p->modo == LOBO_MODO_VM;

    if (vm && t->tipo == TOK_ID) {
        /* --- sentencias de lobo 3 --- */
        if (lobo_igual(t->texto, "esperar")) {
            p->pos = 1;
            if (!lobo_expresion(p)) return 0;
            if (!lobo_fin_tokens(p)) {
                lobo_error(p, "sobran datos tras esperar");
                return 0;
            }
            return lobo_emitir(p, LOBO_ESPERAR, 0);
        }
        if (lobo_igual(t->texto, "detener")) {
            int todo = 0;
            p->pos = 1;
            if (!lobo_fin_tokens(p)) {
                if (p->num_tokens == 2 && p->tokens[1].tipo == TOK_ID &&
                    lobo_igual(p->tokens[1].texto, "todo"))
                    todo = 1;
                else {
                    lobo_error(p, "detener solo admite la palabra todo");
                    return 0;
                }
            }
            return lobo_emitir(p, LOBO_DETENER, todo);
        }
        /* Alias de la fase 27: detener_script y detener_todo, con o sin
         * parentesis, para que el planificador documente las dos acciones. */
        if (lobo_igual(t->texto, "detener_script") ||
            lobo_igual(t->texto, "detener_todo")) {
            int todo = lobo_igual(t->texto, "detener_todo");
            if (p->num_tokens == 1)
                return lobo_emitir(p, LOBO_DETENER, todo);
            if (p->num_tokens == 3 && p->tokens[1].tipo == TOK_OP &&
                lobo_igual(p->tokens[1].texto, "(") &&
                p->tokens[2].tipo == TOK_OP &&
                lobo_igual(p->tokens[2].texto, ")"))
                return lobo_emitir(p, LOBO_DETENER, todo);
            lobo_error(p, "detener_script y detener_todo van sin argumentos");
            return 0;
        }
        if (lobo_igual(t->texto, "reiniciar_escena")) {
            if (p->num_tokens == 1 ||
                (p->num_tokens == 3 && p->tokens[1].tipo == TOK_OP &&
                 lobo_igual(p->tokens[1].texto, "(") &&
                 p->tokens[2].tipo == TOK_OP &&
                 lobo_igual(p->tokens[2].texto, ")"))) {
                /* El opcode apila su resultado (como cerrar); como aqui va
                 * suelto, se descarta para no dejar la pila creciendo. */
                if (!lobo_emitir(p, LOBO_REINICIAR_ESCENA, 0)) return 0;
                return lobo_emitir(p, LOBO_POP, 0);
            }
            lobo_error(p, "reiniciar_escena va sin argumentos");
            return 0;
        }
        if (lobo_igual(t->texto, "mensaje")) {
            int idx;
            if (p->num_tokens != 2 || p->tokens[1].tipo != TOK_TXT) {
                lobo_error(p, "mensaje necesita un texto entre comillas");
                return 0;
            }
            idx = lobo_agregar_texto(p, p->tokens[1].texto);
            if (idx < 0) return 0;
            return lobo_emitir(p, LOBO_MENSAJE, idx);
        }
        if (lobo_igual(t->texto, "clonar")) {
            if (p->num_tokens != 1) {
                lobo_error(p, "clonar va solo en la linea");
                return 0;
            }
            if (!lobo_emitir(p, LOBO_CLONAR, 0)) return 0;
            return lobo_emitir(p, LOBO_POP, 0);
        }
        if (lobo_igual(t->texto, "funcion")) {
            int fi;
            LoboBloque *b;
            if (p->funcion_actual >= 0) {
                lobo_error(p, "no se pueden anidar funciones");
                return 0;
            }
            if (p->num_bloques != 0) {
                lobo_error(p, "la funcion va fuera de los bloques");
                return 0;
            }
            fi = lobo_buscar_funcion(p, p->num_tokens > 1 ? p->tokens[1].texto : "");
            if (fi < 0) {
                lobo_error(p, "funcion no preescaneada");
                return 0;
            }
            if (p->prog->funciones[fi].inicio >= 0) {
                lobo_error(p, "funcion repetida");
                return 0;
            }
            if (p->num_bloques >= 16) {
                lobo_error(p, "demasiados bloques anidados");
                return 0;
            }
            b = &p->bloques[p->num_bloques++];
            lobo_bloque_iniciar(b, BLOQUE_FUNCION, p->linea);
            b->salto_funcion = p->prog->largo;
            if (!lobo_emitir(p, LOBO_SALTO, 0)) return 0;
            p->prog->funciones[fi].inicio = p->prog->largo;
            p->funcion_actual = fi;
            return 1;
        }
        if (lobo_igual(t->texto, "retornar")) {
            if (p->funcion_actual < 0) {
                lobo_error(p, "retornar va dentro de una funcion");
                return 0;
            }
            p->pos = 1;
            if (lobo_fin_tokens(p)) {
                if (!lobo_emitir(p, LOBO_NUM, 0)) return 0;
            } else {
                if (!lobo_expresion(p)) return 0;
            }
            if (!lobo_fin_tokens(p)) {
                lobo_error(p, "sobran datos tras retornar");
                return 0;
            }
            return lobo_emitir(p, LOBO_RETORNAR, 0);
        }
        /* Declaraciones con tipo (fase 28). */
        {
            int tipo = lobo_tipo_codigo(t->texto);
            if (tipo >= 0) {
                int nombre_en = 1;
                if (tipo == LOBO_TIPO_LISTA && p->num_tokens >= 3 &&
                    p->tokens[1].tipo == TOK_ID &&
                    lobo_tipo_codigo(p->tokens[1].texto) >= 0 &&
                    p->tokens[2].tipo == TOK_ID)
                    nombre_en = 2;
                if (p->num_tokens < nombre_en + 1 ||
                    p->tokens[nombre_en].tipo != TOK_ID) {
                    lobo_error(p, "la declaracion necesita un nombre");
                    return 0;
                }
                if (tipo == LOBO_TIPO_LISTA) {
                    int li = lobo_buscar_lista(p, p->tokens[nombre_en].texto);
                    if (li < 0) {
                        lobo_error(p, "lista no preescaneada");
                        return 0;
                    }
                    /* El subtipo lo guardo el preescaneo. */
                    return 1; /* declaracion: no emite nada */
                }
                {
                    const char *nombre = p->tokens[nombre_en].texto;
                    int es_global = 0, es_entidad = 0;
                    int destino = lobo_resolver_destino(p, nombre, &es_global,
                                                         &es_entidad);
                    if (destino < 0) return 0;
                    lobo_marcar_tipo(p, nombre, tipo);
                    if (p->num_tokens == nombre_en + 1) {
                        /* Sin valor: 0 o texto vacio. */
                        if (tipo == LOBO_TIPO_TEXTO) {
                            int idx = lobo_agregar_texto(p, "");
                            if (idx < 0) return 0;
                            if (!lobo_emitir(p, LOBO_TEXTO, idx)) return 0;
                        } else if (!lobo_emitir(p, LOBO_NUM, 0)) {
                            return 0;
                        }
                        if (es_global)
                            return lobo_emitir(p, LOBO_GUARDAR_GLOBAL, destino);
                        if (es_entidad)
                            return lobo_emitir(p, LOBO_GUARDAR_ENTIDAD, destino);
                        return lobo_emitir(p, LOBO_ASIGNA, destino);
                    }
                    if (p->num_tokens < nombre_en + 2 ||
                        p->tokens[nombre_en + 1].tipo != TOK_OP ||
                        !lobo_igual(p->tokens[nombre_en + 1].texto, "=")) {
                        lobo_error(p, "se esperaba = en la declaracion");
                        return 0;
                    }
                    p->pos = nombre_en + 2;
                    if (!lobo_expresion(p)) return 0;
                    if (!lobo_fin_tokens(p)) {
                        lobo_error(p, "sobran datos tras la declaracion");
                        return 0;
                    }
                    if (es_global)
                        return lobo_emitir(p, LOBO_GUARDAR_GLOBAL, destino);
                    if (es_entidad)
                        return lobo_emitir(p, LOBO_GUARDAR_ENTIDAD, destino);
                    return lobo_emitir(p, LOBO_ASIGNA, destino);
                }
            }
        }
        if (lobo_igual(t->texto, "repetir")) {
            LoboBloque *b;
            int temp;
            if (p->num_tokens < 2) {
                lobo_error(p, "repetir necesita un numero");
                return 0;
            }
            if (p->num_bloques >= 16) {
                lobo_error(p, "demasiados bloques anidados");
                return 0;
            }
            temp = lobo_local_temporal(p);
            if (temp < 0) return 0;
            b = &p->bloques[p->num_bloques++];
            lobo_bloque_iniciar(b, BLOQUE_REPETIR, p->linea);
            b->contador = temp;
            p->pos = 1;
            if (!lobo_expresion(p)) return 0;
            if (!lobo_fin_tokens(p)) {
                lobo_error(p, "sobran datos tras repetir");
                return 0;
            }
            if (!lobo_emitir(p, LOBO_ASIGNA, LOBO_VAR_LOCALES + temp)) return 0;
            b->mientras_inicio = p->prog->largo;
            if (!lobo_emitir(p, LOBO_VAR, LOBO_VAR_LOCALES + temp)) return 0;
            if (!lobo_emitir(p, LOBO_NUM, 0)) return 0;
            if (!lobo_emitir(p, LOBO_MAYOR, 0)) return 0;
            b->salto_falso = p->prog->largo;
            return lobo_emitir(p, LOBO_SALTO_FALSO, 0);
        }
        if (lobo_igual(t->texto, "para")) {
            LoboBloque *b;
            int li, variable, idx;
            if (p->num_tokens < 5 || p->tokens[1].tipo != TOK_ID ||
                !lobo_igual(p->tokens[1].texto, "cada") ||
                p->tokens[2].tipo != TOK_ID || p->tokens[3].tipo != TOK_ID ||
                p->tokens[4].tipo != TOK_ID ||
                (!lobo_igual(p->tokens[3].texto, "en") &&
                 !lobo_igual(p->tokens[3].texto, "de"))) {
                lobo_error(p, "para cada variable en lista");
                return 0;
            }
            li = lobo_buscar_lista(p, p->tokens[4].texto);
            if (li < 0) {
                lobo_error(p, "lista desconocida en para cada");
                return 0;
            }
            variable = lobo_buscar_local(p, p->tokens[2].texto);
            if (variable < 0) {
                lobo_error(p, "variable de para cada desconocida");
                return 0;
            }
            idx = lobo_local_temporal(p);
            if (idx < 0) return 0;
            if (p->num_bloques >= 16) {
                lobo_error(p, "demasiados bloques anidados");
                return 0;
            }
            b = &p->bloques[p->num_bloques++];
            lobo_bloque_iniciar(b, BLOQUE_PARA_CADA, p->linea);
            b->lista = li;
            b->variable = variable;
            b->indice = idx;
            if (!lobo_emitir(p, LOBO_NUM, 0)) return 0;
            if (!lobo_emitir(p, LOBO_ASIGNA, LOBO_VAR_LOCALES + idx)) return 0;
            b->mientras_inicio = p->prog->largo;
            if (!lobo_emitir(p, LOBO_VAR, LOBO_VAR_LOCALES + idx)) return 0;
            if (!lobo_emitir(p, LOBO_LISTA_LARGO, li)) return 0;
            if (!lobo_emitir(p, LOBO_MENOR, 0)) return 0;
            b->salto_falso = p->prog->largo;
            if (!lobo_emitir(p, LOBO_SALTO_FALSO, 0)) return 0;
            if (!lobo_emitir(p, LOBO_VAR, LOBO_VAR_LOCALES + idx)) return 0;
            if (!lobo_emitir(p, LOBO_LISTA_ELEMENTO, li)) return 0;
            return lobo_emitir(p, LOBO_ASIGNA, LOBO_VAR_LOCALES + variable);
        }
        if (lobo_igual(t->texto, "romper") ||
            lobo_igual(t->texto, "continuar")) {
            int bi = lobo_bloque_bucle(p);
            LoboBloque *b;
            int salto;
            if (bi < 0) {
                lobo_error(p, "romper y continuar van dentro de un bucle");
                return 0;
            }
            if (p->num_tokens != 1) {
                lobo_error(p, "romper y continuar van solos en la linea");
                return 0;
            }
            b = &p->bloques[bi];
            salto = p->prog->largo;
            if (!lobo_emitir(p, LOBO_SALTO, 0)) return 0;
            if (lobo_igual(t->texto, "romper")) {
                if (b->n_romper >= LOBO_ROMPER_MAX) {
                    lobo_error(p, "demasiados romper en este bucle");
                    return 0;
                }
                b->salto_romper[b->n_romper++] = salto;
            } else {
                if (b->n_continuar >= LOBO_CONTINUAR_MAX) {
                    lobo_error(p, "demasiados continuar en este bucle");
                    return 0;
                }
                b->salto_continuar[b->n_continuar++] = salto;
            }
            return 1;
        }
    }

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
            lobo_bloque_iniciar(b, BLOQUE_SI, p->linea);
            b->salto_falso = p->prog->largo;
            return lobo_emitir(p, LOBO_SALTO_FALSO, 0);
        }
        if (lobo_igual(t->texto, "sino")) {
            LoboBloque *b;
            if (p->num_bloques == 0 || p->bloques[p->num_bloques - 1].tipo != BLOQUE_SI) {
                lobo_error(p, "sino sin un si abierto");
                return 0;
            }
            b = &p->bloques[p->num_bloques - 1];
            /* `sino si cond` se compila como un sino que salta al final
             * de la cadena y un si anidado en la rama falsa. Un solo fin
             * cierra toda la cadena (campo encadenado). */
            if (p->num_tokens >= 2 && p->tokens[1].tipo == TOK_ID &&
                lobo_igual(p->tokens[1].texto, "si")) {
                LoboBloque *nuevo;
                if (p->num_tokens < 3) {
                    lobo_error(p, "sino si necesita una condicion");
                    return 0;
                }
                if (p->num_bloques >= 16) {
                    lobo_error(p, "demasiados bloques anidados");
                    return 0;
                }
                b->salto_sino = p->prog->largo;
                if (!lobo_emitir(p, LOBO_SALTO, 0)) return 0;
                lobo_parchear(p->prog, b->salto_falso, p->prog->largo);
                b->tipo = BLOQUE_SINO;
                nuevo = &p->bloques[p->num_bloques++];
                lobo_bloque_iniciar(nuevo, BLOQUE_SI, p->linea);
                nuevo->encadenado = 1;
                p->pos = 2;
                if (!lobo_expresion(p)) return 0;
                if (!lobo_fin_tokens(p)) {
                    lobo_error(p, "sobran datos tras la condicion del si");
                    return 0;
                }
                nuevo->salto_falso = p->prog->largo;
                return lobo_emitir(p, LOBO_SALTO_FALSO, 0);
            }
            if (p->num_tokens != 1) {
                lobo_error(p, "sino va solo en la linea");
                return 0;
            }
            b->salto_sino = p->prog->largo;
            if (!lobo_emitir(p, LOBO_SALTO, 0)) return 0;
            /* el cuerpo del si salta aqui (al principio del sino) */
            lobo_parchear(p->prog, b->salto_falso, p->prog->largo);
            b->tipo = BLOQUE_SINO;
            return 1;
        }
        if (lobo_igual(t->texto, "fin")) {
            LoboBloque *b;
            int fin, i;
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
                fin = p->prog->largo;
                lobo_parchear(p->prog, b->salto_falso, fin);
                for (i = 0; i < b->n_romper; i++)
                    lobo_parchear(p->prog, b->salto_romper[i], fin);
                for (i = 0; i < b->n_continuar; i++)
                    lobo_parchear(p->prog, b->salto_continuar[i],
                                  b->mientras_inicio);
            } else if (b->tipo == BLOQUE_REPETIR ||
                       b->tipo == BLOQUE_PARA_CADA) {
                b->continuar_inicio = p->prog->largo;
                if (b->tipo == BLOQUE_REPETIR) {
                    if (!lobo_emitir(p, LOBO_VAR,
                                     LOBO_VAR_LOCALES + b->contador)) return 0;
                    if (!lobo_emitir(p, LOBO_NUM, 1)) return 0;
                    if (!lobo_emitir(p, LOBO_RESTA, 0)) return 0;
                    if (!lobo_emitir(p, LOBO_ASIGNA,
                                     LOBO_VAR_LOCALES + b->contador)) return 0;
                } else {
                    if (!lobo_emitir(p, LOBO_VAR,
                                     LOBO_VAR_LOCALES + b->indice)) return 0;
                    if (!lobo_emitir(p, LOBO_NUM, 1)) return 0;
                    if (!lobo_emitir(p, LOBO_SUMA, 0)) return 0;
                    if (!lobo_emitir(p, LOBO_ASIGNA,
                                     LOBO_VAR_LOCALES + b->indice)) return 0;
                }
                if (!lobo_emitir(p, LOBO_SALTO, b->mientras_inicio)) return 0;
                fin = p->prog->largo;
                lobo_parchear(p->prog, b->salto_falso, fin);
                for (i = 0; i < b->n_romper; i++)
                    lobo_parchear(p->prog, b->salto_romper[i], fin);
                for (i = 0; i < b->n_continuar; i++)
                    lobo_parchear(p->prog, b->salto_continuar[i],
                                  b->continuar_inicio);
            } else if (b->tipo == BLOQUE_FUNCION) {
                if (!lobo_cerrar_funcion(p, b)) return 0;
                p->num_bloques--;
                return 1;
            } else {
                /* si a pelo: el falso salta aqui; con sino: el salto
                 * del sino apunta aqui. */
                fin = p->prog->largo;
                if (b->salto_sino >= 0)
                    lobo_parchear(p->prog, b->salto_sino, fin);
                else
                    lobo_parchear(p->prog, b->salto_falso, fin);
                /* Una cadena de "sino si" se cierra con un solo fin. */
                for (;;) {
                    int encadenado = b->encadenado;
                    p->num_bloques--;
                    if (!encadenado) break;
                    if (p->num_bloques <= 0) {
                        lobo_error(p, "falta el fin del si encadenado");
                        return 0;
                    }
                    b = &p->bloques[p->num_bloques - 1];
                    if (b->salto_sino >= 0)
                        lobo_parchear(p->prog, b->salto_sino, fin);
                }
                return 1;
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
            lobo_bloque_iniciar(b, BLOQUE_MIENTRAS, p->linea);
            b->mientras_inicio = p->prog->largo;
            if (!lobo_expresion(p)) return 0;
            if (!lobo_fin_tokens(p)) {
                lobo_error(p, "sobran datos tras la condicion del mientras");
                return 0;
            }
            b->salto_falso = p->prog->largo;
            return lobo_emitir(p, LOBO_SALTO_FALSO, 0);
        }
        /* Asignacion a mi (fase 26): mi.x = ... o mi.variable("n") = ... */
        if (vm && p->num_tokens >= 4 && lobo_igual(t->texto, "mi") &&
            p->tokens[1].tipo == TOK_OP &&
            lobo_igual(p->tokens[1].texto, ".")) {
            int campo = -1, desde;
            if (p->tokens[2].tipo != TOK_ID) {
                lobo_error(p, "se esperaba un campo de mi");
                return 0;
            }
            if (lobo_igual(p->tokens[2].texto, "variable")) {
                int slot;
                if (p->num_tokens < 7 ||
                    p->tokens[3].tipo != TOK_OP ||
                    !lobo_igual(p->tokens[3].texto, "(") ||
                    p->tokens[4].tipo != TOK_TXT ||
                    p->tokens[5].tipo != TOK_OP ||
                    !lobo_igual(p->tokens[5].texto, ")") ||
                    p->tokens[6].tipo != TOK_OP ||
                    !lobo_igual(p->tokens[6].texto, "=")) {
                    lobo_error(p, "asignacion invalida a mi.variable");
                    return 0;
                }
                slot = lobo_buscar_entidad(p, p->tokens[4].texto);
                if (slot < 0) {
                    lobo_error(p, "variable de entidad desconocida");
                    return 0;
                }
                campo = LOBO_MI_VARS + slot;
                desde = 7;
            } else {
                if (p->tokens[3].tipo != TOK_OP ||
                    !lobo_igual(p->tokens[3].texto, "=")) {
                    lobo_error(p, "asignacion invalida a mi");
                    return 0;
                }
                campo = lobo_campo_mi(p->tokens[2].texto);
                if (campo < 0) {
                    lobo_error(p, "campo de mi desconocido");
                    return 0;
                }
                desde = 4;
            }
            p->pos = desde;
            if (!lobo_expresion(p)) return 0;
            if (!lobo_fin_tokens(p)) {
                lobo_error(p, "sobran datos tras la asignacion");
                return 0;
            }
            return lobo_emitir(p, LOBO_GUARDAR_MI, campo);
        }
        /* Asignacion: nombre = expresion. */
        if (p->num_tokens >= 2 && p->tokens[1].tipo == TOK_OP &&
            p->tokens[1].texto[0] == '=' && p->tokens[1].texto[1] == 0) {
            int es_global = 0, es_entidad = 0;
            int destino = lobo_resolver_destino(p, t->texto, &es_global,
                                                 &es_entidad);
            if (destino < 0) return 0;
            p->pos = 2;
            if (!lobo_expresion(p)) return 0;
            if (!lobo_fin_tokens(p)) {
                lobo_error(p, "sobran datos tras la asignacion");
                return 0;
            }
            if (es_global) return lobo_emitir(p, LOBO_GUARDAR_GLOBAL, destino);
            if (es_entidad) return lobo_emitir(p, LOBO_GUARDAR_ENTIDAD, destino);
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
/* lobo_compilar / lobo_parsear                                       */
/* ------------------------------------------------------------------ */

void lobo_programa_iniciar(LoboPrograma *programa) {
    if (!programa) return;
    programa->insts = NULL;
    programa->largo = 0;
    programa->capacidad = 0;
    programa->num_funciones = 0;
    programa->num_textos = 0;
    programa->num_ent_nombres = 0;
    {
        int i;
        for (i = 0; i < LOBO_LOCALES_MAX; i++) programa->tipos_locales[i] = 0;
        for (i = 0; i < LOBO_GLOBALES_MAX; i++) programa->tipos_globales[i] = 0;
        for (i = 0; i < LOBO_ENTIDAD_VARS_MAX; i++) programa->tipos_entidad[i] = 0;
        for (i = 0; i < LOBO_LISTAS_MAX; i++) programa->tipos_listas[i] = 0;
    }
}

void lobo_programa_liberar(LoboPrograma *programa) {
    if (!programa) return;
    free(programa->insts);
    lobo_programa_iniciar(programa);
}

int lobo_compilar(const char *texto, LoboPrograma *programa, int modo,
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
    if (modo != LOBO_MODO_VM) modo = LOBO_MODO_CLASICO;
    programa->largo = 0;
    programa->num_funciones = 0;
    programa->num_textos = 0;
    programa->num_ent_nombres = 0;
    if (!lobo_programa_reservar(programa, 1)) {
        if (error && tam_error > 0)
            lobo_copiar(error, "sin memoria para el programa", tam_error);
        return 0;
    }

    p.prog = programa;
    p.error = error;
    p.tam_error = tam_error;
    p.linea = 1;
    p.modo = modo;
    p.limite_insts = modo == LOBO_MODO_VM ? LOBO_INSTS_MAX : LOBO_INSTS_CLASICO;
    p.limite_locales =
        modo == LOBO_MODO_VM ? LOBO_LOCALES_MAX : LOBO_LOCALES_CLASICO;
    p.num_tokens = 0;
    p.pos = 0;
    p.num_bloques = 0;
    p.funcion_actual = -1;
    p.num_locales = 0;
    p.num_globales = 0;
    p.num_listas = 0;
    p.num_temporales = 0;
    programa->num_ent_nombres = 0;
    {
        int i;
        for (i = 0; i < LOBO_LOCALES_MAX; i++) {
            p.locales[i][0] = 0;
            p.tipos_locales[i] = 0;
            programa->tipos_locales[i] = 0;
        }
        for (i = 0; i < LOBO_GLOBALES_MAX; i++) {
            p.globales[i][0] = 0;
            p.tipos_globales[i] = 0;
            programa->tipos_globales[i] = 0;
        }
        for (i = 0; i < LOBO_ENTIDAD_NOMBRES_MAX; i++)
            programa->entidad_nombres[i][0] = 0;
        for (i = 0; i < LOBO_ENTIDAD_VARS_MAX; i++) {
            p.tipos_entidad[i] = 0;
            programa->tipos_entidad[i] = 0;
        }
        for (i = 0; i < LOBO_LISTAS_MAX; i++) {
            p.listas[i][0] = 0;
            p.tipos_listas[i] = 0;
            programa->tipos_listas[i] = 0;
        }
    }

    /* 1. Preescaneo de locales, globales, listas y funciones. */
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
            n = lobo_tokenizar(q, largo, p.tokens, LOBO_TOKENS_LINEA,
                               modo == LOBO_MODO_VM);
            if (n < 0) {
                lobo_error_token(&p, n);
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
    /* Los tipos declarados viajan con el programa para que el editor y
     * la serializacion LOGIC3 sepan que guarda cada variable. */
    {
        int i;
        for (i = 0; i < LOBO_LOCALES_MAX; i++)
            programa->tipos_locales[i] = p.tipos_locales[i];
        for (i = 0; i < LOBO_GLOBALES_MAX; i++)
            programa->tipos_globales[i] = p.tipos_globales[i];
        for (i = 0; i < LOBO_ENTIDAD_VARS_MAX; i++)
            programa->tipos_entidad[i] = p.tipos_entidad[i];
        for (i = 0; i < LOBO_LISTAS_MAX; i++)
            programa->tipos_listas[i] = p.tipos_listas[i];
    }
    return 1;
}

int lobo_parsear(const char *texto, LoboPrograma *programa,
                 char *error, int tam_error) {
    return lobo_compilar(texto, programa, LOBO_MODO_CLASICO, error, tam_error);
}

/* ------------------------------------------------------------------ */
/* Interprete clasico (LOGIC2)                                        */
/* ------------------------------------------------------------------ */

/* Lee una variable (global del juego o local del script). */
static int lobo_leer_var(const LoboEstado *st, const LoboMaquina *m,
                         int var) {
    switch (var) {
    case LOBO_VAR_X: return st->x;
    case LOBO_VAR_Y: return st->y;
    case LOBO_VAR_VEL: return st->vel;
    case LOBO_VAR_FOTOGRAMA: return st->fotograma;
    case LOBO_VAR_VEL_X: return st->vel_x;
    case LOBO_VAR_VEL_Y: return st->vel_y;
    case LOBO_VAR_GRAVEDAD: return st->gravedad;
    case LOBO_VAR_REBOTE: return st->rebote;
    case LOBO_VAR_FRICCION: return st->friccion;
    case LOBO_VAR_EN_SUELO: return st->en_suelo;
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
    case LOBO_VAR_VEL_X: st->vel_x = valor; return;
    case LOBO_VAR_VEL_Y: st->vel_y = valor; return;
    case LOBO_VAR_GRAVEDAD: st->gravedad = valor; return;
    case LOBO_VAR_REBOTE: st->rebote = valor; return;
    case LOBO_VAR_FRICCION: st->friccion = valor; return;
    case LOBO_VAR_EN_SUELO: st->en_suelo = valor; return;
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
        /* Fase 37: entrada extendida. */
        case LOBO_TECLA_PULSADA:
        case LOBO_TECLA_MANTENIDA:
        case LOBO_TECLA_SOLTADA:
        case LOBO_RATON_X:
        case LOBO_RATON_Y:
        case LOBO_RATON_RUEDA:
        case LOBO_RATON_DENTRO:
            if (np >= LOBO_PILA_MAX) { maquina->agotado = 1; return; }
            pila[np++] = lobo_entrada_valor(estado, in->op, in->valor);
            break;
        case LOBO_RATON_BOTON:
            if (np < 1) { maquina->agotado = 1; return; }
            b = pila[--np];
            if (np >= LOBO_PILA_MAX) { maquina->agotado = 1; return; }
            pila[np++] = lobo_entrada_valor(estado, in->op, b);
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

/* ------------------------------------------------------------------ */
/* VM resumible (lobo 3, fase 25)                                     */
/* ------------------------------------------------------------------ */

/* Apila un valor; deja ok a 0 si la pila esta llena. */
static void lobo_vm_push(LoboMaquina *m, int valor, int *ok) {
    if (m->np >= LOBO_PILA_VM_MAX) { *ok = 0; return; }
    m->pila[m->np++] = valor;
}

/* Desapila un valor; deja ok a 0 si la pila esta vacia. */
static int lobo_vm_pop(LoboMaquina *m, int *ok) {
    if (m->np <= 0) { *ok = 0; return 0; }
    return m->pila[--m->np];
}

/* Lista con ese indice (NULL si no hay memoria o el indice no vale). */
static LoboLista *lobo_vm_lista(LoboMemoria *mem, int indice) {
    if (!mem || indice < 0 || indice >= LOBO_LISTAS_MAX) return NULL;
    return &mem->listas[indice];
}

/* --- textos por valor (fase 28) ------------------------------------- */

/* Un valor de la pila es texto si tiene la marca alta. */
static int lobo_es_texto(int valor) {
    return ((unsigned)valor & (unsigned)LOBO_TEXTO_MARCA) != 0;
}

/* Handle de un texto del programa o del runtime. */
static int lobo_texto_handle(int indice) {
    return (int)((unsigned)LOBO_TEXTO_MARCA | (unsigned)indice);
}

/* Resuelve un handle a una cadena C. Los textos del programa van primero
 * y los creados por unir() despues, compartiendo la memoria del juego. */
static const char *lobo_texto_resolver(const LoboPrograma *programa,
                                       LoboMemoria *memoria, int valor) {
    int idx;
    if (!lobo_es_texto(valor)) return NULL;
    idx = (int)((unsigned)valor & 0x3fffffffu);
    if (idx < LOBO_TEXTOS_MAX) {
        if (idx >= 0 && idx < programa->num_textos)
            return programa->textos[idx];
        return NULL;
    }
    idx -= LOBO_TEXTOS_MAX;
    if (memoria && idx >= 0 && idx < LOBO_TEXTOS_RUNTIME_MAX &&
        idx < memoria->num_textos)
        return memoria->textos[idx];
    return NULL;
}

/* Largo de un texto, sin pasar de los 64 bytes. */
static int lobo_texto_largo(const char *texto) {
    int n = 0;
    while (texto[n] && n < LOBO_TEXTO_MAX - 1) n++;
    return n;
}

/* Concatena dos textos en la tabla runtime y devuelve su handle. Si no
 * hay memoria (caso de un --vm-test sin LoboMemoria) devuelve un handle
 * vacio estable para no romper el determinismo. */
static int lobo_texto_unir(LoboMemoria *memoria, const char *a,
                           const char *b) {
    char resultado[LOBO_TEXTO_MAX];
    int i = 0, j, idx;
    for (j = 0; a && a[j] && i < LOBO_TEXTO_MAX - 1; j++)
        resultado[i++] = a[j];
    for (j = 0; b && b[j] && i < LOBO_TEXTO_MAX - 1; j++)
        resultado[i++] = b[j];
    resultado[i] = 0;
    if (!memoria) return lobo_texto_handle(0);
    if (memoria->num_textos < LOBO_TEXTOS_RUNTIME_MAX)
        idx = memoria->num_textos++;
    else
        idx = LOBO_TEXTOS_RUNTIME_MAX - 1; /* recicla el ultimo slot */
    lobo_copiar(memoria->textos[idx], resultado, LOBO_TEXTO_MAX);
    return lobo_texto_handle(LOBO_TEXTOS_MAX + idx);
}

/* Comparacion con textos: si ambos lados son texto se comparan byte a
 * byte; si no, se mantiene la comparacion entera de siempre. */
static int lobo_comparar(const LoboPrograma *programa, LoboMemoria *memoria,
                         int op, int a, int b) {
    const char *ta = lobo_texto_resolver(programa, memoria, a);
    const char *tb = lobo_texto_resolver(programa, memoria, b);
    int c = 0;
    if (ta && tb) {
        while (ta[c] && ta[c] == tb[c]) c++;
        c = ta[c] - tb[c];
    } else {
        c = a - b;
    }
    if (op == LOBO_IGUAL) return c == 0;
    if (op == LOBO_DISTINTO) return c != 0;
    if (op == LOBO_MENOR) return c < 0;
    if (op == LOBO_MAYOR) return c > 0;
    if (op == LOBO_MENOR_IG) return c <= 0;
    return c >= 0; /* LOBO_MAYOR_IG */
}

/* Generador lineal congruencial, propio de cada maquina y por tanto
 * reproducible con --vm-test y con -Repetir. */
static int lobo_aleatorio(LoboMaquina *maquina, int a, int b) {
    unsigned semilla;
    unsigned rango;
    if (a > b) { int t = a; a = b; b = t; }
    rango = (unsigned)(b - a) + 1u;
    semilla = (unsigned)maquina->semilla * 1103515245u + 12345u;
    maquina->semilla = (int)(semilla & 0x7fffffffu);
    return a + (int)((semilla >> 16) % rango);
}

/* Ejecuta una instruccion. */
int lobo_paso(const LoboPrograma *programa, LoboEstado *estado,
              LoboMaquina *maquina) {
    const LoboInst *in;
    int ok = 1, a = 0, b = 0;

    if (!programa || !maquina) return LOBO_PASO_ERROR;
    if (maquina->estado != LOBO_VM_LISTO) return LOBO_PASO_EVENTO;
    if (programa->largo <= 0 || maquina->pc < 0 ||
        maquina->pc >= programa->largo) {
        maquina->estado = LOBO_VM_TERMINADO;
        return LOBO_PASO_EVENTO;
    }
    in = &programa->insts[maquina->pc++];
    if (g_hook_instruccion) g_hook_instruccion(maquina->pc - 1, 0);

    switch (in->op) {
    case LOBO_NUM:
        lobo_vm_push(maquina, in->valor, &ok);
        break;
    case LOBO_VAR:
        lobo_vm_push(maquina, lobo_leer_var(estado, maquina, in->valor), &ok);
        break;
    case LOBO_TECLA:
        lobo_vm_push(maquina, (estado->entrada & (unsigned)in->valor) != 0, &ok);
        break;
    /* Fase 37: entrada extendida. */
    case LOBO_TECLA_PULSADA:
    case LOBO_TECLA_MANTENIDA:
    case LOBO_TECLA_SOLTADA:
    case LOBO_RATON_X:
    case LOBO_RATON_Y:
    case LOBO_RATON_RUEDA:
    case LOBO_RATON_DENTRO:
        lobo_vm_push(maquina, lobo_entrada_valor(estado, in->op, in->valor), &ok);
        break;
    case LOBO_RATON_BOTON:
        a = lobo_vm_pop(maquina, &ok);
        if (ok) lobo_vm_push(maquina, lobo_entrada_valor(estado, in->op, a), &ok);
        break;
    case LOBO_CHOCAR:
        b = lobo_vm_pop(maquina, &ok);
        a = lobo_vm_pop(maquina, &ok);
        if (ok) {
            int r = estado->choca ? estado->choca(estado->ctx, a, b) != 0 : 0;
            lobo_vm_push(maquina, r, &ok);
        }
        break;
    case LOBO_CERRAR:
        maquina->cerrar = 1;
        lobo_vm_push(maquina, 0, &ok);
        break;
    case LOBO_SUMA:
        b = lobo_vm_pop(maquina, &ok);
        a = lobo_vm_pop(maquina, &ok);
        if (ok) lobo_vm_push(maquina, (int)((unsigned)a + (unsigned)b), &ok);
        break;
    case LOBO_RESTA:
        b = lobo_vm_pop(maquina, &ok);
        a = lobo_vm_pop(maquina, &ok);
        if (ok) lobo_vm_push(maquina, (int)((unsigned)a - (unsigned)b), &ok);
        break;
    case LOBO_MULT:
        b = lobo_vm_pop(maquina, &ok);
        a = lobo_vm_pop(maquina, &ok);
        if (ok) lobo_vm_push(maquina, (int)((unsigned)a * (unsigned)b), &ok);
        break;
    case LOBO_DIV:
        b = lobo_vm_pop(maquina, &ok);
        a = lobo_vm_pop(maquina, &ok);
        if (ok) {
            int r = (b == 0 || (a == LOBO_INT_MIN && b == -1)) ? 0 : a / b;
            lobo_vm_push(maquina, r, &ok);
        }
        break;
    case LOBO_MOD:
        b = lobo_vm_pop(maquina, &ok);
        a = lobo_vm_pop(maquina, &ok);
        if (ok) {
            int r = (b == 0 || (a == LOBO_INT_MIN && b == -1)) ? 0 : a % b;
            lobo_vm_push(maquina, r, &ok);
        }
        break;
    case LOBO_IGUAL:
    case LOBO_DISTINTO:
    case LOBO_MENOR:
    case LOBO_MAYOR:
    case LOBO_MENOR_IG:
    case LOBO_MAYOR_IG:
        b = lobo_vm_pop(maquina, &ok);
        a = lobo_vm_pop(maquina, &ok);
        if (ok)
            lobo_vm_push(maquina,
                         lobo_comparar(programa, estado->memoria, in->op,
                                       a, b), &ok);
        break;
    case LOBO_Y:
    case LOBO_O:
        b = lobo_vm_pop(maquina, &ok);
        a = lobo_vm_pop(maquina, &ok);
        if (ok)
            lobo_vm_push(maquina, in->op == LOBO_Y ? (a != 0 && b != 0)
                                                   : (a != 0 || b != 0), &ok);
        break;
    case LOBO_NO:
        a = lobo_vm_pop(maquina, &ok);
        if (ok) lobo_vm_push(maquina, a == 0, &ok);
        break;
    case LOBO_ASIGNA:
        a = lobo_vm_pop(maquina, &ok);
        if (ok) lobo_escribir_var(estado, maquina, in->valor, a);
        break;
    case LOBO_SALTO:
        if (in->valor < 0 || in->valor > programa->largo) { ok = 0; break; }
        maquina->pc = in->valor;
        break;
    case LOBO_SALTO_FALSO:
        a = lobo_vm_pop(maquina, &ok);
        if (ok && a == 0) {
            if (in->valor < 0 || in->valor > programa->largo) { ok = 0; break; }
            maquina->pc = in->valor;
        }
        break;
    case LOBO_POP:
        if (maquina->np <= 0) { ok = 0; break; }
        maquina->np--;
        break;

    /* --- lobo 3 --- */
    case LOBO_ESPERAR:
        a = lobo_vm_pop(maquina, &ok);
        if (ok && a > 0) {
            maquina->espera = a;
            maquina->estado = LOBO_VM_ESPERANDO;
            return LOBO_PASO_EVENTO;
        }
        break;
    case LOBO_DETENER:
        if (in->valor == 1) maquina->detener_todo = 1;
        maquina->estado = LOBO_VM_TERMINADO;
        return LOBO_PASO_EVENTO;
    case LOBO_LLAMAR: {
        const LoboFuncion *f;
        LoboFrame *fr;
        if (in->valor < 0 || in->valor >= programa->num_funciones) {
            ok = 0;
            break;
        }
        f = &programa->funciones[in->valor];
        if (f->inicio < 0 || f->inicio >= programa->largo) { ok = 0; break; }
        if (maquina->num_frames >= LOBO_FRAMES_MAX) { ok = 0; break; }
        if (maquina->np < f->params) { ok = 0; break; }
        fr = &maquina->frames[maquina->num_frames];
        fr->retorno = maquina->pc;
        fr->pila_base = maquina->np - f->params;
        {
            int i;
            for (i = 0; i < LOBO_LOCALES_MAX; i++)
                fr->locales[i] = maquina->locales[i];
        }
        maquina->num_frames++;
        maquina->np = fr->pila_base;
        {
            int i;
            for (i = 0; i < LOBO_LOCALES_MAX; i++) maquina->locales[i] = 0;
            for (i = 0; i < f->params; i++)
                maquina->locales[f->param_local[i]] = maquina->pila[fr->pila_base + i];
        }
        maquina->pc = f->inicio;
        break;
    }
    case LOBO_RETORNAR: {
        LoboFrame *fr;
        if (maquina->num_frames <= 0) {
            maquina->estado = LOBO_VM_TERMINADO;
            return LOBO_PASO_EVENTO;
        }
        a = lobo_vm_pop(maquina, &ok);
        if (!ok) break;
        fr = &maquina->frames[--maquina->num_frames];
        maquina->np = fr->pila_base;
        {
            int i;
            for (i = 0; i < LOBO_LOCALES_MAX; i++)
                maquina->locales[i] = fr->locales[i];
        }
        maquina->pc = fr->retorno;
        lobo_vm_push(maquina, a, &ok);
        break;
    }
    case LOBO_CARGAR_GLOBAL:
        lobo_vm_push(maquina,
                     (estado->memoria && in->valor >= 0 &&
                      in->valor < LOBO_GLOBALES_MAX)
                         ? estado->memoria->globales[in->valor]
                         : 0,
                     &ok);
        break;
    case LOBO_GUARDAR_GLOBAL:
        a = lobo_vm_pop(maquina, &ok);
        if (ok && estado->memoria && in->valor >= 0 &&
            in->valor < LOBO_GLOBALES_MAX)
            estado->memoria->globales[in->valor] = a;
        break;
    case LOBO_CARGAR_ENTIDAD:
        if (maquina->entidad > 0 && estado->entidad_leer)
            lobo_vm_push(maquina,
                         estado->entidad_leer(estado->ctx_entidades,
                                              maquina->entidad,
                                              LOBO_MI_VARS + in->valor),
                         &ok);
        else
            lobo_vm_push(maquina,
                         (estado->memoria && in->valor >= 0 &&
                          in->valor < LOBO_ENTIDAD_VARS_MAX)
                             ? estado->memoria->entidad_vars[in->valor]
                             : 0,
                         &ok);
        break;
    case LOBO_GUARDAR_ENTIDAD:
        a = lobo_vm_pop(maquina, &ok);
        if (ok && maquina->entidad > 0 && estado->entidad_escribir)
            estado->entidad_escribir(estado->ctx_entidades, maquina->entidad,
                                     LOBO_MI_VARS + in->valor, a);
        else if (ok && estado->memoria && in->valor >= 0 &&
                 in->valor < LOBO_ENTIDAD_VARS_MAX)
            estado->memoria->entidad_vars[in->valor] = a;
        break;
    case LOBO_LISTA_LARGO: {
        LoboLista *l = lobo_vm_lista(estado->memoria, in->valor);
        lobo_vm_push(maquina, l ? l->n : 0, &ok);
        break;
    }
    case LOBO_LISTA_ELEMENTO: {
        LoboLista *l = lobo_vm_lista(estado->memoria, in->valor);
        a = lobo_vm_pop(maquina, &ok);
        if (ok)
            lobo_vm_push(maquina,
                         (l && a >= 0 && a < l->n) ? l->elem[a] : 0, &ok);
        break;
    }
    case LOBO_LISTA_FIJAR: {
        LoboLista *l = lobo_vm_lista(estado->memoria, in->valor);
        a = lobo_vm_pop(maquina, &ok); /* valor */
        b = lobo_vm_pop(maquina, &ok); /* indice */
        if (ok && l && b >= 0 && b < l->n) l->elem[b] = a;
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_LISTA_ANADIR: {
        LoboLista *l = lobo_vm_lista(estado->memoria, in->valor);
        a = lobo_vm_pop(maquina, &ok);
        if (ok && l && l->n < LOBO_LISTA_ELEMENTOS_MAX) l->elem[l->n++] = a;
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_LISTA_INSERTAR: {
        LoboLista *l = lobo_vm_lista(estado->memoria, in->valor);
        int i;
        a = lobo_vm_pop(maquina, &ok); /* valor */
        b = lobo_vm_pop(maquina, &ok); /* indice */
        if (ok && l && l->n < LOBO_LISTA_ELEMENTOS_MAX && b >= 0 && b <= l->n) {
            for (i = l->n; i > b; i--) l->elem[i] = l->elem[i - 1];
            l->elem[b] = a;
            l->n++;
        }
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_LISTA_BORRAR: {
        LoboLista *l = lobo_vm_lista(estado->memoria, in->valor);
        int i;
        b = lobo_vm_pop(maquina, &ok);
        if (ok && l && b >= 0 && b < l->n) {
            for (i = b; i + 1 < l->n; i++) l->elem[i] = l->elem[i + 1];
            l->n--;
        }
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_LISTA_TIENE: {
        LoboLista *l = lobo_vm_lista(estado->memoria, in->valor);
        int i, r = 0;
        a = lobo_vm_pop(maquina, &ok);
        if (ok) {
            if (l) {
                for (i = 0; i < l->n; i++)
                    if (l->elem[i] == a) { r = 1; break; }
            }
            lobo_vm_push(maquina, r, &ok);
        }
        break;
    }
    case LOBO_LISTA_ORDENAR: {
        LoboLista *l = lobo_vm_lista(estado->memoria, in->valor);
        int i, j;
        if (l) {
            for (i = 1; i < l->n; i++) {
                int v = l->elem[i];
                j = i - 1;
                while (j >= 0 && l->elem[j] > v) {
                    l->elem[j + 1] = l->elem[j];
                    j--;
                }
                l->elem[j + 1] = v;
            }
        }
        lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_LISTA_LIMPIAR: {
        LoboLista *l = lobo_vm_lista(estado->memoria, in->valor);
        if (l) l->n = 0;
        lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_MENSAJE:
        if (in->valor >= 0 && in->valor < programa->num_textos) {
            const char *texto = programa->textos[in->valor];
            if (estado->enviar)
                estado->enviar(estado->ctx_eventos, maquina->entidad, NULL,
                               texto);
            if (estado->memoria) {
                lobo_copiar(estado->memoria->mensaje, texto, LOBO_TEXTO_MAX);
                estado->memoria->mensajes++;
            }
        }
        break;
    /* --- fase 27: mensajes --- */
    case LOBO_ENVIAR:
        if (in->valor >= 0 && in->valor < programa->num_textos) {
            const char *texto = programa->textos[in->valor];
            if (estado->enviar)
                estado->enviar(estado->ctx_eventos, maquina->entidad, NULL,
                               texto);
            if (estado->memoria) {
                lobo_copiar(estado->memoria->mensaje, texto, LOBO_TEXTO_MAX);
                estado->memoria->mensajes++;
            }
        }
        lobo_vm_push(maquina, 0, &ok);
        break;
    case LOBO_ENVIAR_A:
        if (in->valor >= 0 && in->valor < programa->num_textos) {
            const char *texto = programa->textos[in->valor];
            const char *destino =
                (in->extra >= 0 && in->extra < programa->num_textos)
                    ? programa->textos[in->extra]
                    : NULL;
            if (estado->enviar)
                estado->enviar(estado->ctx_eventos, maquina->entidad, destino,
                               texto);
            if (estado->memoria) {
                lobo_copiar(estado->memoria->mensaje, texto, LOBO_TEXTO_MAX);
                estado->memoria->mensajes++;
            }
        }
        lobo_vm_push(maquina, 0, &ok);
        break;
    case LOBO_REINICIAR_ESCENA:
        if (estado->reiniciar_escena)
            estado->reiniciar_escena(estado->ctx_eventos);
        lobo_vm_push(maquina, 0, &ok);
        break;
    case LOBO_CLONAR: {
        int nuevo = 0;
        if (estado->memoria) estado->memoria->clones++;
        maquina->clones++;
        if (maquina->entidad > 0 && estado->entidad_clonar)
            nuevo = estado->entidad_clonar(estado->ctx_entidades,
                                           maquina->entidad);
        lobo_vm_push(maquina, nuevo < 0 ? 0 : nuevo, &ok);
        break;
    }
    /* --- fase 26: mi y entidades --- */
    case LOBO_CARGAR_MI: {
        int campo = in->valor;
        int valor = 0;
        if (campo >= LOBO_MI_VARS) {
            int slot = campo - LOBO_MI_VARS;
            if (maquina->entidad > 0 && estado->entidad_leer)
                valor = estado->entidad_leer(estado->ctx_entidades,
                                             maquina->entidad, campo);
            else if (estado->memoria && slot >= 0 &&
                     slot < LOBO_ENTIDAD_VARS_MAX)
                valor = estado->memoria->entidad_vars[slot];
        } else if (maquina->entidad > 0 && estado->entidad_leer) {
            valor = estado->entidad_leer(estado->ctx_entidades,
                                         maquina->entidad, campo);
        } else {
            /* Sin entidad, `mi` cae en el sprite del juego. */
            switch (campo) {
            case LOBO_MI_X: valor = estado->x; break;
            case LOBO_MI_Y: valor = estado->y; break;
            case LOBO_MI_VISIBLE: valor = 1; break;
            default: valor = 0; break;
            }
        }
        lobo_vm_push(maquina, valor, &ok);
        break;
    }
    case LOBO_GUARDAR_MI: {
        int campo = in->valor;
        a = lobo_vm_pop(maquina, &ok);
        if (!ok) break;
        if (campo >= LOBO_MI_VARS) {
            int slot = campo - LOBO_MI_VARS;
            if (maquina->entidad > 0 && estado->entidad_escribir)
                estado->entidad_escribir(estado->ctx_entidades,
                                         maquina->entidad, campo, a);
            else if (estado->memoria && slot >= 0 &&
                     slot < LOBO_ENTIDAD_VARS_MAX)
                estado->memoria->entidad_vars[slot] = a;
        } else if (maquina->entidad > 0 && estado->entidad_escribir) {
            estado->entidad_escribir(estado->ctx_entidades, maquina->entidad,
                                     campo, a);
        } else {
            switch (campo) {
            case LOBO_MI_X: estado->x = a; break;
            case LOBO_MI_Y: estado->y = a; break;
            default: break; /* sin entidad no hay mas estado que mover */
            }
        }
        break;
    }
    case LOBO_CREAR: {
        int idx = in->valor;
        b = lobo_vm_pop(maquina, &ok); /* y */
        a = lobo_vm_pop(maquina, &ok); /* x */
        if (ok) {
            int id = -1;
            if (estado->entidad_crear && idx >= 0 &&
                idx < programa->num_textos)
                id = estado->entidad_crear(estado->ctx_entidades,
                                           programa->textos[idx], a, b);
            lobo_vm_push(maquina, id < 0 ? -1 : id, &ok);
        }
        break;
    }
    case LOBO_BORRAR_ENTIDAD: {
        int id = in->valor == 1 ? lobo_vm_pop(maquina, &ok)
                                : maquina->entidad;
        if (ok && id > 0 && estado->entidad_borrar)
            estado->entidad_borrar(estado->ctx_entidades, id);
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_MOVER_ENTIDAD: {
        int id, x, y;
        y = lobo_vm_pop(maquina, &ok);
        x = lobo_vm_pop(maquina, &ok);
        id = lobo_vm_pop(maquina, &ok);
        if (ok && id > 0 && estado->entidad_mover)
            estado->entidad_mover(estado->ctx_entidades, id, x, y);
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    /* --- fase 28: built-ins numericos y de texto --- */
    case LOBO_TEXTO:
        lobo_vm_push(maquina, lobo_texto_handle(in->valor), &ok);
        break;
    case LOBO_ABS:
        a = lobo_vm_pop(maquina, &ok);
        if (ok)
            lobo_vm_push(maquina,
                         a == LOBO_INT_MIN ? 0 : (a < 0 ? -a : a), &ok);
        break;
    case LOBO_MIN:
    case LOBO_MAX:
        b = lobo_vm_pop(maquina, &ok);
        a = lobo_vm_pop(maquina, &ok);
        if (ok)
            lobo_vm_push(maquina,
                         (in->op == LOBO_MIN) ? (a < b ? a : b)
                                              : (a > b ? a : b), &ok);
        break;
    case LOBO_LIMITAR: {
        int valor;
        b = lobo_vm_pop(maquina, &ok); /* hi */
        a = lobo_vm_pop(maquina, &ok); /* lo */
        valor = lobo_vm_pop(maquina, &ok);
        if (a > b) { int t = a; a = b; b = t; }
        if (ok)
            lobo_vm_push(maquina, valor < a ? a : (valor > b ? b : valor),
                         &ok);
        break;
    }
    case LOBO_REDONDEAR:
        a = lobo_vm_pop(maquina, &ok);
        if (ok) lobo_vm_push(maquina, a, &ok);
        break;
    case LOBO_ALEATORIO:
        b = lobo_vm_pop(maquina, &ok);
        a = lobo_vm_pop(maquina, &ok);
        if (ok) lobo_vm_push(maquina, lobo_aleatorio(maquina, a, b), &ok);
        break;
    case LOBO_LONGITUD: {
        const char *texto;
        a = lobo_vm_pop(maquina, &ok);
        texto = lobo_texto_resolver(programa, estado->memoria, a);
        if (ok) lobo_vm_push(maquina, texto ? lobo_texto_largo(texto) : 0,
                             &ok);
        break;
    }
    case LOBO_LETRA: {
        const char *texto;
        b = lobo_vm_pop(maquina, &ok); /* indice */
        a = lobo_vm_pop(maquina, &ok); /* texto */
        texto = lobo_texto_resolver(programa, estado->memoria, a);
        if (ok)
            lobo_vm_push(maquina,
                         (texto && b >= 0 && b < lobo_texto_largo(texto))
                             ? (unsigned char)texto[b]
                             : 0, &ok);
        break;
    }
    case LOBO_UNIR: {
        const char *ta, *tb;
        b = lobo_vm_pop(maquina, &ok);
        a = lobo_vm_pop(maquina, &ok);
        ta = lobo_texto_resolver(programa, estado->memoria, a);
        tb = lobo_texto_resolver(programa, estado->memoria, b);
        if (ok)
            lobo_vm_push(maquina,
                         lobo_texto_unir(estado->memoria, ta, tb), &ok);
        break;
    }
    /* --- fase 29: colisiones --- */
    case LOBO_TOCA:
        a = lobo_vm_pop(maquina, &ok);
        if (ok)
            lobo_vm_push(maquina,
                         (estado->toca && maquina->entidad > 0 &&
                          estado->toca(estado->ctx_colisiones,
                                       maquina->entidad, a))
                             ? 1 : 0, &ok);
        break;
    case LOBO_TOCA_TAG: {
        const char *tag = NULL;
        if (in->valor >= 0 && in->valor < programa->num_textos)
            tag = programa->textos[in->valor];
        if (ok)
            lobo_vm_push(maquina,
                         (tag && estado->toca_tag && maquina->entidad > 0 &&
                          estado->toca_tag(estado->ctx_colisiones,
                                           maquina->entidad, tag))
                             ? 1 : 0, &ok);
        break;
    }
    case LOBO_DISTANCIA:
        a = lobo_vm_pop(maquina, &ok);
        if (ok)
            lobo_vm_push(maquina,
                         (estado->distancia && maquina->entidad > 0)
                             ? estado->distancia(estado->ctx_colisiones,
                                                 maquina->entidad, a)
                             : -1, &ok);
        break;
    case LOBO_CAJA_DE:
        b = lobo_vm_pop(maquina, &ok); /* indice: 0=x 1=y 2=w 3=h */
        a = lobo_vm_pop(maquina, &ok); /* id */
        if (ok)
            lobo_vm_push(maquina,
                         (estado->caja_de && maquina->entidad > 0)
                             ? estado->caja_de(estado->ctx_colisiones,
                                               maquina->entidad, a, b)
                             : 0, &ok);
        break;
    /* --- fase 38: HUD, texto, UI y dialogos --- */
    case LOBO_HUD_VIDAS: {
        int anterior = 0;
        a = lobo_vm_pop(maquina, &ok);
        if (ok && estado->hud_vidas)
            anterior = estado->hud_vidas(estado->ctx_hud, a);
        if (ok) lobo_vm_push(maquina, anterior, &ok);
        break;
    }
    case LOBO_HUD_PUNTOS: {
        int anterior = 0;
        a = lobo_vm_pop(maquina, &ok);
        if (ok && estado->hud_puntos)
            anterior = estado->hud_puntos(estado->ctx_hud, a);
        if (ok) lobo_vm_push(maquina, anterior, &ok);
        break;
    }
    case LOBO_HUD_DIALOGO: {
        const char *texto;
        a = lobo_vm_pop(maquina, &ok); /* texto */
        b = lobo_vm_pop(maquina, &ok); /* linea */
        texto = lobo_texto_resolver(programa, estado->memoria, a);
        if (ok && estado->hud_dialogo && texto)
            estado->hud_dialogo(estado->ctx_hud, b, texto);
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_HUD_TEXTO: {
        const char *texto;
        int e, f, h;
        e = lobo_vm_pop(maquina, &ok); /* escala */
        f = lobo_vm_pop(maquina, &ok); /* color */
        h = lobo_vm_pop(maquina, &ok); /* texto */
        b = lobo_vm_pop(maquina, &ok); /* y */
        a = lobo_vm_pop(maquina, &ok); /* x */
        texto = lobo_texto_resolver(programa, estado->memoria, h);
        if (ok && estado->hud_texto && texto)
            estado->hud_texto(estado->ctx_hud, a, b, texto,
                              (unsigned)f, e);
        if (ok) lobo_vm_push(maquina, 1, &ok);
        break;
    }
    case LOBO_HUD_RECT: {
        int c, d, e;
        e = lobo_vm_pop(maquina, &ok); /* color */
        d = lobo_vm_pop(maquina, &ok); /* h */
        c = lobo_vm_pop(maquina, &ok); /* w */
        b = lobo_vm_pop(maquina, &ok); /* y */
        a = lobo_vm_pop(maquina, &ok); /* x */
        if (ok && estado->hud_rect)
            estado->hud_rect(estado->ctx_hud, a, b, c, d, (unsigned)e);
        if (ok) lobo_vm_push(maquina, 1, &ok);
        break;
    }
    case LOBO_HUD_BARRA: {
        int c, d, e, f, g;
        g = lobo_vm_pop(maquina, &ok); /* color */
        f = lobo_vm_pop(maquina, &ok); /* max */
        e = lobo_vm_pop(maquina, &ok); /* valor */
        d = lobo_vm_pop(maquina, &ok); /* h */
        c = lobo_vm_pop(maquina, &ok); /* w */
        b = lobo_vm_pop(maquina, &ok); /* y */
        a = lobo_vm_pop(maquina, &ok); /* x */
        if (ok && estado->hud_barra)
            estado->hud_barra(estado->ctx_hud, a, b, c, d, e, f,
                              (unsigned)g);
        if (ok) lobo_vm_push(maquina, 1, &ok);
        break;
    }
    case LOBO_HUD_IMAGEN:
        b = lobo_vm_pop(maquina, &ok); /* y */
        a = lobo_vm_pop(maquina, &ok); /* x */
        {
            int id = lobo_vm_pop(maquina, &ok);
            if (ok && estado->hud_imagen)
                estado->hud_imagen(estado->ctx_hud, id, a, b);
        }
        if (ok) lobo_vm_push(maquina, 1, &ok);
        break;
    case LOBO_HUD_BOTON: {
        const char *texto;
        int c, d, e, f;
        a = lobo_vm_pop(maquina, &ok); /* etiqueta */
        f = lobo_vm_pop(maquina, &ok); /* h */
        e = lobo_vm_pop(maquina, &ok); /* w */
        d = lobo_vm_pop(maquina, &ok); /* y */
        c = lobo_vm_pop(maquina, &ok); /* x */
        {
            int id = lobo_vm_pop(maquina, &ok);
            b = id;
            texto = lobo_texto_resolver(programa, estado->memoria, a);
            if (ok && estado->hud_boton && texto) {
                int pulsado = estado->hud_boton(estado->ctx_hud, b, c, d,
                                                e, f, texto);
                lobo_vm_push(maquina, pulsado, &ok);
                break;
            }
        }
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_HUD_PLANTILLA:
        a = lobo_vm_pop(maquina, &ok);
        if (ok && estado->hud_plantilla)
            estado->hud_plantilla(estado->ctx_hud, a);
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    /* --- fase 39: sonido y musica simple --- */
    case LOBO_SONAR:
    case LOBO_MUSICA: {
        const char *nombre;
        int resultado = 0;
        a = lobo_vm_pop(maquina, &ok);
        nombre = lobo_texto_resolver(programa, estado->memoria, a);
        if (ok && nombre) {
            if (in->op == LOBO_SONAR) {
                if (estado->sonar)
                    resultado = estado->sonar(estado->ctx_audio, nombre);
            } else if (estado->musica) {
                resultado = estado->musica(estado->ctx_audio, nombre);
            }
        }
        if (ok) lobo_vm_push(maquina, resultado, &ok);
        break;
    }
    case LOBO_DETENER_SONIDO:
        if (ok && estado->detener_sonido)
            estado->detener_sonido(estado->ctx_audio);
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    case LOBO_VOLUMEN: {
        int anterior = 80;
        a = lobo_vm_pop(maquina, &ok);
        if (ok && estado->volumen)
            anterior = estado->volumen(estado->ctx_audio, a);
        if (ok) lobo_vm_push(maquina, anterior, &ok);
        break;
    }
    case LOBO_VOLUMEN_ACTUAL: {
        int actual = 80;
        if (ok && estado->volumen_actual)
            actual = estado->volumen_actual(estado->ctx_audio);
        if (ok) lobo_vm_push(maquina, actual, &ok);
        break;
    }
    /* --- fase 40: guardado, records y ajustes ---
     * Los textos entre comillas llegan como handles y se resuelven al
     * nombre de la clave. Sin callbacks no se toca ningun archivo. */
    case LOBO_GUARDAR:
    case LOBO_RECORD:
    case LOBO_AJUSTE: {
        const char *clave;
        int valor = 0;
        int resultado = 0;
        a = lobo_vm_pop(maquina, &ok); /* valor (el ultimo argumento) */
        b = lobo_vm_pop(maquina, &ok); /* texto de la clave */
        valor = a;
        clave = lobo_texto_resolver(programa, estado->memoria, b);
        if (ok && clave) {
            switch (in->op) {
            case LOBO_GUARDAR:
                resultado = estado->guardar ?
                    estado->guardar(estado->ctx_save, clave, valor) : 1;
                break;
            case LOBO_RECORD:
                resultado = estado->record ?
                    estado->record(estado->ctx_save, clave, valor) : valor;
                break;
            default:
                resultado = estado->ajuste ?
                    estado->ajuste(estado->ctx_save, clave, valor) : valor;
                break;
            }
        }
        if (ok) lobo_vm_push(maquina, resultado, &ok);
        break;
    }
    case LOBO_CARGAR:
    case LOBO_RECORD_ACTUAL:
    case LOBO_AJUSTE_ACTUAL: {
        const char *clave;
        int resultado = 0;
        a = lobo_vm_pop(maquina, &ok);
        clave = lobo_texto_resolver(programa, estado->memoria, a);
        if (ok && clave) {
            switch (in->op) {
            case LOBO_CARGAR:
                if (estado->cargar)
                    resultado = estado->cargar(estado->ctx_save, clave);
                break;
            case LOBO_RECORD_ACTUAL:
                if (estado->record_actual)
                    resultado = estado->record_actual(estado->ctx_save, clave);
                break;
            default:
                if (estado->ajuste_actual)
                    resultado = estado->ajuste_actual(estado->ctx_save, clave);
                break;
            }
        }
        if (ok) lobo_vm_push(maquina, resultado, &ok);
        break;
    }
    case LOBO_BORRAR_SAVE: {
        int resultado = estado->borrar_save ?
            estado->borrar_save(estado->ctx_save) : 1;
        if (ok) lobo_vm_push(maquina, resultado, &ok);
        break;
    }
    /* --- fase 41: escenas, transiciones, camara y efectos ---
     * Sin callbacks el valor es neutro, pero la pila sigue cuadrando. */
    case LOBO_CAMBIAR_ESCENA: {
        int resultado = 0;
        a = lobo_vm_pop(maquina, &ok);
        if (ok && estado->cambiar_escena)
            resultado = estado->cambiar_escena(estado->ctx_escenas, a);
        if (ok) lobo_vm_push(maquina, resultado, &ok);
        break;
    }
    case LOBO_ESCENA_ACTUAL: {
        int actual = 0;
        if (estado->escena_actual)
            actual = estado->escena_actual(estado->ctx_escenas);
        if (ok) lobo_vm_push(maquina, actual, &ok);
        break;
    }
    case LOBO_TRANSICION: {
        int fotogramas, tipo;
        fotogramas = lobo_vm_pop(maquina, &ok);
        tipo = lobo_vm_pop(maquina, &ok);
        if (ok && estado->efecto)
            estado->efecto(estado->ctx_escenas, 0, tipo, fotogramas, 0, 0);
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_CAMARA_OBJETIVO: {
        int anterior = 0;
        a = lobo_vm_pop(maquina, &ok);
        if (ok && estado->camara)
            anterior = estado->camara(estado->ctx_escenas, 0, a, 0, 0, 0);
        if (ok) lobo_vm_push(maquina, anterior, &ok);
        break;
    }
    case LOBO_CAMARA_ZOOM: {
        int anterior = 1;
        a = lobo_vm_pop(maquina, &ok);
        if (ok && estado->camara)
            anterior = estado->camara(estado->ctx_escenas, 1, a, 0, 0, 0);
        if (ok) lobo_vm_push(maquina, anterior, &ok);
        break;
    }
    case LOBO_CAMARA_LIMITES: {
        int alto, ancho, y, x;
        alto = lobo_vm_pop(maquina, &ok);
        ancho = lobo_vm_pop(maquina, &ok);
        y = lobo_vm_pop(maquina, &ok);
        x = lobo_vm_pop(maquina, &ok);
        if (ok && estado->camara)
            estado->camara(estado->ctx_escenas, 2, x, y, ancho, alto);
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_CAMARA_SUAVIZAR: {
        int anterior = 0;
        a = lobo_vm_pop(maquina, &ok);
        if (ok && estado->camara)
            anterior = estado->camara(estado->ctx_escenas, 3, a, 0, 0, 0);
        if (ok) lobo_vm_push(maquina, anterior, &ok);
        break;
    }
    case LOBO_PARPADEO: {
        int periodo, id;
        periodo = lobo_vm_pop(maquina, &ok);
        id = lobo_vm_pop(maquina, &ok);
        if (ok && estado->efecto)
            estado->efecto(estado->ctx_escenas, 1, id, periodo, 0, 0);
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_TEMBLOR: {
        int fotogramas, intensidad;
        fotogramas = lobo_vm_pop(maquina, &ok);
        intensidad = lobo_vm_pop(maquina, &ok);
        if (ok && estado->efecto)
            estado->efecto(estado->ctx_escenas, 2, intensidad, fotogramas,
                           0, 0);
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_FLASH: {
        int fotogramas, color;
        fotogramas = lobo_vm_pop(maquina, &ok);
        color = lobo_vm_pop(maquina, &ok);
        if (ok && estado->efecto)
            estado->efecto(estado->ctx_escenas, 3, color, fotogramas, 0, 0);
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    case LOBO_FADE: {
        int fotogramas, alpha, color;
        fotogramas = lobo_vm_pop(maquina, &ok);
        alpha = lobo_vm_pop(maquina, &ok);
        color = lobo_vm_pop(maquina, &ok);
        if (ok && estado->efecto)
            estado->efecto(estado->ctx_escenas, 4, color, alpha, fotogramas, 0);
        if (ok) lobo_vm_push(maquina, 0, &ok);
        break;
    }
    default:
        ok = 0;
        break;
    }

    if (!ok) {
        /* Error terminal: la maquina queda limpia para el proximo evento. */
        maquina->agotado = 1;
        maquina->np = 0;
        maquina->num_frames = 0;
        maquina->estado = LOBO_VM_TERMINADO;
        if (g_hook_instruccion) g_hook_instruccion(maquina->pc - 1, 1);
        return LOBO_PASO_ERROR;
    }
    return LOBO_PASO_SIGUE;
}

int lobo_vm_fotograma(const LoboPrograma *programa, LoboEstado *estado,
                      LoboMaquina *maquina) {
    long pasos = LOBO_PASOS_VM_MAX;

    if (!programa || !maquina) return 0;
    maquina->cerrar = 0;
    maquina->agotado = 0;

    /* El evento de fotograma vuelve a disparar un script terminado. */
    if (maquina->estado == LOBO_VM_TERMINADO) {
        maquina->pc = 0;
        maquina->np = 0;
        maquina->num_frames = 0;
        maquina->espera = 0;
        maquina->estado = LOBO_VM_LISTO;
    } else if (maquina->estado == LOBO_VM_ESPERANDO) {
        if (maquina->espera > 0) maquina->espera--;
        if (maquina->espera > 0) return 0;
        maquina->estado = LOBO_VM_LISTO;
    }

    while (--pasos >= 0) {
        int r = lobo_paso(programa, estado, maquina);
        if (r != LOBO_PASO_SIGUE) return 1;
        if (g_hook_debe_parar && g_hook_debe_parar()) return 1;
    }
    maquina->agotado = 1;
    return 0;
}

/* Corre hasta un evento. Devuelve 1 si llego, 0 si agoto el presupuesto. */
static int lobo_vm_correr(const LoboPrograma *programa, LoboEstado *estado,
                          LoboMaquina *maquina) {
    long pasos = LOBO_PASOS_VM_MAX;
    while (--pasos >= 0) {
        int r = lobo_paso(programa, estado, maquina);
        if (r != LOBO_PASO_SIGUE) return 1;
        if (g_hook_debe_parar && g_hook_debe_parar()) return 1;
    }
    maquina->agotado = 1;
    return 0;
}

int lobo_vm_evento(const LoboPrograma *programa, LoboEstado *estado,
                   LoboMaquina *maquina) {
    if (!programa || !maquina) return 0;
    maquina->cerrar = 0;
    maquina->agotado = 0;
    if (maquina->estado == LOBO_VM_ESPERANDO) {
        /* El disparo llega mientras el script espera: el planificador
         * decide si lo pospone (mensajes) o lo pierde (teclas). */
        return 0;
    }
    if (maquina->estado == LOBO_VM_TERMINADO) {
        maquina->pc = 0;
        maquina->np = 0;
        maquina->num_frames = 0;
        maquina->espera = 0;
        maquina->estado = LOBO_VM_LISTO;
    }
    return lobo_vm_correr(programa, estado, maquina);
}

int lobo_vm_reanudar(const LoboPrograma *programa, LoboEstado *estado,
                     LoboMaquina *maquina) {
    if (!programa || !maquina) return 0;
    if (maquina->estado != LOBO_VM_ESPERANDO) return 0;
    maquina->cerrar = 0;
    maquina->agotado = 0;
    if (maquina->espera > 0) maquina->espera--;
    if (maquina->espera > 0) return 0;
    maquina->estado = LOBO_VM_LISTO;
    return lobo_vm_correr(programa, estado, maquina);
}

void lobo_maquina_reiniciar(LoboMaquina *maquina) {
    int i;
    for (i = 0; i < LOBO_LOCALES_MAX; i++) maquina->locales[i] = 0;
    maquina->cerrar = 0;
    maquina->agotado = 0;
    maquina->pc = 0;
    maquina->np = 0;
    maquina->num_frames = 0;
    maquina->espera = 0;
    maquina->estado = LOBO_VM_LISTO;
    maquina->detener_todo = 0;
    maquina->clones = 0;
    maquina->entidad = 0;
    maquina->semilla = 12345; /* aleatorio() reproducible */
}
