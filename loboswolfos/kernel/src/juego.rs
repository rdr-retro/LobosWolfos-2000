//! juego.rs - el juego montado sobre el nucleo del motor (fase 6).
//!
//! Puerto de `motor/juego.c` (el `game.exe` de Windows): el mismo juego
//! corriendo sobre el kernel loboswolfos en vez de sobre Win32. La logica
//! es identica (sprite, camara, colisiones y ritmo de 60 fotogramas por
//! segundo); lo que cambia es el andamiaje:
//!
//!   - el mapa no se lee de `maps\` sino del blob LGB1 de la zona de
//!     juego del disco (activo.rs),
//!   - el framebuffer es una pantalla de RAM que se vuelca al modo VBE
//!     activo (presentando centrado si el modo es mayor que 640x480),
//!   - el ritmo lo marca el PIT a 100 Hz (cada tic avanza 3/5 de
//!     fotograma: 60 actualizaciones por segundo exactas),
//!   - la entrada sondea el teclado (flechas E0, WASD, espacio y Escape
//!     en input.rs) y, para las pruebas por serie, las letras ASCII,
//!   - los botones FUNCIONES del proyecto corren su codigo lobo cada
//!     fotograma (fase 8, lobo.rs): cuando hay scripts, el codigo manda
//!     y el movimiento por defecto queda fuera (como juego.c).

use crate::activo;
use crate::cpu;
use crate::gfx;
use crate::input;
use crate::lobo;
use crate::motor::{self, MotorCelda, MotorMapa};
use crate::motor::{
    MOTOR_ENT_ABAJO, MOTOR_ENT_ARRIBA, MOTOR_ENT_CERRAR, MOTOR_ENT_DERECHA, MOTOR_ENT_IZQUIERDA,
    MOTOR_ENT_SALTO,
};
use crate::mouse;
use crate::pit;
use crate::println;
use crate::sync::SyncCell;

const VENTANA_ANCHO: i32 = 640;
const VENTANA_ALTO: i32 = 480;
const SPRITE_LADO: i32 = 24;
const SPRITE_VEL: i32 = 3;
/// Azul noche, el mismo fondo de game.exe.
const FONDO: u32 = 0x00101826;

/// Framebuffer del juego: 640x480 pixeles de 4 bytes (1.17 MiB). Vive en
/// .bss, que kernel.ld deja acabar antes de la pila de 3 MiB.
static FB: SyncCell<[u32; (VENTANA_ANCHO * VENTANA_ALTO) as usize]> =
    SyncCell::new([0; (VENTANA_ANCHO * VENTANA_ALTO) as usize]);

/// Sprite del jugador (se genera al arrancar, como sprite_crear).
static SPRITE: SyncCell<[u32; (SPRITE_LADO * SPRITE_LADO) as usize]> =
    SyncCell::new([0; (SPRITE_LADO * SPRITE_LADO) as usize]);

/* ------------------------------------------------------------------ */
/* Scripts lobo: el codigo de los botones FUNCIONES (fase 8)           */
/* ------------------------------------------------------------------ */

/* Un script por elemento del lienzo con funcion de codigo: todos ven
 * el mismo estado del juego (la posicion del lobo, las teclas) y cada
 * uno guarda su maquina (sus variables locales). Los elementos que
 * repiten funcion comparten el programa compilado. */
static PROGRAMAS: SyncCell<[lobo::LoboPrograma; activo::FUNCIONES_MAX]> =
    SyncCell::new([lobo::LoboPrograma::NUEVO; activo::FUNCIONES_MAX]);
static PROG_ID: SyncCell<[i32; activo::FUNCIONES_MAX]> =
    SyncCell::new([0; activo::FUNCIONES_MAX]);
static NUM_PROGRAMAS: SyncCell<usize> = SyncCell::new(0);

/// Un script corriendo: su programa (compartido por id) y su maquina.
struct Script {
    /// Indice del programa en PROGRAMAS.
    prog: usize,
    maquina: lobo::LoboMaquina,
}

impl Script {
    const NUEVO: Script = Script {
        prog: 0,
        maquina: lobo::LoboMaquina::NUEVA,
    };
}

