//! lobovm.rs - VM resumible del subconjunto BIOS (fase 43).
//!
//! Ejecuta el bytecode que genera el editor con `lobo_compilar(...,
//! LOBO_MODO_VM, ...)`. Los numeros de opcode son los mismos de
//! `motor/lobo.h` y `kernel/src/lobo.rs`; el editor congel los programas en
//! la entrada `vm.lbin` del blob LGB1 para que Windows y la BIOS corran la
//! misma lista de instrucciones.
//!
//! No hay parser en el kernel: el texto del proyecto ya fue compilado por
//! el motor de 32 bits. Esto mantiene el interpreter pequeno y evita que
//! las dos gramaticas diverjan. Los opcodes fuera del subconjunto (ver
//! BIOS_SUBCONJUNTO.md) no se ejecutan: el editor avisa al compilar la
//! imagen y el script se marca como agotado en la serie.

use crate::biosent;
use crate::lobo;
use crate::sync::SyncCell;

/* ------------------------------------------------------------------ */
/* Limites y numeros de opcode (espejo de motor/lobo.h)                 */
/* ------------------------------------------------------------------ */

pub const PROGRAMAS_MAX: usize = 32;
pub const INSTS_MAX: usize = 4096;
pub const PILA_MAX: usize = 256;
pub const LOCALES_MAX: usize = 64;
pub const TEXTOS_MAX: usize = 64;
pub const TEXTOS_RUNTIME_MAX: usize = 32;
pub const TEXTO_MAX: usize = 64;
pub const PASOS_POR_FOTOGRAMA: i32 = 1000000;

const OP_NUM: u8 = 0;
const OP_VAR: u8 = 1;
const OP_TECLA: u8 = 2;
const OP_CHOCAR: u8 = 3;
const OP_CERRAR: u8 = 4;
const OP_SUMA: u8 = 5;
const OP_RESTA: u8 = 6;
const OP_MULT: u8 = 7;
const OP_DIV: u8 = 8;
const OP_MOD: u8 = 9;
const OP_IGUAL: u8 = 10;
const OP_DISTINTO: u8 = 11;
const OP_MENOR: u8 = 12;
const OP_MAYOR: u8 = 13;
const OP_MENOR_IG: u8 = 14;
const OP_MAYOR_IG: u8 = 15;
const OP_Y: u8 = 16;
const OP_O: u8 = 17;
const OP_NO: u8 = 18;
const OP_ASIGNA: u8 = 19;
const OP_SALTO: u8 = 20;
const OP_SALTO_FALSO: u8 = 21;
const OP_POP: u8 = 22;
const OP_ESPERAR: u8 = 23;
const OP_CARGAR_GLOBAL: u8 = 26;
const OP_GUARDAR_GLOBAL: u8 = 27;
const OP_MENSAJE: u8 = 39;
const OP_DETENER: u8 = 40;
const OP_CARGAR_MI: u8 = 42;
const OP_GUARDAR_MI: u8 = 43;
const OP_CREAR: u8 = 44;
const OP_BORRAR_ENTIDAD: u8 = 45;
const OP_MOVER_ENTIDAD: u8 = 46;
const OP_ENVIAR: u8 = 47;
const OP_REINICIAR_ESCENA: u8 = 49;
const OP_ABS: u8 = 50;
const OP_MIN: u8 = 51;
const OP_MAX: u8 = 52;
const OP_LIMITAR: u8 = 53;
const OP_REDONDEAR: u8 = 54;
const OP_ALEATORIO: u8 = 55;
const OP_TOCA: u8 = 60;
const OP_TOCA_TAG: u8 = 61;
const OP_DISTANCIA: u8 = 62;
const OP_CAJA_DE: u8 = 63;
const OP_TEXTO: u8 = 56;
const OP_LONGITUD: u8 = 57;
const OP_LETRA: u8 = 58;
const OP_UNIR: u8 = 59;
const OP_TECLA_PULSADA: u8 = 64;
const OP_TECLA_MANTENIDA: u8 = 65;
const OP_TECLA_SOLTADA: u8 = 66;
const OP_RATON_BOTON: u8 = 67;
const OP_RATON_X: u8 = 68;
const OP_RATON_Y: u8 = 69;
const OP_RATON_RUEDA: u8 = 70;
const OP_RATON_DENTRO: u8 = 71;
const OP_HUD_VIDAS: u8 = 72;
const OP_HUD_PUNTOS: u8 = 73;
const OP_HUD_DIALOGO: u8 = 74;
const OP_SONAR: u8 = 81;
const OP_MUSICA: u8 = 82;
const OP_DETENER_SONIDO: u8 = 83;
const OP_VOLUMEN: u8 = 84;
const OP_VOLUMEN_ACTUAL: u8 = 85;

