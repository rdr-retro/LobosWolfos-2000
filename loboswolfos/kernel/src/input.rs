//! Entrada de teclado: sondea el controlador PS/2 y, ademas, el puerto serie
//! (util para probar rocky de forma automatica con `qemu -serial stdio`).

use core::sync::atomic::{AtomicBool, AtomicU32, Ordering};

use crate::cpu::inb;
use crate::motor::{
    MOTOR_ENT_ABAJO, MOTOR_ENT_ARRIBA, MOTOR_ENT_CERRAR, MOTOR_ENT_DERECHA, MOTOR_ENT_IZQUIERDA,
    MOTOR_ENT_SALTO,
};
use crate::serial::Serial;
use crate::sync::SyncCell;

static SHIFT: AtomicBool = AtomicBool::new(false);
static EXTENDED: AtomicBool = AtomicBool::new(false);

/* ------------------------------------------------------------------ */
/* Entrada extendida (fase 37)                                          */
/* ------------------------------------------------------------------ */

/// Ranuras de las 32 teclas, en el mismo orden que `motor/entrada.h`.
pub const ENT_IZQUIERDA: i32 = 0;
pub const ENT_DERECHA: i32 = 1;
pub const ENT_ARRIBA: i32 = 2;
pub const ENT_ABAJO: i32 = 3;
pub const ENT_ESPACIO: i32 = 4;
pub const ENT_ENTER: i32 = 5;
pub const ENT_A: i32 = 6;
pub const MOTOR_TECLAS_MAX: i32 = 32;

static TECLAS_EXT: AtomicU32 = AtomicU32::new(0);
static ANTERIOR_EXT: AtomicU32 = AtomicU32::new(0);

/// Ranura asociada a un bit legacy MOTOR_ENT_*, o -1 si no tiene.
fn slot_legacy(bit: u32) -> i32 {
    match bit {
        MOTOR_ENT_IZQUIERDA => ENT_IZQUIERDA,
        MOTOR_ENT_DERECHA => ENT_DERECHA,
        MOTOR_ENT_ARRIBA => ENT_ARRIBA,
        MOTOR_ENT_ABAJO => ENT_ABAJO,
        MOTOR_ENT_SALTO => ENT_ESPACIO,
        _ => -1,
    }
}

/// Cambia una ranura de la entrada extendida.
pub fn poner_slot(slot: i32, pulsado: bool) {
    if slot < 0 || slot >= MOTOR_TECLAS_MAX {
        return;
    }
    let bit = 1u32 << (slot as u32);
    if pulsado {
        TECLAS_EXT.fetch_or(bit, Ordering::Relaxed);
    } else {
        TECLAS_EXT.fetch_and(!bit, Ordering::Relaxed);
    }
}

/// Mascara actual de teclas sostenidas.
pub fn entrada_ext_actual() -> u32 {
    TECLAS_EXT.load(Ordering::Relaxed)
}

/// Mascara del fotograma anterior (para calcular flancos).
pub fn entrada_ext_anterior() -> u32 {
    ANTERIOR_EXT.load(Ordering::Relaxed)
}

/// Cierra un fotograma: la mascara actual pasa a ser la anterior.
pub fn siguiente_ext() {
    ANTERIOR_EXT.store(entrada_ext_actual(), Ordering::Relaxed);
}

fn bit_slot(slot: i32) -> u32 {
    if slot < 0 || slot >= MOTOR_TECLAS_MAX {
        0
    } else {
        1u32 << (slot as u32)
    }
}

pub fn entrada_ext_mantenida(slot: i32) -> bool {
    let b = bit_slot(slot);
    b != 0 && entrada_ext_actual() & b != 0
}

pub fn entrada_ext_pulsada(slot: i32) -> bool {
    let b = bit_slot(slot);
    b != 0 && entrada_ext_actual() & b != 0 && entrada_ext_anterior() & b == 0
}

pub fn entrada_ext_soltada(slot: i32) -> bool {
    let b = bit_slot(slot);
    b != 0 && entrada_ext_actual() & b == 0 && entrada_ext_anterior() & b != 0
}

