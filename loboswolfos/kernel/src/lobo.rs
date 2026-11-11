//! lobo.rs - lenguaje de los botones FUNCIONES (fase 8), puerto a Rust.
//!
//! Es el puerto de `motor/lobo.c` al kernel: la misma gramatica, la
//! misma numeracion de opcodes (LOBO_NUM..LOBO_POP, 0..22) y los mismos
//! limites, para que un boton escrito en el editor corra igual en
//! Windows y en la BIOS. Las diferencias con el C son las impuestas por
//! el entorno:
//!
//!   - el texto entra como `&[u8]` crudo (sin 0 final): el LOGIC2 trae
//!     el largo de cada codigo,
//!   - la colision `choca(x, y)` entra como cierre en vez de puntero
//!     con contexto (el juego aporta la caja del sprite contra su mapa),
//!   - nada puede entrar en panico: todos los indices se comprueban y
//!     la aritmetica es `wrapping_*` (el C hacia el cast a unsigned).
//!
//! El parser es de dos pasadas: la primera preescanea los nombres que
//! se asignan en el programa (las variables locales del script) para
//! poder rechazar los nombres desconocidos; la segunda compila linea a
//! linea con descenso recursivo para las expresiones y una pila de
//! bloques abiertos para si/sino/fin y mientras/fin. Los errores llevan
//! la forma "linea N: motivo" en el bufer que pide el llamante.

/* ------------------------------------------------------------------ */
/* Limites y constantes (espejo de lobo.h)                             */
/* ------------------------------------------------------------------ */

/// Instrucciones por programa.
pub const LOBO_INSTS_MAX: usize = 256;
/// Valores apilados por expresion.
pub const LOBO_PILA_MAX: usize = 32;
/// Variables locales por script.
pub const LOBO_LOCALES_MAX: usize = 16;
/// Largo de un nombre de variable.
pub const LOBO_NOMBRE_MAX: usize = 32;
/// Largo del bufer de mensaje de error.
pub const LOBO_ERROR_MAX: usize = 96;
/// Pasos por ejecucion (por fotograma): un bucle sin salida no cuelga.
pub const LOBO_PASOS_MAX: i32 = 200000;
/// Bloques si/sino/mientras anidados a la vez.
pub const LOBO_BLOQUES_MAX: usize = 16;
/// Tokens por linea.
pub const LOBO_TOKENS_LINEA: usize = 48;

/* Indices de las variables del juego dentro del espacio comun
 * (0..3 globales, 4..19 locales del script). */
pub const LOBO_VAR_X: i32 = 0;
pub const LOBO_VAR_Y: i32 = 1;
pub const LOBO_VAR_VEL: i32 = 2;
pub const LOBO_VAR_FOTOGRAMA: i32 = 3;
pub const LOBO_VAR_LOCALES: i32 = 4;

/* Bits de tecla: los mismos MOTOR_ENT_* de motor.rs (el juego pasa su
 * palabra de entrada tal cual). */
pub const LOBO_TECLA_IZQUIERDA: i32 = 0x01;
pub const LOBO_TECLA_DERECHA: i32 = 0x02;
pub const LOBO_TECLA_ARRIBA: i32 = 0x04;
pub const LOBO_TECLA_ABAJO: i32 = 0x08;
pub const LOBO_TECLA_SALTO: i32 = 0x10;
pub const LOBO_TECLA_CERRAR: i32 = 0x20;

/* Codigos de operacion. El numero de cada uno es parte del formato:
 * es la misma numeracion que el enum de lobo.h. */
pub const LOBO_NUM: u8 = 0; /* valor: constante a apilar */
pub const LOBO_VAR: u8 = 1; /* valor: indice de variable */
pub const LOBO_TECLA: u8 = 2; /* valor: bit de tecla */
pub const LOBO_CHOCAR: u8 = 3; /* colision: apila 0/1 */
pub const LOBO_CERRAR: u8 = 4; /* pedir cerrar el juego */
pub const LOBO_SUMA: u8 = 5;
pub const LOBO_RESTA: u8 = 6;
pub const LOBO_MULT: u8 = 7;
pub const LOBO_DIV: u8 = 8; /* division entera; entre 0 da 0 */
pub const LOBO_MOD: u8 = 9; /* resto; entre 0 da 0 */
pub const LOBO_IGUAL: u8 = 10;
pub const LOBO_DISTINTO: u8 = 11;
pub const LOBO_MENOR: u8 = 12;
pub const LOBO_MAYOR: u8 = 13;
pub const LOBO_MENOR_IG: u8 = 14;
pub const LOBO_MAYOR_IG: u8 = 15;
pub const LOBO_Y: u8 = 16; /* y logico (ambos lados se evaluan) */
pub const LOBO_O: u8 = 17; /* o logico */
pub const LOBO_NO: u8 = 18; /* no logico */
pub const LOBO_ASIGNA: u8 = 19; /* valor: indice de variable */
pub const LOBO_SALTO: u8 = 20; /* valor: instruccion destino */
pub const LOBO_SALTO_FALSO: u8 = 21; /* valor: destino si el tope es 0 */
pub const LOBO_POP: u8 = 22; /* descarta el tope (expr suelta) */

