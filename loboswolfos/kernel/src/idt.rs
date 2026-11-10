//! Tabla de descriptores de interrupciones (IDT) y gestion de excepciones/IRQ.

use core::arch::asm;

use crate::pic;
use crate::pit;
use crate::println;
use crate::sync::SyncCell;

/// Marco de pila que los stubs de `interrupts.s` construyen antes de llamar
/// a `isr_dispatch`. El orden debe coincidir con el de los `push`/`pop`.
#[repr(C)]
pub struct InterruptStack {
    pub r15: u64,
    pub r14: u64,
    pub r13: u64,
    pub r12: u64,
    pub r11: u64,
    pub r10: u64,
    pub r9: u64,
    pub r8: u64,
    pub rdi: u64,
    pub rsi: u64,
    pub rbp: u64,
    pub rbx: u64,
    pub rdx: u64,
    pub rcx: u64,
    pub rax: u64,
    pub vector: u64,
    pub error: u64,
    pub rip: u64,
    pub cs: u64,
    pub rflags: u64,
    pub rsp: u64,
    pub ss: u64,
}

#[repr(C, packed)]
#[derive(Clone, Copy)]
struct IdtEntry {
    offset_low: u16,
    selector: u16,
    ist: u8,
    type_attr: u8,
    offset_mid: u16,
    offset_high: u32,
    zero: u32,
}

impl IdtEntry {
    const fn missing() -> Self {
        IdtEntry {
            offset_low: 0,
            selector: 0,
            ist: 0,
            type_attr: 0,
            offset_mid: 0,
            offset_high: 0,
            zero: 0,
        }
    }

    fn new(handler: u64) -> Self {
        IdtEntry {
            offset_low: handler as u16,
            selector: 0x18,
            ist: 0,
            type_attr: 0x8E, // presente, anillo 0, puerta de interrupcion de 64 bits
            offset_mid: (handler >> 16) as u16,
            offset_high: (handler >> 32) as u32,
            zero: 0,
        }
    }
}

#[repr(C, packed)]
struct IdtDescriptor {
    limit: u16,
    base: u64,
}

static IDT: SyncCell<[IdtEntry; 256]> = SyncCell::new([IdtEntry::missing(); 256]);

extern "C" {
    /// Direcciones de los stubs 0..47 (tabla definida en `interrupts.s`).
    static isr_vectors: [unsafe extern "C" fn(); 48];
    /// Stub generico para cualquier vector sin uno propio.
    fn isr_espurio();
}

pub fn init() {
    unsafe {
        let idt = &mut *IDT.get();
        // Todas las puertas tienen manejador: ninguna excepcion puede quedarse
        // sin puerta (eso acababa en #DF y ocultaba el fallo de verdad).
        for (vector, handler) in isr_vectors.iter().enumerate() {
            idt[vector] = IdtEntry::new(*handler as u64);
        }
        let generico = isr_espurio as *const () as u64;
        for vector in isr_vectors.len()..256 {
            idt[vector] = IdtEntry::new(generico);
        }

        let descriptor = IdtDescriptor {
            limit: (core::mem::size_of::<[IdtEntry; 256]>() - 1) as u16,
            base: idt.as_ptr() as u64,
        };

        asm!("lidt [{}]", in(reg) &descriptor, options(readonly, nostack));
    }
}

/// Vectores que empujan codigo de error.
fn empuja_codigo_de_error(vector: u64) -> bool {
    matches!(vector, 8 | 10 | 11 | 12 | 13 | 14 | 17 | 21 | 29 | 30)
}

/// Si un vector con codigo de error lo dispara algo que no empuja codigo (una
/// IRQ, por ejemplo) el marco queda desplazado 8 bytes: lo que esperamos en
/// `error` es el RIP real. Se detecta porque el CS de verdad tiene que ser el
/// selector de codigo del kernel (0x18) y lo recolocamos.
fn normalizar_marco(f: &mut InterruptStack) {
    if !empuja_codigo_de_error(f.vector) || f.cs == 0x18 || f.rip != 0x18 {
        return;
    }
    f.ss = f.rsp;
    f.rsp = f.rflags;
    f.rflags = f.cs;
    f.cs = f.rip;
    f.rip = f.error;
    f.error = 0;
}

