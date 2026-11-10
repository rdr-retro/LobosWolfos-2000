/* juego.c - game.exe, el juego montado sobre el motor egine (fase 3).
 *
 * Abre una ventana, carga el proyecto (un .logic del gestor de logica o
 * directamente un .mav con sus tiles PNG), dibuja el mapa con el nucleo
 * en ensamblador (motor_mapa_render) y mueve un sprite con el teclado
 * colisionando contra las celdas del mapa (motor_aabb).
 *
 * Flags de prueba (mismo estilo que el editor):
 *   --logica ruta     proyecto .logic del gestor (busca el mapa en maps/)
 *   --mapa ruta       mapa .mav directo (gana al proyecto)
 *   --captura ruta    corre N fotogramas sin ventana y guarda BMP; sale
 *   --fotogramas N    fotogramas de la captura (60 por defecto)
 *   --tecla nombre    tecla mantenida durante la captura
 *                     (izquierda/derecha/arriba/abajo/saltar)
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "motor.h"

#define VENTANA_ANCHO 640
#define VENTANA_ALTO  480
#define FPS_OBJETIVO  60
#define MS_POR_FOTO   (1000 / FPS_OBJETIVO)
#define SPRITE_LADO   24
#define SPRITE_VEL    3

typedef struct Juego {
    MotorMapa mapa;
    char mapa_usado[MAX_PATH];
    u32 *fb;                 /* framebuffer: DIB en ventana, malloc en captura */
    int fb_ancho, fb_alto;
    u32 sprite[SPRITE_LADO * SPRITE_LADO]; /* sprite generado, 24x24 */
    int x, y;                /* posicion del sprite en el mundo */
    unsigned entrada;        /* bits MOTOR_ENT_* */
    int corriendo;
} Juego;

static Juego juego;

/* ------------------------------------------------------------------ */
/* Sprite del jugador: un lobito azul de 24x24 dibujado a mano         */
/* ------------------------------------------------------------------ */