/* ------------------------------------------------------------------ */
/* Programas, estado y maquinas                                        */
/* ------------------------------------------------------------------ */

/// Una instruccion del programa compilado.
#[derive(Clone, Copy)]
pub struct LoboInst {
    pub op: u8,
    pub valor: i32,
}

impl LoboInst {
    pub const NUEVA: LoboInst = LoboInst { op: 0, valor: 0 };
}

/// Programa compilado: lista plana de instrucciones.
pub struct LoboPrograma {
    pub insts: [LoboInst; LOBO_INSTS_MAX],
    pub largo: usize,
}

impl LoboPrograma {
    pub const NUEVO: LoboPrograma = LoboPrograma {
        insts: [LoboInst::NUEVA; LOBO_INSTS_MAX],
        largo: 0,
    };
}

/// Lo que el juego expone a los scripts: posicion del sprite,
/// velocidad, contador de fotogramas y bits de entrada.
pub struct LoboEstado {
    pub x: i32,
    pub y: i32,
    pub vel: i32,
    pub fotograma: i32,
    /// Bits LOBO_TECLA_* (== MOTOR_ENT_*).
    pub entrada: u32,
}

/// Estado persistente de un script entre fotogramas.
pub struct LoboMaquina {
    pub locales: [i32; LOBO_LOCALES_MAX],
    /// 1 si el script llamo a cerrar().
    pub cerrar: bool,
    /// 1 si se quedo sin presupuesto de pasos.
    pub agotado: bool,
}

impl LoboMaquina {
    pub const NUEVA: LoboMaquina = LoboMaquina {
        locales: [0; LOBO_LOCALES_MAX],
        cerrar: false,
        agotado: false,
    };

    /// Reinicia la maquina (locales a cero) para un uso nuevo.
    pub fn reiniciar(&mut self) {
        self.locales = [0; LOBO_LOCALES_MAX];
        self.cerrar = false;
        self.agotado = false;
    }
}

/* ------------------------------------------------------------------ */
/* Utilidades de texto (sin core::fmt, para medir igual que el C)      */
/* ------------------------------------------------------------------ */

/// Longitud util de un nombre fijo (hasta su 0 final).
fn largo_nombre(nombre: &[u8; LOBO_NOMBRE_MAX]) -> usize {
    nombre
        .iter()
        .position(|&b| b == 0)
        .unwrap_or(LOBO_NOMBRE_MAX)
}

/// Un nombre fijo (con relleno de ceros) es ese literal.
fn nombre_es(nombre: &[u8; LOBO_NOMBRE_MAX], literal: &[u8]) -> bool {
    let n = largo_nombre(nombre);
    n == literal.len() && &nombre[..n] == literal
}

/// Dos nombres fijos (con relleno de ceros) son iguales hasta sus 0.
fn nombres_iguales(a: &[u8; LOBO_NOMBRE_MAX], b: &[u8; LOBO_NOMBRE_MAX]) -> bool {
    let (na, nb) = (largo_nombre(a), largo_nombre(b));
    na == nb && &a[..na] == &b[..nb]
}

/// Deja el mensaje de error con el numero de linea (el primero gana).
///
/// Funcion libre en vez de metodo del parser: asi el parser puede
/// prestar su bufer de error sin pelearse con los demas prestamos.
/// El mensaje queda terminado en 0 para que el llamante sepa donde
/// acaba (los mensajes son ASCII, del propio parser).
fn escribir_error(error: &mut [u8], linea: i32, mensaje: &str) {
    if error.is_empty() || error[0] != 0 {
        return; // ya hay un error (o no hay donde escribirlo)
    }
    let tope = error.len() - 1; // ultimo indice usable (deja sitio al 0)
    let mut j = 0usize;
    for &b in b"linea " {
        if j >= tope {
            break;
        }
        error[j] = b;
        j += 1;
    }
    // Numero de linea a mano (sin formato: cero dependencias).
    let mut v = if linea <= 0 { 1 } else { linea };
    let mut num = [0u8; 12];
    let mut n = 0usize;
    loop {
        num[n] = b'0' + (v % 10) as u8;
        n += 1;
        v /= 10;
        if v == 0 {
            break;
        }
    }
    let mut k = n as isize - 1;
    while k >= 0 && j < tope {
        error[j] = num[k as usize];
        j += 1;
        k -= 1;
    }
    if j < tope {
        error[j] = b':';
        j += 1;
    }
    if j < tope {
        error[j] = b' ';
        j += 1;
    }
    for &b in mensaje.as_bytes() {
        if j >= tope {
            break;
        }
        error[j] = b;
        j += 1;
    }
    error[j] = 0;
}

/* ------------------------------------------------------------------ */
/* Tokens                                                              */
/* ------------------------------------------------------------------ */

const TOK_NUM: u8 = 0;
const TOK_ID: u8 = 1;
const TOK_OP: u8 = 2;

#[derive(Clone, Copy)]
struct LoboToken {
    tipo: u8,
    /// Identificador u operador (con relleno de ceros).
    texto: [u8; LOBO_NOMBRE_MAX],
    /// Valor si tipo == TOK_NUM.
    numero: i32,
}

impl LoboToken {
    const NUEVO: LoboToken = LoboToken {
        tipo: 0,
        texto: [0; LOBO_NOMBRE_MAX],
        numero: 0,
    };
}

