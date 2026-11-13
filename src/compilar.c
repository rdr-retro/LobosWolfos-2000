/* compilar.c - menu COMPILE: compilar el proyecto (fases 4 y 7).
 *
 * "Compilar para Windows" produce un game.exe autonomo: la plantilla
 * del motor (motor/game.exe, que viaja incrustada en el editor como
 * recurso IDR_MOTOR) mas el proyecto activo pegado al final:
 *
 *   [exe del motor][entradas del blob]["LGB1" u32 tam_blob u32]
 *
 * "Compilar para BIOS" produce la imagen USB de loboswolfos (32 MiB):
 * la plantilla de arranque (MBR + segundo tramo + kernel, incrustada
 * como recurso IDR_KERNEL) con el proyecto en la zona de juego:
 *
 *   [plantilla][ceros hasta LBA 2048][blob rellenado a sector]
 *   ["LGB1" u32 largo total][ceros hasta 32 MiB]
 *
 * En los dos casos el blob lleva el proyecto entero como archivos
 * virtuales:
 *   - "proyecto.logic": la logica activa del gestor
 *   - "maps\<nombre>.mav": cada mapa referenciado por un elemento
 *   - "<tile>.png": cada imagen que usan esos mapas
 * El mapa es opcional (fase 12): sin ningun elemento de mapa el blob
 * solo lleva la logica y el juego corre con la ventana en negro.
 *
 * Al arrancar, el motor lee el pie LGB1 (activo_blob_cargar de
 * motor/activo.c) y carga el proyecto del propio ejecutable, asi el
 * juego compilado funciona en cualquier carpeta, sin maps/ ni tiles/.
 * El kernel hace lo mismo sector a sector con el pie de la zona de
 * juego (activo.rs), que ademas cuenta el blob entero (pie incluido) y
 * queda al final del ultimo sector. El formato de las entradas
 * (u32 largo_nombre, u32 largo_datos, nombre, datos) es el mismo que
 * parsean el motor y el kernel; solo cambia el separador de la carpeta
 * de mapas: "\\" para el motor de Windows y "/" para el kernel, que
 * busca las entradas con barra normal.
 */
#include "app.h"
#include "recurso.h"

#define BLOB_MAGICO "LGB1"
#define COMPILAR_IMAGENES_MAX 256

/* Trazado de la imagen USB (el mismo que monta el xtask de loboswolfos,
 * ver montar_blob): la zona del juego empieza en el LBA 2048 y la imagen
 * entera mide 32 MiB. El tope del blob es BLOB_MAX de activo.rs. */
#define BIOS_SECTOR        512
#define BIOS_JUEGO_LBA     2048
#define BIOS_IMAGEN_BYTES  (32 * 1024 * 1024)
#define BIOS_BLOB_MAX      (1024 * 1024)

#pragma pack(push, 1)
typedef struct {
    u32 largo_nombre;
    u32 largo_datos;
} CabeceraBlob;
#pragma pack(pop)

/* Mensaje del ultimo fallo de compilacion (lo muestra el dialogo). */
static char compilar_error_msj[256];

/* ------------------------------------------------------------------ */
/* Bloque de bytes que crece al agregar (para montar el blob)          */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned char *datos;
    int largo;
    int capacidad;
} Bloque;

static int bloque_agregar(Bloque *b, const void *datos, int largo) {
    if (b->largo + largo > b->capacidad) {
        int nueva = b->capacidad ? b->capacidad : 4096;
        unsigned char *nuevos;
        while (nueva < b->largo + largo) nueva *= 2;
        nuevos = (unsigned char *)realloc(b->datos, (size_t)nueva);
        if (!nuevos) return 0;
        b->datos = nuevos;
        b->capacidad = nueva;
    }
    if (largo > 0) memcpy(b->datos + b->largo, datos, (size_t)largo);
    b->largo += largo;
    return 1;
}

