/* ==========================================================================
 * juego_ventana.c - vista previa del juego dentro del editor (fase 9).
 *
 * Los botones play/pausa/stop del extremo derecho de la barra de menus
 * prueban el proyecto activo en una ventana propia, sin exportar ningun
 * .exe: el editor reune el mismo blob LGB1 que usa "Compilar para
 * Windows" (compilar_blob_memoria), lo registra en memoria como archivos
 * virtuales del motor (activo_blob_registrar) y monta el juego con el
 * nucleo en ensamblador (motor_*) y los scripts lobo de los botones
 * FUNCIONES.
 *
 * La ventana no bloquea al editor: el juego avanza con un timer de
 * Win32 y los botones de la barra cambian el estado (marcha, pausa,
 * cerrado). Escape y la X de la ventana equivalen a stop.
 * ========================================================================== */
#include "app.h"
#include "../motor/motor.h"

#define JV_ANCHO      640
#define JV_ALTO       480
#define JV_FPS        60
#define JV_MS_POR_FOTO (1000 / JV_FPS)
#define JV_SPRITE_LADO 24
#define JV_SPRITE_VEL  3
#define JV_CLASE      "LobosWolfosVistaPrevia"

/* ------------------------------------------------------------------ */
/* Estado de la vista previa                                           */
/* ------------------------------------------------------------------ */

static int g_estado = JUEGO_VENTANA_CERRADO;
static HWND g_hwnd;
static HDC g_memdc;
static HBITMAP g_dib;
static HGDIOBJ g_dib_anterior;
static u32 *g_fb;              /* framebuffer 640x480 (DIB o malloc) */
static int g_clase_lista;

static MotorMapa g_mapa;
static char g_mapa_usado[MAX_PATH];

static u32 g_sprite[JV_SPRITE_LADO * JV_SPRITE_LADO];
static int g_x, g_y;           /* posicion del sprite en el mundo */
static unsigned g_entrada;     /* bits MOTOR_ENT_* */
static int g_fotograma;

/* ------------------------------------------------------------------ */
/* Sprite del jugador: el mismo lobito azul de game.exe                */
/* ------------------------------------------------------------------ */