/// Es espacio en blanco.
fn espacio(c: u8) -> bool {
    c == b' ' || c == b'\t'
}

/// Es inicial de identificador.
fn letra(c: u8) -> bool {
    c.is_ascii_lowercase() || c.is_ascii_uppercase() || c == b'_'
}

/// Es continuacion de identificador.
fn alfanum(c: u8) -> bool {
    letra(c) || c.is_ascii_digit()
}

/// Parte una linea en tokens. Devuelve el numero de tokens; los
/// operadores de dos caracteres se juntan en uno (==, !=, <=, >=) y el
/// `#` corta la linea. -1: nombre demasiado largo; -2: caracter
/// inesperado.
fn tokenizar(linea: &[u8], tokens: &mut [LoboToken]) -> isize {
    let mut n = 0usize;
    let mut i = 0usize;
    while i < linea.len() && n < tokens.len() {
        let c = linea[i];
        if espacio(c) {
            i += 1;
            continue;
        }
        if c == b'#' {
            break; // comentario hasta el final de la linea
        }
        if c.is_ascii_digit() {
            let mut v: i32 = 0;
            let mut digitos = 0;
            while i < linea.len() && linea[i].is_ascii_digit() {
                v = v.wrapping_mul(10).wrapping_add((linea[i] - b'0') as i32);
                i += 1;
                digitos += 1;
                if digitos > 9 {
                    v = 999999999;
                }
            }
            tokens[n].tipo = TOK_NUM;
            tokens[n].numero = v;
            tokens[n].texto = [0; LOBO_NOMBRE_MAX];
            n += 1;
            continue;
        }
        if letra(c) {
            let mut j = 0usize;
            while i < linea.len() && alfanum(linea[i]) {
                if j < LOBO_NOMBRE_MAX - 1 {
                    tokens[n].texto[j] = linea[i];
                }
                j += 1;
                i += 1;
            }
            if j >= LOBO_NOMBRE_MAX {
                return -1; // nombre demasiado largo
            }
            tokens[n].texto[j] = 0;
            tokens[n].tipo = TOK_ID;
            tokens[n].numero = 0;
            n += 1;
            continue;
        }
        // Operadores de dos caracteres primero.
        if i + 1 < linea.len() {
            let c2 = linea[i + 1];
            if (c == b'=' && c2 == b'=') || (c == b'!' && c2 == b'=')
                || (c == b'<' && c2 == b'=') || (c == b'>' && c2 == b'=')
            {
                tokens[n].tipo = TOK_OP;
                tokens[n].texto = [0; LOBO_NOMBRE_MAX];
                tokens[n].texto[0] = c;
                tokens[n].texto[1] = c2;
                tokens[n].numero = 0;
                n += 1;
                i += 2;
                continue;
            }
        }
        if matches!(
            c,
            b'=' | b'!' | b'<' | b'>' | b'+' | b'-' | b'*' | b'/' | b'%' | b'&' | b'|' | b'('
                | b')' | b','
        ) {
            tokens[n].tipo = TOK_OP;
            tokens[n].texto = [0; LOBO_NOMBRE_MAX];
            tokens[n].texto[0] = c;
            tokens[n].numero = 0;
            n += 1;
            i += 1;
            continue;
        }
        return -2; // caracter inesperado
    }
    n as isize
}

/* ------------------------------------------------------------------ */
/* Parser                                                              */
/* ------------------------------------------------------------------ */

/* Tipos de bloque abierto (si/sino y mientras). */
const BLOQUE_SI: u8 = 0; /* si ... (sin sino todavia) */
const BLOQUE_SINO: u8 = 1; /* si ... sino ... */
const BLOQUE_MIENTRAS: u8 = 2;

#[derive(Clone, Copy)]
struct LoboBloque {
    tipo: u8,
    /// Linea donde se abrio, para los errores.
    linea: i32,
    /// Instruccion SALTO_FALSO a parchear al fin.
    salto_falso: i32,
    /// SALTO del sino a parchear al fin (-1 si no hay).
    salto_sino: i32,
    /// Instruccion donde empieza la condicion (mientras).
    mientras_inicio: i32,
}

impl LoboBloque {
    const NUEVO: LoboBloque = LoboBloque {
        tipo: 0,
        linea: 0,
        salto_falso: 0,
        salto_sino: -1,
        mientras_inicio: -1,
    };
}

struct LoboParser<'a> {
    prog: &'a mut LoboPrograma,
    error: &'a mut [u8],
    /// Linea actual (1-based).
    linea: i32,
    tokens: [LoboToken; LOBO_TOKENS_LINEA],
    num_tokens: usize,
    /// Token actual de la linea.
    pos: usize,
    bloques: [LoboBloque; LOBO_BLOQUES_MAX],
    num_bloques: usize,
    locales: [[u8; LOBO_NOMBRE_MAX]; LOBO_LOCALES_MAX],
    num_locales: usize,
}