/* Una entrada del blob: cabecera + nombre + datos. */
static int blob_entrada(Bloque *b, const char *nombre, const void *datos,
                        int largo) {
    CabeceraBlob cab;
    cab.largo_nombre = (u32)strlen(nombre);
    cab.largo_datos = (u32)(largo > 0 ? largo : 0);
    if (!bloque_agregar(b, &cab, sizeof(cab))) return 0;
    if (!bloque_agregar(b, nombre, (int)strlen(nombre))) return 0;
    if (largo > 0 && !bloque_agregar(b, datos, largo)) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Lectura de los .mav del proyecto (mismo parseo que mapa.c/activo.c)  */
/* ------------------------------------------------------------------ */

/* Extrae el valor de cadena de una linea Lua: clave = "valor". */
static int extraer_cadena_lua(const char *texto, const char *clave,
                              char *dst, int tam) {
    const char *pos = strstr(texto, clave);
    const char *q1, *q2;
    int n;
    if (!pos) return 0;
    q1 = strchr(pos + strlen(clave), '"');
    if (!q1) return 0;
    q2 = strchr(q1 + 1, '"');
    if (!q2) return 0;
    n = (int)(q2 - q1 - 1);
    if (n >= tam) n = tam - 1;
    memcpy(dst, q1 + 1, (size_t)n);
    dst[n] = '\0';
    return 1;
}

/* Nombre base de una ruta (tras la ultima barra). */
static const char *nombre_base(const char *ruta) {
    const char *p = strrchr(ruta, '\\');
    const char *q = strrchr(ruta, '/');
    if (q && (!p || q > p)) p = q;
    return p ? p + 1 : ruta;
}

static int existe_archivo(const char *ruta) {
    DWORD atributos = GetFileAttributesA(ruta);
    return atributos != INVALID_FILE_ATTRIBUTES &&
           (atributos & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

/* Nombres de las imagenes que usa un .mav, deduplicados. Devuelve el
 * numero encontrado. */
static int mav_imagenes(const char *contenido, char nombres[][64],
                        int maximo) {
    const char *p = strstr(contenido, "tiles");
    int n = 0;
    if (p) p = strstr(p, "{");
    while (p && *p) {
        const char *fin = strchr(p, '}');
        char bloque[512];
        int nb;
        if (!fin) break;
        nb = (int)(fin - p + 1);
        if (nb >= (int)sizeof(bloque)) nb = (int)sizeof(bloque) - 1;
        memcpy(bloque, p, (size_t)nb);
        bloque[nb] = '\0';
        if (strstr(bloque, "image")) {
            char imagen[64];
            if (extraer_cadena_lua(bloque, "image", imagen,
                                   sizeof(imagen)) && imagen[0]) {
                int i, repetida = 0;
                for (i = 0; i < n; i++)
                    if (_stricmp(nombres[i], imagen) == 0) repetida = 1;
                if (!repetida && n < maximo) {
                    recortar_texto(nombres[n], imagen, 64);
                    n++;
                }
            }
        }
        p = fin + 1;
        p = strstr(p, "{");
    }
    return n;
}

/* Busca el PNG de un tile en el mismo orden que el motor
 * (resolver_imagen de motor/activo.c), para incrustar exactamente el
 * archivo que usara el juego. */
static int resolver_tile(const char *ruta_mav, const char *tiles_dir,
                         const char *imagen, char *dst, int tam) {
    char base[MAX_PATH], carpeta[MAX_PATH], exe[MAX_PATH], cwd[MAX_PATH];

    /* <carpeta del .mav>\<tiles_dir>\<imagen> */
    ruta_carpeta(base, sizeof(base), ruta_mav);
    unir_ruta(carpeta, sizeof(carpeta), base, tiles_dir);
    unir_ruta(dst, tam, carpeta, imagen);
    if (existe_archivo(dst)) return 1;

    /* <carpeta del .mav>\<imagen> */
    unir_ruta(dst, tam, base, imagen);
    if (existe_archivo(dst)) return 1;

    /* <carpeta del exe>\tiles\<imagen> */
    obtener_ruta_exe(exe, sizeof(exe));
    unir_ruta(carpeta, sizeof(carpeta), exe, CARPETA_TILES_TEXTO);
    unir_ruta(dst, tam, carpeta, imagen);
    if (existe_archivo(dst)) return 1;

    /* <directorio de trabajo>\tiles\<imagen> */
    GetCurrentDirectoryA(MAX_PATH, cwd);
    cwd[MAX_PATH - 1] = '\0';
    unir_ruta(carpeta, sizeof(carpeta), cwd, CARPETA_TILES_TEXTO);
    unir_ruta(dst, tam, carpeta, imagen);
    return existe_archivo(dst);
}

/* ------------------------------------------------------------------ */
/* Reunion del proyecto                                                 */
/* ------------------------------------------------------------------ */

/* Reune el proyecto activo como entradas del blob (los pasos 1-3 de la
 * compilacion): "proyecto.logic", cada mapa que usan los elementos del
 * gestor y los PNG de sus tiles, resueltos con el mismo orden de busqueda
 * del motor. El mapa es opcional (fase 12): sin ninguno, el blob solo
 * lleva la logica. El separador va entre la carpeta y el nombre de los
 * mapas: '\\' para el game.exe de Windows y '/' para el kernel loboswolfos
 * (fase 7), que busca las entradas con barra normal. Los tiles van con
 * su nombre base en los dos destinos: el motor los registra asi y el
 * kernel cae al nombre base cuando no ve la ruta con la carpeta.
 * Devuelve 0 y explica el fallo en compilar_error_msj; el llamador
 * libera blob->datos en cualquier caso. */
static int compilar_reunir(Bloque *blob, char separador) {
    char maps_nombres[MODULOS_MAX][MAPS_NOMBRE_MAX];
    char imagenes[COMPILAR_IMAGENES_MAX][64];
    int num_maps = 0, num_imagenes = 0;
    char *logic_datos = NULL;
    int logic_largo = 0;
    int ok;
    int i, j;

    /* 1. Mapas distintos que usan los elementos del gestor. */
    for (i = 0; i < g.num_modulos; i++) {
        const ModuloElemento *m = &g.modulos[i];
        int repetido = 0;
        if (!m->mapa[0]) continue;
        for (j = 0; j < num_maps; j++)
            if (_stricmp(maps_nombres[j], m->mapa) == 0) repetido = 1;
        if (!repetido && num_maps < MODULOS_MAX) {
            recortar_texto(maps_nombres[num_maps], m->mapa,
                           MAPS_NOMBRE_MAX);
            num_maps++;
        }
    }
    /* El mapa es opcional: sin elementos de mapa el juego arranca con la
     * ventana en negro y corre igual sus programas (fase 12). */

    /* 2. La logica activa como archivo virtual "proyecto.logic". */
    if (!logica_guardar_memoria(&logic_datos, &logic_largo)) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "No hay memoria para serializar la logica.");
        return 0;
    }
    ok = blob_entrada(blob, "proyecto.logic", logic_datos, logic_largo);
    free(logic_datos);
    if (!ok) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "No hay memoria para montar el proyecto.");
        return 0;
    }

    /* 3. Cada mapa referenciado y sus tiles, como archivos virtuales. */
    for (i = 0; i < num_maps; i++) {
        char carpeta[MAX_PATH], ruta_mav[MAX_PATH];
        char clave[MAPS_NOMBRE_MAX + 8];
        char tiles_dir[128];
        char imgs_mapa[COMPILAR_IMAGENES_MAX][64];
        char *contenido = NULL;
        int largo = 0, n_imgs, k;

        carpeta_maps(carpeta, sizeof(carpeta));
        unir_ruta(ruta_mav, sizeof(ruta_mav), carpeta, maps_nombres[i]);
        if (!leer_archivo(ruta_mav, &contenido, &largo) || !contenido) {
            formatear(compilar_error_msj, sizeof(compilar_error_msj),
                      "No se pudo leer el mapa %s de la carpeta maps.",
                      maps_nombres[i]);
            return 0;
        }
        formatear(clave, sizeof(clave), "%s%c%s", CARPETA_MAPS_TEXTO,
                  separador, maps_nombres[i]);
        ok = blob_entrada(blob, clave, contenido, largo);
        if (ok) {
            if (!extraer_cadena_lua(contenido, "tiles_dir", tiles_dir,
                                     sizeof(tiles_dir)) || !tiles_dir[0])
                recortar_texto(tiles_dir, CARPETA_TILES_TEXTO,
                                sizeof(tiles_dir));
            n_imgs = mav_imagenes(contenido, imgs_mapa,
                                  COMPILAR_IMAGENES_MAX);
        }
        free(contenido);
        if (!ok) {
            formatear(compilar_error_msj, sizeof(compilar_error_msj),
                      "No hay memoria para montar el proyecto.");
            return 0;
        }

        for (k = 0; k < n_imgs; k++) {
            const char *img = imgs_mapa[k];
            char ruta_img[MAX_PATH];
            char *datos_img = NULL;
            int largo_img = 0;
            int repetida = 0;

            for (j = 0; j < num_imagenes; j++)
                if (_stricmp(imagenes[j], img) == 0) repetida = 1;
            if (repetida) continue;

            if (!resolver_tile(ruta_mav, tiles_dir, img,
                               ruta_img, sizeof(ruta_img))) {
                formatear(compilar_error_msj,
                          sizeof(compilar_error_msj),
                          "No se encontro la imagen del tile %s\n"
                          "(usada por el mapa %s).",
                          img, maps_nombres[i]);
                return 0;
            }
            if (!leer_archivo(ruta_img, &datos_img, &largo_img) ||
                !datos_img) {
                formatear(compilar_error_msj,
                          sizeof(compilar_error_msj),
                          "No se pudo leer la imagen del tile %s.", img);
                return 0;
            }
            ok = blob_entrada(blob, nombre_base(img), datos_img,
                              largo_img);
            free(datos_img);
            if (!ok) {
                formatear(compilar_error_msj,
                          sizeof(compilar_error_msj),
                          "No hay memoria para montar el proyecto.");
                return 0;
            }
            if (num_imagenes < COMPILAR_IMAGENES_MAX)
                recortar_texto(imagenes[num_imagenes++], img, 64);
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Blob en memoria para la vista previa (fase 9)                       */
/* ------------------------------------------------------------------ */

/* Monta el proyecto activo como blob LGB1 en memoria: las mismas
 * entradas que "Compilar para Windows" (proyecto.logic, mapas y tiles),
 * sin plantilla ni pie. El mapa es opcional: sin elementos de mapa la
 * vista previa abre la ventana en negro. La vista previa del editor lo
 * registra como archivos virtuales del motor para probar el juego sin
 * escribir ningun ejecutable. Devuelve 1 y deja *datos (malloc) y
 * *largo; el llamador los libera con free. Si falla, compilar_error_msj
 * explica el motivo. */
int compilar_blob_memoria(unsigned char **datos, int *largo) {
    Bloque blob;
    if (!datos || !largo) return 0;
    *datos = NULL;
    *largo = 0;
    memset(&blob, 0, sizeof(blob));
    if (!compilar_reunir(&blob, '\\')) {
        free(blob.datos);
        return 0;
    }
    *datos = blob.datos;
    *largo = blob.largo;
    return 1;
}

/* Guarda el proyecto activo como paquete .wolfo (fase 15): el mismo
 * blob LGB1 que viaja en el game.exe compilado (proyecto.logic, mapas y
 * tiles) seguido del pie "LGB1" con su largo. El pie permite reconocer
 * el archivo y localizar el blob desde el final, igual que hace el motor
 * con el proyecto incrustado en su ejecutable. Devuelve 1 si se
 * escribio; si no, compilar_error_msj explica el fallo. */
int compilar_guardar_wolfo(const char *ruta) {
    unsigned char *datos = NULL;
    int largo = 0;
    FILE *f;
    u32 blob_largo;
    int ok;

    if (!ruta || !ruta[0]) return 0;
    if (!compilar_blob_memoria(&datos, &largo)) return 0;
    f = fopen(ruta, "wb");
    if (!f) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "No se pudo crear el archivo %s.", ruta);
        free(datos);
        return 0;
    }
    blob_largo = (u32)largo;
    ok = fwrite(datos, 1, (size_t)largo, f) == (size_t)largo &&
         fwrite(BLOB_MAGICO, 1, 4, f) == 4 &&
         fwrite(&blob_largo, 1, 4, f) == 4;
    fclose(f);
    free(datos);
    if (!ok) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "Fallo al escribir el proyecto %s.", ruta);
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Lectura del paquete .wolfo                                          */
/* ------------------------------------------------------------------ */

