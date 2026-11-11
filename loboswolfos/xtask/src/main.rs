//! Constructor de la imagen USB de loboswolfos.
//!
//! Compila los dos tramos del arranque y el kernel (Rust `no_std` para
//! `x86_64-unknown-none`), los pasa a binario plano con `llvm-objcopy`,
//! parchea el bloque `RKYCFG` del segundo tramo con el tamano del kernel,
//! escribe la tabla de particiones del MBR y monta `loboswolfos.img`:
//!
//!   LBA 0       MBR (`bootusb`, 512 bytes, con la tabla de particiones)
//!   LBA 1..4    segundo tramo (`bootusb2`, cargado en 0x8000)
//!   LBA 5..     kernel (copiado a 0x100000)
//!   LBA 2048..  zona del juego: el blob LGB1 con el proyecto del editor
//!               (`.logic` del gestor, `.mav` de `maps\` y sus tiles PNG)
//!
//! Basado en el `xtask` de rocky; el trazado de la imagen y el bloque RKYCFG
//! son identicos para no tocar el ensamblador de arranque.
//!
//! `plantilla` escribe solo la parte de arranque de la imagen (todo lo que
//! va antes de la zona del juego) en `build/plantilla-kernel.bin`: es la
//! plantilla que el editor incrusta como recurso `IDR_KERNEL` para el menu
//! COMPILE, "Compilar para BIOS" (fase 7), igual que `motor/game.exe` lo es
//! para "Compilar para Windows". `probar-imagen <ruta>` prueba una imagen
//! ya montada (la que genera el editor) sin reconstruirla.

use std::env;
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::Duration;

const TARGET: &str = "x86_64-unknown-none";

const SECTOR: usize = 512;

/// Sectores que ocupa la imagen entera (32 MiB): cabe en cualquier pendrive
/// y deja sitio de sobra para el blob del juego de la fase 6.
const IMAGEN_SECTORES: usize = 32 * 1024 * 1024 / SECTOR;

/// LBA del segundo tramo del arranque.
const TRAMO2_LBA: u32 = 1;
/// Sectores que el MBR lee de golpe al cargar el segundo tramo (el DAP del
/// primer tramo pide 4).
const TRAMO2_SECTORES: u32 = 4;
/// LBA donde empieza el kernel en la imagen.
const KERNEL_LBA: u32 = TRAMO2_LBA + TRAMO2_SECTORES; // 5
/// Marca del bloque de configuracion que parchea este constructor.
const MARCA_CFG: &[u8; 6] = b"RKYCFG";

/// Zona reservada al blob del juego (fase 6).
///
/// Hoy solo aparece en la tabla de particiones y en el bloque de arranque
/// que el tramo 2 deja en 0x7E00: los campos que en rocky apuntaban al
/// volumen Estrato se reutilizan para apuntar aqui.
const JUEGO_LBA: u32 = 2048;
const JUEGO_SECTORES: u32 = (IMAGEN_SECTORES - JUEGO_LBA as usize) as u32;

fn main() {
    let args: Vec<String> = env::args().collect();
    let command = args.get(1).map(String::as_str).unwrap_or("build");
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .expect("sin directorio raiz")
        .to_path_buf();

    match command {
        "build" => {
            build(&root);
        }
        "run" => {
            build(&root);
            run_qemu(&root, true);
        }
        "run-serial" => {
            build(&root);
            run_qemu(&root, false);
        }
        // Prueba automatica de la imagen: QEMU sin ventana, la salida de
        // serie a build/serial.log, y al terminar se vuelca el registro.
        "probar" => {
            build(&root);
            let segundos: u64 = args.get(2).and_then(|s| s.parse().ok()).unwrap_or(15);
            probar(&root, segundos);
        }
        // Prueba del juego (fase 6): arranca la imagen sin ventana, mueve
        // el sprite con sendkey por el monitor de QEMU y comprueba en la
        // serie que el mapa cargo y el sprite se movio.
        "probar-juego" => {
            build(&root);
            probar_juego(&root);
        }
        // Prueba de una imagen ya montada (fase 7): la que genera el
        // editor con "Compilar para BIOS". Igual que probar-juego, pero
        // sin reconstruir nada: la ruta va tal cual (relativa al
        // directorio desde el que se llama).
        "probar-imagen" => {
            let ruta = args.get(2).map(String::as_str).unwrap_or_else(|| {
                eprintln!("uso: xtask probar-imagen <ruta de la imagen>");
                std::process::exit(1);
            });
            probar_imagen(&root, Path::new(ruta), false);
        }
        // Plantilla del kernel para el editor (fase 7): los bytes de la
        // imagen antes de la zona del juego, que el editor incrusta como
        // recurso IDR_KERNEL y completa con el blob del proyecto.
        "plantilla" => plantilla(&root),
        "clean" => clean(&root),
        other => {
            eprintln!(
                "uso: xtask [build|run|run-serial|probar [segundos]|probar-juego|probar-imagen <ruta>|plantilla|clean] (recibi '{other}')"
            );
            std::process::exit(1);
        }
    }
}

