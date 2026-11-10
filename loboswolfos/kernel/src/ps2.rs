//! Controlador PS/2 (i8042): teclado y raton.
//!
//! rocky lee la entrada por **sondeo**: el escritorio llama a `input::poll()` y
//! `mouse::poll()` en cada vuelta de su bucle de eventos, asi que no hacen falta
//! las IRQ1 ni la IRQ12. Pero eso no basta: el controlador y los dos
//! dispositivos hay que ponerlos en marcha al arrancar.
//!
//! En QEMU (SeaBIOS) el 8042 viene listo para usar, y por eso alli el teclado y
//! el raton funcionan sin tocar nada. En un PC real el firmware lo deja en un
//! estado cualquiera: es frecuente que deshabilite el reloj del teclado, que el
//! teclado se quede con el envio de datos apagado (orden 0xF5) o que este
//! esperando el parametro de una orden a medias. En esas condiciones el
//! dispositivo no emite ni un byte y el sondeo no ve absolutamente nada.
//!
//! [`init`] deja el controlador en un estado conocido:
//!
//!   1. para los dos dispositivos y vacia lo que haya pendiente,
//!   2. activa los relojes y las interrupciones del controlador,
//!   3. habilita los puertos de teclado (`0xAE`) y raton (`0xA8`),
//!   4. deja el teclado en el juego de codigos 2 y activa la traduccion del
//!      controlador (asi el kernel siempre recibe codigos set 1),
//!   5. activa el envio de datos del teclado (`0xF4`) y, si no contesta, lo
//!      reinicia (`0xFF`),
//!   6. pone el raton en sus valores por defecto (`0xF6`) y activa su envio
//!      (`0xF4`).
//!
//! Todas las esperas tienen limite: si un dispositivo no esta o no contesta, el
//! arranque sigue y el problema queda registrado en el [`Informe`] que devuelve
//! `init` (el escritorio lo imprime en la ventana Terminal).

use crate::cpu::{inb, outb};

/// Puerto de datos del controlador.
const DATOS: u16 = 0x60;
/// Puerto de estado (lectura) y de ordenes (escritura) del controlador.
const ESTADO: u16 = 0x64;

/// Limite de espera, en lecturas del puerto de estado. Cada acceso a un puerto
/// de E/S cuesta del orden de un microsegundo, asi que 200.000 son unas decimas
/// de segundo: de sobra para un dispositivo presente y una salida rapida si no
/// lo esta.
const GUARDA: u32 = 200_000;

/// Como [`GUARDA`], pero para el reinicio del teclado, que puede tardar medio
/// segundo en contestar con el resultado de su autotest.
const GUARDA_LENTA: u32 = 1_500_000;

/// Resultado de [`init`]: lo que el firmware habia dejado hecho y lo que se ha
/// conseguido. El escritorio lo imprime como diagnostico, porque en un PC real
/// la unica forma de mirarlo suele ser leer la pantalla.
#[derive(Default)]
pub struct Informe {
    /// Byte de configuracion del controlador al llegar.
    pub cfg_antes: u8,
    /// Byte de configuracion que se ha escrito.
    pub cfg_despues: u8,
    /// El teclado acepto el juego de codigos 2.
    pub teclado_set2: bool,
    /// Respuesta del teclado a la orden 0xF4 (0xFA = correcto).
    pub teclado_eco: Option<u8>,
    /// Hubo que reiniciar el teclado (no contestaba).
    pub teclado_reinicio: bool,
    /// El teclado queda en condiciones de enviar datos.
    pub teclado_ok: bool,
    /// El raton contesta a las ordenes.
    pub raton_ack: bool,
    /// Identificador que devuelve el raton (0x00 = raton estandar).
    pub raton_id: Option<u8>,
    /// Puerto de estado al terminar (bit 0 = hay un byte pendiente).
    pub estado_final: u8,
}

