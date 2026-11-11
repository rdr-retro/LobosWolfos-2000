//! activo.rs - carga del proyecto del juego sobre loboswolfos (fase 6).
//!
//! Es el puerto del lado de activos de `motor/activo.c` (el que usa
//! `game.exe`), adaptado al kernel: aqui no hay sistema de archivos ni
//! heap. El proyecto entero viaja en la "zona del juego" del USB (LBA
//! 2048 en adelante) como un blob LGB1 (el mismo formato que se pega al
//! final de `game.exe`), asi:
//!
//!   [entrada 0][entrada 1]...["LGB1" u32 largo_total]
//!
//! Cada entrada es `u32 largo_nombre, u32 largo_datos, nombre, datos`.
//! El constructor (loboswolfos/xtask) lo rellena con el `.logic` del
//! gestor, los `.mav` de `maps\` y los tiles PNG.
//!
//! Memoria: el kernel ocupa [1, 3) MiB y la pila [3, 4) MiB (ver
//! kernel.ld). El juego usa la RAM que hay encima, que bootusb2 deja
//! identidad-mapeada, a base de direcciones fijas como hace bios.rs con
//! su buzon:
//!
//!   4 MiB   copia del blob leida del disco (1 MiB)
//!   5 MiB   rascaderos del decodificador PNG (IDAT, crudo, paleta)
//!   6 MiB   pixeles decodificados de los tiles (2 MiB, a repartir)
//!
//! Las celdas, las imagenes y sus nombres viven en `.bss` (rangos fijos
//! de `motor.h`: 8192 celdas y 256 imagenes).

use crate::bios::{self, Regs};
use crate::motor::{MotorCelda, MotorImagen, MotorMapa};
use crate::println;
use crate::sync::SyncCell;

/* ------------------------------------------------------------------ */
/* Zona del juego en disco                                             */
/* ------------------------------------------------------------------ */

/// Boot info que deja el bloque de arranque en 0x7E00 al saltar al
/// kernel (ver bootusb2.s):
///   +0x00 u32 magia "CORK"   +0x04 u32 unidad de arranque (DL)
///   +0x08 u32 LBA del juego  +0x0C u32 sectores de la zona
const BOOT_INFO: usize = 0x7E00;
const BOOT_MAGIA: u32 = 0x4B524F43; // "CORK" en little-endian

/// DAP del INT 13h extendido y bufer de rebote, en memoria baja. El
/// trampolin baja a modo real con DS=0, asi que la DAP vive por debajo
/// de 64 KiB (va en SI). gfx.rs usa 0x8000/0x8300 para el VBE.
const DAP_ADDR: usize = 0x9000;
const REBOTE_ADDR: usize = 0x9100;
const SECTOR: usize = 512;

/// Copia del blob en RAM (por encima de la pila del kernel).
const BLOB_ADDR: usize = 0x400000;
const BLOB_MAX: usize = 0x100000;

/// Rascaderos del decodificador PNG.
const IDAT_ADDR: usize = 0x500000;
const IDAT_MAX: usize = 0x80000;
const CRUDO_ADDR: usize = 0x580000;
const CRUDO_MAX: usize = 0x80000;
const PLTE_ADDR: usize = 0x5F0000;
const PLTE_MAX: usize = 768;

/// Pixeles decodificados de los tiles, repartidos en orden de llegada.
const PIXELES_ADDR: usize = 0x600000;
const PIXELES_MAX: usize = 0x200000;

/// Limites de motor.h.
pub const CELDAS_MAX: usize = 8192;
pub const IMAGENES_MAX: usize = 256;
const NOMBRE_MAX: usize = 64;

static CELDAS: SyncCell<[MotorCelda; CELDAS_MAX]> =
    SyncCell::new([MotorCelda { x: 0, y: 0, imagen: 0 }; CELDAS_MAX]);
static IMAGENES: SyncCell<[MotorImagen; IMAGENES_MAX]> = SyncCell::new(
    [MotorImagen { pixeles: core::ptr::null(), ancho: 0, alto: 0 }; IMAGENES_MAX],
);
/// Nombres de los tiles cargados, para deduplicar (como el array de
/// nombres de activo.c).
static NOMBRES: SyncCell<[[u8; NOMBRE_MAX]; IMAGENES_MAX]> = SyncCell::new([[0; NOMBRE_MAX]; IMAGENES_MAX]);