/// Compila los dos tramos del arranque y el kernel, y los devuelve ya
/// montados como la **plantilla** de la imagen: los bytes desde el LBA 0
/// hasta el final del kernel, es decir, todo lo que va antes de la zona
/// del juego. El MBR lleva escrita su tabla de particiones y el bloque
/// RKYCFG del segundo tramo queda parcheado con los sectores del kernel
/// y la zona del juego. Devuelve tambien los sectores que ocupa el
/// kernel (para los mensajes).
fn compilar_base(root: &Path, objcopy: &Path) -> (Vec<u8>, usize) {
    let out = root.join("build");
    fs::create_dir_all(&out).expect("no se pudo crear build/");

    // --- Kernel: binario plano que vive en 1 MiB ---
    let kernel_elf = compile_crate(
        root,
        "kernel",
        "loboswolfos",
        &root.join("kernel/kernel.ld"),
    );
    let kernel_bin = out.join("kernel.bin");
    objcopy_binary(objcopy, &kernel_elf, &kernel_bin);
    let kernel = fs::read(&kernel_bin).expect("sin kernel.bin");

    let kernel_sectores = kernel.len().div_ceil(SECTOR);
    if kernel_sectores > u16::MAX as usize {
        panic!("el kernel ocupa {kernel_sectores} sectores y no cabe en 16 bits");
    }
    if KERNEL_LBA as usize + kernel_sectores > JUEGO_LBA as usize {
        panic!(
            "el kernel ({kernel_sectores} sectores desde el LBA {KERNEL_LBA}) invade la zona del juego (LBA {JUEGO_LBA})"
        );
    }

    // --- Segundo tramo: el que carga el kernel y entra en modo largo ---
    let mut tramo2 = compilar_tramo2(root, objcopy);
    // Bloque de configuracion: sectores del kernel y zona del juego (en
    // rocky estos campos apuntaban al volumen Estrato del disco).
    parchear_cfg(
        &mut tramo2,
        kernel_sectores as u16,
        JUEGO_LBA,
        JUEGO_SECTORES,
    );

    // --- Primer tramo: el sector de arranque (512 bytes) ---
    let bootusb_elf = compile_crate(root, "boot", "bootusb", &root.join("boot/bootusb.ld"));
    let bootusb_bin = out.join("bootusb.bin");
    objcopy_binary(objcopy, &bootusb_elf, &bootusb_bin);
    let mut boot = fs::read(&bootusb_bin).expect("sin bootusb.bin");
    if boot.len() != SECTOR {
        panic!(
            "el sector de arranque USB mide {} bytes (deberian ser 512)",
            boot.len()
        );
    }
    escribir_tabla(
        &mut boot,
        &[Particion {
            activa: true,
            tipo: 0x83,
            lba: JUEGO_LBA,
            sectores: JUEGO_SECTORES,
        }],
    );

    // La plantilla es la concatenacion exacta de las tres piezas: MBR
    // (512 bytes), segundo tramo (4 sectores, ya rellenado) y kernel
    // detras. En la imagen ocupan LBA 0, LBA 1..4 y LBA 5.., que son
    // posiciones contiguas, asi que pegarlas equivale al trazado.
    let mut plantilla = Vec::with_capacity(boot.len() + tramo2.len() + kernel.len());
    plantilla.extend_from_slice(&boot);
    plantilla.extend_from_slice(&tramo2);
    plantilla.extend_from_slice(&kernel);
    (plantilla, kernel_sectores)
}