static SCRIPTS: SyncCell<[Script; activo::USADAS_MAX]> =
    SyncCell::new([Script::NUEVO; activo::USADAS_MAX]);
static NUM_SCRIPTS: SyncCell<usize> = SyncCell::new(0);

/// Compila los scripts del proyecto y prepara una maquina por elemento
/// (el puerto de scripts_preparar de juego.c). Las funciones vacias o
/// borradas se saltan; las que no compilan se avisan por la serie y
/// tambien se saltan: el resto del juego sigue corriendo.
fn scripts_preparar() {
    let ids = activo::funciones_usadas();
    let programas = unsafe { &mut *PROGRAMAS.get() };
    let prog_id = unsafe { &mut *PROG_ID.get() };
    let scripts = unsafe { &mut *SCRIPTS.get() };
    let mut num_programas = 0usize;
    let mut num_scripts = 0usize;

    for &id in ids {
        let Some(codigo) = activo::funcion_codigo(id) else {
            continue; // id colgante: conservado, pero no hay codigo
        };
        if codigo.is_empty() {
            continue;
        }

        // Los elementos que repiten funcion comparten el programa.
        let mut prog = None;
        for j in 0..num_programas {
            if prog_id[j] == id {
                prog = Some(j);
                break;
            }
        }
        let prog = match prog {
            Some(j) => j,
            None => {
                if num_programas >= activo::FUNCIONES_MAX {
                    continue; // a lo sumo hay FUNCIONES_MAX funciones
                }
                let mut error = [0u8; lobo::LOBO_ERROR_MAX];
                if !lobo::parsear(codigo, &mut programas[num_programas], &mut error) {
                    let n = error.iter().position(|&b| b == 0).unwrap_or(error.len());
                    let msg = core::str::from_utf8(&error[..n]).unwrap_or("(ilegible)");
                    println!("egine: lobo: error {} {}.", id, msg);
                    continue;
                }
                prog_id[num_programas] = id;
                num_programas += 1;
                num_programas - 1
            }
        };
        scripts[num_scripts].prog = prog;
        scripts[num_scripts].maquina.reiniciar();
        num_scripts += 1;
    }

    unsafe { *NUM_PROGRAMAS.get() = num_programas };
    unsafe { *NUM_SCRIPTS.get() = num_scripts };
    println!("egine: lobo: {} scripts.", num_scripts);
}

/// Colision AABB de la caja del sprite contra las celdas del mapa,
/// fuera del Juego para poder cerrarla sobre datos copiados (los
/// scripts escriben x e y al final, como juego.c: no hace falta el
/// prestamo del Juego mientras corren).
fn choca_caja(x: i32, y: i32, celdas: &[MotorCelda], lado: i32) -> bool {
    for c in celdas {
        if motor::aabb(
            x,
            y,
            SPRITE_LADO,
            SPRITE_LADO,
            c.x * lado,
            c.y * lado,
            lado,
            lado,
        ) {
            return true;
        }
    }
    false
}

struct Juego {
    mapa: MotorMapa,
    /// Proyecto sin mapa (fase 12): no se pinta nada y la pantalla queda
    /// en negro, pero los programas siguen corriendo.
    sin_mapa: bool,
    /// Posicion del sprite en el mundo.
    x: i32,
    y: i32,
    /// Bits MOTOR_ENT_*.
    entrada: u32,
    /// Contador de actualizaciones (lo leen los scripts).
    fotograma: i32,
    corriendo: bool,
}