/// Indice de variable del juego, tecla o funcion integrada que
/// responde a ese nombre; -1 si no es ninguna de las conocidas.
/// Los otros dos valores son las banderas de tecla y funcion.
fn nombre_conocido(nombre: &[u8; LOBO_NOMBRE_MAX]) -> (i32, bool, bool) {
    if nombre_es(nombre, b"x") {
        return (LOBO_VAR_X, false, false);
    }
    if nombre_es(nombre, b"y") {
        return (LOBO_VAR_Y, false, false);
    }
    if nombre_es(nombre, b"vel") {
        return (LOBO_VAR_VEL, false, false);
    }
    if nombre_es(nombre, b"fotograma") {
        return (LOBO_VAR_FOTOGRAMA, false, false);
    }
    if nombre_es(nombre, b"tecla_izquierda") {
        return (LOBO_TECLA_IZQUIERDA, true, false);
    }
    if nombre_es(nombre, b"tecla_derecha") {
        return (LOBO_TECLA_DERECHA, true, false);
    }
    if nombre_es(nombre, b"tecla_arriba") {
        return (LOBO_TECLA_ARRIBA, true, false);
    }
    if nombre_es(nombre, b"tecla_abajo") {
        return (LOBO_TECLA_ABAJO, true, false);
    }
    if nombre_es(nombre, b"tecla_salto") {
        return (LOBO_TECLA_SALTO, true, false);
    }
    if nombre_es(nombre, b"tecla_cerrar") {
        return (LOBO_TECLA_CERRAR, true, false);
    }
    if nombre_es(nombre, b"choca") {
        return (0, false, true);
    }
    if nombre_es(nombre, b"cerrar") {
        return (0, false, true);
    }
    (-1, false, false)
}

impl<'a> LoboParser<'a> {
    /// Deja el mensaje de error de la linea actual (el primero gana).
    fn error(&mut self, mensaje: &str) {
        let linea = self.linea;
        escribir_error(self.error, linea, mensaje);
    }

    /// Anade una instruccion al programa; false si ya no cabe.
    fn emitir(&mut self, op: u8, valor: i32) -> bool {
        if self.prog.largo >= LOBO_INSTS_MAX {
            self.error("el programa es demasiado largo");
            return false;
        }
        self.prog.insts[self.prog.largo] = LoboInst { op, valor };
        self.prog.largo += 1;
        true
    }

    /// Indice de la local con ese nombre (-1 si no esta preescaneada).
    fn buscar_local(&self, nombre: &[u8; LOBO_NOMBRE_MAX]) -> i32 {
        for i in 0..self.num_locales {
            if nombres_iguales(&self.locales[i], nombre) {
                return i as i32;
            }
        }
        -1
    }

    /// Registra un nombre como variable local del script.
    fn nueva_local(&mut self, nombre: &[u8; LOBO_NOMBRE_MAX]) -> i32 {
        let i = self.buscar_local(nombre);
        if i >= 0 {
            return i;
        }
        if self.num_locales >= LOBO_LOCALES_MAX {
            self.error("demasiadas variables locales (maximo 16)");
            return -1;
        }
        /* copia con relleno de ceros tras el 0 final, como el C */
        let n = largo_nombre(nombre);
        self.locales[self.num_locales][..n].copy_from_slice(&nombre[..n]);
        self.locales[self.num_locales][n..].fill(0);
        self.num_locales += 1;
        (self.num_locales - 1) as i32
    }

    /* ---------------- expresiones (descenso recursivo) ------------- */

    /// Token actual ya consumido?
    fn fin_tokens(&self) -> bool {
        self.pos >= self.num_tokens
    }

    /// Come el token actual si es ese operador exacto.
    fn comer_op(&mut self, op: &[u8]) -> bool {
        if !self.fin_tokens()
            && self.tokens[self.pos].tipo == TOK_OP
            && nombre_es(&self.tokens[self.pos].texto, op)
        {
            self.pos += 1;
            true
        } else {
            false
        }
    }

    /// Come un operador de un conjunto dado; devuelve su indice o -1.
    fn comer_cualquiera(&mut self, ops: &[&[u8]]) -> isize {
        for (i, op) in ops.iter().enumerate() {
            if self.comer_op(op) {
                return i as isize;
            }
        }
        -1
    }

    /// Primario: numero, variable, tecla o llamada a funcion integrada.
    fn primario(&mut self) -> bool {
        if self.fin_tokens() {
            self.error("se esperaba una expresion");
            return false;
        }
        let t = self.tokens[self.pos];
        if t.tipo == TOK_NUM {
            self.pos += 1;
            return self.emitir(LOBO_NUM, t.numero);
        }
        if t.tipo == TOK_ID {
            self.pos += 1;
            let (conocido, es_tecla, es_funcion) = nombre_conocido(&t.texto);
            if es_funcion {
                /* llamada: choca(x, y) o cerrar() */
                if !self.comer_op(b"(") {
                    self.error("se esperaba ( despues del nombre");
                    return false;
                }
                if nombre_es(&t.texto, b"cerrar") {
                    if !self.comer_op(b")") {
                        self.error("cerrar() no lleva argumentos");
                        return false;
                    }
                    return self.emitir(LOBO_CERRAR, 0);
                }
                if !self.expresion() {
                    return false;
                }
                if !self.comer_op(b",") {
                    self.error("choca necesita dos argumentos");
                    return false;
                }
                if !self.expresion() {
                    return false;
                }
                if !self.comer_op(b")") {
                    self.error("se esperaba ) para cerrar la llamada");
                    return false;
                }
                return self.emitir(LOBO_CHOCAR, 0);
            }
            if es_tecla {
                return self.emitir(LOBO_TECLA, conocido);
            }
            if conocido >= 0 {
                return self.emitir(LOBO_VAR, conocido);
            }
            let local = self.buscar_local(&t.texto);
            if local < 0 {
                self.error("nombre desconocido");
                return false;
            }
            return self.emitir(LOBO_VAR, LOBO_VAR_LOCALES + local);
        }
        if self.comer_op(b"(") {
            if !self.expresion() {
                return false;
            }
            if !self.comer_op(b")") {
                self.error("se esperaba )");
                return false;
            }
            return true;
        }
        self.error("se esperaba una expresion");
        false
    }

