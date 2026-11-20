//! biosent.rs - entorno del subconjunto BIOS (fase 43).
//!
//! Estado compartido por los scripts que corren en la VM resumible:
//! entidades, colisiones, eventos, variables globales y trajes. Es la
//! contraparte `no_std` de motor/entidades.c, motor/eventos.c y
//! motor/colisiones.c, reducida al subconjunto que puede arrancar desde la
//! BIOS (ver BIOS_SUBCONJUNTO.md en la raiz del proyecto).
//!
//! Todo vive en `static` de tamano fijo: no hay heap en el kernel. Los
//! programas que piden una capacidad mayor reciben un valor neutro y el
//! editor avisa al compilar la imagen, asi que ningun indice puede salirse
//! de su array.

use crate::gfx;
use crate::input;
use crate::sync::SyncCell;

/* ------------------------------------------------------------------ */
/* Limites                                                             */
/* ------------------------------------------------------------------ */

/// Entidades vivas a la vez (incluye el jugador, que es la id 1).
pub const ENTIDADES_MAX: usize = 64;
/// Variables por entidad.
pub const ENTIDAD_VARS_MAX: usize = 64;
/// Variables globales compartidas por todos los scripts.
pub const GLOBALES_MAX: usize = 128;
/// Eventos pendientes en la cola.
pub const EVENTOS_MAX: usize = 64;
/// Eventos que se repiten cada fotograma, declarados por el editor.
pub const EVENTOS_DECLARADOS_MAX: usize = 64;

/* Tipos de evento, los mismos que logic3.h (L3_EVENTO_*). */
pub const EV_INICIO: i32 = 1;
pub const EV_FOTOGRAMA: i32 = 2;
pub const EV_TECLA: i32 = 3;
pub const EV_RECIBIR: i32 = 4;
pub const EV_TOCAR: i32 = 5;
pub const EV_CADA: i32 = 6;
pub const EV_CLONAR: i32 = 7;
pub const EV_ESCENA: i32 = 8;

/* Campos de `mi`, los mismos que motor/lobo.h. */
pub const MI_X: i32 = 0;
pub const MI_Y: i32 = 1;
pub const MI_VIDA: i32 = 2;
pub const MI_VISIBLE: i32 = 3;
pub const MI_TRAJE: i32 = 4;
pub const MI_ID: i32 = 5;
pub const MI_ESCENA: i32 = 6;
pub const MI_ANCHO: i32 = 7;
pub const MI_ALTO: i32 = 8;
pub const MI_CAPA: i32 = 9;
pub const MI_VARS: i32 = 1000;

/* ------------------------------------------------------------------ */
/* Entidades                                                           */
/* ------------------------------------------------------------------ */

#[derive(Clone, Copy)]
pub struct Entidad {
    pub id: i32,
    pub tag: [u8; 16],
    pub tag_largo: usize,
    pub x: i32,
    pub y: i32,
    pub w: i32,
    pub h: i32,
    pub visible: bool,
    pub capa: i32,
    pub traje: i32,
    pub vida: i32,
    pub escena: i32,
    pub vars: [i32; ENTIDAD_VARS_MAX],
}

impl Entidad {
    const NUEVA: Entidad = Entidad {
        id: 0,
        tag: [0; 16],
        tag_largo: 0,
        x: 0,
        y: 0,
        w: 24,
        h: 24,
        visible: true,
        capa: 0,
        traje: 0,
        vida: 1,
        escena: 1,
        vars: [0; ENTIDAD_VARS_MAX],
    };

    /// Compara la etiqueta con un texto sin el cero final.
    fn tag_es(&self, texto: &[u8]) -> bool {
        self.tag_largo == texto.len() && &self.tag[..self.tag_largo] == texto
    }
}

static ENTIDADES: SyncCell<[Entidad; ENTIDADES_MAX]> =
    SyncCell::new([Entidad::NUEVA; ENTIDADES_MAX]);
static NUM_ENTIDADES: SyncCell<usize> = SyncCell::new(0);
static SIGUIENTE_ID: SyncCell<i32> = SyncCell::new(2);

