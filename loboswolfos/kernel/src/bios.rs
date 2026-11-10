//! Llamadas al BIOS (INT 10h / VESA VBE) desde el kernel de 64 bits.
//!
//! El kernel no puede ejecutar codigo del BIOS directamente (esta en modo
//! largo), asi que `bios.s` monta un pequeno trampolin que baja a modo real,
//! ejecuta el INT 10h y vuelve. Aqui se rellena el buzon de registros y se
//! exponen unas pocas rutinas de alto nivel (VBE, fuente de la VGA, modo texto).

use core::arch::global_asm;
use core::sync::atomic::{AtomicBool, Ordering};

use crate::sync::SyncCell;

global_asm!(include_str!("bios.s"));

/// Buzon de registros compartido con el trampolin (memoria baja).
const MAILBOX: usize = 0x7400;
/// Direccion donde el kernel copia el trampolin de 16/32 bits.
const BLOB_ADDR: usize = 0x7200;

const IN_EAX: usize = 0x00;
const IN_EBX: usize = 0x04;
const IN_ECX: usize = 0x08;
const IN_EDX: usize = 0x0C;
const IN_ESI: usize = 0x10;
const IN_EDI: usize = 0x14;
const IN_ES: usize = 0x18;
const IN_EBP: usize = 0x30;
const OUT_EAX: usize = 0x1C;
const OUT_EBX: usize = 0x20;
const OUT_ECX: usize = 0x24;
const OUT_EDX: usize = 0x28;
const OUT_FLAGS: usize = 0x2C;
const OUT_EBP: usize = 0x34;
const OUT_ES: usize = 0x38;
/// Banderin de control: si vale 1, el trampolin enciende IF durante el INT.
const IN_IF: usize = 0x3A;

/// `true` cuando el kernel ya es dueno del PIC y del PIT (a partir de `[4/4]`).
///
/// El firmware toquetea esos dos chips a su antojo, y el sitio donde mas se nota
/// es al activar un modo de video VESA: muchos BIOS reprograman el PIT para sus
/// propias esperas y lo dejan en modo "un solo disparo", de forma que la IRQ0
/// deja de llegar aunque el contador tenga los tics de antes. Con el escritorio
/// durmiendose con `hlt`, eso se ve como una GUI dibujada pero muerta (ni raton
/// ni teclado), y ademas cuelga para siempre todo lo que espera al temporizador
/// (los pitidos de `sound`, las esperas de los programas `.rok`...).
///
/// En cuanto el kernel toma el control de los dos chips, cada llamada al BIOS
/// los vuelve a dejar como estaban (ver [`tomar_interrupciones`]).
static PIC_PIT_PROPIOS: AtomicBool = AtomicBool::new(false);

/// Avisa a [`bios_call`] de que el kernel ya ha reubicado el PIC y programado el
/// PIT, asi que hay que restaurarlos despues de cada llamada al BIOS.
///
/// Debe llamarse despues del `[4/4]` (nunca antes: las primeras llamadas al BIOS
/// -mapa E820, disco- se hacen justamente con el PIC y el PIT del firmware, y
/// reubicarlos entonces seria el fallo que ya costo cara una vez).
pub fn tomar_interrupciones() {
    PIC_PIT_PROPIOS.store(true, Ordering::Relaxed);
}

/// Devuelve el PIC y el PIT al estado del kernel tras una llamada al BIOS.
///
/// El PIC se reinicia entero (no solo las mascaras): asi se limpia cualquier
/// interrupcion que el firmware hubiera dejado en servicio. Hacerlo con las
/// interrupciones apagadas evita que una IRQ se cuele a mitad de la secuencia.
unsafe fn rearmar_interrupciones() {
    crate::cpu::sin_interrupciones(|| {
        crate::pic::init();
        crate::pit::init(100);
    });
}