/// Ranura de una letra a..z (o A..Z).
pub fn slot_letra(ch: u8) -> i32 {
    let minus = ch.to_ascii_lowercase();
    if minus.is_ascii_lowercase() {
        ENT_A + (minus - b'a') as i32
    } else {
        -1
    }
}

/// Resuelve el nombre entre comillas de una nativa de la fase 37.
pub fn slot_nombre(nombre: &[u8]) -> i32 {
    match nombre {
        b"izquierda" => ENT_IZQUIERDA,
        b"derecha" => ENT_DERECHA,
        b"arriba" => ENT_ARRIBA,
        b"abajo" => ENT_ABAJO,
        b"espacio" | b"salto" => ENT_ESPACIO,
        b"enter" | b"intro" => ENT_ENTER,
        [ch] => slot_letra(*ch),
        _ => -1,
    }
}

/* ------------------------------------------------------------------ */
/* Entrada del motor (fase 6)                                           */
/* ------------------------------------------------------------------ */

/// Teclas del juego mantenidas ahora mismo (bits MOTOR_ENT_*): las
/// flechas del bloque E0, WASD, espacio y Escape.
static ENTRADA: AtomicU32 = AtomicU32::new(0);

/// Teclas del juego pulsadas desde la ultima lectura (bits MOTOR_ENT_*).
///
/// Un toque rapido puede soltarse entero entre dos sondeos (QEMU manda
/// la pulsacion y la suelta del tiron): las pulsaciones se recuerdan
/// aqui para que cuenten al menos como un fotograma de entrada, igual
/// que una tecla mantenida cuenta todo el tiempo que este apretada.
static PULSOS: AtomicU32 = AtomicU32::new(0);

fn apretar(bit: u32) {
    ENTRADA.fetch_or(bit, Ordering::Relaxed);
    PULSOS.fetch_or(bit, Ordering::Relaxed);
    let slot = slot_legacy(bit);
    if slot >= 0 {
        poner_slot(slot, true);
    }
}

fn soltar(bit: u32) {
    ENTRADA.fetch_and(!bit, Ordering::Relaxed);
    let slot = slot_legacy(bit);
    if slot >= 0 {
        poner_slot(slot, false);
    }
}

/// Bits MOTOR_ENT_* de las teclas del juego mantenidas ahora mismo.
pub fn entrada() -> u32 {
    ENTRADA.load(Ordering::Relaxed)
}

/// Bits MOTOR_ENT_* de las teclas pulsadas desde la ultima llamada. Se
/// borra al leer (cada toque cuenta un fotograma).
pub fn tomar_pulsos() -> u32 {
    PULSOS.swap(0, Ordering::Relaxed)
}

/// Bufer circular que llena la IRQ4 del puerto serie.
const RING_SIZE: usize = 512;

struct Ring {
    buf: [u8; RING_SIZE],
    head: usize,
    tail: usize,
}

impl Ring {
    const fn new() -> Self {
        Ring {
            buf: [0; RING_SIZE],
            head: 0,
            tail: 0,
        }
    }

    fn push(&mut self, byte: u8) {
        let next = (self.head + 1) % RING_SIZE;
        if next != self.tail {
            self.buf[self.head] = byte;
            self.head = next;
        }
    }

    fn pop(&mut self) -> Option<u8> {
        if self.head == self.tail {
            None
        } else {
            let byte = self.buf[self.tail];
            self.tail = (self.tail + 1) % RING_SIZE;
            Some(byte)
        }
    }
}

static RING: SyncCell<Ring> = SyncCell::new(Ring::new());

/// Manejador de la IRQ4: vacia el UART en el bufer circular.
pub fn on_serial_irq() {
    recoger();
}

/// Vacia el UART en el bufer circular. Se llama desde la IRQ4 y tambien, como
/// respaldo, desde el sondeo del teclado. Desactiva las interrupciones mientras
/// toca el bufer para no corromperlo si la IRQ llega a mitad.
pub fn recoger() {
    unsafe {
        crate::cpu::sin_interrupciones(|| {
            let serial = Serial::new();
            let ring = &mut *RING.get();
            while let Some(byte) = serial.try_read() {
                ring.push(match byte {
                    b'\r' => b'\n',
                    other => other,
                });
            }
        });
    }
}