impl Juego {
    /// Un fotograma de logica: los scripts lobo del proyecto si los hay
    /// (fase 8), o el movimiento por defecto si no.
    fn actualizar(&mut self) {
        let e = self.entrada;

        self.fotograma += 1;

        /* Escape cierra siempre, alla donde se mueva el lobo. */
        if e & MOTOR_ENT_CERRAR != 0 {
            self.corriendo = false;
        }

        let num_scripts = unsafe { *NUM_SCRIPTS.get() }.min(activo::USADAS_MAX);
        if num_scripts > 0 {
            /* Modo scripts (fase 8): el codigo lobo de los botones
             * FUNCIONES manda y el movimiento por defecto queda fuera.
             * Los scripts ven una copia del estado y al final solo se
             * traen de vuelta x e y (vel y fotograma son de entrada, no
             * de vuelta: como juego.c). */
            let mut st = lobo::LoboEstado {
                x: self.x,
                y: self.y,
                vel: SPRITE_VEL,
                fotograma: self.fotograma,
                entrada: e, /* LOBO_TECLA_* == MOTOR_ENT_* */
            };
            /* El mapa se copia a datos sueltos para que el cierre de
             * choca() no tome prestado el Juego mientras corren. */
            let celdas_ptr = self.mapa.celdas;
            let num_celdas = self.mapa.num_celdas;
            let lado = self.mapa.lado;
            let celdas =
                unsafe { core::slice::from_raw_parts(celdas_ptr, num_celdas as usize) };
            let programas = unsafe { &*PROGRAMAS.get() };
            let scripts = unsafe { &mut *SCRIPTS.get() };
            for s in scripts.iter_mut().take(num_scripts) {
                let mut choca = |cx: i32, cy: i32| choca_caja(cx, cy, celdas, lado);
                lobo::ejecutar(&programas[s.prog], &mut st, &mut choca, &mut s.maquina);
                if s.maquina.cerrar {
                    self.corriendo = false;
                }
            }
            self.x = st.x;
            self.y = st.y;
            return;
        }

        /* Movimiento por defecto (sin scripts en el proyecto). */
        let mut vx = 0;
        let mut vy = 0;
        if e & MOTOR_ENT_IZQUIERDA != 0 {
            vx -= SPRITE_VEL;
        }
        if e & MOTOR_ENT_DERECHA != 0 {
            vx += SPRITE_VEL;
        }
        if e & MOTOR_ENT_ARRIBA != 0 {
            vy -= SPRITE_VEL;
        }
        if e & MOTOR_ENT_ABAJO != 0 {
            vy += SPRITE_VEL;
        }

        if vx != 0 {
            self.x += vx;
            if self.choca(self.x, self.y) {
                self.x -= vx;
            }
        }
        if vy != 0 {
            self.y += vy;
            if self.choca(self.x, self.y) {
                self.y -= vy;
            }
        }
    }

    /// Colision AABB de una caja contra las celdas solidas del mapa (todas
    /// las celdas son muros, como en juego.c).
    fn choca_wh(&self, x: i32, y: i32, ancho: i32, alto: i32) -> bool {
        let celdas = unsafe {
            core::slice::from_raw_parts(self.mapa.celdas, self.mapa.num_celdas as usize)
        };
        let lado = self.mapa.lado;
        for c in celdas {
            if motor::aabb(
                x,
                y,
                ancho,
                alto,
                c.x * lado,
                c.y * lado,
                lado,
                lado,
            ) {
                return true;
            }
        }
        false
    }

    /// Colision AABB del sprite contra las celdas solidas del mapa.
    fn choca(&self, x: i32, y: i32) -> bool {
        self.choca_wh(x, y, SPRITE_LADO, SPRITE_LADO)
    }

    /// Busca un punto libre para aparecer: en espiral desde la posicion
    /// inicial hasta encontrar un hueco con margen (el sprite mas 8 pixeles
    /// de holgura en cada direccion), para no nacer pegado a un muro. Sin
    /// esto el sprite naceria dentro del tile (0,0) y no podria moverse.
    fn buscar_spawn(&mut self) {
        const MARGEN: i32 = 8;
        let caja = SPRITE_LADO + 2 * MARGEN;
        if !self.choca_wh(self.x - MARGEN, self.y - MARGEN, caja, caja) {
            return; // la posicion inicial ya esta libre
        }
        let mut r = 16;
        while r <= 4096 {
            let mut y = -r;
            while y <= r {
                // El perimetro del anillo se recorre fino (paso 8) y lo de
                // en medio a saltos: solo hace falta mirar el borde.
                let paso = if y == -r || y == r { 8 } else { 2 * r };
                let mut x = -r;
                while x <= r {
                    if !self.choca_wh(x - MARGEN, y - MARGEN, caja, caja) {
                        self.x = x;
                        self.y = y;
                        return;
                    }
                    x += paso;
                }
                y += 8;
            }
            r += 16;
        }
    }

