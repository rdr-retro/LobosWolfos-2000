//! loboswolfos - el kernel de LobosWolfos 2000 (x86_64, no_std).
//!
//! Reduccion de rocky al juego: arranca por BIOS desde un pendrive (MBR +
//! segundo tramo), levanta un modo VESA VBE con framebuffer lineal, enciende
//! la entrada PS/2 y dibuja la pantalla de prueba del motor directamente
//! sobre el LFB. No hay heap ni paginacion dinamica: el kernel de la fase 5
//! solo tiene que probar que la base del motor (video + reloj + entrada)
//! funciona sobre hierro propio.
//!
//! Arranque: `bootusb2.s` deja los primeros 4 GiB identidad-mapeados, copia
//! el kernel a 1 MiB y salta a `rust_main` (ver `start.s`).

#![no_std]
#![no_main]
// loboswolfos es una reduccion de rocky y conserva codigo que hoy no usa
// (lista de modos VBE, glifos de texto...): la fase 6 puede necesitarlo.
#![allow(dead_code)]

use core::arch::global_asm;
use core::panic::PanicInfo;

mod bios;
mod cmos;
mod console;
mod cpu;
mod gfx;
mod idt;
mod input;
mod mouse;
mod pic;
mod pit;
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

    // Modo grafico VBE con framebuffer lineal. El primer modo disponible de
    // la lista de la demo (1024x768 y hacia abajo).
    let Some((modo, _)) = gfx::find_first_demo_mode() else {
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

    let canvas = gfx::Canvas::new(&mi);
    let fuente = bios::get_font();
    if fuente.is_none() {
        println!("loboswolfos: el BIOS no ofrece la fuente ROM 8x16 (no habra texto en pantalla).");
    }

    bucle_grafico(&canvas, fuente)
}

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    console::panic_report(info)
}

/// Bucle de reserva sin graficos: eco de la entrada por la consola (que
/// sigue saliendo por el puerto serie, luego la prueba sin ventana funciona
/// igual).
fn bucle_texto() -> ! {
    println!("loboswolfos: esperando entrada (eco por la consola)...");
    loop {
        if let Some(tecla) = input::poll() {
            println!("loboswolfos: tecla 0x{:02X}.", tecla);
        }
        mouse::poll();
        unsafe { cpu::hlt() };
    }
}

/// Bucle principal de la pantalla de prueba: sondea teclado y raton, y
/// redibuja el fotograma completo despues de cada interrupcion del reloj
/// (la IRQ0 despierta el `hlt`, asi que el ritmo de fotogramas es de 100 Hz
/// como mucho).
fn bucle_grafico(canvas: &gfx::Canvas, fuente: Option<bios::Font>) -> ! {
    let mut raton_x = canvas.width / 2;
    let mut raton_y = canvas.height / 2;
    let mut botones = 0u8;
    let mut ultimo_segundo = 0u64;
    let mut ultima_tecla = 0u8;
    // Distancia recorrida desde la ultima linea del registro (para no
    // ahogar la serie con el movimiento del raton).
    let mut acumulado = 0i32;

    loop {
        // Entrada por sondeo, como rocky: sin IRQ1 ni IRQ12.
        while let Some(tecla) = input::poll() {
            ultima_tecla = tecla;
            println!("loboswolfos: tecla 0x{:02X}.", tecla);
        }
        mouse::poll();
        let (dx, dy, botones_ahora) = mouse::tomar();
        if dx != 0 || dy != 0 {
            raton_x = (raton_x + dx).clamp(0, canvas.width - 1);
            raton_y = (raton_y + dy).clamp(0, canvas.height - 1);
            acumulado += dx.abs() + dy.abs();
            if acumulado >= 48 {
                acumulado = 0;
                println!(
                    "loboswolfos: raton en ({}, {}), botones {:03b}.",
                    raton_x, raton_y, botones_ahora
                );
            }
        }
        if botones_ahora != botones {
            botones = botones_ahora;
            println!("loboswolfos: botones del raton: {:03b}.", botones);
        }

        let segundo = pit::seconds();
        if segundo != ultimo_segundo {
            ultimo_segundo = segundo;
            println!(
                "loboswolfos: reloj activo: {} tics ({} s).",
                pit::ticks(),
                segundo
            );
        }

        dibujar_fotograma(canvas, &fuente, raton_x, raton_y, botones, ultima_tecla);

        // Duerme hasta la proxima IRQ0: nada de quemar CPU en el sondeo.
        unsafe { cpu::hlt() };
    }
}