/// Letra del scancode set 1 (sin el bit de suelta), si es una letra.
fn letra_scancode(make: u8) -> Option<u8> {
    Some(match make {
        0x1E => b'a',
        0x30 => b'b',
        0x2E => b'c',
        0x20 => b'd',
        0x12 => b'e',
        0x21 => b'f',
        0x22 => b'g',
        0x23 => b'h',
        0x17 => b'i',
        0x24 => b'j',
        0x25 => b'k',
        0x26 => b'l',
        0x32 => b'm',
        0x31 => b'n',
        0x18 => b'o',
        0x19 => b'p',
        0x10 => b'q',
        0x13 => b'r',
        0x1F => b's',
        0x14 => b't',
        0x16 => b'u',
        0x2F => b'v',
        0x11 => b'w',
        0x2D => b'x',
        0x15 => b'y',
        0x2C => b'z',
        _ => return None,
    })
}

/// Traduce un codigo de barrido (set 1) a ASCII.
fn translate(scancode: u8) -> Option<u8> {
    // Bytes que no son pulsaciones: ACK (0xFA), autotest correcto (0xAA), eco
    // (0xEE), reintento (0xFE), desbordamiento/ultimo byte invalido (0x00/0xFF).
    // Si se colasen acabarian en pantalla como caracteres sueltos.
    if matches!(scancode, 0x00 | 0xAA | 0xEE | 0xFA | 0xFE | 0xFF) {
        return None;
    }
    if scancode == 0xE0 {
        EXTENDED.store(true, Ordering::Relaxed);
        return None;
    }
    if scancode == 0xE1 {
        // Prefijo de la tecla Pausa (cadena larga): se ignora igual.
        EXTENDED.store(true, Ordering::Relaxed);
        return None;
    }
    if EXTENDED.swap(false, Ordering::Relaxed) {
        // Teclas del bloque extendido (E0): aqui van las flechas, que son
        // la entrada del juego (make 0x4x, break 0xCx).
        match scancode {
            0x4B => apretar(MOTOR_ENT_IZQUIERDA),
            0x4D => apretar(MOTOR_ENT_DERECHA),
            0x48 => apretar(MOTOR_ENT_ARRIBA),
            0x50 => apretar(MOTOR_ENT_ABAJO),
            0xCB => soltar(MOTOR_ENT_IZQUIERDA),
            0xCD => soltar(MOTOR_ENT_DERECHA),
            0xC8 => soltar(MOTOR_ENT_ARRIBA),
            0xD0 => soltar(MOTOR_ENT_ABAJO),
            _ => {}
        }
        return None; // el resto de teclas extendidas se ignora
    }

    // Teclas del juego sin prefijo: WASD, espacio y Escape. El estado se
    // mantiene aparte (para el motor) sin tocar la traduccion a ASCII de
    // abajo (que sigue sirviendo para la consola).
    match scancode {
        0x11 => apretar(MOTOR_ENT_ARRIBA),       // W
        0x91 => soltar(MOTOR_ENT_ARRIBA),
        0x1E => apretar(MOTOR_ENT_IZQUIERDA),    // A
        0x9E => soltar(MOTOR_ENT_IZQUIERDA),
        0x1F => apretar(MOTOR_ENT_ABAJO),        // S
        0x9F => soltar(MOTOR_ENT_ABAJO),
        0x20 => apretar(MOTOR_ENT_DERECHA),     // D
        0xA0 => soltar(MOTOR_ENT_DERECHA),
        0x39 => apretar(MOTOR_ENT_SALTO),        // espacio
        0xB9 => soltar(MOTOR_ENT_SALTO),
        0x01 => apretar(MOTOR_ENT_CERRAR),       // Escape
        0x81 => soltar(MOTOR_ENT_CERRAR),
        _ => {}
    }

    /* Fase 37: toda letra tiene ranura propia, no solo WASD. El bit 7 del
     * scancode indica que se suelta; las flechas ya las actualizo `apretar`. */
    if let Some(letra) = letra_scancode(scancode & 0x7F) {
        poner_slot(slot_letra(letra), scancode & 0x80 == 0);
    }
    if scancode == 0x1C || scancode == 0x9C {
        poner_slot(ENT_ENTER, scancode == 0x1C);
    }

    let shift = SHIFT.load(Ordering::Relaxed);
    match scancode {
        0x2A | 0x36 => {
            SHIFT.store(true, Ordering::Relaxed);
            None
        }
        0xAA | 0xB6 => {
            SHIFT.store(false, Ordering::Relaxed);
            None
        }
        0x1C => Some(b'\n'),
        0x0E => Some(0x08),
        0x39 => Some(b' '),
        0x0F => Some(b'\t'),
        0x01 => Some(0x1B), // ESC
        _ => {
            let (normal, shifted) = base_char(scancode)?;
            if shift {
                Some(shifted)
            } else {
                Some(normal)
            }
        }
    }
}

