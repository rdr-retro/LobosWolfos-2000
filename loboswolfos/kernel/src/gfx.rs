//! Graficos 2D: modos VESA VBE y un lienzo de pixeles sobre el framebuffer.
//!
//! Para cambiar de resolucion se pregunta al BIOS (via `crate::bios`) por la
//! lista de modos VBE, se inspecciona cada uno y se activa el que interesa. El
//! framebuffer resultante es lineal (`PhysBasePtr`), mapeado por identidad, de
//! modo que el kernel puede escribir en el directamente.

use crate::bios::{self, Regs};
use crate::lobo;
use crate::println;

/// Buffer de 512 bytes para el bloque de informacion del controlador VBE.
const INFO_ADDR: usize = 0x8000;
const INFO_SEG: u16 = INFO_ADDR as u16 >> 4;
/// Buffer de 256 bytes para el bloque de informacion de modo VBE.
const MODE_ADDR: usize = 0x8300;
const MODE_SEG: u16 = MODE_ADDR as u16 >> 4;

fn rd8(addr: usize) -> u8 {
    unsafe { core::ptr::read_volatile(addr as *const u8) }
}
fn rd16(addr: usize) -> u16 {
    unsafe { core::ptr::read_volatile(addr as *const u16) }
}
fn rd32(addr: usize) -> u32 {
    unsafe { core::ptr::read_volatile(addr as *const u32) }
}

/// Descripcion de un modo grafico VBE.
#[derive(Clone, Copy)]
pub struct ModeInfo {
    pub width: u32,
    pub height: u32,
    pub bpp: u32,
    /// Bytes por linea (pitch).
    pub pitch: u32,
    /// Direccion fisica del framebuffer lineal.
    pub lfb: u32,
}

/// Consulta el bloque del controlador VBE y devuelve el puntero a la lista de
/// modos (direccion fisica). `None` si el BIOS no ofrece VESA.
fn mode_list() -> Option<usize> {
    let r = bios::int10(Regs {
        eax: 0x4F00,
        es: INFO_SEG,
        edi: 0,
        ..Default::default()
    });
    if r.eax as u16 != 0x004F {
        return None;
    }
    let sig = [rd8(INFO_ADDR), rd8(INFO_ADDR + 1), rd8(INFO_ADDR + 2), rd8(INFO_ADDR + 3)];
    if &sig != b"VESA" {
        return None;
    }
    let off = rd16(INFO_ADDR + 14) as usize;
    let seg = rd16(INFO_ADDR + 16) as usize;
    Some(seg * 16 + off)
}

/// Recorre todos los numeros de modo que anuncia el BIOS.
pub fn each_mode<F: FnMut(u16)>(mut f: F) {
    if let Some(mut p) = mode_list() {
        loop {
            let mode = rd16(p);
            if mode == 0xFFFF {
                break;
            }
            f(mode);
            p += 2;
        }
    }
}

/// Rellena el bloque de modo y lo interpreta. Solo acepta modos de color
/// directo (32 bpp) con framebuffer lineal.
pub fn mode_info(mode: u16) -> Option<ModeInfo> {
    let r = bios::int10(Regs {
        eax: 0x4F01,
        ecx: mode as u32, // VBE 2.0+: el modo va en CX
        ebx: mode as u32, // por compatibilidad con BIOS antiguos
        es: MODE_SEG,
        edi: 0,
        ..Default::default()
    });
    if r.eax as u16 != 0x004F {
        return None;
    }

    let attrs = rd16(MODE_ADDR);
    let supported = attrs & 0x0001 != 0; // modo soportado por el hardware
    let linear = attrs & 0x0080 != 0; // framebuffer lineal disponible
    let model = rd8(MODE_ADDR + 27); // 6 = color directo
    let bpp = rd8(MODE_ADDR + 25);
    if !supported || !linear || model != 6 || bpp != 32 {
        return None;
    }

    Some(ModeInfo {
        width: rd16(MODE_ADDR + 18) as u32,
        height: rd16(MODE_ADDR + 20) as u32,
        bpp: bpp as u32,
        pitch: rd16(MODE_ADDR + 16) as u32,
        lfb: rd32(MODE_ADDR + 40),
    })
}

