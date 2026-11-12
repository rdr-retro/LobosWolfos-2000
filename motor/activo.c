/* activo.c - carga del proyecto del juego para el motor egine (game.exe).
 *
 * Lee lo mismo que produce el editor: mapas `.mav` (Lua de texto con
 * tile_size, tiles_dir y celdas), proyectos `.logic` del gestor de logica
 * y tiles PNG con la API plana de GDI+ (igual que src/imagen.c del
 * editor, sin depender de el: el motor es independiente).
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "motor.h"

/* ------------------------------------------------------------------ */
/* GDI+ plana (solo lo necesario para leer PNG)                        */
/* ------------------------------------------------------------------ */

typedef struct GdiplusStartupInput {
    UINT32 GdiplusVersion;
    void *DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} GdiplusStartupInput;

typedef struct GpRect {
    INT X, Y, Width, Height;
} GpRect;

typedef struct GpBitmapData {
    UINT Width;
    UINT Height;
    INT Stride;
    INT PixelFormat;
    void *Scan0;
    UINT_PTR Reserved;
} GpBitmapData;

#define PIXEL_FORMAT_32BPP_ARGB 0x0026200A
#define LOCK_READ 3

__declspec(dllimport) int WINAPI GdiplusStartup(ULONG_PTR *token, const GdiplusStartupInput *input, void *output);
__declspec(dllimport) void WINAPI GdiplusShutdown(ULONG_PTR token);
__declspec(dllimport) int WINAPI GdipCreateBitmapFromFile(const WCHAR *filename, void **bitmap);
__declspec(dllimport) int WINAPI GdipGetImageWidth(void *image, UINT *width);
__declspec(dllimport) int WINAPI GdipGetImageHeight(void *image, UINT *height);
__declspec(dllimport) int WINAPI GdipBitmapLockBits(void *bitmap, const GpRect *rect, UINT flags, INT format, GpBitmapData *datos);
__declspec(dllimport) int WINAPI GdipBitmapUnlockBits(void *bitmap, const GpBitmapData *datos);
__declspec(dllimport) void WINAPI GdipDisposeImage(void *image);

static ULONG_PTR gdiplus_token;
static int gdiplus_ok;

int activo_gdiplus_iniciar(void) {
    GdiplusStartupInput entrada;
    if (gdiplus_ok) return 1; /* ya iniciado por la vista previa */
    entrada.GdiplusVersion = 1;
    entrada.DebugEventCallback = NULL;
    entrada.SuppressBackgroundThread = FALSE;
    entrada.SuppressExternalCodecs = FALSE;
    if (GdiplusStartup(&gdiplus_token, &entrada, NULL) == 0) gdiplus_ok = 1;
    return gdiplus_ok;
}

void activo_gdiplus_terminar(void) {
    if (gdiplus_ok) GdiplusShutdown(gdiplus_token);
    gdiplus_ok = 0;
}

/* Decodifica desde memoria (IStream), para los PNG del proyecto
 * incrustado: asi el juego compilado no toca el disco. */
__declspec(dllimport) int WINAPI GdipCreateBitmapFromStream(void *stream,
                                                            void **bitmap);

/* ------------------------------------------------------------------ */
/* Proyecto incrustado (blob LGB1 al final del propio ejecutable)      */
/* ------------------------------------------------------------------ */
/* El editor compila el juego pegando el proyecto al final del exe del
 * motor:
 *
 *   [exe del motor][entradas del blob]["LGB1" u32 tam_blob u32]
 *
 * Cada entrada del blob es: u32 largo_nombre, u32 largo_datos, los
 * bytes del nombre y los bytes del archivo. Aqui se registran como
 * archivos virtuales y las funciones de carga de abajo (leer_archivo,
 * archivo_existe, cargar_imagen) los consultan antes que el disco, asi
 * el game.exe compilado funciona en cualquier carpeta sin depender de
 * maps/ ni tiles/. */

typedef struct EntradaVirtual {
    char *nombre;
    unsigned char *datos;
    int largo;
    struct EntradaVirtual *sig;
} EntradaVirtual;

static EntradaVirtual *g_virtuales = NULL;

