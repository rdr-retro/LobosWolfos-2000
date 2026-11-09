/* ==========================================================================
 * Imagen: carga y guardado de PNG/BMP.
 *
 * PNG usa GDI+ (incluido en Windows XP). Si GDI+ no estuviera disponible,
 * BMP sigue funcionando con codigo propio.
 * ========================================================================== */
#include "app.h"
#include <objbase.h>

/* --- Declaraciones minimas de la API plana de GDI+ (C) --------------- */
typedef struct GdiplusStartupInput {
    UINT32 GdiplusVersion;
    void *DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} GdiplusStartupInput;

typedef struct GpRect {
    INT X;
    INT Y;
    INT Width;
    INT Height;
} GpRect;

typedef struct GpBitmapData {
    UINT Width;
    UINT Height;
    INT Stride;
    INT PixelFormat;
    void *Scan0;
    UINT_PTR Reserved;
} GpBitmapData;

__declspec(dllimport) int WINAPI GdiplusStartup(ULONG_PTR *token, const GdiplusStartupInput *input, void *output);
__declspec(dllimport) void WINAPI GdiplusShutdown(ULONG_PTR token);
__declspec(dllimport) int WINAPI GdipCreateBitmapFromFile(const WCHAR *filename, void **bitmap);
__declspec(dllimport) int WINAPI GdipCreateBitmapFromStream(IStream *stream, void **bitmap);
__declspec(dllimport) int WINAPI GdipCreateBitmapFromScan0(INT width, INT height, INT stride, INT format,
                                                          BYTE *scan0, void **bitmap);
__declspec(dllimport) int WINAPI GdipGetImageWidth(void *image, UINT *width);
__declspec(dllimport) int WINAPI GdipGetImageHeight(void *image, UINT *height);
__declspec(dllimport) int WINAPI GdipBitmapLockBits(void *bitmap, const GpRect *rect, UINT flags, INT format,
                                                   GpBitmapData *data);
__declspec(dllimport) int WINAPI GdipBitmapUnlockBits(void *bitmap, GpBitmapData *data);
__declspec(dllimport) int WINAPI GdipDisposeImage(void *image);
__declspec(dllimport) int WINAPI GdipSaveImageToFile(void *image, const WCHAR *filename,
                                                     const CLSID *clsidEncoder, const void *encoderParams);

#define PIXEL_FORMAT_32BPP_ARGB 0x0026200A
#define LOCK_READ  1
#define LOCK_WRITE 2

static const CLSID CLSID_PNG =
    {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};

static ULONG_PTR gdiplus_token = 0;
static int gdiplus_ok = 0;

static void a_wide(const char *texto, WCHAR *dst, int maximo) {
    MultiByteToWideChar(CP_ACP, 0, texto, -1, dst, maximo);
}

int imagen_iniciar(void) {
    GdiplusStartupInput entrada;
    memset(&entrada, 0, sizeof(entrada));
    entrada.GdiplusVersion = 1;
    if (GdiplusStartup(&gdiplus_token, &entrada, NULL) == 0) {
        gdiplus_ok = 1;
        return 1;
    }
    gdiplus_ok = 0;
    return 0;
}

void imagen_terminar(void) {
    if (gdiplus_ok) {
        GdiplusShutdown(gdiplus_token);
        gdiplus_ok = 0;
    }
    gdiplus_token = 0;
}