static void sprite_crear(void) {
    int px, py;
    for (py = 0; py < SPRITE_LADO; py++) {
        for (px = 0; px < SPRITE_LADO; px++) {
            u32 c = 0xFF2456A6u;                    /* cuerpo */
            int borde = px == 0 || px == SPRITE_LADO - 1 ||
                        py == 0 || py == SPRITE_LADO - 1;
            int esquina = (px < 2 || px > SPRITE_LADO - 3) &&
                         (py < 2 || py > SPRITE_LADO - 3);
            int oreja = (px >= 1 && px <= 6 && py <= 7) ||
                        (px >= SPRITE_LADO - 7 && px <= SPRITE_LADO - 2 && py <= 7);
            int ojo = (py >= 8 && py <= 10) &&
                      ((px >= 5 && px <= 7) || (px >= 16 && px <= 18));
            int hocico = (py >= 14 && py <= 19) &&
                        (px >= 9 && px <= 14);
            if (esquina) { c = 0; }
            else if (oreja) c = 0xFF16325Eu;         /* orejas oscuras */
            else if (borde) c = 0xFF16325Eu;
            else if (ojo) c = 0xFFFFFFFFu;            /* ojos */
            else if (hocico) c = 0xFFD8E6F8u;         /* hocico claro */
            juego.sprite[py * SPRITE_LADO + px] = c;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Colision del sprite contra las celdas solidas del mapa             */
/* ------------------------------------------------------------------ */

static int choca_con_mapa_wh(int x, int y, int w, int h) {
    const MotorMapa *m = &juego.mapa;
    int i;
    for (i = 0; i < m->num_celdas; i++) {
        const MotorCelda *c = &m->celdas[i];
        if (motor_aabb(x, y, w, h,
                       c->x * m->lado, c->y * m->lado, m->lado, m->lado))
            return 1;
    }
    return 0;
}

static int choca_con_mapa(int x, int y) {
    return choca_con_mapa_wh(x, y, SPRITE_LADO, SPRITE_LADO);
}

/* Busca un punto libre para aparecer: en espiral desde la posicion
 * inicial hasta encontrar un hueco con margen (el sprite mas 8 pixeles
 * de holgura en cada direccion), para no nacer pegado a un muro. */
static void buscar_spawn(void) {
    const int margen = 8;
    int r, x, y;
    if (!choca_con_mapa_wh(juego.x - margen, juego.y - margen,
                           SPRITE_LADO + 2 * margen,
                           SPRITE_LADO + 2 * margen))
        return;
    for (r = 16; r <= 4096; r += 16) {
        for (y = -r; y <= r; y += 8) {
            int paso = (y == -r || y == r) ? 8 : 2 * r;
            for (x = -r; x <= r; x += paso) {
                if (!choca_con_mapa_wh(x - margen, y - margen,
                                       SPRITE_LADO + 2 * margen,
                                       SPRITE_LADO + 2 * margen)) {
                    juego.x = x;
                    juego.y = y;
                    return;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Actualizar y renderizar un fotograma                                */
/* ------------------------------------------------------------------ */

static void actualizar(void) {
    int vx = 0, vy = 0;
    unsigned e = juego.entrada;

    if (e & MOTOR_ENT_IZQUIERDA) vx -= SPRITE_VEL;
    if (e & MOTOR_ENT_DERECHA)   vx += SPRITE_VEL;
    if (e & MOTOR_ENT_ARRIBA)    vy -= SPRITE_VEL;
    if (e & MOTOR_ENT_ABAJO)     vy += SPRITE_VEL;
    if (e & MOTOR_ENT_CERRAR) juego.corriendo = 0;

    /* movimiento separado por eje para deslizar contra los tiles */
    if (vx) {
        juego.x += vx;
        if (choca_con_mapa(juego.x, juego.y)) juego.x -= vx;
    }
    if (vy) {
        juego.y += vy;
        if (choca_con_mapa(juego.x, juego.y)) juego.y -= vy;
    }
}

static void renderizar(void) {
    MotorMapa *m = &juego.mapa;
    int cam_x, cam_y;

    /* camara centrada en el sprite */
    cam_x = juego.x + SPRITE_LADO / 2 - juego.fb_ancho / 2;
    cam_y = juego.y + SPRITE_LADO / 2 - juego.fb_alto / 2;

    motor_limpiar(juego.fb, juego.fb_ancho, juego.fb_alto, 0x00101826u);
    motor_mapa_render(juego.fb, juego.fb_ancho, juego.fb_alto,
                      cam_x, cam_y, m->lado, m->celdas, m->num_celdas,
                      m->imagenes);
    motor_blit_alpha(juego.fb, juego.fb_ancho, juego.fb_alto,
                     juego.x - cam_x, juego.y - cam_y,
                     juego.sprite, SPRITE_LADO, SPRITE_LADO);
}

/* ------------------------------------------------------------------ */
/* Modo ventana                                                        */
/* ------------------------------------------------------------------ */

static HDC memdc;
static HBITMAP dib;
static void *dib_pixeles;
static HBITMAP dib_anterior;

static LRESULT CALLBACK proc_ventana(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static unsigned leer_teclado(void) {
    unsigned e = 0;
    if (GetAsyncKeyState(VK_LEFT) & 0x8000)  e |= MOTOR_ENT_IZQUIERDA;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) e |= MOTOR_ENT_DERECHA;
    if (GetAsyncKeyState(VK_UP) & 0x8000)    e |= MOTOR_ENT_ARRIBA;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)  e |= MOTOR_ENT_ABAJO;
    if (GetAsyncKeyState('W') & 0x8000) e |= MOTOR_ENT_ARRIBA;
    if (GetAsyncKeyState('S') & 0x8000) e |= MOTOR_ENT_ABAJO;
    if (GetAsyncKeyState('A') & 0x8000) e |= MOTOR_ENT_IZQUIERDA;
    if (GetAsyncKeyState('D') & 0x8000) e |= MOTOR_ENT_DERECHA;
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) e |= MOTOR_ENT_SALTO;
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) e |= MOTOR_ENT_CERRAR;
    return e;
}

static int correr_ventana(HINSTANCE instancia) {
    WNDCLASSA clase;
    HWND hwnd;
    MSG msg;
    BITMAPINFO bmi;
    HDC pantalla;
    RECT rect;
    char titulo[MAX_PATH + 64];
    DWORD t0;

    memset(&clase, 0, sizeof(clase));
    clase.lpfnWndProc = proc_ventana;
    clase.hInstance = instancia;
    clase.lpszClassName = "EGINE_JUEGO";
    clase.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassA(&clase)) return 1;

    rect.left = 0; rect.top = 0;
    rect.right = VENTANA_ANCHO; rect.bottom = VENTANA_ALTO;
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    _snprintf(titulo, sizeof(titulo), "egine - LobosWolfos 2000 - %s",
              juego.mapa_usado);
    titulo[sizeof(titulo) - 1] = '\0';
    hwnd = CreateWindowA("EGINE_JUEGO", titulo, WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         rect.right - rect.left, rect.bottom - rect.top,
                         NULL, NULL, instancia, NULL);
    if (!hwnd) return 1;

    /* framebuffer: DIB de 32 bits top-down, igual que el editor */
    pantalla = GetDC(NULL);
    memdc = CreateCompatibleDC(pantalla);
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = VENTANA_ANCHO;
    bmi.bmiHeader.biHeight = -VENTANA_ALTO;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    dib = CreateDIBSection(pantalla, &bmi, DIB_RGB_COLORS, &dib_pixeles,
                           NULL, 0);
    ReleaseDC(NULL, pantalla);
    if (!memdc || !dib) return 1;
    dib_anterior = SelectObject(memdc, dib);
    juego.fb = (u32 *)dib_pixeles;
    juego.fb_ancho = VENTANA_ANCHO;
    juego.fb_alto = VENTANA_ALTO;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    juego.corriendo = 1;
    while (juego.corriendo) {
        t0 = GetTickCount();
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) juego.corriendo = 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        juego.entrada = leer_teclado();
        actualizar();
        if (!juego.corriendo) break;
        renderizar();
        {
            HDC hdc = GetDC(hwnd);
            BitBlt(hdc, 0, 0, VENTANA_ANCHO, VENTANA_ALTO, memdc, 0, 0,
                   SRCCOPY);
            ReleaseDC(hwnd, hdc);
        }
        /* fotograma sincronizado a FPS_OBJETIVO */
        while (GetTickCount() - t0 < (DWORD)MS_POR_FOTO) Sleep(1);
    }

    SelectObject(memdc, dib_anterior);
    DeleteObject(dib);
    DeleteDC(memdc);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Modo captura (sin ventana, determinista)                            */
/* ------------------------------------------------------------------ */

static int correr_captura(const char *ruta_bmp, int fotogramas) {
    int i;
    juego.fb = (u32 *)malloc((size_t)VENTANA_ANCHO * VENTANA_ALTO * sizeof(u32));
    if (!juego.fb) return 1;
    juego.fb_ancho = VENTANA_ANCHO;
    juego.fb_alto = VENTANA_ALTO;
    for (i = 0; i < fotogramas && juego.corriendo; i++) {
        actualizar();
        renderizar();
    }
    if (!activo_guardar_bmp(ruta_bmp, juego.fb, juego.fb_ancho, juego.fb_alto))
        return 1;
    free(juego.fb);
    juego.fb = NULL;
    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

static int opcion(const char *nombre) {
    return strcmp(nombre, "--logica") == 0 || strcmp(nombre, "--mapa") == 0 ||
           strcmp(nombre, "--captura") == 0 ||
           strcmp(nombre, "--fotogramas") == 0 || strcmp(nombre, "--tecla") == 0;
}

int WINAPI WinMain(HINSTANCE instancia, HINSTANCE prev, LPSTR cmdline, int show) {
    const char *ruta_logica = NULL, *ruta_mapa = NULL, *ruta_bmp = NULL;
    int fotogramas = 60;
    unsigned tecla = 0;
    int i;
    (void)prev; (void)cmdline; (void)show;

    for (i = 1; i < __argc; i++) {
        if (opcion(__argv[i]) && i + 1 < __argc) {
            const char *v = __argv[++i];
            if (strcmp(__argv[i - 1], "--logica") == 0) ruta_logica = v;
            else if (strcmp(__argv[i - 1], "--mapa") == 0) ruta_mapa = v;
            else if (strcmp(__argv[i - 1], "--captura") == 0) ruta_bmp = v;
            else if (strcmp(__argv[i - 1], "--fotogramas") == 0)
                fotogramas = atoi(v);
            else if (strcmp(__argv[i - 1], "--tecla") == 0) {
                if (strcmp(v, "izquierda") == 0) tecla = MOTOR_ENT_IZQUIERDA;
                else if (strcmp(v, "derecha") == 0) tecla = MOTOR_ENT_DERECHA;
                else if (strcmp(v, "arriba") == 0) tecla = MOTOR_ENT_ARRIBA;
                else if (strcmp(v, "abajo") == 0) tecla = MOTOR_ENT_ABAJO;
                else if (strcmp(v, "saltar") == 0) tecla = MOTOR_ENT_SALTO;
            }
        }
    }
    if (fotogramas < 1) fotogramas = 1;
    if (fotogramas > 100000) fotogramas = 100000;

    if (!activo_gdiplus_iniciar()) {
        MessageBoxA(NULL, "No se pudo iniciar GDI+ (gdiplus.dll)",
                    "egine", MB_ICONERROR);
        return 1;
    }

    memset(&juego, 0, sizeof(juego));
    juego.corriendo = 1;
    juego.x = 16;
    juego.y = 16;
    juego.entrada = tecla; /* en captura se mantiene pulsada */
    sprite_crear();

    if (ruta_mapa) {
        if (!activo_cargar_mapa(ruta_mapa, &juego.mapa)) {
            char msg[512];
            _snprintf(msg, sizeof(msg), "No se pudo cargar el mapa:\n%s",
                      ruta_mapa);
            msg[sizeof(msg) - 1] = '\0';
            MessageBoxA(NULL, msg, "egine", MB_ICONERROR);
            return 1;
        }
        strncpy(juego.mapa_usado, ruta_mapa, sizeof(juego.mapa_usado) - 1);
    } else {
        if (!activo_cargar_proyecto(ruta_logica, &juego.mapa,
                                    juego.mapa_usado,
                                    sizeof(juego.mapa_usado))) {
            MessageBoxA(NULL,
                        "No se pudo cargar el proyecto ni un mapa .mav\n"
                        "(busca un .logic o maps\\*.mav junto al juego)",
                        "egine", MB_ICONERROR);
            return 1;
        }
    }

    if (ruta_bmp) {
        buscar_spawn();
        return correr_captura(ruta_bmp, fotogramas);
    }
    buscar_spawn();
    return correr_ventana(instancia);
}