/// Compila todo y monta `loboswolfos.img`. Devuelve su ruta.
fn build(root: &Path) -> PathBuf {
    let objcopy = find_llvm_objcopy();
    let (plantilla, kernel_sectores) = compilar_base(root, &objcopy);

    // --- La imagen ---
    let mut img = vec![0u8; IMAGEN_SECTORES * SECTOR];
    // LBA 0..: arranque y kernel. La plantilla ya trae el MBR con su
    // tabla de particiones, el segundo tramo parcheado y el kernel.
    img[..plantilla.len()].copy_from_slice(&plantilla);
    // LBA 2048..: la zona del juego con el blob LGB1 del proyecto.
    let blob = montar_blob(&root);
    let joff = JUEGO_LBA as usize * SECTOR;
    let jmax = JUEGO_SECTORES as usize * SECTOR;
    if blob.len() > jmax {
        panic!(
            "el blob del proyecto mide {} bytes y la zona del juego solo tiene {}",
            blob.len(),
            jmax
        );
    }
    img[joff..joff + blob.len()].copy_from_slice(&blob);

    let path = root.join("loboswolfos.img");
    fs::write(&path, &img).expect("no se pudo escribir loboswolfos.img");
    println!(
        "loboswolfos: imagen {} ({} bytes; kernel {} sectores; juego {} sectores).",
        path.display(),
        img.len(),
        kernel_sectores,
        blob.len() / SECTOR
    );
    path
}

/// Escribe la plantilla del kernel en `build/plantilla-kernel.bin`: la
/// parte de arranque de la imagen, sin la zona del juego (fase 7).
///
/// El editor la incrusta como recurso `IDR_KERNEL` (menu COMPILE,
/// "Compilar para BIOS") y completa el resto de la imagen el solo:
/// ceros hasta el LBA 2048, el blob LGB1 del proyecto activo y ceros
/// hasta los 32 MiB, igual que `montar_blob` hace aqui.
fn plantilla(root: &Path) {
    let objcopy = find_llvm_objcopy();
    let (bytes, kernel_sectores) = compilar_base(root, &objcopy);
    let path = root.join("build").join("plantilla-kernel.bin");
    fs::write(&path, &bytes).expect("no se pudo escribir la plantilla del kernel");
    println!(
        "loboswolfos: plantilla {} ({} bytes; kernel {} sectores; zona del juego en LBA {JUEGO_LBA}).",
        path.display(),
        bytes.len(),
        kernel_sectores
    );
}

/* ------------------------------------------------------------------ */
/* Zona del juego: el blob LGB1                                        */
/* ------------------------------------------------------------------ */

/// Lo que lee el kernel como maximo (BLOB_MAX de kernel/src/activo.rs).
const BLOB_MAX: usize = 1024 * 1024;

/// Monta el blob LGB1 con el proyecto del editor: el mismo formato que
/// `compilar.c` pega detras de la plantilla al fabricar `game.exe`
/// (entradas `u32 largo_nombre, u32 largo_datos, nombre, datos` y pie
/// `"LGB1"` + `u32 largo`), con dos ajustes para el arranque por BIOS:
///
///   - los nombres de entrada llevan barra normal ("maps/mapa.mav"),
///     que es como los busca el kernel (activo.rs),
///   - el blob se rellena con ceros hasta un multiplo de sector y el
///     largo del pie cuenta el blob entero, pie incluido: el kernel lee
///     la zona sector a sector y para cuando el pie cuadra con lo
///     leido.
///
/// El contenido sale del proyecto del editor (la carpeta de encima de
/// loboswolfos): la logica activa `proyecto.logic` si existe, todos los
/// `.mav` de `maps\\` y todos los `.png` de `tiles\\`. Sin logica el
/// kernel cae al mapa por defecto (`maps/mapa.mav`), igual que
/// `activo.c` en Windows.
fn montar_blob(root: &Path) -> Vec<u8> {
    let repo = root.parent().expect("sin carpeta del editor sobre loboswolfos");
    let mut blob: Vec<u8> = Vec::new();

    let logic = repo.join("proyecto.logic");
    if logic.is_file() {
        agregar_entrada(&mut blob, "proyecto.logic", &logic);
    } else {
        // Sin logica del editor en el raiz: una de prueba con un boton
        // FUNCIONES (fase 8), para que las pruebas automaticas ejerzan
        // el lobo tambien en la BIOS.
        agregar_entrada_datos(&mut blob, "proyecto.logic", &logica_de_prueba(repo));
    }
    for nombre in listar_carpeta(&repo.join("maps"), "mav") {
        agregar_entrada(&mut blob, &format!("maps/{nombre}"), &repo.join("maps").join(&nombre));
    }
    for nombre in listar_carpeta(&repo.join("tiles"), "png") {
        agregar_entrada(&mut blob, &format!("tiles/{nombre}"), &repo.join("tiles").join(&nombre));
    }

    if blob.is_empty() {
        panic!(
            "no hay proyecto que montar en {} (se espera maps\\*.mav y tiles\\*.png)",
            repo.display()
        );
    }

    // Pie al final del ultimo sector de la zona (rellenando con ceros).
    let largo = (blob.len() + 8).div_ceil(SECTOR) * SECTOR;
    if largo > BLOB_MAX {
        panic!(
            "el proyecto del editor mide {} bytes y el kernel solo carga {BLOB_MAX}",
            largo
        );
    }
    blob.resize(largo - 8, 0);
    blob.extend_from_slice(b"LGB1");
    blob.extend_from_slice(&(largo as u32).to_le_bytes());
    blob
}

