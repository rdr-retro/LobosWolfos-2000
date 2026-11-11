//! inflate.rs - descompresor DEFLATE (RFC 1951) y zlib (RFC 1950).
//!
//! Un PNG guarda los pixeles comprimidos: los chunks `IDAT` contienen un
//! flujo zlib (2 bytes de cabecera, un flujo DEFLATE y un `adler32` al
//! final). Este descompresor es el mismo "puff" de rocky
//! (`kernel/src/libraries/inflate`), portado a loboswolfos sin `alloc`:
//! el kernel no tiene heap, asi que la salida se escribe en un slice
//! prestado por el llamador y las tablas de Huffman son arrays fijos.
//! Correcto antes que rapido.

/// Errores de descompresion.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Error {
    /// El flujo esta corrupto, truncado o no cabe en la salida.
    Datos,
}

type Resultado<T> = core::result::Result<T, Error>;

/// Lector de bits en orden LSB-primero, como exige DEFLATE.
struct Bits<'a> {
    d: &'a [u8],
    i: usize,
    b: u32,
    n: u32,
}

impl<'a> Bits<'a> {
    fn new(d: &'a [u8]) -> Bits<'a> {
        Bits {
            d,
            i: 0,
            b: 0,
            n: 0,
        }
    }

    /// Lee `n` bits (n <= 24) en orden LSB-primero.
    fn bits(&mut self, n: u32) -> Resultado<u32> {
        while self.n < n {
            if self.i >= self.d.len() {
                return Err(Error::Datos);
            }
            self.b |= (self.d[self.i] as u32) << self.n;
            self.i += 1;
            self.n += 8;
        }
        let v = self.b & ((1u32 << n) - 1);
        self.b >>= n;
        self.n -= n;
        Ok(v)
    }

    fn bit(&mut self) -> Resultado<u32> {
        self.bits(1)
    }

    /// Descartar los bits que sobran de la ultima muestra parcial, para
    /// volver a un limite de byte (lo necesitan los bloques sin comprimir).
    fn alinear(&mut self) {
        let r = self.n % 8;
        self.b >>= r;
        self.n -= r;
    }
}

/// Un arbol de Huffman canonico listo para decodificar.
///
/// Sin `alloc`: `symbol` es un array fijo (288 es el maximo de simbolos
/// de literales/longitudes) y `simbolos` dice cuantos estan en uso.
struct Huffman {
    count: [u16; 16],
    symbol: [u16; 288],
    simbolos: usize,
}

/// Construye un arbol a partir de las longitudes de codigo de cada simbolo
/// (0 = simbolo ausente). Es el algoritmo de `puff.c` (zlib).
fn construir(lens: &[u8]) -> Huffman {
    let mut count = [0u16; 16];
    for &l in lens {
        count[l as usize] += 1;
    }
    count[0] = 0; // los simbolos de longitud cero no cuentan
    let mut offs = [0u16; 16];
    let mut total = 0u16;
    for len in 1..16 {
        offs[len] = total;
        total += count[len];
    }
    let mut symbol = [0u16; 288];
    for (sym, &l) in lens.iter().enumerate() {
        if l != 0 {
            symbol[offs[l as usize] as usize] = sym as u16;
            offs[l as usize] += 1;
        }
    }
    Huffman {
        count,
        symbol,
        simbolos: total as usize,
    }
}

/// Decodifica un simbolo leyendo bits hasta completar un codigo.
fn decodificar(bits: &mut Bits, h: &Huffman) -> Resultado<u16> {
    let mut codigo: i32 = 0;
    let mut primero: i32 = 0;
    let mut indice: i32 = 0;
    for len in 1..=15 {
        codigo |= bits.bit()? as i32;
        let cuenta = h.count[len] as i32;
        if codigo - primero < cuenta {
            let pos = (indice + (codigo - primero)) as usize;
            if pos >= h.simbolos {
                return Err(Error::Datos);
            }
            return Ok(h.symbol[pos]);
        }
        indice += cuenta;
        primero = (primero + cuenta) << 1;
        codigo <<= 1;
    }
    Err(Error::Datos)
}

// Tablas del RFC 1951 para las longitudes y distancias.
const LARGO_BASE: [u16; 29] = [
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131,
    163, 195, 227, 258,
];
const LARGO_EXTRA: [u32; 29] = [
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
];
const DIST_BASE: [u16; 30] = [
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537,
    2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
];
const DIST_EXTRA: [u32; 30] = [
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13,
    13,
];

/// Tablas fijas de Huffman (bloques de tipo 1).
fn tablas_fijas() -> (Huffman, Huffman) {
    let mut lit = [0u8; 288];
    for l in lit.iter_mut().take(144) {
        *l = 8;
    }
    for l in lit.iter_mut().take(256).skip(144) {
        *l = 9;
    }
    for l in lit.iter_mut().take(280).skip(256) {
        *l = 7;
    }
    for l in lit.iter_mut().take(288).skip(280) {
        *l = 8;
    }
    let dist = [5u8; 30];
    (construir(&lit), construir(&dist))
}

/// Tablas dinamicas (bloques de tipo 2).
fn tablas_dinamicas(bits: &mut Bits) -> Resultado<(Huffman, Huffman)> {
    let hlit = bits.bits(5)? as usize + 257;
    let hdist = bits.bits(5)? as usize + 1;
    let hclen = bits.bits(4)? as usize + 4;
    if hlit > 286 || hdist > 30 {
        return Err(Error::Datos);
    }

    const ORDEN: [usize; 19] = [
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
    ];
    let mut cl = [0u8; 19];
    for i in 0..hclen {
        cl[ORDEN[i]] = bits.bits(3)? as u8;
    }
    let hcl = construir(&cl);

    let mut lens = [0u8; 288 + 30];
    let mut i = 0usize;
    while i < hlit + hdist {
        let sym = decodificar(bits, &hcl)?;
        match sym {
            0..=15 => {
                lens[i] = sym as u8;
                i += 1;
            }
            16 => {
                if i == 0 {
                    return Err(Error::Datos);
                }
                let prev = lens[i - 1];
                let rep = 3 + bits.bits(2)? as usize;
                for _ in 0..rep {
                    if i >= lens.len() {
                        return Err(Error::Datos);
                    }
                    lens[i] = prev;
                    i += 1;
                }
            }
            17 => {
                let rep = 3 + bits.bits(3)? as usize;
                for _ in 0..rep {
                    if i >= lens.len() {
                        return Err(Error::Datos);
                    }
                    lens[i] = 0;
                    i += 1;
                }
            }
            18 => {
                let rep = 11 + bits.bits(7)? as usize;
                for _ in 0..rep {
                    if i >= lens.len() {
                        return Err(Error::Datos);
                    }
                    lens[i] = 0;
                    i += 1;
                }
            }
            _ => return Err(Error::Datos),
        }
    }
    if lens[256] == 0 {
        return Err(Error::Datos);
    }
    let lit = construir(&lens[..hlit]);
    let dist = construir(&lens[hlit..hlit + hdist]);
    Ok((lit, dist))
}

/// Expande los simbolos de un bloque hasta el codigo de fin (256).
///
/// La diferencia con rocky: aqui la salida es un slice prestado y `pos`
/// va marcando cuanto se ha escrito (en vez de un `Vec` que crece).
fn expandir(
    bits: &mut Bits,
    lit: &Huffman,
    dist: &Huffman,
    salida: &mut [u8],
    pos: &mut usize,
) -> Resultado<()> {
    loop {
        let sym = decodificar(bits, lit)?;
        if sym < 256 {
            if *pos >= salida.len() {
                return Err(Error::Datos);
            }
            salida[*pos] = sym as u8;
            *pos += 1;
        } else if sym == 256 {
            return Ok(());
        } else {
            let s = sym as usize - 257;
            if s >= LARGO_BASE.len() {
                return Err(Error::Datos);
            }
            let largo = LARGO_BASE[s] as usize + bits.bits(LARGO_EXTRA[s])? as usize;

            let d = decodificar(bits, dist)? as usize;
            if d >= DIST_BASE.len() {
                return Err(Error::Datos);
            }
            let distancia = DIST_BASE[d] as usize + bits.bits(DIST_EXTRA[d])? as usize;
            if distancia == 0 || distancia > *pos {
                return Err(Error::Datos);
            }
            // Copia byte a byte: asi funciona tambien cuando se solapan (una
            // distancia menor que el largo repite el patron que acaba de salir).
            let inicio = *pos - distancia;
            for k in 0..largo {
                if *pos >= salida.len() {
                    return Err(Error::Datos);
                }
                salida[*pos] = salida[inicio + k];
                *pos += 1;
            }
        }
    }
}

/// Descomprime un flujo DEFLATE en crudo en `salida` y devuelve los bytes
/// escritos.
pub fn inflar(d: &[u8], salida: &mut [u8]) -> Resultado<usize> {
    let mut bits = Bits::new(d);
    let mut pos = 0usize;
    loop {
        let ultimo = bits.bit()?;
        let tipo = bits.bits(2)?;
        match tipo {
            0 => {
                bits.alinear();
                let len = bits.bits(16)? as usize;
                let nlen = bits.bits(16)? as usize;
                if len != (!nlen & 0xFFFF) {
                    return Err(Error::Datos);
                }
                for _ in 0..len {
                    if pos >= salida.len() {
                        return Err(Error::Datos);
                    }
                    salida[pos] = bits.bits(8)? as u8;
                    pos += 1;
                }
            }
            1 => {
                let (lit, dist) = tablas_fijas();
                expandir(&mut bits, &lit, &dist, salida, &mut pos)?;
            }
            2 => {
                let (lit, dist) = tablas_dinamicas(&mut bits)?;
                expandir(&mut bits, &lit, &dist, salida, &mut pos)?;
            }
            _ => return Err(Error::Datos),
        }
        if ultimo == 1 {
            break;
        }
    }
    Ok(pos)
}

/// Descomprime un flujo zlib (el que usan los `IDAT` de un PNG) en `salida`
/// y devuelve los bytes escritos.
pub fn zlib(d: &[u8], salida: &mut [u8]) -> Resultado<usize> {
    if d.len() < 2 {
        return Err(Error::Datos);
    }
    let cmf = d[0];
    let flg = d[1];
    if cmf & 0x0F != 8 {
        return Err(Error::Datos); // metodo de compresion distinto de DEFLATE
    }
    if ((cmf as u16) << 8 | flg as u16) % 31 != 0 {
        return Err(Error::Datos); // checksum de cabecera incorrecto
    }
    if flg & 0x20 != 0 {
        return Err(Error::Datos); // diccionario predefinido: no soportado
    }
    inflar(&d[2..], salida)
}