/// Globales compartidas: las leen y escriben todos los scripts.
static GLOBALES: SyncCell<[i32; GLOBALES_MAX]> = SyncCell::new([0; GLOBALES_MAX]);

/// Variables de la entidad 1 (el jugador), usadas por `mi.variable` cuando
/// el script no ha creado ninguna entidad propia.
static VARS_JUGADOR: SyncCell<[i32; ENTIDAD_VARS_MAX]> =
    SyncCell::new([0; ENTIDAD_VARS_MAX]);

/// Numero de entidades vivas.
pub fn cantidad() -> usize {
    unsafe { *NUM_ENTIDADES.get() }
}

/// Deja el entorno como al arrancar. La entidad 1 es el jugador, que el
/// juego mueve tambien desde la posicion del sprite.
pub fn limpiar(x: i32, y: i32) {
    let entidades = unsafe { &mut *ENTIDADES.get() };
    for e in entidades.iter_mut() {
        *e = Entidad::NUEVA;
    }
    entidades[0].id = 1;
    entidades[0].x = x;
    entidades[0].y = y;
    unsafe {
        *NUM_ENTIDADES.get() = 1;
        *SIGUIENTE_ID.get() = 2;
        *GLOBALES.get() = [0; GLOBALES_MAX];
        *VARS_JUGADOR.get() = [0; ENTIDAD_VARS_MAX];
    }
    limpiar_eventos();
}

/// Busca una entidad por id y devuelve su indice en el array.
fn indice(id: i32) -> Option<usize> {
    let n = cantidad();
    let entidades = unsafe { &*ENTIDADES.get() };
    for i in 0..n {
        if entidades[i].id == id {
            return Some(i);
        }
    }
    None
}

/// Crea una entidad con una etiqueta, devuelve su id o -1 si no cabe.
pub fn crear(tag: &[u8], x: i32, y: i32) -> i32 {
    let n = cantidad();
    if n >= ENTIDADES_MAX {
        return -1;
    }
    let id = unsafe { *SIGUIENTE_ID.get() };
    unsafe { *SIGUIENTE_ID.get() = id.wrapping_add(1) };
    let entidades = unsafe { &mut *ENTIDADES.get() };
    let e = &mut entidades[n];
    *e = Entidad::NUEVA;
    e.id = id;
    let largo = tag.len().min(e.tag.len());
    e.tag[..largo].copy_from_slice(&tag[..largo]);
    e.tag_largo = largo;
    e.x = x;
    e.y = y;
    unsafe { *NUM_ENTIDADES.get() = n + 1 };
    emitir(EV_CLONAR, id, -1, id);
    id
}

/// Borra una entidad. La id 1 (jugador) no se puede borrar: el juego
/// siempre necesita un sprite al que seguir con la camara.
pub fn borrar(id: i32) -> bool {
    if id == 1 {
        return false;
    }
    let Some(i) = indice(id) else {
        return false;
    };
    let n = cantidad();
    let entidades = unsafe { &mut *ENTIDADES.get() };
    for j in i..n - 1 {
        entidades[j] = entidades[j + 1];
    }
    unsafe { *NUM_ENTIDADES.get() = n - 1 };
    true
}

/// Mueve una entidad a una posicion absoluta.
pub fn mover(id: i32, x: i32, y: i32) -> bool {
    let Some(i) = indice(id) else {
        return false;
    };
    let entidades = unsafe { &mut *ENTIDADES.get() };
    entidades[i].x = x;
    entidades[i].y = y;
    true
}

/// Sincroniza la posicion del jugador con la del sprite del juego.
pub fn jugador_mover(x: i32, y: i32) {
    let entidades = unsafe { &mut *ENTIDADES.get() };
    if cantidad() > 0 {
        entidades[0].x = x;
        entidades[0].y = y;
    }
}