/// Siguiente hueco libre en la zona de pixeles de los tiles.
static PIXELES_SIGUIENTE: SyncCell<usize> = SyncCell::new(PIXELES_ADDR);

fn rd32(addr: usize) -> u32 {
    unsafe { core::ptr::read_volatile(addr as *const u32) }
}

fn wr8(addr: usize, v: u8) {
    unsafe { core::ptr::write_volatile(addr as *mut u8, v) }
}

fn wr16(addr: usize, v: u16) {
    unsafe { core::ptr::write_volatile(addr as *mut u16, v) }
}

fn wr64(addr: usize, v: u64) {
    unsafe { core::ptr::write_volatile(addr as *mut u64, v) }
}

/* ------------------------------------------------------------------ */
/* Lectura de la zona del juego con el BIOS                            */
/* ------------------------------------------------------------------ */

/// Lee un sector del disco de arranque al bufer de rebote (memoria baja)
/// con el INT 13h extendido (AH=42h) y devuelve false si el BIOS falla.
fn leer_sector(unidad: u8, lba: u32) -> bool {
    // DAP: tamano, reservado, sectores, offset, segmento, LBA de 64 bits.
    wr8(DAP_ADDR, 16);
    wr8(DAP_ADDR + 1, 0);
    wr16(DAP_ADDR + 2, 1);
    wr16(DAP_ADDR + 4, (REBOTE_ADDR & 0x0F) as u16);
    wr16(DAP_ADDR + 6, (REBOTE_ADDR >> 4) as u16);
    wr64(DAP_ADDR + 8, lba as u64);

    let r = bios::int13(Regs {
        eax: 0x4200,
        edx: unidad as u32,
        esi: DAP_ADDR as u32,
        ..Default::default()
    });
    !r.carry && (r.eax >> 8) as u8 == 0
}

/// Lee la zona del juego del disco y la deja copiada en RAM.
///
/// El blob termina con su pie de 8 bytes ("LGB1" + largo total) y el
/// constructor lo rellena con ceros hasta un multiplo de sector, asi que
/// el pie acaba siempre justo al final de un sector: se lee sector a
/// sector y se para cuando el pie cuadra con lo llevado leido.
fn cargar_blob() -> Option<&'static [u8]> {
    if rd32(BOOT_INFO) != BOOT_MAGIA {
        println!("egine: sin bloque de arranque (magia CORK).");
        return None;
    }
    let unidad = rd32(BOOT_INFO + 4) as u8;
    let lba = rd32(BOOT_INFO + 8);
    let sectores = rd32(BOOT_INFO + 0xC);
    if sectores == 0 {
        println!("egine: el disco no tiene zona de juego.");
        return None;
    }
    println!(
        "egine: zona del juego en LBA {} ({} sectores, unidad 0x{:02X}).",
        lba, sectores, unidad
    );

    let dst = BLOB_ADDR as *mut u8;
    let mut leidos = 0usize;
    let mut sector = 0u32;
    while sector < sectores && leidos + SECTOR <= BLOB_MAX {
        if !leer_sector(unidad, lba + sector) {
            println!("egine: fallo de disco al leer el sector {}.", sector);
            return None;
        }
        unsafe {
            core::ptr::copy_nonoverlapping(REBOTE_ADDR as *const u8, dst.add(leidos), SECTOR);
        }
        leidos += SECTOR;
        sector += 1;

        // Pie del blob al final del sector recien leido.
        if leidos >= 8 {
            let pie = unsafe { core::slice::from_raw_parts(dst.add(leidos - 8), 8) };
            let magia = u32::from_le_bytes([pie[0], pie[1], pie[2], pie[3]]);
            let largo = u32::from_le_bytes([pie[4], pie[5], pie[6], pie[7]]);
            if magia == 0x3142474C && largo as usize == leidos {
                // "LGB1"
                let blob = unsafe { core::slice::from_raw_parts(dst, leidos) };
                println!("egine: blob del juego: {} bytes.", largo);
                return Some(blob);
            }
        }
    }
    println!("egine: la zona de juego no termina en un blob LGB1.");
    None
}

/* ------------------------------------------------------------------ */
/* Entradas del blob                                                   */
/* ------------------------------------------------------------------ */