/* ------------------------------------------------------------------ */
/* Programas                                                           */
/* ------------------------------------------------------------------ */

#[derive(Clone, Copy)]
struct VmInst {
    op: u8,
    valor: i32,
    extra: i32,
}

/// Un programa no copia el bytecode: apunta directamente a la entrada
/// `vm.lbin` del blob, que vive en RAM fija fuera de `.bss`. Con 32
/// programas de 4096 instrucciones, copiarlos aqui inundaria la pila del
/// kernel (ver kernel.ld).
#[derive(Clone, Copy)]
pub struct VmPrograma {
    id: i32,
    insts: &'static [u8],
    largo: usize,
    textos: &'static [u8],
    num_textos: usize,
}

impl VmPrograma {
    const NUEVO: VmPrograma = VmPrograma {
        id: 0,
        insts: &[],
        largo: 0,
        textos: &[],
        num_textos: 0,
    };

    /// Lee una instruccion de 12 bytes del bytecode congelado.
    fn inst(&self, i: usize) -> VmInst {
        let b = i * 12;
        let op = self.insts.get(b).copied().unwrap_or(0);
        let valor = lee_i32_en(self.insts, b + 4);
        let extra = lee_i32_en(self.insts, b + 8);
        VmInst { op, valor, extra }
    }

    /// Texto constante `i`, con los 64 bytes que ocupa en el blob.
    fn texto(&self, i: usize) -> &[u8] {
        let a = i * TEXTO_MAX;
        self.textos.get(a..a + TEXTO_MAX).unwrap_or(&[])
    }
}

static PROGRAMAS: SyncCell<[VmPrograma; PROGRAMAS_MAX]> =
    SyncCell::new([VmPrograma::NUEVO; PROGRAMAS_MAX]);
static NUM_PROGRAMAS: SyncCell<usize> = SyncCell::new(0);

/* Textos creados por `unir`, con los handles 64..95 del motor C. */
static TEXTOS_RUNTIME: SyncCell<[[u8; TEXTO_MAX]; TEXTOS_RUNTIME_MAX]> =
    SyncCell::new([[0; TEXTO_MAX]; TEXTOS_RUNTIME_MAX]);
static NUM_TEXTOS_RUNTIME: SyncCell<usize> = SyncCell::new(0);

/// Estado de una maquina resumible. Conserva el pc y la pila entre
/// fotogramas, igual que LoboMaquina de motor/lobo.h.
#[derive(Clone, Copy)]
pub struct Maquina {
    pub locales: [i32; LOCALES_MAX],
    pub cerrar: bool,
    pub agotado: bool,
    pc: usize,
    pila: [i32; PILA_MAX],
    np: usize,
    espera: i32,
    estado: u8,
    detener_todo: bool,
    entidad: i32,
    semilla: u32,
}

impl Maquina {
    pub const NUEVA: Maquina = Maquina {
        locales: [0; LOCALES_MAX],
        cerrar: false,
        agotado: false,
        pc: 0,
        pila: [0; PILA_MAX],
        np: 0,
        espera: 0,
        estado: 0,
        detener_todo: false,
        entidad: 1,
        semilla: 12345,
    };

    pub fn reiniciar(&mut self) {
        *self = Maquina::NUEVA;
    }

    pub fn detener_todo(&self) -> bool {
        self.detener_todo
    }

    /// True mientras el script esta dentro de `esperar`.
    pub fn esta_esperando(&self) -> bool {
        self.estado == 1
    }

    /// Entidad a la que pertenecen `mi` y `crear` en el evento actual.
    pub fn establecer_entidad(&mut self, id: i32) {
        self.entidad = if id <= 0 { 1 } else { id };
    }
}

/// Lee un u32 little-endian y avanza el cursor.
fn leer_u32(d: &[u8], p: &mut usize) -> Option<u32> {
    let b = d.get(*p..*p + 4)?;
    *p += 4;
    Some(u32::from_le_bytes([b[0], b[1], b[2], b[3]]))
}

fn leer_i32(d: &[u8], p: &mut usize) -> Option<i32> {
    leer_u32(d, p).map(|v| v as i32)
}

