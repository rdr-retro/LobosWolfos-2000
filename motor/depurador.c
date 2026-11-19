/* depurador.c - estado y puntos de parada del depurador (fase 34).
 *
 * El modulo no abre ventanas ni conoce el bucle del juego: expone un estado
 * que game.exe consulta y que la VM y el planificador actualizan mediante
 * hooks. Sin --depurar los hooks no se registran y el coste es cero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "depurador.h"
#include "lobo.h"
#include "logic3_motor.h"

typedef struct Depurador {
    int activo;
    int pausado;
    int modo;
    int cantidad;
    int pasos_bloque;
    int pasos_instruccion;
    int cortar;
    int bloque, evento, script, pc, error, fotograma;
    char observados[DEPURADOR_OBS_MAX][DEPURADOR_NOMBRE_MAX];
    int indices[DEPURADOR_OBS_MAX]; /* -1 = buscar por nombre */
    int num_observados;
    int break_bloques[DEPURADOR_BREAKS_MAX];
    int num_break_bloques;
    int break_eventos[DEPURADOR_BREAKS_MAX];
    int num_break_eventos;
    char consola[DEPURADOR_CONSOLA_MAX][DEPURADOR_CONSOLA_TEXTO_MAX];
    int consola_ini, consola_num;
    unsigned *entradas;
    int num_entradas, cap_entradas;
    int grabando;
    int tiene_replay;
    char mensaje[DEPURADOR_NOMBRE_MAX];
    int semilla;
    int volcado_iniciado;
} Depurador;

static Depurador dep;

/* ------------------------------------------------------------------ */
/* Estado basico                                                       */
/* ------------------------------------------------------------------ */

void depurador_iniciar(void) {
    memset(&dep, 0, sizeof(dep));
    dep.semilla = 12345;
}

void depurador_terminar(void) {
    free(dep.entradas);
    dep.entradas = NULL;
    dep.num_entradas = 0;
    dep.cap_entradas = 0;
}

void depurador_activar(int valor) {
    dep.activo = valor ? 1 : 0;
    if (!dep.activo) dep.pausado = 0;
}

int depurador_esta_activo(void) { return dep.activo; }

void depurador_pausar(void) {
    if (!dep.activo) return;
    dep.pausado = 1;
}

void depurador_reanudar(void) {
    dep.pausado = 0;
    dep.cortar = 0;
    if (dep.modo == DEPURADOR_PASO_INSTRUCCION) {
        dep.pasos_instruccion = dep.cantidad > 0 ? dep.cantidad : 1;
    } else if (dep.modo == DEPURADOR_PASO_BLOQUE) {
        dep.pasos_bloque = 1;
    }
}

int depurador_esta_pausado(void) { return dep.activo && dep.pausado; }

void depurador_paso(int modo, int cantidad) {
    dep.modo = modo;
    dep.cantidad = cantidad;
    if (modo == DEPURADOR_PASO_INSTRUCCION)
        dep.pasos_instruccion = cantidad > 0 ? cantidad : 1;
    else if (modo == DEPURADOR_PASO_BLOQUE)
        dep.pasos_bloque = 1;
}

int depurador_modo_paso(void) { return dep.modo; }

void depurador_fotograma(int valor) { dep.fotograma = valor; }
int depurador_fotograma_actual(void) { return dep.fotograma; }

int depurador_bloque_actual(void) { return dep.bloque; }
int depurador_evento_actual(void) { return dep.evento; }
int depurador_script_actual(void) { return dep.script; }
int depurador_pc_actual(void) { return dep.pc; }
int depurador_error_actual(void) { return dep.error; }

/* ------------------------------------------------------------------ */
/* Consola                                                             */
/* ------------------------------------------------------------------ */

void depurador_consola(const char *texto) {
    int i;
    if (!texto || !texto[0]) return;
    if (dep.consola_num < DEPURADOR_CONSOLA_MAX) {
        i = (dep.consola_ini + dep.consola_num) % DEPURADOR_CONSOLA_MAX;
        dep.consola_num++;
    } else {
        i = dep.consola_ini;
        dep.consola_ini = (dep.consola_ini + 1) % DEPURADOR_CONSOLA_MAX;
    }
    strncpy(dep.consola[i], texto, DEPURADOR_CONSOLA_TEXTO_MAX - 1);
    dep.consola[i][DEPURADOR_CONSOLA_TEXTO_MAX - 1] = '\0';
}

int depurador_num_consola(void) { return dep.consola_num; }