/* Lee una entrada del blob y avanza el cursor. Devuelve 1 si hay entrada,
 * 0 al llegar al final y -1 si el paquete esta danado. El nombre se copia
 * a `nombre` (con terminador) y los datos quedan apuntando al buffer. */
static int blob_leer_entrada(const unsigned char **cursor,
                             const unsigned char *fin, char *nombre,
                             int tam_nombre, const unsigned char **datos,
                             u32 *largo) {
    CabeceraBlob cab;
    size_t resto;

    if (*cursor >= fin) return 0;
    resto = (size_t)(fin - *cursor);
    if (resto < sizeof(cab)) return -1;
    memcpy(&cab, *cursor, sizeof(cab));
    *cursor += sizeof(cab);
    if (cab.largo_nombre == 0 || cab.largo_nombre >= (u32)tam_nombre)
        return -1;
    resto = (size_t)(fin - *cursor);
    if ((size_t)cab.largo_nombre > resto) return -1;
    memcpy(nombre, *cursor, (size_t)cab.largo_nombre);
    nombre[cab.largo_nombre] = '\0';
    *cursor += cab.largo_nombre;
    resto = (size_t)(fin - *cursor);
    if ((size_t)cab.largo_datos > resto) return -1;
    *datos = *cursor;
    *largo = cab.largo_datos;
    *cursor += cab.largo_datos;
    return 1;
}

