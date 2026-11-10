//! Consola del kernel: escribe a la vez en la VGA de texto y en el puerto serie.
//!
//! Copiada de rocky y reducida: loboswolfos no tiene escritorio grafico, asi
//! que no hace falta la terminal virtual (`term`) ni guardar y restaurar el
//! texto al cambiar de modo. Con el modo VBE activo la VGA de texto ya no se
//! ve, pero el puerto serie sigue registrando todo (en QEMU, `-serial`).

use core::fmt::{self, Write};

use crate::serial::Serial;
use crate::sync::SyncCell;
use crate::vga::Vga;

pub struct Console {
    vga: Vga,
    serial: Serial,
}

impl Console {
    const fn new() -> Self {
        Console {
            vga: Vga::new(),
            serial: Serial::new(),
        }
    }

    pub fn clear(&mut self) {
        self.vga.clear();
    }

    pub fn put_byte(&mut self, byte: u8) {
        // La salida por serie se mantiene siempre (depuracion y pruebas).
        self.serial.write_byte(byte);
        self.vga.put_byte(byte);
    }
}

impl Write for Console {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        for byte in s.bytes() {
            if byte == b'\n' {
                self.put_byte(b'\r');
            }
            self.put_byte(byte);
        }
        Ok(())
    }
}

static CONSOLE: SyncCell<Console> = SyncCell::new(Console::new());

fn console() -> &'static mut Console {
    // Seguro: loboswolfos es mononucleo y monotarea; nunca hay dos
    // referencias vivas simultaneas porque todas las funciones de
    // impresion son de corta duracion.
    unsafe { &mut *CONSOLE.get() }
}

pub fn init() {
    let c = console();
    c.serial.init();
    c.clear();
}

pub fn clear() {
    console().clear();
}

pub fn put(byte: u8) {
    console().put_byte(byte);
}

pub fn write_bytes(bytes: &[u8]) {
    for &b in bytes {
        put(b);
    }
}

pub fn _print(args: fmt::Arguments) {
    let _ = console().write_fmt(args);
}

#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => ($crate::console::_print(format_args!($($arg)*)));
}

#[macro_export]
macro_rules! println {
    () => ($crate::print!("\n"));
    ($($arg:tt)*) => ($crate::print!("{}\n", format_args!($($arg)*)));
}

/// Imprime un fallo irrecuperable del kernel.
pub fn panic_report(info: &core::panic::PanicInfo) -> ! {
    println!();
    println!("*** loboswolfos: PANICO DEL KERNEL ***");
    if let Some(loc) = info.location() {
        println!("  en {}:{}", loc.file(), loc.line());
    }
    println!("  {}", info.message());
    crate::cpu::halt_loop()
}
