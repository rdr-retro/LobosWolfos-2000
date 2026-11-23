/* inicio.c - servicio de nodos con historial compartido (plan 17.1).
 *
 * Capa delgada sobre ProjectModel: guarda un snapshot antes de cada cambio y
 * lo intercambia con el futuro al deshacer o rehacer. No depende de Win32 ni
 * de la interfaz, asi que se puede probar con la flag --inicio sin abrir la
 * aplicacion. Los snapshots viven en la instancia estatica de inicio.h.
 */
#include "inicio.h"
#include "modelo_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copiar_seguro(char *dst, size_t tam, const char *src) {
    size_t n;
    if (!dst || tam == 0) return;
    if (!src) src = "";
    n = strlen(src);
    if (n >= tam) n = tam - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void diag_fijar(char *dst, size_t tam, const char *texto) {
    if (dst && tam) copiar_seguro(dst, tam, texto);
}

static void ultimo_fijar(Inicio *in, const char *texto) {
    if (in) copiar_seguro(in->ultimo, sizeof(in->ultimo), texto);
}

static void limpiar_espacios(char *texto) {
    size_t i;
    for (i = 0; texto && texto[i]; i++)
        if (texto[i] == ' ' || texto[i] == '\t') texto[i] = '_';
}

static void recortar(char *texto) {
    size_t n, primero, ultimo;
    if (!texto) return;
    n = strlen(texto);
    primero = 0;
    while (primero < n && (texto[primero] == ' ' || texto[primero] == '\t'))
        primero++;
    ultimo = n;
    while (ultimo > primero &&
           (texto[ultimo - 1] == ' ' || texto[ultimo - 1] == '\t'))
        ultimo--;
    if (primero) memmove(texto, texto + primero, ultimo - primero);
    texto[ultimo - primero] = '\0';
}

/* Parte "campo|campo;campo" sin tocar la cadena original. */
static void trozo(const char **p, char *dst, size_t tam) {
    size_t n = 0;
    if (!dst || tam == 0) return;
    if (!p || !*p) {
        dst[0] = '\0';
        return;
    }
    while (**p && **p != '|' && **p != ';' && n + 1 < tam) {
        dst[n++] = **p;
        (*p)++;
    }
    dst[n] = '\0';
    if (**p == '|') (*p)++;
}

static int numero(const char *texto) {
    if (!texto || !texto[0]) return 0;
    return atoi(texto);
}

static void empujar(ProjectModel *pila, int *num, const ProjectModel *modelo) {
    int i;
    if (*num >= INICIO_HISTORIAL_MAX) {
        for (i = 1; i < INICIO_HISTORIAL_MAX; i++)
            pila[i - 1] = pila[i];
        *num = INICIO_HISTORIAL_MAX - 1;
    }
    pm_copiar(modelo, &pila[*num]);
    (*num)++;
}

/* Todo cambio valido descarta el futuro, como en cualquier editor. */
static void guardar_pasado(Inicio *in) {
    if (!in) return;
    empujar(in->pasado, &in->num_pasado, &in->modelo);
    in->num_futuro = 0;
}

Inicio *inicio_servicio(void) {
    static Inicio servicio;
    static int listo = 0;
    if (!listo) {
        inicio_iniciar(&servicio);
        listo = 1;
    }
    return &servicio;
}

void inicio_iniciar(Inicio *in) {
    if (!in) return;
    memset(in, 0, sizeof(*in));
    in->modelo.siguiente_id = 1;
}

int inicio_cargar(Inicio *in, const char *ruta, char *diagnostico,
                  size_t longitud) {
    ProjectModel *m;
    diag_fijar(diagnostico, longitud, "");
    if (!in || !ruta || !ruta[0]) {
        diag_fijar(diagnostico, longitud, "falta la ruta del .log");
        return 0;
    }
    m = ml_leer(ruta, diagnostico, longitud);
    if (!m) {
        if (diagnostico && longitud && !diagnostico[0])
            diag_fijar(diagnostico, longitud, "no se pudo leer el .log");
        return 0;
    }
    /* ml_leer escribe "sin errores" cuando todo va bien; aqui solo interesa
     * el diagnostico de fallo, asi que se limpia para no confundirlo con el
     * detalle de una orden posterior. */
    diag_fijar(diagnostico, longitud, "");
    /* Cargar empieza una nueva sesion: el historial anterior no aplica. */
    inicio_iniciar(in);
    pm_copiar(m, &in->modelo);
    copiar_seguro(in->origen, sizeof(in->origen), ruta);
    ultimo_fijar(in, "cargado");
    pm_destruir(m);
    return 1;
}

int inicio_crear(Inicio *in, int kind, const char *nombre, int id_padre) {
    int id;
    char error[PM_ERROR_MAX];

    if (!in) return 0;
    guardar_pasado(in);
    id = pm_anadir(&in->modelo, kind, nombre);
    if (id && id_padre &&
        !pm_reparentar(&in->modelo, id, id_padre)) id = 0;
    if (!id) {
        copiar_seguro(error, sizeof(error), pm_error(&in->modelo));
        pm_copiar(&in->pasado[in->num_pasado - 1], &in->modelo);
        in->num_pasado--;
        in->num_futuro = 0;
        ultimo_fijar(in, error);
        return 0;
    }
    ultimo_fijar(in, "creado");
    return 1;
}