/// Verifica que el PIT siga generando IRQ0 después de un cambio de modo BIOS.
/// Si el firmware lo dejó en one-shot, rearma PIC/PIT del kernel y vuelve a
/// comprobarlo. Evita mostrar un escritorio aparentemente completo cuyo
/// planificador ya no recibe los tics necesarios.
pub fn asegurar_reloj_kernel() -> bool {
    if !PIC_PIT_PROPIOS.load(Ordering::Relaxed) || crate::pit::late() {
        return true;
    }

    unsafe { rearmar_interrupciones() };
    crate::pit::late()
}

extern "C" {
    fn rocky_load_gdt();
    fn rocky_bios_call();
    static rocky_int_imm: u8;
    static __lowtext_start: u8;
    static __lowtext_end: u8;
}

/// Estado de los registros con los que se invoca el BIOS.
#[derive(Clone, Copy, Default)]
pub struct Regs {
    pub eax: u32,
    pub ebx: u32,
    pub ecx: u32,
    pub edx: u32,
    pub esi: u32,
    pub edi: u32,
    pub ebp: u32,
    /// Segmento que se cargara en `ES` (para los buffers del BIOS).
    pub es: u16,
    /// `true` si el BIOS devolvio el flag de acarreo (CF) activo.
    pub carry: bool,
}

fn wr32(off: usize, value: u32) {
    unsafe { core::ptr::write_volatile((MAILBOX + off) as *mut u32, value) }
}

fn wr16(off: usize, value: u16) {
    unsafe { core::ptr::write_volatile((MAILBOX + off) as *mut u16, value) }
}

fn wr8(off: usize, value: u8) {
    unsafe { core::ptr::write_volatile((MAILBOX + off) as *mut u8, value) }
}

fn rd32(off: usize) -> u32 {
    unsafe { core::ptr::read_volatile((MAILBOX + off) as *const u32) }
}

fn rd16(off: usize) -> u16 {
    unsafe { core::ptr::read_volatile((MAILBOX + off) as *const u16) }
}

/// Prepara el trampolin. Debe llamarse una sola vez, al arrancar.
pub unsafe fn init() {
    let src = core::ptr::addr_of!(__lowtext_start);
    let len = core::ptr::addr_of!(__lowtext_end) as usize - src as usize;
    core::ptr::copy_nonoverlapping(src, BLOB_ADDR as *mut u8, len);
    rocky_load_gdt();
}

/// Ejecuta una interrupcion del BIOS con los registros indicados. `vector` es el
/// numero de `INT` (0x10 video, 0x15 sistema, ...). El kernel parchea el
/// inmediato del trampolin antes de saltar a memoria baja.
fn bios_call(vector: u8, r: Regs) -> Regs {
    wr32(IN_EAX, r.eax);
    wr32(IN_EBX, r.ebx);
    wr32(IN_ECX, r.ecx);
    wr32(IN_EDX, r.edx);
    wr32(IN_ESI, r.esi);
    wr32(IN_EDI, r.edi);
    wr32(IN_EBP, r.ebp);
    wr16(IN_ES, r.es);
    // Solo pedimos encender IF en el INT 15h (mapa E820): es la unica llamada
    // que se hace con el PIC y el PIT todavia como los dejo el firmware, asi
    // que una interrupcion alli cae en el manejador del propio firmware. En las
    // llamadas posteriores (INT 13h al disco) el kernel ya reubico el PIC en
    // 0x20, y una IRQ en modo real saltaria a una entrada de la IVT que no es
    // nuestra: IF se queda apagado.
    wr8(IN_IF, if vector == 0x15 { 1 } else { 0 });

    // El trampolin se copia tal cual a BLOB_ADDR, asi que la distancia entre
    // `rocky_int_imm` y el inicio de `.lowtext` se conserva: parcheamos el
    // inmediato del INT en la copia de memoria baja.
    let off = (core::ptr::addr_of!(rocky_int_imm) as usize)
        - (core::ptr::addr_of!(__lowtext_start) as usize);
    unsafe {
        core::ptr::write_volatile((BLOB_ADDR + off) as *mut u8, vector);
        // El trampolin puede encender IF mientras dura el INT (solo en el
        // E820) para que el firmware pueda usar su temporizador (IRQ0). La UART
        // deja activada su IRQ de recepcion (IRQ4); si el cable de COM1 captase
        // ruido justo en ese instante, la interrupcion caeria en el mapa del PIC
        // del firmware y podria dejar la linea activa (tormenta). Se apaga
        // mientras dura la llamada y se restaura despues tal y como estaba.
        let ier = crate::serial::COM1 + 1;
        let previo = crate::cpu::inb(ier);
        crate::cpu::outb(ier, 0x00);
        rocky_bios_call();
        crate::cpu::outb(ier, previo);
    };

    // El firmware ha podido reprogramar el PIT (o el PIC) mientras atendia la
    // llamada; si el kernel ya manda en ellos, se vuelven a dejar como estaban.
    if PIC_PIT_PROPIOS.load(Ordering::Relaxed) {
        unsafe { rearmar_interrupciones() };
    }

    Regs {
        eax: rd32(OUT_EAX),
        ebx: rd32(OUT_EBX),
        ecx: rd32(OUT_ECX),
        edx: rd32(OUT_EDX),
        esi: 0,
        edi: 0,
        ebp: rd32(OUT_EBP),
        es: rd16(OUT_ES),
        carry: rd16(OUT_FLAGS) & 0x0001 != 0,
    }
}