/// Busca un modo de color directo de 32 bpp con la resolucion pedida.
pub fn find_mode(width: u32, height: u32) -> Option<(u16, ModeInfo)> {
    let mut found = None;
    each_mode(|m| {
        if found.is_none() {
            if let Some(mi) = mode_info(m) {
                if mi.width == width && mi.height == height {
                    found = Some((m, mi));
                }
            }
        }
    });
    found
}

/// Activa un modo VBE (con framebuffer lineal) y devuelve su descripcion.
pub fn set_mode(mode: u16) -> Option<ModeInfo> {
    let mi = mode_info(mode)?;
    let r = bios::int10(Regs {
        eax: 0x4F02,
        ebx: mode as u32 | 0x4000, // bit 14: usar el framebuffer lineal
        ..Default::default()
    });
    if r.carry || r.eax as u16 != 0x004F {
        return None;
    }
    Some(mi)
}

/// Resoluciones que intenta usar la demo, de mayor a menor.
pub const DEMO_RESOLUTIONS: [(u32, u32); 6] = [
    (1024, 768),
    (800, 600),
    (640, 480),
    (1280, 720),
    (320, 200),
    (1600, 900),
];

/// Encuentra el primer modo disponible de la lista de la demo.
pub fn find_first_demo_mode() -> Option<(u16, ModeInfo)> {
    for &(w, h) in DEMO_RESOLUTIONS.iter() {
        if let Some(found) = find_mode(w, h) {
            return Some(found);
        }
    }
    None
}

/// Imprime por consola el catalogo de modos VBE de 32 bpp.
pub fn print_modes() {
    if mode_list().is_none() {
        println!("VBE no disponible (se necesita una VGA con VESA).");
        return;
    }
    println!();
    println!("Modos VESA VBE de 32 bpp con framebuffer lineal:");
    println!("  modo    resolucion     en pantalla   LFB fisico");
    println!("  ----    ------------   -----------   ----------");

    let mut count = 0;
    each_mode(|m| {
        if let Some(mi) = mode_info(m) {
            // Solo los que caben comodamente en 32 bits y son razonables.
            if mi.width >= 320 && mi.height >= 200 && mi.width <= 2560 {
                println!(
                    "  0x{:04X}  {:>4}x{:<4}      {:>4}x{:<4}   0x{:08X}",
                    m, mi.width, mi.height, mi.width, mi.height, mi.lfb
                );
                count += 1;
            }
        }
    });

    if count == 0 {
        println!("  (ninguno encontrado)");
    } else {
        println!("  {} modo(s).", count);
    }
    println!();
}

/// Color directo de 32 bpp: 0x00RRGGBB.
#[inline]
pub const fn rgb(r: u8, g: u8, b: u8) -> u32 {
    ((r as u32) << 16) | ((g as u32) << 8) | b as u32
}

/// Numero de `u32` que ocupa el *buffer* trasero de un modo (filas x paso).
pub fn buffer_pixels(mi: &ModeInfo) -> usize {
    (mi.pitch as usize / 4) * mi.height as usize
}

/// Lienzo 2D. Puede dibujar directamente sobre el framebuffer de video o, mejor,
/// sobre un *buffer* en RAM y volcarlo de golpe con [`Canvas::present`] (doble
/// buffer), que evita el parpadeo de los elementos fijos como el texto.
pub struct Canvas {
    ptr: *mut u32,
    /// Framebuffer real de video; `ptr` puede ser un *buffer* intermedio.
    lfb: *mut u32,
    pub width: i32,
    pub height: i32,
    /// Paso de linea en pixeles.
    pub stride: usize,
}

impl Canvas {
    /// Crea un lienzo que dibuja directamente sobre el framebuffer de video.
    pub fn new(mi: &ModeInfo) -> Canvas {
        let lfb = mi.lfb as *mut u32;
        Canvas {
            ptr: lfb,
            lfb,
            width: mi.width as i32,
            height: mi.height as i32,
            stride: (mi.pitch as usize) / 4,
        }
    }