/// Lee un i32 little-endian de un slice sin avanzar ningun cursor.
fn lee_i32_en(d: &[u8], p: usize) -> i32 {
    let b = d.get(p..p + 4).unwrap_or(&[0, 0, 0, 0]);
    i32::from_le_bytes([b[0], b[1], b[2], b[3]])
}

/// Lee el bytecode `vm.lbin`. Devuelve el numero de programas cargados o
/// -1 si la cabecera no cuadra. Un blob viejo sin esta entrada devuelve 0
/// y el juego sigue con los scripts clasicos.
pub fn cargar(d: &'static [u8]) -> i32 {
    unsafe {
        *NUM_PROGRAMAS.get() = 0;
        *NUM_TEXTOS_RUNTIME.get() = 0;
    }
    if d.len() < 16 || &d[..4] != b"LVM1" {
        return -1;
    }
    let mut p = 4usize;
    let version = leer_u32(d, &mut p).unwrap_or(0);
    if version != 1 {
        return -1;
    }
    let num = leer_u32(d, &mut p).unwrap_or(0) as usize;
    let num_eventos = leer_u32(d, &mut p).unwrap_or(0) as usize;
    if num > PROGRAMAS_MAX || num_eventos > biosent::EVENTOS_DECLARADOS_MAX {
        return -1;
    }

    let programas = unsafe { &mut *PROGRAMAS.get() };
    for i in 0..num {
        let prog = &mut programas[i];
        *prog = VmPrograma::NUEVO;
        prog.id = leer_i32(d, &mut p).unwrap_or(-1);
        let ninst = leer_u32(d, &mut p).unwrap_or(0) as usize;
        let ntextos = leer_u32(d, &mut p).unwrap_or(0) as usize;
        if ninst > INSTS_MAX || ntextos > TEXTOS_MAX {
            unsafe { *NUM_PROGRAMAS.get() = 0 };
            return -1;
        }
        let fin_insts = match p.checked_add(ninst * 12) {
            Some(v) if v <= d.len() => v,
            _ => {
                unsafe { *NUM_PROGRAMAS.get() = 0 };
                return -1;
            }
        };
        prog.largo = ninst;
        prog.insts = &d[p..fin_insts];
        p = fin_insts;
        let fin_textos = match p.checked_add(ntextos * TEXTO_MAX) {
            Some(v) if v <= d.len() => v,
            _ => {
                unsafe { *NUM_PROGRAMAS.get() = 0 };
                return -1;
            }
        };
        prog.num_textos = ntextos;
        prog.textos = &d[p..fin_textos];
        p = fin_textos;
    }
    // Eventos declarados: tipo, parametro, script, objeto.
    biosent::limpiar_eventos();
    for _ in 0..num_eventos {
        let tipo = leer_i32(d, &mut p).unwrap_or(0);
        let parametro = leer_i32(d, &mut p).unwrap_or(0);
        let script = leer_i32(d, &mut p).unwrap_or(-1);
        let objeto = leer_i32(d, &mut p).unwrap_or(0);
        biosent::declarar(tipo, parametro, script, objeto);
    }
    unsafe { *NUM_PROGRAMAS.get() = num };
    num as i32
}

/// Numero de programas VM cargados.
pub fn cantidad() -> usize {
    unsafe { *NUM_PROGRAMAS.get() }
}

/// Busca el programa de una funcion por su id del LOGIC2.
pub fn programa(id: i32) -> Option<&'static VmPrograma> {
    let n = cantidad();
    let programas = unsafe { &*PROGRAMAS.get() };
    for prog in programas.iter().take(n) {
        if prog.id == id {
            return Some(prog);
        }
    }
    None
}

/// Indice de un programa por el id de su funcion LOGIC2.
pub fn indice_programa(id: i32) -> Option<usize> {
    let n = cantidad();
    let programas = unsafe { &*PROGRAMAS.get() };
    for i in 0..n {
        if programas[i].id == id {
            return Some(i);
        }
    }
    None
}

/// Programa por indice (lo usan los eventos declarados).
pub fn programa_indice(i: i32) -> Option<&'static VmPrograma> {
    let n = cantidad();
    if i < 0 || i as usize >= n {
        return None;
    }
    Some(unsafe { &(*PROGRAMAS.get())[i as usize] })
}

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */

fn texto_largo(t: &[u8]) -> usize {
    t.iter().position(|&b| b == 0).unwrap_or(t.len())
}