const char *depurador_consola_linea(int i) {
    if (i < 0 || i >= dep.consola_num) return "";
    return dep.consola[(dep.consola_ini + i) % DEPURADOR_CONSOLA_MAX];
}

/* ------------------------------------------------------------------ */
/* Observacion y breakpoints                                           */
/* ------------------------------------------------------------------ */

void depurador_observar(const char *nombre) {
    char limpio[DEPURADOR_NOMBRE_MAX];
    char *p;
    int indice = -1, i, n;
    if (!nombre || !nombre[0]) return;
    strncpy(limpio, nombre, sizeof(limpio) - 1);
    limpio[sizeof(limpio) - 1] = '\0';
    /* `nombre:indice` permite observar una global aunque el lector del
     * motor no conserve su nombre (las variables globales de LOGIC3 se
     * identifican por posicion de declaracion). */
    for (p = limpio; *p; p++) {
        if (*p == ':' && p[1]) {
            indice = atoi(p + 1);
            *p = '\0';
            break;
        }
    }
    if (!limpio[0]) return;
    for (i = 0; i < dep.num_observados; i++)
        if (strcmp(dep.observados[i], limpio) == 0) return;
    n = dep.num_observados;
    if (n >= DEPURADOR_OBS_MAX) return;
    memcpy(dep.observados[n], limpio, sizeof(dep.observados[n]) - 1);
    dep.observados[n][DEPURADOR_NOMBRE_MAX - 1] = '\0';
    dep.indices[n] = indice;
    dep.num_observados++;
}

int depurador_num_observados(void) { return dep.num_observados; }

const char *depurador_observado(int i) {
    if (i < 0 || i >= dep.num_observados) return "";
    return dep.observados[i];
}

void depurador_break_bloque(int bloque) {
    int i;
    for (i = 0; i < dep.num_break_bloques; i++)
        if (dep.break_bloques[i] == bloque) return;
    if (dep.num_break_bloques >= DEPURADOR_BREAKS_MAX) return;
    dep.break_bloques[dep.num_break_bloques++] = bloque;
}

void depurador_break_evento(int evento) {
    int i;
    for (i = 0; i < dep.num_break_eventos; i++)
        if (dep.break_eventos[i] == evento) return;
    if (dep.num_break_eventos >= DEPURADOR_BREAKS_MAX) return;
    dep.break_eventos[dep.num_break_eventos++] = evento;
}

int depurador_num_break_bloques(void) { return dep.num_break_bloques; }
int depurador_num_break_eventos(void) { return dep.num_break_eventos; }

/* ------------------------------------------------------------------ */
/* Puntos de parada                                                    */
/* ------------------------------------------------------------------ */

int depurador_antes_bloque(int bloque, int evento, int script) {
    char msg[DEPURADOR_CONSOLA_TEXTO_MAX];
    int i;
    if (!dep.activo) return 0;
    dep.bloque = bloque;
    dep.evento = evento;
    dep.script = script;
    dep.error = 0;
    for (i = 0; i < dep.num_break_bloques; i++) {
        if (dep.break_bloques[i] == bloque) {
            _snprintf(msg, sizeof(msg), "break bloque %d", bloque);
            msg[sizeof(msg) - 1] = '\0';
            depurador_consola(msg);
            depurador_pausar();
            return 1;
        }
    }
    for (i = 0; i < dep.num_break_eventos; i++) {
        if (dep.break_eventos[i] == evento) {
            _snprintf(msg, sizeof(msg), "break evento %d", evento);
            msg[sizeof(msg) - 1] = '\0';
            depurador_consola(msg);
            depurador_pausar();
            return 1;
        }
    }
    return 0;
}

int depurador_despues_bloque(int bloque, int evento, int script, int pc,
                             int error) {
    if (!dep.activo) return 0;
    dep.bloque = bloque;
    dep.evento = evento;
    dep.script = script;
    dep.pc = pc;
    dep.error = error;
    if (dep.modo == DEPURADOR_PASO_BLOQUE && dep.pasos_bloque > 0) {
        dep.pasos_bloque--;
        if (dep.pasos_bloque <= 0) {
            dep.modo = DEPURADOR_PASO_LIBRE;
            depurador_consola("paso de bloque completado");
            depurador_pausar();
            return 1;
        }
    }
    return 0;
}