static void jv_sprite_crear(void) {
    int px, py;
    for (py = 0; py < JV_SPRITE_LADO; py++) {
        for (px = 0; px < JV_SPRITE_LADO; px++) {
            u32 c = 0xFF2456A6u;                    /* cuerpo */
            int borde = px == 0 || px == JV_SPRITE_LADO - 1 ||
                        py == 0 || py == JV_SPRITE_LADO - 1;
            int esquina = (px < 2 || px > JV_SPRITE_LADO - 3) &&
                         (py < 2 || py > JV_SPRITE_LADO - 3);
            int oreja = (px >= 1 && px <= 6 && py <= 7) ||
                        (px >= JV_SPRITE_LADO - 7 && px <= JV_SPRITE_LADO - 2 &&
                         py <= 7);
            int ojo = (py >= 8 && py <= 10) &&
                      ((px >= 5 && px <= 7) || (px >= 16 && px <= 18));
            int hocico = (py >= 14 && py <= 19) &&
                         (px >= 9 && px <= 14);
            if (esquina) { c = 0; }
            else if (oreja) c = 0xFF16325Eu;         /* orejas oscuras */
            else if (borde) c = 0xFF16325Eu;
            else if (ojo) c = 0xFFFFFFFFu;           /* ojos */
            else if (hocico) c = 0xFFD8E6F8u;        /* hocico claro */
            g_sprite[py * JV_SPRITE_LADO + px] = c;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Colision del sprite contra las celdas solidas del mapa              */
/* ------------------------------------------------------------------ */

static int jv_choca_con_mapa_wh(int x, int y, int w, int h) {
    const MotorMapa *m = &g_mapa;
    int i;
    for (i = 0; i < m->num_celdas; i++) {
        const MotorCelda *c = &m->celdas[i];
        if (motor_aabb(x, y, w, h,
                       c->x * m->lado, c->y * m->lado, m->lado, m->lado))
            return 1;
    }
    return 0;
}

static int jv_choca_con_mapa(int x, int y) {
    return jv_choca_con_mapa_wh(x, y, JV_SPRITE_LADO, JV_SPRITE_LADO);
}

/* Punto libre para aparecer, con 8 pixeles de holgura (igual que el
 * buscar_spawn de motor/juego.c). */
static void jv_buscar_spawn(void) {
    const int margen = 8;
    int r, x, y;
    if (!jv_choca_con_mapa_wh(g_x - margen, g_y - margen,
                              JV_SPRITE_LADO + 2 * margen,
                              JV_SPRITE_LADO + 2 * margen))
        return;
    for (r = 16; r <= 4096; r += 16) {
        for (y = -r; y <= r; y += 8) {
            int paso = (y == -r || y == r) ? 8 : 2 * r;
            for (x = -r; x <= r; x += paso) {
                if (!jv_choca_con_mapa_wh(x - margen, y - margen,
                                          JV_SPRITE_LADO + 2 * margen,
                                          JV_SPRITE_LADO + 2 * margen)) {
                    g_x = x;
                    g_y = y;
                    return;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Scripts lobo: el codigo de los botones FUNCIONES (fase 8)           */
/* ------------------------------------------------------------------ */

static LoboPrograma g_programas[ACTIVO_FUNCIONES_MAX];
static int g_prog_id[ACTIVO_FUNCIONES_MAX];
static int g_num_programas;

static struct {
    int prog;
    LoboMaquina maquina;
} g_scripts[ACTIVO_USADAS_MAX];
static int g_num_scripts;

/* Callback de choca() del lenguaje. */
static int jv_lobo_choca(void *ctx, int x, int y) {
    (void)ctx;
    return jv_choca_con_mapa(x, y);
}

/* Compila los scripts del proyecto y prepara una maquina por elemento.
 * Las funciones que no compilan se avisan (si `avisar`) y se saltan: el
 * resto del juego sigue corriendo. */
static void jv_scripts_preparar(int avisar) {
    int ids[ACTIVO_USADAS_MAX];
    int n = activo_funciones_usadas(ids, ACTIVO_USADAS_MAX);
    int i, j;

    g_num_programas = 0;
    g_num_scripts = 0;
    for (i = 0; i < n; i++) {
        const char *codigo = activo_funcion_codigo(ids[i]);
        char error[LOBO_ERROR_MAX];
        int prog = -1;

        if (!codigo || !codigo[0]) continue;

        for (j = 0; j < g_num_programas; j++)
            if (g_prog_id[j] == ids[i]) { prog = j; break; }
        if (prog < 0) {
            if (!lobo_parsear(codigo, &g_programas[g_num_programas],
                              error, sizeof(error))) {
                if (avisar) {
                    char msg[256];
                    _snprintf(msg, sizeof(msg),
                              "La funcion %d no compila:\n%s", ids[i], error);
                    msg[sizeof(msg) - 1] = '\0';
                    MessageBoxA(g.hwnd, msg, "Vista previa",
                                MB_ICONWARNING);
                }
                continue;
            }
            g_prog_id[g_num_programas] = ids[i];
            prog = g_num_programas++;
        }
        g_scripts[g_num_scripts].prog = prog;
        lobo_maquina_reiniciar(&g_scripts[g_num_scripts].maquina);
        g_num_scripts++;
    }
}

/* ------------------------------------------------------------------ */
/* Actualizar y renderizar un fotograma                                */
/* ------------------------------------------------------------------ */

/* Devuelve 1 si el juego sigue y 0 si un script pidio cerrar. */
static int jv_actualizar(void) {
    unsigned e = g_entrada;

    g_fotograma++;

    /* Escape cierra siempre, alla donde se mueva el lobo. */
    if (e & MOTOR_ENT_CERRAR) return 0;

    if (g_num_scripts > 0) {
        /* Modo scripts: el codigo lobo manda y el movimiento por defecto
         * queda fuera (igual que game.exe). */
        LoboEstado st;
        int i;
        st.x = g_x;
        st.y = g_y;
        st.vel = JV_SPRITE_VEL;
        st.fotograma = g_fotograma;
        st.entrada = g_entrada; /* LOBO_TECLA_* == MOTOR_ENT_* */
        st.choca = jv_lobo_choca;
        st.ctx = NULL;
        for (i = 0; i < g_num_scripts; i++) {
            lobo_ejecutar(&g_programas[g_scripts[i].prog], &st,
                          &g_scripts[i].maquina);
            if (g_scripts[i].maquina.cerrar) return 0;
        }
        g_x = st.x;
        g_y = st.y;
        return 1;
    }

    /* Movimiento por defecto (sin scripts en el proyecto). */
    {
        int vx = 0, vy = 0;
        if (e & MOTOR_ENT_IZQUIERDA) vx -= JV_SPRITE_VEL;
        if (e & MOTOR_ENT_DERECHA)   vx += JV_SPRITE_VEL;
        if (e & MOTOR_ENT_ARRIBA)    vy -= JV_SPRITE_VEL;
        if (e & MOTOR_ENT_ABAJO)     vy += JV_SPRITE_VEL;

        /* movimiento separado por eje para deslizar contra los tiles */
        if (vx) {
            g_x += vx;
            if (jv_choca_con_mapa(g_x, g_y)) g_x -= vx;
        }
        if (vy) {
            g_y += vy;
            if (jv_choca_con_mapa(g_x, g_y)) g_y -= vy;
        }
    }
    return 1;
}

static void jv_renderizar(void) {
    MotorMapa *m = &g_mapa;
    int cam_x, cam_y;

    if (!g_fb) return;

    /* Sin mapa el proyecto corre igual, pero no se pinta nada: la
     * ventana queda en negro (fase 12). */
    if (!g_mapa_usado[0]) {
        motor_limpiar(g_fb, JV_ANCHO, JV_ALTO, 0x00000000u);
        return;
    }

    /* camara centrada en el sprite */
    cam_x = g_x + JV_SPRITE_LADO / 2 - JV_ANCHO / 2;
    cam_y = g_y + JV_SPRITE_LADO / 2 - JV_ALTO / 2;

    motor_limpiar(g_fb, JV_ANCHO, JV_ALTO, 0x00101826u);
    motor_mapa_render(g_fb, JV_ANCHO, JV_ALTO, cam_x, cam_y, m->lado,
                      m->celdas, m->num_celdas, m->imagenes);
    motor_blit_alpha(g_fb, JV_ANCHO, JV_ALTO,
                     g_x - cam_x, g_y - cam_y,
                     g_sprite, JV_SPRITE_LADO, JV_SPRITE_LADO);
}

static void jv_blit(HWND hwnd) {
    HDC dc;
    if (!hwnd || !g_memdc) return;
    dc = GetDC(hwnd);
    if (dc) {
        BitBlt(dc, 0, 0, JV_ANCHO, JV_ALTO, g_memdc, 0, 0, SRCCOPY);
        ReleaseDC(hwnd, dc);
    }
}

/* ------------------------------------------------------------------ */
/* Teclado                                                             */
/* ------------------------------------------------------------------ */

static unsigned jv_leer_teclado(void) {
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

/* ------------------------------------------------------------------ */
/* Carga y liberacion de los recursos del juego                        */
/* ------------------------------------------------------------------ */

/* Libera el mapa, los archivos virtuales, los scripts y el DIB. No
 * libera g_fb: en modo captura el buffer es de malloc (lo libera el
 * llamador). */
static void jv_liberar(void) {
    if (g_memdc) {
        if (g_dib_anterior) SelectObject(g_memdc, g_dib_anterior);
        if (g_dib) DeleteObject(g_dib);
        DeleteDC(g_memdc);
    }
    g_memdc = NULL;
    g_dib = NULL;
    g_dib_anterior = NULL;
    g_fb = NULL;
    motor_mapa_liberar(&g_mapa);
    activo_virtuales_limpiar();
    g_num_programas = 0;
    g_num_scripts = 0;
    g_estado = JUEGO_VENTANA_CERRADO;
}

/* Reune el proyecto en memoria y lo carga en el motor. `avisar` muestra
 * los errores en un cuadro (modo ventana); en la captura se callan. */
static int jv_cargar_proyecto(int avisar) {
    unsigned char *blob = NULL;
    int largo = 0;

    if (!compilar_blob_memoria(&blob, &largo)) {
        if (avisar)
            MessageBoxA(g.hwnd, compilar_ultimo_error(), "Vista previa",
                        MB_ICONWARNING);
        return 0;
    }
    if (!activo_gdiplus_iniciar()) {
        free(blob);
        if (avisar)
            MessageBoxA(g.hwnd, "No se pudo iniciar GDI+ (gdiplus.dll).",
                        "Vista previa", MB_ICONERROR);
        return 0;
    }
    if (activo_blob_registrar(blob, largo) <= 0) {
        free(blob);
        if (avisar)
            MessageBoxA(g.hwnd,
                        "No se pudo preparar el proyecto para la vista previa.",
                        "Vista previa", MB_ICONERROR);
        activo_virtuales_limpiar();
        return 0;
    }
    free(blob);

    memset(&g_mapa, 0, sizeof(g_mapa));
    if (!activo_cargar_proyecto("proyecto.logic", &g_mapa,
                                g_mapa_usado, sizeof(g_mapa_usado))) {
        if (avisar)
            MessageBoxA(g.hwnd,
                        "No se pudo cargar el mapa del proyecto.\n"
                        "Revisa el elemento de mapa y la carpeta maps.",
                        "Vista previa", MB_ICONERROR);
        motor_mapa_liberar(&g_mapa);
        activo_virtuales_limpiar();
        return 0;
    }

    jv_sprite_crear();
    g_x = 16;
    g_y = 16;
    g_fotograma = 0;
    g_entrada = 0;
    jv_buscar_spawn();
    jv_scripts_preparar(avisar);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Ventana                                                             */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK jv_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (g_memdc)
            BitBlt(dc, 0, 0, JV_ANCHO, JV_ALTO, g_memdc, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) juego_ventana_detener();
        return 0;
    case WM_TIMER:
        if (wp == 1 && g_estado == JUEGO_VENTANA_JUGANDO) {
            int sigue;
            g_entrada = jv_leer_teclado();
            sigue = jv_actualizar();
            jv_renderizar();
            jv_blit(hwnd);
            if (!sigue) juego_ventana_detener();
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        g_hwnd = NULL;
        jv_liberar();
        app_invalidar();
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* Crea la ventana y el DIB de 640x480. Devuelve 1 si todo fue bien. */
static int jv_crear_ventana(void) {
    HINSTANCE inst = GetModuleHandleA(NULL);
    WNDCLASSA clase;
    RECT rect;
    HWND hwnd;
    BITMAPINFO bmi;
    HDC pantalla;
    char titulo[MAX_PATH + 64];

    if (!g_clase_lista) {
        memset(&clase, 0, sizeof(clase));
        clase.lpfnWndProc = jv_proc;
        clase.hInstance = inst;
        clase.hCursor = LoadCursorA(NULL, IDC_ARROW);
        clase.hbrBackground = NULL;
        clase.lpszClassName = JV_CLASE;
        if (!RegisterClassA(&clase)) return 0;
        g_clase_lista = 1;
    }

    rect.left = 0; rect.top = 0;
    rect.right = JV_ANCHO; rect.bottom = JV_ALTO;
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    _snprintf(titulo, sizeof(titulo), "Vista previa - %s",
              g_mapa_usado[0] ? g_mapa_usado : "sin mapa");
    titulo[sizeof(titulo) - 1] = '\0';
    hwnd = CreateWindowA(JV_CLASE, titulo, WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         rect.right - rect.left, rect.bottom - rect.top,
                         NULL, NULL, inst, NULL);
    if (!hwnd) return 0;
    g_hwnd = hwnd;

    /* framebuffer: DIB de 32 bits top-down, igual que el editor */
    pantalla = GetDC(NULL);
    g_memdc = CreateCompatibleDC(pantalla);
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = JV_ANCHO;
    bmi.bmiHeader.biHeight = -JV_ALTO;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    g_dib = CreateDIBSection(pantalla, &bmi, DIB_RGB_COLORS,
                             (void **)&g_fb, NULL, 0);
    ReleaseDC(NULL, pantalla);
    if (!g_memdc || !g_dib) {
        DestroyWindow(hwnd); /* WM_DESTROY limpia lo que haya */
        return 0;
    }
    g_dib_anterior = SelectObject(g_memdc, g_dib);
    return 1;
}

/* ------------------------------------------------------------------ */
/* API para la barra de menus y las pruebas                            */
/* ------------------------------------------------------------------ */

int juego_ventana_estado(void) {
    return g_estado;
}

/* Play: abre la vista previa. En pausa reanuda; si ya esta en marcha,
 * reinicia el proyecto (util para probar cambios en la logica). */
int juego_ventana_abrir(void) {
    if (g_estado == JUEGO_VENTANA_PAUSA) {
        juego_ventana_reanudar();
        return 1;
    }
    if (g_estado != JUEGO_VENTANA_CERRADO) juego_ventana_detener();

    if (!jv_cargar_proyecto(1)) return 0;
    if (!jv_crear_ventana()) {
        jv_liberar();
        app_invalidar();
        return 0;
    }

    g_estado = JUEGO_VENTANA_JUGANDO;
    jv_renderizar();
    jv_blit(g_hwnd);
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    SetForegroundWindow(g_hwnd);
    SetFocus(g_hwnd);
    SetTimer(g_hwnd, 1, JV_MS_POR_FOTO, NULL);
    app_estado("Vista previa: %s",
               g_mapa_usado[0] ? g_mapa_usado : "sin mapa (ventana en negro)");
    app_invalidar();
    return 1;
}

void juego_ventana_pausar(void) {
    if (g_estado != JUEGO_VENTANA_JUGANDO) return;
    g_estado = JUEGO_VENTANA_PAUSA;
    g_entrada = 0;
    app_estado("Vista previa en pausa.");
    app_invalidar();
}

void juego_ventana_reanudar(void) {
    if (g_estado != JUEGO_VENTANA_PAUSA) return;
    g_estado = JUEGO_VENTANA_JUGANDO;
    g_entrada = 0;
    app_estado("Vista previa en marcha.");
    app_invalidar();
}

void juego_ventana_detener(void) {
    if (g_hwnd) {
        DestroyWindow(g_hwnd); /* WM_DESTROY libera el juego */
        g_hwnd = NULL;
    } else if (g_estado != JUEGO_VENTANA_CERRADO) {
        jv_liberar();
    }
    app_invalidar();
}

/* Cierre al salir del editor. */
void juego_ventana_terminar(void) {
    if (g_hwnd) {
        HWND hwnd = g_hwnd;
        g_hwnd = NULL;
        DestroyWindow(hwnd);
    }
    jv_liberar();
    activo_gdiplus_terminar();
}

/* Captura determinista sin ventana, para comparar la vista previa con el
 * game.exe compilado: actualiza N fotogramas y guarda el framebuffer. */
int juego_ventana_capturar(const char *ruta, int fotogramas) {
    int i, ok;

    if (!ruta || !ruta[0]) return 0;
    if (fotogramas < 1) fotogramas = 1;
    if (fotogramas > 100000) fotogramas = 100000;

    if (g_estado != JUEGO_VENTANA_CERRADO) juego_ventana_detener();
    if (!jv_cargar_proyecto(0)) return 0;

    g_fb = (u32 *)malloc((size_t)JV_ANCHO * JV_ALTO * sizeof(u32));
    if (!g_fb) {
        jv_liberar();
        return 0;
    }

    g_estado = JUEGO_VENTANA_JUGANDO;
    for (i = 0; i < fotogramas; i++) {
        int sigue;
        g_entrada = 0;
        sigue = jv_actualizar();
        jv_renderizar();
        if (!sigue) break;
    }
    ok = activo_guardar_bmp(ruta, g_fb, JV_ANCHO, JV_ALTO);
    free(g_fb);
    g_fb = NULL;
    jv_liberar();
    app_invalidar();
    return ok;
}