/// Anade una entrada al blob (cabecera + nombre + datos).
fn agregar_entrada(blob: &mut Vec<u8>, nombre: &str, ruta: &Path) {
    let datos = fs::read(ruta)
        .unwrap_or_else(|e| panic!("no se pudo leer {} ({}): {e}", ruta.display(), nombre));
    agregar_entrada_datos(blob, nombre, &datos);
}

/// Lo mismo con los datos ya en memoria (la logica de prueba se genera
/// al vuelo, sin tocar el disco).
fn agregar_entrada_datos(blob: &mut Vec<u8>, nombre: &str, datos: &[u8]) {
    println!("loboswolfos: blob += {} ({} bytes)", nombre, datos.len());
    blob.extend_from_slice(&(nombre.len() as u32).to_le_bytes());
    blob.extend_from_slice(&(datos.len() as u32).to_le_bytes());
    blob.extend_from_slice(nombre.as_bytes());
    blob.extend_from_slice(datos);
}

/// proyecto.logic LOGIC2 de prueba (fase 8), para cuando el raiz no
/// tiene logica del editor: dos elementos (el primer mapa de maps\ y
/// un boton FUNCIONES de id 1) y una funcion de codigo lobo que pasea
/// al lobo en circulo y responde a las teclas. El kernel la carga igual
/// que una del editor.
fn logica_de_prueba(repo: &Path) -> Vec<u8> {
    let mapa = listar_carpeta(&repo.join("maps"), "mav")
        .into_iter()
        .next()
        .unwrap_or_default();
    // El mismo formato que logica_guardar_memoria del editor: lineas
    // LF, codigo crudo con su largo exacto y un separador final.
    let codigo = "\
# paseo autonomo del lobo (fase 8)\n\
vel = 3\n\
si tecla_izquierda\n\
x = x - vel\n\
fin\n\
si tecla_derecha\n\
x = x + vel\n\
fin\n\
si tecla_arriba\n\
y = y - vel\n\
fin\n\
si tecla_abajo\n\
y = y + vel\n\
fin\n\
si paso < 90\n\
x = x + 1\n\
sino\n\
si paso < 180\n\
y = y + 1\n\
sino\n\
paso = 0\n\
fin\n\
fin\n\
paso = paso + 1\n";

    let mut logic = String::new();
    logic += "LOGIC2\n";
    logic += "0 0\n";
    logic += "2\n";
    logic += &format!("0 0 200 150 1 0 \"{mapa}\" -1\n");
    logic += "220 0 100 100 2 0 \"\" 1\n";
    logic += "1\n";
    logic += &format!("1 \"paseo\" {}\n", codigo.len());
    logic += codigo;
    logic += "\n";
    logic.into_bytes()
}

