/* modelo_log.c - conversion del .log actual de ejemplo al ProjectModel.
 *
 * El .log de ejemplo es un LOGIC3 con una seccion `montaje` escrita a mano:
 * cabecera `proyecto`, nodos `nodo ID tipo ...` y enlaces `conecta A -> B`.
 * Este lector no toca los lectores de LOGIC3: solo interpreta esa seccion
 * textual y construye un grafo con el contrato de proyecto_modelo.h.
 *
 * La posicion x,y del nodo pasa al estado de UI. El campo `enlace` se guarda
 * como propiedad para no perder informacion del formato anterior. Los enlaces
 * `conecta` pasan a entidades PM_KIND_CONEXION con puertos por defecto
 * `salida` y `entrada`; si el texto escribe `A:puerto -> B:puerto`, se respeta.
 */
#include "modelo_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ML_LINEA_MAX     1024
#define ML_TOKEN_MAX     128
#define ML_TOKENS_MAX     32
#define ML_NODOS_MAX      64
#define ML_ENLACES_MAX    64

typedef struct {
    int id;
    int kind;
    int linea;
    int x, y;
    char tipo[PM_NOMBRE_MAX];
    char nombre[PM_NOMBRE_MAX];
    char enlace[PM_TEXTO_MAX];
    char valor[PM_TEXTO_MAX];
    char valor2[PM_TEXTO_MAX];
} NodoLog;

typedef struct {
    int origen;
    int destino;
    int linea;
    char puerto_origen[PM_NOMBRE_MAX];
    char puerto_destino[PM_NOMBRE_MAX];
} EnlaceLog;