/* Nombre base de una ruta (tras la ultima barra). */
static const char *nombre_base(const char *ruta) {
    const char *p = strrchr(ruta, '\\');
    const char *q = strrchr(ruta, '/');
    if (q && (!p || q > p)) p = q;
    return p ? p + 1 : ruta;
}

/* Busca un archivo del proyecto incrustado: primero por la ruta
 * completa y luego por nombre de archivo (dentro del blob las carpetas
 * no importan). */
static EntradaVirtual *virtual_buscar(const char *ruta) {
    EntradaVirtual *e;
    if (!ruta || !ruta[0]) return NULL;
    for (e = g_virtuales; e; e = e->sig)
        if (strcmp(e->nombre, ruta) == 0) return e;
    {
        const char *base = nombre_base(ruta);
        for (e = g_virtuales; e; e = e->sig)
            if (strcmp(nombre_base(e->nombre), base) == 0) return e;
    }
    return NULL;
}

#pragma pack(push, 1)
typedef struct {
    u32 largo_nombre;
    u32 largo_datos;
} CabeceraEntrada;
#pragma pack(pop)

/* Registra un archivo virtual (copia nombre y datos). */
static int virtual_agregar(const char *nombre, const unsigned char *datos,
                           int largo) {
    EntradaVirtual *e = (EntradaVirtual *)calloc(1, sizeof(*e));
    if (!e) return 0;
    e->nombre = (char *)malloc(strlen(nombre) + 1);
    e->datos = (unsigned char *)malloc(largo > 0 ? (size_t)largo : 1);
    if (!e->nombre || !e->datos) {
        free(e->nombre);
        free(e->datos);
        free(e);
        return 0;
    }
    strcpy(e->nombre, nombre);
    if (largo > 0) memcpy(e->datos, datos, (size_t)largo);
    e->largo = largo;
    e->sig = g_virtuales;
    g_virtuales = e;
    return 1;
}

/* Parsea las entradas de un blob y las registra como virtuales.
 * Devuelve el numero de entradas registradas. */
static int blob_parsear(const unsigned char *datos, int largo) {
    int pos = 0, num = 0;
    while (pos + (int)sizeof(CabeceraEntrada) <= largo) {
        CabeceraEntrada cab;
        char nombre[256];
        int ln, ld;
        memcpy(&cab, datos + pos, sizeof(cab));
        pos += (int)sizeof(cab);
        ln = (int)cab.largo_nombre;
        ld = (int)cab.largo_datos;
        if (ln <= 0 || ln > 200 || ld < 0 || ld > 16 * 1024 * 1024 ||
            pos + ln + ld > largo) break;
        memcpy(nombre, datos + pos, (size_t)ln);
        nombre[ln] = '\0';
        pos += ln;
        if (!virtual_agregar(nombre, datos + pos, ld)) break;
        pos += ld;
        num++;
    }
    return num;
}

/* Libera todos los archivos virtuales registrados hasta ahora. */
void activo_virtuales_limpiar(void) {
    EntradaVirtual *e = g_virtuales;
    while (e) {
        EntradaVirtual *sig = e->sig;
        free(e->nombre);
        free(e->datos);
        free(e);
        e = sig;
    }
    g_virtuales = NULL;
}

/* Registra un blob LGB1 (sin el pie) como archivos virtuales del motor.
 * Lo usa la vista previa del editor: reune el proyecto en memoria y lo
 * carga sin escribir ningun archivo. Sustituye a los virtuales que
 * hubiera. Devuelve el numero de entradas registradas. */
int activo_blob_registrar(const unsigned char *datos, int largo) {
    activo_virtuales_limpiar();
    if (!datos || largo <= 0) return 0;
    return blob_parsear(datos, largo);
}

/* Lee el pie LGB1 del final del propio ejecutable y registra su blob
 * como archivos virtuales. Devuelve el numero de archivos; 0 si este
 * exe no lleva proyecto incrustado o el pie no es valido. */