    /// Crea un lienzo con **doble buffer**: se dibuja en `buffer` (memoria del
    /// heap, reservada por el llamante con [`buffer_pixels`]) y
    /// [`Canvas::present`] lo copia al framebuffer. Si `buffer` es nulo, se
    /// dibuja directamente sobre el framebuffer.
    pub fn new_buffered(mi: &ModeInfo, buffer: *mut u32) -> Canvas {
        let lfb = mi.lfb as *mut u32;
        let stride = (mi.pitch as usize) / 4;
        if !buffer.is_null() {
            Canvas {
                ptr: buffer,
                lfb,
                width: mi.width as i32,
                height: mi.height as i32,
                stride,
            }
        } else {
            Canvas::new(mi)
        }
    }

    /// Crea un lienzo sobre un *buffer* de RAM propio (por ejemplo, el contenido
    /// de una ventana). No hay framebuffer de video asociado, de modo que
    /// [`Canvas::present`] no hace nada: el destino y el framebuffer coinciden.
    pub fn from_buffer(ptr: *mut u32, width: i32, height: i32, stride: usize) -> Canvas {
        Canvas {
            ptr,
            lfb: ptr,
            width,
            height,
            stride,
        }
    }

    /// Copia un rectangulo desde otro *buffer* (con paso `src_stride`) a este
    /// lienzo, recortando a los limites de la pantalla. Se copia fila a fila.
    pub fn blit(&self, src: *const u32, src_stride: usize, x: i32, y: i32, w: i32, h: i32) {
        if src.is_null() || w <= 0 || h <= 0 {
            return;
        }
        let x0 = x.max(0);
        let y0 = y.max(0);
        let x1 = (x + w).min(self.width);
        let y1 = (y + h).min(self.height);
        if x0 >= x1 || y0 >= y1 {
            return;
        }
        let n = (x1 - x0) as usize;
        let mut yy = y0;
        while yy < y1 {
            let sy = (yy - y) as usize;
            let sx = (x0 - x) as usize;
            unsafe {
                let dst = self.ptr.add(yy as usize * self.stride + x0 as usize);
                let s = src.add(sy * src_stride + sx);
                core::ptr::copy_nonoverlapping(s, dst, n);
            }
            yy += 1;
        }
    }

    /// Blit transformado (fase 30): aplica flip, rotacion de 0/90/180/270
    /// grados y zoom entero, con el pivote del sprite situado en `(x, y)`.
    /// El origen es un rectangulo de `src` con paso `src_stride`; solo se
    /// escriben los pixeles visibles del lienzo. No usa memoria dinamica.
    pub fn blit_ex(
        &self,
        src: *const u32,
        src_stride: usize,
        x: i32,
        y: i32,
        sx: i32,
        sy: i32,
        w: i32,
        h: i32,
        pivote_x: i32,
        pivote_y: i32,
        flip_x: bool,
        flip_y: bool,
        rotacion: i32,
        zoom: i32,
    ) {
        if src.is_null() || w <= 0 || h <= 0 {
            return;
        }
        let zoom = if zoom < 1 { 1 } else if zoom > 4 { 4 } else { zoom };
        let mut j = 0;
        while j < h {
            let mut i = 0;
            while i < w {
                let ox = sx + i;
                let oy = sy + j;
                if ox >= 0 && oy >= 0 {
                    unsafe {
                        let color = *src.add(oy as usize * src_stride + ox as usize);
                        if color & 0xFF00_0000 != 0 {
                            let fx = if flip_x { w - 1 - i } else { i };
                            let fy = if flip_y { h - 1 - j } else { j };
                            let mut rx = fx - pivote_x;
                            let mut ry = fy - pivote_y;
                            match rotacion {
                                90 => {
                                    let t = rx;
                                    rx = -ry;
                                    ry = t;
                                }
                                180 => {
                                    rx = -rx;
                                    ry = -ry;
                                }
                                270 => {
                                    let t = rx;
                                    rx = ry;
                                    ry = -t;
                                }
                                _ => {}
                            }
                            let px = x + rx * zoom;
                            let py = y + ry * zoom;
                            let mut by = 0;
                            while by < zoom {
                                let dy = py + by;
                                if dy >= 0 && dy < self.height {
                                    let mut bx = 0;
                                    while bx < zoom {
                                        let dx = px + bx;
                                        if dx >= 0 && dx < self.width {
                                            self.put(dx, dy, color);
                                        }
                                        bx += 1;
                                    }
                                }
                                by += 1;
                            }
                        }
                    }
                }
                i += 1;
            }
            j += 1;
        }
    }