/// Nombres de los archivos de una carpeta con esa extension, en orden
/// alfabetico (read_dir no promete ninguno).
fn listar_carpeta(dir: &Path, ext: &str) -> Vec<String> {
    let entradas = fs::read_dir(dir)
        .unwrap_or_else(|e| panic!("no se pudo abrir {} (*.{ext}): {e}", dir.display()));
    let mut nombres: Vec<String> = entradas
        .flatten()
        .filter(|e| {
            e.path()
                .extension()
                .map(|x| x.eq_ignore_ascii_case(ext))
                .unwrap_or(false)
        })
        .filter_map(|e| e.file_name().into_string().ok())
        .collect();
    nombres.sort();
    nombres
}

/// Compila el segundo tramo del arranque (`bootusb2.bin`) y lo devuelve con
/// el tamano que le reservan los cargadores (cuatro sectores).
fn compilar_tramo2(root: &Path, objcopy: &Path) -> Vec<u8> {
    let out = root.join("build");
    let tramo2_elf = compile_crate(root, "boot", "bootusb2", &root.join("boot/bootusb2.ld"));
    let tramo2_bin = out.join("bootusb2.bin");
    objcopy_binary(objcopy, &tramo2_elf, &tramo2_bin);
    let mut tramo2 = fs::read(&tramo2_bin).expect("sin bootusb2.bin");
    let bytes = TRAMO2_SECTORES as usize * SECTOR;
    if tramo2.len() > bytes {
        panic!(
            "el segundo tramo mide {} bytes y los cargadores solo leen {bytes} ({TRAMO2_SECTORES} sectores)",
            tramo2.len()
        );
    }
    tramo2.resize(bytes, 0);
    tramo2
}

/// Escribe `kernel_sectors` (u16), `juego_lba` (u32) y `juego_sectores` (u32)
/// justo detras de la marca `RKYCFG` del segundo tramo de arranque.
fn parchear_cfg(sector: &mut [u8], kernel_sectors: u16, juego_lba: u32, juego_sectores: u32) {
    let pos = sector
        .windows(MARCA_CFG.len())
        .position(|w| w == &MARCA_CFG[..])
        .expect("no se encontro la marca RKYCFG en el segundo tramo del arranque USB");
    let mut off = pos + MARCA_CFG.len();
    sector[off..off + 2].copy_from_slice(&kernel_sectors.to_le_bytes());
    off += 2;
    sector[off..off + 4].copy_from_slice(&juego_lba.to_le_bytes());
    off += 4;
    sector[off..off + 4].copy_from_slice(&juego_sectores.to_le_bytes());
}

/// Una entrada de la tabla de particiones del MBR.
struct Particion {
    /// Marca de arranque (`0x80`).
    activa: bool,
    /// Tipo de particion (`0x83` Linux/datos).
    tipo: u8,
    /// LBA del primer sector.
    lba: u32,
    /// Sectores que ocupa.
    sectores: u32,
}

/// Escribe la tabla de particiones del MBR (cuatro entradas, bytes 446..510).
///
/// Solo toca la tabla y la firma: el resto del sector (el codigo de arranque)
/// se respeta, que es justo lo que necesita `bootusb.s`.
fn escribir_tabla(sector: &mut [u8], partes: &[Particion]) {
    const P: usize = 446;
    assert!(
        partes.len() <= 4,
        "el MBR solo tiene sitio para cuatro particiones"
    );
    for byte in &mut sector[P..P + 64] {
        *byte = 0;
    }
    for (i, p) in partes.iter().enumerate() {
        let o = P + i * 16;
        sector[o] = if p.activa { 0x80 } else { 0x00 };
        sector[o + 1..o + 4].copy_from_slice(&chs(p.lba));
        sector[o + 4] = p.tipo;
        sector[o + 5..o + 8].copy_from_slice(&chs(p.lba + p.sectores - 1));
        sector[o + 8..o + 12].copy_from_slice(&p.lba.to_le_bytes());
        sector[o + 12..o + 16].copy_from_slice(&p.sectores.to_le_bytes());
    }
    sector[510] = 0x55;
    sector[511] = 0xAA;
}

/// CHS aproximado de una LBA (el que pondria cualquier herramienta).
///
/// Los discos grandes se salen del CHS, que solo llega a 1023 cilindros; en
/// ese caso se pone el tope (`0xFE 0xFF 0xFF`), que es lo que hacen los
/// formateadores: la LBA es la que manda.
fn chs(lba: u32) -> [u8; 3] {
    let cil = lba / (255 * 63);
    let resto = lba % (255 * 63);
    let cab = resto / 63;
    let sec = (resto % 63) + 1;
    if cil > 1023 {
        return [0xFE, 0xFF, 0xFF];
    }
    [
        cab as u8,
        ((sec & 0x3F) | ((cil >> 8) << 6)) as u8,
        (cil & 0xFF) as u8,
    ]
}