fn vector_name(vector: u64) -> &'static str {
    match vector {
        0 => "division por cero (#DE)",
        1 => "depuracion (#DB)",
        2 => "interrupcion no enmascarable (NMI)",
        3 => "punto de ruptura (#BP)",
        4 => "desbordamiento (#OF)",
        5 => "limite de rango (#BR)",
        6 => "codigo invalido (#UD)",
        7 => "dispositivo no disponible (#NM)",
        8 => "doble fallo (#DF)",
        9 => "excepcion obsoleta",
        10 => "TSS invalido (#TS)",
        11 => "segmento ausente (#NP)",
        12 => "falta de pila (#SS)",
        13 => "fallo general de proteccion (#GP)",
        14 => "fallo de pagina (#PF)",
        15 => "vector reservado",
        16 => "error de coma flotante (#MF)",
        17 => "comprobacion de alineacion (#AC)",
        18 => "comprobacion de maquina (#MC)",
        19 => "error SIMD (#XM)",
        20 => "excepcion de virtualizacion (#VE)",
        21 => "proteccion de control (#CP)",
        29 => "excepcion de comunicacion (#VC)",
        30 => "excepcion de seguridad (#SX)",
        32 => "IRQ0: temporizador",
        33 => "IRQ1: teclado",
        34 => "IRQ2: cascada",
        35 => "IRQ3: COM2",
        36 => "IRQ4: COM1",
        37 => "IRQ5",
        38 => "IRQ6",
        39 => "IRQ7",
        40 => "IRQ8: reloj de tiempo real",
        41 => "IRQ9",
        42 => "IRQ10",
        43 => "IRQ11",
        44 => "IRQ12: raton",
        45 => "IRQ13: coprocesador",
        46 => "IRQ14: disco",
        47 => "IRQ15",
        0x1000 => "vector sin manejador (48..255)",
        0x1001 => "numero de vector invalido",
        _ => "excepcion desconocida",
    }
}

#[no_mangle]
pub extern "C" fn isr_dispatch(stack: *mut InterruptStack) {
    let frame = unsafe { &mut *stack };
    normalizar_marco(frame);
    match frame.vector {
        32 => {
            pit::tick();
            unsafe { pic::eoi(0) };
        }
        33 => {
            unsafe { pic::eoi(1) };
        }
        36 => {
            crate::input::on_serial_irq();
            unsafe { pic::eoi(4) };
        }
        // Cualquier otra IRQ (32..47, PIC ya remapeado): se atiende y se sigue.
        v if (32..48).contains(&v) => {
            unsafe { pic::eoi((v - 32) as u8) };
        }
        v => fatal(v, frame),
    }
}

fn fatal(vector: u64, f: &InterruptStack) -> ! {
    // Una linea por dato, y cortas: asi nada se corta ni se mezcla al leer la
    // pantalla (en un PC real casi siempre se lee a mano o con una foto).
    println!();
    println!("*** loboswolfos: fallo grave, el ordenador se detiene ***");
    println!(" vector {}", vector);
    println!(" causa  {}", vector_name(vector));
    println!(" error  {:016X}", f.error);
    println!(" rip    {:016X}", f.rip);
    println!(" cs     {:016X}", f.cs);
    println!(" rflags {:016X}", f.rflags);
    println!(" rsp    {:016X}", f.rsp);
    println!(" ss     {:016X}", f.ss);
    unsafe {
        let cr0: u64;
        let cr4: u64;
        asm!("mov {}, cr0", out(reg) cr0, options(nomem, nostack, preserves_flags));
        asm!("mov {}, cr4", out(reg) cr4, options(nomem, nostack, preserves_flags));
        println!(" cr0    {:016X}", cr0);
        println!(" cr4    {:016X}", cr4);

        // IDTR real y puerta del vector: si no son las del kernel, el problema
        // esta en la propia IDT.
        let mut idtr = [0u8; 10];
        asm!("sidt [{}]", in(reg) idtr.as_mut_ptr(), options(nostack, preserves_flags));
        let limite = u16::from_le_bytes([idtr[0], idtr[1]]);
        let base = u64::from_le_bytes([
            idtr[2], idtr[3], idtr[4], idtr[5], idtr[6], idtr[7], idtr[8], idtr[9],
        ]);
        println!(" idtr   {:016X}", base);
        println!(" idtl   {:04X}", limite);
        if base <= 0x00FF_FFFF && (vector as u16).saturating_mul(16) <= limite {
            let g = (base + vector * 16) as *const u8;
            let off_lo = u16::from_le_bytes([*g, *g.add(1)]);
            let sel = u16::from_le_bytes([*g.add(2), *g.add(3)]);
            let tipo = *g.add(5);
            let off_mid = u16::from_le_bytes([*g.add(6), *g.add(7)]);
            let off_hi = u32::from_le_bytes([*g.add(8), *g.add(9), *g.add(10), *g.add(11)]);
            let off = (off_lo as u64) | ((off_mid as u64) << 16) | ((off_hi as u64) << 32);
            println!(" puerta {:04X} {:016X} {:02X}", sel, off, tipo);
        }
    }
    if vector == 14 {
        let cr2: u64;
        unsafe { asm!("mov {}, cr2", out(reg) cr2, options(nomem, nostack)) };
        println!(" dir    {:016X}", cr2);
    }
    println!();
    crate::cpu::halt_loop()
}