/* Nombre de archivo de una entrada del paquete, sin su carpeta. */
static const char *blob_nombre_archivo(const char *nombre) {
    const char *p = nombre;
    const char *base = nombre;
    while (*p) {
        if (*p == '\\' || *p == '/') base = p + 1;
        p++;
    }
    return base;
}

/* Un nombre de archivo simple, sin rutas ni caracteres reservados. */
static int blob_nombre_valido(const char *nombre) {
    const char *p;
    if (!nombre || !nombre[0] || strcmp(nombre, ".") == 0 ||
        strcmp(nombre, "..") == 0)
        return 0;
    for (p = nombre; *p; p++) {
        if (*p == '\\' || *p == '/' || *p == ':' || *p == '*' ||
            *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|')
            return 0;
    }
    return 1;
}

/* Escribe una entrada en una carpeta del proyecto. */
static int blob_extraer(const char *carpeta, const char *nombre,
                        const unsigned char *datos, u32 largo) {
    char ruta[MAX_PATH];
    crear_carpeta(carpeta);
    unir_ruta(ruta, sizeof(ruta), carpeta, nombre);
    return escribir_archivo(ruta, (const char *)datos, (int)largo);
}

/* Carga un proyecto completo desde un paquete .wolfo (fase 15): localiza
 * el pie "LGB1" + largo al final, recorre las entradas del blob y restaura
 * la logica, los mapas y los tiles. Los mapas y tiles se escriben en las
 * carpetas del proyecto (maps/ y tiles/) y la logica se carga con el mismo
 * lector de los .logic; al terminar se recargan los tiles y se abre el
 * primer mapa que usa la logica. Devuelve 1 si se cargo; si no,
 * compilar_error_msj explica el fallo. */
int compilar_cargar_wolfo(const char *ruta) {
    char *archivo = NULL;
    int tam = 0;
    const unsigned char *base;
    const unsigned char *fin;
    const unsigned char *cursor;
    const unsigned char *datos;
    const unsigned char *logic_datos = NULL;
    u32 largo;
    u32 logic_largo = 0;
    u32 blob_largo;
    char nombre[512];
    char carpeta[MAX_PATH];
    char temp_dir[MAX_PATH];
    char temp_ruta[MAX_PATH];
    int leido;
    int num_mapas = 0, num_tiles = 0;
    int ok = 0;

    compilar_error_msj[0] = '\0';
    if (!ruta || !ruta[0]) return 0;
    if (!leer_archivo(ruta, &archivo, &tam) || !archivo) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "No se pudo leer el archivo %s.", ruta);
        return 0;
    }
    if (tam < (int)(sizeof(CabeceraBlob) + 8) ||
        memcmp(archivo + tam - 8, BLOB_MAGICO, 4) != 0) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "%s no es un proyecto .wolfo valido.", ruta);
        free(archivo);
        return 0;
    }
    memcpy(&blob_largo, archivo + tam - 4, sizeof(blob_largo));
    if ((u32)(tam - 8) < blob_largo) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "El paquete %s esta incompleto o danado.", ruta);
        free(archivo);
        return 0;
    }
    base = (const unsigned char *)archivo + (tam - 8 - (int)blob_largo);
    fin = (const unsigned char *)archivo + (tam - 8);

    /* 1. Localizar y validar la logica antes de tocar el proyecto. */
    cursor = base;
    while ((leido = blob_leer_entrada(&cursor, fin, nombre, sizeof(nombre),
                                      &datos, &largo)) == 1) {
        if (strcmp(nombre, "proyecto.logic") == 0) {
            logic_datos = datos;
            logic_largo = largo;
        }
    }
    if (leido < 0) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "El paquete %s tiene entradas danadas.", ruta);
        free(archivo);
        return 0;
    }
    if (!logic_datos || logic_largo < 6 ||
        (memcmp(logic_datos, "LOGIC2", 6) != 0 &&
         memcmp(logic_datos, "LOGIC1", 6) != 0)) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "El proyecto %s no lleva una logica valida.", ruta);
        free(archivo);
        return 0;
    }

    /* 2. Extraer los mapas y los tiles a sus carpetas. El paquete puede
     * venir con barra normal (imagen de BIOS) o invertida (game.exe). */
    cursor = base;
    while ((leido = blob_leer_entrada(&cursor, fin, nombre, sizeof(nombre),
                                      &datos, &largo)) == 1) {
        const char *archivo_nombre;
        int es_mapa;
        int es_png;
        if (strcmp(nombre, "proyecto.logic") == 0) continue;
        es_mapa = (_strnicmp(nombre, CARPETA_MAPS_TEXTO "\\",
                             (int)strlen(CARPETA_MAPS_TEXTO) + 1) == 0 ||
                   _strnicmp(nombre, CARPETA_MAPS_TEXTO "/",
                             (int)strlen(CARPETA_MAPS_TEXTO) + 1) == 0);
        archivo_nombre = blob_nombre_archivo(nombre);
        es_png = (strlen(archivo_nombre) > 4 &&
                  _stricmp(archivo_nombre + strlen(archivo_nombre) - 4,
                           ".png") == 0);
        if (!blob_nombre_valido(archivo_nombre)) continue;
        if (es_mapa) {
            carpeta_maps(carpeta, sizeof(carpeta));
            if (!blob_extraer(carpeta, archivo_nombre, datos, largo)) {
                formatear(compilar_error_msj, sizeof(compilar_error_msj),
                          "No se pudo escribir el mapa %s.",
                          archivo_nombre);
                free(archivo);
                return 0;
            }
            num_mapas++;
        } else if (es_png) {
            obtener_ruta_exe(carpeta, sizeof(carpeta));
            unir_ruta(carpeta, sizeof(carpeta), carpeta,
                      CARPETA_TILES_TEXTO);
            if (!blob_extraer(carpeta, archivo_nombre, datos, largo)) {
                formatear(compilar_error_msj, sizeof(compilar_error_msj),
                          "No se pudo escribir el tile %s.",
                          archivo_nombre);
                free(archivo);
                return 0;
            }
            num_tiles++;
        }
    }
    if (leido < 0) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "El paquete %s tiene entradas danadas.", ruta);
        free(archivo);
        return 0;
    }

    /* 3. Cargar la logica con el lector de los .logic, desde un archivo
     * temporal en la carpeta de temporales de Windows. */
    if (GetTempPathA(sizeof(temp_dir), temp_dir) == 0 ||
        GetTempFileNameA(temp_dir, "lwf", 0, temp_ruta) == 0) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "No se pudo crear un archivo temporal.");
        free(archivo);
        return 0;
    }
    if (!escribir_archivo(temp_ruta, (const char *)logic_datos,
                          (int)logic_largo)) {
        DeleteFileA(temp_ruta);
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "No se pudo preparar la logica del proyecto.");
        free(archivo);
        return 0;
    }
    ok = logica_cargar_archivo(temp_ruta);
    DeleteFileA(temp_ruta);
    if (!ok) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "No se pudo cargar la logica del proyecto %s.", ruta);
        free(archivo);
        return 0;
    }
    free(archivo);

    /* 4. Refrescar los tiles y abrir el primer mapa que usa la logica. */
    if (num_tiles > 0) tiles_cargar();
    {
        int i;
        for (i = 0; i < g.num_modulos; i++) {
            char mapa_ruta[MAX_PATH];
            if (!g.modulos[i].mapa[0]) continue;
            carpeta_maps(carpeta, sizeof(carpeta));
            unir_ruta(mapa_ruta, sizeof(mapa_ruta), carpeta,
                      g.modulos[i].mapa);
            if (GetFileAttributesA(mapa_ruta) == INVALID_FILE_ATTRIBUTES)
                continue;
            strncpy(g.mapa_ruta, mapa_ruta, MAX_PATH - 1);
            g.mapa_ruta[MAX_PATH - 1] = 0;
            mapa_reiniciar();
            mapa_cargar(g.mapa_ruta);
            break;
        }
    }
    (void)num_mapas;
    return 1;
}