    /// Unario: -expr, !expr, no expr o un primario.
    fn unario(&mut self) -> bool {
        if self.comer_op(b"-") {
            if !self.emitir(LOBO_NUM, 0) {
                return false;
            }
            if !self.unario() {
                return false;
            }
            return self.emitir(LOBO_RESTA, 0);
        }
        /* "!" es un operador: comer_op ya avanza el cursor. */
        if self.comer_op(b"!") {
            if !self.unario() {
                return false;
            }
            return self.emitir(LOBO_NO, 0);
        }
        /* "no" es un identificador: hay que avanzar el cursor a mano. */
        if !self.fin_tokens()
            && self.tokens[self.pos].tipo == TOK_ID
            && nombre_es(&self.tokens[self.pos].texto, b"no")
        {
            self.pos += 1;
            if !self.unario() {
                return false;
            }
            return self.emitir(LOBO_NO, 0);
        }
        self.primario()
    }

    /// Producto: * / %.
    fn producto(&mut self) -> bool {
        let ops: [&[u8]; 3] = [b"*", b"/", b"%"];
        if !self.unario() {
            return false;
        }
        loop {
            let cual = self.comer_cualquiera(&ops);
            if cual < 0 {
                return true;
            }
            if !self.unario() {
                return false;
            }
            if !self.emitir(LOBO_MULT + cual as u8, 0) {
                return false;
            }
        }
    }

    /// Suma: + -.
    fn suma(&mut self) -> bool {
        let ops: [&[u8]; 2] = [b"+", b"-"];
        if !self.producto() {
            return false;
        }
        loop {
            let cual = self.comer_cualquiera(&ops);
            if cual < 0 {
                return true;
            }
            if !self.producto() {
                return false;
            }
            if !self.emitir(LOBO_SUMA + cual as u8, 0) {
                return false;
            }
        }
    }

    /// Comparacion: == != < > <= >= (una sola, sin encadenar).
    fn comparacion(&mut self) -> bool {
        let ops: [&[u8]; 6] = [b"==", b"!=", b"<", b">", b"<=", b">="];
        if !self.suma() {
            return false;
        }
        let cual = self.comer_cualquiera(&ops);
        if cual < 0 {
            return true;
        }
        if !self.suma() {
            return false;
        }
        self.emitir(LOBO_IGUAL + cual as u8, 0)
    }

    /// Y logico: &.
    fn y(&mut self) -> bool {
        if !self.comparacion() {
            return false;
        }
        while self.comer_op(b"&") {
            if !self.comparacion() {
                return false;
            }
            if !self.emitir(LOBO_Y, 0) {
                return false;
            }
        }
        true
    }

    /// O logico: |.
    fn o(&mut self) -> bool {
        if !self.y() {
            return false;
        }
        while self.comer_op(b"|") {
            if !self.y() {
                return false;
            }
            if !self.emitir(LOBO_O, 0) {
                return false;
            }
        }
        true
    }

    fn expresion(&mut self) -> bool {
        self.o()
    }

    /* ---------------- pasada de preescaneo ------------------------ */

    /// Recorre todas las lineas y registra cada "nombre =" como local.
    /// Con eso el parseo puede rechazar los nombres desconocidos (los
    /// errores tipograficos dan error de sintaxis en vez de leer 0 en
    /// silencio).
    fn preescanear(&mut self, texto: &[u8]) -> bool {
        let mut inicio = 0usize;
        let mut linea = 1i32;
        while inicio < texto.len() {
            let mut fin = inicio;
            while fin < texto.len() && texto[fin] != b'\n' {
                fin += 1;
            }
            let mut largo = fin - inicio;
            /* quitar el \r del final si el texto viene de un CRLF */
            if largo > 0 && texto[inicio + largo - 1] == b'\r' {
                largo -= 1;
            }
            self.linea = linea;
            let mut tokens = [LoboToken::NUEVO; LOBO_TOKENS_LINEA];
            let n = tokenizar(&texto[inicio..inicio + largo], &mut tokens);
            if n < 0 {
                self.error(if n == -1 {
                    "nombre demasiado largo"
                } else {
                    "caracter inesperado"
                });
                return false;
            }
            let num = n as usize;
            let mut i = 0usize;
            while i + 1 < num {
                if tokens[i].tipo == TOK_ID
                    && tokens[i + 1].tipo == TOK_OP
                    && tokens[i + 1].texto[0] == b'='
                    && tokens[i + 1].texto[1] == 0
                {
                    let (conocido, es_tecla, es_funcion) = nombre_conocido(&tokens[i].texto);
                    if conocido >= 0 && !es_funcion {
                        i += 1;
                        continue; /* global del juego (la tecla salta aqui) */
                    }
                    if es_tecla {
                        /* rama muerta en la practica: el continue de
                         * arriba la tapa; el error real salta en
                         * sentencia (se conserva por fidelidad al C) */
                        self.linea = linea;
                        self.error("las teclas no se pueden asignar");
                        return false;
                    }
                    if es_funcion {
                        self.linea = linea;
                        self.error("no se puede asignar a una funcion");
                        return false;
                    }
                    if self.nueva_local(&tokens[i].texto) < 0 {
                        self.linea = linea;
                        return false;
                    }
                }
                i += 1;
            }
            if fin < texto.len() {
                fin += 1; /* saltar el \n */
            }
            inicio = fin;
            linea += 1;
        }
        true
    }