static u32 *copiar_desde_bitmap(void *bmp, int *ancho, int *alto) {
    UINT w = 0, h = 0;
    GpRect rect;
    GpBitmapData datos;
    u32 *salida;
    int y;

    if (!bmp) return NULL;
    if (GdipGetImageWidth(bmp, &w) != 0 || GdipGetImageHeight(bmp, &h) != 0) return NULL;
    if (w == 0 || h == 0 || w > 8192 || h > 8192) return NULL;
    rect.X = 0; rect.Y = 0; rect.Width = (INT)w; rect.Height = (INT)h;
    memset(&datos, 0, sizeof(datos));
    if (GdipBitmapLockBits(bmp, &rect, LOCK_READ, PIXEL_FORMAT_32BPP_ARGB, &datos) != 0) return NULL;
    salida = (u32 *)malloc((size_t)w * h * sizeof(u32));
    if (!salida) {
        GdipBitmapUnlockBits(bmp, &datos);
        return NULL;
    }
    for (y = 0; y < (int)h; y++) {
        const u32 *fila = (const u32 *)((const BYTE *)datos.Scan0 + (size_t)y * datos.Stride);
        memcpy(salida + (size_t)y * w, fila, (size_t)w * sizeof(u32));
        /* GDI+ entrega BGRA = 0xAARRGGBB, igual que nuestro buffer. */
    }
    GdipBitmapUnlockBits(bmp, &datos);
    *ancho = (int)w;
    *alto = (int)h;
    return salida;
}

static u32 *cargar_bmp_propio(const char *ruta, int *ancho, int *alto) {
    FILE *f = fopen(ruta, "rb");
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    int w, h, bpp, comp;
    u32 *pix;
    int x, y, fila_bytes;
    u8 *fila;
    if (!f) return NULL;
    if (fread(&fh, sizeof(fh), 1, f) != 1 || fread(&ih, sizeof(ih), 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    if (fh.bfType != 0x4D42) { fclose(f); return NULL; }
    w = ih.biWidth;
    h = ih.biHeight;
    bpp = ih.biBitCount;
    comp = ih.biCompression;
    if (w <= 0 || w > 8192 || h == 0 || h < -8192 || h > 8192 || (bpp != 24 && bpp != 32) || comp != 0) {
        fclose(f);
        return NULL;
    }
    {
        int alto_abs = h < 0 ? -h : h;
        fila_bytes = ((w * bpp / 8) + 3) & ~3;
        fila = (u8 *)malloc((size_t)fila_bytes);
        pix = (u32 *)malloc((size_t)w * alto_abs * sizeof(u32));
        if (!fila || !pix) { free(fila); free(pix); fclose(f); return NULL; }
        fseek(f, fh.bfOffBits, SEEK_SET);
        for (y = 0; y < alto_abs; y++) {
            int dy = h < 0 ? y : alto_abs - 1 - y;
            if (fread(fila, 1, (size_t)fila_bytes, f) != (size_t)fila_bytes) break;
            for (x = 0; x < w; x++) {
                u8 b = fila[x * (bpp / 8) + 0];
                u8 gg = fila[x * (bpp / 8) + 1];
                u8 r = fila[x * (bpp / 8) + 2];
                u8 a = bpp == 32 ? fila[x * 4 + 3] : 255;
                if (a == 0 && bpp == 32) a = 255;
                pix[(size_t)dy * w + x] = ((u32)a << 24) | ((u32)r << 16) | ((u32)gg << 8) | b;
            }
        }
        free(fila);
        fclose(f);
        *ancho = w;
        *alto = alto_abs;
        return pix;
    }
}

u32 *imagen_cargar(const char *ruta, int *ancho, int *alto) {
    WCHAR wruta[MAX_PATH * 2];
    void *bmp = NULL;
    u32 *pix;
    if (!ruta) return NULL;
    if (termina_con(ruta, ".bmp")) {
        pix = cargar_bmp_propio(ruta, ancho, alto);
        if (pix) return pix;
    }
    if (!gdiplus_ok) return NULL;
    a_wide(ruta, wruta, MAX_PATH * 2);
    if (GdipCreateBitmapFromFile(wruta, &bmp) != 0 || !bmp) return NULL;
    pix = copiar_desde_bitmap(bmp, ancho, alto);
    GdipDisposeImage(bmp);
    return pix;
}

u32 *imagen_desde_memoria(const void *datos, int largo, int *ancho, int *alto) {
    HGLOBAL memoria;
    IStream *stream = NULL;
    void *bmp = NULL;
    u32 *pix = NULL;
    if (!gdiplus_ok || !datos || largo <= 0) return NULL;
    memoria = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)largo);
    if (!memoria) return NULL;
    {
        void *destino = GlobalLock(memoria);
        if (!destino) { GlobalFree(memoria); return NULL; }
        memcpy(destino, datos, (size_t)largo);
        GlobalUnlock(memoria);
    }
    if (CreateStreamOnHGlobal(memoria, TRUE, &stream) != S_OK || !stream) {
        GlobalFree(memoria);
        return NULL;
    }
    {
        LARGE_INTEGER cero;
        cero.QuadPart = 0;
        stream->lpVtbl->Seek(stream, cero, STREAM_SEEK_SET, NULL);
    }
    if (GdipCreateBitmapFromStream(stream, &bmp) == 0 && bmp) {
        pix = copiar_desde_bitmap(bmp, ancho, alto);
        GdipDisposeImage(bmp);
    }
    stream->lpVtbl->Release(stream);
    return pix;
}