impl Informe {
    /// Imprime el diagnostico, una linea por dato.
    pub fn imprimir(&self) {
        crate::println!("entrada (controlador PS/2):");
        crate::println!(
            "  configuracion   0x{:02X} -> 0x{:02X}   (estado final 0x{:02X})",
            self.cfg_antes,
            self.cfg_despues,
            self.estado_final
        );
        if self.teclado_ok {
            if self.teclado_reinicio {
                crate::println!("  teclado         activo tras reiniciarlo (0xF4 = 0xFA)");
            } else {
                match self.teclado_eco {
                    Some(v) => crate::println!("  teclado         activo (0xF4 = 0x{:02X})", v),
                    None => crate::println!("  teclado         activo (0xF4 = sin dato)"),
                }
            }
        } else {
            match self.teclado_eco {
                Some(v) => crate::println!("  teclado         SIN RESPUESTA a 0xF4 (0x{:02X})", v),
                None => crate::println!("  teclado         SIN RESPUESTA a 0xF4 (sin dato)"),
            }
            crate::println!("                  el firmware no lo entrega al 8042");
            crate::println!("                  revisa el soporte de teclado en la BIOS");
        }
        if self.raton_ack {
            match self.raton_id {
                Some(v) => crate::println!("  raton           activo (identificador 0x{:02X})", v),
                None => crate::println!("  raton           activo (identificador sin dato)"),
            }
        } else {
            crate::println!("  raton           SIN RESPUESTA en el puerto auxiliar");
            crate::println!("                  (un raton USB solo aparece ahi si la");
            crate::println!("                   BIOS lo emula como PS/2)");
        }
    }
}

/// Estado del controlador (puerto 0x64).
fn estado() -> u8 {
    unsafe { inb(ESTADO) }
}

/// Espera a que el bufer de entrada del controlador quede libre.
unsafe fn esperar_entrada() -> bool {
    let mut guarda = 0u32;
    while inb(ESTADO) & 0x02 != 0 {
        guarda += 1;
        if guarda > GUARDA {
            return false;
        }
    }
    true
}

/// Envia una orden al controlador (puerto 0x64).
unsafe fn orden(cmd: u8) -> bool {
    if !esperar_entrada() {
        return false;
    }
    outb(ESTADO, cmd);
    true
}

/// Escribe un byte en el puerto de datos del controlador (0x60).
unsafe fn escribir_dato(dato: u8) -> bool {
    if !esperar_entrada() {
        return false;
    }
    outb(DATOS, dato);
    true
}

/// Lee un byte del puerto de datos, esperando (con limite) a que lo haya.
unsafe fn leer_dato_espera(guarda_max: u32) -> Option<u8> {
    let mut guarda = 0u32;
    while inb(ESTADO) & 0x01 == 0 {
        guarda += 1;
        if guarda > guarda_max {
            return None;
        }
    }
    Some(inb(DATOS))
}

/// Como [`leer_dato_espera`] con la espera normal.
unsafe fn leer_dato() -> Option<u8> {
    leer_dato_espera(GUARDA)
}

/// Descarta los bytes pendientes en el controlador.
unsafe fn drenar() {
    let mut guarda = 0u32;
    while inb(ESTADO) & 0x01 != 0 && guarda < 256 {
        let _ = inb(DATOS);
        guarda += 1;
    }
}

/// Lee el byte de configuracion del controlador (orden 0x20).
unsafe fn leer_config() -> u8 {
    if !orden(0x20) {
        return 0;
    }
    leer_dato().unwrap_or(0)
}

/// Escribe el byte de configuracion del controlador (orden 0x60).
unsafe fn escribir_config(cfg: u8) -> bool {
    orden(0x60) && escribir_dato(cfg)
}

/// Envia una orden al teclado y devuelve su primera respuesta (0xFA = ACK).
unsafe fn orden_teclado(cmd: u8) -> Option<u8> {
    if !escribir_dato(cmd) {
        return None;
    }
    leer_dato()
}