    /* ---------------- compilacion de una linea --------------------- */

    /// Compila la linea actual de self.tokens como sentencia.
    fn sentencia(&mut self) -> bool {
        let t = self.tokens[0];

        if t.tipo == TOK_ID {
            /* Palabras del lenguaje. */
            if nombre_es(&t.texto, b"si") {
                self.pos = 1;
                if !self.expresion() {
                    return false;
                }
                if !self.fin_tokens() {
                    self.error("sobran datos tras la condicion del si");
                    return false;
                }
                if self.num_bloques >= LOBO_BLOQUES_MAX {
                    self.error("demasiados bloques anidados");
                    return false;
                }
                let bi = self.num_bloques;
                self.num_bloques += 1;
                self.bloques[bi].tipo = BLOQUE_SI;
                self.bloques[bi].linea = self.linea;
                self.bloques[bi].salto_sino = -1;
                self.bloques[bi].mientras_inicio = -1;
                self.bloques[bi].salto_falso = self.prog.largo as i32;
                return self.emitir(LOBO_SALTO_FALSO, 0);
            }
            if nombre_es(&t.texto, b"sino") {
                if self.num_bloques == 0
                    || self.bloques[self.num_bloques - 1].tipo != BLOQUE_SI
                {
                    self.error("sino sin un si abierto");
                    return false;
                }
                if self.num_tokens != 1 {
                    self.error("sino va solo en la linea");
                    return false;
                }
                let bi = self.num_bloques - 1;
                self.bloques[bi].salto_sino = self.prog.largo as i32;
                if !self.emitir(LOBO_SALTO, 0) {
                    return false;
                }
                /* el cuerpo del si salta aqui (al principio del sino) */
                let destino = self.prog.largo as i32;
                let indice = self.bloques[bi].salto_falso as usize;
                if indice < self.prog.largo {
                    self.prog.insts[indice].valor = destino;
                }
                self.bloques[bi].tipo = BLOQUE_SINO;
                return true;
            }
            if nombre_es(&t.texto, b"fin") {
                if self.num_bloques == 0 {
                    self.error("fin sin bloque abierto");
                    return false;
                }
                if self.num_tokens != 1 {
                    self.error("fin va solo en la linea");
                    return false;
                }
                let bi = self.num_bloques - 1;
                if self.bloques[bi].tipo == BLOQUE_MIENTRAS {
                    if !self.emitir(LOBO_SALTO, self.bloques[bi].mientras_inicio) {
                        return false;
                    }
                    let destino = self.prog.largo as i32;
                    let indice = self.bloques[bi].salto_falso as usize;
                    if indice < self.prog.largo {
                        self.prog.insts[indice].valor = destino;
                    }
                } else {
                    /* si a pelo: el falso salta aqui; con sino: el salto
                     * del sino apunta aqui. */
                    let destino = self.prog.largo as i32;
                    let indice = if self.bloques[bi].salto_sino >= 0 {
                        self.bloques[bi].salto_sino as usize
                    } else {
                        self.bloques[bi].salto_falso as usize
                    };
                    if indice < self.prog.largo {
                        self.prog.insts[indice].valor = destino;
                    }
                }
                self.num_bloques -= 1;
                return true;
            }
            if nombre_es(&t.texto, b"mientras") {
                self.pos = 1;
                if self.num_bloques >= LOBO_BLOQUES_MAX {
                    self.error("demasiados bloques anidados");
                    return false;
                }
                let bi = self.num_bloques;
                self.num_bloques += 1;
                self.bloques[bi].tipo = BLOQUE_MIENTRAS;
                self.bloques[bi].linea = self.linea;
                self.bloques[bi].salto_sino = -1;
                self.bloques[bi].mientras_inicio = self.prog.largo as i32;
                if !self.expresion() {
                    return false;
                }
                if !self.fin_tokens() {
                    self.error("sobran datos tras la condicion del mientras");
                    return false;
                }
                self.bloques[bi].salto_falso = self.prog.largo as i32;
                return self.emitir(LOBO_SALTO_FALSO, 0);
            }
            /* Asignacion: nombre = expresion. */
            if self.num_tokens >= 2
                && self.tokens[1].tipo == TOK_OP
                && self.tokens[1].texto[0] == b'='
                && self.tokens[1].texto[1] == 0
            {
                let (conocido, es_tecla, es_funcion) = nombre_conocido(&t.texto);
                let destino: i32;
                if es_tecla {
                    self.error("las teclas no se pueden asignar");
                    return false;
                }
                if es_funcion {
                    self.error("no se puede asignar a una funcion");
                    return false;
                }
                if conocido >= 0 {
                    destino = conocido;
                } else {
                    let local = self.buscar_local(&t.texto);
                    if local < 0 {
                        self.error("nombre desconocido");
                        return false;
                    }
                    destino = LOBO_VAR_LOCALES + local;
                }
                self.pos = 2;
                if !self.expresion() {
                    return false;
                }
                if !self.fin_tokens() {
                    self.error("sobran datos tras la asignacion");
                    return false;
                }
                return self.emitir(LOBO_ASIGNA, destino);
            }
        }

        /* Expresion suelta (p. ej. cerrar()): se evalua y se descarta. */
        self.pos = 0;
        if !self.expresion() {
            return false;
        }
        if !self.fin_tokens() {
            self.error("sobran datos al final de la linea");
            return false;
        }
        self.emitir(LOBO_POP, 0)
    }
}

