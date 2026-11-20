/* save.h - guardado, records y persistencia (fase 40).
 *
 * Un proyecto guarda su progreso en `saves/<proyecto>.sav`: texto de una
 * linea por clave con la forma `clave valor`. Los records usan la clave
 * `record.<nombre>` y los ajustes del usuario `ajuste.<nombre>`, asi todo
 * el estado cabe en un solo archivo y se escribe de una vez.
 *
 * La escritura es atomica: se escribe `<proyecto>.sav.tmp` y se reemplaza
 * el definitivo con MoveFileEx. Si el definitivo esta corrupto se prueba
 * el temporal; si los dos fallan el juego arranca con la tabla vacia.
 *
 * Las salidas nuevas estan detras de --saves y --volcar-saves. Sin esos
 * flags no se abre ningun archivo y las capturas, trazas y volcados de las
 * fases anteriores siguen byte a byte identicos.
 */
#ifndef MOTOR_SAVE_H
#define MOTOR_SAVE_H

#define SAVE_CLAVES_MAX  256
#define SAVE_CLAVE_MAX   48
#define SAVE_VALOR_MAX   128
#define SAVE_LIMITE      (64 * 1024)

void motor_save_limpiar(void);
void motor_save_activar(int valor);
int  motor_save_esta_activo(void);
void motor_save_directorio(const char *ruta);
void motor_save_base(const char *ruta_proyecto);

int  motor_save_guardar(const char *clave, int valor);
int  motor_save_cargar(const char *clave);
int  motor_save_borrar(void);
int  motor_save_record(const char *clave, int valor);
int  motor_save_record_actual(const char *clave);
int  motor_save_ajuste(const char *nombre, int valor);
int  motor_save_ajuste_actual(const char *nombre);

/* Ruta del archivo del proyecto (para el volcado y las pruebas). */
void motor_save_ruta(char *dst, int tam);
void motor_save_volcar(const char *ruta);

#endif /* MOTOR_SAVE_H */
