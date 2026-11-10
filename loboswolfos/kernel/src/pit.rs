//! Temporizador programable de intervalo 8253/8254 (canal 0 -> IRQ0).

use core::sync::atomic::{AtomicU64, Ordering};

use crate::cpu::outb;

static TICKS: AtomicU64 = AtomicU64::new(0);

/// Frecuencia de la base de tiempo del PIT.
const PIT_FREQUENCY: u32 = 1_193_182;

pub unsafe fn init(hz: u32) {
    let divisor = PIT_FREQUENCY / hz;
    outb(0x43, 0x36); // canal 0, modo 3, lobyte/hibyte
    outb(0x40, (divisor & 0xFF) as u8);
    outb(0x40, ((divisor >> 8) & 0xFF) as u8);
}

pub fn tick() {
    TICKS.fetch_add(1, Ordering::Relaxed);
}

/// Numero de tics desde el arranque (100 Hz).
pub fn ticks() -> u64 {
    TICKS.load(Ordering::Relaxed)
}

/// Segundos transcurridos desde el arranque.
pub fn seconds() -> u64 {
    ticks() / 100
}

/// Comprueba que la IRQ0 **late de verdad**, no que el contador tenga tics
/// viejos.
///
/// El escritorio se duerme con `hlt` cuando no tiene nada que hacer, y `hlt` no
/// vuelve nunca si el temporizador no interrumpe: la GUI quedaria dibujada pero
/// muerta (ni raton ni teclado). Mirar solo `ticks() > 0` no vale: el firmware
/// puede reprogramar el PIT al activar el modo de video y dejarlo en modo "un
/// solo disparo", con lo que los tics que ya habia se quedan congelados y la
/// comprobacion pasaria mientras el temporizador esta muerto.
///
/// Aqui se espera a que el contador avance cinco tics (50 ms a 100 Hz): los tics
/// que hubiera antes no cuentan, porque se mira el avance, y ningun disparo
/// suelto alcanza cinco. El cambio de segundo del RTC, visto dos veces, pone
/// tope (mas de un segundo sin tics es un PIT muerto) por si el temporizador no
/// llegara a latir; solo entonces cuesta ~1 s, y nunca mas.
///
/// Lo usaba el escritorio grafico antiguo antes de dibujar su primer fotograma;
/// hoy no tiene quien lo llame.
#[allow(dead_code)]
pub fn late() -> bool {
    let objetivo = ticks() + 5;
    // El RTC solo se consulta de vez en cuando: cada lectura son varias
    // operaciones de E/S y no hace falta mirarlo mas de una vez por milisegundo.
    let mut segundo = crate::cmos::time().2;
    let mut cambios = 0u8;
    for i in 0..2_000_000 {
        if ticks() >= objetivo {
            return true;
        }
        if i & 0x3FF == 0 {
            let ahora = crate::cmos::time().2;
            if ahora != segundo {
                cambios += 1;
                if cambios >= 2 {
                    // Un segundo entero (o mas) sin ni siquiera cinco tics.
                    return false;
                }
                segundo = ahora;
            }
        }
        unsafe {
            let _ = crate::cpu::inb(0x80);
        }
    }
    false
}
