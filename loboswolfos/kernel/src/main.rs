//! loboswolfos - el kernel de LobosWolfos 2000 (x86_64, no_std).
//!
//! Reduccion de rocky al juego: arranca por BIOS desde un pendrive (MBR +
//! segundo tramo), levanta un modo VESA VBE con framebuffer lineal, enciende
//! la entrada PS/2 y corre el juego del gestor de logica sobre el nucleo del
//! motor en ensamblador (fase 6). No hay heap ni paginacion dinamica: los
//! activos del juego viven en la "zona del juego" del disco (blob LGB1) y se
//! cargan a RAM a base de direcciones fijas (ver activo.rs).
//!
//! Arranque: `bootusb2.s` deja los primeros 4 GiB identidad-mapeados, copia
//! el kernel a 1 MiB y salta a `rust_main` (ver `start.s`).

#![no_std]
#![no_main]
// loboswolfos es una reduccion de rocky y conserva codigo que hoy no usa
// (lista de modos VBE, glifos de texto...): las fases siguientes pueden
// necesitarlo.
#![allow(dead_code)]

use core::arch::global_asm;
use core::panic::PanicInfo;

mod activo;
mod bios;
mod cmos;
mod console;
mod cpu;
mod gfx;
mod idt;
mod inflate;
mod input;
mod juego;
mod mouse;
mod motor;
mod pic;
mod pit;
mod png;
mod ps2;
mod serial;
mod sync;
mod vga;

global_asm!(include_str!("start.s"));
global_asm!(include_str!("interrupts.s"));

#[no_mangle]
pub extern "C" fn rust_main() -> ! {
    console::init();
    println!("loboswolfos: cargando kernel (x86_64, modo largo)...");

    let marca = cpu::marca();
    let fin = marca.iter().position(|&b| b == 0).unwrap_or(marca.len());
    if let Ok(nombre) = core::str::from_utf8(&marca[..fin]) {
        if !nombre.is_empty() {
            println!("loboswolfos: CPU: {}.", nombre.trim_end());
        }
    }

    idt::init();

    // Los servicios del BIOS (VESA...) se piden ANTES de tocar el PIC ni el
    // PIT. En cuanto el kernel reubica el PIC en 0x20 y reprograma el
    // temporizador, un manejador del firmware que reactive las
    // interrupciones saltaria a la entrada de la IVT equivocada, y en un PC
    // real eso cuelga la maquina.
    println!("loboswolfos: [1/3] trampolin del BIOS...");
    unsafe { bios::init() };

    println!("loboswolfos: [2/3] temporizador e interrupciones...");
    unsafe {
        pic::init();
        pit::init(100);
        cpu::enable_interrupts();
    }
    // A partir de aqui el PIC y el PIT son del kernel: cada llamada al BIOS
    // los volvera a dejar como estan (el firmware los reprograma, sobre todo
    // al activar el modo VESA, y sin esto la IRQ0 puede dejar de llegar).
    bios::tomar_interrupciones();

    println!("loboswolfos: [3/3] entrada PS/2...");
    let informe = ps2::init();
    informe.imprimir();

    // Modo grafico VBE con framebuffer lineal. El juego corre en una
    // pantalla de 640x480 (la misma ventana que game.exe), asi que se
    // prefiere el modo justo; si el BIOS no lo tiene, el primero de la
    // lista que quepa (y el juego se dibuja centrado en el).
    let Some((modo, _)) = gfx::find_mode(640, 480).or_else(gfx::find_first_demo_mode) else {
        println!("loboswolfos: sin modo VBE de 32 bpp; el kernel se queda en modo texto.");
        println!("loboswolfos: (en QEMU hace falta la VGA estandar: -vga std).");
        bucle_texto()
    };
    let Some(mi) = gfx::set_mode(modo) else {
        println!("loboswolfos: el BIOS no activo el modo 0x{:04X}.", modo);
        bucle_texto()
    };
    println!(
        "loboswolfos: VBE activo: modo 0x{:04X}, {}x{} a {} bpp, LFB 0x{:08X}.",
        modo, mi.width, mi.height, mi.bpp, mi.lfb
    );

    // El firmware puede dejar el PIT en un solo disparo al cambiar de modo
    // de video: si el reloj no late, se rearma y se comprueba otra vez.
    if bios::asegurar_reloj_kernel() {
        println!("loboswolfos: reloj latiendo tras el cambio de modo.");
    } else {
        println!("loboswolfos: AVISO: la IRQ0 no llega tras el cambio de modo.");
    }

    // Fase 6: el juego del gestor de logica sobre el nucleo del motor.
    juego::arrancar(&mi)
}

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    console::panic_report(info)
}

/// Bucle de reserva sin graficos: eco de la entrada por la consola (que
/// sigue saliendo por el puerto serie, luego la prueba sin ventana funciona
/// igual). Lo usan los arranques sin modo VBE y el juego sin proyecto.
pub(crate) fn bucle_texto() -> ! {
    println!("loboswolfos: esperando entrada (eco por la consola)...");
    loop {
        if let Some(tecla) = input::poll() {
            println!("loboswolfos: tecla 0x{:02X}.", tecla);
        }
        mouse::poll();
        unsafe { cpu::hlt() };
    }
}
