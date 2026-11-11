//! png.rs - lector de PNG (RFC 2083) del kernel.
//!
//! Decodifica un PNG a un mapa de pixeles `0xAARRGGBB` (alfa en 31..24)
//! listo para pintar con transparencia, igual que el lector de rocky
//! (`kernel/src/libraries/png`). Se admiten los tipos de color habituales
//! (gris, RGB, paleta, gris+alfa y RGBA) a 8 y 16 bits por canal y sin
//! entrelazar; el resto da error sin tumbar el sistema.
//!
//! La diferencia con rocky es que aqui no hay `alloc`: los cuatro buferes
//! que necesita la decodificacion los presta el llamador (`idat` para
//! concatenar los chunks, `crudo` para la salida de zlib y el
//! desfiltrado, `plte` para la paleta y `pixeles` para el resultado). El
//! desfiltrado se hace en el propio `crudo`, en el sitio, para no pedir
//! otro bufer.

use crate::inflate;

/// Errores al leer un PNG.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Error {
    /// No es un PNG o esta mal formado.
    Formato,
    /// Es un PNG valido pero de un tipo que no soportamos.
    Soporte,
    /// Los datos comprimidos estan corruptos o no caben en los buferes.
    Datos,
}

/// Cadena de 8 bytes con la que empieza todo PNG.
const FIRMA: &[u8; 8] = b"\x89PNG\r\n\x1a\n";

fn be32(b: &[u8]) -> u32 {
    ((b[0] as u32) << 24) | ((b[1] as u32) << 16) | ((b[2] as u32) << 8) | b[3] as u32
}

/// Bytes que ocupa un pixel segun el tipo de color y la profundidad.
fn bytes_por_pixel(color: u8, depth: u8) -> Option<usize> {
    Some(match (color, depth) {
        (0, 8) => 1,  // gris
        (0, 16) => 2, // gris 16 bits
        (2, 8) => 3,  // RGB
        (2, 16) => 6, // RGB 16 bits
        (3, 8) => 1,  // paleta
        (4, 8) => 2,  // gris + alfa
        (4, 16) => 4,
        (6, 8) => 4,  // RGBA
        (6, 16) => 8,
        _ => return None,
    })
}

/// Decodifica un PNG completo y devuelve `(ancho, alto)`.
///
/// Los buferes prestados: `idat` recibe los chunks `IDAT` concatenados
/// (con la salida de zlib cruda hace falta uno del tamano del PNG
/// comprimido), `crudo` la salida de zlib y el desfiltrado (necesita
/// `(ancho * bpp + 1) * alto` bytes), `plte` la paleta (768 bytes) y
/// `pixeles` el resultado (`ancho * alto` u32).
pub fn decodificar(
    d: &[u8],
    idat: &mut [u8],
    crudo: &mut [u8],
    plte: &mut [u8],
    pixeles: &mut [u32],
) -> Result<(usize, usize), Error> {
    if d.len() < 8 || &d[0..8] != FIRMA {
        return Err(Error::Formato);
    }

    let mut pos = 8usize;
    let mut ancho = 0usize;
    let mut alto = 0usize;
    let mut profundidad = 0u8;
    let mut color = 0u8;
    let mut entrelazado = 0u8;
    let mut idat_largo = 0usize;
    let mut plte_largo = 0usize;
    let mut visto_ihdr = false;

    while pos + 8 <= d.len() {
        let len = be32(&d[pos..pos + 4]) as usize;
        let tipo = &d[pos + 4..pos + 8];
        let ini = pos + 8;
        let fin = ini.checked_add(len).ok_or(Error::Formato)?;
        if fin + 4 > d.len() {
            return Err(Error::Formato);
        }
        let cuerpo = &d[ini..fin];
        match tipo {
            b"IHDR" => {
                if len < 13 {
                    return Err(Error::Formato);
                }
                ancho = be32(&cuerpo[0..4]) as usize;
                alto = be32(&cuerpo[4..8]) as usize;
                profundidad = cuerpo[8];
                color = cuerpo[9];
                if cuerpo[10] != 0 || cuerpo[11] != 0 {
                    return Err(Error::Soporte); // metodo de compresion/filtro raros
                }
                entrelazado = cuerpo[12];
                visto_ihdr = true;
            }
            b"PLTE" => {
                if plte_largo + cuerpo.len() > plte.len() {
                    return Err(Error::Datos); // paleta mas grande de lo previsto
                }
                plte[plte_largo..plte_largo + cuerpo.len()].copy_from_slice(cuerpo);
                plte_largo += cuerpo.len();
            }
            b"IDAT" => {
                if idat_largo + cuerpo.len() > idat.len() {
                    return Err(Error::Datos); // PNG comprimido mas grande de lo previsto
                }
                idat[idat_largo..idat_largo + cuerpo.len()].copy_from_slice(cuerpo);
                idat_largo += cuerpo.len();
            }
            b"IEND" => break,
            _ => {}
        }
        pos = fin + 4; // tras el CRC
    }

    if !visto_ihdr || ancho == 0 || alto == 0 {
        return Err(Error::Formato);
    }
    if entrelazado != 0 {
        return Err(Error::Soporte); // entrelazado Adam7: no soportado
    }
    let bpp = bytes_por_pixel(color, profundidad).ok_or(Error::Soporte)?;
    if color == 3 && plte_largo < 3 {
        return Err(Error::Formato);
    }
    if ancho.checked_mul(alto).ok_or(Error::Datos)? > pixeles.len() {
        return Err(Error::Datos); // no cabe en el bufer de salida
    }

    let crudo_largo = inflate::zlib(&idat[..idat_largo], crudo).map_err(|_| Error::Datos)?;
    let fila = ancho * bpp;
    let necesario = fila.checked_add(1).and_then(|v| v.checked_mul(alto)).ok_or(Error::Datos)?;
    if crudo_largo < necesario {
        return Err(Error::Datos);
    }

    desfiltrar(crudo, ancho, alto, bpp).ok_or(Error::Datos)?;
    a_rgb(
        crudo,
        ancho,
        alto,
        color,
        profundidad,
        bpp,
        &plte[..plte_largo],
        pixeles,
    )?;
    Ok((ancho, alto))
}

