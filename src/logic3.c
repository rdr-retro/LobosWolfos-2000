/* logic3.c - lector, escritor, validacion y migracion de LOGIC3 (fase 24).
 *
 * El formato y el modelo estan descritos en logic3.h. El lector trabaja
 * sobre el archivo completo en memoria para poder comprobar el CRC32 y
 * para no depender de lineas de tamano fijo. Los errores siempre llevan
 * el numero de linea: "linea N: motivo".
 *
 * Esta fase no cambia el runtime: el motor sigue leyendo LOGIC2. Al
 * abrir un LOGIC3 se vuelca una vista clasica en `g.modulos` y
 * `g.funciones` (mapas por escena y bloques `codigo.lobo`), y al
 * guardar se escribe el modelo LOGIC3 guardado, no esa vista.
 */
#include "logic3.h"

#define L3_ARCHIVO_MAX (16 * 1024 * 1024)
#define L3_TOKENS_MAX  288 /* 4 de cabecera + 256 elementos de lista */

/* Contexto del lector: donde caen los registros indentados. */
enum {
    CTX_NINGUNO = 0,
    CTX_OBJETO,
    CTX_FUNCION
};

typedef struct {
    char *tok[L3_TOKENS_MAX];
    int n;
} Linea3;

typedef struct {
    char *datos;
    int largo;
    int cap;
    int error;
} Buffer3;

typedef struct {
    int id;
    int linea;
} IdLinea3;

/* Modelo LOGIC3 activo del editor. La vista clasica vive en `g`. */
static Proyecto3 g3;
static int g3_activo = 0;
static char g3_error[L3_ERROR_MAX];

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */
static void copiar_texto(char *dst, int tam, const char *src) {
    int n;
    if (!dst || tam <= 0) return;
    if (!src) src = "";
    n = (int)strlen(src);
    if (n >= tam) n = tam - 1;
    if (n > 0) memcpy(dst, src, (size_t)n);
    dst[n] = 0;
}

