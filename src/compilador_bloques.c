/* ==========================================================================
 * Compilador bloques -> lobo 3 (fase 33).
 *
 * Recorre el modelo del lienzo y escribe un script lobo por evento. El codigo
 * se valida con el compilador de la VM, asi que el motor lo ejecuta sin haber
 * tocado ni el formato LOGIC3 ni su CRC. Cada linea generada recuerda el id
 * del bloque que la origino para poder resaltarlo cuando hay un error.
 * ========================================================================== */
#include "app.h"

/* Indices del catalogo compartidos con bloques.c. */
enum {
    DEF_INICIO = 0,
    DEF_TECLA,
    DEF_RECIBIR,
    DEF_REPETIR,
    DEF_SI,
    DEF_MOVER_X,
    DEF_VARIABLE,
    DEF_SUMA,
    DEF_MAYOR,
    DEF_CODIGO,
    DEF_HUD_VIDAS,
    DEF_HUD_PUNTOS,
    DEF_HUD_DIALOGO,
    DEF_HUD_TEXTO,
    DEF_HUD_PLANTILLA
};

static char codigo[COMPILADOR_CODIGO_MAX];
static int longitud;
static int linea_bloque[COMPILADOR_LINEAS_MAX];
static int num_lineas;
static char ultimo_error[LOBO_ERROR_MAX];
static int bloque_error;
static int linea_error;

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */
static void copiar_texto(char *dst, int tam, const char *src) {
    int n = 0;
    if (!src) src = "";
    while (src[n] && n < tam - 1) {
        dst[n] = src[n];
        n++;
    }
    dst[n] = 0;
}

static void copiar_escapado(char *dst, int tam, const char *src) {
    int n = 0;
    if (!src) src = "";
    while (src[n] && n < tam - 3) {
        if (src[n] == '"' || src[n] == '\\') {
            dst[n++] = '\\';
            if (n >= tam - 2) break;
        }
        dst[n] = src[n];
        n++;
    }
    dst[n] = 0;
}

static int campo_entero(const char *texto) {
    char tmp[32];
    copiar_texto(tmp, sizeof(tmp), texto);
    return atoi(tmp);
}

static int hijo_en_slot(int padre, const char *slot) {
    int i, id;
    for (i = 0; i < bloques_cantidad(); i++) {
        id = bloques_nodo_id(i);
        if (bloques_nodo_padre(id) == padre &&
            strcmp(bloques_nodo_slot(id), slot) == 0) return id;
    }
    return 0;
}

static void anadir_texto(const char *texto, int id) {
    int n = (int)strlen(texto);
    if (longitud + n + 2 >= COMPILADOR_CODIGO_MAX) {
        if (!ultimo_error[0])
            copiar_texto(ultimo_error, sizeof(ultimo_error),
                         "linea 1: codigo generado demasiado largo");
        return;
    }
    memcpy(codigo + longitud, texto, (size_t)n);
    longitud += n;
    codigo[longitud++] = '\n';
    codigo[longitud] = 0;
    if (num_lineas < COMPILADOR_LINEAS_MAX)
        linea_bloque[num_lineas++] = id;
    else if (!ultimo_error[0])
        copiar_texto(ultimo_error, sizeof(ultimo_error),
                     "linea 1: demasiadas lineas generadas");
}

static void anadir_linea(int id, const char *formato, ...) {
    char linea[512];
    va_list args;
    va_start(args, formato);
    vsnprintf(linea, sizeof(linea), formato, args);
    va_end(args);
    anadir_texto(linea, id);
}

static void anadir_bloque_codigo(int id) {
    const char *texto = bloques_codigo_obtener(id);
    char linea[512];
    const char *p = texto;
    if (!p || !p[0]) {
        p = bloques_nodo_campo(id, 0);
        if (!p || !p[0]) p = "";
    }
    while (*p) {
        const char *fin = strchr(p, '\n');
        int n = fin ? (int)(fin - p) : (int)strlen(p);
        if (n > (int)sizeof(linea) - 1) n = (int)sizeof(linea) - 1;
        memcpy(linea, p, (size_t)n);
        linea[n] = 0;
        anadir_texto(linea, id);
        if (!fin) break;
        p = fin + 1;
    }
}