/// Compila un binario de una crate del proyecto para `x86_64-unknown-none`
/// con el script de enlazado indicado y devuelve la ruta del ELF.
fn compile_crate(root: &Path, dir: &str, bin: &str, linker: &Path) -> PathBuf {
    let crate_dir = root.join(dir);
    let manifest = crate_dir.join("Cargo.toml");

    let rustflags = format!(
        "-Crelocation-model=static\x1f-Cpanic=abort\x1f-Ccode-model=kernel\x1f-Clink-arg=-T\x1f-Clink-arg={}",
        linker.display()
    );

    // Los flags van por CARGO_ENCODED_RUSTFLAGS (separados por \x1f): la ruta
    // de este repositorio lleva espacios ("GreenBlue kit") y con RUSTFLAGS
    // normal, que cargo parte por espacios, el -Clink-arg del linker se
    // romperia en dos.
    let status = Command::new("cargo")
        .arg("build")
        .arg("--release")
        .arg("--target")
        .arg(TARGET)
        .arg("--manifest-path")
        .arg(&manifest)
        .arg("--bin")
        .arg(bin)
        .env("CARGO_ENCODED_RUSTFLAGS", rustflags)
        .status()
        .expect("no se pudo ejecutar cargo");

    if !status.success() {
        panic!("fallo al compilar '{dir}'");
    }

    let elf = crate_dir
        .join("target")
        .join(TARGET)
        .join("release")
        .join(bin);
    if !elf.exists() {
        panic!("no se encontro el ELF esperado en {}", elf.display());
    }
    elf
}

/// Convierte un ELF en binario plano (solo las secciones cargables).
fn objcopy_binary(objcopy: &Path, elf: &Path, out: &Path) {
    let status = Command::new(objcopy)
        .arg("-O")
        .arg("binary")
        .arg(elf)
        .arg(out)
        .status()
        .expect("no se pudo ejecutar llvm-objcopy");
    if !status.success() {
        panic!("llvm-objcopy fallo para {}", elf.display());
    }
}

/// Busca el `llvm-objcopy` que viene con la toolchain de Rust.
fn find_llvm_objcopy() -> PathBuf {
    let sysroot = Command::new("rustc")
        .arg("--print")
        .arg("sysroot")
        .output()
        .expect("no se pudo ejecutar rustc");
    let sysroot = PathBuf::from(String::from_utf8_lossy(&sysroot.stdout).trim());

    let rustlib = sysroot.join("lib").join("rustlib");
    if let Ok(entries) = fs::read_dir(&rustlib) {
        for entry in entries.flatten() {
            let candidate = entry.path().join("bin").join("llvm-objcopy.exe");
            if candidate.exists() {
                return candidate;
            }
        }
    }

    // Ultimo recurso: confiar en el PATH.
    PathBuf::from("llvm-objcopy")
}

/// Busca el QEMU del sistema en los sitios habituales.
fn find_qemu() -> PathBuf {
    let mut candidates: Vec<PathBuf> = Vec::new();

    // Instalacion por usuario: %LOCALAPPDATA%\Programs\qemu\...
    if let Ok(local) = env::var("LOCALAPPDATA") {
        candidates.push(
            PathBuf::from(local)
                .join("Programs")
                .join("qemu")
                .join("qemu-system-x86_64.exe"),
        );
    }
    // Instalaciones clasicas del sistema.
    candidates.push(PathBuf::from(
        r"C:\Program Files\qemu\qemu-system-x86_64.exe",
    ));
    candidates.push(PathBuf::from(
        r"C:\Program Files (x86)\qemu\qemu-system-x86_64.exe",
    ));

    for c in candidates {
        if c.exists() {
            return c;
        }
    }

    // Ultimo recurso: confiar en el PATH.
    PathBuf::from("qemu-system-x86_64")
}

