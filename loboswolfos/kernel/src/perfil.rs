//! perfil.rs - perfilador y presupuesto del espejo BIOS (fase 42).
//!
//! Es el reflejo de `motor/eventos.c` y `motor/juego.c` en el kernel: el
//! reloj lo da el PIT (100 Hz, 10 ms por tic = 10000 us), las instrucciones
//! las cuenta `lobo.rs` y el aviso de presupuesto se escribe por serie de
//! la consola. No hay heap ni alloc: los contadores viven en una SyncCell
//! de tamano fijo y no cambian el resultado de ningun script.
//!
//! El perfilador arranca desactivado: sin `activar()` no se lee el PIT ni
//! se cuenta nada, igual que en Windows sin --perfil.

use crate::pit;
use crate::println;
use crate::sync::SyncCell;

/// 60 fotogramas por segundo, en microsegundos (el mismo valor que
/// `--presupuesto` por defecto de game.exe).
pub const PRESUPUESTO_US: u64 = 16_667;

/// Una serie por script del proyecto (activo::USADAS_MAX).
const SERIES_MAX: usize = 128;

/// Contadores de una serie (un script de codigo.lobo).
#[derive(Clone, Copy)]
struct Serie {
    instrucciones: u64,
    disparos: u64,
}

impl Serie {
    const NUEVA: Serie = Serie {
        instrucciones: 0,
        disparos: 0,
    };
}

/// Estado global del perfilador.
struct Datos {
    activo: bool,
    t0: u64,
    ins0: u64,
    serie: usize,
    num_series: usize,
    instrucciones: u64,
    ins_foto: u64,
    us_foto: u64,
    us_total: u64,
    us_max: u64,
    fotogramas: u64,
    avisos: u64,
    series: [Serie; SERIES_MAX],
}

impl Datos {
    const NUEVO: Datos = Datos {
        activo: false,
        t0: 0,
        ins0: 0,
        serie: 0,
        num_series: 0,
        instrucciones: 0,
        ins_foto: 0,
        us_foto: 0,
        us_total: 0,
        us_max: 0,
        fotogramas: 0,
        avisos: 0,
        series: [Serie::NUEVA; SERIES_MAX],
    };
}

static DATOS: SyncCell<Datos> = SyncCell::new(Datos::NUEVO);

/// Microsegundos transcurridos desde el arranque. Un tic del PIT son
/// 10 ms; se satura para no desbordar en sesiones muy largas.
pub fn ahora_us() -> u64 {
    pit::ticks().saturating_mul(10_000)
}

/// Engancha el perfilador. Equivale a `--perfil` en game.exe: solo aqui
/// se empiezan a contar instrucciones y a mirar el reloj.
pub fn activar() {
    let d = unsafe { &mut *DATOS.get() };
    *d = Datos::NUEVO;
    d.activo = true;
}

pub fn activo() -> bool {
    unsafe { (*DATOS.get()).activo }
}

/// Cuenta una instruccion de la VM. Lo llama `lobo::ejecutar` una vez por
/// opcode; no altera la maquina ni los valores de los scripts.
pub fn contar_instruccion() {
    let d = unsafe { &mut *DATOS.get() };
    if !d.activo {
        return;
    }
    d.instrucciones = d.instrucciones.saturating_add(1);
    d.series[d.serie].instrucciones = d.series[d.serie].instrucciones.saturating_add(1);
}

/// Selecciona la serie del script que va a correr a continuacion y cuenta
/// un disparo. Lo llama el bucle de `juego::actualizar` con el indice del
/// boton FUNCIONES.
pub fn serie(i: usize) {
    let d = unsafe { &mut *DATOS.get() };
    if !d.activo {
        return;
    }
    let i = if i < SERIES_MAX { i } else { SERIES_MAX - 1 };
    d.serie = i;
    if i + 1 > d.num_series {
        d.num_series = i + 1;
    }
    d.series[i].disparos = d.series[i].disparos.saturating_add(1);
}

/// Marca el comienzo del fotograma, antes de despachar los scripts.
pub fn iniciar_fotograma() {
    let d = unsafe { &mut *DATOS.get() };
    if !d.activo {
        return;
    }
    d.t0 = ahora_us();
    d.ins0 = d.instrucciones;
    d.serie = 0;
}

/// Cierra el fotograma: totales, maximo y aviso si se paso el presupuesto.
/// La degradacion no se aplica aqui: el BIOS no tiene un presupuesto de
/// blits ajustable (no hay cache de trajes), asi que solo avisa.
pub fn fin_fotograma() {
    let d = unsafe { &mut *DATOS.get() };
    if !d.activo {
        return;
    }
    let us = ahora_us().saturating_sub(d.t0);
    d.ins_foto = d.instrucciones.saturating_sub(d.ins0);
    d.us_foto = us;
    d.fotogramas = d.fotogramas.saturating_add(1);
    d.us_total = d.us_total.saturating_add(us);
    if us > d.us_max {
        d.us_max = us;
    }
    if us > PRESUPUESTO_US {
        d.avisos = d.avisos.saturating_add(1);
        println!(
            "egine: presupuesto excedido: {} us en el fotograma {}.",
            us, d.fotogramas
        );
    }
}

/// Informe final por serie, con los mismos conceptos que
/// `--volcar-perfil` de game.exe: tiempos, presupuesto, memoria del blob
/// y del runtime (celdas y tiles del mapa cargado), y una linea por script.
pub fn informar(celdas: i32, tiles: i32) {
    let d = unsafe { &mut *DATOS.get() };
    if !d.activo {
        return;
    }
    let medios = if d.fotogramas > 0 {
        d.us_total / d.fotogramas
    } else {
        0
    };
    println!(
        "egine: perfil: fotogramas {} instrucciones {} us totales {} us medios {} us maximos {} avisos {}.",
        d.fotogramas, d.instrucciones, d.us_total, medios, d.us_max, d.avisos
    );
    println!(
        "egine: presupuesto {} us; ultimo fotograma {} instrucciones {} us.",
        PRESUPUESTO_US, d.ins_foto, d.us_foto
    );

    let blob = crate::activo::blob_cargado().map_or(0, |b| b.len());
    let celdas_bytes = (celdas.max(0) as usize).saturating_mul(core::mem::size_of::<crate::motor::MotorCelda>());
    let tiles_bytes = (tiles.max(0) as usize).saturating_mul(core::mem::size_of::<crate::motor::MotorImagen>());
    println!(
        "egine: memoria: blob {} celdas {} tiles {} runtime {}.",
        blob, celdas, tiles, blob.saturating_add(celdas_bytes).saturating_add(tiles_bytes)
    );

    for i in 0..d.num_series {
        println!(
            "egine: script {} instrucciones {} disparos {}.",
            i, d.series[i].instrucciones, d.series[i].disparos
        );
    }
}
