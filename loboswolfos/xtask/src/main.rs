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
//!   LBA 2048..  zona del juego (la fase 6 pondra ahi el blob del proyecto)
//!
//! Basado en el `xtask` de rocky; el trazado de la imagen y el bloque RKYCFG
//! son identicos para no tocar el ensamblador de arranque.

use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

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
        "clean" => clean(&root),
        other => {
            eprintln!(
                "uso: xtask [build|run|run-serial|probar [segundos]|clean] (recibi '{other}')"
            );
            std::process::exit(1);
        }
    }
}

/// Compila todo y monta `loboswolfos.img`. Devuelve su ruta.
fn build(root: &Path) -> PathBuf {
    let out = root.join("build");
    fs::create_dir_all(&out).expect("no se pudo crear build/");

    let objcopy = find_llvm_objcopy();

    // --- Kernel: binario plano que vive en 1 MiB ---
    let kernel_elf = compile_crate(
        root,
        "kernel",
        "loboswolfos",
        &root.join("kernel/kernel.ld"),
    );
    let kernel_bin = out.join("kernel.bin");
    objcopy_binary(&objcopy, &kernel_elf, &kernel_bin);
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
    let mut tramo2 = compilar_tramo2(root, &objcopy);
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
    objcopy_binary(&objcopy, &bootusb_elf, &bootusb_bin);
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

    // --- La imagen ---
    let mut img = vec![0u8; IMAGEN_SECTORES * SECTOR];
    // LBA 0: primer tramo (ya lleva la tabla de particiones).
    img[..SECTOR].copy_from_slice(&boot);
    // LBA 1..: segundo tramo (el que carga el kernel).
    let toff = TRAMO2_LBA as usize * SECTOR;
    img[toff..toff + tramo2.len()].copy_from_slice(&tramo2);
    // LBA 5..: el kernel, tal cual.
    let koff = KERNEL_LBA as usize * SECTOR;
    img[koff..koff + kernel.len()].copy_from_slice(&kernel);

    let path = root.join("loboswolfos.img");
    fs::write(&path, &img).expect("no se pudo escribir loboswolfos.img");
    println!(
        "loboswolfos: imagen {} ({} bytes; kernel {}, {} sectores).",
        path.display(),
        img.len(),
        kernel_bin.display(),
        kernel_sectores
    );
    path
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

/// Borra los destinos de compilacion, el directorio de build y la imagen.
fn clean(root: &Path) {
    for dir in ["boot", "kernel"] {
        let _ = fs::remove_dir_all(root.join(dir).join("target"));
    }
    let _ = fs::remove_dir_all(root.join("build"));
    let _ = fs::remove_file(root.join("loboswolfos.img"));
    println!("loboswolfos: limpio.");
}
