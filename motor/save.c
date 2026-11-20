/* save.c - guardado, records y persistencia (fase 40).
 *
 * Tabla de claves enteras por proyecto. El archivo es texto, con una
 * cabecera y una linea `clave valor` por entrada:
 *
 *   lw2ksave 1
 *   puntos 120
 *   record.puntos 500
 *   ajuste.volumen 70
 *
 * Escritura atomica: primero el `.tmp` y despues MoveFileEx con
 * MOVEFILE_REPLACE_EXISTING. Si el definitivo no se puede leer (corte de
 * luz a mitad de la copia) se prueba el temporal y, si tampoco, el juego
 * sigue con la tabla vacia. Ningun error de disco impide arrancar.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "save.h"

typedef struct SaveEntrada {
    char clave[SAVE_CLAVE_MAX];
    int valor;
} SaveEntrada;

static SaveEntrada g_entradas[SAVE_CLAVES_MAX];
static int g_num;
static int g_activo;
static int g_cargada;
static int g_recuperado;
static char g_nombre[64];
static char g_ruta_proyecto[MAX_PATH];
static char g_directorio[MAX_PATH];
static char g_ruta[MAX_PATH];
static char g_temporal[MAX_PATH];

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */

static int existe(const char *ruta) {
    FILE *f = fopen(ruta, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int clave_valida(const char *clave) {
    int i;
    if (!clave || !clave[0]) return 0;
    for (i = 0; clave[i]; i++) {
        char c = clave[i];
        int bien = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '.' ||
                   c == '-';
        if (!bien) return 0;
    }
    return i < SAVE_CLAVE_MAX;
}

static int buscar(const char *clave) {
    int i;
    for (i = 0; i < g_num; i++)
        if (strcmp(g_entradas[i].clave, clave) == 0) return i;
    return -1;
}

static int poner(const char *clave, int valor) {
    int i = buscar(clave);
    if (i >= 0) {
        g_entradas[i].valor = valor;
        return 1;
    }
    if (!clave_valida(clave) || g_num >= SAVE_CLAVES_MAX) return 0;
    strcpy(g_entradas[g_num].clave, clave);
    g_entradas[g_num].valor = valor;
    g_num++;
    return 1;
}

/* El nombre del save es el del proyecto sin extension ni caracteres
 * raros, asi el mismo nombre sirve en Windows y en el kernel. */
static void derivar_nombre(const char *ruta) {
    const char *base = ruta;
    const char *p;
    char *q = g_nombre;
    char *fin = g_nombre + sizeof(g_nombre) - 1;

    if (!ruta || !ruta[0]) {
        strcpy(g_nombre, "juego");
        return;
    }
    for (p = ruta; *p; p++)
        if (*p == '\\' || *p == '/') base = p + 1;
    for (p = base; *p && *p != '.'; p++) {
        char c = *p;
        int bien = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (q < fin) *q++ = bien ? c : '_';
    }
    *q = '\0';
    if (!g_nombre[0]) strcpy(g_nombre, "juego");
}

static void derivar_rutas(void) {
    char carpeta[MAX_PATH];

    if (g_directorio[0]) {
        strcpy(carpeta, g_directorio);
    } else if (g_ruta_proyecto[0]) {
        char *ultimo = NULL;
        char *p;
        strcpy(carpeta, g_ruta_proyecto);
        for (p = carpeta; *p; p++)
            if (*p == '\\' || *p == '/') ultimo = p;
        if (ultimo) {
            *ultimo = '\0';
            _snprintf(carpeta + strlen(carpeta),
                      sizeof(carpeta) - strlen(carpeta), "\\saves");
        } else {
            strcpy(carpeta, "saves");
        }
    } else {
        strcpy(carpeta, "saves");
    }
    carpeta[sizeof(carpeta) - 1] = '\0';
    _snprintf(g_ruta, sizeof(g_ruta), "%s\\%s.sav", carpeta, g_nombre);
    g_ruta[sizeof(g_ruta) - 1] = '\0';
    _snprintf(g_temporal, sizeof(g_temporal), "%s.tmp", g_ruta);
    g_temporal[sizeof(g_temporal) - 1] = '\0';
}

/* ------------------------------------------------------------------ */
/* Disco                                                               */
/* ------------------------------------------------------------------ */

/* Lee un archivo de save. Devuelve 1 solo si la cabecera y TODAS las
 * lineas son validas: un save a medias se descarta por completo. */
static int leer_archivo(const char *ruta, SaveEntrada *dst, int *num) {
    char linea[SAVE_CLAVE_MAX + 64];
    char clave[SAVE_CLAVE_MAX];
    int valor;
    FILE *f = fopen(ruta, "rt");
    int n = 0;
    int correcto = 1;

    if (!f) return 0;
    if (!fgets(linea, sizeof(linea), f) ||
        strncmp(linea, "lw2ksave 1", 10) != 0)
        correcto = 0;
    while (correcto && fgets(linea, sizeof(linea), f)) {
        if (linea[0] == '\n' || linea[0] == '\r') continue;
        if (sscanf(linea, "%47s %d", clave, &valor) != 2 ||
            !clave_valida(clave) || n >= SAVE_CLAVES_MAX) {
            correcto = 0;
            break;
        }
        strcpy(dst[n].clave, clave);
        dst[n].valor = valor;
        n++;
    }
    fclose(f);
    if (!correcto) return 0;
    *num = n;
    return 1;
}

/* Carga la tabla una vez por proceso. Primero el definitivo; si esta
 * corrupto se recupera del temporal, que es el que quedo intacto tras un
 * corte durante el reemplazo. */
static void cargar_tabla(void) {
    SaveEntrada temporal[SAVE_CLAVES_MAX];
    int num = 0;

    if (g_cargada) return;
    g_cargada = 1;
    g_num = 0;
    g_recuperado = 0;
    derivar_rutas();
    if (leer_archivo(g_ruta, g_entradas, &g_num)) return;
    if (leer_archivo(g_temporal, temporal, &num)) {
        memcpy(g_entradas, temporal, sizeof(SaveEntrada) * (size_t)num);
        g_num = num;
        g_recuperado = 1;
    }
}

static int escribir_tabla(void) {
    char buffer[SAVE_LIMITE + SAVE_CLAVE_MAX + 64];
    int largo;
    int i;
    FILE *f;
    char tmp[MAX_PATH];

    derivar_rutas();
    largo = _snprintf(buffer, sizeof(buffer), "lw2ksave 1\n");
    for (i = 0; i < g_num; i++) {
        int n = _snprintf(buffer + largo, sizeof(buffer) - (size_t)largo,
                          "%s %d\n", g_entradas[i].clave,
                          g_entradas[i].valor);
        if (n < 0 || largo + n >= (int)sizeof(buffer)) return 0;
        largo += n;
    }
    if (largo > SAVE_LIMITE) return 0;
    f = fopen(g_temporal, "wb");
    if (!f) return 0;
    if (fwrite(buffer, 1, (size_t)largo, f) != (size_t)largo) {
        fclose(f);
        return 0;
    }
    if (fclose(f) != 0) return 0;
    strcpy(tmp, g_temporal);
    if (!MoveFileExA(tmp, g_ruta,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        /* En sistemas sin MoveFileEx el rename de C tambien sirve. */
        if (rename(tmp, g_ruta) != 0) return 0;
    }
    g_recuperado = 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* API publica                                                          */
/* ------------------------------------------------------------------ */

void motor_save_limpiar(void) {
    g_num = 0;
    g_activo = 0;
    g_cargada = 0;
    g_recuperado = 0;
    g_nombre[0] = '\0';
    g_ruta_proyecto[0] = '\0';
    g_directorio[0] = '\0';
    g_ruta[0] = '\0';
    g_temporal[0] = '\0';
}

void motor_save_activar(int valor) { g_activo = valor != 0; }
int motor_save_esta_activo(void) { return g_activo; }

void motor_save_directorio(const char *ruta) {
    if (!ruta || !ruta[0]) return;
    strcpy(g_directorio, ruta);
    g_directorio[sizeof(g_directorio) - 1] = '\0';
    g_cargada = 0;
    g_num = 0;
}

void motor_save_base(const char *ruta_proyecto) {
    if (!ruta_proyecto || !ruta_proyecto[0]) return;
    strcpy(g_ruta_proyecto, ruta_proyecto);
    g_ruta_proyecto[sizeof(g_ruta_proyecto) - 1] = '\0';
    derivar_nombre(ruta_proyecto);
    g_cargada = 0;
    g_num = 0;
}

int motor_save_guardar(const char *clave, int valor) {
    if (!g_activo || !clave_valida(clave)) return 0;
    cargar_tabla();
    if (!poner(clave, valor)) return 0;
    if (!escribir_tabla()) return 0;
    return 1;
}

int motor_save_cargar(const char *clave) {
    int i;
    if (!g_activo || !clave_valida(clave)) return 0;
    cargar_tabla();
    i = buscar(clave);
    return i >= 0 ? g_entradas[i].valor : 0;
}

int motor_save_borrar(void) {
    if (!g_activo) return 0;
    cargar_tabla();
    g_num = 0;
    return escribir_tabla();
}

int motor_save_record(const char *clave, int valor) {
    char nombre[SAVE_CLAVE_MAX];
    int anterior;
    if (!g_activo || !clave_valida(clave)) return 0;
    _snprintf(nombre, sizeof(nombre), "record.%s", clave);
    nombre[sizeof(nombre) - 1] = '\0';
    if (!clave_valida(nombre)) return 0;
    cargar_tabla();
    anterior = motor_save_record_actual(clave);
    if (valor <= anterior) return anterior;
    if (!poner(nombre, valor) || !escribir_tabla()) return anterior;
    return valor;
}

int motor_save_record_actual(const char *clave) {
    char nombre[SAVE_CLAVE_MAX];
    if (!g_activo || !clave_valida(clave)) return 0;
    _snprintf(nombre, sizeof(nombre), "record.%s", clave);
    nombre[sizeof(nombre) - 1] = '\0';
    return motor_save_cargar(nombre);
}

int motor_save_ajuste(const char *nombre, int valor) {
    char clave[SAVE_CLAVE_MAX];
    int anterior;
    if (!g_activo || !clave_valida(nombre)) return 0;
    _snprintf(clave, sizeof(clave), "ajuste.%s", nombre);
    clave[sizeof(clave) - 1] = '\0';
    if (!clave_valida(clave)) return 0;
    cargar_tabla();
    anterior = motor_save_ajuste_actual(nombre);
    if (!poner(clave, valor) || !escribir_tabla()) return anterior;
    return anterior;
}

int motor_save_ajuste_actual(const char *nombre) {
    char clave[SAVE_CLAVE_MAX];
    if (!g_activo || !clave_valida(nombre)) return 0;
    _snprintf(clave, sizeof(clave), "ajuste.%s", nombre);
    clave[sizeof(clave) - 1] = '\0';
    return motor_save_cargar(clave);
}

void motor_save_ruta(char *dst, int tam) {
    derivar_rutas();
    if (tam > 0) {
        strcpy(dst, g_ruta);
        dst[tam - 1] = '\0';
    }
}

void motor_save_volcar(const char *ruta) {
    FILE *f;
    int orden[SAVE_CLAVES_MAX];
    int i, j;

    if (!ruta) return;
    cargar_tabla();
    for (i = 0; i < g_num; i++) orden[i] = i;
    /* Orden alfabetico burbuja: deterministico y sin qsort con contexto. */
    for (i = 0; i < g_num; i++)
        for (j = i + 1; j < g_num; j++)
            if (strcmp(g_entradas[orden[i]].clave,
                       g_entradas[orden[j]].clave) > 0) {
                int t = orden[i];
                orden[i] = orden[j];
                orden[j] = t;
            }
    f = fopen(ruta, "wt");
    if (!f) return;
    fprintf(f, "save: activo %d\n", g_activo);
    fprintf(f, "save: nombre %s\n", g_nombre);
    fprintf(f, "save: ruta %s\n", g_ruta);
    fprintf(f, "save: entradas %d\n", g_num);
    for (i = 0; i < g_num; i++) {
        int k = orden[i];
        fprintf(f, "save: clave %s %d\n", g_entradas[k].clave,
                g_entradas[k].valor);
    }
    fprintf(f, "save: recuperado %d\n", g_recuperado);
    fprintf(f, "save: temporal %d\n", existe(g_temporal));
    fclose(f);
}