    /// Indica si se esta usando doble buffer.
    pub fn is_buffered(&self) -> bool {
        self.ptr != self.lfb
    }

    /// Copia el *buffer* al framebuffer de video (doble buffer). Se sincroniza
    /// con el retrazo vertical para reducir el *tearing*.
    pub fn present(&self) {
        if self.ptr == self.lfb {
            return;
        }
        wait_vblank();
        for y in 0..self.height as usize {
            let off = y * self.stride;
            unsafe {
                core::ptr::copy_nonoverlapping(
                    self.ptr.add(off),
                    self.lfb.add(off),
                    self.width as usize,
                );
            }
        }
    }

    #[inline]
    pub fn put(&self, x: i32, y: i32, color: u32) {
        if x < 0 || y < 0 || x >= self.width || y >= self.height {
            return;
        }
        unsafe {
            let base = self.ptr.add(y as usize * self.stride + x as usize);
            core::ptr::write_volatile(base, color);
        }
    }

    /// Lee el color de un pixel (`0` si el punto queda fuera del lienzo).
    #[inline]
    pub fn get(&self, x: i32, y: i32) -> u32 {
        if x < 0 || y < 0 || x >= self.width || y >= self.height {
            return 0;
        }
        unsafe { core::ptr::read_volatile(self.ptr.add(y as usize * self.stride + x as usize)) }
    }

    /// Mezcla un color con el pixel existente (`alpha` de 0 a 255).
    pub fn blend(&self, x: i32, y: i32, color: u32, alpha: u32) {
        if x < 0 || y < 0 || x >= self.width || y >= self.height {
            return;
        }
        unsafe {
            let p = self.ptr.add(y as usize * self.stride + x as usize);
            let dst = core::ptr::read_volatile(p);
            let r = (color >> 16) & 0xFF;
            let g = (color >> 8) & 0xFF;
            let b = color & 0xFF;
            let dr = (dst >> 16) & 0xFF;
            let dg = (dst >> 8) & 0xFF;
            let db = dst & 0xFF;
            let ia = 256 - alpha;
            let nr = (r * alpha + dr * ia) >> 8;
            let ng = (g * alpha + dg * ia) >> 8;
            let nb = (b * alpha + db * ia) >> 8;
            core::ptr::write_volatile(p, (nr << 16) | (ng << 8) | nb);
        }
    }

    /// Puntero al primer pixel de la fila `y` (sin comprobar limites).
    #[inline]
    fn row_ptr(&self, y: i32) -> *mut u32 {
        unsafe { self.ptr.add(y as usize * self.stride) }
    }

    pub fn hline(&self, x0: i32, x1: i32, y: i32, color: u32) {
        if y < 0 || y >= self.height {
            return;
        }
        let a = x0.min(x1).max(0);
        let b = x0.max(x1).min(self.width - 1);
        if a > b {
            return;
        }
        let row = self.row_ptr(y);
        let mut x = a;
        while x <= b {
            unsafe { core::ptr::write_volatile(row.add(x as usize), color) }
            x += 1;
        }
    }

    pub fn vline(&self, x: i32, y0: i32, y1: i32, color: u32) {
        if x < 0 || x >= self.width {
            return;
        }
        let a = y0.min(y1).max(0);
        let b = y0.max(y1).min(self.height - 1);
        let mut y = a;
        while y <= b {
            unsafe { core::ptr::write_volatile(self.row_ptr(y).add(x as usize), color) }
            y += 1;
        }
    }

    pub fn fill_rect(&self, x: i32, y: i32, w: i32, h: i32, color: u32) {
        if w <= 0 {
            return;
        }
        for yy in y..y + h {
            self.hline(x, x + w - 1, yy, color);
        }
    }

    /// Linea con el algoritmo de Bresenham.
    pub fn line(&self, mut x0: i32, mut y0: i32, x1: i32, y1: i32, color: u32) {
        let dx = (x1 - x0).abs();
        let dy = -(y1 - y0).abs();
        let sx = if x0 < x1 { 1 } else { -1 };
        let sy = if y0 < y1 { 1 } else { -1 };
        let mut err = dx + dy;
        loop {
            self.put(x0, y0, color);
            if x0 == x1 && y0 == y1 {
                break;
            }
            let e2 = 2 * err;
            if e2 >= dy {
                err += dy;
                x0 += sx;
            }
            if e2 <= dx {
                err += dx;
                y0 += sy;
            }
        }
    }

