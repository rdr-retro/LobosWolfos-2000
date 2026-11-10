//! Entrada de teclado: sondea el controlador PS/2 y, ademas, el puerto serie
//! (util para probar rocky de forma automatica con `qemu -serial stdio`).

use core::sync::atomic::{AtomicBool, Ordering};

use crate::cpu::inb;
use crate::serial::Serial;
use crate::sync::SyncCell;

static SHIFT: AtomicBool = AtomicBool::new(false);
static EXTENDED: AtomicBool = AtomicBool::new(false);

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
        return None; // teclas extendidas (flechas, etc.) ignoradas
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