/* Texto del ultimo fallo de compilacion o de reunion del proyecto. */
const char *compilar_ultimo_error(void) {
    return compilar_error_msj;
}

/* ------------------------------------------------------------------ */
/* Compilacion                                                         */
/* ------------------------------------------------------------------ */

/* Compila el proyecto activo en un game.exe de Windows de 32 bits:
 * plantilla del motor mas el blob del proyecto al final. Devuelve 1 si
 * se escribio el archivo; si no, compilar_error_msj explica el fallo. */
int compilar_windows(const char *ruta) {
    Bloque blob;
    HRSRC rec;
    HGLOBAL carga;
    const void *exe_datos;
    DWORD exe_largo;
    FILE *f;
    u32 blob_largo;
    int ok;

    if (!ruta || !ruta[0]) return 0;
    memset(&blob, 0, sizeof(blob));

    /* 1-3. El proyecto activo como entradas del blob. */
    if (!compilar_reunir(&blob, '\\')) {
        free(blob.datos);
        return 0;
    }

    /* 4. Plantilla del motor, incrustada como recurso del editor. */
    rec = FindResourceA(NULL, MAKEINTRESOURCEA(IDR_MOTOR), RT_RCDATA);
    carga = rec ? LoadResource(NULL, rec) : NULL;
    exe_datos = carga ? LockResource(carga) : NULL;
    exe_largo = carga ? SizeofResource(NULL, rec) : 0;
    if (!exe_datos || exe_largo == 0) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "No se encontro la plantilla del motor (IDR_MOTOR).");
        free(blob.datos);
        return 0;
    }

    /* 5. Escribir el ejecutable: plantilla + blob + pie. */
    f = fopen(ruta, "wb");
    if (!f) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "No se pudo crear el archivo %s.", ruta);
        free(blob.datos);
        return 0;
    }
    blob_largo = (u32)blob.largo;
    ok = fwrite(exe_datos, 1, exe_largo, f) == exe_largo &&
         fwrite(blob.datos, 1, (size_t)blob.largo, f) == (size_t)blob.largo &&
         fwrite(BLOB_MAGICO, 1, 4, f) == 4 &&
         fwrite(&blob_largo, 1, 4, f) == 4;
    fclose(f);
    free(blob.datos);
    if (!ok) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "Fallo al escribir el ejecutable %s.", ruta);
        return 0;
    }
    return 1;
}

