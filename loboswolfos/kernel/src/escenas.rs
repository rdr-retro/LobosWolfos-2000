//! escenas.rs - escenas multiples, transiciones, camara y efectos (fase 41).
//!
//! Espejo del estado `EscenasRuntime` de `motor/juego.c`. Los builtins de
//! `lobo.rs` escriben aqui y el juego lee el estado al cerrar cada
//! fotograma: aplica el cambio de escena a mitad de la transicion, mueve la
//! camara con suavizado entero y dibuja los overlays de efectos.
//!
//! No hay CLI en la BIOS, asi que el sistema esta siempre disponible: un
//! proyecto antiguo no llama a estos builtins y no cambia nada.

use crate::sync::SyncCell;

/* Campo de camara, el mismo que motor/lobo.h. */
pub const CAM_OBJETIVO: i32 = 0;
pub const CAM_ZOOM: i32 = 1;
pub const CAM_LIMITES: i32 = 2;
pub const CAM_SUAVIZAR: i32 = 3;

/* Campo de efecto, el mismo que motor/lobo.h. */
pub const EF_TRANSICION: i32 = 0;
pub const EF_PARPADEO: i32 = 1;
pub const EF_TEMBLOR: i32 = 2;
pub const EF_FLASH: i32 = 3;
pub const EF_FADE: i32 = 4;

#[derive(Clone, Copy)]
pub struct EscenasEstado {
    pub inicial: i32,
    pub actual: i32,
    pub cambios: i32,
    pub reinicios: i32,
    pendiente: i32,
    pub objetivo: i32,
    pub zoom: i32,
    pub limites: [i32; 4],
    pub suave: i32,
    cam_listo: bool,
    cam_x: i32,
    cam_y: i32,
    trans_activo: bool,
    pub trans_tipo: i32,
    pub trans_duracion: i32,
    pub trans_transcurrido: i32,
    pub trans_fase: i32,
    pub parpadeo_id: i32,
    pub parpadeo_periodo: i32,
    pub temblor_intensidad: i32,
    pub temblor_restantes: i32,
    pub flash_color: i32,
    pub flash_restantes: i32,
    flash_duracion: i32,
    pub fade_color: i32,
    pub fade_alpha: i32,
    pub fade_restantes: i32,
    reinicio_pedido: bool,
}

impl EscenasEstado {
    const NUEVO: EscenasEstado = EscenasEstado {
        inicial: 1,
        actual: 1,
        cambios: 0,
        reinicios: 0,
        pendiente: 0,
        objetivo: 0,
        zoom: 1,
        limites: [0; 4],
        suave: 0,
        cam_listo: false,
        cam_x: 0,
        cam_y: 0,
        trans_activo: false,
        trans_tipo: 0,
        trans_duracion: 0,
        trans_transcurrido: 0,
        trans_fase: 0,
        parpadeo_id: 0,
        parpadeo_periodo: 0,
        temblor_intensidad: 0,
        temblor_restantes: 0,
        flash_color: 0,
        flash_restantes: 0,
        flash_duracion: 0,
        fade_color: 0,
        fade_alpha: 0,
        fade_restantes: 0,
        reinicio_pedido: false,
    };
}

static ESTADO: SyncCell<EscenasEstado> = SyncCell::new(EscenasEstado::NUEVO);

/// Deja el estado como al arrancar (lo llama juego::arrancar).
pub fn limpiar() {
    unsafe { *ESTADO.get() = EscenasEstado::NUEVO };
}

/// Copia del estado para el juego (el dibujo no comparte prestamos).
pub fn estado() -> EscenasEstado {
    unsafe { *ESTADO.get() }
}

/// `cambiar_escena(id)`: guarda la peticion; el juego la aplica al cerrar el
/// fotograma (o a mitad de la transicion).
pub fn cambiar_escena(id: i32) -> i32 {
    let e = unsafe { &mut *ESTADO.get() };
    if id <= 0 {
        return 0;
    }
    e.pendiente = id;
    1
}