/* Se llama desde el hook de la VM antes de ejecutar cada instruccion. */
void depurador_instruccion(int pc, int error) {
    char msg[DEPURADOR_CONSOLA_TEXTO_MAX];
    if (!dep.activo) return;
    dep.pc = pc;
    if (error) {
        dep.error = 1;
        _snprintf(msg, sizeof(msg), "error en pc %d", pc);
        msg[sizeof(msg) - 1] = '\0';
        depurador_consola(msg);
    }
    if (dep.modo == DEPURADOR_PASO_INSTRUCCION && dep.pasos_instruccion > 0) {
        dep.pasos_instruccion--;
        if (dep.pasos_instruccion <= 0 && !dep.pausado) {
            /* El paso es de un solo disparo: al cumplirse vuelve a la
             * ejecucion libre para no contar instrucciones indefinidamente. */
            dep.modo = DEPURADOR_PASO_LIBRE;
            depurador_consola("paso de instruccion completado");
            depurador_pausar();
            dep.cortar = 1;
        }
    }
}

int depurador_cortar_instrucciones(void) { return dep.cortar; }

/* ------------------------------------------------------------------ */
/* Semilla y replay                                                    */
/* ------------------------------------------------------------------ */

void depurador_semilla(int valor) { dep.semilla = valor; }
int depurador_semilla_valor(void) { return dep.semilla; }

unsigned depurador_entrada(int fotograma, unsigned actual) {
    if (dep.tiene_replay && fotograma >= 0 && fotograma < dep.num_entradas)
        return dep.entradas[fotograma];
    return actual;
}

void depurador_grabar_entrada(int fotograma, unsigned entrada) {
    if (!dep.grabando) return;
    if (fotograma < 0 || fotograma >= DEPURADOR_REPLAY_MAX) return;
    if (fotograma >= dep.num_entradas) {
        int desde = dep.num_entradas, i;
        int nuevo = desde + 64;
        unsigned *tmp;
        if (nuevo > DEPURADOR_REPLAY_MAX) nuevo = DEPURADOR_REPLAY_MAX;
        tmp = (unsigned *)realloc(dep.entradas, (size_t)nuevo * sizeof(unsigned));
        if (!tmp) return;
        dep.entradas = tmp;
        dep.cap_entradas = nuevo;
        for (i = desde; i < nuevo; i++) dep.entradas[i] = 0;
    }
    dep.entradas[fotograma] = entrada;
    if (fotograma + 1 > dep.num_entradas) dep.num_entradas = fotograma + 1;
}

void depurador_replay_grabar(int valor) {
    dep.grabando = valor ? 1 : 0;
    if (dep.grabando) {
        dep.tiene_replay = 0;
        dep.num_entradas = 0;
    }
}

void depurador_replay_mensaje(const char *texto) {
    if (!texto) { dep.mensaje[0] = '\0'; return; }
    strncpy(dep.mensaje, texto, DEPURADOR_NOMBRE_MAX - 1);
    dep.mensaje[DEPURADOR_NOMBRE_MAX - 1] = '\0';
}

int depurador_guardar_replay(const char *ruta) {
    FILE *f;
    int i;
    if (!ruta || !ruta[0]) return 1;
    f = fopen(ruta, "wt");
    if (!f) return 1;
    fprintf(f, "replay 1\n");
    fprintf(f, "semilla %d\n", dep.semilla);
    fprintf(f, "mensaje %s\n", dep.mensaje);
    fprintf(f, "fotogramas %d\n", dep.num_entradas);
    for (i = 0; i < dep.num_entradas; i++)
        fprintf(f, "%u\n", dep.entradas[i]);
    fclose(f);
    return 0;
}

int depurador_cargar_replay(const char *ruta, char *mensaje, int tam_mensaje,
                            int *semilla) {
    FILE *f;
    char linea[256];
    int n = 0;
    if (!ruta || !ruta[0]) return 1;
    f = fopen(ruta, "rt");
    if (!f) return 1;
    dep.num_entradas = 0;
    while (n < DEPURADOR_REPLAY_MAX && fgets(linea, sizeof(linea), f)) {
        char *p = linea;
        unsigned v;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '#') continue;
        if (strncmp(p, "replay", 6) == 0) continue;
        if (sscanf(p, "semilla %d", &dep.semilla) == 1) continue;
        if (strncmp(p, "mensaje", 7) == 0) {
            const char *m = p + 7;
            while (*m == ' ' || *m == '\t') m++;
            depurador_replay_mensaje(m);
            continue;
        }
        if (strncmp(p, "fotogramas", 10) == 0) continue;
        if (sscanf(p, "%u", &v) != 1) continue;
        if (n >= dep.cap_entradas) {
            int nuevo = n + 128, i;
            unsigned *tmp;
            if (nuevo > DEPURADOR_REPLAY_MAX) nuevo = DEPURADOR_REPLAY_MAX;
            tmp = (unsigned *)realloc(dep.entradas,
                                      (size_t)nuevo * sizeof(unsigned));
            if (!tmp) break;
            dep.entradas = tmp;
            dep.cap_entradas = nuevo;
            for (i = n; i < nuevo; i++) dep.entradas[i] = 0;
        }
        dep.entradas[n++] = v;
    }
    fclose(f);
    dep.num_entradas = n;
    dep.tiene_replay = 1;
    dep.grabando = 0;
    if (mensaje && tam_mensaje > 0) {
        strncpy(mensaje, dep.mensaje, tam_mensaje - 1);
        mensaje[tam_mensaje - 1] = '\0';
    }
    if (semilla) *semilla = dep.semilla;
    return 0;
}