    /// Contorno de un circulo.
    pub fn circle(&self, cx: i32, cy: i32, r: i32, color: u32) {
        let mut x = r;
        let mut y = 0;
        let mut err = 1 - r;
        while x >= y {
            self.put(cx + x, cy + y, color);
            self.put(cx + y, cy + x, color);
            self.put(cx - y, cy + x, color);
            self.put(cx - x, cy + y, color);
            self.put(cx - x, cy - y, color);
            self.put(cx - y, cy - x, color);
            self.put(cx + y, cy - x, color);
            self.put(cx + x, cy - y, color);
            y += 1;
            if err < 0 {
                err += 2 * y + 1;
            } else {
                x -= 1;
                err += 2 * (y - x) + 1;
            }
        }
    }

    /// Disco relleno de color solido.
    pub fn disc(&self, cx: i32, cy: i32, r: i32, color: u32) {
        if r <= 0 {
            return;
        }
        let r2 = r * r;
        for dy in -r..=r {
            let span = isqrt((r2 - dy * dy) as u32) as i32;
            self.hline(cx - span, cx + span, cy + dy, color);
        }
    }

    /// Disco con un borde mas claro para dar sensacion de esfera.
    pub fn sphere(&self, cx: i32, cy: i32, r: i32, color: u32) {
        let r2 = r * r;
        for dy in -r..=r {
            let span = isqrt((r2 - dy * dy) as u32) as i32;
            for dx in -span..=span {
                let d2 = (dx * dx + dy * dy) as u32;
                let shade = 255 - (isqrt(d2) * 160 / (r.max(1) as u32));
                let (cr, cg, cb) = (
                    ((color >> 16) & 0xFF) * shade / 255,
                    ((color >> 8) & 0xFF) * shade / 255,
                    (color & 0xFF) * shade / 255,
                );
                self.put(cx + dx, cy + dy, rgb(cr as u8, cg as u8, cb as u8));
            }
        }
    }
}

/// Espera al inicio del retrazo vertical de la VGA (puerto 0x3DA, bit 3).
/// Lleva un limite de iteraciones por si el emulador no lo genera.
fn wait_vblank() {
    use crate::cpu::inb;
    unsafe {
        // Primero hay que estar fuera del retrazo, luego esperar a que empiece.
        let mut guard = 0u32;
        while inb(0x3DA) & 0x08 != 0 {
            guard += 1;
            if guard > 5_000_000 {
                return;
            }
        }
        guard = 0;
        while inb(0x3DA) & 0x08 == 0 {
            guard += 1;
            if guard > 5_000_000 {
                return;
            }
        }
    }
}

/// Raiz cuadrada entera.
pub fn isqrt(n: u32) -> u32 {
    if n == 0 {
        return 0;
    }
    let mut x = n;
    let mut y = (x + 1) / 2;
    while y < x {
        x = y;
        y = (x + n / x) / 2;
    }
    x
}

/// Ancho en pixels de cada glifo de la fuente del BIOS (VGA = 8).
pub const FONT_W: i32 = 8;

/// Dibuja una cadena usando la fuente de la VGA (1 byte por fila, MSB a la
/// izquierda) devuelta por [`crate::bios::get_font`].
///
/// `scale` amplia cada pixel del glifo (1 = tamano original, 2 = doble, ...).
pub fn draw_text(
    c: &Canvas,
    font: &crate::bios::Font,
    x: i32,
    y: i32,
    s: &str,
    color: u32,
    scale: i32,
) {
    let height = font.bytes_per_char as i32;
    let mut cx = x;
    for ch in s.bytes() {
        let glyph = font.addr + ch as usize * font.bytes_per_char;
        for row in 0..height {
            let bits = unsafe { core::ptr::read_volatile((glyph + row as usize) as *const u8) };
            if bits == 0 {
                continue;
            }
            for col in 0..FONT_W {
                if bits & (0x80 >> col) != 0 {
                    if scale <= 1 {
                        c.put(cx + col, y + row, color);
                    } else {
                        c.fill_rect(cx + col * scale, y + row * scale, scale, scale, color);
                    }
                }
            }
        }
        cx += FONT_W * scale.max(1);
    }
}