    /// Camara centrada en el sprite + mapa + sprite con transparencia.
    fn renderizar(&self, fb: &mut [u32]) {
        /* Sin mapa no se pinta nada: pantalla negra (fase 12). */
        if self.sin_mapa {
            motor::limpiar(fb.as_mut_ptr(), VENTANA_ANCHO, VENTANA_ALTO, 0);
            return;
        }

        let cam_x = self.x + SPRITE_LADO / 2 - VENTANA_ANCHO / 2;
        let cam_y = self.y + SPRITE_LADO / 2 - VENTANA_ALTO / 2;

        motor::limpiar(fb.as_mut_ptr(), VENTANA_ANCHO, VENTANA_ALTO, FONDO);
        motor::mapa_render(
            fb.as_mut_ptr(),
            VENTANA_ANCHO,
            VENTANA_ALTO,
            cam_x,
            cam_y,
            &self.mapa,
        );
        let sprite = unsafe { &*SPRITE.get() };
        motor::blit_alpha(
            fb.as_mut_ptr(),
            VENTANA_ANCHO,
            VENTANA_ALTO,
            self.x - cam_x,
            self.y - cam_y,
            sprite.as_ptr(),
            SPRITE_LADO,
            SPRITE_LADO,
        );
    }
}

/// Sprite del jugador: un lobito azul de 24x24 dibujado a mano (el mismo
/// de juego.c).
fn sprite_crear() {
    let sprite = unsafe { &mut *SPRITE.get() };
    let l = SPRITE_LADO;
    for py in 0..l {
        for px in 0..l {
            let mut c = 0xFF2456A6u32; // cuerpo
            let borde = px == 0 || px == l - 1 || py == 0 || py == l - 1;
            let esquina = (px < 2 || px > l - 3) && (py < 2 || py > l - 3);
            let oreja = (px >= 1 && px <= 6 && py <= 7) || (px >= l - 7 && px <= l - 2 && py <= 7);
            let ojo = (py >= 8 && py <= 10) && ((px >= 5 && px <= 7) || (px >= 16 && px <= 18));
            let hocico = (py >= 14 && py <= 19) && (px >= 9 && px <= 14);
            if esquina {
                c = 0; // esquinas redondeadas: transparentes
            } else if oreja {
                c = 0xFF16325Eu32; // orejas oscuras
            } else if borde {
                c = 0xFF16325Eu32;
            } else if ojo {
                c = 0xFFFFFFFFu32; // ojos
            } else if hocico {
                c = 0xFFD8E6F8u32; // hocico claro
            }
            sprite[(py * l + px) as usize] = c;
        }
    }
}

/// Copia el framebuffer del juego al modo VBE activo, centrado si el
/// modo es mayor que 640x480 (respetando el paso de linea del modo).
fn presentar(fb: &[u32], mi: &gfx::ModeInfo) {
    let lfb = mi.lfb as *mut u32;
    let stride = (mi.pitch as usize) / 4;
    let ox = ((mi.width as i32 - VENTANA_ANCHO) / 2).max(0) as usize;
    let oy = ((mi.height as i32 - VENTANA_ALTO) / 2).max(0) as usize;
    for y in 0..VENTANA_ALTO as usize {
        let dst = unsafe { lfb.add((y + oy) * stride + ox) };
        let src = &fb[y * VENTANA_ANCHO as usize..(y + 1) * VENTANA_ANCHO as usize];
        unsafe { core::ptr::copy_nonoverlapping(src.as_ptr(), dst, VENTANA_ANCHO as usize) };
    }
}

/// Letra llegada por la serie (ASCII) a bits del motor: para probar el
/// juego sin teclado, meneando la entrada desde el monitor de QEMU o un
/// cable de serie. Vale como un toque: un solo fotograma.
fn bits_de_serie(tecla: u8) -> u32 {
    match tecla {
        b'a' | b'A' => MOTOR_ENT_IZQUIERDA,
        b'd' | b'D' => MOTOR_ENT_DERECHA,
        b'w' | b'W' => MOTOR_ENT_ARRIBA,
        b's' | b'S' => MOTOR_ENT_ABAJO,
        b' ' => MOTOR_ENT_SALTO,
        0x1B => MOTOR_ENT_CERRAR,
        _ => 0,
    }
}

