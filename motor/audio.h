/* audio.h - sonido y musica simple (fase 39).
 *
 * Mezclador de ocho canales a 22050 Hz, mono y 16 bits. El canal 7 queda
 * reservado para la musica en bucle; los efectos usan los canales 0..6.
 * Se admiten WAV PCM mono de 8 o 16 bits a 11025 o 22050 Hz con hasta
 * 256 KB de datos, el mismo limite que el editor de la fase 35.
 *
 * Las salidas nuevas estan detras de --audio, --audio-sonido, --volumen y
 * --volcar-audio. Sin esos flags no se abre ningun dispositivo y las
 * capturas y trazas de las fases anteriores siguen byte a byte identicas.
 */
#ifndef MOTOR_AUDIO_H
#define MOTOR_AUDIO_H

#include "motor.h"

#define AUDIO_CANALES        8
#define AUDIO_MUSICA_CANAL   7
#define AUDIO_SONIDOS_MAX    16
#define AUDIO_NOMBRE_MAX     48
#define AUDIO_VOLUMEN_MAX    100
#define AUDIO_FRECUENCIA     22050
#define AUDIO_MUESTRAS_FOTO  368   /* 22050 / 60, redondeado */
#define AUDIO_BUFERES        4
#define AUDIO_WAV_LIMITE     (256 * 1024)

void motor_audio_limpiar(void);
void motor_audio_activar(int valor);
int  motor_audio_esta_activo(void);
void motor_audio_base(const char *ruta_proyecto);
void motor_audio_dispositivo(int valor);

int  motor_audio_cargar(const char *nombre, const char *ruta);
int  motor_audio_sonar(const char *nombre);
int  motor_audio_musica(const char *nombre);
int  motor_audio_detener(void);
int  motor_audio_volumen(int valor);
int  motor_audio_volumen_actual(void);

/* Avanza la reproduccion un fotograma y escribe al dispositivo si hay uno. */
void motor_audio_fotograma(void);
void motor_audio_volcar(const char *ruta);

#endif /* MOTOR_AUDIO_H */