/// Busca una entrada del blob por nombre (primero la ruta completa y
/// luego el nombre base, como activo.c) y devuelve sus datos.
fn buscar_entrada<'a>(blob: &'a [u8], nombre: &str) -> Option<&'a [u8]> {
    let mut pos = 0usize;
    while pos + 8 <= blob.len() {
        let ln = u32::from_le_bytes([blob[pos], blob[pos + 1], blob[pos + 2], blob[pos + 3]])
            as usize;
        let ld = u32::from_le_bytes([blob[pos + 4], blob[pos + 5], blob[pos + 6], blob[pos + 7]])
            as usize;
        if pos + 8 + ln + ld > blob.len() {
            break;
        }
        let entrada_nombre = &blob[pos + 8..pos + 8 + ln];
        let datos = &blob[pos + 8 + ln..pos + 8 + ln + ld];
        if entrada_nombre == nombre.as_bytes() {
            return Some(datos);
        }
        // nombre base: lo que va tras la ultima barra (normal o invertida)
        if let Some(base) = nombre.rfind(['/', '\\']) {
            if entrada_nombre == nombre[base + 1..].as_bytes() {
                return Some(datos);
            }
        }
        pos += 8 + ln + ld;
    }
    None
}

/* ------------------------------------------------------------------ */
/* Parseo de los archivos virtuales (estilo Lua del editor)            */
/* ------------------------------------------------------------------ */

/// `clave = 123` (o con espacios, dos puntos...) en un texto Lua-like.
fn extraer_entero(texto: &[u8], clave: &str) -> Option<i32> {
    let clave = clave.as_bytes();
    let mut pos = buscar(texto, clave)? + clave.len();
    while pos < texto.len() && matches!(texto[pos], b' ' | b'=' | b'\t' | b':') {
        pos += 1;
    }
    let mut negativo = false;
    if pos < texto.len() && texto[pos] == b'-' {
        negativo = true;
        pos += 1;
    }
    let mut valor: i32 = 0;
    let mut digitos = 0;
    while pos < texto.len() && texto[pos].is_ascii_digit() {
        valor = valor * 10 + (texto[pos] - b'0') as i32;
        pos += 1;
        digitos += 1;
    }
    if digitos == 0 {
        return None;
    }
    Some(if negativo { -valor } else { valor })
}

/// `clave = "texto"`: lo que va entre la primera pareja de comillas que
/// sigue a la clave.
fn extraer_cadena<'a>(texto: &'a [u8], clave: &str) -> Option<&'a [u8]> {
    let clave = clave.as_bytes();
    let mut pos = buscar(texto, clave)? + clave.len();
    while pos < texto.len() && texto[pos] != b'"' {
        pos += 1;
    }
    pos += 1;
    let inicio = pos;
    while pos < texto.len() && texto[pos] != b'"' {
        pos += 1;
    }
    if pos >= texto.len() {
        return None;
    }
    Some(&texto[inicio..pos])
}

/// Busca una subcadena byte a byte (un strstr de andar por casa).
fn buscar(texto: &[u8], aguja: &[u8]) -> Option<usize> {
    if aguja.is_empty() || texto.len() < aguja.len() {
        return None;
    }
    (0..=texto.len() - aguja.len()).find(|&i| &texto[i..i + aguja.len()] == aguja)
}

/// Lo que va entre la primera pareja de comillas de una linea.
fn entre_comillas(linea: &[u8]) -> Option<&[u8]> {
    let q1 = linea.iter().position(|&b| b == b'"')?;
    let q2 = linea.iter().skip(q1 + 1).position(|&b| b == b'"')? + q1 + 1;
    Some(&linea[q1 + 1..q2])
}

/// Pega `prefijo/nombre` en un bufer fijo y lo devuelve como &str (la
/// version sin alloc de unir_ruta de activo.c).
fn unir_ruta<'a>(bufer: &'a mut [u8], prefijo: &str, nombre: &str) -> &'a str {
    let mut n = prefijo.len().min(bufer.len());
    bufer[..n].copy_from_slice(&prefijo.as_bytes()[..n]);
    if n < bufer.len() {
        bufer[n] = b'/';
        n += 1;
    }
    let m = nombre.len().min(bufer.len() - n);
    bufer[n..n + m].copy_from_slice(&nombre.as_bytes()[..m]);
    core::str::from_utf8(&bufer[..n + m]).unwrap_or("")
}