/// Ejecuta `INT 10h` (video) con los registros indicados y devuelve el resultado.
pub fn int10(r: Regs) -> Regs {
    bios_call(0x10, r)
}

/// Ejecuta `INT 10h` ignorando el resultado (para ordenes sin retorno).
pub fn int10_void(r: Regs) {
    let _ = int10(r);
}

/// Ejecuta `INT 15h` (servicios del sistema) y devuelve el resultado.
pub fn int15(r: Regs) -> Regs {
    bios_call(0x15, r)
}

/// Ejecuta `INT 13h` (acceso a disco del BIOS) y devuelve el resultado.
///
/// Para las funciones extendidas (`AH=42h`/`43h`) hay que dejar en `esi` la
/// direccion de la DAP y en `edx` (DL) la unidad; ambos viven por debajo de
/// 1 MiB porque el trampolin baja a modo real.
pub fn int13(r: Regs) -> Regs {
    bios_call(0x13, r)
}

/// Vuelve al modo texto clasico de 80x25 (INT 10h, AX=0003h).
///
/// En loboswolfos no hay escritorio que restaurar: el BIOS deja la memoria
/// de texto en blanco y la consola sigue escribiendo encima.
pub fn set_text_mode() {
    int10_void(Regs {
        eax: 0x0003,
        ..Default::default()
    });
}

/// Fuente de caracteres que expone el BIOS de la VGA.
#[derive(Clone, Copy)]
pub struct Font {
    /// Direccion lineal del primer glifo (caracter 0).
    pub addr: usize,
    /// Bytes por caracter (normalmente 16 para 8x16).
    pub bytes_per_char: usize,
    /// Altura en lineas de cada caracter.
    pub rows: usize,
}

/// Altura de la fuente que se copia a RAM (8x16, la de la VGA).
const GLIFO_MAX: usize = 16;
/// Copia RAM de la fuente: 256 caracteres de hasta [`GLIFO_MAX`] filas.
const FUENTE_TAM: usize = 256 * GLIFO_MAX;

/// Copia editable de la fuente de la VGA.
///
/// La ROM esta en memoria de solo lectura, y el juego CP437 que trae no incluye
/// los acentos en mayuscula (solo `É Ä Ö Ü Ç Ñ`). Para poder escribir `Á Ó Ú...`
/// la fuente se copia aqui al arrancar y se le anaden esos glifos recomponiendo
/// el acento de su forma minuscula sobre la letra mayuscula.
static COPIA: SyncCell<[u8; FUENTE_TAM]> = SyncCell::new([0; FUENTE_TAM]);
/// Fuente ya preparada, para no repetir la copia en cada llamada.
static PREPARADA: SyncCell<Option<Font>> = SyncCell::new(None);