int activo_blob_cargar(void) {
    char ruta[MAX_PATH];
    FILE *f;
    long tam;
    unsigned char pie[8];
    u32 largo_blob;
    unsigned char *blob;
    int num;

    GetModuleFileNameA(NULL, ruta, sizeof(ruta));
    ruta[sizeof(ruta) - 1] = '\0';
    f = fopen(ruta, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    tam = ftell(f);
    if (tam < 16) { fclose(f); return 0; }
    fseek(f, tam - 8, SEEK_SET);
    if (fread(pie, 1, 8, f) != 8 || memcmp(pie, "LGB1", 4) != 0) {
        fclose(f);
        return 0;
    }
    memcpy(&largo_blob, pie + 4, 4);
    if (largo_blob == 0 || (long)largo_blob > tam - 8) {
        fclose(f);
        return 0;
    }
    blob = (unsigned char *)malloc(largo_blob);
    if (!blob) { fclose(f); return 0; }
    fseek(f, tam - 8 - (long)largo_blob, SEEK_SET);
    if (fread(blob, 1, largo_blob, f) != largo_blob) {
        free(blob);
        fclose(f);
        return 0;
    }
    fclose(f);
    num = blob_parsear(blob, (int)largo_blob);
    free(blob);
    return num;
}

/* Extrae los pixeles de un bitmap GDI+ ya decodificado y lo libera.
 * Los pixeles quedan en 0xAARRGGBB, igual que el framebuffer. */
static int extraer_pixeles(void *bmp, MotorImagen *img) {
    GpRect rect;
    GpBitmapData datos;
    UINT w, h;
    int y;

    memset(img, 0, sizeof(*img));
    if (GdipGetImageWidth(bmp, &w) != 0 || GdipGetImageHeight(bmp, &h) != 0 ||
        w == 0 || h == 0 || w > 8192 || h > 8192) {
        GdipDisposeImage(bmp);
        return 0;
    }
    rect.X = 0; rect.Y = 0; rect.Width = (INT)w; rect.Height = (INT)h;
    memset(&datos, 0, sizeof(datos));
    if (GdipBitmapLockBits(bmp, &rect, LOCK_READ, PIXEL_FORMAT_32BPP_ARGB,
                            &datos) != 0) {
        GdipDisposeImage(bmp);
        return 0;
    }
    img->pixeles = (u32 *)malloc((size_t)w * h * sizeof(u32));
    if (img->pixeles) {
        for (y = 0; y < (int)h; y++) {
            const u32 *fila = (const u32 *)((const BYTE *)datos.Scan0 +
                                            (size_t)y * datos.Stride);
            memcpy(img->pixeles + (size_t)y * w, fila, (size_t)w * sizeof(u32));
        }
        img->ancho = (int)w;
        img->alto = (int)h;
    }
    GdipBitmapUnlockBits(bmp, &datos);
    GdipDisposeImage(bmp);
    return img->pixeles != NULL;
}

/* Carga un PNG (o BMP) en una MotorImagen. Devuelve 0 si falla; los
 * pixeles quedan en 0xAARRGGBB, igual que el framebuffer del editor.
 * Si el archivo vive en el proyecto incrustado se decodifica desde
 * memoria con un IStream, sin tocar el disco. */
static int cargar_imagen(const char *ruta, MotorImagen *img) {
    WCHAR wruta[MAX_PATH * 2];
    void *bmp = NULL;
    EntradaVirtual *v;

    if (!ruta || !ruta[0] || !gdiplus_ok) return 0;
    v = virtual_buscar(ruta);
    if (v) {
        HGLOBAL h;
        void *p;
        IStream *stream = NULL;
        if (v->largo <= 0) return 0;
        h = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)v->largo);
        if (!h) return 0;
        p = GlobalLock(h);
        if (!p) { GlobalFree(h); return 0; }
        memcpy(p, v->datos, (size_t)v->largo);
        GlobalUnlock(h);
        if (CreateStreamOnHGlobal(h, TRUE, &stream) != 0 || !stream) {
            GlobalFree(h);
            return 0;
        }
        if (GdipCreateBitmapFromStream(stream, &bmp) != 0 || !bmp) {
            stream->lpVtbl->Release(stream);
            return 0;
        }
        stream->lpVtbl->Release(stream);
        return extraer_pixeles(bmp, img);
    }
    MultiByteToWideChar(CP_ACP, 0, ruta, -1, wruta, MAX_PATH * 2);
    if (GdipCreateBitmapFromFile(wruta, &bmp) != 0 || !bmp) return 0;
    return extraer_pixeles(bmp, img);
}

