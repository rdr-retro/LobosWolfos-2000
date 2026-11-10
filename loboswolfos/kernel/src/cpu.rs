//! Utilidades de CPU: E/S de puertos, interrupciones y parada.

use core::arch::asm;

#[inline(always)]
pub unsafe fn outb(port: u16, value: u8) {
    asm!("out dx, al", in("dx") port, in("al") value,
         options(nomem, nostack, preserves_flags));
}

#[inline(always)]
pub unsafe fn inb(port: u16) -> u8 {
    let value: u8;
    asm!("in al, dx", out("al") value, in("dx") port,
         options(nomem, nostack, preserves_flags));
    value
}

#[inline(always)]
pub unsafe fn outw(port: u16, value: u16) {
    asm!("out dx, ax", in("dx") port, in("ax") value,
         options(nomem, nostack, preserves_flags));
}

#[inline(always)]
pub unsafe fn inw(port: u16) -> u16 {
    let value: u16;
    asm!("in ax, dx", out("ax") value, in("dx") port,
         options(nomem, nostack, preserves_flags));
    value
}

/// Pequena pausa de E/S (usada con el PIC).
#[inline(always)]
pub unsafe fn io_wait() {
    outb(0x80, 0);
}

/// Ejecuta `CPUID` con `leaf`/`sub` y devuelve `(EAX, EBX, ECX, EDX)`.
///
/// Solo se usa para identificar el procesador (ver [`fabricante`] y [`marca`]).
#[inline(always)]
pub unsafe fn cpuid(leaf: u32, sub: u32) -> (u32, u32, u32, u32) {
    let (a, b, c, d);
    // `rbx` lo reserva LLVM (no admite restricciones sobre el), asi que se
    // intercambia con un registro libre antes y despues de `CPUID`.
    //
    // El temporal es `inout` a proposito: al ser una entrada ademas de salida, el
    // asignador le da un registro propio y no puede reutilizar `eax` ni `ecx`
    // (que llevan la hoja), cosa que si podria pasar con un `out` que se escribe
    // antes de leer las entradas.
    let mut _salvo: u64 = 0;
    asm!(
        "xchg {salvo}, rbx",
        "cpuid",
        "mov {b:e}, ebx",
        "xchg {salvo}, rbx",
        salvo = inout(reg) _salvo,
        inlateout("eax") leaf => a,
        b = lateout(reg) b,
        inlateout("ecx") sub => c,
        out("edx") d,
        options(nomem, nostack, preserves_flags),
    );
    (a, b, c, d)
}

/// Fabricante de la CPU: 12 bytes ASCII, p. ej. `GenuineIntel`.
pub fn fabricante() -> [u8; 12] {
    let (_, b, c, d) = unsafe { cpuid(0, 0) };
    let mut out = [0u8; 12];
    // El fabricante va en EBX, EDX, ECX (en ese orden).
    out[0..4].copy_from_slice(&b.to_le_bytes());
    out[4..8].copy_from_slice(&d.to_le_bytes());
    out[8..12].copy_from_slice(&c.to_le_bytes());
    out
}

/// Cadena de marca del procesador (48 bytes, rellena con espacios o nulos).
///
/// Vacia si la CPU no la soporta; entonces conviene usar [`fabricante`].
pub fn marca() -> [u8; 48] {
    let mut out = [0u8; 48];
    let max = unsafe { cpuid(0x8000_0000, 0) }.0;
    if max < 0x8000_0004 {
        return out;
    }
    for (i, leaf) in [0x8000_0002u32, 0x8000_0003, 0x8000_0004].iter().enumerate() {
        let (a, b, c, d) = unsafe { cpuid(*leaf, 0) };
        let base = i * 16;
        for (j, w) in [a, b, c, d].iter().enumerate() {
            out[base + j * 4..base + j * 4 + 4].copy_from_slice(&w.to_le_bytes());
        }
    }
    out
}

#[inline(always)]
pub unsafe fn enable_interrupts() {
    asm!("sti", options(nomem, nostack));
}

/// Ejecuta `f` con las interrupciones desactivadas y restaura el estado previo.
///
/// Sirve para tocar estructuras compartidas con los manejadores de IRQ (por
/// ejemplo, el bufer circular del puerto serie) sin que una interrupcion se
/// cuele a mitad de la operacion.
#[inline(always)]
pub unsafe fn sin_interrupciones<F: FnOnce() -> R, R>(f: F) -> R {
    let flags: u64;
    // Guarda RFLAGS en un registro (sin tocar la pila de forma asimetrica) y
    // desactiva las interrupciones.
    asm!("pushfq; pop {}", out(reg) flags, options(nomem));
    asm!("cli", options(nomem, nostack));
    let resultado = f();
    if flags & (1 << 9) != 0 {
        asm!("sti", options(nomem, nostack));
    }
    resultado
}

#[inline(always)]
pub unsafe fn hlt() {
    asm!("hlt", options(nomem, nostack));
}

/// Bucle de parada definitiva (tras un fallo irrecuperable).
pub fn halt_loop() -> ! {
    loop {
        unsafe { hlt() };
    }
}

/// Apaga la maquina virtual (ACPI y el dispositivo de depuracion de QEMU).
pub fn shutdown() -> ! {
    unsafe {
        outw(0x604, 0x2000); // ACPI PM1a_CNT (QEMU)
        outw(0xB004, 0x2000); // ACPI antiguo (Bochs/QEMU)
        outb(0xF4, 0x10); // puerto de pruebas isa-debug-exit (por si acaso)
    }
    halt_loop()
}