/// Igual que [`draw_text`] pero dibuja primero una sombra desplazada.
pub fn draw_text_shadow(
    c: &Canvas,
    font: &crate::bios::Font,
    x: i32,
    y: i32,
    s: &str,
    color: u32,
    scale: i32,
) {
    draw_text(c, font, x + scale, y + scale, s, 0x000000, scale);
    draw_text(c, font, x, y, s, color, scale);
}

/// Longitud en pixels que ocupara `s` al dibujarse con `scale`.
pub fn text_width(s: &str, scale: i32) -> i32 {
    s.len() as i32 * FONT_W * scale.max(1)
}

/* ------------------------------------------------------------------ */
/* Fase 38: fuente 5x7 propia y HUD                                     */
/* ------------------------------------------------------------------ */

/* Fuente bitmap minima, la misma metrica que hud.c de Windows: 7 filas
 * de 5 pixeles, bit 4 a la izquierda y 6 pixeles de avance por letra.
 * No usa la BIOS: la fuente va en datos y funciona tras cambiar de modo. */
#[derive(Clone, Copy)]
struct GlifoHud([u8; 7]);

static G_NUM: [GlifoHud; 10] = [
    GlifoHud([0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E]),
    GlifoHud([0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E]),
    GlifoHud([0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F]),
    GlifoHud([0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E]),
    GlifoHud([0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02]),
    GlifoHud([0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E]),
    GlifoHud([0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E]),
    GlifoHud([0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08]),
    GlifoHud([0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E]),
    GlifoHud([0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C]),
];

static G_LETRAS: [GlifoHud; 26] = [
    GlifoHud([0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11]), // A
    GlifoHud([0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E]), // B
    GlifoHud([0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E]), // C
    GlifoHud([0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E]), // D
    GlifoHud([0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F]), // E
    GlifoHud([0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10]), // F
    GlifoHud([0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E]), // G
    GlifoHud([0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11]), // H
    GlifoHud([0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E]), // I
    GlifoHud([0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C]), // J
    GlifoHud([0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11]), // K
    GlifoHud([0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F]), // L
    GlifoHud([0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11]), // M
    GlifoHud([0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11]), // N
    GlifoHud([0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E]), // O
    GlifoHud([0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10]), // P
    GlifoHud([0x0E, 0x11, 0x11, 0x11, 0x15, 0x0E, 0x01]), // Q
    GlifoHud([0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11]), // R
    GlifoHud([0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E]), // S
    GlifoHud([0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04]), // T
    GlifoHud([0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E]), // U
    GlifoHud([0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04]), // V
    GlifoHud([0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A]), // W
    GlifoHud([0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11]), // X
    GlifoHud([0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04]), // Y
    GlifoHud([0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F]), // Z
];

static G_ESPACIO: GlifoHud = GlifoHud([0, 0, 0, 0, 0, 0, 0]);
static G_DOS_PUNTOS: GlifoHud = GlifoHud([0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x04]);
static G_IGUAL: GlifoHud = GlifoHud([0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00]);
static G_MENOS: GlifoHud = GlifoHud([0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00]);
static G_MAYOR: GlifoHud = GlifoHud([0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02]);
static G_PUNTO: GlifoHud = GlifoHud([0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04]);
static G_DIAGONAL: GlifoHud = GlifoHud([0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10]);
static G_NUMERAL: GlifoHud = GlifoHud([0x0A, 0x0A, 0x1F, 0x0A, 0x0A, 0x00, 0x00]);
static G_INTERROGACION: GlifoHud = GlifoHud([0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04]);
static G_CORCHETE: GlifoHud = GlifoHud([0x0E, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0E]);

const HUD_FONDO: u32 = 0xC010141C;
const HUD_BORDE: u32 = 0xFFD8D8D8;
const HUD_TEXTO: u32 = 0xFFF0F0F0;
const HUD_RESAR: u32 = 0xFFFFC040;
const HUD_BARRA: u32 = 0xFF40C060;
const HUD_BARRA_VACIA: u32 = 0xFF603030;
const HUD_BOTON: u32 = 0xFF305080;