/* ------------------------------------------------------------------ */
/* lobo_parsear                                                        */
/* ------------------------------------------------------------------ */

/// Compila el texto de un script. Devuelve true si compila; si no,
/// false y `error` lleva el mensaje "linea N: ..." (terminado en 0).
/// `programa` queda vacio al empezar.
pub fn parsear(texto: &[u8], programa: &mut LoboPrograma, error: &mut [u8]) -> bool {
    if !error.is_empty() {
        error[0] = 0;
    }
    programa.largo = 0;
    programa.insts = [LoboInst::NUEVA; LOBO_INSTS_MAX];

    let mut p = LoboParser {
        prog: programa,
        error,
        linea: 1,
        tokens: [LoboToken::NUEVO; LOBO_TOKENS_LINEA],
        num_tokens: 0,
        pos: 0,
        bloques: [LoboBloque::NUEVO; LOBO_BLOQUES_MAX],
        num_bloques: 0,
        locales: [[0; LOBO_NOMBRE_MAX]; LOBO_LOCALES_MAX],
        num_locales: 0,
    };

    /* 1. Preescaneo de locales. */
    if !p.preescanear(texto) {
        return false;
    }
    if !p.error.is_empty() && p.error[0] != 0 {
        return false;
    }

    /* 2. Compilacion linea a linea. */
    let mut inicio = 0usize;
    let mut linea = 1i32;
    while inicio < texto.len() {
        let mut fin = inicio;
        while fin < texto.len() && texto[fin] != b'\n' {
            fin += 1;
        }
        let mut largo = fin - inicio;
        /* quitar el \r del final si viene de un archivo de Windows */
        if largo > 0 && texto[inicio + largo - 1] == b'\r' {
            largo -= 1;
        }
        p.linea = linea;
        let n = tokenizar(&texto[inicio..inicio + largo], &mut p.tokens);
        if n < 0 {
            p.error(if n == -1 {
                "nombre demasiado largo"
            } else {
                "caracter inesperado"
            });
            return false;
        }
        p.num_tokens = n as usize;
        p.pos = 0;
        if p.num_tokens > 0 {
            if !p.sentencia() {
                return false;
            }
            if !p.error.is_empty() && p.error[0] != 0 {
                return false;
            }
        }
        if fin < texto.len() {
            fin += 1; /* saltar el \n */
        }
        inicio = fin;
        linea += 1;
    }

    if p.num_bloques > 0 {
        p.linea = p.bloques[p.num_bloques - 1].linea;
        p.error("falta el fin del bloque");
        return false;
    }
    true
}

/* ------------------------------------------------------------------ */
/* Interprete                                                          */
/* ------------------------------------------------------------------ */

/// Lee una variable (global del juego o local del script).
fn leer_var(estado: &LoboEstado, maquina: &LoboMaquina, var: i32) -> i32 {
    match var {
        LOBO_VAR_X => estado.x,
        LOBO_VAR_Y => estado.y,
        LOBO_VAR_VEL => estado.vel,
        LOBO_VAR_FOTOGRAMA => estado.fotograma,
        _ => {
            if var >= LOBO_VAR_LOCALES && var < LOBO_VAR_LOCALES + LOBO_LOCALES_MAX as i32 {
                maquina.locales[(var - LOBO_VAR_LOCALES) as usize]
            } else {
                0
            }
        }
    }
}

/// Escribe una variable (global del juego o local del script).
fn escribir_var(estado: &mut LoboEstado, maquina: &mut LoboMaquina, var: i32, valor: i32) {
    match var {
        LOBO_VAR_X => estado.x = valor,
        LOBO_VAR_Y => estado.y = valor,
        LOBO_VAR_VEL => estado.vel = valor,
        LOBO_VAR_FOTOGRAMA => estado.fotograma = valor,
        _ => {
            if var >= LOBO_VAR_LOCALES && var < LOBO_VAR_LOCALES + LOBO_LOCALES_MAX as i32 {
                maquina.locales[(var - LOBO_VAR_LOCALES) as usize] = valor;
            }
        }
    }
}

