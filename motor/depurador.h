/* depurador.h - depurador del motor egine (fase 34).
 *
 * Modulo de C puro sin ventana. Guarda el estado de una sesion de
 * depuracion: pausa, paso por bloque o por instruccion, breakpoints,
 * variables observadas, consola de mensajes y errores, y grabacion o
 * reproduccion determinista de la entrada.
 *
 * Los hooks son opcionales: mientras no se activa --depurar, ninguna
 * llamada tiene efecto y las trazas de las fases anteriores quedan
 * exactamente igual. Los enlazan game.exe y el editor.
 */
#ifndef DEPURADOR_H
#define DEPURADOR_H

#define DEPURADOR_NOMBRE_MAX    64
#define DEPURADOR_RUTA_MAX      260
#define DEPURADOR_OBS_MAX       16
#define DEPURADOR_BREAKS_MAX    16
#define DEPURADOR_CONSOLA_MAX   32
#define DEPURADOR_CONSOLA_TEXTO_MAX 96
#define DEPURADOR_REPLAY_MAX    100000
#define DEPURADOR_ORDEN_MAX     256

/* Modos de paso. 0 = ejecucion libre, 1 = un bloque, 2 = instrucciones. */
#define DEPURADOR_PASO_LIBRE       0
#define DEPURADOR_PASO_BLOQUE      1
#define DEPURADOR_PASO_INSTRUCCION 2

/* Prepara el estado del depurador. No toca la memoria de la VM. */
void depurador_iniciar(void);

/* Libera la grabacion de replay si la hubiera. */
void depurador_terminar(void);

/* Activa o desactiva el depurador. */
void depurador_activar(int valor);
int depurador_esta_activo(void);

/* Pausa y reanuda la ejecucion. */
void depurador_pausar(void);
void depurador_reanudar(void);
int depurador_esta_pausado(void);

/* Configura un paso. `cantidad` solo se usa en modo instruccion. */
void depurador_paso(int modo, int cantidad);
int depurador_modo_paso(void);

/* Registra el bloque que va a ejecutar el planificador. Devuelve 1 si
 * ha saltado un breakpoint y la ejecucion debe quedar pausada. */
int depurador_antes_bloque(int bloque, int evento, int script);

/* Registra el bloque ya ejecutado. Cuenta los pasos por bloque y puede
 * dejar el depurador pausado. Devuelve 1 si ha pausado. */
int depurador_despues_bloque(int bloque, int evento, int script, int pc,
                             int error);

/* Observa una variable global por nombre. */
void depurador_observar(const char *nombre);
int depurador_num_observados(void);
const char *depurador_observado(int i);

/* Valor actual de un observado. `encontrado` se pone a 1 si se pudo
 * resolver. `memoria` es LoboMemoria* y `proyecto` MotorL3*. */
int depurador_observado_valor(int i, const void *memoria,
                              const void *proyecto, int *encontrado);

/* Breakpoints por id de bloque y por tipo de evento LOGIC3. */
void depurador_break_bloque(int bloque);
void depurador_break_evento(int evento);
int depurador_num_break_bloques(void);
int depurador_num_break_eventos(void);

/* Anade una linea a la consola (circulares: se conservan las ultimas). */
void depurador_consola(const char *texto);
int depurador_num_consola(void);
const char *depurador_consola_linea(int i);

/* Hook de instruccion. Lo llama la VM desde lobo.c. */
void depurador_instruccion(int pc, int error);

/* La VM consulta esto en sus bucles para cortar en un paso. */
int depurador_cortar_instrucciones(void);

/* Semilla del generador aleatorio de cada maquina. */
void depurador_semilla(int valor);
int depurador_semilla_valor(void);

/* Entrada del replay. `actual` se usa si no hay grabacion. */
unsigned depurador_entrada(int fotograma, unsigned actual);
void depurador_grabar_entrada(int fotograma, unsigned entrada);
int depurador_guardar_replay(const char *ruta);
int depurador_cargar_replay(const char *ruta, char *mensaje, int tam_mensaje,
                            int *semilla);
int depurador_tiene_replay(void);
void depurador_replay_mensaje(const char *texto);
void depurador_replay_grabar(int valor);

/* Escribe el estado de cada parada en `ruta`. Con `final` anade el cierre. */
void depurador_volcar(const char *ruta, const void *memoria,
                      const void *proyecto, int final);

/* Compara dos trazas y escribe la primera divergencia en `ruta`.
 * Devuelve 0 si no hay divergencias, 1 si las hay, 2 si no puede leer. */
int depurador_comparar(const char *ruta_a, const char *ruta_b,
                       const char *ruta_salida);

/* Bloque, evento, script, pc y error del ultimo punto de parada. */
int depurador_bloque_actual(void);
int depurador_evento_actual(void);
int depurador_script_actual(void);
int depurador_pc_actual(void);
int depurador_error_actual(void);
int depurador_fotograma_actual(void);
void depurador_fotograma(int valor);

#endif /* DEPURADOR_H */
