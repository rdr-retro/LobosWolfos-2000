/* audio.c - sonido y musica simple (fase 39).
 *
 * Carga WAV PCM mono de 8 o 16 bits a 11025 o 22050 Hz, los convierte a
 * 22050 Hz / 16 bits y mezcla hasta ocho canales. El canal 7 se reserva para
 * la musica en bucle. winmm.dll se carga en tiempo de ejecucion, igual que
 * hace el editor en src/sonido.c, para no anadir -lwinmm al enlazado.
 *
 * Sin dispositivo (o en captura headless) el estado avanza igual: el juego
 * sigue y el volcado sigue siendo determinista.
 */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"

typedef struct AudioSonido {
    char nombre[AUDIO_NOMBRE_MAX];
    short *muestras;
    int numero;
    int hz, bits, bytes;
} AudioSonido;

typedef struct AudioCanal {
    int sonido;      /* indice en g_sonidos, -1 si libre */
    int pos;         /* muestra siguiente */
    int bucle;       /* 1 para la musica */
    int volumen;     /* por canal, 0..100 */
    int vueltas;     /* vueltas completadas de un sonido en bucle */
} AudioCanal;

static AudioSonido g_sonidos[AUDIO_SONIDOS_MAX];
static int g_num_sonidos;
static AudioCanal g_canales[AUDIO_CANALES];
static int g_activo;
static int g_dispositivo;
static int g_volumen = 80;
static char g_base[MAX_PATH];

/* winmm se carga solo si se pide salida real. */
typedef MMRESULT (WINAPI *FnAbrir)(HWAVEOUT *, UINT, LPCWAVEFORMATEX,
                                   DWORD_PTR, DWORD_PTR, DWORD);
typedef MMRESULT (WINAPI *FnPreparar)(HWAVEOUT, LPWAVEHDR, UINT);
typedef MMRESULT (WINAPI *FnEscribir)(HWAVEOUT, LPWAVEHDR, UINT);
typedef MMRESULT (WINAPI *FnDespreparar)(HWAVEOUT, LPWAVEHDR, UINT);
typedef MMRESULT (WINAPI *FnReiniciar)(HWAVEOUT);
typedef MMRESULT (WINAPI *FnCerrar)(HWAVEOUT);

static HMODULE g_winmm;
static FnAbrir fn_abrir;
static FnPreparar fn_preparar;
static FnEscribir fn_escribir;
static FnDespreparar fn_despreparar;
static FnReiniciar fn_reiniciar;
static FnCerrar fn_cerrar;
static HWAVEOUT g_salida;
static WAVEHDR g_cabeceras[AUDIO_BUFERES];
static short g_buffer[AUDIO_BUFERES][AUDIO_MUESTRAS_FOTO];
static int g_libres[AUDIO_BUFERES];
static int g_siguiente;
static int g_abierto;
/* Contadores del volcado: permiten comprobar sin dispositivo que sonar()
 * se lanzo una vez y que la musica ha dado vueltas. */
static int g_sonar_llamadas;
static int g_musica_llamadas;
static int g_vueltas_totales;

/* ------------------------------------------------------------------ */
/* Utilidades                                                         */
/* ------------------------------------------------------------------ */

static unsigned short leer_u16(const unsigned char *p) {
    return (unsigned short)((unsigned int)p[0] | ((unsigned int)p[1] << 8));
}