/// Busca un nombre en el array (deduplicacion) o lo guarda en el primer
/// hueco libre. Devuelve su indice.
fn registrar_nombre(nombre: &str) -> Option<usize> {
    let nombres = unsafe { &mut *NOMBRES.get() };
    let bytes = nombre.as_bytes();
    let n = bytes.len().min(NOMBRE_MAX - 1);
    for i in 0..IMAGENES_MAX {
        let fin = nombres[i].iter().position(|&b| b == 0).unwrap_or(NOMBRE_MAX);
        if fin == 0 {
            nombres[i][..n].copy_from_slice(&bytes[..n]);
            return Some(i); // hueco libre: registrado
        }
        if fin == n && &nombres[i][..n] == &bytes[..n] {
            return Some(i); // ya estaba
        }
    }
    None
}

/// El mapa que toca usar: el primero del proyecto `.logic` (pantallas del
/// juego) y, si no hay proyecto, `maps/mapa.mav` (el mismo criterio que
/// activo.c).
fn buscar_mapa(blob: &[u8]) -> Option<(&[u8], &'static str)> {
    if let Some(logic) = buscar_entrada(blob, "proyecto.logic") {
        // Formato del gestor: "LOGIC1", pan, numero de elementos y una
        // linea por elemento: x y an alto id plegado "mapa" funcion.
        // La pantalla del juego es el primer elemento con mapa.
        let mut clave_buf = [0u8; 96];
        let mut linea_inicio = 0usize;
        let mut lineas = 0usize;
        for i in 0..=logic.len() {
            if i == logic.len() || logic[i] == b'\n' {
                if lineas >= 3 {
                    if let Some(mapa) = entre_comillas(&logic[linea_inicio..i]) {
                        if let Ok(nombre) = core::str::from_utf8(mapa) {
                            let clave = unir_ruta(&mut clave_buf, "maps", nombre);
                            if let Some(datos) = buscar_entrada(blob, clave) {
                                println!("egine: mapa del proyecto: {}.", clave);
                                return Some((datos, "proyecto.logic"));
                            }
                        }
                    }
                }
                linea_inicio = i + 1;
                lineas += 1;
            }
        }
        println!("egine: el proyecto no apunta a ningun mapa del blob.");
    }
    let datos = buscar_entrada(blob, "maps/mapa.mav")?;
    println!("egine: mapa por defecto: maps/mapa.mav.");
    Some((datos, "maps/mapa.mav"))
}

/* ------------------------------------------------------------------ */
/* Mapa .mav                                                           */
/* ------------------------------------------------------------------ */

/// Carga el mapa y sus tiles. Devuelve el MotorMapa listo para el nucleo
/// (las celdas en .bss, los pixeles en la arena de arriba).
pub fn cargar(blob: &[u8]) -> Option<MotorMapa> {
    let (mav, origen) = buscar_mapa(blob)?;

    let mut lado = extraer_entero(mav, "tile_size").unwrap_or(32);
    lado = lado.clamp(4, 256);
    let tiles_dir = extraer_cadena(mav, "tiles_dir").unwrap_or(b"tiles");
    let tiles_dir = core::str::from_utf8(tiles_dir).unwrap_or("tiles");

    let celdas = unsafe { &mut *CELDAS.get() };
    let imagenes = unsafe { &mut *IMAGENES.get() };
    let mut num_celdas = 0usize;
    let mut num_imagenes = 0usize;

    // Bloques "{ x = .., y = .., image = ".." }" dentro de tiles = { ... }.
    let inicio_tiles = buscar(mav, b"tiles")?;
    let mut p = buscar(&mav[inicio_tiles..], b"{")? + inicio_tiles;
    loop {
        let Some(cierre) = buscar(&mav[p..], b"}") else {
            break;
        };
        let bloque = &mav[p..p + cierre + 1];
        if buscar(bloque, b"image").is_some() {
            if let (Some(x), Some(y), Some(nombre)) = (
                extraer_entero(bloque, "x"),
                extraer_entero(bloque, "y"),
                extraer_cadena(bloque, "image"),
            ) {
                if let Ok(nombre) = core::str::from_utf8(nombre) {
                    // deduplicar por nombre; si es nuevo, decodificarlo
                    let idx = match registrar_nombre(nombre) {
                        Some(i) if i < num_imagenes => Some(i),
                        Some(i) => {
                            if cargar_imagen(blob, tiles_dir, nombre, &mut imagenes[i]) {
                                num_imagenes = i + 1;
                                Some(i)
                            } else {
                                None // celda hueca
                            }
                        }
                        None => None,
                    };
                    if let Some(idx) = idx {
                        if num_celdas < CELDAS_MAX {
                            celdas[num_celdas] = MotorCelda { x, y, imagen: idx as i32 };
                            num_celdas += 1;
                        }
                    }
                }
            }
        }
        p += cierre + 1;
        match buscar(&mav[p..], b"{") {
            Some(rel) => p += rel,
            None => break,
        }
    }

    println!(
        "egine: mapa {}: {} celdas, {} tiles, lado {}.",
        origen, num_celdas, num_imagenes, lado
    );
    if num_celdas == 0 {
        return None;
    }

    Some(MotorMapa {
        celdas: celdas.as_mut_ptr(),
        num_celdas: num_celdas as i32,
        lado,
        imagenes: imagenes.as_mut_ptr(),
        num_imagenes: num_imagenes as i32,
    })
}