fn resolver_texto(prog: &VmPrograma, valor: i32) -> &[u8] {
    let v = valor as u32;
    /* Las instrucciones (mensaje, crear, toca_tag, sonar...) guardsan el
     * indice a secas; las expresiones de texto (`"hola"`, `unir`) apilan un
     * handle con LOBO_TEXTO_MARCA. Los dos llegan aqui. */
    let idx = if v & 0x4000_0000 != 0 {
        (v & 0x3fff_ffff) as usize
    } else {
        v as usize
    };
    if idx < prog.num_textos {
        let t = prog.texto(idx);
        return &t[..texto_largo(t)];
    }
    if idx >= TEXTOS_MAX && idx < TEXTOS_MAX + TEXTOS_RUNTIME_MAX {
        let n = unsafe { *NUM_TEXTOS_RUNTIME.get() };
        if idx - TEXTOS_MAX < n {
            let t = unsafe { &(*TEXTOS_RUNTIME.get())[idx - TEXTOS_MAX] };
            return &t[..texto_largo(t)];
        }
    }
    &[]
}

fn push(pila: &mut [i32], np: &mut usize, valor: i32) -> bool {
    if *np >= pila.len() {
        return false;
    }
    pila[*np] = valor;
    *np += 1;
    true
}

fn pop(pila: &[i32], np: &mut usize) -> Option<i32> {
    if *np == 0 {
        return None;
    }
    *np -= 1;
    Some(pila[*np])
}

fn leer_var(_prog: &VmPrograma, st: &lobo::LoboEstado, m: &Maquina, var: i32) -> i32 {
    match var {
        0 => st.x,
        1 => st.y,
        2 => st.vel,
        3 => st.fotograma,
        v if v >= 4 && v < 4 + LOCALES_MAX as i32 => m.locales[(v - 4) as usize],
        /* 1000..1005 son fisica en el motor C; fuera del subconjunto BIOS
         * valen 0. Las globales van por CARGAR_GLOBAL/GUARDAR_GLOBAL. */
        _ => 0,
    }
}

fn escribir_var(prog: &VmPrograma, st: &mut lobo::LoboEstado, m: &mut Maquina, var: i32, valor: i32) {
    match var {
        0 => st.x = valor,
        1 => st.y = valor,
        2 => st.vel = valor,
        3 => st.fotograma = valor,
        v if v >= 4 && v < 4 + LOCALES_MAX as i32 => m.locales[(v - 4) as usize] = valor,
        _ => {}
    }
    let _ = prog;
}

fn aleatorio(m: &mut Maquina, a: i32, b: i32) -> i32 {
    if a >= b {
        return a;
    }
    m.semilla = m
        .semilla
        .wrapping_mul(1664525)
        .wrapping_add(1013904223);
    let r = (m.semilla >> 8) as i32 & 0x7fff_ffff;
    a + (r % (b - a + 1))
}

/* ------------------------------------------------------------------ */
/* Paso de la VM                                                       */
/* ------------------------------------------------------------------ */