static unsigned int leer_u32(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static int existe(const char *ruta) {
    FILE *f = fopen(ruta, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int nombre_valido(const char *nombre) {
    int i;
    if (!nombre || !nombre[0]) return 0;
    for (i = 0; nombre[i]; i++) {
        char c = nombre[i];
        int bien = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!bien) return 0;
    }
    return 1;
}

static int buscar_sonido(const char *nombre) {
    int i;
    if (!nombre) return -1;
    for (i = 0; i < g_num_sonidos; i++)
        if (strcmp(g_sonidos[i].nombre, nombre) == 0) return i;
    return -1;
}

/* Busca el WAV junto al proyecto, en sonidos\ o en el directorio actual. */
static int ruta_por_nombre(const char *nombre, char *dst, int tam) {
    if (!nombre_valido(nombre) || tam < 8) return 0;
    if (g_base[0]) {
        _snprintf(dst, tam, "%s\\sonidos\\%s.wav", g_base, nombre);
        dst[tam - 1] = '\0';
        if (existe(dst)) return 1;
        _snprintf(dst, tam, "%s\\%s.wav", g_base, nombre);
        dst[tam - 1] = '\0';
        if (existe(dst)) return 1;
    }
    _snprintf(dst, tam, "sonidos\\%s.wav", nombre);
    dst[tam - 1] = '\0';
    if (existe(dst)) return 1;
    _snprintf(dst, tam, "%s.wav", nombre);
    dst[tam - 1] = '\0';
    return existe(dst);
}

/* ------------------------------------------------------------------ */
/* Carga de WAV                                                       */
/* ------------------------------------------------------------------ */

static int cargar_wav(const char *ruta, AudioSonido *s) {
    FILE *f;
    unsigned char *datos;
    long largo;
    unsigned int pos, offset = 0, tam = 0, partes = 0;
    int hz = 0, bits = 0, k, base;
    unsigned char *p;

    if (!ruta) return 0;
    f = fopen(ruta, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    largo = ftell(f);
    if (largo < 44 || largo > (long)AUDIO_WAV_LIMITE + 4096) {
        fclose(f);
        return 0;
    }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    datos = (unsigned char *)malloc((size_t)largo);
    if (!datos) { fclose(f); return 0; }
    if (fread(datos, 1, (size_t)largo, f) != (size_t)largo) {
        free(datos);
        fclose(f);
        return 0;
    }
    fclose(f);

    p = datos;
    if (memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0) {
        free(datos);
        return 0;
    }
    pos = 12;
    while (pos + 8 <= (unsigned int)largo) {
        unsigned int id = leer_u32(p + pos);
        unsigned int n = leer_u32(p + pos + 4);
        if (pos + 8 + n > (unsigned int)largo) break;
        if (id == 0x20746D66u /* fmt */ && n >= 16) {
            unsigned short formato = leer_u16(p + pos + 8);
            unsigned short canales = leer_u16(p + pos + 10);
            unsigned int frec = leer_u32(p + pos + 12);
            unsigned short bps = leer_u16(p + pos + 22);
            if (formato != 1 || canales != 1) break;
            if (frec != 11025u && frec != 22050u) break;
            if (bps != 8 && bps != 16) break;
            hz = (int)frec;
            bits = (int)bps;
            partes |= 1u;
        } else if (id == 0x61746164u /* data */) {
            offset = pos + 8;
            tam = n;
            partes |= 2u;
        }
        pos += 8 + n + (n & 1u);
    }
    if (partes != 3u || tam == 0 || tam > (unsigned int)AUDIO_WAV_LIMITE ||
        offset + tam > (unsigned int)largo) {
        free(datos);
        return 0;
    }

    base = (int)(tam / (unsigned int)(bits / 8));
    s->numero = hz == 11025 ? base * 2 : base;
    s->muestras = (short *)malloc((size_t)s->numero * sizeof(short));
    if (!s->muestras) {
        free(datos);
        s->numero = 0;
        return 0;
    }
    for (k = 0; k < base; k++) {
        int v;
        if (bits == 8) v = ((int)p[offset + k] - 128) << 8;
        else v = (int)leer_u16(p + offset + k * 2);
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        if (hz == 11025) {
            s->muestras[k * 2] = (short)v;
            s->muestras[k * 2 + 1] = (short)v;
        } else {
            s->muestras[k] = (short)v;
        }
    }
    s->hz = hz;
    s->bits = bits;
    s->bytes = (int)tam;
    free(datos);
    return 1;
}

/* ------------------------------------------------------------------ */
/* waveOut                                                            */
/* ------------------------------------------------------------------ */

static void cerrar_dispositivo(void) {
    int i;
    if (!g_abierto) return;
    if (g_salida && fn_reiniciar) fn_reiniciar(g_salida);
    for (i = 0; i < AUDIO_BUFERES; i++) {
        if (g_cabeceras[i].dwFlags & WHDR_PREPARED)
            fn_despreparar(g_salida, &g_cabeceras[i], sizeof(WAVEHDR));
    }
    if (fn_cerrar) fn_cerrar(g_salida);
    g_salida = NULL;
    g_abierto = 0;
    if (g_winmm) FreeLibrary(g_winmm);
    g_winmm = NULL;
    fn_abrir = NULL;
    fn_preparar = NULL;
    fn_escribir = NULL;
    fn_despreparar = NULL;
    fn_reiniciar = NULL;
    fn_cerrar = NULL;
}

static int abrir_dispositivo(void) {
    WAVEFORMATEX fmt;
    MMRESULT r;
    int i;
    if (g_abierto || !g_dispositivo) return 0;
    g_winmm = LoadLibraryA("winmm.dll");
    if (!g_winmm) return 0;
    fn_abrir = (FnAbrir)GetProcAddress(g_winmm, "waveOutOpen");
    fn_preparar = (FnPreparar)GetProcAddress(g_winmm, "waveOutPrepareHeader");
    fn_escribir = (FnEscribir)GetProcAddress(g_winmm, "waveOutWrite");
    fn_despreparar = (FnDespreparar)GetProcAddress(g_winmm,
                                                   "waveOutUnprepareHeader");
    fn_reiniciar = (FnReiniciar)GetProcAddress(g_winmm, "waveOutReset");
    fn_cerrar = (FnCerrar)GetProcAddress(g_winmm, "waveOutClose");
    if (!fn_abrir || !fn_preparar || !fn_escribir || !fn_despreparar ||
        !fn_reiniciar || !fn_cerrar) {
        cerrar_dispositivo();
        return 0;
    }
    memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = AUDIO_FRECUENCIA;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = AUDIO_FRECUENCIA * 2;
    r = fn_abrir(&g_salida, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL);
    if (r != MMSYSERR_NOERROR || !g_salida) {
        cerrar_dispositivo();
        return 0;
    }
    for (i = 0; i < AUDIO_BUFERES; i++) {
        memset(&g_cabeceras[i], 0, sizeof(WAVEHDR));
        g_cabeceras[i].lpData = (LPSTR)g_buffer[i];
        g_cabeceras[i].dwBufferLength = AUDIO_MUESTRAS_FOTO * 2;
        if (fn_preparar(g_salida, &g_cabeceras[i], sizeof(WAVEHDR)) !=
            MMSYSERR_NOERROR) {
            cerrar_dispositivo();
            return 0;
        }
        g_libres[i] = 1;
    }
    g_siguiente = 0;
    g_abierto = 1;
    return 1;
}

/* ------------------------------------------------------------------ */
/* API publica                                                        */
/* ------------------------------------------------------------------ */

void motor_audio_limpiar(void) {
    int i;
    cerrar_dispositivo();
    for (i = 0; i < g_num_sonidos; i++) {
        free(g_sonidos[i].muestras);
        memset(&g_sonidos[i], 0, sizeof(g_sonidos[i]));
    }
    g_num_sonidos = 0;
    memset(g_canales, 0, sizeof(g_canales));
    for (i = 0; i < AUDIO_CANALES; i++) g_canales[i].sonido = -1;
    g_activo = 0;
    g_dispositivo = 0;
    g_volumen = 80;
    g_base[0] = '\0';
    g_siguiente = 0;
    for (i = 0; i < AUDIO_BUFERES; i++) g_libres[i] = 0;
    g_sonar_llamadas = 0;
    g_musica_llamadas = 0;
    g_vueltas_totales = 0;
}

void motor_audio_activar(int valor) {
    g_activo = valor ? 1 : 0;
    if (!g_activo) cerrar_dispositivo();
}

int motor_audio_esta_activo(void) { return g_activo; }

void motor_audio_base(const char *ruta_proyecto) {
    int i, corte = -1;
    g_base[0] = '\0';
    if (!ruta_proyecto) return;
    for (i = 0; ruta_proyecto[i]; i++)
        if (ruta_proyecto[i] == '\\' || ruta_proyecto[i] == '/') corte = i;
    if (corte < 0) return;
    if (corte >= MAX_PATH) corte = MAX_PATH - 1;
    memcpy(g_base, ruta_proyecto, (size_t)corte);
    g_base[corte] = '\0';
}

void motor_audio_dispositivo(int valor) {
    g_dispositivo = valor ? 1 : 0;
    if (!g_dispositivo) cerrar_dispositivo();
}

int motor_audio_cargar(const char *nombre, const char *ruta) {
    char resuelto[MAX_PATH];
    AudioSonido sonido;
    int idx;
    if (!nombre_valido(nombre)) return -1;
    idx = buscar_sonido(nombre);
    if (idx >= 0) return idx;
    if (g_num_sonidos >= AUDIO_SONIDOS_MAX) return -1;
    if (!ruta || !ruta[0]) {
        if (!ruta_por_nombre(nombre, resuelto, sizeof(resuelto))) return -1;
        ruta = resuelto;
    }
    memset(&sonido, 0, sizeof(sonido));
    if (!cargar_wav(ruta, &sonido)) return -1;
    strcpy(sonido.nombre, nombre);
    g_sonidos[g_num_sonidos] = sonido;
    return g_num_sonidos++;
}

int motor_audio_sonar(const char *nombre) {
    int idx, ch;
    idx = motor_audio_cargar(nombre, NULL);
    if (idx < 0) return 0;
    for (ch = 0; ch < AUDIO_MUSICA_CANAL; ch++)
        if (g_canales[ch].sonido < 0) break;
    if (ch >= AUDIO_MUSICA_CANAL) return 0;
    g_canales[ch].sonido = idx;
    g_canales[ch].pos = 0;
    g_canales[ch].bucle = 0;
    g_canales[ch].volumen = 100;
    g_canales[ch].vueltas = 0;
    g_sonar_llamadas++;
    return 1;
}

int motor_audio_musica(const char *nombre) {
    int idx;
    if (!nombre || !nombre[0]) {
        memset(&g_canales[AUDIO_MUSICA_CANAL], 0,
               sizeof(g_canales[AUDIO_MUSICA_CANAL]));
        g_canales[AUDIO_MUSICA_CANAL].sonido = -1;
        return 0;
    }
    idx = motor_audio_cargar(nombre, NULL);
    if (idx < 0) return 0;
    g_canales[AUDIO_MUSICA_CANAL].sonido = idx;
    g_canales[AUDIO_MUSICA_CANAL].pos = 0;
    g_canales[AUDIO_MUSICA_CANAL].bucle = 1;
    g_canales[AUDIO_MUSICA_CANAL].volumen = 100;
    g_canales[AUDIO_MUSICA_CANAL].vueltas = 0;
    g_musica_llamadas++;
    return 1;
}

int motor_audio_detener(void) {
    int i;
    for (i = 0; i < AUDIO_CANALES; i++) {
        memset(&g_canales[i], 0, sizeof(g_canales[i]));
        g_canales[i].sonido = -1;
    }
    return 0;
}

int motor_audio_volumen(int valor) {
    int anterior = g_volumen;
    if (valor < 0) valor = 0;
    if (valor > AUDIO_VOLUMEN_MAX) valor = AUDIO_VOLUMEN_MAX;
    g_volumen = valor;
    return anterior;
}

int motor_audio_volumen_actual(void) { return g_volumen; }

void motor_audio_fotograma(void) {
    short mezcla[AUDIO_MUESTRAS_FOTO];
    int i, ch, buf = -1;

    if (!g_activo) return;
    if (g_dispositivo && !g_abierto) abrir_dispositivo();

    for (i = 0; i < AUDIO_MUESTRAS_FOTO; i++) {
        int total = 0;
        for (ch = 0; ch < AUDIO_CANALES; ch++) {
            AudioCanal *c = &g_canales[ch];
            AudioSonido *s;
            if (c->sonido < 0) continue;
            s = &g_sonidos[c->sonido];
            if (c->pos >= s->numero) {
                if (c->bucle) {
                    c->pos = 0;
                    c->vueltas++;
                    g_vueltas_totales++;
                } else {
                    c->sonido = -1;
                    continue;
                }
            }
            total += (s->muestras[c->pos] * c->volumen * g_volumen) / 10000;
            c->pos++;
        }
        if (total > 32767) total = 32767;
        if (total < -32768) total = -32768;
        mezcla[i] = (short)total;
    }

    if (!g_abierto) return;
    for (i = 0; i < AUDIO_BUFERES; i++) {
        int k = (g_siguiente + i) % AUDIO_BUFERES;
        if (!g_libres[k] && (g_cabeceras[k].dwFlags & WHDR_DONE))
            g_libres[k] = 1;
        if (g_libres[k]) { buf = k; break; }
    }
    if (buf < 0) return; /* todos ocupados: este fotograma no suena */
    memcpy(g_buffer[buf], mezcla, sizeof(mezcla));
    g_cabeceras[buf].dwBufferLength = AUDIO_MUESTRAS_FOTO * 2;
    g_cabeceras[buf].dwFlags = 0;
    if (fn_escribir(g_salida, &g_cabeceras[buf], sizeof(WAVEHDR)) ==
        MMSYSERR_NOERROR) {
        g_libres[buf] = 0;
        g_siguiente = (buf + 1) % AUDIO_BUFERES;
    }
}

void motor_audio_volcar(const char *ruta) {
    FILE *f;
    int i;
    if (!ruta) return;
    f = fopen(ruta, "wt");
    if (!f) return;
    fprintf(f, "audio: activo %d\n", g_activo);
    fprintf(f, "audio: dispositivo %d\n", g_abierto ? 1 : 0);
    fprintf(f, "audio: volumen %d\n", g_volumen);
    fprintf(f, "audio: base %s\n", g_base[0] ? g_base : "-");
    fprintf(f, "audio: sonar %d\n", g_sonar_llamadas);
    fprintf(f, "audio: musica %d\n", g_musica_llamadas);
    fprintf(f, "audio: vueltas %d\n", g_vueltas_totales);
    fprintf(f, "audio: sonidos %d\n", g_num_sonidos);
    for (i = 0; i < g_num_sonidos; i++) {
        AudioSonido *s = &g_sonidos[i];
        fprintf(f, "audio: sonido %d %s %d %d %d\n", i, s->nombre,
                s->numero, s->hz, s->bytes);
    }
    fprintf(f, "audio: canales %d\n", AUDIO_CANALES);
    for (i = 0; i < AUDIO_CANALES; i++) {
        AudioCanal *c = &g_canales[i];
        if (c->sonido < 0) {
            fprintf(f, "audio: canal %d libre\n", i);
        } else {
            fprintf(f, "audio: canal %d %s %d %d %d %d\n", i,
                    g_sonidos[c->sonido].nombre, c->pos, c->volumen,
                    c->bucle, c->vueltas);
        }
    }
    fclose(f);
}
