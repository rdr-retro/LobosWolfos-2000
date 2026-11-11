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
//!     en input.rs) y, para las pruebas por serie, las letras ASCII.

use crate::activo;
use crate::cpu;
use crate::gfx;
use crate::input;
use crate::motor::{self, MotorMapa};
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

struct Juego {
    mapa: MotorMapa,
    /// Posicion del sprite en el mundo.
    x: i32,
    y: i32,
    /// Bits MOTOR_ENT_*.
    entrada: u32,
    corriendo: bool,
}

impl Juego {
    /// Movimiento separado por eje para deslizar contra los tiles.
    fn actualizar(&mut self) {
        let mut vx = 0;
        let mut vy = 0;
        let e = self.entrada;

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
        if e & MOTOR_ENT_CERRAR != 0 {
            self.corriendo = false;
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
        for c in celdas {
            if motor::aabb(
                x,
                y,
                ancho,
                alto,
                c.x * self.mapa.lado,
                c.y * self.mapa.lado,
                self.mapa.lado,
                self.mapa.lado,
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
    let fb = unsafe { &mut *FB.get() };

    let mut juego = Juego {
        mapa,
        x: 16,
        y: 16,
        entrada: 0,
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