/// Acentos, como filas de 8 pixeles de arriba a abajo.
///
/// En CP437 solo existen el grave, el circunflejo y la virgulilla (y estos dos
/// ultimos sueltos tampoco). El agudo y la dieresis se copian de las propias
/// letras acentuadas de la ROM (`á`, `ä`), que es donde mejor dibujados estan.
const ACENTO_AGUDO: [u8; 3] = [0x18, 0x30, 0x60];
const ACENTO_GRAVE: [u8; 3] = [0x60, 0x30, 0x18];
const ACENTO_CIRCUNFLEJO: [u8; 3] = [0x10, 0x38, 0x6C];
const ACENTO_DIERESIS: [u8; 1] = [0xC6];
const ACENTO_VIRGULILLA: [u8; 2] = [0x76, 0xDC];

/// Glifos que la ROM no trae y que aqui se componen.
///
/// Cada entrada es `(destino, letra base, acento, es mayuscula)`.
const COMPONER: [(u8, u8, &[u8], bool); 23] = [
    (0xC0, b'A', &ACENTO_GRAVE, true),       // À
    (0xC1, b'A', &ACENTO_AGUDO, true),       // Á
    (0xC2, b'A', &ACENTO_CIRCUNFLEJO, true), // Â
    (0xC3, b'A', &ACENTO_VIRGULILLA, true),  // Ã
    (0xC8, b'E', &ACENTO_GRAVE, true),       // È
    (0xC9, b'E', &ACENTO_AGUDO, true),       // É
    (0xCA, b'E', &ACENTO_CIRCUNFLEJO, true), // Ê
    (0xCB, b'E', &ACENTO_DIERESIS, true),    // Ë
    (0xCC, b'I', &ACENTO_GRAVE, true),       // Ì
    (0xCD, b'I', &ACENTO_AGUDO, true),       // Í
    (0xCE, b'I', &ACENTO_CIRCUNFLEJO, true), // Î
    (0xCF, b'I', &ACENTO_DIERESIS, true),    // Ï
    (0xD2, b'O', &ACENTO_GRAVE, true),       // Ò
    (0xD3, b'O', &ACENTO_AGUDO, true),       // Ó
    (0xD4, b'O', &ACENTO_CIRCUNFLEJO, true), // Ô
    (0xD5, b'O', &ACENTO_VIRGULILLA, true),  // Õ
    (0xD9, b'U', &ACENTO_GRAVE, true),       // Ù
    (0xDA, b'U', &ACENTO_AGUDO, true),       // Ú
    (0xDB, b'U', &ACENTO_CIRCUNFLEJO, true), // Û
    (0xDD, b'Y', &ACENTO_AGUDO, true),       // Ý
    (0xE3, b'a', &ACENTO_VIRGULILLA, false), // ã
    (0xF5, b'o', &ACENTO_VIRGULILLA, false), // õ
    (0xFD, b'y', &ACENTO_AGUDO, false),      // ý
];

/// En modo texto la VGA dibuja con el generador de caracteres de su ROM, que no
/// tiene esos acentos: se sustituyen por la letra base. La copia RAM de
/// [`get_font`] si los trae, asi que la GUI si puede pintarlos.
pub fn glifo_texto(b: u8) -> u8 {
    match COMPONER.iter().find(|c| c.0 == b) {
        Some(c) => c.1,
        None => b,
    }
}