/// Decodifica un tile del blob a la arena de pixeles y rellena la
/// MotorImagen (pixeles a NULL si algo falla: la celda queda hueca).
fn cargar_imagen(blob: &[u8], tiles_dir: &str, nombre: &str, img: &mut MotorImagen) -> bool {
    let mut ruta_buf = [0u8; 96];
    let ruta = unir_ruta(&mut ruta_buf, tiles_dir, nombre);
    let Some(png) = buscar_entrada(blob, ruta).or_else(|| buscar_entrada(blob, nombre)) else {
        println!("egine: el tile {} no esta en el blob.", nombre);
        return false;
    };

    let idat = unsafe { core::slice::from_raw_parts_mut(IDAT_ADDR as *mut u8, IDAT_MAX) };
    let crudo = unsafe { core::slice::from_raw_parts_mut(CRUDO_ADDR as *mut u8, CRUDO_MAX) };
    let plte = unsafe { core::slice::from_raw_parts_mut(PLTE_ADDR as *mut u8, PLTE_MAX) };

    // Hueco de los pixeles en la arena, alineado a 4.
    let Some((ancho, alto)) = extraer_dimensiones(png) else {
        println!("egine: el tile {} no parece un PNG.", nombre);
        return false;
    };
    let pixeles_n = ancho as usize * alto as usize;
    let cursor = unsafe { &mut *PIXELES_SIGUIENTE.get() };
    let inicio = (*cursor + 3) & !3;
    let fin = inicio + pixeles_n * 4;
    if fin > PIXELES_ADDR + PIXELES_MAX {
        println!("egine: no queda arena para el tile {}.", nombre);
        return false;
    }
    let pixeles = unsafe { core::slice::from_raw_parts_mut(inicio as *mut u32, pixeles_n) };

    match crate::png::decodificar(png, idat, crudo, plte, pixeles) {
        Ok((w, h)) => {
            img.pixeles = pixeles.as_ptr();
            img.ancho = w as i32;
            img.alto = h as i32;
            *cursor = fin;
            println!("egine: tile {} decodificado ({}x{}).", nombre, w, h);
            true
        }
        Err(e) => {
            println!("egine: el tile {} no se pudo decodificar ({:?}).", nombre, e);
            false
        }
    }
}

/// Ancho y alto de un PNG (del IHDR) sin decodificarlo, para reservar el
/// hueco justo antes de gastar los rascaderos.
fn extraer_dimensiones(png: &[u8]) -> Option<(u32, u32)> {
    if png.len() < 24 || &png[0..8] != b"\x89PNG\r\n\x1a\n" {
        return None;
    }
    if &png[12..16] != b"IHDR" {
        return None;
    }
    let ancho = u32::from_be_bytes([png[16], png[17], png[18], png[19]]);
    let alto = u32::from_be_bytes([png[20], png[21], png[22], png[23]]);
    if ancho == 0 || alto == 0 || ancho > 2048 || alto > 2048 {
        return None;
    }
    Some((ancho, alto))
}

/// Lee la zona de juego del disco (blob LGB1) y carga el mapa.
///
/// Devuelve el mapa montado o None (con el motivo en la consola).
pub fn cargar_todo() -> Option<MotorMapa> {
    let blob = cargar_blob()?;
    cargar(blob)
}