/// Las vocales acentuadas y la ene caen en su letra base: la fuente
/// solo tiene 5x7 y los textos del editor pueden llevar acentos.
fn glifo_hud(ch: u8) -> &'static GlifoHud {
    let c = match ch {
        0xC1 | 0xE1 => b'A',
        0xC9 | 0xE9 => b'E',
        0xCD | 0xED => b'I',
        0xD3 | 0xF3 | 0xD4 | 0xF4 => b'O',
        0xDA | 0xFA => b'U',
        0xD1 | 0xF1 => b'N',
        _ => ch,
    };
    match c {
        b'0'..=b'9' => &G_NUM[(c - b'0') as usize],
        b'a'..=b'z' => &G_LETRAS[(c - b'a') as usize],
        b'A'..=b'Z' => &G_LETRAS[(c - b'A') as usize],
        b':' => &G_DOS_PUNTOS,
        b'=' => &G_IGUAL,
        b'-' => &G_MENOS,
        b'>' => &G_MAYOR,
        b'.' => &G_PUNTO,
        b'/' => &G_DIAGONAL,
        b'#' => &G_NUMERAL,
        b'?' | 0xA1 | 0xBF => &G_INTERROGACION,
        b'[' | b']' => &G_CORCHETE,
        _ => &G_ESPACIO,
    }
}

/// Recorta un bufer fijo hasta su primer 0, como hace strlen en el C.
fn texto_util(texto: &[u8]) -> &[u8] {
    let n = texto.iter().position(|&b| b == 0).unwrap_or(texto.len());
    &texto[..n]
}

/// Dibuja bytes de texto con la fuente 5x7 (`escala` 1 o 2).
fn texto_hud(c: &Canvas, x: i32, y: i32, texto: &[u8], color: u32, escala: i32) {
    let escala = if escala < 1 { 1 } else if escala > 2 { 2 } else { escala };
    let avance = 6 * escala;
    for (n, &ch) in texto_util(texto).iter().enumerate() {
        let g = glifo_hud(ch);
        for fila in 0..7 {
            for col in 0..5 {
                if g.0[fila] & (0x10 >> col) != 0 {
                    c.fill_rect(
                        x + n as i32 * avance + col as i32 * escala,
                        y + fila as i32 * escala,
                        escala,
                        escala,
                        color,
                    );
                }
            }
        }
    }
}

fn ancho_texto_hud(texto: &[u8], escala: i32) -> i32 {
    texto_util(texto).len() as i32 * 6 * escala.max(1)
}

/// Entero a decimal sin formatos (no hay `core::fmt` en este kernel).
fn numero_texto(v: i32, dst: &mut [u8]) -> usize {
    let neg = v < 0;
    let mut n = if neg { -(v as i64) as u64 } else { v as u64 };
    let mut tmp = [0u8; 20];
    let mut k = tmp.len();
    loop {
        k -= 1;
        tmp[k] = b'0' + (n % 10) as u8;
        n /= 10;
        if n == 0 {
            break;
        }
    }
    let mut j = 0;
    if neg {
        dst[j] = b'-';
        j += 1;
    }
    let len = tmp.len() - k;
    dst[j..j + len].copy_from_slice(&tmp[k..]);
    j + len
}

fn borde_hud(c: &Canvas, x: i32, y: i32, w: i32, h: i32, color: u32) {
    if w <= 0 || h <= 0 {
        return;
    }
    c.hline(x, x + w - 1, y, color);
    c.hline(x, x + w - 1, y + h - 1, color);
    c.vline(x, y, y + h - 1, color);
    c.vline(x + w - 1, y, y + h - 1, color);
}

fn cabecera_hud(c: &Canvas, hud: &lobo::HudEstado) {
    c.fill_rect(0, 0, c.width, 22, HUD_FONDO);
    c.hline(0, c.width - 1, 21, HUD_BORDE);

    let mut num = [0u8; 16];
    let n = numero_texto(hud.vidas, &mut num);
    texto_hud(c, 8, 7, b"VIDAS:", HUD_RESAR, 1);
    texto_hud(c, 50, 7, &num[..n], HUD_RESAR, 1);

    let mut puntos = [0u8; 24];
    puntos[..8].copy_from_slice(b"PUNTOS: ");
    let n = numero_texto(hud.puntos, &mut num);
    puntos[8..8 + n].copy_from_slice(&num[..n]);
    let x = (c.width - 8 - ancho_texto_hud(&puntos[..8 + n], 1)).max(8);
    texto_hud(c, x, 7, &puntos[..8 + n], HUD_TEXTO, 1);
}