/// Carga el proyecto de la zona de juego del disco y corre el bucle
/// principal hasta que se pida cerrar (Escape).
pub fn arrancar(mi: &gfx::ModeInfo) -> ! {
    sprite_crear();
    let Some(mapa) = activo::cargar_todo() else {
        println!("egine: sin proyecto que cargar; el kernel se queda en modo texto.");
        crate::bucle_texto()
    };
    // Un proyecto sin mapa es valido (fase 12): el juego corre con la
    // pantalla en negro, sin mapa ni sprite.
    let sin_mapa = mapa.num_celdas == 0;
    if sin_mapa {
        println!("egine: sin mapa: ventana en negro.");
    }
    // Compila los botones FUNCIONES del proyecto (fase 8): el resto del
    // juego sigue corriendo aunque alguno no compile.
    scripts_preparar();
    let fb = unsafe { &mut *FB.get() };

    let mut juego = Juego {
        mapa,
        sin_mapa,
        x: 16,
        y: 16,
        entrada: 0,
        fotograma: 0,
        corriendo: true,
    };
    // El hueco de nacimiento con margen, para no empezar pegado a un muro.
    juego.buscar_spawn();
    juego.renderizar(fb);
    presentar(fb, mi);
    println!(
        "egine: juego listo (sprite en ({}, {}), teclas: flechas/WASD, Escape sale).",
        juego.x, juego.y
    );

    // Ritmo: el PIT late a 100 Hz y el juego corre a 60 fotogramas/s
    // (cada tic avanza 3/5 de fotograma, en aritmetica entera).
    let mut acumulado = 0u32;
    let mut ultimo_tick = pit::ticks();
    let mut ultimo_segundo = pit::seconds();

    // Toques pendientes: una tecla soltada entre dos sondeos (o una letra
    // de la serie) espera aqui hasta que corra un fotograma. Sin esto se
    // perderia cuando el acumulador todavia no llega (un tic son 3/5 de
    // fotograma: a veces hay que esperar al siguiente).
    let mut toque: u32 = 0;

    loop {
        // Entrada por sondeo, como el resto del kernel: sin IRQ1.
        let mut serie = 0u32;
        while let Some(tecla) = input::poll() {
            let bits = bits_de_serie(tecla);
            if bits == 0 {
                println!("egine: tecla 0x{:02X}.", tecla);
            }
            serie |= bits;
        }
        // El juego no usa el raton, pero hay que seguir drenando el
        // controlador PS/2 para que sus paquetes no llenen el bufer.
        mouse::poll();

        let ahora = pit::ticks();
        acumulado += ((ahora - ultimo_tick) * 3) as u32;
        ultimo_tick = ahora;

        let mantenida = input::entrada();
        let mut actualizado = false;
        if acumulado >= 5 {
            // Los toques cuentan para el primer fotograma del lote.
            let pulso = input::tomar_pulsos() | toque | serie;
            toque = 0;
            let mut primero = true;
            while acumulado >= 5 {
                acumulado -= 5;
                juego.entrada = mantenida | if primero { pulso } else { 0 };
                primero = false; // un toque suelto vale un solo fotograma
                juego.actualizar();
                actualizado = true;
            }
        } else {
            toque |= serie;
        }
        if !juego.corriendo {
            println!("egine: fin del juego.");
            loop {
                unsafe { cpu::hlt() };
            }
        }
        if actualizado {
            juego.renderizar(fb);
            presentar(fb, mi);
        }

        // Rastro por la serie (la prueba automatica mira estas lineas).
        let segundo = pit::seconds();
        if segundo != ultimo_segundo {
            ultimo_segundo = segundo;
            println!("egine: sprite en ({}, {}).", juego.x, juego.y);
        }

        // Duerme hasta la proxima IRQ0.
        unsafe { cpu::hlt() };
    }
}
