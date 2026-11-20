//! audio.rs - beep del PC por el canal 2 del PIT (fase 39).
//!
//! El canal 0 del PIT sigue siendo del temporizador del kernel (pit.rs, a
//! 100 Hz para la IRQ0): aqui solo se toca el canal 2, que es el que llega
//! al altavoz del PC a traves del registro del sistema del puerto 0x61:
//!
//!   - bit 0: gate del canal 2,
//!   - bit 1: speaker data (deja pasar la onda del canal 2).
//!
//! No hay retardo ni bucle de espera: cada opcode programa el tono y
//! `fotograma()` solo avanza el estado y cambia o apaga la nota que toca.
//! El altavoz del PC no tiene volumen analogico, asi que el volumen
//! maestro 0..100 decide si suena (0 apaga, 1..100 habilita la nota).

use crate::cpu;
use crate::sync::SyncCell;

/// Largo del nombre del efecto guardado en el estado.
pub const AUDIO_NOMBRE_MAX: usize = 16;
/// Numero de notas de la musica "tema".
pub const AUDIO_TEMA_NOTAS: usize = 4;

/// Secuencia de "tema", una nota por tic de audio.
const TEMA: [u32; AUDIO_TEMA_NOTAS] = [523, 659, 784, 659];
const NOMBRE_MONEDA: &[u8] = b"moneda";
const NOMBRE_TEMA: &[u8] = b"tema";
/// Reloj de referencia del PIT (el mismo que usa pit.rs).
const PIT_FRECUENCIA: u32 = 1_193_182;

const PUERTO_CONTROL: u16 = 0x43;
const PUERTO_CANAL2: u16 = 0x42;
const PUERTO_ALTAVOZ: u16 = 0x61;
/// Canal 2, modo 3 (onda cuadrada), lobyte/hibyte, cuenta binaria.
const CONTROL_CANAL2: u8 = 0xB6;
/// Bits del puerto 0x61 que manda el altavoz.
const ALTAVOZ_ENCENDIDO: u8 = 0x03;

/// Un efecto suelto: nombre, frecuencia y tics que le quedan.
#[derive(Clone, Copy)]
struct Efecto {
    nombre: [u8; AUDIO_NOMBRE_MAX],
    frecuencia: u32,
    duracion: u32,
}

impl Efecto {
    const NUEVO: Efecto = Efecto {
        nombre: [0; AUDIO_NOMBRE_MAX],
        frecuencia: 0,
        duracion: 0,
    };

    fn activo(&self) -> bool {
        self.duracion > 0
    }
}

/// Musica en bucle: las notas, cuantas hay y la nota actual.
#[derive(Clone, Copy)]
struct Musica {
    notas: [u32; AUDIO_TEMA_NOTAS],
    largo: usize,
    indice: usize,
}

impl Musica {
    const NUEVA: Musica = Musica {
        notas: [0; AUDIO_TEMA_NOTAS],
        largo: 0,
        indice: 0,
    };

    fn activa(&self) -> bool {
        self.largo > 0
    }

    fn nota(&self) -> u32 {
        if self.largo == 0 {
            0
        } else {
            self.notas[self.indice % self.largo]
        }
    }
}

/// Estado global minimo del audio.
#[derive(Clone, Copy)]
struct AudioEstado {
    volumen: i32,
    tics: u64,
    efecto: Efecto,
    musica: Musica,
    /// Frecuencia ya programada en el canal 2 (0 = altavoz apagado).
    frecuencia: u32,
}

impl AudioEstado {
    const NUEVO: AudioEstado = AudioEstado {
        volumen: 100,
        tics: 0,
        efecto: Efecto::NUEVO,
        musica: Musica::NUEVA,
        frecuencia: 0,
    };
}

/* Unico estado del audio del kernel: el juego es monotarea. */
static AUDIO: SyncCell<AudioEstado> = SyncCell::new(AudioEstado::NUEVO);

/// Compara un texto de bytes (con 0 final o sin el) con un nombre ascii.
fn nombre_es(texto: &[u8], esperado: &[u8]) -> bool {
    let util = texto.iter().position(|&b| b == 0).unwrap_or(texto.len());
    util == esperado.len() && &texto[..util] == esperado
}

/// Frecuencia y duracion de un efecto por su nombre, sin acentos.
fn efecto_de_nombre(nombre: &[u8]) -> (u32, u32) {
    if nombre_es(nombre, NOMBRE_MONEDA) {
        (880, 8)
    } else {
        (440, 8)
    }
}