/// Ejecuta un programa ya compilado contra el estado del juego. La
/// maquina conserva las locales y las peticiones de cerrar. `choca` es
/// la prueba de colision que el juego expone a los scripts (la caja
/// del sprite contra el mapa).
pub fn ejecutar<F: FnMut(i32, i32) -> bool>(
    programa: &LoboPrograma,
    estado: &mut LoboEstado,
    choca: &mut F,
    maquina: &mut LoboMaquina,
) {
    let mut pila = [0i32; LOBO_PILA_MAX];
    let mut np = 0usize; /* valores apilados */
    let mut pasos = LOBO_PASOS_MAX;
    let mut i = 0usize;

    maquina.cerrar = false;
    maquina.agotado = false;

    while i < programa.largo {
        let in_ = programa.insts[i];
        pasos -= 1;
        if pasos < 0 {
            maquina.agotado = true;
            return;
        }
        match in_.op {
            LOBO_NUM => {
                if np >= LOBO_PILA_MAX {
                    maquina.agotado = true;
                    return;
                }
                pila[np] = in_.valor;
                np += 1;
            }
            LOBO_VAR => {
                if np >= LOBO_PILA_MAX {
                    maquina.agotado = true;
                    return;
                }
                pila[np] = leer_var(estado, maquina, in_.valor);
                np += 1;
            }
            LOBO_TECLA => {
                if np >= LOBO_PILA_MAX {
                    maquina.agotado = true;
                    return;
                }
                pila[np] = (estado.entrada & in_.valor as u32 != 0) as i32;
                np += 1;
            }
            LOBO_CHOCAR => {
                if np < 2 {
                    maquina.agotado = true;
                    return;
                }
                np -= 1;
                let b = pila[np];
                np -= 1;
                let a = pila[np];
                pila[np] = choca(a, b) as i32;
                np += 1;
            }
            LOBO_CERRAR => {
                maquina.cerrar = true;
                if np >= LOBO_PILA_MAX {
                    maquina.agotado = true;
                    return;
                }
                pila[np] = 0;
                np += 1;
            }
            LOBO_SUMA | LOBO_RESTA | LOBO_MULT => {
                if np < 2 {
                    maquina.agotado = true;
                    return;
                }
                np -= 1;
                let b = pila[np];
                np -= 1;
                let a = pila[np];
                pila[np] = match in_.op {
                    LOBO_SUMA => a.wrapping_add(b),
                    LOBO_RESTA => a.wrapping_sub(b),
                    _ => a.wrapping_mul(b),
                };
                np += 1;
            }
            LOBO_DIV | LOBO_MOD => {
                if np < 2 {
                    maquina.agotado = true;
                    return;
                }
                np -= 1;
                let b = pila[np];
                np -= 1;
                let a = pila[np];
                /* entre 0, o desbordando el minimo: 0 (como el C) */
                pila[np] = if b == 0 || (a == i32::MIN && b == -1) {
                    0
                } else if in_.op == LOBO_DIV {
                    a / b
                } else {
                    a % b
                };
                np += 1;
            }
            LOBO_IGUAL | LOBO_DISTINTO | LOBO_MENOR | LOBO_MAYOR | LOBO_MENOR_IG | LOBO_MAYOR_IG => {
                if np < 2 {
                    maquina.agotado = true;
                    return;
                }
                np -= 1;
                let b = pila[np];
                np -= 1;
                let a = pila[np];
                pila[np] = match in_.op {
                    LOBO_IGUAL => (a == b) as i32,
                    LOBO_DISTINTO => (a != b) as i32,
                    LOBO_MENOR => (a < b) as i32,
                    LOBO_MAYOR => (a > b) as i32,
                    LOBO_MENOR_IG => (a <= b) as i32,
                    _ => (a >= b) as i32,
                };
                np += 1;
            }
            LOBO_Y | LOBO_O => {
                if np < 2 {
                    maquina.agotado = true;
                    return;
                }
                np -= 1;
                let b = pila[np];
                np -= 1;
                let a = pila[np];
                pila[np] = if in_.op == LOBO_Y {
                    (a != 0 && b != 0) as i32
                } else {
                    (a != 0 || b != 0) as i32
                };
                np += 1;
            }
            LOBO_NO => {
                if np < 1 {
                    maquina.agotado = true;
                    return;
                }
                pila[np - 1] = (pila[np - 1] == 0) as i32;
            }
            LOBO_ASIGNA => {
                if np < 1 {
                    maquina.agotado = true;
                    return;
                }
                np -= 1;
                escribir_var(estado, maquina, in_.valor, pila[np]);
            }
            LOBO_SALTO => {
                let destino = in_.valor;
                if destino < 0 || destino as usize > programa.largo {
                    maquina.agotado = true; /* programa corrupto: no pasa */
                    return;
                }
                i = destino as usize;
                continue;
            }
            LOBO_SALTO_FALSO => {
                if np < 1 {
                    maquina.agotado = true;
                    return;
                }
                np -= 1;
                if pila[np] == 0 {
                    let destino = in_.valor;
                    if destino < 0 || destino as usize > programa.largo {
                        maquina.agotado = true; /* programa corrupto */
                        return;
                    }
                    i = destino as usize;
                    continue;
                }
            }
            LOBO_POP => {
                if np > 0 {
                    np -= 1;
                }
            }
            _ => {
                maquina.agotado = true;
                return;
            }
        }
        i += 1;
    }
}