int imagen_guardar_png(const char *ruta, const u32 *pixeles, int ancho, int alto) {
    WCHAR wruta[MAX_PATH * 2];
    void *bmp = NULL;
    int ok;
    if (!gdiplus_ok || !ruta || !pixeles || ancho < 1 || alto < 1) return 0;
    if (GdipCreateBitmapFromScan0(ancho, alto, ancho * 4, PIXEL_FORMAT_32BPP_ARGB,
                                  (BYTE *)pixeles, &bmp) != 0 || !bmp)
        return 0;
    a_wide(ruta, wruta, MAX_PATH * 2);
    ok = GdipSaveImageToFile(bmp, wruta, &CLSID_PNG, NULL) == 0;
    GdipDisposeImage(bmp);
    return ok;
}

int imagen_guardar_bmp(const char *ruta, const u32 *pixeles, int ancho, int alto) {
    FILE *f;
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    int fila_bytes, x, y;
    u8 *fila;
    if (!ruta || !pixeles || ancho < 1 || alto < 1) return 0;
    fila_bytes = ((ancho * 3) + 3) & ~3;
    memset(&fh, 0, sizeof(fh));
    memset(&ih, 0, sizeof(ih));
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + (DWORD)fila_bytes * (DWORD)alto;
    ih.biSize = sizeof(ih);
    ih.biWidth = ancho;
    ih.biHeight = alto;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = (DWORD)fila_bytes * (DWORD)alto;
    f = fopen(ruta, "wb");
    if (!f) return 0;
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    fila = (u8 *)calloc((size_t)fila_bytes, 1);
    if (!fila) { fclose(f); return 0; }
    for (y = alto - 1; y >= 0; y--) {
        for (x = 0; x < ancho; x++) {
            u32 c = pixeles[(size_t)y * ancho + x];
            fila[x * 3 + 0] = (u8)(c & 0xFF);
            fila[x * 3 + 1] = (u8)((c >> 8) & 0xFF);
            fila[x * 3 + 2] = (u8)((c >> 16) & 0xFF);
        }
        fwrite(fila, 1, (size_t)fila_bytes, f);
    }
    free(fila);
    fclose(f);
    return 1;
}

int imagen_guardar(const char *ruta, const u32 *pixeles, int ancho, int alto) {
    if (termina_con(ruta, ".bmp")) return imagen_guardar_bmp(ruta, pixeles, ancho, alto);
    return imagen_guardar_png(ruta, pixeles, ancho, alto);
}

u32 *imagen_escalar(const u32 *origen, int ow, int oh, int dw, int dh) {
    u32 *dst;
    int x, y;
    if (!origen || ow < 1 || oh < 1 || dw < 1 || dh < 1) return NULL;
    dst = (u32 *)malloc((size_t)dw * dh * sizeof(u32));
    if (!dst) return NULL;
    for (y = 0; y < dh; y++) {
        int sy = y * oh / dh;
        for (x = 0; x < dw; x++) {
            int sx = x * ow / dw;
            dst[(size_t)y * dw + x] = origen[(size_t)sy * ow + sx];
        }
    }
    return dst;
}