int inicio_borrar(Inicio *in, int id) {
    int borrados;
    char error[PM_ERROR_MAX];

    if (!in) return 0;
    guardar_pasado(in);
    borrados = pm_borrar(&in->modelo, id);
    if (!borrados) {
        copiar_seguro(error, sizeof(error), pm_error(&in->modelo));
        pm_copiar(&in->pasado[in->num_pasado - 1], &in->modelo);
        in->num_pasado--;
        in->num_futuro = 0;
        ultimo_fijar(in, error);
        return 0;
    }
    ultimo_fijar(in, "borrado");
    return 1;
}

int inicio_reparentar(Inicio *in, int id, int id_padre) {
    char error[PM_ERROR_MAX];

    if (!in) return 0;
    guardar_pasado(in);
    if (!pm_reparentar(&in->modelo, id, id_padre)) {
        copiar_seguro(error, sizeof(error), pm_error(&in->modelo));
        pm_copiar(&in->pasado[in->num_pasado - 1], &in->modelo);
        in->num_pasado--;
        in->num_futuro = 0;
        ultimo_fijar(in, error);
        return 0;
    }
    ultimo_fijar(in, "reparentado");
    return 1;
}

int inicio_deshacer(Inicio *in) {
    if (!in || in->num_pasado <= 0) {
        ultimo_fijar(in, "no hay cambios que deshacer");
        return 0;
    }
    empujar(in->futuro, &in->num_futuro, &in->modelo);
    in->num_pasado--;
    pm_copiar(&in->pasado[in->num_pasado], &in->modelo);
    ultimo_fijar(in, "deshecho");
    return 1;
}

int inicio_rehacer(Inicio *in) {
    if (!in || in->num_futuro <= 0) {
        ultimo_fijar(in, "no hay cambios que rehacer");
        return 0;
    }
    empujar(in->pasado, &in->num_pasado, &in->modelo);
    in->num_futuro--;
    pm_copiar(&in->futuro[in->num_futuro], &in->modelo);
    ultimo_fijar(in, "rehecho");
    return 1;
}

void inicio_volcar(const Inicio *in, const char *ruta) {
    char ultimo[PM_ERROR_MAX];
    FILE *f;

    if (!in || !ruta || !ruta[0]) return;
    pm_volcar(&in->modelo, ruta);
    copiar_seguro(ultimo, sizeof(ultimo), in->ultimo);
    limpiar_espacios(ultimo);
    f = fopen(ruta, "a");
    if (!f) return;
    fprintf(f, "inicio undo=%d redo=%d ultimo=%s\n",
            in->num_pasado, in->num_futuro, ultimo);
    fclose(f);
}

int inicio_kind(const char *texto) {
    static const char *nombres[] = {
        "proyecto", "recurso", "escena", "actor",
        "instancia", "control", "conexion", "accion"
    };
    int i, n;
    if (!texto || !texto[0]) return 0;
    for (i = 0; i < 8; i++)
        if (_stricmp(texto, nombres[i]) == 0) return i + 1;
    n = numero(texto);
    return n > 0 ? n : 0;
}

int inicio_ejecutar(Inicio *in, const char *comandos, char *diagnostico,
                    size_t longitud) {
    const char *p;
    int num = 0;

    diag_fijar(diagnostico, longitud, "");
    if (!in || !comandos) {
        diag_fijar(diagnostico, longitud, "faltan datos para Inicio");
        return 0;
    }
    p = comandos;
    while (*p) {
        char cmd[32], a[PM_RUTA_MAX], b[PM_NOMBRE_MAX], c[32];
        char mensaje[PM_ERROR_MAX * 2];
        const char *detalle;
        int ok = 0;

        trozo(&p, cmd, sizeof(cmd));
        recortar(cmd);
        if (!cmd[0]) {
            if (*p == ';') p++;
            continue;
        }
        if (strcmp(cmd, "cargar") == 0) {
            trozo(&p, a, sizeof(a));
            recortar(a);
            ok = inicio_cargar(in, a, diagnostico, longitud);
        } else if (strcmp(cmd, "crear") == 0) {
            trozo(&p, a, sizeof(a));
            trozo(&p, b, sizeof(b));
            trozo(&p, c, sizeof(c));
            recortar(a);
            recortar(b);
            recortar(c);
            ok = inicio_crear(in, inicio_kind(a), b, numero(c));
        } else if (strcmp(cmd, "borrar") == 0) {
            trozo(&p, a, sizeof(a));
            recortar(a);
            ok = inicio_borrar(in, numero(a));
        } else if (strcmp(cmd, "reparentar") == 0) {
            trozo(&p, a, sizeof(a));
            trozo(&p, b, sizeof(b));
            recortar(a);
            recortar(b);
            ok = inicio_reparentar(in, numero(a), numero(b));
        } else if (strcmp(cmd, "deshacer") == 0) {
            ok = inicio_deshacer(in);
        } else if (strcmp(cmd, "rehacer") == 0) {
            ok = inicio_rehacer(in);
        } else if (strcmp(cmd, "volcar") == 0) {
            trozo(&p, a, sizeof(a));
            recortar(a);
            inicio_volcar(in, a);
            ok = a[0] != '\0';
        } else {
            diag_fijar(diagnostico, longitud, "comando desconocido");
        }
        if (!ok) {
            detalle = in->ultimo[0] ? in->ultimo : "operacion incorrecta";
            if (diagnostico && longitud && diagnostico[0]) detalle = diagnostico;
            snprintf(mensaje, sizeof(mensaje), "comando %d (%s): %s",
                     num + 1, cmd, detalle);
            diag_fijar(diagnostico, longitud, mensaje);
            return 0;
        }
        num++;
        while (*p && *p != ';') p++;
        if (*p == ';') p++;
    }
    return 1;
}