/* ------------------------------------------------------------------ */
/* Utilidades de archivos y rutas                                      */
/* ------------------------------------------------------------------ */

static char *leer_archivo(const char *ruta, int *largo) {
    FILE *f;
    char *texto;
    long n;
    EntradaVirtual *v = virtual_buscar(ruta);

    if (v) { /* proyecto incrustado: copiar a memoria */
        texto = (char *)malloc((size_t)v->largo + 1);
        if (!texto) return NULL;
        if (v->largo > 0) memcpy(texto, v->datos, (size_t)v->largo);
        texto[v->largo] = '\0';
        *largo = v->largo;
        return texto;
    }
    f = fopen(ruta, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 16 * 1024 * 1024) { fclose(f); return NULL; }
    texto = (char *)malloc((size_t)n + 1);
    if (!texto) { fclose(f); return NULL; }
    if (fread(texto, 1, (size_t)n, f) != (size_t)n) {
        free(texto);
        fclose(f);
        return NULL;
    }
    texto[n] = '\0';
    *largo = (int)n;
    fclose(f);
    return texto;
}

static int archivo_existe(const char *ruta) {
    DWORD atributos;
    if (virtual_buscar(ruta)) return 1; /* proyecto incrustado */
    if (!ruta || !ruta[0]) return 0;
    atributos = GetFileAttributesA(ruta);
    return atributos != INVALID_FILE_ATTRIBUTES &&
           (atributos & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static void ruta_carpeta(const char *ruta, char *dst, int tam) {
    const char *barra = strrchr(ruta, '\\');
    int n;
    if (!barra) barra = strrchr(ruta, '/');
    if (!barra) { dst[0] = '\0'; return; }
    n = (int)(barra - ruta);
    if (n >= tam) n = tam - 1;
    memcpy(dst, ruta, (size_t)n);
    dst[n] = '\0';
}

static void unir_ruta(char *dst, int tam, const char *a, const char *b) {
    _snprintf(dst, tam, "%s\\%s", a, b);
    dst[tam - 1] = '\0';
}

static void ruta_exe(char *dst, int tam) {
    GetModuleFileNameA(NULL, dst, tam);
    dst[tam - 1] = '\0';
}

/* Busca el PNG de un tile: primero junto al .mav (carpeta tiles_dir),
 * luego junto al exe (tiles del proyecto) y por ultimo en la carpeta
 * tiles del directorio de trabajo (permite lanzar el juego desde la
 * raiz del proyecto tengalo donde lo tengas). */
static int resolver_imagen(const char *mapa_ruta, const char *tiles_dir,
                            const char *imagen, char *dst, int tam) {
    char base[MAX_PATH], carpeta[MAX_PATH], exe[MAX_PATH];

    ruta_carpeta(mapa_ruta, base, sizeof(base));
    if (base[0]) {
        unir_ruta(carpeta, sizeof(carpeta), base, tiles_dir);
        unir_ruta(dst, tam, carpeta, imagen);
        if (archivo_existe(dst)) return 1;
        unir_ruta(dst, tam, base, imagen);
        if (archivo_existe(dst)) return 1;
    } else {
        unir_ruta(dst, tam, tiles_dir, imagen);
        if (archivo_existe(dst)) return 1;
    }
    ruta_exe(exe, sizeof(exe));
    ruta_carpeta(exe, base, sizeof(base));
    unir_ruta(carpeta, sizeof(carpeta), base, "tiles");
    unir_ruta(dst, tam, carpeta, imagen);
    if (archivo_existe(dst)) return 1;

    GetCurrentDirectoryA(MAX_PATH, base);
    base[MAX_PATH - 1] = '\0';
    unir_ruta(carpeta, sizeof(carpeta), base, "tiles");
    unir_ruta(dst, tam, carpeta, imagen);
    return archivo_existe(dst);
}

/* ------------------------------------------------------------------ */
/* Parseo de Lua de texto (igual que el editor)                        */
/* ------------------------------------------------------------------ */

static int extraer_entero_lua(const char *texto, const char *clave, int *salida) {
    const char *pos = strstr(texto, clave);
    int negativo = 0, valor = 0, digitos = 0;
    if (!pos) return 0;
    pos += strlen(clave);
    while (*pos == ' ' || *pos == '=' || *pos == '\t' || *pos == ':') pos++;
    if (*pos == '-') { negativo = 1; pos++; }
    while (*pos >= '0' && *pos <= '9') {
        valor = valor * 10 + (*pos - '0');
        pos++;
        digitos++;
    }
    if (!digitos) return 0;
    *salida = negativo ? -valor : valor;
    return 1;
}

static int extraer_cadena_lua(const char *texto, const char *clave,
                              char *dst, int tam) {
    const char *pos = strstr(texto, clave);
    const char *q1, *q2;
    if (!pos) return 0;
    q1 = strchr(pos + strlen(clave), '"');
    if (!q1) return 0;
    q2 = strchr(q1 + 1, '"');
    if (!q2) return 0;
    {
        int n = (int)(q2 - q1 - 1);
        if (n >= tam) n = tam - 1;
        memcpy(dst, q1 + 1, (size_t)n);
        dst[n] = '\0';
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Mapa .mav                                                           */
/* ------------------------------------------------------------------ */

#define MOTOR_CELDAS_MAX 8192
#define MOTOR_IMAGENES_MAX 256

void motor_mapa_liberar(MotorMapa *m) {
    int i;
    if (!m) return;
    for (i = 0; i < m->num_imagenes; i++) free(m->imagenes[i].pixeles);
    free(m->imagenes);
    free(m->celdas);
    memset(m, 0, sizeof(*m));
}

int activo_cargar_mapa(const char *ruta, MotorMapa *m) {
    char *contenido;
    int largo = 0;
    const char *p;
    int lado = 32;
    char tiles_dir[128];
    char nombres[MOTOR_IMAGENES_MAX][64];
    int num_nombres = 0;

    memset(m, 0, sizeof(*m));
    if (!ruta) return 0;
    contenido = leer_archivo(ruta, &largo);
    if (!contenido) return 0;

    extraer_entero_lua(contenido, "tile_size", &lado);
    if (lado < 4) lado = 4;
    if (lado > 256) lado = 256;
    m->lado = lado;
    if (!extraer_cadena_lua(contenido, "tiles_dir", tiles_dir, sizeof(tiles_dir)))
        strcpy(tiles_dir, "tiles");
    if (!tiles_dir[0]) strcpy(tiles_dir, "tiles");

    m->celdas = (MotorCelda *)calloc(MOTOR_CELDAS_MAX, sizeof(MotorCelda));
    m->imagenes = (MotorImagen *)calloc(MOTOR_IMAGENES_MAX, sizeof(MotorImagen));
    if (!m->celdas || !m->imagenes) { motor_mapa_liberar(m); free(contenido); return 0; }

    p = strstr(contenido, "tiles");
    if (p) p = strstr(p, "{");
    while (p && *p) {
        const char *fin = strchr(p, '}');
        char bloque[512];
        int n;
        if (!fin) break;
        n = (int)(fin - p + 1);
        if (n >= (int)sizeof(bloque)) n = (int)sizeof(bloque) - 1;
        memcpy(bloque, p, (size_t)n);
        bloque[n] = '\0';
        if (strstr(bloque, "image")) {
            int x, y;
            char imagen[64];
            if (extraer_entero_lua(bloque, "x", &x) &&
                extraer_entero_lua(bloque, "y", &y) &&
                extraer_cadena_lua(bloque, "image", imagen, sizeof(imagen))) {
                int idx = -1, i;
                /* deduplicar por nombre */
                for (i = 0; i < num_nombres; i++) {
                    if (strcmp(nombres[i], imagen) == 0) { idx = i; break; }
                }
                if (idx < 0 && num_nombres < MOTOR_IMAGENES_MAX) {
                    idx = num_nombres;
                    _snprintf(nombres[num_nombres], 64, "%s", imagen);
                    nombres[num_nombres][63] = '\0';
                    num_nombres++;
                }
                if (idx >= 0 && m->num_celdas < MOTOR_CELDAS_MAX) {
                    MotorCelda *c = &m->celdas[m->num_celdas++];
                    c->x = x;
                    c->y = y;
                    c->imagen = idx;
                    if (m->num_imagenes <= idx) m->num_imagenes = idx + 1;
                }
            }
        }
        p = fin + 1;
        p = strstr(p, "{");
    }
    free(contenido);

    /* cargar las imagenes deduplicadas */
    {
        int i;
        for (i = 0; i < m->num_imagenes; i++) {
            char ruta_img[MAX_PATH];
            if (resolver_imagen(ruta, tiles_dir, nombres[i],
                                ruta_img, sizeof(ruta_img)))
                cargar_imagen(ruta_img, &m->imagenes[i]);
            /* si falla, pixeles queda NULL: la celda se dibuja hueca */
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Proyecto .logic                                                     */
/* ------------------------------------------------------------------ */

/* Formato (logica_guardar_archivo del editor):
 *   LOGIC2
 *   <pan_x> <pan_y>
 *   <num_elementos>
 *   <x> <y> <ancho> <alto> <id> <plegado> "<mapa>" <funcion>
 *   <num_funciones>
 *   <id> "<nombre>" <largo>
 *   <codigo lobo de largo bytes crudos>
 *
 * El juego usa el primer elemento con mapa asociado (las pantallas del
 * juego). El mapa tambien es opcional (fase 12): sin ningun elemento de
 * mapa el proyecto corre con el mapa vacio y la ventana en negro. El
 * inicio de programa (funcion 0, fase 11) es opcional: marca donde
 * empieza cada programa y separa sus modulos, pero los elementos con
 * codigo corren aunque no haya ninguno. El campo funcion pasa
 * de indice a referencia: -1 = item de mapa, 0 = inicio de programa y
 * >= 1 el id de un boton FUNCIONES de codigo lobo, que el motor corre
 * cada fotograma (ver lobo.h). Los proyectos LOGIC1 (sin la seccion de
 * funciones) siguen cargando igual. */

/* Busca en la linea el nombre entre comillas y el entero que va despues.
 * Devuelve 1 si la linea tiene la forma completa. */
static int parsear_elemento(const char *linea, char *mapa, int tam_mapa,
                             int *funcion) {
    const char *q1 = strchr(linea, '"');
    const char *q2;
    int n;
    const char *q;
    mapa[0] = '\0';
    *funcion = -1;
    if (!q1) return 0;
    q2 = strchr(q1 + 1, '"');
    if (!q2) return 0;
    n = (int)(q2 - q1 - 1);
    if (n >= tam_mapa) n = tam_mapa - 1;
    memcpy(mapa, q1 + 1, (size_t)n);
    mapa[n] = '\0';
    q = q2 + 1;
    while (*q == ' ' || *q == '\t') q++;
    if ((*q >= '0' && *q <= '9') || *q == '-') {
        *funcion = (int)strtol(q, NULL, 10);
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Funciones de codigo lobo del proyecto (LOGIC2, fase 8)              */
/* ------------------------------------------------------------------ */

/* Un boton FUNCIONES serializado: su id y su texto lobo. */
typedef struct ActivoFuncion {
    int id;
    int largo; /* bytes utiles del codigo (sin el 0 final) */
    char codigo[ACTIVO_FUNCION_CODIGO_MAX + 1];
} ActivoFuncion;

static ActivoFuncion g_funciones[ACTIVO_FUNCIONES_MAX];
static int g_num_funciones = 0;

/* Ids de las funciones que usan los elementos del lienzo, en orden de
 * aparicion: el motor corre un script por elemento. */
static int g_usadas[ACTIVO_USADAS_MAX];
static int g_num_usadas = 0;

/* Slot de la funcion con ese id (creandolo si hay sitio): la ultima
 * definicion gana, como en el resto del formato. */
static ActivoFuncion *funcion_slot(int id) {
    int i;
    for (i = 0; i < g_num_funciones; i++)
        if (g_funciones[i].id == id) return &g_funciones[i];
    if (g_num_funciones >= ACTIVO_FUNCIONES_MAX) return NULL;
    g_funciones[g_num_funciones].id = id;
    return &g_funciones[g_num_funciones++];
}

const char *activo_funcion_codigo(int id) {
    int i;
    for (i = 0; i < g_num_funciones; i++)
        if (g_funciones[i].id == id) return g_funciones[i].codigo;
    return NULL;
}

int activo_funciones_usadas(int *ids, int max) {
    int i, n = g_num_usadas < max ? g_num_usadas : max;
    for (i = 0; i < n; i++) ids[i] = g_usadas[i];
    return n;
}

/* Parsea la seccion de funciones de un LOGIC2 desde `p` (justo despues
 * de los elementos): una linea con el numero de funciones y, por cada
 * una, la cabecera <id> "<nombre>" <largo>, su codigo crudo de largo
 * bytes (puede llevar saltos de linea) y un separador final. Tolerante
 * a lo que falte al final del archivo. */
static void cargar_funciones(const char *p, char *linea, int tam_linea) {
    int nf, k;
    nf = (int)strtol(p, NULL, 10);
    if (nf < 0 || nf > 100000) nf = 0;
    while (*p && *p != '\n') p++;
    if (*p) p++;

    for (k = 0; k < nf && p && *p; k++) {
        const char *fin = strchr(p, '\n');
        int n = fin ? (int)(fin - p) : (int)strlen(p);
        int id = 0, largo = 0, copia;
        char *q1, *q2;
        ActivoFuncion *fn = NULL;

        if (n >= tam_linea) n = tam_linea - 1;
        memcpy(linea, p, (size_t)n);
        linea[n] = '\0';
        /* cabecera: <id> "<nombre>" <largo> */
        id = (int)strtol(linea, NULL, 10);
        q1 = strchr(linea, '"');
        q2 = q1 ? strchr(q1 + 1, '"') : NULL;
        if (q2) largo = (int)strtol(q2 + 1, NULL, 10);
        if (largo < 0) largo = 0;
        p = fin ? fin + 1 : NULL;
        if (!p) break;

        copia = largo > ACTIVO_FUNCION_CODIGO_MAX
                  ? ACTIVO_FUNCION_CODIGO_MAX : largo;
        if (id >= 1 && copia > 0) fn = funcion_slot(id);
        if (fn) {
            memcpy(fn->codigo, p, (size_t)copia);
            fn->codigo[copia] = '\0';
            fn->largo = copia;
        }
        /* avanzar el codigo entero aunque no quepa en el slot */
        while (largo > 0 && *p) { p++; largo--; }
        if (*p == '\n') p++; /* separador tras el codigo */
    }
}

int activo_cargar_proyecto(const char *ruta_logic, MotorMapa *m,
                           char *mapa_usado, int tam_usado) {
    char ruta_mapa[MAX_PATH];
    int mapa_pedido = 0;

    mapa_usado[0] = '\0';
    ruta_mapa[0] = '\0';
    g_num_funciones = 0;
    g_num_usadas = 0;

    if (ruta_logic && ruta_logic[0]) {
        char *contenido;
        int largo = 0;
        char linea[512];
        const char *p;
        int num = 0, i = 0, hay_egine = 0, es_logic2 = 0;

        contenido = leer_archivo(ruta_logic, &largo);
        if (!contenido) return 0;
        p = contenido;
        /* cabecera: LOGIC1 o LOGIC2 / pan / num */
        if (strncmp(p, "LOGIC2", 6) == 0) es_logic2 = 1;
        else if (strncmp(p, "LOGIC1", 6) != 0) { free(contenido); return 0; }
        p += 6;
        while (*p == '\r' || *p == '\n') p++;
        while (*p && *p != '\n') p++;          /* linea del pan */
        if (*p) p++;
        num = (int)strtol(p, NULL, 10);
        if (num < 0 || num > 100000) num = 0;
        while (*p && *p != '\n') p++;
        if (*p) p++;

        while (i < num && p && *p) {
            const char *fin = strchr(p, '\n');
            int n = fin ? (int)(fin - p) : (int)strlen(p);
            char mapa[128];
            int funcion = -1;
            if (n >= (int)sizeof(linea)) n = (int)sizeof(linea) - 1;
            memcpy(linea, p, (size_t)n);
            linea[n] = '\0';
            if (parsear_elemento(linea, mapa, sizeof(mapa), &funcion)) {
                if (funcion == 0) hay_egine = 1;
                if (funcion >= 1 && g_num_usadas < ACTIVO_USADAS_MAX)
                    g_usadas[g_num_usadas++] = funcion;
                if (!ruta_mapa[0] && mapa[0]) {
                    /* el mapa del proyecto vive en maps/ */
                    mapa_pedido = 1;
                    _snprintf(ruta_mapa, sizeof(ruta_mapa), "maps\\%s", mapa);
                    ruta_mapa[sizeof(ruta_mapa) - 1] = '\0';
                    if (!archivo_existe(ruta_mapa)) ruta_mapa[0] = '\0';
                }
            }
            i++;
            p = fin ? fin + 1 : NULL;
        }
        /* LOGIC2: funciones de codigo lobo tras los elementos. */
        if (es_logic2 && p && *p)
            cargar_funciones(p, linea, (int)sizeof(linea));
        free(contenido);
        (void)hay_egine; /* informacion para futuras fases */
    }

    if (!ruta_mapa[0]) {
        /* Sin elemento de mapa el proyecto corre igual, con el mapa
         * vacio: la ventana queda en negro (fase 12). Si el proyecto
         * pedia un mapa y no esta, si es un error. */
        if (mapa_pedido) return 0;
        memset(m, 0, sizeof(*m));
        mapa_usado[0] = '\0';
        return 1;
    }
    if (!activo_cargar_mapa(ruta_mapa, m)) return 0;
    strncpy(mapa_usado, ruta_mapa, tam_usado - 1);
    mapa_usado[tam_usado - 1] = '\0';
    return 1;
}

/* ------------------------------------------------------------------ */
/* Guardar BMP de 24 bits (para --captura)                             */
/* ------------------------------------------------------------------ */

#pragma pack(push, 1)
typedef struct {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1, bfReserved2;
    DWORD bfOffBits;
} BMPFileHeader;
typedef struct {
    DWORD biSize;
    LONG biWidth, biHeight;
    WORD biPlanes, biBitCount;
    DWORD biCompression, biSizeImage;
    LONG biXPelsPerMeter, biYPelsPerMeter;
    DWORD biClrUsed, biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)

int activo_guardar_bmp(const char *ruta, const u32 *pixeles, int ancho, int alto) {
    FILE *f;
    BMPFileHeader fh;
    BMPInfoHeader ih;
    int fila_bytes, x, y;
    u8 *fila;

    if (!ruta || !pixeles || ancho < 1 || alto < 1) return 0;
    fila_bytes = ((ancho * 3) + 3) & ~3;
    memset(&fh, 0, sizeof(fh));
    memset(&ih, 0, sizeof(ih));
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + (DWORD)fila_bytes * (DWORD)alto;
    ih.biSize = sizeof(ih);
    ih.biWidth = ancho;
    ih.biHeight = alto;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = 0;
    ih.biSizeImage = (DWORD)fila_bytes * (DWORD)alto;

    f = fopen(ruta, "wb");
    if (!f) return 0;
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    fila = (u8 *)calloc((size_t)fila_bytes, 1);
    if (!fila) { fclose(f); return 0; }
    for (y = alto - 1; y >= 0; y--) {
        for (x = 0; x < ancho; x++) {
            u32 c = pixeles[(size_t)y * ancho + x];
            fila[x * 3 + 0] = (u8)(c & 0xFF);          /* B */
            fila[x * 3 + 1] = (u8)((c >> 8) & 0xFF);  /* G */
            fila[x * 3 + 2] = (u8)((c >> 16) & 0xFF); /* R */
        }
        fwrite(fila, 1, (size_t)fila_bytes, f);
    }
    free(fila);
    fclose(f);
    return 1;
}