static void ml_copiar(char *dst, size_t tam, const char *src) {
    size_t n;
    if (!dst || tam == 0) return;
    if (!src) src = "";
    n = strlen(src);
    if (n >= tam) n = tam - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static int ml_entero(const char *texto, int *salida) {
    char *fin;
    long valor;
    if (!texto || !*texto || !salida) return 0;
    valor = strtol(texto, &fin, 10);
    if (*fin) return 0;
    if (valor < -2147483647L || valor > 2147483647L) return 0;
    *salida = (int)valor;
    return 1;
}

/* Divide la linea en tokens. Entre comillas se toma literalmente hasta la
 * comilla de cierre; el resto se separa por espacios o tabuladores. */
static int ml_tokenizar(char * linea, char tok[][ML_TOKEN_MAX], int max) {
    int n = 0;
    while (*linea) {
        char *dst;
        int k = 0;
        while (*linea == ' ' || *linea == '\t' || *linea == '\r' ||
               *linea == '\n')
            linea++;
        if (!*linea) break;
        if (n >= max) return -1;
        dst = tok[n++];
        if (*linea == '"') {
            linea++;
            while (*linea && *linea != '"') {
                if (k < ML_TOKEN_MAX - 1) dst[k++] = *linea;
                linea++;
            }
            if (*linea == '"') linea++;
        } else {
            while (*linea && *linea != ' ' && *linea != '\t' &&
                   *linea != '\r' && *linea != '\n') {
                if (k < ML_TOKEN_MAX - 1) dst[k++] = *linea;
                linea++;
            }
        }
        dst[k] = '\0';
    }
    return n;
}

static int ml_fallo(char *diagnostico, size_t longitud, int linea,
                    const char *texto) {
    char buf[PM_ERROR_MAX];
    if (linea > 0) snprintf(buf, sizeof(buf), "linea %d: %s", linea, texto);
    else ml_copiar(buf, sizeof(buf), texto);
    ml_copiar(diagnostico, longitud, buf);
    return 0;
}

static int ml_kind(const char *tipo) {
    if (!tipo || !tipo[0]) return 0;
    if (strcmp(tipo, "sistema.escena") == 0) return PM_KIND_ESCENA;
    if (strcmp(tipo, "sistema.actor") == 0) return PM_KIND_ACTOR;
    if (strcmp(tipo, "sistema.teclado") == 0) return PM_KIND_CONTROL;
    if (strcmp(tipo, "sistema.recurso.imagen") == 0 ||
        strcmp(tipo, "sistema.recurso.mapa") == 0) return PM_KIND_RECURSO;
    if (strcmp(tipo, "sistema.accion.movimiento") == 0) return PM_KIND_ACCION;
    return 0;
}

/* Acepta `2` y `2:salida`. El puerto por defecto depende del extremo. */
static int ml_extremo(const char *texto, int *id, char *puerto, size_t tam,
                      const char *por_defecto) {
    const char *dos_puntos;
    char numero[ML_TOKEN_MAX];
    size_t n;
    if (!texto || !*texto || !id) return 0;
    dos_puntos = strchr(texto, ':');
    n = dos_puntos ? (size_t)(dos_puntos - texto) : strlen(texto);
    if (n == 0 || n >= sizeof(numero)) return 0;
    memcpy(numero, texto, n);
    numero[n] = '\0';
    if (!ml_entero(numero, id) || *id <= 0) return 0;
    ml_copiar(puerto, tam, dos_puntos ? dos_puntos + 1 : por_defecto);
    return 1;
}

static int ml_buscar_nodo(const NodoLog *nod, int num, int id) {
    int i;
    for (i = 0; i < num; i++) if (nod[i].id == id) return 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* LOG 2: montaje de escenas, nodos jerarquicos y scripts externos.          */
/* ------------------------------------------------------------------------- */

static int ml_v2_valor(char tok[][ML_TOKEN_MAX], int n, const char *clave,
                       char *salida, size_t tam) {
    int i;
    if (!salida || tam == 0) return 0;
    salida[0] = '\0';
    for (i = 1; i + 1 < n; i += 2) {
        if (strcmp(tok[i], clave) != 0) continue;
        ml_copiar(salida, tam, tok[i + 1]);
        return 1;
    }
    return 0;
}

static int ml_v2_entero(char tok[][ML_TOKEN_MAX], int n, const char *clave,
                        int *salida, int obligatorio) {
    char texto[ML_TOKEN_MAX];
    if (!ml_v2_valor(tok, n, clave, texto, sizeof(texto))) {
        if (salida) *salida = 0;
        return obligatorio ? 0 : 1;
    }
    return ml_entero(texto, salida);
}

static int ml_v2_anadir_propiedades(EntidadModelo *e, char tok[][ML_TOKEN_MAX],
                                    int n, int linea) {
    int i;
    for (i = 1; i + 1 < n; i += 2) {
        /* id y nombre ya viven en los campos estructurales de la entidad.
         * Duplicarlos como propiedades ensucia el inspector y permite que
         * una edicion aparente cambiar una cosa distinta de la que serializa
         * el LOG 2. El resto de claves conserva el orden del archivo. */
        if (strcmp(tok[i], "id") == 0 || strcmp(tok[i], "nombre") == 0)
            continue;
        if (!pm_propiedad(e, tok[i], tok[i + 1], linea)) return 0;
    }
    return 1;
}

static ProjectModel *ml_leer_v2(const char *ruta, char *diagnostico,
                                size_t longitud) {
    FILE *f;
    char linea[ML_LINEA_MAX];
    char tok[ML_TOKENS_MAX][ML_TOKEN_MAX];
    char nombre_proyecto[PM_NOMBRE_MAX] = "Proyecto";
    char nombre[PM_NOMBRE_MAX];
    char color[PM_TEXTO_MAX] = "";
    char ruta_recurso[PM_RUTA_MAX];
    char tipo[PM_TEXTO_MAX];
    char evento[PM_TEXTO_MAX];
    int ancho = 0, alto = 0, fps = 0;
    int linea_proyecto = 0, num_linea = 0, n;
    int id, escena, padre, recurso, script, objetivo;
    int inicial = 0;
    ProjectModel *m = NULL;
    EntidadModelo *e;
    char validacion[PM_ERROR_MAX];

    f = fopen(ruta, "r");
    if (!f) {
        ml_fallo(diagnostico, longitud, 0, "no se pudo abrir el .log");
        return NULL;
    }
    m = pm_crear();
    if (!m) {
        fclose(f);
        ml_fallo(diagnostico, longitud, 0, "sin memoria para el modelo");
        return NULL;
    }

    while (fgets(linea, sizeof(linea), f)) {
        num_linea++;
        n = ml_tokenizar(linea, tok, ML_TOKENS_MAX);
        if (n <= 0) continue;
        if (n < 0) {
            ml_fallo(diagnostico, longitud, num_linea, "demasiados campos");
            goto error;
        }
        if (tok[0][0] == '#') continue;
        if (strcmp(tok[0], "LOG") == 0) {
            if (n < 2 || strcmp(tok[1], "2") != 0) {
                ml_fallo(diagnostico, longitud, num_linea,
                         "version LOG no soportada");
                goto error;
            }
            continue;
        }
        if (strcmp(tok[0], "FIN") == 0) break;

        if (strcmp(tok[0], "proyecto") == 0) {
            if (linea_proyecto || !ml_v2_valor(tok, n, "nombre",
                                               nombre_proyecto,
                                               sizeof(nombre_proyecto))) {
                ml_fallo(diagnostico, longitud, num_linea,
                         "proyecto incompleto o duplicado");
                goto error;
            }
            (void)ml_v2_entero(tok, n, "ancho", &ancho, 0);
            (void)ml_v2_entero(tok, n, "alto", &alto, 0);
            (void)ml_v2_entero(tok, n, "fps", &fps, 0);
            (void)ml_v2_valor(tok, n, "fondo", color, sizeof(color));
            linea_proyecto = num_linea;
            continue;
        }

        if (strcmp(tok[0], "recurso") == 0) {
            nombre[0] = '\0';
            if (!ml_v2_entero(tok, n, "id", &id, 1) ||
                !ml_v2_valor(tok, n, "ruta", ruta_recurso,
                             sizeof(ruta_recurso))) {
                ml_fallo(diagnostico, longitud, num_linea,
                         "recurso sin id o ruta");
                goto error;
            }
            if (!ml_v2_valor(tok, n, "tipo", tipo, sizeof(tipo)))
                ml_copiar(tipo, sizeof(tipo), "recurso");
            if (!ml_v2_valor(tok, n, "nombre", nombre, sizeof(nombre)))
                ml_copiar(nombre, sizeof(nombre), ruta_recurso);
            if (!pm_anadir_con_id(m, id, PM_KIND_RECURSO, nombre)) goto error;
            e = pm_entidad(m, id);
            pm_origen(e, ruta, num_linea);
            if (!pm_propiedad(e, "tipo", tipo, num_linea) ||
                !pm_propiedad(e, "ruta", ruta_recurso, num_linea) ||
                !ml_v2_anadir_propiedades(e, tok, n, num_linea)) goto error;
            continue;
        }

        if (strcmp(tok[0], "script") == 0) {
            nombre[0] = '\0';
            if (!ml_v2_entero(tok, n, "id", &id, 1) ||
                !ml_v2_valor(tok, n, "ruta", ruta_recurso,
                             sizeof(ruta_recurso))) {
                ml_fallo(diagnostico, longitud, num_linea,
                         "script sin id o ruta");
                goto error;
            }
            if (!ml_v2_valor(tok, n, "nombre", nombre, sizeof(nombre)))
                ml_copiar(nombre, sizeof(nombre), ruta_recurso);
            if (!pm_anadir_con_id(m, id, PM_KIND_SCRIPT, nombre)) goto error;
            e = pm_entidad(m, id);
            pm_origen(e, ruta, num_linea);
            if (!pm_propiedad(e, "ruta", ruta_recurso, num_linea) ||
                !ml_v2_anadir_propiedades(e, tok, n, num_linea)) goto error;
            continue;
        }

        if (strcmp(tok[0], "escena") == 0) {
            nombre[0] = '\0';
            if (!ml_v2_entero(tok, n, "id", &id, 1) ||
                !ml_v2_valor(tok, n, "nombre", nombre, sizeof(nombre))) {
                ml_fallo(diagnostico, longitud, num_linea,
                         "escena sin id o nombre");
                goto error;
            }
            if (!pm_anadir_con_id(m, id, PM_KIND_ESCENA, nombre)) goto error;
            e = pm_entidad(m, id);
            pm_origen(e, ruta, num_linea);
            (void)ml_v2_entero(tok, n, "inicial", &inicial, 0);
            if (!ml_v2_anadir_propiedades(e, tok, n, num_linea)) goto error;
            continue;
        }

        if (strcmp(tok[0], "nodo") == 0) {
            nombre[0] = '\0';
            if (!ml_v2_entero(tok, n, "id", &id, 1) ||
                !ml_v2_entero(tok, n, "escena", &escena, 1) ||
                !ml_v2_entero(tok, n, "padre", &padre, 1) ||
                !ml_v2_valor(tok, n, "tipo", tipo, sizeof(tipo)) ||
                !ml_v2_valor(tok, n, "nombre", nombre, sizeof(nombre))) {
                ml_fallo(diagnostico, longitud, num_linea,
                         "nodo sin escena, padre, tipo o nombre");
                goto error;
            }
            if (!pm_anadir_con_id(m, id, PM_KIND_NODO, nombre)) goto error;
            e = pm_entidad(m, id);
            pm_origen(e, ruta, num_linea);
            if (!ml_v2_anadir_propiedades(e, tok, n, num_linea) ||
                !pm_referencia(e, "escena", escena, NULL, num_linea)) goto error;
            if (padre > 0 &&
                !pm_referencia(e, "padre", padre, NULL, num_linea)) goto error;
            if (ml_v2_entero(tok, n, "recurso", &recurso, 0) && recurso > 0 &&
                !pm_referencia(e, "recurso", recurso, NULL, num_linea)) goto error;
            continue;
        }

        if (strcmp(tok[0], "asociar") == 0) {
            char objetivo_clave[PM_NOMBRE_MAX];
            int binding_id;
            if (n < 7 || strcmp(tok[1], "script") != 0 ||
                !ml_entero(tok[2], &script) || script <= 0 ||
                (strcmp(tok[3], "nodo") != 0 && strcmp(tok[3], "escena") != 0) ||
                !ml_entero(tok[4], &objetivo) || objetivo <= 0 ||
                strcmp(tok[5], "evento") != 0) {
                ml_fallo(diagnostico, longitud, num_linea,
                         "asociacion de script incompleta");
                goto error;
            }
            ml_copiar(objetivo_clave, sizeof(objetivo_clave), tok[3]);
            ml_copiar(evento, sizeof(evento), tok[6]);
            binding_id = pm_anadir(m, PM_KIND_BINDING, "asociacion");
            if (!binding_id) goto error;
            e = pm_entidad(m, binding_id);
            pm_origen(e, ruta, num_linea);
            if (!pm_propiedad(e, "evento", evento, num_linea) ||
                !pm_propiedad(e, "objetivo_tipo", objetivo_clave, num_linea) ||
                !pm_referencia(e, "script", script, NULL, num_linea) ||
                !pm_referencia(e, objetivo_clave, objetivo, NULL, num_linea))
                goto error;
            continue;
        }

        ml_fallo(diagnostico, longitud, num_linea, "sentencia LOG desconocida");
        goto error;
    }

    if (!linea_proyecto) {
        ml_fallo(diagnostico, longitud, num_linea, "falta la cabecera proyecto");
        goto error;
    }
    id = pm_anadir(m, PM_KIND_PROYECTO, nombre_proyecto);
    if (!id) goto error;
    e = pm_entidad(m, id);
    pm_origen(e, ruta, linea_proyecto);
    {
        char numero[32];
        snprintf(numero, sizeof(numero), "%d", ancho);
        pm_propiedad(e, "ancho", numero, linea_proyecto);
        snprintf(numero, sizeof(numero), "%d", alto);
        pm_propiedad(e, "alto", numero, linea_proyecto);
        snprintf(numero, sizeof(numero), "%d", fps);
        pm_propiedad(e, "fps", numero, linea_proyecto);
        pm_propiedad(e, "fondo", color, linea_proyecto);
    }
    fclose(f);
    if (pm_validar(m, validacion, sizeof(validacion))) {
        ml_copiar(diagnostico, longitud, validacion);
        pm_destruir(m);
        return NULL;
    }
    ml_copiar(diagnostico, longitud, validacion);
    return m;

error:
    fclose(f);
    if (m) pm_destruir(m);
    return NULL;
}

ProjectModel *ml_leer(const char *ruta, char *diagnostico, size_t longitud) {
    FILE *f;
    char linea[ML_LINEA_MAX];
    char tok[ML_TOKENS_MAX][ML_TOKEN_MAX];
    NodoLog nod[ML_NODOS_MAX];
    EnlaceLog enl[ML_ENLACES_MAX];
    char nombre[PM_NOMBRE_MAX] = "Proyecto";
    char color[PM_TEXTO_MAX] = "";
    int ancho = 0, alto = 0, capas = 0, fps = 0, modo = 0;
    int linea_proyecto = 0, num_nodos = 0, num_enlaces = 0;
    int dentro_montaje = 0, num_linea = 0;
    ProjectModel *m;
    EntidadModelo *e;
    int id_proyecto, id_conexion;
    char temporal[PM_RUTA_MAX];
    char validacion[PM_ERROR_MAX];
    int i, j, n;

    if (diagnostico && longitud > 0) diagnostico[0] = '\0';
    if (!ruta || !ruta[0]) {
        ml_fallo(diagnostico, longitud, 0, "falta la ruta del .log");
        return NULL;
    }
    {
        FILE *version_f = fopen(ruta, "r");
        char version_linea[ML_LINEA_MAX];
        int version = 0;
        if (version_f) {
            if (fgets(version_linea, sizeof(version_linea), version_f) &&
                sscanf(version_linea, "LOG %d", &version) == 1 && version == 2) {
                fclose(version_f);
                return ml_leer_v2(ruta, diagnostico, longitud);
            }
            fclose(version_f);
        }
    }
    f = fopen(ruta, "r");
    if (!f) {
        ml_fallo(diagnostico, longitud, 0, "no se pudo abrir el .log");
        return NULL;
    }

    while (fgets(linea, sizeof(linea), f)) {
        num_linea++;
        n = ml_tokenizar(linea, tok, ML_TOKENS_MAX);
        if (n <= 0) continue;
        if (n < 0) {
            fclose(f);
            ml_fallo(diagnostico, longitud, num_linea, "demasiados campos");
            return NULL;
        }
        if (tok[0][0] == '#') continue;

        if (strcmp(tok[0], "montaje") == 0) {
            dentro_montaje = 1;
            continue;
        }
        if (strcmp(tok[0], "fin_montaje") == 0) {
            dentro_montaje = 0;
            continue;
        }
        if (strcmp(tok[0], "proyecto") == 0) {
            if (linea_proyecto) {
                fclose(f);
                ml_fallo(diagnostico, longitud, num_linea, "proyecto duplicado");
                return NULL;
            }
            if (n < 8) {
                fclose(f);
                ml_fallo(diagnostico, longitud, num_linea, "proyecto incompleto");
                return NULL;
            }
            linea_proyecto = num_linea;
            ml_copiar(nombre, sizeof(nombre), tok[1]);
            if (!ml_entero(tok[2], &ancho) || !ml_entero(tok[3], &alto) ||
                !ml_entero(tok[4], &capas) || !ml_entero(tok[5], &fps) ||
                !ml_entero(tok[6], &modo)) {
                fclose(f);
                ml_fallo(diagnostico, longitud, num_linea,
                         "campo del proyecto no numerico");
                return NULL;
            }
            ml_copiar(color, sizeof(color), tok[7]);
            continue;
        }
        if (!dentro_montaje) continue;

        if (strcmp(tok[0], "nodo") == 0) {
            NodoLog *nd;
            if (num_nodos >= ML_NODOS_MAX) {
                fclose(f);
                ml_fallo(diagnostico, longitud, num_linea, "demasiados nodos");
                return NULL;
            }
            if (n < 6 || !ml_entero(tok[1], &i) || i <= 0) {
                fclose(f);
                ml_fallo(diagnostico, longitud, num_linea, "nodo sin id valido");
                return NULL;
            }
            if (ml_buscar_nodo(nod, num_nodos, i)) {
                fclose(f);
                ml_fallo(diagnostico, longitud, num_linea,
                         "ID de nodo duplicado");
                return NULL;
            }
            nd = &nod[num_nodos];
            memset(nd, 0, sizeof(*nd));
            nd->id = i;
            nd->linea = num_linea;
            for (j = 2; j + 1 < n; j += 2) {
                const char *clave = tok[j];
                const char *valor = tok[j + 1];
                if (strcmp(clave, "tipo") == 0)
                    ml_copiar(nd->tipo, sizeof(nd->tipo), valor);
                else if (strcmp(clave, "nombre") == 0)
                    ml_copiar(nd->nombre, sizeof(nd->nombre), valor);
                else if (strcmp(clave, "x") == 0) {
                    if (!ml_entero(valor, &nd->x)) {
                        fclose(f);
                        ml_fallo(diagnostico, longitud, num_linea,
                                 "x de nodo no numerica");
                        return NULL;
                    }
                } else if (strcmp(clave, "y") == 0) {
                    if (!ml_entero(valor, &nd->y)) {
                        fclose(f);
                        ml_fallo(diagnostico, longitud, num_linea,
                                 "y de nodo no numerica");
                        return NULL;
                    }
                } else if (strcmp(clave, "enlace") == 0)
                    ml_copiar(nd->enlace, sizeof(nd->enlace), valor);
                else if (strcmp(clave, "valor") == 0)
                    ml_copiar(nd->valor, sizeof(nd->valor), valor);
                else if (strcmp(clave, "valor2") == 0)
                    ml_copiar(nd->valor2, sizeof(nd->valor2), valor);
            }
            nd->kind = ml_kind(nd->tipo);
            if (!nd->kind) {
                fclose(f);
                ml_fallo(diagnostico, longitud, num_linea,
                         "tipo de nodo no soportado");
                return NULL;
            }
            if (!nd->nombre[0])
                ml_copiar(nd->nombre, sizeof(nd->nombre), nd->tipo);
            num_nodos++;
            continue;
        }

        if (strcmp(tok[0], "conecta") == 0) {
            EnlaceLog *ln;
            if (num_enlaces >= ML_ENLACES_MAX) {
                fclose(f);
                ml_fallo(diagnostico, longitud, num_linea,
                         "demasiadas conexiones");
                return NULL;
            }
            if (n < 4 || strcmp(tok[2], "->") != 0) {
                fclose(f);
                ml_fallo(diagnostico, longitud, num_linea,
                         "conecta incompleto");
                return NULL;
            }
            ln = &enl[num_enlaces];
            memset(ln, 0, sizeof(*ln));
            ln->linea = num_linea;
            if (!ml_extremo(tok[1], &ln->origen, ln->puerto_origen,
                            sizeof(ln->puerto_origen), "salida") ||
                !ml_extremo(tok[3], &ln->destino, ln->puerto_destino,
                            sizeof(ln->puerto_destino), "entrada")) {
                fclose(f);
                ml_fallo(diagnostico, longitud, num_linea,
                         "conecta con ID no valido");
                return NULL;
            }
            if (!ml_buscar_nodo(nod, num_nodos, ln->origen) ||
                !ml_buscar_nodo(nod, num_nodos, ln->destino)) {
                fclose(f);
                ml_fallo(diagnostico, longitud, num_linea,
                         "conecta referencia un nodo inexistente");
                return NULL;
            }
            num_enlaces++;
            continue;
        }
    }
    fclose(f);

    if (!linea_proyecto) {
        ml_fallo(diagnostico, longitud, num_linea, "falta la cabecera proyecto");
        return NULL;
    }

    m = pm_crear();
    if (!m) {
        ml_fallo(diagnostico, longitud, 0, "sin memoria para el modelo");
        return NULL;
    }

    for (i = 0; i < num_nodos; i++) {
        const NodoLog *nd = &nod[i];
        id_conexion = pm_anadir_con_id(m, nd->id, nd->kind, nd->nombre);
        if (!id_conexion) goto error_modelo;
        e = pm_entidad(m, id_conexion);
        pm_origen(e, ruta, nd->linea);
        pm_ui(e, nd->x, nd->y, 0, 0, 0, 0, 0);
        if (!pm_propiedad(e, "tipo", nd->tipo, nd->linea) ||
            !pm_propiedad(e, "enlace", nd->enlace, nd->linea) ||
            !pm_propiedad(e, "valor", nd->valor, nd->linea) ||
            !pm_propiedad(e, "valor2", nd->valor2, nd->linea))
            goto error_modelo;
        if (nd->kind == PM_KIND_RECURSO) {
            if (nd->valor2[0]) {
                snprintf(temporal, sizeof(temporal), "%s/%s",
                         nd->valor2, nd->valor);
            } else {
                ml_copiar(temporal, sizeof(temporal), nd->valor);
            }
            for (j = 0; temporal[j]; j++)
                if (temporal[j] == '\\') temporal[j] = '/';
            if (!pm_propiedad(e, "ruta", temporal, nd->linea))
                goto error_modelo;
        }
    }

    id_proyecto = pm_anadir(m, PM_KIND_PROYECTO, nombre);
    if (!id_proyecto) goto error_modelo;
    e = pm_entidad(m, id_proyecto);
    pm_origen(e, ruta, linea_proyecto);
    snprintf(temporal, sizeof(temporal), "%d", ancho);
    pm_propiedad(e, "ancho", temporal, linea_proyecto);
    snprintf(temporal, sizeof(temporal), "%d", alto);
    pm_propiedad(e, "alto", temporal, linea_proyecto);
    snprintf(temporal, sizeof(temporal), "%d", capas);
    pm_propiedad(e, "capas", temporal, linea_proyecto);
    snprintf(temporal, sizeof(temporal), "%d", fps);
    pm_propiedad(e, "fps", temporal, linea_proyecto);
    snprintf(temporal, sizeof(temporal), "%d", modo);
    pm_propiedad(e, "modo", temporal, linea_proyecto);
    pm_propiedad(e, "color", color, linea_proyecto);

    for (i = 0; i < num_enlaces; i++) {
        id_conexion = pm_conectar(m, enl[i].origen, enl[i].puerto_origen,
                                  enl[i].destino, enl[i].puerto_destino,
                                  enl[i].linea);
        if (!id_conexion) goto error_modelo;
        pm_origen(pm_entidad(m, id_conexion), ruta, enl[i].linea);
    }

    if (pm_validar(m, validacion, sizeof(validacion))) {
        ml_copiar(diagnostico, longitud, validacion);
        pm_destruir(m);
        return NULL;
    }
    ml_copiar(diagnostico, longitud, validacion);
    return m;

error_modelo:
    ml_copiar(validacion, sizeof(validacion), pm_error(m));
    if (!validacion[0])
        ml_copiar(validacion, sizeof(validacion), "no cabe la pieza en el modelo");
    ml_copiar(diagnostico, longitud, validacion);
    pm_destruir(m);
    return NULL;
}