/// Lee un campo de `mi`/`caja_de`. Devuelve 0 si la entidad no existe.
pub fn leer(id: i32, campo: i32) -> i32 {
    if campo >= MI_VARS {
        let slot = campo - MI_VARS;
        if id == 1 || indice(id).is_none() {
            let vars = unsafe { &*VARS_JUGADOR.get() };
            return vars.get(slot as usize).copied().unwrap_or(0);
        }
        let entidades = unsafe { &*ENTIDADES.get() };
        let i = indice(id).unwrap();
        return entidades[i].vars.get(slot as usize).copied().unwrap_or(0);
    }
    let Some(i) = indice(id) else {
        return 0;
    };
    let entidades = unsafe { &*ENTIDADES.get() };
    let e = &entidades[i];
    match campo {
        MI_X => e.x,
        MI_Y => e.y,
        MI_VIDA => e.vida,
        MI_VISIBLE => e.visible as i32,
        MI_TRAJE => e.traje,
        MI_ID => e.id,
        MI_ESCENA => e.escena,
        MI_ANCHO => e.w,
        MI_ALTO => e.h,
        MI_CAPA => e.capa,
        _ => 0,
    }
}

/// Escribe un campo de `mi`. Devuelve 1 si la entidad existe.
pub fn escribir(id: i32, campo: i32, valor: i32) -> i32 {
    if campo >= MI_VARS {
        let slot = campo - MI_VARS;
        if slot < 0 || slot >= ENTIDAD_VARS_MAX as i32 {
            return 0;
        }
        let i = if id == 1 { Some(0) } else { indice(id) };
        let Some(i) = i else {
            return 0;
        };
        if i == 0 {
            let vars = unsafe { &mut *VARS_JUGADOR.get() };
            vars[slot as usize] = valor;
        } else {
            let entidades = unsafe { &mut *ENTIDADES.get() };
            entidades[i].vars[slot as usize] = valor;
        }
        return 1;
    }
    let i = if id == 1 { Some(0) } else { indice(id) };
    let Some(i) = i else {
        return 0;
    };
    let entidades = unsafe { &mut *ENTIDADES.get() };
    let e = &mut entidades[i];
    match campo {
        MI_X => e.x = valor,
        MI_Y => e.y = valor,
        MI_VIDA => e.vida = valor,
        MI_VISIBLE => e.visible = valor != 0,
        MI_TRAJE => e.traje = valor,
        MI_ESCENA => e.escena = valor,
        MI_ANCHO => e.w = valor.max(1),
        MI_ALTO => e.h = valor.max(1),
        MI_CAPA => e.capa = valor,
        _ => return 0,
    }
    1
}

/* ------------------------------------------------------------------ */
/* Colisiones                                                          */
/* ------------------------------------------------------------------ */

/// Distancia Manhattan entre las esquinas superiores izquierdas.
pub fn distancia(a: i32, b: i32) -> i32 {
    let (Some(i), Some(j)) = (indice(a), indice(b)) else {
        return -1;
    };
    let entidades = unsafe { &*ENTIDADES.get() };
    (entidades[i].x - entidades[j].x).abs() + (entidades[i].y - entidades[j].y).abs()
}

/// Comprueba si dos cajas AABB se tocan.
fn solapan(ax: i32, ay: i32, aw: i32, ah: i32, bx: i32, by: i32, bw: i32, bh: i32) -> bool {
    ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah
}

/// `toca(id)`: 1 si la entidad actual toca a la otra.
pub fn toca(actual: i32, id: i32) -> i32 {
    let (Some(i), Some(j)) = (indice(actual), indice(id)) else {
        return 0;
    };
    let entidades = unsafe { &*ENTIDADES.get() };
    let a = &entidades[i];
    let b = &entidades[j];
    solapan(a.x, a.y, a.w, a.h, b.x, b.y, b.w, b.h) as i32
}