/// Deshace el filtrado por filas de PNG (tipos 0 a 4), en el sitio.
///
/// Cada fila llega como `[byte de filtro][pixeles de la fila]`; el
/// desfiltrado reescribe los pixeles encima de los crudos. Es seguro
/// porque cada pixel solo mira bytes ya reescritos (el de su izquierda
/// en la misma fila y los de la fila de arriba).
fn desfiltrar(datos: &mut [u8], ancho: usize, alto: usize, bpp: usize) -> Option<()> {
    let fila = ancho * bpp;
    let mut pos = 0usize; // byte de filtro de la fila actual
    for y in 0..alto {
        if pos >= datos.len() {
            return None;
        }
        let f = datos[pos];
        let base = pos + 1; // primer byte de la fila
        if base + fila > datos.len() {
            return None;
        }
        let previa = base - (fila + 1); // primer byte de la fila anterior
        for x in 0..fila {
            let raw = datos[base + x];
            let a = if x >= bpp { datos[base + x - bpp] } else { 0 };
            let b = if y > 0 { datos[previa + x] } else { 0 };
            let c = if x >= bpp && y > 0 {
                datos[previa + x - bpp]
            } else {
                0
            };
            let v = match f {
                0 => raw,
                1 => raw.wrapping_add(a),
                2 => raw.wrapping_add(b),
                3 => raw.wrapping_add(((a as u16 + b as u16) / 2) as u8),
                4 => raw.wrapping_add(paeth(a, b, c)),
                _ => return None,
            };
            datos[base + x] = v;
        }
        pos = base + fila;
    }
    Some(())
}

/// Predictor de Paeth (el filtro 4 de PNG).
fn paeth(a: u8, b: u8, c: u8) -> u8 {
    let p = a as i16 + b as i16 - c as i16;
    let pa = (p - a as i16).abs();
    let pb = (p - b as i16).abs();
    let pc = (p - c as i16).abs();
    if pa <= pb && pa <= pc {
        a
    } else if pb <= pc {
        b
    } else {
        c
    }
}

/// Convierte los pixeles ya desfiltrados a `0xAARRGGBB`, conservando el
/// alfa. El RGB se deja directo (sin componer sobre negro) para que el
/// blit con transparencia del nucleo respete las esquinas del tile.
///
/// Ojo al paso de linea: en `crudo` cada fila lleva todavia su byte de
/// filtro delante (stride = fila + 1).
fn a_rgb(
    f: &[u8],
    ancho: usize,
    alto: usize,
    color: u8,
    depth: u8,
    bpp: usize,
    plte: &[u8],
    out: &mut [u32],
) -> Result<(), Error> {
    let fila = ancho * bpp;
    for y in 0..alto {
        for x in 0..ancho {
            let p = &f[y * (fila + 1) + 1 + x * bpp..];
            let (r, g, b, a): (u8, u8, u8, u8) = match (color, depth) {
                (0, 8) => (p[0], p[0], p[0], 255),
                (0, 16) => (p[0], p[0], p[0], 255),
                (2, 8) => (p[0], p[1], p[2], 255),
                (2, 16) => (p[0], p[2], p[4], 255),
                (3, 8) => {
                    let i = p[0] as usize * 3;
                    if i + 2 >= plte.len() {
                        return Err(Error::Datos);
                    }
                    (plte[i], plte[i + 1], plte[i + 2], 255)
                }
                (4, 8) => (p[0], p[0], p[0], p[1]),
                (4, 16) => (p[0], p[0], p[0], p[2]),
                (6, 8) => (p[0], p[1], p[2], p[3]),
                (6, 16) => (p[0], p[2], p[4], p[6]),
                _ => return Err(Error::Soporte),
            };
            out[y * ancho + x] =
                ((a as u32) << 24) | ((r as u32) << 16) | ((g as u32) << 8) | b as u32;
        }
    }
    Ok(())
}