int depurador_tiene_replay(void) { return dep.tiene_replay; }

/* ------------------------------------------------------------------ */
/* Volcado y comparacion                                               */
/* ------------------------------------------------------------------ */

int depurador_observado_valor(int i, const void *memoria,
                              const void *proyecto, int *encontrado) {
    const LoboMemoria *mem = (const LoboMemoria *)memoria;
    const MotorL3 *l3 = (const MotorL3 *)proyecto;
    int j;
    if (encontrado) *encontrado = 0;
    if (i < 0 || i >= dep.num_observados || !mem) return 0;
    if (dep.indices[i] >= 0 && dep.indices[i] < LOBO_GLOBALES_MAX) {
        if (encontrado) *encontrado = 1;
        return mem->globales[dep.indices[i]];
    }
    if (l3) {
        for (j = 0; j < l3->num_variables && j < LOBO_GLOBALES_MAX; j++) {
            if (strcmp(l3->variables[j].nombre, dep.observados[i]) == 0) {
                if (encontrado) *encontrado = 1;
                return mem->globales[j];
            }
        }
    }
    return 0;
}

void depurador_volcar(const char *ruta, const void *memoria,
                      const void *proyecto, int final) {
    FILE *f;
    int i;
    if (!ruta || !ruta[0]) return;
    f = fopen(ruta, dep.volcado_iniciado ? "at" : "wt");
    if (!f) return;
    dep.volcado_iniciado = 1;
    fprintf(f, "pausa %d %d %d %d %d %d\n", dep.fotograma, dep.bloque,
            dep.evento, dep.script, dep.pc, dep.error);
    for (i = 0; i < dep.num_observados; i++) {
        int encontrado = 0;
        int valor = depurador_observado_valor(i, memoria, proyecto,
                                              &encontrado);
        if (encontrado)
            fprintf(f, "obs %s=%d\n", depurador_observado(i), valor);
        else
            fprintf(f, "obs %s=?\n", depurador_observado(i));
    }
    for (i = 0; i < dep.consola_num; i++)
        fprintf(f, "consola %s\n", depurador_consola_linea(i));
    if (final) {
        fprintf(f, "fin\n");
        dep.volcado_iniciado = 0;
    }
    fclose(f);
}

/* Quita el salto de linea que fgets deja al final de cada lectura. */
static void quitar_salto(char *linea) {
    size_t n = strlen(linea);
    while (n > 0 && (linea[n - 1] == '\r' || linea[n - 1] == '\n'))
        linea[--n] = '\0';
}

int depurador_comparar(const char *ruta_a, const char *ruta_b,
                       const char *ruta_salida) {
    FILE *a, *b, *sal;
    char la[512], lb[512];
    int n = 0, divergencia = 0;
    int ra = 1, rb = 1;

    if (!ruta_a || !ruta_b || !ruta_salida) return 2;
    a = fopen(ruta_a, "rt");
    b = fopen(ruta_b, "rt");
    sal = fopen(ruta_salida, "wt");
    if (!a || !b || !sal) {
        if (a) fclose(a);
        if (b) fclose(b);
        if (sal) fclose(sal);
        return 2;
    }
    while (ra || rb) {
        if (ra && !fgets(la, sizeof(la), a)) ra = 0;
        if (rb && !fgets(lb, sizeof(lb), b)) rb = 0;
        if (ra) quitar_salto(la);
        if (rb) quitar_salto(lb);
        n++;
        if ((ra != rb) || (ra && rb && strcmp(la, lb) != 0)) {
            fprintf(sal, "divergencia %d %s %s\n", n,
                    ra ? la : "<fin>", rb ? lb : "<fin>");
            divergencia = 1;
            break;
        }
        if (!ra && !rb) break;
    }
    if (!divergencia) fprintf(sal, "sin divergencias\n");
    fclose(a);
    fclose(b);
    fclose(sal);
    return divergencia;
}
