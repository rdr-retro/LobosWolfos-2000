//! saves.rs - guardado, records y ajustes del kernel (fase 40).
//!
//! Es el espejo de `motor/save.c` con un sistema de archivos simple dentro
//! del propio kernel: las claves viven en un buffer de texto con la misma
//! forma `lw2ksave 1` + una linea `clave valor` por entrada. El buffer
//! ocupa ocho sectores de 512 bytes y se puede volcar a un sector reservado
//! del USB (fase 43) sin cambiar el formato ni la VM.
//!
//! Aqui no hay E/S de disco: el juego es monotarea y el arranque no debe
//! depender de que la unidad admita escrituras. `bytes()` y
//! `cargar_bytes()` son la frontera para conectar el sector reservado mas
//! adelante.

use crate::sync::SyncCell;

/// Ocho sectores de 512 bytes, el mismo tope que el save de Windows (64 KB
/// se reparte aqui en entrada y cabecera).
pub const SAVE_BUFFER_MAX: usize = 8 * 512;
pub const SAVE_CLAVES_MAX: usize = 128;
pub const SAVE_CLAVE_MAX: usize = 32;

const CABECERA: &[u8] = b"lw2ksave 1\n";
const PREFIJO_RECORD: &[u8] = b"record.";
const PREFIJO_AJUSTE: &[u8] = b"ajuste.";

/// Una clave y su valor. `largo` es la longitud util del nombre.
#[derive(Clone, Copy)]
struct Entrada {
    clave: [u8; SAVE_CLAVE_MAX],
    largo: usize,
    valor: i32,
}

impl Entrada {
    const NUEVA: Entrada = Entrada {
        clave: [0; SAVE_CLAVE_MAX],
        largo: 0,
        valor: 0,
    };
}

/// Tabla del juego y el texto que la representa en el "sector reservado".
#[derive(Clone, Copy)]
struct SaveEstado {
    entradas: [Entrada; SAVE_CLAVES_MAX],
    num: usize,
    buffer: [u8; SAVE_BUFFER_MAX],
    largo: usize,
}

impl SaveEstado {
    const NUEVO: SaveEstado = SaveEstado {
        entradas: [Entrada::NUEVA; SAVE_CLAVES_MAX],
        num: 0,
        buffer: [0; SAVE_BUFFER_MAX],
        largo: 0,
    };
}

static SAVE: SyncCell<SaveEstado> = SyncCell::new(SaveEstado::NUEVO);

/* ------------------------------------------------------------------ */
/* Utilidades de texto                                                 */
/* ------------------------------------------------------------------ */

fn util(texto: &[u8]) -> &[u8] {
    let n = texto.iter().position(|&b| b == 0).unwrap_or(texto.len());
    &texto[..n]
}

fn clave_valida(clave: &[u8]) -> bool {
    if clave.is_empty() || clave.len() > SAVE_CLAVE_MAX {
        return false;
    }
    clave.iter().all(|&c| {
        (c >= b'a' && c <= b'z')
            || (c >= b'A' && c <= b'Z')
            || (c >= b'0' && c <= b'9')
            || c == b'_'
            || c == b'.'
            || c == b'-'
    })
}

fn mismo_nombre(a: &Entrada, b: &[u8]) -> bool {
    &a.clave[..a.largo] == b
}

fn buscar(estado: &SaveEstado, clave: &[u8]) -> Option<usize> {
    estado.entradas[..estado.num]
        .iter()
        .position(|e| mismo_nombre(e, clave))
}

/// Escribe `clave valor\n` en el buffer. Devuelve false si no cabe.
fn escribir_linea(buffer: &mut [u8; SAVE_BUFFER_MAX], pos: &mut usize,
                  clave: &[u8], valor: i32) -> bool {
    if *pos + clave.len() + 14 > SAVE_BUFFER_MAX {
        return false;
    }
    buffer[*pos..*pos + clave.len()].copy_from_slice(clave);
    *pos += clave.len();
    buffer[*pos] = b' ';
    *pos += 1;

    let negativo = valor < 0;
    let mut magnitude = if negativo {
        -(valor as i64)
    } else {
        valor as i64
    };
    let mut digits = [0u8; 12];
    let mut n = 0usize;
    if magnitude == 0 {
        digits[n] = b'0';
        n += 1;
    }
    while magnitude > 0 {
        digits[n] = b'0' + (magnitude % 10) as u8;
        magnitude /= 10;
        n += 1;
    }
    if negativo {
        buffer[*pos] = b'-';
        *pos += 1;
    }
    while n > 0 {
        n -= 1;
        buffer[*pos] = digits[n];
        *pos += 1;
    }
    buffer[*pos] = b'\n';
    *pos += 1;
    true
}