/// `escena_actual()`.
pub fn escena_actual() -> i32 {
    unsafe { (*ESTADO.get()).actual }
}

/// `camara(campo, a, b, c, d)`: devuelve el valor anterior de objetivo, zoom
/// o suavizado, y 0 para limites (igual que el motor C).
pub fn camara(campo: i32, a: i32, b: i32, c: i32, d: i32) -> i32 {
    let e = unsafe { &mut *ESTADO.get() };
    let anterior;
    match campo {
        CAM_OBJETIVO => {
            anterior = e.objetivo;
            e.objetivo = a;
        }
        CAM_ZOOM => {
            anterior = e.zoom;
            e.zoom = if a == 2 || a == 4 { a } else { 1 };
        }
        CAM_LIMITES => {
            e.limites = [a, b, c, d];
            return 0;
        }
        CAM_SUAVIZAR => {
            anterior = e.suave;
            e.suave = if a > 0 { a } else { 0 };
            e.cam_listo = false;
        }
        _ => return 0,
    }
    anterior
}

/// `efecto(campo, a, b, c, d)`: el orden de argumentos es el del builtin
/// (`transicion(tipo, fotogramas)`, `fade(color, alpha, fotogramas)`, ...).
pub fn efecto(campo: i32, a: i32, b: i32, c: i32, _d: i32) -> i32 {
    let e = unsafe { &mut *ESTADO.get() };
    match campo {
        EF_TRANSICION => {
            let n = b.max(0).min(100000);
            e.trans_activo = n > 0;
            e.trans_tipo = a;
            e.trans_duracion = n;
            e.trans_transcurrido = 0;
            e.trans_fase = 1;
        }
        EF_PARPADEO => {
            e.parpadeo_id = a;
            e.parpadeo_periodo = if b > 0 { b } else { 1 };
        }
        EF_TEMBLOR => {
            e.temblor_intensidad = a.max(0);
            e.temblor_restantes = b.max(0).min(100000);
        }
        EF_FLASH => {
            e.flash_color = a;
            e.flash_duracion = b.max(0).min(100000);
            e.flash_restantes = e.flash_duracion;
        }
        EF_FADE => {
            e.fade_color = a;
            e.fade_alpha = b.max(0).min(255);
            e.fade_restantes = c.max(0).min(100000);
        }
        _ => {}
    }
    0
}

/// `reiniciar_escena()` (opcode 49, compartido con el motor C).
pub fn reiniciar() -> i32 {
    let e = unsafe { &mut *ESTADO.get() };
    e.reinicio_pedido = true;
    0
}

/// `true` si un script pidio reiniciar; el juego lo acepta al cerrar el
/// fotograma, cuando ya no queda ningun script mirando el mapa viejo.
pub fn reinicio_pedido() -> bool {
    unsafe { (*ESTADO.get()).reinicio_pedido }
}

pub fn reinicio_aceptado() {
    let e = unsafe { &mut *ESTADO.get() };
    e.reinicio_pedido = false;
    e.reinicios += 1;
    e.pendiente = 0;
    e.parpadeo_id = 0;
    e.parpadeo_periodo = 0;
    e.temblor_intensidad = 0;
    e.temblor_restantes = 0;
    e.flash_restantes = 0;
    e.fade_restantes = 0;
    e.cam_listo = false;
}

