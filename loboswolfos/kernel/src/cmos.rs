//! Reloj de tiempo real (RTC/CMOS) y datos de memoria del BIOS.

use crate::cpu::{inb, outb};

fn read(register: u8) -> u8 {
    unsafe {
        outb(0x70, register);
        // Pequena pausa para que el CMOS procese el indice.
        for _ in 0..8 {
            let _ = inb(0x80);
        }
        inb(0x71)
    }
}

fn update_in_progress() -> bool {
    read(0x0A) & 0x80 != 0
}

fn bcd_to_bin(value: u8) -> u8 {
    (value & 0x0F) + ((value >> 4) * 10)
}

fn bcd_mode() -> bool {
    // Registro de estado B, bit 2 = 1 -> binario.
    read(0x0B) & 0x04 == 0
}

fn decode(value: u8) -> u8 {
    if bcd_mode() {
        bcd_to_bin(value)
    } else {
        value
    }
}

/// Devuelve (horas, minutos, segundos).
pub fn time() -> (u8, u8, u8) {
    while update_in_progress() {}
    let s = decode(read(0x00));
    let m = decode(read(0x02));
    let h = decode(read(0x04)) & 0x7F;
    (h, m, s)
}

/// Devuelve (dia, mes, anio) con el anio completo (p. ej. 2026).
pub fn date() -> (u8, u8, u16) {
    while update_in_progress() {}
    let day = decode(read(0x07));
    let month = decode(read(0x08));
    let year = decode(read(0x09)) as u16;
    let full_year = if year < 80 { 2000 + year as u16 } else { 1900 + year as u16 };
    (day, month, full_year)
}

/// Memoria extendida instalada, en KiB (por encima de 1 MiB).
#[allow(dead_code)] // Se conserva como consulta del CMOS; el PMM usa el mapa E820.
pub fn extended_memory_kb() -> u32 {
    let low = read(0x17) as u32 | ((read(0x18) as u32) << 8);
    if low != 0 {
        low
    } else {
        let high = read(0x34) as u32 | ((read(0x35) as u32) << 8);
        high * 64
    }
}
