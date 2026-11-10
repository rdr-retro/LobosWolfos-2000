//! Segundo tramo del arranque del pendrive USB (BIOS, 16 bits).
//!
//! Lo carga el sector de arranque en 0x8000 y desde ahi carga el kernel con
//! `INT 13h AH=42h` antes de pasar a modo largo. Todo el codigo real vive en
//! `bootusb2.s`, incluido con `global_asm!`.

#![no_std]
#![no_main]

use core::arch::global_asm;
use core::panic::PanicInfo;

global_asm!(include_str!("bootusb2.s"));

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {
        core::hint::spin_loop();
    }
}