/// Pide al BIOS la fuente ROM de 8x16 (INT 10h, AX=1130h, BH=06h) y la deja
/// copiada en RAM con los acentos en mayuscula ya compuestos.
///
/// Devuelve `None` si el BIOS no la ofrece. La fuente incluye los 256
/// caracteres de la pagina 0 (8x16 = 4096 bytes contiguos).
pub fn get_font() -> Option<Font> {
    let guardada = *unsafe { &*PREPARADA.get() };
    if guardada.is_some() {
        return guardada;
    }
    let font = preparar(get_font_rom());
    *unsafe { &mut *PREPARADA.get() } = font;
    font
}

/// Copia la fuente a RAM y le anade los glifos que faltan.
///
/// Solo se copia la de 8x16, que es la que trae la VGA: con cualquier otra el
/// escritorio conserva la de la ROM tal cual.
fn preparar(rom: Option<Font>) -> Option<Font> {
    let rom = rom?;
    if rom.bytes_per_char != GLIFO_MAX {
        return Some(rom);
    }
    let copia = unsafe { &mut *COPIA.get() };
    for c in 0..256usize {
        for f in 0..GLIFO_MAX {
            copia[c * GLIFO_MAX + f] =
                unsafe { core::ptr::read_volatile((rom.addr + c * GLIFO_MAX + f) as *const u8) };
        }
    }
    for &(destino, base, acento, mayus) in COMPONER.iter() {
        componer_acento(copia, destino, base, acento, mayus);
    }
    Some(Font {
        addr: copia.as_ptr() as usize,
        bytes_per_char: GLIFO_MAX,
        rows: GLIFO_MAX,
    })
}

/// Escribe en `destino` la letra `letra` con el `acento` encima.
///
/// Copia el criterio que ya usa la propia fuente ROM en sus acentuados en
/// mayuscula (`Ä Ñ É Å Ü`): el acento se apoya arriba del todo y la letra baja
/// lo justo para dejar una fila en blanco entre ambos, recortandose por abajo
/// para no mover la linea base.
fn componer_acento(copia: &mut [u8], destino: u8, letra: u8, acento: &[u8], mayus: bool) {
    let n = acento.len();
    // Fila de la ultima linea del acento: en mayuscula va pegada arriba del
    // todo; en minuscula se baja a la altura de `á` o `ñ`.
    let ultima = if mayus {
        if n >= 3 {
            2
        } else {
            1
        }
    } else {
        3
    };
    let primera = ultima + 1 - n;
    let d = letra as usize * GLIFO_MAX;
    let t = destino as usize * GLIFO_MAX;
    // Primera y ultima fila que ocupa la letra base.
    let tope = (0..GLIFO_MAX)
        .find(|&f| copia[d + f] != 0)
        .unwrap_or(GLIFO_MAX);
    let fin = match (0..GLIFO_MAX).rev().find(|&f| copia[d + f] != 0) {
        Some(f) => f,
        None => return,
    };
    let baja = (ultima + 2).saturating_sub(tope);
    for f in 0..GLIFO_MAX {
        let src = f as isize - baja as isize;
        copia[t + f] = if src >= 0 && src as usize <= fin {
            copia[d + src as usize]
        } else {
            0
        };
    }
    for (i, &px) in acento.iter().enumerate() {
        copia[t + primera + i] |= px;
    }
}

/// Consulta al BIOS por la direccion de la fuente ROM.
fn get_font_rom() -> Option<Font> {
    let r = int10(Regs {
        eax: 0x1130,
        ebx: 0x0600, // BH=6: fuente ROM 8x16
        ..Default::default()
    });
    let bp = (r.ebp & 0xFFFF) as usize;
    let es = r.es as usize;
    let bytes_per_char = (r.ecx & 0xFFFF) as usize;
    if bp == 0 || bytes_per_char == 0 {
        return None;
    }
    // La fuente de la VGA es de 8 pixels de ancho y un byte por fila, asi que
    // la altura real en filas es `bytes_per_char` (16 para 8x16). Algunos BIOS
    // devuelven en DL un valor poco fiable, por eso no se usa.
    Some(Font {
        addr: es * 16 + bp,
        bytes_per_char,
        rows: bytes_per_char,
    })
}