fn base_char(scancode: u8) -> Option<(u8, u8)> {
    Some(match scancode {
        0x02 => (b'1', b'!'),
        0x03 => (b'2', b'@'),
        0x04 => (b'3', b'#'),
        0x05 => (b'4', b'$'),
        0x06 => (b'5', b'%'),
        0x07 => (b'6', b'^'),
        0x08 => (b'7', b'&'),
        0x09 => (b'8', b'*'),
        0x0A => (b'9', b'('),
        0x0B => (b'0', b')'),
        0x0C => (b'-', b'_'),
        0x0D => (b'=', b'+'),
        0x10 => (b'q', b'Q'),
        0x11 => (b'w', b'W'),
        0x12 => (b'e', b'E'),
        0x13 => (b'r', b'R'),
        0x14 => (b't', b'T'),
        0x15 => (b'y', b'Y'),
        0x16 => (b'u', b'U'),
        0x17 => (b'i', b'I'),
        0x18 => (b'o', b'O'),
        0x19 => (b'p', b'P'),
        0x1A => (b'[', b'{'),
        0x1B => (b']', b'}'),
        0x1E => (b'a', b'A'),
        0x1F => (b's', b'S'),
        0x20 => (b'd', b'D'),
        0x21 => (b'f', b'F'),
        0x22 => (b'g', b'G'),
        0x23 => (b'h', b'H'),
        0x24 => (b'j', b'J'),
        0x25 => (b'k', b'K'),
        0x26 => (b'l', b'L'),
        0x27 => (b';', b':'),
        0x28 => (b'\'', b'"'),
        0x29 => (b'`', b'~'),
        0x2B => (b'\\', b'|'),
        0x2C => (b'z', b'Z'),
        0x2D => (b'x', b'X'),
        0x2E => (b'c', b'C'),
        0x2F => (b'v', b'V'),
        0x30 => (b'b', b'B'),
        0x31 => (b'n', b'N'),
        0x32 => (b'm', b'M'),
        0x33 => (b',', b'<'),
        0x34 => (b'.', b'>'),
        0x35 => (b'/', b'?'),
        _ => return None,
    })
}

fn keyboard_key() -> Option<u8> {
    unsafe {
        let estado = inb(0x64);
        if estado & 0x01 == 0 {
            return None;
        }
        let byte = inb(0x60);
    // Los bits 6 y 7 del puerto de estado avisan de un byte con error de
    // temporizacion o de paridad: se descarta.
    if estado & 0xC0 != 0 {
        return None;
    }
        // El bit 5 distingue el raton del teclado: si el byte viene del puerto
        // auxiliar se lo pasamos al driver del raton y no es una pulsacion.
        if estado & 0x20 != 0 {
            crate::mouse::alimentar(byte);
            return None;
        }
        translate(byte)
    }
}

fn serial_key() -> Option<u8> {
    // El bufer circular lo llena la IRQ4; ademas, por si el pin del teclado se
    // quedo sin atender, vaciamos el UART aqui antes de sacar nada.
    recoger();
    let ring = unsafe { &mut *RING.get() };
    ring.pop()
}

/// Devuelve la siguiente pulsacion disponible, si la hay.
pub fn poll() -> Option<u8> {
    keyboard_key().or_else(serial_key)
}

/// Espera hasta que haya una tecla, sondeando en bucle (estilo DOS).
pub fn wait() -> u8 {
    loop {
        if let Some(key) = poll() {
            return key;
        }
        core::hint::spin_loop();
    }
}