/// Regenera el texto del buffer a partir de la tabla.
fn regenerar(estado: &mut SaveEstado) -> bool {
    let mut pos = 0usize;
    if pos + CABECERA.len() > SAVE_BUFFER_MAX {
        return false;
    }
    estado.buffer[pos..pos + CABECERA.len()].copy_from_slice(CABECERA);
    pos += CABECERA.len();
    for e in estado.entradas[..estado.num].iter() {
        if !escribir_linea(&mut estado.buffer, &mut pos,
                           &e.clave[..e.largo], e.valor) {
            return false;
        }
    }
    estado.largo = pos;
    true
}

/// Convierte un numero decimal del texto, con signo opcional.
fn parsear_numero(linea: &[u8]) -> Option<i32> {
    let mut i = 0usize;
    let mut negativo = false;
    let mut valor: i64 = 0;
    if i < linea.len() && (linea[i] == b'-' || linea[i] == b'+') {
        negativo = linea[i] == b'-';
        i += 1;
    }
    let inicio = i;
    while i < linea.len() && linea[i].is_ascii_digit() {
        valor = valor.saturating_mul(10) + (linea[i] - b'0') as i64;
        i += 1;
    }
    if i == inicio || i != linea.len() {
        return None;
    }
    let valor = if negativo { -valor } else { valor };
    if valor < i32::MIN as i64 || valor > i32::MAX as i64 {
        None
    } else {
        Some(valor as i32)
    }
}

/// Lee el buffer del sector reservado. Una sola linea mala descarta todo,
/// igual que hace el save de Windows: un save corrupto no impide arrancar.
fn desde_buffer(estado: &mut SaveEstado) {
    let mut pos = CABECERA.len();
    let mut tabla = [Entrada::NUEVA; SAVE_CLAVES_MAX];
    let mut num = 0usize;
    if estado.largo < CABECERA.len()
        || &estado.buffer[..CABECERA.len()] != CABECERA
    {
        return;
    }
    while pos < estado.largo {
        let fin = estado.buffer[pos..estado.largo]
            .iter()
            .position(|&b| b == b'\n')
            .map(|n| pos + n)
            .unwrap_or(estado.largo);
        let linea = &estado.buffer[pos..fin];
        pos = if fin < estado.largo { fin + 1 } else { fin };
        if linea.is_empty() {
            continue;
        }
        let Some(espacio) = linea.iter().position(|&b| b == b' ') else {
            return;
        };
        let clave = &linea[..espacio];
        let Some(valor) = parsear_numero(&linea[espacio + 1..]) else {
            return;
        };
        if !clave_valida(clave) || num >= SAVE_CLAVES_MAX {
            return;
        }
        tabla[num].clave[..clave.len()].copy_from_slice(clave);
        tabla[num].largo = clave.len();
        tabla[num].valor = valor;
        num += 1;
    }
    estado.entradas = tabla;
    estado.num = num;
}

/* ------------------------------------------------------------------ */
/* Claves derivadas y API publica                                       */
/* ------------------------------------------------------------------ */

fn clave_con_prefijo(prefijo: &[u8], nombre: &[u8], dst: &mut [u8; SAVE_CLAVE_MAX])
    -> Option<usize>
{
    let total = prefijo.len() + nombre.len();
    if total == 0 || total > SAVE_CLAVE_MAX {
        return None;
    }
    dst[..prefijo.len()].copy_from_slice(prefijo);
    dst[prefijo.len()..total].copy_from_slice(nombre);
    Some(total)
}

/// Deja la tabla como al arrancar: sin entradas y con la cabecera lista.
pub fn limpiar() {
    let estado = unsafe { &mut *SAVE.get() };
    *estado = SaveEstado::NUEVO;
    let _ = regenerar(estado);
}