/// Ejecuta una instruccion. Devuelve 0 si se puede seguir, 1 si llego a un
/// evento y 2 si hubo un error de pila, salto u opcode fuera del subset.
fn paso<F: FnMut(i32, i32) -> bool>(
    prog: &VmPrograma,
    st: &mut lobo::LoboEstado,
    m: &mut Maquina,
    choca: &mut F,
) -> i32 {
    if m.estado != 0 || m.pc >= prog.largo {
        m.estado = 2;
        return 1;
    }
    let i = prog.inst(m.pc);
    m.pc += 1;
    crate::perfil::contar_instruccion();

    let mut ok = true;
    match i.op {
        OP_NUM => ok = push(&mut m.pila, &mut m.np, i.valor),
        OP_VAR => {
            let v = leer_var(prog, st, m, i.valor);
            ok = push(&mut m.pila, &mut m.np, v);
        }
        OP_TECLA => {
            let v = (st.entrada & i.valor as u32 != 0) as i32;
            ok = push(&mut m.pila, &mut m.np, v);
        }
        OP_TECLA_PULSADA | OP_TECLA_MANTENIDA | OP_TECLA_SOLTADA => {
            let e = &st.ext;
            let bit = if i.valor >= 0 && i.valor < 32 {
                1u32 << (i.valor as u32)
            } else {
                0
            };
            let v = if i.op == OP_TECLA_PULSADA {
                (bit != 0 && e.teclas & bit != 0 && e.anterior & bit == 0) as i32
            } else if i.op == OP_TECLA_MANTENIDA {
                (bit != 0 && e.teclas & bit != 0) as i32
            } else {
                (bit != 0 && e.teclas & bit == 0 && e.anterior & bit != 0) as i32
            };
            ok = push(&mut m.pila, &mut m.np, v);
        }
        OP_RATON_X | OP_RATON_Y | OP_RATON_RUEDA | OP_RATON_DENTRO => {
            let e = &st.ext;
            let v = if i.op == OP_RATON_X {
                e.raton_x
            } else if i.op == OP_RATON_Y {
                e.raton_y
            } else if i.op == OP_RATON_RUEDA {
                e.raton_rueda
            } else {
                e.raton_dentro as i32
            };
            ok = push(&mut m.pila, &mut m.np, v);
        }
        OP_RATON_BOTON => {
            let boton = pop(&m.pila, &mut m.np);
            if let Some(boton) = boton {
                let v = if boton >= 1 && boton <= 3 {
                    (st.ext.raton_botones & (1u8 << (boton - 1)) != 0) as i32
                } else {
                    0
                };
                ok = push(&mut m.pila, &mut m.np, v);
            } else {
                ok = false;
            }
        }
        OP_CHOCAR => {
            let y = pop(&m.pila, &mut m.np);
            let x = pop(&m.pila, &mut m.np);
            if let (Some(x), Some(y)) = (x, y) {
                ok = push(&mut m.pila, &mut m.np, choca(x, y) as i32);
            } else {
                ok = false;
            }
        }
        OP_CERRAR => {
            m.cerrar = true;
            ok = push(&mut m.pila, &mut m.np, 0);
        }
        OP_TEXTO => ok = push(&mut m.pila, &mut m.np, 0x4000_0000i32 | i.valor),
        OP_SUMA | OP_RESTA | OP_MULT | OP_DIV | OP_MOD => {
            let b = pop(&m.pila, &mut m.np);
            let a = pop(&m.pila, &mut m.np);
            if let (Some(a), Some(b)) = (a, b) {
                let v = match i.op {
                    OP_SUMA => a.wrapping_add(b),
                    OP_RESTA => a.wrapping_sub(b),
                    OP_MULT => a.wrapping_mul(b),
                    OP_DIV => {
                        if b == 0 || (a == i32::MIN && b == -1) {
                            0
                        } else {
                            a / b
                        }
                    }
                    _ => {
                        if b == 0 || (a == i32::MIN && b == -1) {
                            0
                        } else {
                            a % b
                        }
                    }
                };
                ok = push(&mut m.pila, &mut m.np, v);
            } else {
                ok = false;
            }
        }
        OP_IGUAL | OP_DISTINTO | OP_MENOR | OP_MAYOR | OP_MENOR_IG | OP_MAYOR_IG | OP_Y | OP_O => {
            let b = pop(&m.pila, &mut m.np);
            let a = pop(&m.pila, &mut m.np);
            if let (Some(a), Some(b)) = (a, b) {
                let v = match i.op {
                    OP_IGUAL => (a == b) as i32,
                    OP_DISTINTO => (a != b) as i32,
                    OP_MENOR => (a < b) as i32,
                    OP_MAYOR => (a > b) as i32,
                    OP_MENOR_IG => (a <= b) as i32,
                    OP_MAYOR_IG => (a >= b) as i32,
                    OP_Y => (a != 0 && b != 0) as i32,
                    _ => (a != 0 || b != 0) as i32,
                };
                ok = push(&mut m.pila, &mut m.np, v);
            } else {
                ok = false;
            }
        }
        OP_NO => {
            let a = pop(&m.pila, &mut m.np);
            if let Some(a) = a {
                ok = push(&mut m.pila, &mut m.np, (a == 0) as i32);
            } else {
                ok = false;
            }
        }
        OP_ASIGNA => {
            let a = pop(&m.pila, &mut m.np);
            if let Some(a) = a {
                escribir_var(prog, st, m, i.valor, a);
            } else {
                ok = false;
            }
        }
        OP_SALTO => {
            if i.valor < 0 || i.valor as usize > prog.largo {
                ok = false;
            } else {
                m.pc = i.valor as usize;
            }
        }
        OP_SALTO_FALSO => {
            let a = pop(&m.pila, &mut m.np);
            if let Some(0) = a {
                if i.valor < 0 || i.valor as usize > prog.largo {
                    ok = false;
                } else {
                    m.pc = i.valor as usize;
                }
            } else if a.is_none() {
                ok = false;
            }
        }
        OP_POP => {
            if pop(&m.pila, &mut m.np).is_none() {
                ok = false;
            }
        }
        OP_ESPERAR => {
            let a = pop(&m.pila, &mut m.np);
            if let Some(a) = a {
                if a > 0 {
                    m.espera = a;
                    m.estado = 1;
                    return 1;
                }
            } else {
                ok = false;
            }
        }
        OP_CARGAR_GLOBAL => {
            let v = biosent::global_leer(i.valor);
            ok = push(&mut m.pila, &mut m.np, v);
        }
        OP_GUARDAR_GLOBAL => {
            let a = pop(&m.pila, &mut m.np);
            if let Some(a) = a {
                biosent::global_escribir(i.valor, a);
            } else {
                ok = false;
            }
        }
        OP_CARGAR_MI => {
            let v = biosent::leer(m.entidad, i.valor);
            ok = push(&mut m.pila, &mut m.np, v);
        }
        OP_GUARDAR_MI => {
            let a = pop(&m.pila, &mut m.np);
            if let Some(a) = a {
                biosent::escribir(m.entidad, i.valor, a);
            } else {
                ok = false;
            }
        }
        OP_CREAR => {
            let y = pop(&m.pila, &mut m.np);
            let x = pop(&m.pila, &mut m.np);
            if let (Some(x), Some(y)) = (x, y) {
                let tag = resolver_texto(prog, i.valor);
                let id = biosent::crear(tag, x, y);
                ok = push(&mut m.pila, &mut m.np, id);
            } else {
                ok = false;
            }
        }
        OP_BORRAR_ENTIDAD => {
            let id = if i.valor == 1 {
                pop(&m.pila, &mut m.np).unwrap_or(m.entidad)
            } else {
                m.entidad
            };
            biosent::borrar(id);
            ok = push(&mut m.pila, &mut m.np, 0);
        }
        OP_MOVER_ENTIDAD => {
            let y = pop(&m.pila, &mut m.np);
            let x = pop(&m.pila, &mut m.np);
            let id = pop(&m.pila, &mut m.np);
            if let (Some(id), Some(x), Some(y)) = (id, x, y) {
                biosent::mover(id, x, y);
                ok = push(&mut m.pila, &mut m.np, 0);
            } else {
                ok = false;
            }
        }
        OP_MENSAJE | OP_ENVIAR => {
            let texto = resolver_texto(prog, i.valor);
            biosent::emitir(biosent::EV_RECIBIR, i.valor, -1, m.entidad);
            let msg = core::str::from_utf8(texto).unwrap_or("(ilegible)");
            crate::println!("egine: mensaje {}.", msg);
            /* `mensaje` no apila nada; `enviar` apila 0 (motor/lobo.c). */
            if i.op == OP_ENVIAR {
                ok = push(&mut m.pila, &mut m.np, 0);
            }
        }
        OP_DETENER => {
            if i.valor == 1 {
                m.detener_todo = true;
            }
            m.estado = 2;
            return 1;
        }
        OP_ABS => {
            let a = pop(&m.pila, &mut m.np);
            if let Some(a) = a {
                ok = push(&mut m.pila, &mut m.np, a.saturating_abs());
            } else {
                ok = false;
            }
        }
        OP_MIN | OP_MAX => {
            let b = pop(&m.pila, &mut m.np);
            let a = pop(&m.pila, &mut m.np);
            if let (Some(a), Some(b)) = (a, b) {
                let v = if i.op == OP_MIN {
                    a.min(b)
                } else {
                    a.max(b)
                };
                ok = push(&mut m.pila, &mut m.np, v);
            } else {
                ok = false;
            }
        }
        OP_LIMITAR => {
            let hi = pop(&m.pila, &mut m.np);
            let lo = pop(&m.pila, &mut m.np);
            let valor = pop(&m.pila, &mut m.np);
            if let (Some(valor), Some(lo), Some(hi)) = (valor, lo, hi) {
                let (lo, hi) = if lo > hi { (hi, lo) } else { (lo, hi) };
                ok = push(&mut m.pila, &mut m.np, valor.clamp(lo, hi));
            } else {
                ok = false;
            }
        }
        OP_REDONDEAR => {
            let a = pop(&m.pila, &mut m.np);
            if let Some(a) = a {
                ok = push(&mut m.pila, &mut m.np, a);
            } else {
                ok = false;
            }
        }
        OP_ALEATORIO => {
            let b = pop(&m.pila, &mut m.np);
            let a = pop(&m.pila, &mut m.np);
            if let (Some(a), Some(b)) = (a, b) {
                let v = aleatorio(m, a, b);
                ok = push(&mut m.pila, &mut m.np, v);
            } else {
                ok = false;
            }
        }
        OP_REINICIAR_ESCENA => {
            crate::escenas::reiniciar();
            ok = push(&mut m.pila, &mut m.np, 0);
        }
        OP_TOCA => {
            let id = pop(&m.pila, &mut m.np);
            if let Some(id) = id {
                let v = biosent::toca(m.entidad, id);
                ok = push(&mut m.pila, &mut m.np, v);
            } else {
                ok = false;
            }
        }
        OP_TOCA_TAG => {
            let tag = resolver_texto(prog, i.valor);
            let v = biosent::toca_tag(m.entidad, tag);
            ok = push(&mut m.pila, &mut m.np, v);
        }
        OP_DISTANCIA => {
            let id = pop(&m.pila, &mut m.np);
            if let Some(id) = id {
                let v = biosent::distancia(m.entidad, id);
                ok = push(&mut m.pila, &mut m.np, v);
            } else {
                ok = false;
            }
        }
        OP_CAJA_DE => {
            let campo = pop(&m.pila, &mut m.np);
            let id = pop(&m.pila, &mut m.np);
            if let (Some(id), Some(campo)) = (id, campo) {
                let v = biosent::caja_de(id, campo);
                ok = push(&mut m.pila, &mut m.np, v);
            } else {
                ok = false;
            }
        }
        OP_LONGITUD => {
            let a = pop(&m.pila, &mut m.np);
            if let Some(a) = a {
                let v = resolver_texto(prog, a).len() as i32;
                ok = push(&mut m.pila, &mut m.np, v);
            } else {
                ok = false;
            }
        }
        OP_LETRA => {
            let indice = pop(&m.pila, &mut m.np);
            let handle = pop(&m.pila, &mut m.np);
            if let (Some(handle), Some(indice)) = (handle, indice) {
                let texto = resolver_texto(prog, handle);
                let v = if indice >= 0 && (indice as usize) < texto.len() {
                    texto[indice as usize] as i32
                } else {
                    0
                };
                ok = push(&mut m.pila, &mut m.np, v);
            } else {
                ok = false;
            }
        }
        OP_UNIR => {
            let b = pop(&m.pila, &mut m.np);
            let a = pop(&m.pila, &mut m.np);
            if let (Some(a), Some(b)) = (a, b) {
                let ta = resolver_texto(prog, a);
                let tb = resolver_texto(prog, b);
                let mut buf = [0u8; TEXTO_MAX];
                let n = ta.len().min(TEXTO_MAX - 1);
                buf[..n].copy_from_slice(&ta[..n]);
                let n2 = tb.len().min(TEXTO_MAX - 1 - n);
                buf[n..n + n2].copy_from_slice(&tb[..n2]);
                /* `unir` crea un texto en la memoria de la VM, con handle
                 * 64..95, igual que LoboMemoria del motor C. */
                let n_runtime = unsafe { *NUM_TEXTOS_RUNTIME.get() };
                let slot = if n_runtime < TEXTOS_RUNTIME_MAX {
                    n_runtime
                } else {
                    TEXTOS_RUNTIME_MAX - 1
                };
                unsafe {
                    (*TEXTOS_RUNTIME.get())[slot] = buf;
                    if n_runtime < TEXTOS_RUNTIME_MAX {
                        *NUM_TEXTOS_RUNTIME.get() = n_runtime + 1;
                    }
                }
                let handle = 0x4000_0000i32 | (TEXTOS_MAX + slot) as i32;
                ok = push(&mut m.pila, &mut m.np, handle);
            } else {
                ok = false;
            }
        }
        OP_HUD_VIDAS | OP_HUD_PUNTOS | OP_HUD_DIALOGO => {
            if i.op == OP_HUD_VIDAS {
                let a = pop(&m.pila, &mut m.np);
                if let Some(a) = a {
                    let anterior = lobo::hud_vidas(a);
                    ok = push(&mut m.pila, &mut m.np, anterior);
                } else {
                    ok = false;
                }
            } else if i.op == OP_HUD_PUNTOS {
                let a = pop(&m.pila, &mut m.np);
                if let Some(a) = a {
                    let anterior = lobo::hud_puntos(a);
                    ok = push(&mut m.pila, &mut m.np, anterior);
                } else {
                    ok = false;
                }
            } else {
                let linea = pop(&m.pila, &mut m.np);
                let handle = pop(&m.pila, &mut m.np);
                if let (Some(linea), Some(handle)) = (linea, handle) {
                    lobo::hud_dialogo(linea, resolver_texto(prog, handle));
                    ok = push(&mut m.pila, &mut m.np, 0);
                } else {
                    ok = false;
                }
            }
        }
        OP_SONAR | OP_MUSICA | OP_DETENER_SONIDO | OP_VOLUMEN | OP_VOLUMEN_ACTUAL => {
            match i.op {
                OP_SONAR => {
                    let handle = pop(&m.pila, &mut m.np);
                    if let Some(handle) = handle {
                        crate::audio::sonar(resolver_texto(prog, handle));
                        ok = push(&mut m.pila, &mut m.np, 1);
                    } else {
                        ok = false;
                    }
                }
                OP_MUSICA => {
                    let handle = pop(&m.pila, &mut m.np);
                    if let Some(handle) = handle {
                        crate::audio::musica(resolver_texto(prog, handle));
                        ok = push(&mut m.pila, &mut m.np, 1);
                    } else {
                        ok = false;
                    }
                }
                OP_DETENER_SONIDO => {
                    crate::audio::detener();
                    ok = push(&mut m.pila, &mut m.np, 0);
                }
                OP_VOLUMEN => {
                    let v = pop(&m.pila, &mut m.np);
                    if let Some(v) = v {
                        let anterior = crate::audio::volumen(v);
                        ok = push(&mut m.pila, &mut m.np, anterior);
                    } else {
                        ok = false;
                    }
                }
                _ => {
                    let v = crate::audio::volumen_actual();
                    ok = push(&mut m.pila, &mut m.np, v);
                }
            }
        }
        _ => ok = false, // opcode fuera del subconjunto
    }

    if !ok {
        m.agotado = true;
        m.estado = 2;
        return 2;
    }
    if m.pc >= prog.largo {
        m.estado = 2;
        return 1;
    }
    0
}