/// Arranca la imagen en QEMU como disco duro de arranque.
fn run_qemu(root: &Path, window: bool) {
    let qemu = find_qemu();
    let img = root.join("loboswolfos.img");

    let mut cmd = Command::new(&qemu);
    cmd.arg("-m")
        .arg("256")
        .arg("-boot")
        .arg("order=c") // arranca del disco duro: el MBR de la imagen
        .arg("-drive")
        .arg(format!(
            "if=ide,index=0,media=disk,format=raw,file={}",
            img.display()
        ))
        .arg("-serial")
        .arg("stdio")
        .arg("-no-reboot");

    if window {
        cmd.arg("-vga").arg("std");
    } else {
        cmd.arg("-display").arg("none");
    }

    println!("Ejecutando: {} (Ctrl+C para salir)", qemu.display());
    let status = match cmd.status() {
        Ok(status) => status,
        Err(err) => {
            eprintln!();
            eprintln!("No se pudo ejecutar QEMU ({}).", err);
            eprintln!("Instala QEMU o deja el ejecutable en uno de estos sitios:");
            eprintln!("  - %LOCALAPPDATA%\\Programs\\qemu\\qemu-system-x86_64.exe");
            eprintln!("  - C:\\Program Files\\qemu\\qemu-system-x86_64.exe");
            eprintln!("  - en el PATH");
            std::process::exit(1);
        }
    };
    std::process::exit(status.code().unwrap_or(0));
}

/// Prueba la imagen sin ventana: QEMU vuelca la serie a `build/serial.log`
/// durante `segundos`, se detiene y el registro se imprime por la salida
/// estandar. Es la "verificacion por pixeles" del kernel: lo que cuenta es
/// el rastro del arranque en la serie.
fn probar(root: &Path, segundos: u64) {
    let qemu = find_qemu();
    let img = root.join("loboswolfos.img");
    let log = root.join("build").join("serial.log");
    if log.exists() {
        fs::remove_file(&log).expect("no se pudo borrar el registro anterior");
    }

    let mut cmd = Command::new(&qemu);
    cmd.arg("-m")
        .arg("256")
        .arg("-boot")
        .arg("order=c")
        .arg("-drive")
        .arg(format!(
            "if=ide,index=0,media=disk,format=raw,file={}",
            img.display()
        ))
        .arg("-serial")
        .arg(format!("file:{}", log.display()))
        .arg("-display")
        .arg("none")
        .arg("-no-reboot");

    println!("loboswolfos: probando la imagen {} s sin ventana...", segundos);
    let mut hijo = cmd.spawn().expect("no se pudo ejecutar QEMU");
    std::thread::sleep(std::time::Duration::from_secs(segundos));
    let _ = hijo.kill();
    let _ = hijo.wait();

    match fs::read_to_string(&log) {
        Ok(texto) => {
            println!("---- build/serial.log ----");
            println!("{texto}");
            println!("---- fin del registro ----");
        }
        Err(_) => eprintln!(
            "no se pudo leer {} (¿arranco QEMU?)",
            log.display()
        ),
    }
}

/// Prueba automatica del juego (fase 6): arranca la imagen sin ventana
/// con la serie a `build/serial.log` y el monitor por la entrada
/// estandar, para mover el sprite con `sendkey` (con tiempo de retencion:
/// la tecla queda apretada una fraccion de segundo y el juego corre
/// varios fotogramas con ella). Al final comprueba en el registro que el
/// mapa cargo, que el juego arranco y que el sprite se movio de su sitio
/// de nacimiento.
fn probar_juego(root: &Path) {
    probar_imagen(root, &root.join("loboswolfos.img"), true);
}

