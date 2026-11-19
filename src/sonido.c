/* ==========================================================================
 * Validacion y reproduccion de WAV (fase 35).
 *
 * Solo se admiten WAV PCM mono de 8 o 16 bits a 11025 o 22050 Hz y con
 * menos de 256 KB de datos. La reproduccion usa PlaySoundA de winmm.dll
 * cargada en tiempo de ejecucion: asi no hace falta anadir -lwinmm al
 * enlazado y el editor sigue funcionando en Windows 2000/XP de 32 bits.
 * ========================================================================== */
#include "app.h"

#define WAV_LIMITE_DATOS (256 * 1024)

static unsigned short leer_u16(const unsigned char *p) {
    return (unsigned short)((unsigned int)p[0] | ((unsigned int)p[1] << 8));
}

static unsigned int leer_u32(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

int sonido_validar_wav(const char *ruta, int *frecuencia, int *canales,
                       int *bits, int *bytes) {
    char *datos = NULL;
    int largo = 0;
    const unsigned char *p;
    unsigned int pos, tam = 0, fmt = 0, datos_wav = 0;
    int ok = 0;

    if (frecuencia) *frecuencia = 0;
    if (canales) *canales = 0;
    if (bits) *bits = 0;
    if (bytes) *bytes = 0;
    if (!ruta || !leer_archivo(ruta, &datos, &largo) || largo < 44) {
        free(datos);
        return 0;
    }
    p = (const unsigned char *)datos;
    if (memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0)
        goto fin;

    pos = 12;
    while (pos + 8 <= (unsigned int)largo) {
        unsigned int id = leer_u32(p + pos);
        unsigned int n = leer_u32(p + pos + 4);
        if (pos + 8 + n > (unsigned int)largo) break;
        if (id == 0x20746D66u /* fmt */ && n >= 16) {
            unsigned short formato = leer_u16(p + pos + 8);
            unsigned short ch = leer_u16(p + pos + 10);
            unsigned int hz = leer_u32(p + pos + 12);
            unsigned short bps = leer_u16(p + pos + 22);
            if (formato != 1 || ch != 1) break;
            if (hz != 11025u && hz != 22050u) break;
            if (bps != 8 && bps != 16) break;
            fmt = 1;
            if (frecuencia) *frecuencia = (int)hz;
            if (canales) *canales = (int)ch;
            if (bits) *bits = (int)bps;
        } else if (id == 0x61746164u /* data */) {
            datos_wav = 1;
            tam = n;
            if (bytes) *bytes = (int)n;
        }
        pos += 8 + n + (n & 1u);
    }
    ok = fmt && datos_wav && tam > 0 && tam <= WAV_LIMITE_DATOS;

fin:
    free(datos);
    return ok;
}

int sonido_reproducir(const char *ruta) {
    HMODULE winmm;
    typedef BOOL (WINAPI *FnPlaySound)(LPCSTR, HMODULE, DWORD);
    FnPlaySound fn;
    const DWORD SND_FILENAME_FLAG = 0x00020000u;
    const DWORD SND_NODEFAULT_FLAG = 0x00000002u;
    const DWORD SND_ASYNC_FLAG = 0x00000001u;
    BOOL resultado;
    if (!ruta || !ruta[0]) return 0;
    winmm = LoadLibraryA("winmm.dll");
    if (!winmm) return 0;
    fn = (FnPlaySound)GetProcAddress(winmm, "PlaySoundA");
    if (!fn) {
        FreeLibrary(winmm);
        return 0;
    }
    resultado = fn(ruta, NULL,
                   SND_FILENAME_FLAG | SND_NODEFAULT_FLAG | SND_ASYNC_FLAG);
    FreeLibrary(winmm);
    return resultado ? 1 : 0;
}