/* ------------------------------------------------------------------ */
/* Fotogramas y eventos                                                */
/* ------------------------------------------------------------------ */

/// Corre hasta un evento o hasta agotar el presupuesto.
fn correr<F: FnMut(i32, i32) -> bool>(
    prog: &VmPrograma,
    st: &mut lobo::LoboEstado,
    m: &mut Maquina,
    choca: &mut F,
) -> bool {
    let mut pasos = PASOS_POR_FOTOGRAMA;
    while pasos > 0 {
        pasos -= 1;
        let r = paso(prog, st, m, choca);
        if r != 0 {
            return true;
        }
    }
    m.agotado = true;
    false
}

/// Evento de fotograma: reengancha un script terminado, descuenta la
/// espera y corre hasta el proximo evento. Es el equivalente de
/// `lobo_vm_fotograma`.
pub fn fotograma<F: FnMut(i32, i32) -> bool>(
    prog: &VmPrograma,
    st: &mut lobo::LoboEstado,
    m: &mut Maquina,
    choca: &mut F,
) -> bool {
    m.cerrar = false;
    m.agotado = false;
    if m.estado == 2 {
        m.pc = 0;
        m.np = 0;
        m.espera = 0;
        m.estado = 0;
    } else if m.estado == 1 {
        if m.espera > 0 {
            m.espera -= 1;
        }
        if m.espera > 0 {
            return false;
        }
        m.estado = 0;
    }
    correr(prog, st, m, choca)
}