/// `toca_tag("tag")`: 1 si la entidad actual toca alguna entidad con esa
/// etiqueta.
pub fn toca_tag(actual: i32, tag: &[u8]) -> i32 {
    let Some(i) = indice(actual) else {
        return 0;
    };
    let n = cantidad();
    let entidades = unsafe { &*ENTIDADES.get() };
    let a = &entidades[i];
    for e in entidades.iter().take(n) {
        if e.id != actual && e.tag_es(tag) && solapan(a.x, a.y, a.w, a.h, e.x, e.y, e.w, e.h) {
            return 1;
        }
    }
    0
}

/// `caja_de(id, indice)`: 0=x, 1=y, 2=ancho, 3=alto.
pub fn caja_de(id: i32, campo: i32) -> i32 {
    match campo {
        0 => leer(id, MI_X),
        1 => leer(id, MI_Y),
        2 => leer(id, MI_ANCHO),
        3 => leer(id, MI_ALTO),
        _ => 0,
    }
}

/* ------------------------------------------------------------------ */
/* Globales                                                            */
/* ------------------------------------------------------------------ */

pub fn global_leer(slot: i32) -> i32 {
    if slot < 0 || slot >= GLOBALES_MAX as i32 {
        return 0;
    }
    let globales = unsafe { &*GLOBALES.get() };
    globales[slot as usize]
}

pub fn global_escribir(slot: i32, valor: i32) {
    if slot < 0 || slot >= GLOBALES_MAX as i32 {
        return;
    }
    let globales = unsafe { &mut *GLOBALES.get() };
    globales[slot as usize] = valor;
}

/* ------------------------------------------------------------------ */
/* Eventos                                                             */
/* ------------------------------------------------------------------ */

#[derive(Clone, Copy)]
struct Evento {
    tipo: i32,
    parametro: i32,
    script: i32,
    objeto: i32,
    /// Periodo de los eventos `cada` y contador para el proximo disparo.
    contador: i32,
    /// Evita repetir `tocar` mientras las cajas siguen solapando.
    activo: bool,
}

impl Evento {
    const NUEVO: Evento = Evento {
        tipo: 0,
        parametro: 0,
        script: -1,
        objeto: 0,
        contador: 0,
        activo: false,
    };
}

/// Parametro de un evento `tecla`: bits 0..7 con la mascara MOTOR_ENT_*,
/// bits 8..15 con el slot extendido mas 1 (0 si no aplica) y bits 16..23
/// con el flanco (0 mantener, 1 pulsar, 2 soltar).
pub fn tecla_campos(parametro: i32) -> (u32, u32, i32) {
    let p = parametro as u32;
    (
        p & 0xff,
        (p >> 8) & 0xff,
        ((p >> 16) & 0xff) as i32,
    )
}

/// Codifica un evento `tecla` con el formato anterior. Lo usa el editor al
/// serializar `vm.lbin`; el kernel solo necesita descodificarlo.
pub fn tecla_parametro(legacy: u32, slot: u32, flanco: i32) -> i32 {
    ((legacy & 0xff) | ((slot & 0xff) << 8) | ((flanco as u32 & 0xff) << 16)) as i32
}

static COLA: SyncCell<[Evento; EVENTOS_MAX]> = SyncCell::new([Evento::NUEVO; EVENTOS_MAX]);
static NUM_COLA: SyncCell<usize> = SyncCell::new(0);
static DECLARADOS: SyncCell<[Evento; EVENTOS_DECLARADOS_MAX]> =
    SyncCell::new([Evento::NUEVO; EVENTOS_DECLARADOS_MAX]);
static NUM_DECLARADOS: SyncCell<usize> = SyncCell::new(0);

/// Total de eventos emitidos desde el arranque (para la traza).
static EMITIDOS: SyncCell<u32> = SyncCell::new(0);
static EJECUTADOS: SyncCell<u32> = SyncCell::new(0);

/// Vacia la cola y los contadores.
pub fn limpiar_eventos() {
    unsafe {
        *NUM_COLA.get() = 0;
        *NUM_DECLARADOS.get() = 0;
        *EMITIDOS.get() = 0;
        *EJECUTADOS.get() = 0;
    }
}

