//! Controlador de interrupciones programable 8259A (PIC).
//!
//! Se remapea para que las IRQ lleguen a los vectores 0x20..0x2F.

use crate::cpu::{inb, io_wait, outb};

pub unsafe fn init() {
    // ICW1: inicializacion
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();
    // ICW2: desplazamiento de vectores
    outb(0x21, 0x20);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();
    // ICW3: conexion maestro/esclavo
    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();
    // ICW4: modo 8086
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();
    // Mascaras: IRQ0 (temporizador) e IRQ4 (puerto serie) desenmascaradas.
    outb(0x21, 0xEE);
    outb(0xA1, 0xFF);

    // Consume cualquier estado pendiente del PIC.
    let _ = inb(0x21);
}

pub unsafe fn eoi(irq: u8) {
    if irq >= 8 {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}
