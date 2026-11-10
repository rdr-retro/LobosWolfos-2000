//! Controlador de puerto serie COM1 (16550 UART).
//!
//! Se usa como consola de depuracion: con `qemu -serial stdio` todo lo que
//! escribe rocky aparece en el terminal del anfitrion.

use crate::cpu::{inb, outb};

/// Direccion base del puerto serie COM1 (los registros van en COM1+0..COM1+7).
pub const COM1: u16 = 0x3F8;

#[derive(Clone, Copy)]
pub struct Serial;

impl Serial {
    pub const fn new() -> Self {
        Serial
    }

    pub fn init(&self) {
        unsafe {
            outb(COM1 + 1, 0x00); // desactiva IRQ mientras se configura
            outb(COM1 + 3, 0x80); // habilita DLAB
            outb(COM1 + 0, 0x03); // divisor = 3 -> 38400 bps
            outb(COM1 + 1, 0x00);
            outb(COM1 + 3, 0x03); // 8 bits, sin paridad, 1 stop
            outb(COM1 + 2, 0x07); // FIFO activada, disparo a 1 byte (lineas cortas)
            outb(COM1 + 4, 0x0B); // DTR/OUT2 (OUT2 habilita la IRQ)
            // Interrupcion por dato recibido (la IRQ4 la enruta el PIC). Asi el
            // teclado del puerto serie no pierde bytes mientras la CPU esta
            // ocupada (por ejemplo, escribiendo en el disco).
            outb(COM1 + 1, 0x01);
        }
    }

    pub fn write_byte(&self, byte: u8) {
        unsafe {
            // Mientras esperamos a que el transmisor este libre (a 38400 bps,
            // cada byte tarda ~260 us), aprovechamos para vaciar lo recibido en
            // el bufer de entrada: asi el FIFO de recepcion no se desborda
            // mientras la consola esta "hablando".
            while inb(COM1 + 5) & 0x20 == 0 {
                crate::input::recoger();
            }
            outb(COM1, byte);
        }
    }

    /// Lee un byte recibido, si hay alguno disponible.
    pub fn try_read(&self) -> Option<u8> {
        unsafe {
            if inb(COM1 + 5) & 0x01 != 0 {
                Some(inb(COM1))
            } else {
                None
            }
        }
    }
}