/* Escribe n bytes de ceros (el relleno entre las zonas de la imagen). */
static int escribir_ceros(FILE *f, int n) {
    static const unsigned char ceros[64 * 1024];
    while (n > 0) {
        int trozo = n > (int)sizeof(ceros) ? (int)sizeof(ceros) : n;
        if (fwrite(ceros, 1, (size_t)trozo, f) != (size_t)trozo) return 0;
        n -= trozo;
    }
    return 1;
}

/* Compila el proyecto activo en la imagen USB de loboswolfos (menu
 * COMPILE, "Compilar para BIOS", fase 7): la plantilla de arranque
 * (MBR + tramo 2 + kernel, incrustada como recurso IDR_KERNEL) seguida
 * de la zona del juego con el blob LGB1 del proyecto y ceros hasta los
 * 32 MiB.
 *
 * El pie del blob queda al final del ultimo sector de la zona y cuenta
 * el blob entero (pie y relleno incluidos), que es lo que el kernel
 * compara sector a sector al arrancar (activo.rs). Flasheando la
 * imagen a un pendrive (flashear.bat) el juego arranca desde la BIOS
 * sin Windows. Devuelve 1 si se escribio el archivo; si no,
 * compilar_error_msj explica el fallo. */
int compilar_bios(const char *ruta) {
    Bloque blob;
    HRSRC rec;
    HGLOBAL carga;
    const void *arranque_datos;
    DWORD arranque_largo;
    FILE *f;
    u32 largo_total;
    int zona_juego;
    int ok;

    if (!ruta || !ruta[0]) return 0;
    memset(&blob, 0, sizeof(blob));

    /* 1-3. El proyecto activo como entradas del blob (barra normal,
     * que es como las busca el kernel). */
    if (!compilar_reunir(&blob, '/')) {
        free(blob.datos);
        return 0;
    }

    /* 4. Pie al final del ultimo sector: relleno con ceros hasta el
     * multiplo de sector y largo total (pie incluido), que es lo que el
     * kernel compara al leer la zona. */
    largo_total = (u32)((blob.largo + 8 + BIOS_SECTOR - 1) /
                        BIOS_SECTOR * BIOS_SECTOR);
    if ((int)largo_total > BIOS_BLOB_MAX) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "El proyecto compilado mide %d KB y la zona de juego\n"
                  "del kernel loboswolfos admite %d KB.",
                  ((int)largo_total + 1023) / 1024, BIOS_BLOB_MAX / 1024);
        free(blob.datos);
        return 0;
    }

    /* 5. Plantilla del arranque, incrustada como recurso del editor. */
    rec = FindResourceA(NULL, MAKEINTRESOURCEA(IDR_KERNEL), RT_RCDATA);
    carga = rec ? LoadResource(NULL, rec) : NULL;
    arranque_datos = carga ? LockResource(carga) : NULL;
    arranque_largo = carga ? SizeofResource(NULL, rec) : 0;
    if (!arranque_datos || arranque_largo == 0) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "No se encontro la plantilla del kernel (IDR_KERNEL).");
        free(blob.datos);
        return 0;
    }
    if ((int)arranque_largo > BIOS_JUEGO_LBA * BIOS_SECTOR) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "La plantilla del kernel no cabe en la zona de arranque.");
        free(blob.datos);
        return 0;
    }

    /* 6. Escribir la imagen: plantilla, ceros hasta la zona del juego,
     * blob con su pie y ceros hasta los 32 MiB. */
    f = fopen(ruta, "wb");
    if (!f) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "No se pudo crear el archivo %s.", ruta);
        free(blob.datos);
        return 0;
    }
    zona_juego = BIOS_JUEGO_LBA * BIOS_SECTOR;
    ok = fwrite(arranque_datos, 1, arranque_largo, f) == arranque_largo &&
         escribir_ceros(f, zona_juego - (int)arranque_largo) &&
         fwrite(blob.datos, 1, (size_t)blob.largo, f) == (size_t)blob.largo &&
         escribir_ceros(f, (int)largo_total - 8 - blob.largo) &&
         fwrite(BLOB_MAGICO, 1, 4, f) == 4 &&
         fwrite(&largo_total, 1, 4, f) == 4 &&
         escribir_ceros(f, BIOS_IMAGEN_BYTES - zona_juego - (int)largo_total);
    fclose(f);
    free(blob.datos);
    if (!ok) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "Fallo al escribir la imagen %s.", ruta);
        return 0;
    }
    return 1;
}

/* Dialogo del menu COMPILE: elige donde guardar el game.exe compilado. */
void dialogo_compilar_windows(void) {
    char ruta[MAX_PATH];
    OPENFILENAMEA ofn;

    strcpy(ruta, "game.exe");
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = "Ejecutable (*.exe)\0*.exe\0Todos los archivos\0*.*\0\0";
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "exe";
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
                OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameA(&ofn)) return;
    if (compilar_windows(ruta))
        app_estado("Juego compilado: %s", ruta);
    else
        dialogo_error(compilar_error_msj);
    app_invalidar();
}

/* Dialogo del menu COMPILE: elige donde guardar la imagen USB. */
void dialogo_compilar_bios(void) {
    char ruta[MAX_PATH];
    OPENFILENAMEA ofn;

    strcpy(ruta, "loboswolfos.img");
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = "Imagen USB (*.img)\0*.img\0Todos los archivos\0*.*\0\0";
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "img";
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
                OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameA(&ofn)) return;
    if (compilar_bios(ruta))
        app_estado("Imagen USB compilada: %s (flashea con flashear.bat)", ruta);
    else
        dialogo_error(compilar_error_msj);
    app_invalidar();
}