/// Color de una fila del fondo: degradado azul noche de arriba a abajo.
fn color_fondo(canvas: &gfx::Canvas, y: i32) -> u32 {
    let t = y as u32 * 255 / canvas.height as u32;
    gfx::rgb((8 + t / 8) as u8, (16 + t / 4) as u8, (48 + t / 2) as u8)
}

/// Dibuja un fotograma de la pantalla de prueba: fondo, rotulo, indicadores
/// del reloj y de la entrada, y el cursor del raton.
fn dibujar_fotograma(
    canvas: &gfx::Canvas,
    fuente: &Option<bios::Font>,
    raton_x: i32,
    raton_y: i32,
    botones: u8,
    ultima_tecla: u8,
) {
    let amarillo = gfx::rgb(255, 210, 40);
    let blanco = gfx::rgb(235, 240, 255);

    // Fondo: degradado azul noche.
    for y in 0..canvas.height {
        canvas.hline(0, canvas.width - 1, y, color_fondo(canvas, y));
    }

    // Un circulo que late con la IRQ0 (crece y se rellena cada segundo).
    let fase = (pit::ticks() % 100) as i32;
    let radio = 12 + fase / 6;
    if fase < 50 {
        canvas.disc(canvas.width / 2, canvas.height / 2 + 40, radio, gfx::rgb(90, 160, 255));
    } else {
        canvas.circle(canvas.width / 2, canvas.height / 2 + 40, radio, blanco);
    }

    if let Some(f) = fuente {
        // Rotulo y subtitulo centrados.
        let titulo = "loboswolfos";
        let x = (canvas.width - gfx::text_width(titulo, 4)) / 2;
        gfx::draw_text_shadow(canvas, f, x, 32, titulo, amarillo, 4);
        let sub = "kernel de prueba del motor (fase 5)";
        let x2 = (canvas.width - gfx::text_width(sub, 1)) / 2;
        gfx::draw_text_shadow(canvas, f, x2, 96, sub, blanco, 1);

        // Barra del reloj: se llena en un segundo y vuelve a empezar, que es
        // la prueba visual de que la IRQ0 sigue viva con el VBE activo.
        gfx::draw_text(canvas, f, 24, 130, "reloj (IRQ0):", blanco, 1);
        canvas.fill_rect(24, 150, 300, 14, gfx::rgb(20, 30, 60));
        canvas.fill_rect(24, 150, fase * 3, 14, amarillo);
        canvas.vline(24, 148, 166, blanco);
        canvas.vline(323, 148, 166, blanco);

        // Ultima tecla pulsada, en grande a la derecha.
        gfx::draw_text(canvas, f, 24, 176, "tecla:", blanco, 1);
        if ultima_tecla != 0 {
            if let Ok(glyph) = core::str::from_utf8(&[ultima_tecla]) {
                gfx::draw_text_shadow(canvas, f, 24, 194, glyph, amarillo, 3);
            }
        }

        // Botones del raton: tres circulos que se encienden al apretarlos.
        gfx::draw_text(canvas, f, 24, 250, "botones:", blanco, 1);
        for i in 0..3 {
            let color = if botones & (1 << i) != 0 {
                gfx::rgb(255, 90, 90)
            } else {
                gfx::rgb(60, 80, 130)
            };
            canvas.disc(30 + i * 28, 276, 8, color);
        }
    }

    // Cursor del raton: cruz y circulo; rojo con el boton izquierdo apretado.
    let color_cursor = if botones & 1 != 0 {
        gfx::rgb(255, 90, 90)
    } else {
        blanco
    };
    canvas.hline(raton_x - 10, raton_x + 10, raton_y, color_cursor);
    canvas.vline(raton_x, raton_y - 10, raton_y + 10, color_cursor);
    canvas.circle(raton_x, raton_y, 12, color_cursor);
}
