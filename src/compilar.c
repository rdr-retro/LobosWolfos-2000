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
 * del motor. El separador va entre la carpeta y el nombre de los mapas:
 * '\\' para el game.exe de Windows y '/' para el kernel loboswolfos
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
    if (num_maps == 0) {
        formatear(compilar_error_msj, sizeof(compilar_error_msj),
                  "El proyecto no tiene ningun elemento de mapa.\n"
                  "Coloca uno en el gestor de logica antes de compilar.");
        return 0;
    }

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