/// Declara un evento que se dispara cada fotograma. Lo usa el editor al
/// compilar la imagen para representar los eventos `inicio` y `fotograma`
/// de un proyecto LOGIC3.
pub fn declarar(tipo: i32, parametro: i32, script: i32, objeto: i32) -> bool {
    let n = unsafe { *NUM_DECLARADOS.get() };
    if n >= EVENTOS_DECLARADOS_MAX {
        return false;
    }
    let declarados = unsafe { &mut *DECLARADOS.get() };
    declarados[n] = Evento {
        tipo,
        parametro,
        script,
        objeto,
        contador: if tipo == EV_CADA { parametro.max(1) } else { 0 },
        activo: false,
    };
    unsafe { *NUM_DECLARADOS.get() = n + 1 };
    true
}

/// Encola un evento suelto (mensaje, tecla, tocar...).
pub fn emitir(tipo: i32, parametro: i32, script: i32, objeto: i32) -> bool {
    let n = unsafe { *NUM_COLA.get() };
    if n >= EVENTOS_MAX {
        return false;
    }
    let cola = unsafe { &mut *COLA.get() };
    cola[n] = Evento {
        tipo,
        parametro,
        script,
        objeto,
        contador: 0,
        activo: false,
    };
    unsafe {
        *NUM_COLA.get() = n + 1;
        *EMITIDOS.get() = (*EMITIDOS.get()).wrapping_add(1);
    }
    true
}

/// Dispara los eventos declarados que tocan en este fotograma. Solo se
/// repiten `fotograma`, la tecla mantenida y `cada`; `inicio` y `escena`
/// se entregan una vez, `tocar` solo al entrar en contacto y `recibir` y
/// `clonar` los emiten los opcodes de la VM.
pub fn disparar_declarados(
    fotograma: i32,
    entrada: u32,
    anterior: u32,
    ext_actual: u32,
    ext_anterior: u32,
) {
    let n = unsafe { *NUM_DECLARADOS.get() };
    let declarados = unsafe { &mut *DECLARADOS.get() };
    for e in declarados.iter_mut().take(n) {
        match e.tipo {
            EV_INICIO | EV_ESCENA => {
                if fotograma == 1 && !e.activo {
                    e.activo = true;
                    emitir(e.tipo, e.parametro, e.script, e.objeto);
                }
            }
            EV_FOTOGRAMA => {
                emitir(e.tipo, e.parametro, e.script, e.objeto);
            }
            EV_TECLA => {
                let (mascara, slot, flanco) = tecla_campos(e.parametro);
                let (actual, antes) = if mascara != 0 {
                    (entrada & mascara, anterior & mascara)
                } else if slot > 0 && slot <= input::MOTOR_TECLAS_MAX as u32 {
                    let bit = 1u32 << (slot - 1);
                    (ext_actual & bit, ext_anterior & bit)
                } else {
                    (0, 0)
                };
                let disparar = match flanco {
                    0 => actual != 0,                     // mantener
                    1 => actual != 0 && antes == 0,       // pulsar
                    2 => actual == 0 && antes != 0,       // soltar
                    _ => false,
                };
                if disparar {
                    emitir(e.tipo, e.parametro, e.script, e.objeto);
                }
            }
            EV_CADA => {
                if e.contador > 0 {
                    e.contador -= 1;
                }
                if e.contador > 0 {
                    continue;
                }
                e.contador = e.parametro.max(1);
                emitir(e.tipo, e.parametro, e.script, e.objeto);
            }
            EV_TOCAR => {
                let toca = toca(e.objeto, e.parametro) != 0;
                if toca && !e.activo {
                    e.activo = true;
                    emitir(e.tipo, e.parametro, e.script, e.objeto);
                } else if !toca {
                    e.activo = false;
                }
            }
            /* EV_RECIBIR y EV_CLONAR los emiten MENSAJE/ENVIAR y CREAR. */
            _ => {}
        }
    }
}