/* ------------------------------------------------------------------ */
/* Expresiones y sentencias                                            */
/* ------------------------------------------------------------------ */
static void generar_expresion(int id, const char *slot, int campo,
                              char *dst, int tam) {
    int hijo = hijo_en_slot(id, slot);
    if (hijo > 0) {
        int hdef = bloques_nodo_def(hijo);
        char a[110], b[110];
        if (hdef == DEF_SUMA) {
            generar_expresion(hijo, "", 0, a, sizeof(a));
            generar_expresion(hijo, "", 1, b, sizeof(b));
            snprintf(dst, (size_t)tam, "(%s + %s)", a, b);
            return;
        }
        if (hdef == DEF_MAYOR) {
            generar_expresion(hijo, "", 0, a, sizeof(a));
            generar_expresion(hijo, "", 1, b, sizeof(b));
            snprintf(dst, (size_t)tam, "(%s > %s)", a, b);
            return;
        }
        dst[0] = 0;
        return;
    }
    copiar_texto(dst, tam, bloques_nodo_campo(id, campo));
    if (!dst[0]) copiar_texto(dst, tam, "0");
}

static void generar_sentencia(int id, int profundo);

static void generar_hijos(int id, int profundo) {
    int i, hijo;
    if (profundo > 16) return;
    for (i = 0; i < bloques_cantidad(); i++) {
        hijo = bloques_nodo_id(i);
        if (bloques_nodo_padre(hijo) != id) continue;
        if (strcmp(bloques_nodo_slot(hijo), "cuerpo") != 0) continue;
        generar_sentencia(hijo, profundo + 1);
    }
}

void generar_sentencia(int id, int profundo) {
    int def = bloques_nodo_def(id);
    char expr[280], var[48];

    if (def == DEF_INICIO || def == DEF_TECLA || def == DEF_RECIBIR) return;
    if (profundo > 16) return;

    if (def == DEF_REPETIR) {
        anadir_linea(id, "mientras 1");
        generar_hijos(id, profundo);
        anadir_linea(id, "fin");
        return;
    }
    if (def == DEF_SI) {
        generar_expresion(id, "condicion", 0, expr, sizeof(expr));
        if (!expr[0]) copiar_texto(expr, sizeof(expr), "1");
        anadir_linea(id, "si %s", expr);
        generar_hijos(id, profundo);
        anadir_linea(id, "fin");
        return;
    }
    if (def == DEF_MOVER_X) {
        int dx = campo_entero(bloques_nodo_campo(id, 0));
        generar_expresion(id, "dx", 0, expr, sizeof(expr));
        if (!expr[0]) expr[0] = 0;
        if (hijo_en_slot(id, "dx") > 0)
            anadir_linea(id, "x = x + %s", expr);
        else
            anadir_linea(id, "x = x + %d", dx);
        return;
    }
    if (def == DEF_VARIABLE) {
        copiar_escapado(var, sizeof(var), bloques_nodo_campo(id, 0));
        if (!var[0]) copiar_texto(var, sizeof(var), "variable");
        generar_expresion(id, "delta", 1, expr, sizeof(expr));
        if (hijo_en_slot(id, "delta") > 0)
            anadir_linea(id, "mi.variable(\"%s\") = mi.variable(\"%s\") + %s",
                         var, var, expr);
        else
            anadir_linea(id, "mi.variable(\"%s\") = mi.variable(\"%s\") + %d",
                         var, var, campo_entero(bloques_nodo_campo(id, 1)));
        return;
    }
    if (def == DEF_SUMA || def == DEF_MAYOR) {
        generar_expresion(id, "", 0, expr, sizeof(expr));
        anadir_texto(expr, id);
        return;
    }
    if (def == DEF_CODIGO) {
        anadir_bloque_codigo(id);
        return;
    }
    /* Fase 38: el HUD se genera con las mismas llamadas que acepta la VM. */
    if (def == DEF_HUD_VIDAS) {
        anadir_linea(id, "hud_vidas(%d)",
                     campo_entero(bloques_nodo_campo(id, 0)));
        return;
    }
    if (def == DEF_HUD_PUNTOS) {
        anadir_linea(id, "hud_puntos(%d)",
                     campo_entero(bloques_nodo_campo(id, 0)));
        return;
    }
    if (def == DEF_HUD_DIALOGO) {
        char texto[96];
        copiar_escapado(texto, sizeof(texto), bloques_nodo_campo(id, 1));
        anadir_linea(id, "hud_dialogo(%d, \"%s\")",
                     campo_entero(bloques_nodo_campo(id, 0)), texto);
        return;
    }
    if (def == DEF_HUD_TEXTO) {
        char texto[96];
        copiar_escapado(texto, sizeof(texto), bloques_nodo_campo(id, 0));
        anadir_linea(id, "hud_texto(8, %d, \"%s\", 16777215, 1)",
                     campo_entero(bloques_nodo_campo(id, 1)), texto);
        return;
    }
    if (def == DEF_HUD_PLANTILLA) {
        anadir_linea(id, "hud_plantilla(%d)",
                     campo_entero(bloques_nodo_campo(id, 0)));
        return;
    }
    /* Definicion desconocida: se deja una linea que el validador marcara. */
    anadir_linea(id, "bloque_desconocido %d", id);
}

