//! Raton PS/2 colgado del puerto auxiliar del controlador 8042.
//!
//! rocky lo usa por **sondeo**: el escritorio grafico llama a [`poll`] en cada
//! vuelta de su bucle de eventos, de modo que no hace falta programar la IRQ12
//! ni el PIC esclavo. El movimiento se acumula en `dx`/`dy` (en el sistema de
//! pantalla, con el eje Y creciendo hacia abajo) y el estado de los botones en
//! `botones` (bit 0 = izquierdo, bit 1 = derecho, bit 2 = central).
//!
//! Aqui solo vive el montaje de los paquetes de 3 bytes; el controlador en si
//! (habilitar el puerto auxiliar, activar el envio de datos...) se prepara en
//! [`crate::ps2`], junto con el teclado.

use crate::cpu::inb;
use crate::sync::SyncCell;

/// Tamano de la pantalla del juego: el cursor se limita a esta caja.
const PANTALLA_ANCHO: i32 = 640;
const PANTALLA_ALTO: i32 = 480;

/// Estado interno del raton.
struct Estado {
    dx: i32,
    dy: i32,
    botones: u8,
    /// Posicion absoluta del cursor (fase 37), centrada al arrancar.
    x: i32,
    y: i32,
    /// Pasos de rueda pendientes (fase 37).
    rueda: i32,
    /// 1 si el ultimo movimiento dejo el cursor dentro de la pantalla.
    dentro: bool,
    paquete: [u8; 3],
    indice: usize,
}

impl Estado {
    const fn new() -> Self {
        Estado {
            dx: 0,
            dy: 0,
            botones: 0,
            x: PANTALLA_ANCHO / 2,
            y: PANTALLA_ALTO / 2,
            rueda: 0,
            dentro: true,
            paquete: [0; 3],
            indice: 0,
        }
    }
}

static ESTADO: SyncCell<Estado> = SyncCell::new(Estado::new());

fn estado() -> &'static mut Estado {
    // Seguro: rocky es mononucleo y monotarea; no hay dos referencias vivas a
    // la vez porque cada funcion es de corta duracion.
    unsafe { &mut *ESTADO.get() }
}

/// Introduce un byte del raton (lo llama el sondeo del PS/2).
pub fn alimentar(byte: u8) {
    let st = estado();
    match st.indice {
        0 => {
            // El primer byte del paquete siempre lleva el bit 3 a 1.
            if byte & 0x08 != 0 {
                st.paquete[0] = byte;
                st.indice = 1;
            }
        }
        1 => {
            st.paquete[1] = byte;
            st.indice = 2;
        }
        _ => {
            st.paquete[2] = byte;
            st.indice = 0;
            aplicar(st);
        }
    }
}

fn aplicar(st: &mut Estado) {
    let b0 = st.paquete[0];
    if b0 & 0xC0 != 0 {
        return; // desbordamiento de movimiento: se descarta el paquete
    }
    let mut dx = st.paquete[1] as u16;
    if b0 & 0x10 != 0 {
        dx |= 0xFF00; // extension de signo
    }
    let mut dy = st.paquete[2] as u16;
    if b0 & 0x20 != 0 {
        dy |= 0xFF00;
    }
    let mov_x = dx as i16 as i32;
    // El eje Y del raton crece hacia arriba; `wrapping_neg` evita un desborde
    // con i16::MIN (un movimiento imposible, pero el kernel no puede panicar).
    let mov_y = (dy as i16).wrapping_neg() as i32;

    let nx = st.x + mov_x;
    let ny = st.y + mov_y;
    st.dentro = nx >= 0 && nx < PANTALLA_ANCHO && ny >= 0 && ny < PANTALLA_ALTO;
    st.x = nx.clamp(0, PANTALLA_ANCHO - 1);
    st.y = ny.clamp(0, PANTALLA_ALTO - 1);

    st.dx += mov_x;
    st.dy += mov_y;
    st.botones = b0 & 0x07;
}

/// Vacia los bytes del raton pendientes en el controlador (sin tocar el teclado).
pub fn poll() {
    unsafe {
        loop {
            let st = inb(0x64);
            if st & 0x01 == 0 {
                break;
            }
            if st & 0x20 == 0 {
                break; // es del teclado: lo deja para `input`
            }
            let byte = inb(0x60);
            alimentar(byte);
        }
    }
}

/// Devuelve el movimiento acumulado desde la ultima llamada y los botones.
pub fn tomar() -> (i32, i32, u8) {
    let st = estado();
    let r = (st.dx, st.dy, st.botones);
    st.dx = 0;
    st.dy = 0;
    r
}

/* ------------------------------------------------------------------ */
/* Estado que ven los scripts (fase 37)                                 */
/* ------------------------------------------------------------------ */

/// Posicion absoluta del cursor dentro de la pantalla del juego.
pub fn posicion() -> (i32, i32) {
    let st = estado();
    (st.x, st.y)
}

/// Botones pulsados ahora (bit 0 izquierdo, 1 derecho, 2 central).
pub fn botones() -> u8 {
    estado().botones
}

/// 1 si el cursor esta dentro de la pantalla del juego.
pub fn dentro() -> bool {
    estado().dentro
}

/// Pasos de rueda pendientes; se borran al leer, como en `tomar`.
pub fn tomar_rueda() -> i32 {
    let st = estado();
    let r = st.rueda;
    st.rueda = 0;
    r
}

/// Anade pasos de rueda (lo llamara el driver que los descifre).
pub fn alimentar_rueda(pasos: i32) {
    estado().rueda = estado().rueda.wrapping_add(pasos);
}