/// Descuenta una espera de un disparo que no se repite, sin reiniciar el
/// script. Es el equivalente de `lobo_vm_reanudar` de motor/lobo.c.
pub fn reanudar<F: FnMut(i32, i32) -> bool>(
    prog: &VmPrograma,
    st: &mut lobo::LoboEstado,
    m: &mut Maquina,
    choca: &mut F,
) -> bool {
    if m.estado != 1 {
        return false;
    }
    m.cerrar = false;
    m.agotado = false;
    if m.espera > 0 {
        m.espera -= 1;
    }
    if m.espera > 0 {
        return false;
    }
    m.estado = 0;
    correr(prog, st, m, choca)
}

/// Evento de un disparo (inicio, tecla, recibir...). No reinicia un script
/// que sigue esperando; si termino, lo deja listo para el disparo.
pub fn evento<F: FnMut(i32, i32) -> bool>(
    prog: &VmPrograma,
    st: &mut lobo::LoboEstado,
    m: &mut Maquina,
    choca: &mut F,
) -> bool {
    m.cerrar = false;
    m.agotado = false;
    if m.estado == 1 {
        return false;
    }
    if m.estado == 2 {
        m.pc = 0;
        m.np = 0;
        m.espera = 0;
        m.estado = 0;
    }
    correr(prog, st, m, choca)
}