/// Programa el canal 2 del PIT y la puerta del altavoz. No toca el canal 0.
unsafe fn programar(frecuencia: u32) {
    if frecuencia == 0 {
        // Apaga gate y speaker data, sin molestar a los demas bits del 0x61.
        let actual = cpu::inb(PUERTO_ALTAVOZ);
        cpu::outb(PUERTO_ALTAVOZ, actual & !ALTAVOZ_ENCENDIDO);
        return;
    }
    let divisor = (PIT_FRECUENCIA / frecuencia).max(1).min(0xFFFF);
    cpu::outb(PUERTO_CONTROL, CONTROL_CANAL2);
    cpu::outb(PUERTO_CANAL2, (divisor & 0xFF) as u8);
    cpu::outb(PUERTO_CANAL2, ((divisor >> 8) & 0xFF) as u8);
    let actual = cpu::inb(PUERTO_ALTAVOZ);
    cpu::outb(PUERTO_ALTAVOZ, actual | ALTAVOZ_ENCENDIDO);
}

/// Elige la nota que suena: primero la musica, luego el efecto, y apaga
/// si no hay nada o el volumen es 0. Solo hace E/S si la nota cambio.
fn aplicar(estado: &mut AudioEstado) {
    let frecuencia = if estado.volumen == 0 {
        0
    } else if estado.musica.activa() {
        estado.musica.nota()
    } else if estado.efecto.activo() {
        estado.efecto.frecuencia
    } else {
        0
    };
    if frecuencia != estado.frecuencia {
        unsafe { programar(frecuencia) };
        estado.frecuencia = frecuencia;
    }
}

/// Deja el audio como al arrancar: sin nota, volumen al maximo y tics a 0.
pub fn limpiar() {
    unsafe {
        let estado = &mut *AUDIO.get();
        *estado = AudioEstado::NUEVO;
        programar(0);
    }
}

/// Avanza un tic de audio: la musica pasa de nota y el efecto gasta un
/// tic. No bloquea; solo reprograma el tono si ha cambiado.
pub fn fotograma() {
    let estado = unsafe { &mut *AUDIO.get() };
    estado.tics = estado.tics.wrapping_add(1);
    if estado.musica.activa() {
        estado.musica.indice = (estado.musica.indice + 1) % estado.musica.largo;
    } else if estado.efecto.activo() {
        estado.efecto.duracion -= 1;
        if estado.efecto.duracion == 0 {
            estado.efecto.nombre = [0; AUDIO_NOMBRE_MAX];
            estado.efecto.frecuencia = 0;
        }
    }
    aplicar(estado);
}

/// Suena un efecto ("moneda" a 880 Hz; cualquier otro nombre a 440 Hz)
/// durante 8 tics. El nombre puede venir con o sin 0 final.
pub fn sonar(nombre: &[u8]) {
    let (frecuencia, duracion) = efecto_de_nombre(nombre);
    let estado = unsafe { &mut *AUDIO.get() };
    let util = nombre.iter().position(|&b| b == 0).unwrap_or(nombre.len());
    let util = util.min(AUDIO_NOMBRE_MAX);
    estado.efecto.nombre = [0; AUDIO_NOMBRE_MAX];
    estado.efecto.nombre[..util].copy_from_slice(&nombre[..util]);
    estado.efecto.frecuencia = frecuencia;
    estado.efecto.duracion = duracion;
    aplicar(estado);
}

/// Pone una musica en bucle. "tema" usa la secuencia de cuatro notas;
/// cualquier otro nombre queda como un tono de 440 Hz en bucle.
pub fn musica(nombre: &[u8]) {
    let (notas, largo) = if nombre_es(nombre, NOMBRE_TEMA) {
        (TEMA, AUDIO_TEMA_NOTAS)
    } else {
        let mut otro = [0u32; AUDIO_TEMA_NOTAS];
        otro[0] = 440;
        (otro, 1)
    };
    let estado = unsafe { &mut *AUDIO.get() };
    estado.musica.notas = notas;
    estado.musica.largo = largo;
    estado.musica.indice = 0;
    aplicar(estado);
}

/// Para el efecto y la musica, y apaga el altavoz.
pub fn detener() {
    let estado = unsafe { &mut *AUDIO.get() };
    estado.efecto = Efecto::NUEVO;
    estado.musica = Musica::NUEVA;
    aplicar(estado);
}

/// Cambia el volumen maestro y devuelve el anterior (0..100).
pub fn volumen(valor: i32) -> i32 {
    let estado = unsafe { &mut *AUDIO.get() };
    let anterior = estado.volumen;
    estado.volumen = if valor < 0 { 0 } else if valor > 100 { 100 } else { valor };
    aplicar(estado);
    anterior
}

/// Devuelve el volumen maestro actual.
pub fn volumen_actual() -> i32 {
    unsafe { (*AUDIO.get()).volumen }
}
