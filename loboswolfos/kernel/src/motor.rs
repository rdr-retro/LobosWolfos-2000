//! motor.rs - lado Rust del nucleo del motor egine (fase 6 del PLAN.md).
//!
//! `nucleo64.s` lleva las partes esenciales en ensamblador (blits, sello
//! del tile y AABB); este modulo declara la misma interfaz que
//! `motor/motor.h` del editor: los structs en `#[repr(C)]` y las
//! funciones `extern "C"`. Encima pone envolturas seguras para que
//! `juego.rs` no tenga que tocar `unsafe` en cada llamada.
//!
//! Diferencias con el lado de 32 bits (PE/cdecl): aqui los simbolos no
//! llevan guion bajo y `MotorImagen` ocupa 16 bytes (puntero de 8 bytes
//! + ancho + alto), mientras que `MotorCelda` sigue en 12.

use core::arch::global_asm;

// El nucleo esta en sintaxis AT&T (como nucleo32.s) y global_asm! usa
// Intel por defecto: se cambia el dialecto con att_syntax.
global_asm!(include_str!("nucleo64.s"), options(att_syntax));

/* ------------------------------------------------------------------ */
/* Tipos de datos (motor.h)                                            */
/* ------------------------------------------------------------------ */

/// Imagen del juego: una capa de pixeles ya decodificada en 0xAARRGGBB.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct MotorImagen {
    /// NULL = imagen no encontrada, la celda queda hueca.
    pub pixeles: *const u32,
    pub ancho: i32,
    pub alto: i32,
}

/// Celda del mapa: coordenada de rejilla e indice en el array de
/// imagenes del proyecto.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct MotorCelda {
    pub x: i32,
    pub y: i32,
    pub imagen: i32,
}

/// Mapa cargado de un .mav: rejilla de celdas con su lado y las imagenes
/// deduplicadas que usan.
#[repr(C)]
pub struct MotorMapa {
    pub celdas: *mut MotorCelda,
    pub num_celdas: i32,
    /// tile_size del .mav.
    pub lado: i32,
    pub imagenes: *mut MotorImagen,
    pub num_imagenes: i32,
}

/* El puerto a 64 bits tiene que respetar los tamanos que nucleo64.s
 * asume al recorrer los arrays (stride de celdas y de imagenes). */
const _: () = {
    assert!(core::mem::size_of::<MotorImagen>() == 16);
    assert!(core::mem::size_of::<MotorCelda>() == 12);
};

/* ------------------------------------------------------------------ */
/* Entrada empaquetada en bits (motor.h)                                */
/* ------------------------------------------------------------------ */

pub const MOTOR_ENT_IZQUIERDA: u32 = 0x01;
pub const MOTOR_ENT_DERECHA: u32 = 0x02;
pub const MOTOR_ENT_ARRIBA: u32 = 0x04;
pub const MOTOR_ENT_ABAJO: u32 = 0x08;
pub const MOTOR_ENT_SALTO: u32 = 0x10;
pub const MOTOR_ENT_CERRAR: u32 = 0x20;

/* ------------------------------------------------------------------ */
/* Declaracion del nucleo (nucleo64.s)                                  */
/* ------------------------------------------------------------------ */

extern "C" {
    fn motor_limpiar(dst: *mut u32, ancho: i32, alto: i32, color: u32);
    fn motor_blit(dst: *mut u32, dst_w: i32, dst_h: i32, dx: i32, dy: i32,
                  src: *const u32, src_w: i32, src_h: i32);
    fn motor_blit_alpha(dst: *mut u32, dst_w: i32, dst_h: i32, dx: i32,
                        dy: i32, src: *const u32, src_w: i32, src_h: i32);
    fn motor_mapa_render(dst: *mut u32, dst_w: i32, dst_h: i32, cam_x: i32,
                         cam_y: i32, lado: i32, celdas: *const MotorCelda,
                         num_celdas: i32, imagenes: *const MotorImagen);
    fn motor_aabb(ax: i32, ay: i32, aw: i32, ah: i32,
                  bx: i32, by: i32, bw: i32, bh: i32) -> i32;
}

/* ------------------------------------------------------------------ */
/* Envolturas seguras                                                   */
/* ------------------------------------------------------------------ */

/// Rellena el framebuffer con un color solido.
pub fn limpiar(dst: *mut u32, ancho: i32, alto: i32, color: u32) {
    unsafe { motor_limpiar(dst, ancho, alto, color) }
}

/// Blit opaco: copia la imagen entera en (dx, dy), con recorte.
pub fn blit(dst: *mut u32, dst_w: i32, dst_h: i32, dx: i32, dy: i32,
            src: *const u32, src_w: i32, src_h: i32) {
    unsafe { motor_blit(dst, dst_w, dst_h, dx, dy, src, src_w, src_h) }
}

/// Blit con transparencia: igual que blit pero saltando los pixeles
/// con alpha 0.
pub fn blit_alpha(dst: *mut u32, dst_w: i32, dst_h: i32, dx: i32, dy: i32,
                  src: *const u32, src_w: i32, src_h: i32) {
    unsafe { motor_blit_alpha(dst, dst_w, dst_h, dx, dy, src, src_w, src_h) }
}

/// Render del mapa: celdas visibles con el tile estampado escalado y
/// centrado en su celda (el mismo sello que el editor de mapas).
pub fn mapa_render(dst: *mut u32, dst_w: i32, dst_h: i32, cam_x: i32,
                   cam_y: i32, mapa: &MotorMapa) {
    unsafe {
        motor_mapa_render(dst, dst_w, dst_h, cam_x, cam_y, mapa.lado,
                          mapa.celdas, mapa.num_celdas, mapa.imagenes)
    }
}

/// Colision AABB: true si los rectangulos se solapan.
pub fn aabb(ax: i32, ay: i32, aw: i32, ah: i32,
            bx: i32, by: i32, bw: i32, bh: i32) -> bool {
    unsafe { motor_aabb(ax, ay, aw, ah, bx, by, bw, bh) != 0 }
}