/// Lo mismo que `probar_juego` pero con la imagen que se le pase (fase
/// 7): sirve para probar la que genera el editor con "Compilar para
/// BIOS" sin pasar por `build`. `exigir_lobo` (fase 8) exige ademas que
/// los scripts lobo se hayan preparado: la imagen del constructor
/// siempre lleva la logica de prueba, pero la del editor puede no
/// llevar funciones.
fn probar_imagen(root: &Path, img: &Path, exigir_lobo: bool) {
    let qemu = find_qemu();
    let log = root.join("build").join("serial.log");
    fs::create_dir_all(root.join("build")).expect("no se pudo crear build/");
    if !img.exists() {
        eprintln!("loboswolfos: no encuentro la imagen {}.", img.display());
        std::process::exit(1);
    }
    if log.exists() {
        fs::remove_file(&log).expect("no se pudo borrar el registro anterior");
    }

    let mut cmd = Command::new(&qemu);
    cmd.arg("-m")
        .arg("256")
        .arg("-boot")
        .arg("order=c")
        .arg("-drive")
        .arg(format!(
            "if=ide,index=0,media=disk,format=raw,file={}",
            img.display()
        ))
        .arg("-serial")
        .arg(format!("file:{}", log.display()))
        .arg("-monitor")
        .arg("stdio")
        .arg("-display")
        .arg("none")
        .arg("-no-reboot");

    println!("loboswolfos: probando el juego sin ventana...");
    let mut hijo = cmd
        .stdin(Stdio::piped())
        .spawn()
        .expect("no se pudo ejecutar QEMU");
    let mut monitor = hijo.stdin.take().expect("sin entrada para el monitor de QEMU");

    // Arranque: kernel, modo VBE, lectura del blob y decodificado de los
    // tiles (todo por la serie antes de este plazo).
    std::thread::sleep(Duration::from_secs(8));

    // Mover el sprite en las cuatro direcciones, 300 ms apretada cada una
    // (a 60 fotogramas/s son ~18 pasos de 3 pixeles). Si una direccion
    // esta tapada por un muro, el juego desliza en los otros ejes.
    for tecla in ["right", "down", "left", "up"] {
        if writeln!(monitor, "sendkey {tecla} 300").is_err() {
            eprintln!("loboswolfos: el monitor de QEMU se cerro antes de tiempo.");
            break;
        }
        let _ = monitor.flush();
        std::thread::sleep(Duration::from_millis(500));
    }

    // Espera a que el rastro por segundo deje constancia del movimiento.
    std::thread::sleep(Duration::from_secs(3));
    let _ = hijo.kill();
    let _ = hijo.wait();
    drop(monitor);

    let texto = fs::read_to_string(&log)
        .unwrap_or_else(|_| panic!("no se pudo leer {} (¿arranco QEMU?)", log.display()));
    println!("---- build/serial.log ----");
    println!("{texto}");
    println!("---- fin del registro ----");

    // Comprobaciones de la fase 6 (el rastro sale por la serie).
    let mut fallos: Vec<&str> = Vec::new();

    if !texto.contains("egine: mapa ") {
        fallos.push("el mapa no cargo (falta \"egine: mapa\")");
    }
    if !texto.contains("egine: tile ") {
        fallos.push("ningun tile decodifico (falta \"egine: tile\")");
    }
    if !texto.contains("egine: juego listo") {
        fallos.push("el juego no arranco (falta \"egine: juego listo\")");
    }

    let rastro: Vec<&str> = texto
        .lines()
        .filter(|l| l.starts_with("egine: sprite en ("))
        .collect();
    if rastro.is_empty() {
        fallos.push("el rastro del sprite no aparecio (falta \"egine: sprite en\")");
    } else if rastro.iter().all(|l| *l == rastro[0]) {
        fallos.push("el sprite no se movio (el rastro no cambia)");
    }

    // Comprobaciones del lobo (fase 8): ningun script puede fallar al
    // compilar (los errores salen por la serie) y, en la imagen del
    // constructor, tiene que haber la linea de preparacion.
    if texto.contains("egine: lobo: error ") {
        fallos.push("un script lobo no compilo (\"egine: lobo: error\")");
    }
    if exigir_lobo
        && !texto
            .lines()
            .any(|l| l.starts_with("egine: lobo: ") && l.ends_with(" scripts."))
    {
        fallos.push("los scripts lobo no se prepararon (falta \"egine: lobo: N scripts\")");
    }

    if fallos.is_empty() {
        println!("loboswolfos: prueba del juego SUPERADA.");
    } else {
        for fallo in &fallos {
            eprintln!("loboswolfos: FALLO: {fallo}");
        }
        std::process::exit(1);
    }
}

/// Borra los destinos de compilacion, el directorio de build y la imagen.
fn clean(root: &Path) {
    for dir in ["boot", "kernel"] {
        let _ = fs::remove_dir_all(root.join(dir).join("target"));
    }
    let _ = fs::remove_dir_all(root.join("build"));
    let _ = fs::remove_file(root.join("loboswolfos.img"));
    println!("loboswolfos: limpio.");
}