/* ------------------------------------------------------------------ */
/* API publica                                                         */
/* ------------------------------------------------------------------ */
const char *compilador_bloques_codigo(void) { return codigo; }
const char *compilador_bloques_error(void) { return ultimo_error; }
int compilador_bloques_bloque_error(void) { return bloque_error; }
int compilador_bloques_linea_error(void) { return linea_error; }
int compilador_bloques_lineas(void) { return num_lineas; }

void compilador_bloques_generar(void) {
    LoboPrograma programa;
    int i, id, ok;
    const char *p;

    codigo[0] = 0;
    longitud = 0;
    num_lineas = 0;
    ultimo_error[0] = 0;
    bloque_error = 0;
    linea_error = 0;
    g.bloques_error[0] = 0;
    g.bloques_error_bloque = 0;
    g.bloques_error_linea = 0;

    /* Las raices se recorren en el orden visual; el sombrero solo indica el
     * evento, que ya vive en el proyecto LOGIC3. */
    for (i = 0; i < bloques_cantidad(); i++) {
        id = bloques_nodo_id(i);
        /* Los reporteros se resuelven dentro de la sentencia que los usa;
         * si cuelgan solos no producen codigo por si mismos. */
        if (bloques_nodo_padre(id) == 0 &&
            bloques_nodo_def(id) != DEF_SUMA &&
            bloques_nodo_def(id) != DEF_MAYOR)
            generar_sentencia(id, 0);
    }
    codigo[longitud] = 0;

    lobo_programa_iniciar(&programa);
    ok = lobo_compilar(codigo, &programa, LOBO_MODO_VM,
                       ultimo_error, sizeof(ultimo_error));
    lobo_programa_liberar(&programa);
    if (ok) {
        ultimo_error[0] = 0;
        return;
    }

    p = strstr(ultimo_error, "linea ");
    if (p) {
        int n = atoi(p + 6);
        if (n > 0 && n <= num_lineas) {
            bloque_error = linea_bloque[n - 1];
            linea_error = n;
        }
    }
    if (!bloque_error && bloques_cantidad() > 0)
        bloque_error = bloques_nodo_id(0);
    copiar_texto(g.bloques_error, sizeof(g.bloques_error), ultimo_error);
    g.bloques_error_bloque = bloque_error;
    g.bloques_error_linea = linea_error;
}

int compilador_bloques_convertir(void) {
    int id;
    compilador_bloques_generar();
    if (ultimo_error[0]) return 0;
    bloques_reiniciar();
    id = bloques_crear(DEF_CODIGO, 220, 160);
    if (id <= 0) return 0;
    bloques_codigo_poner(id, codigo);
    g.bloques_convertido = 1;
    g.bloques_ver_codigo = 1;
    return 1;
}