/// Texto del "sector reservado". Lo usara la fase 43 para escribir a disco.
pub fn bytes() -> &'static [u8] {
    let estado = unsafe { &*SAVE.get() };
    &estado.buffer[..estado.largo]
}

/// Carga el texto leido del sector reservado. Si no es valido no cambia
/// nada: el juego sigue con la tabla que ya tenia.
pub fn cargar_bytes(datos: &[u8]) {
    let estado = unsafe { &mut *SAVE.get() };
    if datos.len() > SAVE_BUFFER_MAX {
        return;
    }
    estado.buffer[..datos.len()].copy_from_slice(datos);
    estado.largo = datos.len();
    desde_buffer(estado);
}

fn poner(estado: &mut SaveEstado, clave: &[u8], valor: i32) -> bool {
    if !clave_valida(clave) {
        return false;
    }
    if let Some(i) = buscar(estado, clave) {
        estado.entradas[i].valor = valor;
        return regenerar(estado);
    }
    if estado.num >= SAVE_CLAVES_MAX {
        return false;
    }
    let e = &mut estado.entradas[estado.num];
    e.clave[..clave.len()].copy_from_slice(clave);
    e.largo = clave.len();
    e.valor = valor;
    estado.num += 1;
    regenerar(estado)
}

fn leer(estado: &SaveEstado, clave: &[u8]) -> i32 {
    buscar(estado, clave)
        .map(|i| estado.entradas[i].valor)
        .unwrap_or(0)
}

/// Guarda una clave y devuelve 1 si se pudo escribir.
pub fn guardar(clave: &[u8], valor: i32) -> i32 {
    let estado = unsafe { &mut *SAVE.get() };
    if poner(estado, util(clave), valor) { 1 } else { 0 }
}

/// Devuelve el valor de una clave, 0 si no existe.
pub fn cargar(clave: &[u8]) -> i32 {
    let estado = unsafe { &*SAVE.get() };
    leer(estado, util(clave))
}

/// Borra todas las claves del proyecto.
pub fn borrar() -> i32 {
    let estado = unsafe { &mut *SAVE.get() };
    estado.num = 0;
    estado.entradas = [Entrada::NUEVA; SAVE_CLAVES_MAX];
    if regenerar(estado) { 1 } else { 0 }
}

/// Actualiza un record solo si se supera. Devuelve el record resultante.
pub fn record(clave: &[u8], valor: i32) -> i32 {
    let mut nombre = [0u8; SAVE_CLAVE_MAX];
    let Some(n) = clave_con_prefijo(PREFIJO_RECORD, util(clave), &mut nombre)
    else {
        return 0;
    };
    let estado = unsafe { &mut *SAVE.get() };
    let anterior = leer(estado, &nombre[..n]);
    if valor <= anterior {
        return anterior;
    }
    if poner(estado, &nombre[..n], valor) { valor } else { anterior }
}

/// Devuelve el record de una clave, 0 si no hay ninguno.
pub fn record_actual(clave: &[u8]) -> i32 {
    let mut nombre = [0u8; SAVE_CLAVE_MAX];
    let Some(n) = clave_con_prefijo(PREFIJO_RECORD, util(clave), &mut nombre)
    else {
        return 0;
    };
    let estado = unsafe { &*SAVE.get() };
    leer(estado, &nombre[..n])
}

/// Guarda un ajuste del usuario y devuelve el valor anterior.
pub fn ajuste(nombre: &[u8], valor: i32) -> i32 {
    let mut clave = [0u8; SAVE_CLAVE_MAX];
    let Some(n) = clave_con_prefijo(PREFIJO_AJUSTE, util(nombre), &mut clave)
    else {
        return 0;
    };
    let estado = unsafe { &mut *SAVE.get() };
    let anterior = leer(estado, &clave[..n]);
    if poner(estado, &clave[..n], valor) { anterior } else { anterior }
}

/// Devuelve un ajuste del usuario, 0 si no existe.
pub fn ajuste_actual(nombre: &[u8]) -> i32 {
    let mut clave = [0u8; SAVE_CLAVE_MAX];
    let Some(n) = clave_con_prefijo(PREFIJO_AJUSTE, util(nombre), &mut clave)
    else {
        return 0;
    };
    let estado = unsafe { &*SAVE.get() };
    leer(estado, &clave[..n])
}