/// Envia una orden al dispositivo auxiliar (el raton) y espera su ACK.
///
/// El protocolo es el del controlador: primero la orden 0xD4 (siguiente byte
/// para el dispositivo auxiliar) y despues el byte. Si en medio aparece un
/// scancode del teclado, se descarta y se sigue esperando.
unsafe fn orden_aux(cmd: u8) -> Option<u8> {
    if !orden(0xD4) {
        return None;
    }
    if !escribir_dato(cmd) {
        return None;
    }
    for _ in 0..4 {
        match leer_dato() {
            Some(0xFA) => return Some(0xFA),
            Some(_) => continue,
            None => return None,
        }
    }
    None
}

/// Reinicia el teclado (orden 0xFF): contesta ACK (0xFA) y despues 0xAA si su
/// autotest ha ido bien. Un teclado reiniciado queda en el juego de codigos 2
/// con el envio de datos activado, que es justo lo que queremos.
unsafe fn reiniciar_teclado() -> bool {
    if !escribir_dato(0xFF) {
        return false;
    }
    for _ in 0..4 {
        match leer_dato_espera(GUARDA_LENTA) {
            Some(0xAA) => return true,
            Some(_) => continue,
            None => return false,
        }
    }
    false
}

/// Prepara el controlador y los dos dispositivos. Ver el modulo.
pub fn init() -> Informe {
    let mut inf = Informe::default();
    unsafe {
        // 1) Los dos dispositivos se quedan quietos mientras se configura.
        orden(0xAD); // teclado fuera
        orden(0xA7); // raton fuera
        drenar();

        // 2) Byte de configuracion: relojes encendidos e interrupciones
        //    habilitadas. Aunque la entrada se lea por sondeo, un 8042 con el
        //    bit 4 a 1 no deja que el teclado envie nada.
        inf.cfg_antes = leer_config();
        let mut cfg = inf.cfg_antes;
        cfg &= !0x10; // reloj del teclado habilitado
        cfg &= !0x20; // reloj del raton habilitado
        cfg |= 0x01 | 0x02; // IRQ1 (teclado) e IRQ12 (raton)
        escribir_config(cfg);
        inf.cfg_despues = cfg;

        // 3) Habilitar los dos puertos.
        orden(0xAE); // teclado
        orden(0xA8); // auxiliar (raton)

        // 4) Raton: primero se le pide que no envie datos (el firmware puede
        //    haberlo dejado enviando) y se le pone en sus valores por defecto.
        inf.raton_ack = orden_aux(0xF6).is_some();

        // 5) Teclado: juego de codigos 2 + traduccion del controlador. Es el
        //    estado canonico de un PC/AT y el que espera `input` (set 1). Si el
        //    teclado no acepta la orden, se deja el bit de traduccion como
        //    estaba para no estropear una configuracion que fuese buena.
        let a1 = orden_teclado(0xF0);
        let a2 = orden_teclado(0x02);
        inf.teclado_set2 = a1 == Some(0xFA) && a2 == Some(0xFA);
        if inf.teclado_set2 {
            cfg |= 0x40; // traduce set 2 -> set 1
            escribir_config(cfg);
            inf.cfg_despues = cfg;
        }

        // 6) Teclado: activar el envio de datos.
        inf.teclado_eco = orden_teclado(0xF4);
        inf.teclado_ok = inf.teclado_eco == Some(0xFA);
        if !inf.teclado_ok {
            // No contesta: puede estar esperando el parametro de una orden a
            // medias. Un reinicio lo devuelve a su estado por defecto.
            drenar();
            inf.teclado_reinicio = reiniciar_teclado();
            if inf.teclado_reinicio {
                cfg |= 0x40; // un teclado recien reiniciado esta en set 2
                escribir_config(cfg);
                inf.cfg_despues = cfg;
                inf.teclado_ok = true;
            }
        }

        // 7) Raton: activar el envio de datos y preguntar su identificador.
        inf.raton_ack = inf.raton_ack && orden_aux(0xF4).is_some();
        inf.raton_id = match orden_aux(0xF2) {
            Some(0xFA) => leer_dato(),
            _ => None,
        };

        // 8) Como queda el controlador (bit 0 = hay algo pendiente).
        inf.estado_final = estado();
    }
    inf
}