/// Avanza efectos y transiciones. Devuelve el id de la escena que hay que
/// cargar en este fotograma, o None si no hay cambio pendiente.
pub fn fotograma() -> Option<i32> {
    let e = unsafe { &mut *ESTADO.get() };
    if e.temblor_restantes > 0 {
        e.temblor_restantes -= 1;
    }
    if e.flash_restantes > 0 {
        e.flash_restantes -= 1;
    }
    if e.fade_restantes > 0 {
        e.fade_restantes -= 1;
    }
    if !e.trans_activo {
        let id = e.pendiente;
        e.pendiente = 0;
        if id != 0 {
            e.actual = id;
            e.cambios += 1;
            return Some(id);
        }
        return None;
    }
    if e.trans_duracion <= 0 {
        e.trans_activo = false;
        return None;
    }
    e.trans_transcurrido += 1;
    let mitad = (e.trans_duracion + 1) / 2;
    let mut id = 0;
    if e.trans_fase == 1 && e.trans_transcurrido >= mitad {
        id = e.pendiente;
        e.pendiente = 0;
        e.trans_fase = 2;
        if id != 0 {
            e.actual = id;
            e.cambios += 1;
        }
    }
    if e.trans_transcurrido >= e.trans_duracion {
        e.trans_activo = false;
    }
    if id != 0 {
        Some(id)
    } else {
        None
    }
}

/// Camara suavizada con pasos enteros (solo si `suave > 1`).
pub fn suavizar(cam_x: i32, cam_y: i32) -> (i32, i32) {
    let e = unsafe { &mut *ESTADO.get() };
    if e.suave <= 1 {
        return (cam_x, cam_y);
    }
    if !e.cam_listo {
        e.cam_x = cam_x;
        e.cam_y = cam_y;
        e.cam_listo = true;
        return (cam_x, cam_y);
    }
    let dx = cam_x - e.cam_x;
    let dy = cam_y - e.cam_y;
    if dx != 0 {
        let ad = dx.abs();
        let mut paso = ad / e.suave;
        if ad % e.suave != 0 {
            paso += 1;
        }
        if paso < 1 {
            paso = 1;
        }
        e.cam_x += if dx > 0 { paso } else { -paso };
        if (dx > 0 && e.cam_x > cam_x) || (dx < 0 && e.cam_x < cam_x) {
            e.cam_x = cam_x;
        }
    }
    if dy != 0 {
        let ad = dy.abs();
        let mut paso = ad / e.suave;
        if ad % e.suave != 0 {
            paso += 1;
        }
        if paso < 1 {
            paso = 1;
        }
        e.cam_y += if dy > 0 { paso } else { -paso };
        if (dy > 0 && e.cam_y > cam_y) || (dy < 0 && e.cam_y < cam_y) {
            e.cam_y = cam_y;
        }
    }
    (e.cam_x, e.cam_y)
}

/// Desplazamiento deterministico del temblor para un fotograma.
pub fn temblor(fotograma: i32) -> (i32, i32) {
    let e = unsafe { *ESTADO.get() };
    if e.temblor_restantes <= 0 || e.temblor_intensidad <= 0 {
        return (0, 0);
    }
    let radio = e.temblor_intensidad.min(32);
    let rx = (fotograma.wrapping_mul(37).wrapping_add(11))
        .rem_euclid(2 * radio + 1) - radio;
    let ry = (fotograma.wrapping_mul(53).wrapping_add(7))
        .rem_euclid(2 * radio + 1) - radio;
    (rx, ry)
}

/// Opacidad 0..255 del flash (decae desde el primer fotograma).
pub fn flash_alpha() -> i32 {
    let e = unsafe { *ESTADO.get() };
    if e.flash_restantes <= 0 || e.flash_duracion <= 0 {
        return 0;
    }
    (255 * e.flash_restantes / e.flash_duracion).min(255)
}

/// Opacidad 0..255 de la transicion (sube cubriendo, baja descubriendo).
pub fn transicion_alpha() -> i32 {
    let e = unsafe { *ESTADO.get() };
    if !e.trans_activo || e.trans_duracion <= 0 {
        return 0;
    }
    let mitad = (e.trans_duracion + 1) / 2;
    if e.trans_fase == 1 {
        return (255 * e.trans_transcurrido / mitad).min(255);
    }
    let parte = e.trans_duracion - mitad;
    if parte <= 0 {
        return 0;
    }
    (255 * (e.trans_duracion - e.trans_transcurrido) / parte).min(255)
}