static int comparar_entero(const void *a, const void *b) {
    int va = *(const int *)a;
    int vb = *(const int *)b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

static int comparar_id_linea(const void *a, const void *b) {
    const IdLinea3 *va = (const IdLinea3 *)a;
    const IdLinea3 *vb = (const IdLinea3 *)b;
    if (va->id < vb->id) return -1;
    if (va->id > vb->id) return 1;
    return 0;
}

static int entero_en(const int *lista, int n, int valor) {
    int ini = 0, fin = n - 1;
    while (ini <= fin) {
        int medio = (ini + fin) / 2;
        if (lista[medio] == valor) return 1;
        if (lista[medio] < valor) ini = medio + 1;
        else fin = medio - 1;
    }
    return 0;
}

/* Escribe "linea N: motivo" (o solo el motivo si linea <= 0). */
static int fallo3(char *error, int tam, int linea, const char *formato, ...) {
    char motivo[L3_ERROR_MAX];
    va_list ap;
    if (!error || tam <= 0) return 0;
    motivo[0] = 0;
    va_start(ap, formato);
    vsnprintf(motivo, sizeof(motivo), formato, ap);
    va_end(ap);
    if (linea > 0) snprintf(error, (size_t)tam, "linea %d: %s", linea, motivo);
    else snprintf(error, (size_t)tam, "%s", motivo);
    return 0;
}

static int a_entero(const char *texto, int *valor) {
    char *fin = NULL;
    long n;
    if (!texto || !texto[0] || !valor) return 0;
    n = strtol(texto, &fin, 10);
    if (fin == texto || *fin != 0) return 0;
    *valor = (int)n;
    return 1;
}

static int a_u32(const char *texto, unsigned int *valor) {
    char *fin = NULL;
    unsigned long n;
    if (!texto || !texto[0] || !valor) return 0;
    n = strtoul(texto, &fin, 16);
    if (fin == texto || *fin != 0) return 0;
    *valor = (unsigned int)n;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Modelo                                                              */
/* ------------------------------------------------------------------ */
void logic3_iniciar(Proyecto3 *p) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->version = L3_VERSION;
    copiar_texto(p->nombre, sizeof(p->nombre), "Proyecto sin titulo");
    p->ancho = 640;
    p->alto = 480;
    p->limitar_cuadros = 1;
    p->fps = 60;
    p->velocidad = 3;
    p->fondo = 0x001E5A2Eu;
    p->camara.zoom = 1;
    copiar_texto(p->carpetas[0], L3_RUTA_MAX, "tiles");
    copiar_texto(p->carpetas[1], L3_RUTA_MAX, "maps");
    copiar_texto(p->carpetas[2], L3_RUTA_MAX, "trajes");
    copiar_texto(p->carpetas[3], L3_RUTA_MAX, "sonidos");
}

void logic3_liberar(Proyecto3 *p) {
    int i;
    if (!p) return;
    for (i = 0; i < p->num_listas; i++) free(p->listas[i].elementos);
    free(p->objetos);
    free(p->bloques);
    for (i = 0; i < L3_LISTAS_MAX; i++) p->listas[i].elementos = NULL;
    p->objetos = NULL;
    p->bloques = NULL;
    p->num_objetos = 0;
    p->cap_objetos = 0;
    p->num_bloques = 0;
    p->cap_bloques = 0;
}

/* Reserva el array de elementos de una lista la primera vez que aparece
 * un valor. El tope es fijo para que la serializacion sea determinista. */
static int lista_elementos_reservar(Lista3 *li) {
    if (!li) return 0;
    if (li->elementos) return 1;
    li->elementos = (char (*)[L3_TEXTO_MAX])
        calloc(L3_LISTA_ELEMENTOS_MAX, L3_TEXTO_MAX);
    return li->elementos != NULL;
}

static int objetos_reservar(Proyecto3 *p) {
    if (p->objetos) return 1;
    p->objetos = (Objeto3 *)calloc(L3_OBJETOS_MAX, sizeof(Objeto3));
    if (!p->objetos) return 0;
    p->cap_objetos = L3_OBJETOS_MAX;
    return 1;
}

static int bloques_reservar(Proyecto3 *p, int extra) {
    int necesario, cap;
    Bloque3 *nuevo;
    if (extra < 0 || p->num_bloques > L3_BLOQUES_MAX - extra) return 0;
    necesario = p->num_bloques + extra;
    if (necesario <= p->cap_bloques) return 1;
    cap = p->cap_bloques > 0 ? p->cap_bloques : 64;
    while (cap < necesario) {
        if (cap > L3_BLOQUES_MAX / 2) { cap = L3_BLOQUES_MAX; break; }
        cap *= 2;
    }
    if (cap < necesario) cap = necesario;
    nuevo = (Bloque3 *)realloc(p->bloques, sizeof(Bloque3) * (size_t)cap);
    if (!nuevo) return 0;
    p->bloques = nuevo;
    p->cap_bloques = cap;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Nombres del formato                                                 */
/* ------------------------------------------------------------------ */
const char *logic3_tipo_texto(int tipo) {
    switch (tipo) {
        case L3_TIPO_BOOLEANO: return "booleano";
        case L3_TIPO_TEXTO:    return "texto";
        default:               return "entero";
    }
}

int logic3_tipo_desde_texto(const char *texto) {
    if (!texto) return -1;
    if (strcmp(texto, "entero") == 0 || strcmp(texto, "0") == 0)
        return L3_TIPO_ENTERO;
    if (strcmp(texto, "booleano") == 0 || strcmp(texto, "1") == 0)
        return L3_TIPO_BOOLEANO;
    if (strcmp(texto, "texto") == 0 || strcmp(texto, "2") == 0)
        return L3_TIPO_TEXTO;
    return -1;
}

const char *logic3_evento_texto(int tipo) {
    switch (tipo) {
        case L3_EVENTO_INICIO:    return "inicio";
        case L3_EVENTO_FOTOGRAMA: return "fotograma";
        case L3_EVENTO_TECLA:     return "tecla";
        case L3_EVENTO_RECIBIR:   return "recibir";
        case L3_EVENTO_TOCAR:     return "tocar";
        case L3_EVENTO_CADA:      return "cada";
        case L3_EVENTO_CLONAR:    return "clonar";
        case L3_EVENTO_ESCENA:    return "escena";
        case L3_EVENTO_LLAMADA:   return "llamada";
        default:                  return "desconocido";
    }
}

int logic3_evento_desde_texto(const char *texto) {
    if (!texto) return 0;
    if (strcmp(texto, "inicio") == 0) return L3_EVENTO_INICIO;
    if (strcmp(texto, "fotograma") == 0) return L3_EVENTO_FOTOGRAMA;
    if (strcmp(texto, "tecla") == 0) return L3_EVENTO_TECLA;
    if (strcmp(texto, "recibir") == 0) return L3_EVENTO_RECIBIR;
    if (strcmp(texto, "tocar") == 0) return L3_EVENTO_TOCAR;
    if (strcmp(texto, "cada") == 0) return L3_EVENTO_CADA;
    if (strcmp(texto, "clonar") == 0) return L3_EVENTO_CLONAR;
    if (strcmp(texto, "escena") == 0) return L3_EVENTO_ESCENA;
    if (strcmp(texto, "llamada") == 0) return L3_EVENTO_LLAMADA;
    return 0;
}

const char *logic3_flanco_texto(int flanco) {
    switch (flanco) {
        case L3_FLANCO_PULSAR: return "pulsar";
        case L3_FLANCO_SOLTAR: return "soltar";
        default:               return "mantener";
    }
}

static int flanco_desde_texto(const char *texto) {
    int n;
    if (!texto) return -1;
    if (strcmp(texto, "mantener") == 0) return L3_FLANCO_MANTENER;
    if (strcmp(texto, "pulsar") == 0) return L3_FLANCO_PULSAR;
    if (strcmp(texto, "soltar") == 0) return L3_FLANCO_SOLTAR;
    if (a_entero(texto, &n) && n >= 0 && n <= 2) return n;
    return -1;
}

/* ------------------------------------------------------------------ */
/* CRC32                                                               */
/* ------------------------------------------------------------------ */
unsigned int logic3_crc32(const void *datos, int largo) {
    const unsigned char *p = (const unsigned char *)datos;
    unsigned int crc = 0xFFFFFFFFu;
    int i, j;
    if (!p || largo < 0) return 0;
    for (i = 0; i < largo; i++) {
        crc ^= (unsigned int)p[i];
        for (j = 0; j < 8; j++) {
            unsigned int mascara = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mascara);
        }
    }
    return ~crc;
}

/* ------------------------------------------------------------------ */
/* Buffer de escritura                                                 */
/* ------------------------------------------------------------------ */
static int buf_reservar(Buffer3 *b, int extra) {
    int necesario, cap;
    char *nuevo;
    if (!b || b->error) return 0;
    if (extra < 0 || b->largo > L3_ARCHIVO_MAX - extra) {
        b->error = 1;
        return 0;
    }
    necesario = b->largo + extra;
    if (necesario <= b->cap) return 1;
    cap = b->cap > 0 ? b->cap : 4096;
    while (cap < necesario) {
        if (cap > L3_ARCHIVO_MAX / 2) { cap = L3_ARCHIVO_MAX; break; }
        cap *= 2;
    }
    if (cap < necesario) cap = necesario;
    if (cap > L3_ARCHIVO_MAX) cap = L3_ARCHIVO_MAX;
    nuevo = (char *)realloc(b->datos, (size_t)cap);
    if (!nuevo) {
        b->error = 1;
        return 0;
    }
    b->datos = nuevo;
    b->cap = cap;
    if (b->largo == 0) b->datos[0] = 0;
    return 1;
}

static void buf_printf(Buffer3 *b, const char *formato, ...) {
    va_list ap;
    int n, libre;
    if (!b || b->error) return;
    if (!buf_reservar(b, 1)) return;
    for (;;) {
        libre = b->cap - b->largo;
        va_start(ap, formato);
        n = vsnprintf(b->datos + b->largo, (size_t)libre, formato, ap);
        va_end(ap);
        if (n >= 0 && n < libre) {
            b->largo += n;
            return;
        }
        if (n >= libre) {
            if (!buf_reservar(b, n + 1)) return;
        } else {
            if (b->cap >= L3_ARCHIVO_MAX) { b->error = 1; return; }
            if (!buf_reservar(b, b->cap)) return;
        }
    }
}

static void buf_caracter(Buffer3 *b, char c) {
    if (!b || b->error) return;
    if (!buf_reservar(b, 2)) return;
    b->datos[b->largo++] = c;
    b->datos[b->largo] = 0;
}

/* Texto entre comillas con los escapes del formato. */
static void buf_texto(Buffer3 *b, const char *texto) {
    const char *p = texto ? texto : "";
    buf_caracter(b, '"');
    while (*p) {
        switch (*p) {
            case '"':  buf_printf(b, "\\\""); break;
            case '\\': buf_printf(b, "\\\\"); break;
            case '\n': buf_printf(b, "\\n"); break;
            case '\r': buf_printf(b, "\\r"); break;
            case '\t': buf_printf(b, "\\t"); break;
            default:   buf_caracter(b, *p); break;
        }
        p++;
    }
    buf_caracter(b, '"');
}

static int texto_simple(const char *texto) {
    const char *p = texto;
    if (!p || !*p) return 0;
    while (*p) {
        unsigned char c = (unsigned char)*p++;
        if (c <= ' ' || c == '"' || c == '\\' || c == 127) return 0;
    }
    return 1;
}

/* Valores opcionales (variables, etiquetas): sin comillas si es simple. */
static void buf_valor(Buffer3 *b, const char *texto) {
    if (texto_simple(texto)) buf_printf(b, "%s", texto);
    else buf_texto(b, texto);
}

/* ------------------------------------------------------------------ */
/* Tokenizador de una linea                                            */
/* ------------------------------------------------------------------ */
/* Decodifica comillas y escapes en el mismo buffer. Los punteros de
 * `l->tok` apuntan dentro de `linea`. Devuelve 0 si hay demasiados
 * campos. */
static int tokenizar(char *linea, Linea3 *l) {
    char *s = linea;
    l->n = 0;
    while (*s) {
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
        if (!*s) break;
        if (l->n >= L3_TOKENS_MAX) return 0;
        if (*s == '"') {
            char *inicio = s;
            char *dst = s;
            s++;
            while (*s && *s != '"') {
                if (*s == '\\' && s[1]) {
                    s++;
                    switch (*s) {
                        case 'n': *dst++ = '\n'; break;
                        case 'r': *dst++ = '\r'; break;
                        case 't': *dst++ = '\t'; break;
                        case '"': *dst++ = '"'; break;
                        case '\\': *dst++ = '\\'; break;
                        default: *dst++ = *s; break;
                    }
                    s++;
                } else {
                    *dst++ = *s++;
                }
            }
            if (*s == '"') s++;
            *dst = 0;
            l->tok[l->n++] = inicio;
        } else {
            l->tok[l->n++] = s;
            while (*s && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') s++;
            if (*s) *s++ = 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Lector                                                              */
/* ------------------------------------------------------------------ */
static char *leer_todo(const char *ruta, int *largo) {
    FILE *f;
    long tam;
    char *datos;
    size_t leido;
    if (!ruta || !largo) return NULL;
    f = fopen(ruta, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    tam = ftell(f);
    if (tam < 0 || tam > L3_ARCHIVO_MAX) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    datos = (char *)malloc((size_t)tam + 1);
    if (!datos) { fclose(f); return NULL; }
    leido = fread(datos, 1, (size_t)tam, f);
    fclose(f);
    if (leido != (size_t)tam) { free(datos); return NULL; }
    datos[tam] = 0;
    *largo = (int)tam;
    return datos;
}

/* Busca la linea FIN y, si existe, el CRC32 que la sigue. Se ejecuta
 * antes de tokenizar porque el tokenizador modifica el buffer. */
static int prescan_fin(const char *datos, int largo, int *fin_offset,
                       int *crc_estado, unsigned int *crc, int *crc_linea) {
    int pos = 0;
    int linea = 0;
    *crc_estado = 0;
    while (pos < largo) {
        int ini = pos;
        int fin;
        while (pos < largo && datos[pos] != '\n') pos++;
        fin = pos;
        if (pos < largo) pos++;
        linea++;
        {
            int a = ini, b = fin;
            while (a < b && (unsigned char)datos[a] <= ' ') a++;
            while (b > a && (unsigned char)datos[b - 1] <= ' ') b--;
            if (b - a == 3 && memcmp(datos + a, "FIN", 3) == 0) {
                int q, linea_crc = linea;
                *fin_offset = pos;
                q = pos;
                while (q < largo) {
                    int i2 = q, f2;
                    while (q < largo && datos[q] != '\n') q++;
                    f2 = q;
                    if (q < largo) q++;
                    linea_crc++;
                    {
                        int a2 = i2, b2 = f2;
                        while (a2 < b2 && (unsigned char)datos[a2] <= ' ') a2++;
                        while (b2 > a2 && (unsigned char)datos[b2 - 1] <= ' ') b2--;
                        if (b2 == a2) continue;
                        if (b2 - a2 >= 5 && memcmp(datos + a2, "CRC32", 5) == 0) {
                            char hex[16];
                            int h = 0;
                            int p = a2 + 5;
                            while (p < b2 && (datos[p] == ' ' || datos[p] == '\t')) p++;
                            while (p < b2 && h < 15) {
                                char c = datos[p];
                                if (!((c >= '0' && c <= '9') ||
                                      (c >= 'a' && c <= 'f') ||
                                      (c >= 'A' && c <= 'F')))
                                    break;
                                hex[h++] = c;
                                p++;
                            }
                            while (p < b2 && (datos[p] == ' ' || datos[p] == '\t')) p++;
                            hex[h] = 0;
                            if (h > 0 && h <= 8 && p == b2) {
                                *crc = (unsigned int)strtoul(hex, NULL, 16);
                                *crc_estado = 1;
                                *crc_linea = linea_crc;
                            } else {
                                *crc_estado = 2;
                                *crc_linea = linea_crc;
                            }
                        }
                        break;
                    }
                }
                return 1;
            }
        }
    }
    return 0;
}

static int parsear_evento(Linea3 *l, int linea, Evento3 *ev, int max,
                          int *num, int en_funcion, char *error, int tam) {
    int id = 0, tipo = 0;
    if (l->n < 3)
        return fallo3(error, tam, linea, "evento incompleto");
    if (!a_entero(l->tok[1], &id) || id <= 0)
        return fallo3(error, tam, linea, "id de evento invalido");
    tipo = logic3_evento_desde_texto(l->tok[2]);
    if (tipo <= 0)
        return fallo3(error, tam, linea, "tipo de evento desconocido: %s", l->tok[2]);
    if (en_funcion && tipo != L3_EVENTO_LLAMADA)
        return fallo3(error, tam, linea, "una funcion solo admite evento llamada");
    if (!en_funcion && tipo == L3_EVENTO_LLAMADA)
        return fallo3(error, tam, linea, "evento llamada fuera de una funcion");
    if (*num >= max)
        return fallo3(error, tam, linea, "demasiados eventos");

    memset(ev, 0, sizeof(*ev));
    ev->id = id;
    ev->tipo = tipo;
    ev->flanco = L3_FLANCO_MANTENER;
    ev->objeto = -1;
    ev->linea = linea;

    switch (tipo) {
        case L3_EVENTO_TECLA:
            if (l->n < 5)
                return fallo3(error, tam, linea, "evento tecla sin flanco");
            copiar_texto(ev->nombre, sizeof(ev->nombre), l->tok[3]);
            ev->flanco = flanco_desde_texto(l->tok[4]);
            if (ev->flanco < 0)
                return fallo3(error, tam, linea, "flanco desconocido: %s", l->tok[4]);
            break;
        case L3_EVENTO_RECIBIR:
            if (l->n < 4)
                return fallo3(error, tam, linea, "evento recibir sin mensaje");
            copiar_texto(ev->nombre, sizeof(ev->nombre), l->tok[3]);
            break;
        case L3_EVENTO_TOCAR:
            if (l->n < 4)
                return fallo3(error, tam, linea, "evento tocar sin objeto");
            if (!a_entero(l->tok[3], &ev->objeto))
                return fallo3(error, tam, linea, "objeto de tocar invalido");
            if (l->n >= 5) copiar_texto(ev->nombre, sizeof(ev->nombre), l->tok[4]);
            break;
        case L3_EVENTO_CADA:
            if (l->n < 4)
                return fallo3(error, tam, linea, "evento cada sin fotogramas");
            if (!a_entero(l->tok[3], &ev->cada) || ev->cada <= 0)
                return fallo3(error, tam, linea, "fotogramas de cada invalidos");
            break;
        case L3_EVENTO_ESCENA:
            if (l->n < 4)
                return fallo3(error, tam, linea, "evento escena sin id");
            if (!a_entero(l->tok[3], &ev->escena) || ev->escena <= 0)
                return fallo3(error, tam, linea, "id de escena invalido");
            break;
        default:
            break;
    }
    (*num)++;
    return 1;
}

static int parsear_campo(Bloque3 *b, Linea3 *l, int linea,
                         char *error, int tam) {
    const char *clave, *valor;
    if (!b)
        return fallo3(error, tam, linea, "campo fuera de un bloque");
    if (l->n < 3)
        return fallo3(error, tam, linea, "campo sin clave");
    clave = l->tok[1];
    valor = l->n >= 3 ? l->tok[2] : "";
    if (strcmp(b->tipo, "codigo.lobo") == 0 &&
        strcmp(clave, "codigo") == 0) {
        int n = (int)strlen(valor);
        if (n >= FUNCIONES_CODIGO_MAX)
            return fallo3(error, tam, linea,
                          "codigo.lobo supera %d bytes", FUNCIONES_CODIGO_MAX - 1);
        memcpy(b->codigo, valor, (size_t)n);
        b->codigo[n] = 0;
        b->tiene_codigo = 1;
        return 1;
    }
    if (b->num_campos >= L3_CAMPOS_MAX)
        return fallo3(error, tam, linea, "demasiados campos en el bloque");
    copiar_texto(b->campos[b->num_campos].clave, L3_NOMBRE_MAX, clave);
    copiar_texto(b->campos[b->num_campos].valor, L3_TEXTO_MAX, valor);
    b->campos[b->num_campos].linea = linea;
    b->num_campos++;
    return 1;
}

int logic3_cargar(Proyecto3 *p, const char *ruta, char *error, int tam_error) {
    Proyecto3 tmp;
    char *datos;
    int largo = 0, pos = 0, num_linea = 0;
    int visto_cabecera = 0, visto_version = 0, visto_fin = 0;
    int ctx = CTX_NINGUNO, objeto = -1, funcion = -1, bloque = -1;
    int fin_offset = 0, crc_estado = 0, crc_linea = 0;
    unsigned int crc = 0, crc_real = 0;
    int hay_crc = 0;
    int i;

    if (error && tam_error > 0) error[0] = 0;
    if (!p) return 0;
    logic3_iniciar(&tmp);
    datos = leer_todo(ruta, &largo);
    if (!datos)
        return fallo3(error, tam_error, 0, "no se pudo leer el archivo");

    /* Primera linea: LOGIC3 (se admiten espacios delante, no BOM). */
    i = 0;
    while (i < largo && (datos[i] == ' ' || datos[i] == '\t')) i++;
    if (i + 6 > largo || memcmp(datos + i, "LOGIC3", 6) != 0) {
        free(datos);
        return fallo3(error, tam_error, 1, "no es un proyecto LOGIC3");
    }

    prescan_fin(datos, largo, &fin_offset, &crc_estado, &crc, &crc_linea);
    /* El CRC se calcula antes de tokenizar: el tokenizador parte el
     * buffer en sitio (escribe ceros en espacios y comillas) y despues
     * ya no coincidiria con lo que hay en disco. */
    if (crc_estado == 1) {
        crc_real = logic3_crc32(datos, fin_offset);
        hay_crc = 1;
    }

    while (pos < largo) {
        int ini = pos;
        int fin;
        Linea3 l;
        char **tok;

        while (pos < largo && datos[pos] != '\n') pos++;
        fin = pos;
        if (pos < largo) pos++;
        num_linea++;
        if (fin > ini && datos[fin - 1] == '\r') fin--;
        if (fin - ini > L3_LINEA_MAX) {
            fallo3(error, tam_error, num_linea, "linea demasiado larga");
            goto error_lectura;
        }
        datos[fin] = 0;
        if (!tokenizar(datos + ini, &l)) {
            fallo3(error, tam_error, num_linea, "demasiados campos");
            goto error_lectura;
        }
        if (l.n == 0) continue;
        tok = l.tok;

        if (!visto_cabecera) {
            if (strcmp(tok[0], "LOGIC3") != 0) {
                fallo3(error, tam_error, num_linea, "falta la cabecera LOGIC3");
                goto error_lectura;
            }
            visto_cabecera = 1;
            continue;
        }

        if (strcmp(tok[0], "version") == 0) {
            int v = 0;
            if (l.n < 2 || !a_entero(tok[1], &v) || v < 1) {
                fallo3(error, tam_error, num_linea, "version invalida");
                goto error_lectura;
            }
            if (v > L3_VERSION) {
                fallo3(error, tam_error, num_linea,
                       "version %d no soportada (maximo %d)", v, L3_VERSION);
                goto error_lectura;
            }
            tmp.version = v;
            visto_version = 1;
            continue;
        }

        if (strcmp(tok[0], "proyecto") == 0) {
            int ancho, alto, limitar, fps, vel;
            unsigned int fondo;
            if (l.n < 8) {
                fallo3(error, tam_error, num_linea, "proyecto incompleto");
                goto error_lectura;
            }
            if (!a_entero(tok[2], &ancho) || !a_entero(tok[3], &alto) ||
                !a_entero(tok[4], &limitar) || !a_entero(tok[5], &fps) ||
                !a_entero(tok[6], &vel) || !a_u32(tok[7], &fondo)) {
                fallo3(error, tam_error, num_linea, "datos de proyecto invalidos");
                goto error_lectura;
            }
            copiar_texto(tmp.nombre, sizeof(tmp.nombre), tok[1]);
            tmp.ancho = ancho;
            tmp.alto = alto;
            tmp.limitar_cuadros = limitar;
            tmp.fps = fps;
            tmp.velocidad = vel;
            tmp.fondo = (u32)fondo;
            ctx = CTX_NINGUNO;
            objeto = -1;
            funcion = -1;
            bloque = -1;
            continue;
        }

        if (strcmp(tok[0], "carpetas") == 0) {
            if (l.n < 5) {
                fallo3(error, tam_error, num_linea, "faltan carpetas");
                goto error_lectura;
            }
            for (i = 0; i < 4; i++)
                copiar_texto(tmp.carpetas[i], L3_RUTA_MAX, tok[i + 1]);
            continue;
        }

        if (strcmp(tok[0], "camara") == 0) {
            int v;
            tmp.camara.linea = num_linea;
            i = 1;
            while (i < l.n) {
                if (strcmp(tok[i], "objetivo") == 0 && i + 1 < l.n) {
                    if (!a_entero(tok[i + 1], &v)) {
                        fallo3(error, tam_error, num_linea,
                                "objetivo de camara invalido");
                        goto error_lectura;
                    }
                    tmp.camara.objetivo = v > 0 ? v : 0;
                    i += 2;
                } else if (strcmp(tok[i], "limites") == 0 && i + 4 < l.n) {
                    int k;
                    for (k = 0; k < 4; k++) {
                        if (!a_entero(tok[i + 1 + k], &v)) {
                            fallo3(error, tam_error, num_linea,
                                    "limites de camara invalidos");
                            goto error_lectura;
                        }
                        if (v < 0) v = 0;
                        if (k == 0) tmp.camara.x = v;
                        else if (k == 1) tmp.camara.y = v;
                        else if (k == 2) tmp.camara.ancho = v;
                        else tmp.camara.alto = v;
                    }
                    i += 5;
                } else if (strcmp(tok[i], "zoom") == 0 && i + 1 < l.n) {
                    if (!a_entero(tok[i + 1], &v)) {
                        fallo3(error, tam_error, num_linea,
                                "zoom de camara invalido");
                        goto error_lectura;
                    }
                    tmp.camara.zoom = (v == 2 || v == 4) ? v : 1;
                    i += 2;
                } else {
                    i++;
                }
            }
            continue;
        }

        if (strcmp(tok[0], "variable") == 0) {
            Variable3 *v;
            int tipo;
            if (l.n >= 2 && strcmp(tok[1], "global") == 0) {
                if (l.n < 4) {
                    fallo3(error, tam_error, num_linea, "variable global incompleta");
                    goto error_lectura;
                }
                if (tmp.num_variables >= L3_VARIABLES_GLOBAL_MAX) {
                    fallo3(error, tam_error, num_linea, "demasiadas variables globales");
                    goto error_lectura;
                }
                tipo = logic3_tipo_desde_texto(tok[3]);
                if (tipo < 0) {
                    fallo3(error, tam_error, num_linea, "tipo de variable desconocido: %s", tok[3]);
                    goto error_lectura;
                }
                v = &tmp.variables[tmp.num_variables++];
                memset(v, 0, sizeof(*v));
                copiar_texto(v->nombre, sizeof(v->nombre), tok[2]);
                v->tipo = tipo;
                if (l.n >= 5) copiar_texto(v->valor, sizeof(v->valor), tok[4]);
                v->linea = num_linea;
            } else {
                Objeto3 *o;
                if (ctx != CTX_OBJETO || objeto < 0) {
                    fallo3(error, tam_error, num_linea, "variable fuera de un objeto");
                    goto error_lectura;
                }
                if (l.n < 3) {
                    fallo3(error, tam_error, num_linea, "variable de objeto incompleta");
                    goto error_lectura;
                }
                o = &tmp.objetos[objeto];
                if (o->num_variables >= L3_VARIABLES_OBJETO_MAX) {
                    fallo3(error, tam_error, num_linea, "demasiadas variables en el objeto");
                    goto error_lectura;
                }
                tipo = logic3_tipo_desde_texto(tok[2]);
                if (tipo < 0) {
                    fallo3(error, tam_error, num_linea, "tipo de variable desconocido: %s", tok[2]);
                    goto error_lectura;
                }
                v = &o->variables[o->num_variables++];
                memset(v, 0, sizeof(*v));
                copiar_texto(v->nombre, sizeof(v->nombre), tok[1]);
                v->tipo = tipo;
                if (l.n >= 4) copiar_texto(v->valor, sizeof(v->valor), tok[3]);
                v->linea = num_linea;
            }
            continue;
        }

        if (strcmp(tok[0], "lista") == 0) {
            Lista3 *li;
            int capacidad, tipo = L3_TIPO_ENTERO, primero = 4, k;
            if (l.n < 2 || strcmp(tok[1], "global") != 0) {
                fallo3(error, tam_error, num_linea, "solo se admiten listas globales");
                goto error_lectura;
            }
            if (l.n < 4) {
                fallo3(error, tam_error, num_linea, "lista global incompleta");
                goto error_lectura;
            }
            if (tmp.num_listas >= L3_LISTAS_MAX) {
                fallo3(error, tam_error, num_linea, "demasiadas listas globales");
                goto error_lectura;
            }
            if (!a_entero(tok[3], &capacidad) || capacidad < 0) {
                fallo3(error, tam_error, num_linea, "capacidad de lista invalida");
                goto error_lectura;
            }
            /* Quinto token opcional: tipo del elemento. Si no es un tipo
             * conocido se interpretan como valores con tipo entero, para
             * seguir leyendo listas del formato anterior. */
            if (l.n >= 5) {
                int t = logic3_tipo_desde_texto(tok[4]);
                if (t >= 0) {
                    tipo = t;
                    primero = 5;
                }
            }
            li = &tmp.listas[tmp.num_listas++];
            memset(li, 0, sizeof(*li));
            copiar_texto(li->nombre, sizeof(li->nombre), tok[2]);
            li->capacidad = capacidad;
            li->tipo = tipo;
            li->linea = num_linea;
            if (l.n > primero) {
                if (!lista_elementos_reservar(li)) {
                    fallo3(error, tam_error, num_linea,
                           "sin memoria para los elementos de la lista");
                    goto error_lectura;
                }
                for (k = primero; k < l.n &&
                     li->num_elementos < L3_LISTA_ELEMENTOS_MAX; k++)
                    copiar_texto(li->elementos[li->num_elementos++],
                                 L3_TEXTO_MAX, tok[k]);
            }
            continue;
        }

        if (strcmp(tok[0], "escena") == 0) {
            Escena3 *e;
            int id, ancho, alto;
            unsigned int fondo;
            if (l.n < 7) {
                fallo3(error, tam_error, num_linea, "escena incompleta");
                goto error_lectura;
            }
            if (!a_entero(tok[1], &id) || id <= 0 ||
                !a_entero(tok[4], &ancho) || !a_entero(tok[5], &alto) ||
                !a_u32(tok[6], &fondo)) {
                fallo3(error, tam_error, num_linea, "datos de escena invalidos");
                goto error_lectura;
            }
            if (tmp.num_escenas >= L3_ESCENAS_MAX) {
                fallo3(error, tam_error, num_linea, "demasiadas escenas");
                goto error_lectura;
            }
            e = &tmp.escenas[tmp.num_escenas++];
            memset(e, 0, sizeof(*e));
            e->id = id;
            copiar_texto(e->nombre, sizeof(e->nombre), tok[2]);
            copiar_texto(e->mapa, sizeof(e->mapa), tok[3]);
            e->ancho = ancho;
            e->alto = alto;
            e->fondo = (u32)fondo;
            e->linea = num_linea;
            ctx = CTX_NINGUNO;
            objeto = -1;
            funcion = -1;
            bloque = -1;
            continue;
        }

        if (strcmp(tok[0], "objeto") == 0) {
            Objeto3 *o;
            int id, tiene_escena = 0, tiene_x = 0, tiene_y = 0;
            int tiene_w = 0, tiene_h = 0, tiene_visible = 0, tiene_capa = 0;
            if (!objetos_reservar(&tmp)) {
                fallo3(error, tam_error, num_linea, "sin memoria para objetos");
                goto error_lectura;
            }
            if (tmp.num_objetos >= L3_OBJETOS_MAX) {
                fallo3(error, tam_error, num_linea, "demasiados objetos");
                goto error_lectura;
            }
            if (l.n < 3) {
                fallo3(error, tam_error, num_linea, "objeto incompleto");
                goto error_lectura;
            }
            if (!a_entero(tok[1], &id) || id <= 0) {
                fallo3(error, tam_error, num_linea, "id de objeto invalido");
                goto error_lectura;
            }
            o = &tmp.objetos[tmp.num_objetos];
            memset(o, 0, sizeof(*o));
            o->id = id;
            o->visible = 1;
            copiar_texto(o->nombre, sizeof(o->nombre), tok[2]);
            for (i = 3; i < l.n; i += 2) {
                int valor;
                if (i + 1 >= l.n) {
                    fallo3(error, tam_error, num_linea, "falta el valor de %s", tok[i]);
                    goto error_lectura;
                }
                if (!a_entero(tok[i + 1], &valor)) {
                    fallo3(error, tam_error, num_linea, "valor invalido en %s", tok[i]);
                    goto error_lectura;
                }
                if (strcmp(tok[i], "escena") == 0) {
                    o->escena = valor;
                    tiene_escena = 1;
                } else if (strcmp(tok[i], "x") == 0) {
                    o->x = valor;
                    tiene_x = 1;
                } else if (strcmp(tok[i], "y") == 0) {
                    o->y = valor;
                    tiene_y = 1;
                } else if (strcmp(tok[i], "w") == 0) {
                    o->w = valor;
                    tiene_w = 1;
                } else if (strcmp(tok[i], "h") == 0) {
                    o->h = valor;
                    tiene_h = 1;
                } else if (strcmp(tok[i], "visible") == 0) {
                    o->visible = valor;
                    tiene_visible = 1;
                } else if (strcmp(tok[i], "capa") == 0) {
                    o->capa = valor;
                    tiene_capa = 1;
                } else {
                    fallo3(error, tam_error, num_linea, "campo de objeto desconocido: %s", tok[i]);
                    goto error_lectura;
                }
            }
            if (!tiene_escena || !tiene_x || !tiene_y || !tiene_w ||
                !tiene_h || !tiene_visible || !tiene_capa) {
                fallo3(error, tam_error, num_linea, "objeto con campos incompletos");
                goto error_lectura;
            }
            o->linea = num_linea;
            objeto = tmp.num_objetos++;
            funcion = -1;
            ctx = CTX_OBJETO;
            bloque = -1;
            continue;
        }

        if (strcmp(tok[0], "traje") == 0) {
            Objeto3 *o;
            Traje3 *t;
            int indice, ancho, alto, px, py;
            if (ctx != CTX_OBJETO || objeto < 0) {
                fallo3(error, tam_error, num_linea, "traje fuera de un objeto");
                goto error_lectura;
            }
            if (l.n < 7) {
                fallo3(error, tam_error, num_linea, "traje incompleto");
                goto error_lectura;
            }
            if (!a_entero(tok[1], &indice) || indice < 0 ||
                !a_entero(tok[3], &ancho) || !a_entero(tok[4], &alto) ||
                !a_entero(tok[5], &px) || !a_entero(tok[6], &py)) {
                fallo3(error, tam_error, num_linea, "datos de traje invalidos");
                goto error_lectura;
            }
            o = &tmp.objetos[objeto];
            if (o->num_trajes >= L3_TRAJES_MAX) {
                fallo3(error, tam_error, num_linea, "demasiados trajes");
                goto error_lectura;
            }
            t = &o->trajes[o->num_trajes++];
            memset(t, 0, sizeof(*t));
            t->indice = indice;
            copiar_texto(t->imagen, sizeof(t->imagen), tok[2]);
            t->ancho = ancho;
            t->alto = alto;
            t->pivote_x = px;
            t->pivote_y = py;
            t->cuadros = 1;
            t->velocidad = 1;
            /* Extensiones opcionales como pares clave valor: no cambian
             * el formato de los archivos que no las usan. */
            for (i = 7; i + 1 < l.n; i += 2) {
                int v;
                if (strcmp(tok[i], "cuadros") == 0 && a_entero(tok[i + 1], &v)) {
                    if (v > 0) t->cuadros = v;
                } else if (strcmp(tok[i], "cuadro_ancho") == 0 &&
                           a_entero(tok[i + 1], &v)) {
                    t->cuadro_ancho = v;
                } else if (strcmp(tok[i], "cuadro_alto") == 0 &&
                           a_entero(tok[i + 1], &v)) {
                    t->cuadro_alto = v;
                } else if (strcmp(tok[i], "flip_x") == 0) {
                    t->flip_x = strcmp(tok[i + 1], "1") == 0;
                } else if (strcmp(tok[i], "flip_y") == 0) {
                    t->flip_y = strcmp(tok[i + 1], "1") == 0;
                } else if (strcmp(tok[i], "rotacion") == 0 &&
                           a_entero(tok[i + 1], &v)) {
                    t->rotacion = (v == 90 || v == 180 || v == 270) ? v : 0;
                } else if (strcmp(tok[i], "velocidad") == 0 &&
                           a_entero(tok[i + 1], &v)) {
                    if (v > 0) t->velocidad = v;
                }
            }
            t->linea = num_linea;
            continue;
        }

        if (strcmp(tok[0], "evento") == 0) {
            int ok;
            if (ctx == CTX_OBJETO && objeto >= 0) {
                Objeto3 *o = &tmp.objetos[objeto];
                ok = parsear_evento(&l, num_linea, &o->eventos[o->num_eventos],
                                    L3_EVENTOS_OBJETO_MAX, &o->num_eventos,
                                    0, error, tam_error);
            } else if (ctx == CTX_FUNCION && funcion >= 0) {
                Funcion3 *fn = &tmp.funciones[funcion];
                ok = parsear_evento(&l, num_linea, &fn->eventos[fn->num_eventos],
                                    L3_EVENTOS_FUNCION_MAX, &fn->num_eventos,
                                    1, error, tam_error);
            } else {
                fallo3(error, tam_error, num_linea, "evento fuera de un objeto o funcion");
                ok = 0;
            }
            if (!ok) goto error_lectura;
            bloque = -1;
            continue;
        }

        if (strcmp(tok[0], "funcion") == 0) {
            Funcion3 *fn;
            int id, parametros, retorno;
            if (l.n < 5) {
                fallo3(error, tam_error, num_linea, "funcion incompleta");
                goto error_lectura;
            }
            if (!a_entero(tok[1], &id) || id <= 0 ||
                !a_entero(tok[3], &parametros) || !a_entero(tok[4], &retorno)) {
                fallo3(error, tam_error, num_linea, "datos de funcion invalidos");
                goto error_lectura;
            }
            if (tmp.num_funciones >= L3_FUNCIONES_MAX) {
                fallo3(error, tam_error, num_linea, "demasiadas funciones");
                goto error_lectura;
            }
            fn = &tmp.funciones[tmp.num_funciones];
            memset(fn, 0, sizeof(*fn));
            fn->id = id;
            copiar_texto(fn->nombre, sizeof(fn->nombre), tok[2]);
            fn->parametros = parametros;
            fn->retorno = retorno;
            fn->linea = num_linea;
            funcion = tmp.num_funciones++;
            objeto = -1;
            ctx = CTX_FUNCION;
            bloque = -1;
            continue;
        }

        if (strcmp(tok[0], "bloque") == 0) {
            Bloque3 *b;
            int id = 0, evento = -1, padre = -1, orden = -1;
            int tiene_tipo = 0;
            if (!bloques_reservar(&tmp, 1)) {
                fallo3(error, tam_error, num_linea, "demasiados bloques");
                goto error_lectura;
            }
            if (l.n < 3) {
                fallo3(error, tam_error, num_linea, "bloque incompleto");
                goto error_lectura;
            }
            if (!a_entero(tok[1], &id) || id <= 0) {
                fallo3(error, tam_error, num_linea, "id de bloque invalido");
                goto error_lectura;
            }
            b = &tmp.bloques[tmp.num_bloques];
            memset(b, 0, sizeof(*b));
            b->id = id;
            for (i = 2; i < l.n; i += 2) {
                if (i + 1 >= l.n) {
                    fallo3(error, tam_error, num_linea, "falta el valor de %s", tok[i]);
                    goto error_lectura;
                }
                if (strcmp(tok[i], "evento") == 0) {
                    if (!a_entero(tok[i + 1], &evento)) {
                        fallo3(error, tam_error, num_linea, "evento de bloque invalido");
                        goto error_lectura;
                    }
                } else if (strcmp(tok[i], "padre") == 0) {
                    if (!a_entero(tok[i + 1], &padre)) {
                        fallo3(error, tam_error, num_linea, "padre de bloque invalido");
                        goto error_lectura;
                    }
                } else if (strcmp(tok[i], "orden") == 0) {
                    if (!a_entero(tok[i + 1], &orden)) {
                        fallo3(error, tam_error, num_linea, "orden de bloque invalido");
                        goto error_lectura;
                    }
                } else if (strcmp(tok[i], "tipo") == 0) {
                    copiar_texto(b->tipo, sizeof(b->tipo), tok[i + 1]);
                    tiene_tipo = 1;
                } else {
                    fallo3(error, tam_error, num_linea, "campo de bloque desconocido: %s", tok[i]);
                    goto error_lectura;
                }
            }
            if (evento < 0 || padre < 0 || orden < 0 || !tiene_tipo) {
                fallo3(error, tam_error, num_linea, "bloque con campos incompletos");
                goto error_lectura;
            }
            b->evento = evento;
            b->padre = padre;
            b->orden = orden;
            b->linea = num_linea;
            bloque = tmp.num_bloques++;
            continue;
        }

        if (strcmp(tok[0], "campo") == 0) {
            Bloque3 *b = (bloque >= 0) ? &tmp.bloques[bloque] : NULL;
            if (!parsear_campo(b, &l, num_linea, error, tam_error))
                goto error_lectura;
            continue;
        }

        if (strcmp(tok[0], "ranura") == 0) {
            Bloque3 *b;
            int ref;
            if (bloque < 0) {
                fallo3(error, tam_error, num_linea, "ranura fuera de un bloque");
                goto error_lectura;
            }
            if (l.n < 4) {
                fallo3(error, tam_error, num_linea, "ranura incompleta");
                goto error_lectura;
            }
            if (!a_entero(tok[2], &ref) || ref <= 0) {
                fallo3(error, tam_error, num_linea, "bloque de ranura invalido");
                goto error_lectura;
            }
            b = &tmp.bloques[bloque];
            if (b->num_ranuras >= L3_RANURAS_MAX) {
                fallo3(error, tam_error, num_linea, "demasiadas ranuras en el bloque");
                goto error_lectura;
            }
            copiar_texto(b->ranuras[b->num_ranuras].clave, L3_NOMBRE_MAX, tok[1]);
            b->ranuras[b->num_ranuras].bloque = ref;
            b->ranuras[b->num_ranuras].linea = num_linea;
            b->num_ranuras++;
            continue;
        }

        if (strcmp(tok[0], "hijo") == 0) {
            Bloque3 *b;
            int ref;
            if (bloque < 0) {
                fallo3(error, tam_error, num_linea, "hijo fuera de un bloque");
                goto error_lectura;
            }
            if (l.n < 3 || !a_entero(tok[1], &ref) || ref <= 0) {
                fallo3(error, tam_error, num_linea, "hijo invalido");
                goto error_lectura;
            }
            b = &tmp.bloques[bloque];
            if (b->num_hijos >= L3_HIJOS_MAX) {
                fallo3(error, tam_error, num_linea, "demasiados hijos en el bloque");
                goto error_lectura;
            }
            b->hijos[b->num_hijos++] = ref;
            continue;
        }

        if (strcmp(tok[0], "FIN") == 0) {
            visto_fin = 1;
            pos = largo;
            break;
        }

        /* Seccion desconocida: se ignora para permitir extensiones. */
    }

    if (!visto_cabecera) {
        fallo3(error, tam_error, 1, "falta la cabecera LOGIC3");
        goto error_lectura;
    }
    if (!visto_version) {
        fallo3(error, tam_error, num_linea > 0 ? num_linea : 1, "falta version");
        goto error_lectura;
    }
    if (!visto_fin) {
        fallo3(error, tam_error, num_linea + 1, "falta FIN");
        goto error_lectura;
    }
    if (crc_estado == 2) {
        fallo3(error, tam_error, crc_linea, "CRC32 invalido");
        goto error_lectura;
    }
    if (hay_crc && crc_real != crc) {
        fallo3(error, tam_error, crc_linea,
               "CRC32 no coincide (archivo %08X, cabecera %08X)",
               (unsigned int)crc_real, crc);
        goto error_lectura;
    }
    if (!logic3_validar(&tmp, error, tam_error))
        goto error_lectura;

    free(datos);
    logic3_liberar(p);
    *p = tmp;
    return 1;

error_lectura:
    free(datos);
    logic3_liberar(&tmp);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Validador                                                           */
/* ------------------------------------------------------------------ */
static const char *codigo_de_bloque(const Bloque3 *b) {
    int i;
    if (!b) return "";
    if (b->tiene_codigo) return b->codigo;
    for (i = 0; i < b->num_campos; i++) {
        if (strcmp(b->campos[i].clave, "codigo") == 0)
            return b->campos[i].valor;
    }
    return "";
}

static const Escena3 *buscar_escena(const Proyecto3 *p, int id) {
    int i;
    for (i = 0; i < p->num_escenas; i++)
        if (p->escenas[i].id == id) return &p->escenas[i];
    return NULL;
}

static const Objeto3 *buscar_objeto(const Proyecto3 *p, int id) {
    int i;
    for (i = 0; i < p->num_objetos; i++)
        if (p->objetos[i].id == id) return &p->objetos[i];
    return NULL;
}

static int validar_ids(const int *ids, const int *lineas, int n,
                       const char *que, char *error, int tam) {
    IdLinea3 *v;
    int i;
    if (n <= 1) return 1;
    v = (IdLinea3 *)malloc(sizeof(IdLinea3) * (size_t)n);
    if (!v) return 1;
    for (i = 0; i < n; i++) {
        v[i].id = ids[i];
        v[i].linea = lineas[i];
    }
    qsort(v, (size_t)n, sizeof(IdLinea3), comparar_id_linea);
    for (i = 1; i < n; i++) {
        if (v[i].id == v[i - 1].id) {
            int linea = v[i].linea;
            int id = v[i].id;
            free(v);
            return fallo3(error, tam, linea, "id de %s duplicado: %d", que, id);
        }
    }
    free(v);
    return 1;
}

static int validar_tipo(int tipo, int linea, const char *que,
                        char *error, int tam) {
    if (tipo < L3_TIPO_ENTERO || tipo > L3_TIPO_TEXTO)
        return fallo3(error, tam, linea, "tipo de %s invalido: %d", que, tipo);
    return 1;
}

/* Comprueba que el valor escrito encaja con el tipo declarado. Los textos
 * se leen tal cual; el tope de 64 bytes es el mismo que usa lobo. */
static int validar_valor_tipo(int tipo, const char *valor, int linea,
                              const char *que, char *error, int tam) {
    int n;
    if (!valor) return 1;
    if ((int)strlen(valor) >= L3_TEXTO_MAX)
        return fallo3(error, tam, linea, "%s demasiado largo", que);
    if (tipo == L3_TIPO_ENTERO) {
        if (!a_entero(valor, &n))
            return fallo3(error, tam, linea, "%s entero invalido: %s", que, valor);
    } else if (tipo == L3_TIPO_BOOLEANO) {
        if (!a_entero(valor, &n) || (n != 0 && n != 1))
            return fallo3(error, tam, linea, "%s booleano invalido: %s", que, valor);
    }
    return 1;
}

static int validar_expresion(const char *valor, int linea, const char *que,
                             char *error, int tam) {
    LoboPrograma programa;
    char e[LOBO_ERROR_MAX];
    int ok;
    if (!valor || valor[0] != '=') return 1;
    lobo_programa_iniciar(&programa);
    ok = lobo_compilar(valor + 1, &programa, LOBO_MODO_VM, e, sizeof(e));
    lobo_programa_liberar(&programa);
    if (!ok)
        return fallo3(error, tam, linea, "expresion de %s no compila: %s", que, e);
    return 1;
}

int logic3_validar(const Proyecto3 *p, char *error, int tam_error) {
    int i, j, n;
    int total_eventos = 0, total_bloques;
    int *ev_ids = NULL, *ev_lineas = NULL;
    int *bl_ids = NULL, *bl_lineas = NULL;
    int escenas_ids[L3_ESCENAS_MAX], objetos_ids[L3_OBJETOS_MAX];
    int funciones_ids[L3_FUNCIONES_MAX];
    int escenas_lineas[L3_ESCENAS_MAX], objetos_lineas[L3_OBJETOS_MAX];
    int funciones_lineas[L3_FUNCIONES_MAX];

    if (error && tam_error > 0) error[0] = 0;
#define FALLO_VALIDAR(...) \
    do { fallo3(error, tam_error, __VA_ARGS__); goto fallo; } while (0)

    if (!p) FALLO_VALIDAR(0, "modelo nulo");
    if (p->version < 1 || p->version > L3_VERSION)
        FALLO_VALIDAR(1, "version %d no soportada", p->version);
    if (p->num_escenas < 0 || p->num_escenas > L3_ESCENAS_MAX)
        FALLO_VALIDAR(0, "numero de escenas fuera de rango");
    if (p->num_objetos < 0 || p->num_objetos > L3_OBJETOS_MAX)
        FALLO_VALIDAR(0, "numero de objetos fuera de rango");
    if (p->num_funciones < 0 || p->num_funciones > L3_FUNCIONES_MAX)
        FALLO_VALIDAR(0, "numero de funciones fuera de rango");
    if (p->num_bloques < 0 || p->num_bloques > L3_BLOQUES_MAX)
        FALLO_VALIDAR(0, "numero de bloques fuera de rango");
    if (p->num_objetos > 0 && !p->objetos)
        FALLO_VALIDAR(0, "faltan los objetos del modelo");
    if (p->num_bloques > 0 && !p->bloques)
        FALLO_VALIDAR(0, "faltan los bloques del modelo");
    if (p->num_variables < 0 || p->num_variables > L3_VARIABLES_GLOBAL_MAX)
        FALLO_VALIDAR(0, "numero de variables fuera de rango");
    if (p->num_listas < 0 || p->num_listas > L3_LISTAS_MAX)
        FALLO_VALIDAR(0, "numero de listas fuera de rango");
    if (p->ancho <= 0 || p->alto <= 0)
        FALLO_VALIDAR(1, "resolucion de proyecto invalida");
    if (p->fps <= 0) FALLO_VALIDAR(1, "fps invalido");
    if (p->camara.zoom != 1 && p->camara.zoom != 2 && p->camara.zoom != 4)
        FALLO_VALIDAR(p->camara.linea ? p->camara.linea : 1,
                      "zoom de camara invalido");
    if (p->camara.x < 0 || p->camara.y < 0 || p->camara.ancho < 0 ||
        p->camara.alto < 0)
        FALLO_VALIDAR(p->camara.linea ? p->camara.linea : 1,
                      "limites de camara invalidos");

    /* Limites y tipos de la cabecera. */
    for (i = 0; i < p->num_variables; i++) {
        const Variable3 *v = &p->variables[i];
        if (!validar_tipo(v->tipo, v->linea, "variable global", error, tam_error))
            goto fallo;
        if (!validar_valor_tipo(v->tipo, v->valor, v->linea,
                                "valor de variable global", error, tam_error))
            goto fallo;
    }
    for (i = 0; i < p->num_listas; i++) {
        const Lista3 *li = &p->listas[i];
        if (li->capacidad < 0 || li->capacidad > L3_LISTA_ELEMENTOS_MAX)
            FALLO_VALIDAR(li->linea, "capacidad de lista fuera de rango");
        if (!validar_tipo(li->tipo, li->linea, "lista", error, tam_error))
            goto fallo;
        if (li->num_elementos < 0 || li->num_elementos > li->capacidad ||
            li->num_elementos > L3_LISTA_ELEMENTOS_MAX)
            FALLO_VALIDAR(li->linea, "numero de elementos de lista fuera de rango");
        for (j = 0; j < li->num_elementos; j++)
            if (!validar_valor_tipo(li->tipo, li->elementos[j], li->linea,
                                    "elemento de lista", error, tam_error))
                goto fallo;
    }

    /* Ids y conteos antes de reservar los arrays de apoyo. */
    for (i = 0; i < p->num_escenas; i++) {
        escenas_ids[i] = p->escenas[i].id;
        escenas_lineas[i] = p->escenas[i].linea;
    }
    for (i = 0; i < p->num_objetos; i++) {
        const Objeto3 *o = &p->objetos[i];
        if (o->num_trajes < 0 || o->num_trajes > L3_TRAJES_MAX)
            FALLO_VALIDAR(o->linea, "numero de trajes fuera de rango");
        if (o->num_variables < 0 || o->num_variables > L3_VARIABLES_OBJETO_MAX)
            FALLO_VALIDAR(o->linea, "numero de variables fuera de rango");
        if (o->num_eventos < 0 || o->num_eventos > L3_EVENTOS_OBJETO_MAX)
            FALLO_VALIDAR(o->linea, "numero de eventos fuera de rango");
        objetos_ids[i] = o->id;
        objetos_lineas[i] = o->linea;
        total_eventos += o->num_eventos;
    }
    for (i = 0; i < p->num_funciones; i++) {
        const Funcion3 *fn = &p->funciones[i];
        if (fn->num_eventos < 0 || fn->num_eventos > L3_EVENTOS_FUNCION_MAX)
            FALLO_VALIDAR(fn->linea, "numero de eventos fuera de rango");
        funciones_ids[i] = fn->id;
        funciones_lineas[i] = fn->linea;
        total_eventos += fn->num_eventos;
    }
    if (!validar_ids(escenas_ids, escenas_lineas, p->num_escenas,
                     "escena", error, tam_error))
        goto fallo;
    if (!validar_ids(objetos_ids, objetos_lineas, p->num_objetos,
                     "objeto", error, tam_error))
        goto fallo;
    if (!validar_ids(funciones_ids, funciones_lineas, p->num_funciones,
                     "funcion", error, tam_error))
        goto fallo;

    /* Ids de eventos (globales: los bloques los referencian). */
    if (total_eventos > 0) {
        ev_ids = (int *)malloc(sizeof(int) * (size_t)total_eventos);
        ev_lineas = (int *)malloc(sizeof(int) * (size_t)total_eventos);
        if (!ev_ids || !ev_lineas)
            FALLO_VALIDAR(0, "sin memoria para validar eventos");
        n = 0;
        for (i = 0; i < p->num_objetos; i++) {
            const Objeto3 *o = &p->objetos[i];
            for (j = 0; j < o->num_eventos; j++) {
                ev_ids[n] = o->eventos[j].id;
                ev_lineas[n] = o->eventos[j].linea;
                n++;
            }
        }
        for (i = 0; i < p->num_funciones; i++) {
            const Funcion3 *fn = &p->funciones[i];
            for (j = 0; j < fn->num_eventos; j++) {
                ev_ids[n] = fn->eventos[j].id;
                ev_lineas[n] = fn->eventos[j].linea;
                n++;
            }
        }
        if (!validar_ids(ev_ids, ev_lineas, n, "evento", error, tam_error))
            goto fallo;
    }

    /* Ids de bloques. */
    total_bloques = p->num_bloques;
    if (total_bloques > 0) {
        bl_ids = (int *)malloc(sizeof(int) * (size_t)total_bloques);
        bl_lineas = (int *)malloc(sizeof(int) * (size_t)total_bloques);
        if (!bl_ids || !bl_lineas)
            FALLO_VALIDAR(0, "sin memoria para validar bloques");
        for (i = 0; i < total_bloques; i++) {
            bl_ids[i] = p->bloques[i].id;
            bl_lineas[i] = p->bloques[i].linea;
        }
        if (!validar_ids(bl_ids, bl_lineas, total_bloques, "bloque",
                         error, tam_error))
            goto fallo;
    }

    /* Escenas. */
    for (i = 0; i < p->num_escenas; i++) {
        const Escena3 *e = &p->escenas[i];
        if (e->id <= 0)
            FALLO_VALIDAR(e->linea, "id de escena invalido");
        if (e->ancho <= 0 || e->alto <= 0)
            FALLO_VALIDAR(e->linea, "tamano de escena invalido");
    }

    /* Objetos, trajes, variables y eventos. */
    for (i = 0; i < p->num_objetos; i++) {
        const Objeto3 *o = &p->objetos[i];
        if (o->id <= 0)
            FALLO_VALIDAR(o->linea, "id de objeto invalido");
        if (!buscar_escena(p, o->escena))
            FALLO_VALIDAR(o->linea,
                          "objeto %d referencia una escena inexistente: %d",
                          o->id, o->escena);
        if (o->w <= 0 || o->h <= 0)
            FALLO_VALIDAR(o->linea, "tamano de objeto invalido");
        if (o->visible != 0 && o->visible != 1)
            FALLO_VALIDAR(o->linea, "visible debe ser 0 o 1");
        for (j = 0; j < o->num_trajes; j++) {
            const Traje3 *t = &o->trajes[j];
            if (t->indice < 0)
                FALLO_VALIDAR(t->linea, "indice de traje negativo");
            /* 0 significa "valor por defecto" para modelos construidos
             * por codigo; el lector ya los normaliza a 1. */
            if (t->cuadros < 0 || t->cuadros > 1024)
                FALLO_VALIDAR(t->linea, "numero de cuadros fuera de rango");
            if (t->velocidad < 0 || t->velocidad > 3600)
                FALLO_VALIDAR(t->linea, "velocidad de traje fuera de rango");
            if (t->cuadro_ancho < 0 || t->cuadro_alto < 0)
                FALLO_VALIDAR(t->linea, "tamano de cuadro invalido");
            if (t->rotacion != 0 && t->rotacion != 90 &&
                t->rotacion != 180 && t->rotacion != 270)
                FALLO_VALIDAR(t->linea, "rotacion de traje invalida");
        }
        for (j = 0; j < o->num_variables; j++) {
            const Variable3 *v = &o->variables[j];
            if (!validar_tipo(v->tipo, v->linea,
                              "variable de objeto", error, tam_error))
                goto fallo;
            if (!validar_valor_tipo(v->tipo, v->valor, v->linea,
                                    "valor de variable de objeto", error,
                                    tam_error))
                goto fallo;
        }
        for (j = 0; j < o->num_eventos; j++) {
            const Evento3 *ev = &o->eventos[j];
            if (ev->id <= 0)
                FALLO_VALIDAR(ev->linea, "id de evento invalido");
            if (ev->tipo < L3_EVENTO_INICIO || ev->tipo > L3_EVENTO_LLAMADA)
                FALLO_VALIDAR(ev->linea, "tipo de evento invalido");
            if (ev->tipo == L3_EVENTO_LLAMADA)
                FALLO_VALIDAR(ev->linea, "evento llamada fuera de una funcion");
            if (ev->tipo == L3_EVENTO_TECLA &&
                (ev->flanco < 0 || ev->flanco > 2))
                FALLO_VALIDAR(ev->linea, "flanco de tecla invalido");
            if (ev->tipo == L3_EVENTO_TOCAR && ev->objeto != -1 &&
                !buscar_objeto(p, ev->objeto))
                FALLO_VALIDAR(ev->linea,
                              "evento tocar referencia un objeto inexistente: %d",
                              ev->objeto);
            if (ev->tipo == L3_EVENTO_CADA && ev->cada <= 0)
                FALLO_VALIDAR(ev->linea, "evento cada invalido");
            if (ev->tipo == L3_EVENTO_ESCENA &&
                !buscar_escena(p, ev->escena))
                FALLO_VALIDAR(ev->linea,
                              "evento escena referencia una escena inexistente: %d",
                              ev->escena);
        }
    }

    /* Funciones y sus eventos llamada. */
    for (i = 0; i < p->num_funciones; i++) {
        const Funcion3 *fn = &p->funciones[i];
        if (fn->id <= 0)
            FALLO_VALIDAR(fn->linea, "id de funcion invalido");
        for (j = 0; j < fn->num_eventos; j++) {
            const Evento3 *ev = &fn->eventos[j];
            if (ev->id <= 0)
                FALLO_VALIDAR(ev->linea, "id de evento invalido");
            if (ev->tipo != L3_EVENTO_LLAMADA)
                FALLO_VALIDAR(ev->linea, "una funcion solo admite evento llamada");
        }
    }

    /* Referencias y expresiones de los bloques. */
    if (ev_ids && total_eventos > 0)
        qsort(ev_ids, (size_t)total_eventos, sizeof(int), comparar_entero);
    if (bl_ids && total_bloques > 0)
        qsort(bl_ids, (size_t)total_bloques, sizeof(int), comparar_entero);
    for (i = 0; i < p->num_bloques; i++) {
        const Bloque3 *b = &p->bloques[i];
        if (b->id <= 0)
            FALLO_VALIDAR(b->linea, "id de bloque invalido");
        if (b->evento <= 0 || !entero_en(ev_ids, total_eventos, b->evento))
            FALLO_VALIDAR(b->linea,
                          "bloque %d referencia un evento inexistente: %d",
                          b->id, b->evento);
        if (b->padre != 0 && !entero_en(bl_ids, total_bloques, b->padre))
            FALLO_VALIDAR(b->linea,
                          "bloque %d referencia un padre inexistente: %d",
                          b->id, b->padre);
        if (b->padre == b->id)
            FALLO_VALIDAR(b->linea, "un bloque no puede ser su padre");
        if (b->orden < 0)
            FALLO_VALIDAR(b->linea, "orden de bloque negativo");
        if (!b->tipo[0])
            FALLO_VALIDAR(b->linea, "bloque sin tipo");
        if (b->num_campos < 0 || b->num_campos > L3_CAMPOS_MAX)
            FALLO_VALIDAR(b->linea, "numero de campos fuera de rango");
        if (b->num_ranuras < 0 || b->num_ranuras > L3_RANURAS_MAX)
            FALLO_VALIDAR(b->linea, "numero de ranuras fuera de rango");
        if (b->num_hijos < 0 || b->num_hijos > L3_HIJOS_MAX)
            FALLO_VALIDAR(b->linea, "numero de hijos fuera de rango");
        if (strcmp(b->tipo, "codigo.lobo") == 0) {
            const char *codigo = codigo_de_bloque(b);
            if (codigo && codigo[0]) {
                LoboPrograma programa;
                char e[LOBO_ERROR_MAX];
                int ok;
                lobo_programa_iniciar(&programa);
                ok = lobo_compilar(codigo, &programa, LOBO_MODO_VM, e,
                                   sizeof(e));
                lobo_programa_liberar(&programa);
                if (!ok)
                    FALLO_VALIDAR(b->linea, "codigo.lobo no compila: %s", e);
            }
        }
        for (j = 0; j < b->num_campos; j++) {
            if (strcmp(b->tipo, "codigo.lobo") == 0 &&
                strcmp(b->campos[j].clave, "codigo") == 0)
                continue;
            if (!validar_expresion(b->campos[j].valor, b->campos[j].linea,
                                   b->campos[j].clave, error, tam_error))
                goto fallo;
        }
        for (j = 0; j < b->num_ranuras; j++) {
            if (!entero_en(bl_ids, total_bloques, b->ranuras[j].bloque))
                FALLO_VALIDAR(b->ranuras[j].linea,
                              "ranura %s referencia un bloque inexistente: %d",
                              b->ranuras[j].clave, b->ranuras[j].bloque);
        }
        for (j = 0; j < b->num_hijos; j++) {
            if (!entero_en(bl_ids, total_bloques, b->hijos[j]))
                FALLO_VALIDAR(b->linea, "hijo inexistente: %d", b->hijos[j]);
        }
    }

    free(ev_ids);
    free(ev_lineas);
    free(bl_ids);
    free(bl_lineas);
    return 1;

fallo:
    free(ev_ids);
    free(ev_lineas);
    free(bl_ids);
    free(bl_lineas);
    return 0;
#undef FALLO_VALIDAR
}

/* ------------------------------------------------------------------ */
/* Escritor                                                            */
/* ------------------------------------------------------------------ */
static void escribir_evento(Buffer3 *b, const Evento3 *ev) {
    buf_printf(b, "  evento %d %s", ev->id, logic3_evento_texto(ev->tipo));
    switch (ev->tipo) {
        case L3_EVENTO_TECLA:
            buf_printf(b, " ");
            buf_texto(b, ev->nombre);
            buf_printf(b, " %s", logic3_flanco_texto(ev->flanco));
            break;
        case L3_EVENTO_RECIBIR:
            buf_printf(b, " ");
            buf_texto(b, ev->nombre);
            break;
        case L3_EVENTO_TOCAR:
            buf_printf(b, " %d ", ev->objeto);
            buf_valor(b, ev->nombre);
            break;
        case L3_EVENTO_CADA:
            buf_printf(b, " %d", ev->cada);
            break;
        case L3_EVENTO_ESCENA:
            buf_printf(b, " %d", ev->escena);
            break;
        default:
            break;
    }
    buf_printf(b, "\n");
}

static void escribir_bloque(Buffer3 *b, const Bloque3 *bl) {
    int i;
    buf_printf(b, "bloque %d evento %d padre %d orden %d tipo ",
               bl->id, bl->evento, bl->padre, bl->orden);
    buf_texto(b, bl->tipo);
    buf_printf(b, "\n");
    if (strcmp(bl->tipo, "codigo.lobo") == 0 && bl->tiene_codigo) {
        buf_printf(b, "  campo \"codigo\" ");
        buf_texto(b, bl->codigo);
        buf_printf(b, "\n");
    }
    for (i = 0; i < bl->num_campos; i++) {
        if (strcmp(bl->tipo, "codigo.lobo") == 0 && bl->tiene_codigo &&
            strcmp(bl->campos[i].clave, "codigo") == 0)
            continue;
        buf_printf(b, "  campo ");
        buf_texto(b, bl->campos[i].clave);
        buf_printf(b, " ");
        buf_texto(b, bl->campos[i].valor);
        buf_printf(b, "\n");
    }
    for (i = 0; i < bl->num_ranuras; i++) {
        buf_printf(b, "  ranura ");
        buf_texto(b, bl->ranuras[i].clave);
        buf_printf(b, " %d\n", bl->ranuras[i].bloque);
    }
    for (i = 0; i < bl->num_hijos; i++)
        buf_printf(b, "  hijo %d\n", bl->hijos[i]);
}

/* Los enteros y booleanos se escriben en forma canonica para que guardar
 * dos veces seguidas produzca exactamente el mismo archivo. */
static const char *valor_canonico(int tipo, const char *valor,
                                  char *dst, int tam) {
    int n;
    if (valor && (tipo == L3_TIPO_ENTERO || tipo == L3_TIPO_BOOLEANO) &&
        a_entero(valor, &n)) {
        snprintf(dst, (size_t)tam, "%d", n);
        return dst;
    }
    copiar_texto(dst, tam, valor ? valor : "");
    return dst;
}

static int serializar_logic3(const Proyecto3 *p, Buffer3 *b) {
    int i, j;
    if (!p || !b) return 0;
    buf_printf(b, "LOGIC3\n");
    buf_printf(b, "version %d\n", L3_VERSION);
    buf_printf(b, "proyecto ");
    buf_texto(b, p->nombre);
    buf_printf(b, " %d %d %d %d %d %08X\n",
               p->ancho, p->alto, p->limitar_cuadros, p->fps,
               p->velocidad, (unsigned int)p->fondo);
    buf_printf(b, "carpetas ");
    for (i = 0; i < 4; i++) {
        if (i) buf_printf(b, " ");
        buf_texto(b, p->carpetas[i]);
    }
    buf_printf(b, "\n");
    /* La camara es opcional: no se escribe si esta en los valores por
     * defecto, para no cambiar el CRC de los proyectos antiguos. */
    if (p->camara.objetivo || p->camara.x || p->camara.y ||
        p->camara.ancho || p->camara.alto || p->camara.zoom != 1) {
        buf_printf(b, "camara");
        if (p->camara.objetivo)
            buf_printf(b, " objetivo %d", p->camara.objetivo);
        if (p->camara.x || p->camara.y || p->camara.ancho || p->camara.alto)
            buf_printf(b, " limites %d %d %d %d", p->camara.x, p->camara.y,
                       p->camara.ancho, p->camara.alto);
        if (p->camara.zoom != 1)
            buf_printf(b, " zoom %d", p->camara.zoom);
        buf_printf(b, "\n");
    }
    for (i = 0; i < p->num_variables; i++) {
        const Variable3 *v = &p->variables[i];
        char canon[L3_TEXTO_MAX];
        buf_printf(b, "variable global ");
        buf_texto(b, v->nombre);
        buf_printf(b, " %s ", logic3_tipo_texto(v->tipo));
        buf_valor(b, valor_canonico(v->tipo, v->valor, canon, sizeof(canon)));
        buf_printf(b, "\n");
    }
    for (i = 0; i < p->num_listas; i++) {
        const Lista3 *li = &p->listas[i];
        buf_printf(b, "lista global ");
        buf_texto(b, li->nombre);
        buf_printf(b, " %d %s", li->capacidad, logic3_tipo_texto(li->tipo));
        for (j = 0; j < li->num_elementos; j++) {
            char canon[L3_TEXTO_MAX];
            buf_printf(b, " ");
            buf_valor(b, valor_canonico(li->tipo, li->elementos[j], canon,
                                        sizeof(canon)));
        }
        buf_printf(b, "\n");
    }
    for (i = 0; i < p->num_escenas; i++) {
        const Escena3 *e = &p->escenas[i];
        buf_printf(b, "escena %d ", e->id);
        buf_texto(b, e->nombre);
        buf_printf(b, " ");
        buf_texto(b, e->mapa);
        buf_printf(b, " %d %d %08X\n", e->ancho, e->alto, (unsigned int)e->fondo);
    }
    for (i = 0; i < p->num_objetos; i++) {
        Objeto3 *o = &p->objetos[i];
        buf_printf(b, "objeto %d ", o->id);
        buf_texto(b, o->nombre);
        buf_printf(b, " escena %d x %d y %d w %d h %d visible %d capa %d\n",
                   o->escena, o->x, o->y, o->w, o->h, o->visible ? 1 : 0, o->capa);
        for (j = 0; j < o->num_trajes; j++) {
            const Traje3 *t = &o->trajes[j];
            buf_printf(b, "  traje %d ", t->indice);
            buf_texto(b, t->imagen);
            buf_printf(b, " %d %d %d %d", t->ancho, t->alto,
                       t->pivote_x, t->pivote_y);
            if (t->cuadros > 1) buf_printf(b, " cuadros %d", t->cuadros);
            if (t->cuadro_ancho) buf_printf(b, " cuadro_ancho %d", t->cuadro_ancho);
            if (t->cuadro_alto) buf_printf(b, " cuadro_alto %d", t->cuadro_alto);
            if (t->flip_x) buf_printf(b, " flip_x 1");
            if (t->flip_y) buf_printf(b, " flip_y 1");
            if (t->rotacion) buf_printf(b, " rotacion %d", t->rotacion);
            if (t->velocidad > 1) buf_printf(b, " velocidad %d", t->velocidad);
            buf_printf(b, "\n");
        }
        for (j = 0; j < o->num_variables; j++) {
            const Variable3 *v = &o->variables[j];
            char canon[L3_TEXTO_MAX];
            buf_printf(b, "  variable ");
            buf_texto(b, v->nombre);
            buf_printf(b, " %s ", logic3_tipo_texto(v->tipo));
            buf_valor(b, valor_canonico(v->tipo, v->valor, canon, sizeof(canon)));
            buf_printf(b, "\n");
        }
        for (j = 0; j < o->num_eventos; j++) escribir_evento(b, &o->eventos[j]);
    }
    for (i = 0; i < p->num_funciones; i++) {
        const Funcion3 *fn = &p->funciones[i];
        buf_printf(b, "funcion %d ", fn->id);
        buf_texto(b, fn->nombre);
        buf_printf(b, " %d %d\n", fn->parametros, fn->retorno);
        for (j = 0; j < fn->num_eventos; j++) escribir_evento(b, &fn->eventos[j]);
    }
    for (i = 0; i < p->num_bloques; i++) escribir_bloque(b, &p->bloques[i]);
    buf_printf(b, "FIN\n");
    if (!b->error)
        buf_printf(b, "CRC32 %08X\n", logic3_crc32(b->datos, b->largo));
    return b->error ? 0 : 1;
}

int logic3_guardar(const Proyecto3 *p, const char *ruta, char *error, int tam_error) {
    Buffer3 b;
    FILE *f;
    int ok;
    if (error && tam_error > 0) error[0] = 0;
    if (!logic3_validar(p, error, tam_error)) return 0;
    memset(&b, 0, sizeof(b));
    if (!buf_reservar(&b, 4096)) {
        return fallo3(error, tam_error, 0, "sin memoria para escribir");
    }
    if (!serializar_logic3(p, &b)) {
        free(b.datos);
        return fallo3(error, tam_error, 0, "no se pudo serializar el proyecto");
    }
    f = fopen(ruta, "wb");
    if (!f) {
        free(b.datos);
        return fallo3(error, tam_error, 0, "no se pudo crear el archivo");
    }
    ok = fwrite(b.datos, 1, (size_t)b.largo, f) == (size_t)b.largo;
    fclose(f);
    free(b.datos);
    if (!ok) return fallo3(error, tam_error, 0, "no se pudo escribir el archivo");
    return 1;
}

/* ------------------------------------------------------------------ */
/* Migracion LOGIC2 <-> LOGIC3                                         */
/* ------------------------------------------------------------------ */
int logic3_desde_logic2(Proyecto3 *p, const AjustesProyecto *ajustes,
                        const ModuloElemento *modulos, int num_modulos,
                        const FuncionCodigo *funciones, int num_funciones,
                        const int *orden, int num_orden) {
    int i, k;
    const char *mapa = "";
    Escena3 *escena;
    Objeto3 *objeto;
    Evento3 *evento;
    int siguiente_bloque = 1;
    int orden_bloque = 0;

    if (!p) return 0;
    logic3_iniciar(p);
    if (ajustes) {
        copiar_texto(p->nombre, sizeof(p->nombre), ajustes->nombre);
        p->ancho = ajustes->ancho > 0 ? ajustes->ancho : 640;
        p->alto = ajustes->alto > 0 ? ajustes->alto : 480;
        p->limitar_cuadros = ajustes->limitar_cuadros ? 1 : 0;
        p->fps = ajustes->fps > 0 ? ajustes->fps : 60;
        p->velocidad = ajustes->velocidad;
        p->fondo = ajustes->fondo;
    }
    if (num_modulos > 0 && modulos) {
        for (i = 0; i < num_modulos; i++) {
            if (modulos[i].funcion < 0 && modulos[i].mapa[0]) {
                mapa = modulos[i].mapa;
                break;
            }
        }
    }

    escena = &p->escenas[0];
    escena->id = 1;
    copiar_texto(escena->nombre, sizeof(escena->nombre), "Escena 1");
    copiar_texto(escena->mapa, sizeof(escena->mapa), mapa);
    escena->ancho = p->ancho;
    escena->alto = p->alto;
    escena->fondo = p->fondo;
    escena->linea = 0;
    p->num_escenas = 1;

    if (!objetos_reservar(p)) return 0;
    objeto = &p->objetos[0];
    memset(objeto, 0, sizeof(*objeto));
    objeto->id = 1;
    copiar_texto(objeto->nombre, sizeof(objeto->nombre), "Escena");
    objeto->escena = 1;
    objeto->x = 0;
    objeto->y = 0;
    objeto->w = p->ancho;
    objeto->h = p->alto;
    objeto->visible = 1;
    objeto->capa = 0;
    objeto->num_trajes = 1;
    objeto->trajes[0].indice = 0;
    objeto->trajes[0].imagen[0] = 0;
    objeto->trajes[0].ancho = 0;
    objeto->trajes[0].alto = 0;
    objeto->trajes[0].pivote_x = 0;
    objeto->trajes[0].pivote_y = 0;
    /* Valores por defecto de la fase 30: un cuadro y un fotograma por
     * cuadro, para que el archivo convertido siga siendo canonico. */
    objeto->trajes[0].cuadros = 1;
    objeto->trajes[0].velocidad = 1;
    objeto->num_eventos = 1;
    evento = &objeto->eventos[0];
    memset(evento, 0, sizeof(*evento));
    evento->id = 1;
    evento->tipo = L3_EVENTO_FOTOGRAMA;
    evento->flanco = L3_FLANCO_MANTENER;
    evento->objeto = -1;
    p->num_objetos = 1;

    /* Un bloque codigo.lobo por cada funcion usada, en el orden de
     * ejecucion del gestor (o en el orden del array si no se pasa). */
    if (orden && num_orden > 0) {
        for (k = 0; k < num_orden; k++) {
            int idx = orden[k];
            int id_func, fi;
            const FuncionCodigo *fn = NULL;
            Bloque3 *b;
            if (idx < 0 || idx >= num_modulos || !modulos) continue;
            id_func = modulos[idx].funcion;
            if (id_func < 1) continue;
            for (fi = 0; fi < num_funciones; fi++) {
                if (funciones[fi].id == id_func) { fn = &funciones[fi]; break; }
            }
            if (!fn) continue;
            if (!bloques_reservar(p, 1)) return 0;
            b = &p->bloques[p->num_bloques];
            memset(b, 0, sizeof(*b));
            b->id = siguiente_bloque++;
            b->evento = 1;
            b->padre = 0;
            b->orden = orden_bloque++;
            copiar_texto(b->tipo, sizeof(b->tipo), "codigo.lobo");
            {
                int largo = fn->largo;
                if (largo < 0) largo = 0;
                if (largo >= FUNCIONES_CODIGO_MAX) largo = FUNCIONES_CODIGO_MAX - 1;
                memcpy(b->codigo, fn->codigo, (size_t)largo);
                b->codigo[largo] = 0;
                b->tiene_codigo = 1;
            }
            p->num_bloques++;
        }
    } else if (funciones && num_funciones > 0) {
        for (i = 0; i < num_funciones; i++) {
            const FuncionCodigo *fn = &funciones[i];
            Bloque3 *b;
            if (!bloques_reservar(p, 1)) return 0;
            b = &p->bloques[p->num_bloques];
            memset(b, 0, sizeof(*b));
            b->id = siguiente_bloque++;
            b->evento = 1;
            b->padre = 0;
            b->orden = orden_bloque++;
            copiar_texto(b->tipo, sizeof(b->tipo), "codigo.lobo");
            {
                int largo = fn->largo;
                if (largo < 0) largo = 0;
                if (largo >= FUNCIONES_CODIGO_MAX) largo = FUNCIONES_CODIGO_MAX - 1;
                memcpy(b->codigo, fn->codigo, (size_t)largo);
                b->codigo[largo] = 0;
                b->tiene_codigo = 1;
            }
            p->num_bloques++;
        }
    }
    return 1;
}

static int elemento_antes_logic2(const ModuloElemento *a,
                                 const ModuloElemento *b) {
    if (a->y != b->y) return a->y < b->y;
    if (a->x != b->x) return a->x < b->x;
    return a->id < b->id;
}

int logic3_a_logic2(const Proyecto3 *p, AjustesProyecto *ajustes,
                    ModuloElemento *modulos, int *num_modulos, int max_modulos,
                    FuncionCodigo *funciones, int *num_funciones, int max_funciones,
                    char *error, int tam_error) {
    int i, ncod = 0, items, total;

    if (error && tam_error > 0) error[0] = 0;
    if (!p || !ajustes || !modulos || !num_modulos || !funciones || !num_funciones)
        return fallo3(error, tam_error, 0, "argumentos nulos");
    if (max_modulos < 0 || max_funciones < 0)
        return fallo3(error, tam_error, 0, "limites invalidos");
    if (!logic3_validar(p, error, tam_error)) return 0;

    for (i = 0; i < p->num_bloques; i++) {
        if (strcmp(p->bloques[i].tipo, "codigo.lobo") == 0) ncod++;
    }
    items = p->num_escenas;
    total = items + ncod;
    if (items > max_modulos)
        return fallo3(error, tam_error, 0,
                      "no caben %d escenas en %d elementos", items, max_modulos);
    if (ncod > max_funciones)
        return fallo3(error, tam_error, 0,
                      "no caben %d bloques codigo.lobo en %d funciones",
                      ncod, max_funciones);
    if (total > max_modulos)
        return fallo3(error, tam_error, 0,
                      "no caben %d elementos en el gestor (maximo %d)",
                      total, max_modulos);

    ajustes->nombre[0] = 0;
    copiar_texto(ajustes->nombre, sizeof(ajustes->nombre), p->nombre);
    ajustes->ancho = p->ancho;
    ajustes->alto = p->alto;
    ajustes->limitar_cuadros = p->limitar_cuadros ? 1 : 0;
    ajustes->fps = p->fps;
    ajustes->velocidad = p->velocidad;
    ajustes->fondo = p->fondo;

    memset(modulos, 0, sizeof(ModuloElemento) * (size_t)max_modulos);
    memset(funciones, 0, sizeof(FuncionCodigo) * (size_t)max_funciones);

    for (i = 0; i < p->num_escenas; i++) {
        ModuloElemento *m = &modulos[i];
        m->x = 0;
        m->y = i * 80;
        m->ancho = MODULOS_ELEMENTO_ANCHO;
        m->alto = MODULOS_ELEMENTO_ALTO;
        m->id = i + 1;
        m->plegado = 0;
        copiar_texto(m->mapa, sizeof(m->mapa), p->escenas[i].mapa);
        m->funcion = -1;
    }

    ncod = 0;
    for (i = 0; i < p->num_bloques; i++) {
        const Bloque3 *b = &p->bloques[i];
        FuncionCodigo *f;
        ModuloElemento *m;
        const char *codigo;
        int largo;
        if (strcmp(b->tipo, "codigo.lobo") != 0) continue;
        codigo = codigo_de_bloque(b);
        f = &funciones[ncod];
        memset(f, 0, sizeof(*f));
        f->id = ncod + 1;
        snprintf(f->nombre, sizeof(f->nombre), "Codigo %d", ncod + 1);
        largo = (int)strlen(codigo);
        if (largo >= FUNCIONES_CODIGO_MAX) largo = FUNCIONES_CODIGO_MAX - 1;
        memcpy(f->codigo, codigo, (size_t)largo);
        f->codigo[largo] = 0;
        f->largo = largo;
        f->cursor = 0;
        f->scroll = 0;
        funciones_codigo_validar(f);

        m = &modulos[items + ncod];
        m->x = 200;
        m->y = ncod * 80;
        m->ancho = MODULOS_ELEMENTO_ANCHO;
        m->alto = MODULOS_ELEMENTO_ALTO;
        m->id = items + ncod + 1;
        m->plegado = 0;
        m->mapa[0] = 0;
        m->funcion = f->id;
        ncod++;
    }

    *num_modulos = total;
    *num_funciones = ncod;
    return 1;
}

/* Serializa el modelo como LOGIC2 en memoria (exportacion de emergencia). */
static int serializar_logic2(const AjustesProyecto *ajustes,
                             const ModuloElemento *modulos, int num_modulos,
                             const FuncionCodigo *funciones, int num_funciones,
                             Buffer3 *b) {
    int orden[MODULOS_MAX];
    int norden = 0;
    int i, j;
    char linea[256];

    buf_printf(b, "LOGIC2\n");
    buf_printf(b, "0 0\n");
    buf_printf(b, "%d\n", num_modulos);
    for (i = 0; i < num_modulos; i++) {
        const ModuloElemento *m = &modulos[i];
        if (m->funcion >= 0) continue;
        buf_printf(b, "%d %d %d %d %d %d \"%s\" %d\n",
                   m->x, m->y, m->ancho, m->alto, m->id, m->plegado,
                   m->mapa, m->funcion);
    }
    for (i = 0; i < num_modulos; i++) {
        if (modulos[i].funcion < 0) continue;
        j = norden++;
        while (j > 0 && elemento_antes_logic2(&modulos[i], &modulos[orden[j - 1]])) {
            orden[j] = orden[j - 1];
            j--;
        }
        orden[j] = i;
    }
    for (i = 0; i < norden; i++) {
        const ModuloElemento *m = &modulos[orden[i]];
        buf_printf(b, "%d %d %d %d %d %d \"%s\" %d\n",
                   m->x, m->y, m->ancho, m->alto, m->id, m->plegado,
                   m->mapa, m->funcion);
    }
    buf_printf(b, "%d\n", num_funciones);
    for (i = 0; i < num_funciones; i++) {
        const FuncionCodigo *f = &funciones[i];
        buf_printf(b, "%d \"%s\" %d\n", f->id, f->nombre, f->largo);
        if (f->largo > 0) buf_printf(b, "%.*s", f->largo, f->codigo);
        buf_printf(b, "\n");
    }
    snprintf(linea, sizeof(linea),
             "PROYECTO \"%s\" %d %d %d %d %d %08X\n",
             ajustes->nombre, ajustes->ancho, ajustes->alto,
             ajustes->limitar_cuadros, ajustes->fps, ajustes->velocidad,
             (unsigned int)ajustes->fondo);
    buf_printf(b, "%s", linea);
    return b->error ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* Integracion con el editor                                           */
/* ------------------------------------------------------------------ */
void logic3_olvidar(void) {
    logic3_liberar(&g3);
    memset(&g3, 0, sizeof(g3));
    g3_activo = 0;
    g3_error[0] = 0;
}

int logic3_activo(void) {
    return g3_activo;
}

Proyecto3 *logic3_modelo_activo(void) {
    return g3_activo ? &g3 : NULL;
}

int logic3_nuevo_editor(void) {
    if (g3_activo) return 1;
    logic3_olvidar();
    logic3_iniciar(&g3);
    if (g.ajustes.nombre[0])
        copiar_texto(g3.nombre, sizeof(g3.nombre), g.ajustes.nombre);
    if (g.ajustes.ancho > 0) g3.ancho = g.ajustes.ancho;
    if (g.ajustes.alto > 0) g3.alto = g.ajustes.alto;
    g3.limitar_cuadros = g.ajustes.limitar_cuadros ? 1 : 0;
    if (g.ajustes.fps > 0) g3.fps = g.ajustes.fps;
    if (g.ajustes.velocidad > 0) g3.velocidad = g.ajustes.velocidad;
    if (g.ajustes.fondo) g3.fondo = g.ajustes.fondo;
    g3_activo = 1;
    g3_error[0] = 0;
    return 1;
}

const char *logic3_ultimo_error(void) {
    return g3_error;
}

void logic3_contar(int *escenas, int *objetos, int *eventos,
                   int *bloques, int *variables) {
    int e = 0, o = 0, ev = 0, b = 0, v = 0;
    int i;
    if (g3_activo) {
        e = g3.num_escenas;
        o = g3.num_objetos;
        b = g3.num_bloques;
        v = g3.num_variables;
        for (i = 0; i < g3.num_objetos; i++) {
            ev += g3.objetos[i].num_eventos;
            v += g3.objetos[i].num_variables;
        }
        for (i = 0; i < g3.num_funciones; i++)
            ev += g3.funciones[i].num_eventos;
    }
    if (escenas) *escenas = e;
    if (objetos) *objetos = o;
    if (eventos) *eventos = ev;
    if (bloques) *bloques = b;
    if (variables) *variables = v;
}

int logic3_cargar_editor(const char *ruta) {
    Proyecto3 tmp;
    char error[L3_ERROR_MAX];
    int i, max_id;

    logic3_iniciar(&tmp);
    if (!logic3_cargar(&tmp, ruta, error, sizeof(error))) {
        logic3_liberar(&tmp);
        copiar_texto(g3_error, sizeof(g3_error), error);
        return 0;
    }

    logic3_olvidar();
    logica_iniciar();
    g3 = tmp;
    g3_activo = 1;
    g3_error[0] = 0;
    /* La seleccion y el scroll del estudio se recuerdan por archivo, sin
     * escribirlos en LOGIC3: el formato y su CRC quedan intactos. */
    estudio_proyecto_cambiar(ruta);

    /* Cabecera del proyecto. */
    copiar_texto(g.ajustes.nombre, sizeof(g.ajustes.nombre), g3.nombre);
    g.ajustes.ancho = g3.ancho;
    g.ajustes.alto = g3.alto;
    g.ajustes.limitar_cuadros = g3.limitar_cuadros ? 1 : 0;
    g.ajustes.fps = g3.fps;
    g.ajustes.velocidad = g3.velocidad;
    g.ajustes.fondo = g3.fondo;

    /* Vista clasica: mapas por escena y bloques codigo.lobo como
     * funciones. Si no cabe, el modelo sigue activo y la vista queda
     * vacia; el archivo se guarda desde el modelo, no desde la vista. */
    {
        AjustesProyecto aj = g.ajustes;
        int nm = 0, nf = 0;
        char e2[L3_ERROR_MAX];
        if (logic3_a_logic2(&g3, &aj, g.modulos, &nm, MODULOS_MAX,
                            g.funciones, &nf, FUNCIONES_MAX, e2, sizeof(e2))) {
            g.num_modulos = nm;
            g.num_funciones = nf;
            g.ajustes = aj;
        } else {
            g.num_modulos = 0;
            g.num_funciones = 0;
        }
    }

    max_id = 0;
    for (i = 0; i < g.num_modulos; i++)
        if (g.modulos[i].id > max_id) max_id = g.modulos[i].id;
    g.modulo_siguiente_id = max_id + 1;
    max_id = 0;
    for (i = 0; i < g.num_funciones; i++)
        if (g.funciones[i].id > max_id) max_id = g.funciones[i].id;
    g.funcion_siguiente_id = max_id + 1;
    app_menus_resolucion();
    return 1;
}

int logic3_guardar_actual(const char *ruta) {
    char error[L3_ERROR_MAX];
    if (!g3_activo || !ruta) {
        copiar_texto(g3_error, sizeof(g3_error), "no hay un proyecto LOGIC3 activo");
        return 0;
    }
    /* La cabecera manda en el editor: si el usuario cambio AJUSTES DEL
     * PROYECTO, el archivo guardado refleja esos valores. */
    copiar_texto(g3.nombre, sizeof(g3.nombre), g.ajustes.nombre);
    g3.ancho = g.ajustes.ancho;
    g3.alto = g.ajustes.alto;
    g3.limitar_cuadros = g.ajustes.limitar_cuadros ? 1 : 0;
    g3.fps = g.ajustes.fps;
    g3.velocidad = g.ajustes.velocidad;
    g3.fondo = g.ajustes.fondo;
    g3.version = L3_VERSION;
    if (!logic3_guardar(&g3, ruta, error, sizeof(error))) {
        copiar_texto(g3_error, sizeof(g3_error), error);
        return 0;
    }
    g3_error[0] = 0;
    estudio_proyecto_guardar(ruta);
    estudio_guardar_estado();
    return 1;
}

int logic3_exportar_logic2(const char *ruta) {
    ModuloElemento modulos[MODULOS_MAX];
    FuncionCodigo funciones[FUNCIONES_MAX];
    AjustesProyecto aj;
    int nm = 0, nf = 0;
    char error[L3_ERROR_MAX];
    Buffer3 b;
    FILE *f;
    int ok;

    if (!g3_activo || !ruta) {
        copiar_texto(g3_error, sizeof(g3_error), "no hay un proyecto LOGIC3 activo");
        return 0;
    }
    memset(&aj, 0, sizeof(aj));
    if (!logic3_a_logic2(&g3, &aj, modulos, &nm, MODULOS_MAX,
                         funciones, &nf, FUNCIONES_MAX, error, sizeof(error))) {
        copiar_texto(g3_error, sizeof(g3_error), error);
        return 0;
    }
    memset(&b, 0, sizeof(b));
    if (!buf_reservar(&b, 4096) ||
        !serializar_logic2(&aj, modulos, nm, funciones, nf, &b)) {
        free(b.datos);
        copiar_texto(g3_error, sizeof(g3_error), "sin memoria para exportar");
        return 0;
    }
    f = fopen(ruta, "wb");
    if (!f) {
        free(b.datos);
        copiar_texto(g3_error, sizeof(g3_error), "no se pudo crear el archivo LOGIC2");
        return 0;
    }
    ok = fwrite(b.datos, 1, (size_t)b.largo, f) == (size_t)b.largo;
    fclose(f);
    free(b.datos);
    if (!ok) {
        copiar_texto(g3_error, sizeof(g3_error), "no se pudo escribir el archivo LOGIC2");
        return 0;
    }
    g3_error[0] = 0;
    return 1;
}

int logic3_convertir_logic2(const char *ruta) {
    Proyecto3 tmp;
    char salida[MAX_PATH];
    char error[L3_ERROR_MAX];
    char cabecera[16];
    FILE *f;
    int ok, norden;
    int orden[MODULOS_MAX];

    if (!ruta) return 0;
    f = fopen(ruta, "rb");
    if (!f) {
        copiar_texto(g3_error, sizeof(g3_error), "no se pudo abrir el archivo LOGIC2");
        return 0;
    }
    cabecera[0] = 0;
    if (!fgets(cabecera, sizeof(cabecera), f)) {
        fclose(f);
        copiar_texto(g3_error, sizeof(g3_error), "el archivo esta vacio");
        return 0;
    }
    fclose(f);
    if (strncmp(cabecera, "LOGIC3", 6) == 0) {
        copiar_texto(g3_error, sizeof(g3_error), "el archivo ya es LOGIC3");
        return 0;
    }
    if (strncmp(cabecera, "LOGIC1", 6) != 0 &&
        strncmp(cabecera, "LOGIC2", 6) != 0) {
        copiar_texto(g3_error, sizeof(g3_error), "el archivo no es LOGIC1 ni LOGIC2");
        return 0;
    }
    if (!logica_cargar_archivo(ruta)) {
        copiar_texto(g3_error, sizeof(g3_error), "no se pudo cargar el proyecto clasico");
        return 0;
    }
    if ((int)strlen(ruta) + 8 >= MAX_PATH) {
        copiar_texto(g3_error, sizeof(g3_error), "la ruta de salida es demasiado larga");
        return 0;
    }
    snprintf(salida, sizeof(salida), "%s.logic3", ruta);

    logic3_iniciar(&tmp);
    norden = modulos_orden_ejecucion(orden);
    if (!logic3_desde_logic2(&tmp, &g.ajustes, g.modulos, g.num_modulos,
                             g.funciones, g.num_funciones, orden, norden)) {
        logic3_liberar(&tmp);
        copiar_texto(g3_error, sizeof(g3_error), "no se pudo convertir el proyecto");
        return 0;
    }
    ok = logic3_guardar(&tmp, salida, error, sizeof(error));
    if (!ok) {
        logic3_liberar(&tmp);
        copiar_texto(g3_error, sizeof(g3_error), error);
        return 0;
    }
    logic3_olvidar();
    g3 = tmp;
    g3_activo = 1;
    g3_error[0] = 0;
    return 1;
}