fn barra_hud(c: &Canvas, e: &lobo::HudElemento) {
    c.fill_rect(e.x, e.y, e.w, e.h, HUD_BARRA_VACIA);
    if e.max > 0 && e.valor > 0 {
        let lleno = (e.w as i64 * e.valor as i64 / e.max as i64)
            .max(0)
            .min(e.w as i64) as i32;
        c.fill_rect(e.x, e.y, lleno, e.h, e.color);
    }
    borde_hud(c, e.x, e.y, e.w, e.h, HUD_BORDE);
}

fn boton_hud(c: &Canvas, e: &lobo::HudElemento) {
    c.fill_rect(e.x, e.y, e.w, e.h, HUD_BOTON);
    borde_hud(c, e.x, e.y, e.w, e.h, HUD_BORDE);
    let tx = e.x + (e.w - ancho_texto_hud(&e.texto, 1)) / 2;
    let ty = e.y + (e.h - 7) / 2;
    texto_hud(c, tx, ty, &e.texto, HUD_TEXTO, 1);
}

fn dialogo_hud(c: &Canvas, hud: &lobo::HudEstado) {
    if hud.dialogo_n == 0 {
        return;
    }
    let y = c.height - 48;
    if y < 24 {
        return;
    }
    c.fill_rect(8, y, c.width - 16, 44, HUD_FONDO);
    borde_hud(c, 8, y, c.width - 16, 44, HUD_BORDE);
    for i in 0..lobo::HUD_DIALOGO_LINEAS {
        let color = if i == hud.dialogo_actual { HUD_RESAR } else { HUD_TEXTO };
        texto_hud(c, 16, y + 7 + i as i32 * 12, &hud.dialogo[i], color, 1);
    }
}

fn plantilla_hud(c: &Canvas, hud: &lobo::HudEstado) {
    if hud.plantilla == 0 {
        return;
    }
    let mut w = c.width / 2;
    let h = 76;
    if w < 180 {
        w = c.width - 16;
    }
    let x = (c.width - w) / 2;
    let y = (c.height - h) / 2;
    c.fill_rect(x, y, w, h, HUD_FONDO);
    borde_hud(c, x, y, w, h, HUD_BORDE);
    if hud.plantilla == 1 {
        texto_hud(c, x + 12, y + 14, b"PAUSA", HUD_RESAR, 2);
    } else {
        texto_hud(c, x + 12, y + 14, b"FIN DEL JUEGO", HUD_RESAR, 2);
    }
    texto_hud(c, x + 12, y + 48, b"PULSA ENTER PARA SEGUIR", HUD_TEXTO, 1);
}

/// Dibuja el HUD sobre el lienzo, justo despues del mundo. No toca la BIOS.
/// Las imagenes no tienen callback en el kernel: se marca su hueco con un
/// recuadro para que la capa sea visible sin dependencias externas.
pub fn dibujar_hud(c: &Canvas, hud: &lobo::HudEstado) {
    if !hud.activo {
        return;
    }
    cabecera_hud(c, hud);
    for e in hud.elementos.iter().take(hud.nelementos) {
        match e.tipo {
            lobo::HUD_ELEM_TEXTO => {
                texto_hud(c, e.x, e.y, &e.texto, e.color, e.extra);
            }
            lobo::HUD_ELEM_RECT => {
                c.fill_rect(e.x, e.y, e.w, e.h, e.color);
            }
            lobo::HUD_ELEM_BARRA => barra_hud(c, e),
            lobo::HUD_ELEM_IMAGEN => {
                c.fill_rect(e.x, e.y, 16, 16, HUD_BOTON);
                borde_hud(c, e.x, e.y, 16, 16, HUD_BORDE);
            }
            lobo::HUD_ELEM_BOTON => boton_hud(c, e),
            _ => {}
        }
    }
    dialogo_hud(c, hud);
    plantilla_hud(c, hud);
}