/// Indica si un evento declarado se repite cada fotograma. Lo usa el
/// juego para elegir entre `lobovm::fotograma` y `lobovm::evento`.
pub fn es_repetitivo(tipo: i32, parametro: i32) -> bool {
    if tipo == EV_FOTOGRAMA {
        return true;
    }
    tipo == EV_TECLA && tecla_campos(parametro).2 == 0
}

/// Recorre los eventos declarados y devuelve los ids de funcion que pueden
/// tener una espera pendiente. El juego los reanuda con `lobovm::reanudar`.
pub fn scripts_esperables() -> [i32; EVENTOS_DECLARADOS_MAX] {
    let n = unsafe { *NUM_DECLARADOS.get() };
    let declarados = unsafe { &*DECLARADOS.get() };
    let mut ids = [-1; EVENTOS_DECLARADOS_MAX];
    let mut k = 0;
    for e in declarados.iter().take(n) {
        if es_repetitivo(e.tipo, e.parametro) {
            continue;
        }
        if e.script < 0 || k >= ids.len() {
            continue;
        }
        if !ids[..k].contains(&e.script) {
            ids[k] = e.script;
            k += 1;
        }
    }
    ids
}

/// Saca el primer evento de la cola.
pub fn tomar() -> Option<(i32, i32, i32, i32)> {
    let n = unsafe { *NUM_COLA.get() };
    if n == 0 {
        return None;
    }
    let cola = unsafe { &mut *COLA.get() };
    let e = cola[0];
    for i in 0..n - 1 {
        cola[i] = cola[i + 1];
    }
    unsafe { *NUM_COLA.get() = n - 1 };
    Some((e.tipo, e.parametro, e.script, e.objeto))
}

/// Cuenta un evento entregado (para la traza de la suite).
pub fn evento_ejecutado() {
    unsafe { *EJECUTADOS.get() = (*EJECUTADOS.get()).wrapping_add(1) };
}

/// Eventos pendientes y contadores del planificador.
pub fn contadores() -> (usize, u32, u32) {
    unsafe {
        (
            *NUM_COLA.get(),
            *EMITIDOS.get(),
            *EJECUTADOS.get(),
        )
    }
}

/* ------------------------------------------------------------------ */
/* Render de trajes                                                    */
/* ------------------------------------------------------------------ */

/// Paleta de los trajes del subconjunto. El traje 0 es el lobo azul del
/// sprite clasico; los demas son variantes que el editor puede asignar a
/// los objetos de una escena.
const TRAJES: [u32; 8] = [
    0xFF2456A6, // 0 azul (lobo)
    0xFFE0A030, // 1 oro
    0xFF50B050, // 2 verde
    0xFFB04050, // 3 rojo
    0xFF8050C0, // 4 morado
    0xFF40C0C0, // 5 cian
    0xFFF0F0F0, // 6 blanco
    0xFF303030, // 7 oscuro
];

/// Color de un traje (el ultimo si el indice esta fuera de la tabla).
pub fn traje_color(traje: i32) -> u32 {
    if traje < 0 {
        return TRAJES[0];
    }
    TRAJES[(traje as usize) % TRAJES.len()]
}

/// Dibuja las entidades visibles sobre el framebuffer del juego, ordenadas
/// por capa y con el color de su traje. No dibuja la entidad 1 (el jugador
/// lo pinta juego.rs con su sprite de 24x24).
pub fn renderizar(c: &gfx::Canvas, cam_x: i32, cam_y: i32) {
    let n = cantidad();
    let entidades = unsafe { &*ENTIDADES.get() };
    for capa in -16..=16 {
        for e in entidades.iter().take(n) {
            if e.id == 1 || !e.visible || e.capa != capa {
                continue;
            }
            let x = e.x - cam_x;
            let y = e.y - cam_y;
            if x + e.w < 0 || y + e.h < 0 || x > 640 || y > 480 {
                continue;
            }
            let color = traje_color(e.traje);
            c.fill_rect(x, y, e.w, e.h, color);
            if e.w >= 6 && e.h >= 6 {
                c.fill_rect(x + 2, y + 2, e.w - 4, 2, 0x101010u32);
            }
        }
    }
}
