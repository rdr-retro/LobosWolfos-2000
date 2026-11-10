/* ------------------------------------------------------------------------
 * loboswolfos - segundo tramo del arranque (BIOS, 16 bits)
 * ------------------------------------------------------------------------
 * Copiado de rocky para loboswolfos; el trazado de la imagen no cambia.
 * ------------------------------------------------------------------------
 * Lo carga el sector de arranque (`bootusb.s` para el pendrive, `boot.s`
 * para el disquete) en 0x8000 y salta aqui. Este tramo es el que hace el
 * trabajo pesado y, sobre todo, el que puede informar de lo que pasa:
 *
 *   1. Ensena la unidad de arranque y los sectores que ocupa el kernel.
 *   2. Carga el kernel con INT 13h AH=42h. Cada lote se lee en un buffer de
 *      rebote en RAM convencional (0x600..0x7400) y se copia a su sitio
 *      definitivo en 1 MiB entrando y saliendo de modo protegido. Asi no se
 *      escribe **nada** por encima de 0x7400 mientras quedan llamadas al
 *      firmware: hay BIOS que guardan su estado (o su buffer de USB) en
 *      0x90000 y devolvian error 0x01 en cuanto el cargador tocaba esa zona.
 *      Avanza por los sectores que la BIOS dice haber transferido de verdad.
 *      Si una lectura falla, reinicia el controlador y parte el lote por la
 *      mitad; solo cuando falla un sector suelto por LBA se pasa a CHS. Si el
 *      CHS tampoco funciona, lo dice por pantalla y se para (nunca sigue con
 *      un kernel parcial).
 *   3. Enciende A20, monta la GDT, pasa a modo protegido de 32 bits,
 *      identidad-mapea los primeros 4 GiB con paginas de 2 MiB y entra en
 *      modo largo.
 *   4. Deja la informacion de arranque en 0x7E00 y salta al kernel (0x100000).
 * ------------------------------------------------------------------------ */

.section .text.boot, "ax"

/* Buffer de rebote para las lecturas de la BIOS.
 *
 * Se coloca en RAM convencional libre para evitar que la BIOS se queje de
 * zonas usadas o protegidas. */
.equ BOUNCE_SEG, 0x0000          /* segmento 0000 */
.equ BOUNCE_OFF,  0x0600          /* desplazamiento 0600: fisico 0x0600 */
.equ BOUNCE_SECTORES, 55         /* hasta ~0x7400 */

.code16
.global _start
_start:
    cli
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     sp, 0x7C00
    cld

    /* A20: primero el servicio BIOS estandar; en BIOS antiguas se usa el
     * puerto 0x92 sin tocar su bit RESET. */
    mov     ax, 0x2401
    int     0x15
    jnc     .La20_ok
    in      al, 0x92
    and     al, 0xFE
    or      al, 2
    out     0x92, al
.La20_ok:

    call    serie_init
    call    probe_chs

    /* Lo primero de todo: decir que se ha llegado hasta aqui. Si esta linea
     * se ve en pantalla, la BIOS ha arrancado el pendrive y este codigo esta
     * vivo; si no se ve, el problema es anterior (arranque de la BIOS). */
    lea     si, [msg_tramo]
    call    imprime
    mov     al, byte ptr [0x7F00]
    call    hex8
    lea     si, [msg_kernel]
    call    imprime
    mov     ax, [kernel_sectors]
    call    hex16
    lea     si, [msg_sectores]
    call    imprime

    call    carga_kernel

    lea     si, [msg_cargado]
    call    imprime

    /* GDT y modo protegido */
    lgdt    [gdt_desc]
    mov     eax, cr0
    or      eax, 1
    mov     cr0, eax
    .byte   0xEA
    .word   pm_entry
    .word   0x08

/* ---- Carga del kernel, directa a 1 MiB ----
 *
 * En discos fijos se usa INT 13h AH=42h (LBA); en disquetes, que no tienen por
 * que implementar EDD, se usa INT 13h AH=02h (CHS) desde el principio.
 *
 * La BIOS solo escribe por debajo de 1 MiB: cada tanda se lee en el buffer de
 * rebote (0x600..0x7400) y `copiar_pm` la lleva a 0x100000 + lo ya cargado.
 * Durante toda la carga no se escribe ni un byte por encima de 0x7400, asi
 * que no hay forma de pisar memoria o estado de la BIOS a mitad de camino.
 *
 * La DAP se reconstruye en cada lote (LBA = kernel_lba + cargado): si la
 * BIOS falla no se avanza nada. El lote empieza en BOUNCE_SECTORES sectores y
 * se parte por la mitad cada fallo, con el controlador reiniciado antes de
 * cada reintento; solo cuando falla un sector suelto por LBA se pasa a CHS.
 */
carga_kernel:
    /* Un disquete no tiene por que implementar AH=42h: la EDD solo es
     * obligatoria en discos fijos y muchas BIOS devuelven error 0x01 (esta
     * incluida la de QEMU). Si la unidad es un disquete se va directo a CHS,
     * que es lo normal y no hay nada que avisar. */
    cmp     byte ptr [0x7F00], 0x80
    jae     .Lunidad_fija
    mov     byte ptr [usar_chs], 1
.Lunidad_fija:
    mov     ax, [kernel_sectors]
    test    ax, ax
    jnz     .Ltam
    mov     ax, 1024                /* sin parchear: 512 KiB */
.Ltam:
    mov     [total], ax
    mov     word ptr [hecho], 0
    mov     word ptr [lote], BOUNCE_SECTORES

.Lsiguiente:
    mov     ax, [total]
    sub     ax, [hecho]
    jz      .Lfin_carga
    mov     cx, [lote]              /* nunca mas sectores de los que faltan */
    cmp     cx, ax
    jbe     .Ln_ok
    mov     cx, ax
.Ln_ok:
    mov     [pedido], cx
    mov     [chs_restantes], cx
    mov     word ptr [dap + 4], BOUNCE_OFF
    mov     word ptr [dap + 6], BOUNCE_SEG
    mov     eax, [kernel_lba]
    movzx   edx, word ptr [hecho]
    add     eax, edx
    mov     [dap + 8], eax

.Lreintento:
    mov     ax, [pedido]
    mov     [dap + 2], ax
    cmp     byte ptr [usar_chs], 0
    jne     .Lchs

    mov     dl, byte ptr [0x7F00]
    lea     si, [dap]
    mov     ax, 0x4200
    int     0x13
    jnc     .Llba_bien

    /* La BIOS no ha querido: se reinicia el controlador y se prueba con la
     * mitad. Muchas fallan por tamanio (demasiados sectores por llamada) o
     * por la direccion de destino; con el rebote el destino ya no es el
     * problema, y partir el lote cubre lo demas. */
    mov     [ultimo_error], ah
    call    reset_bios
.Lparte:
    cmp     word ptr [pedido], 1
    jbe     .Lpasa_chs
    shr     word ptr [lote], 1
    jnz     .Llote_ok
    mov     word ptr [lote], 1
.Llote_ok:
    mov     ax, [total]
    sub     ax, [hecho]
    mov     cx, [lote]
    cmp     cx, ax
    jbe     .Lpedido_ok
    mov     cx, ax
.Lpedido_ok:
    mov     [pedido], cx
    jmp     .Lreintento

.Llba_bien:
    mov     cx, [dap + 2]           /* sectores transferidos de verdad */
    test    cx, cx
    jnz     .Lbien
    mov     byte ptr [ultimo_error], 0
    jmp     .Lparte                 /* 0 leidos: se trata como un fallo */

.Lpasa_chs:
    /* Un solo sector por LBA ya falla: se informa del ultimo fallo y se
     * sigue con lecturas CHS para lo que queda de carga. */
    lea     si, [msg_fallo]
    call    imprime
    mov     ax, [dap + 8]
    call    hex16
    lea     si, [msg_codigo]
    call    imprime
    mov     al, [ultimo_error]
    call    hex8
    lea     si, [msg_crlf]
    call    imprime
    lea     si, [msg_chs]
    call    imprime
    mov     byte ptr [usar_chs], 1

    /* El lote pudo partirse durante los reintentos por LBA: para CHS solo
     * cuentan los sectores que quedaban en el ultimo pedido. */
    mov     ax, [pedido]
    mov     [chs_restantes], ax

.Lchs:
    /* Lectura CHS por tandas: como mucho una pista por llamada (la BIOS no
     * garantiza que AH=02 pueda cruzarla) y nunca mas sectores de los que
     * quedan en el lote, para no salir del buffer de rebote. */
    mov     word ptr [dap + 4], BOUNCE_OFF
    mov     word ptr [dap + 6], BOUNCE_SEG
.Lchs_uno:
    mov     ax, word ptr [dap + 8]   /* LBA actual */
    xor     dx, dx
    movzx   bx, byte ptr [spt]
    div     bx                       /* DX = sector dentro de la pista */
    mov     cx, bx
    sub     cx, dx                   /* sectores hasta el final de la pista */
    mov     ax, [chs_restantes]
    cmp     ax, cx
    jbe     .Lchs_tanda
    mov     ax, cx
.Lchs_tanda:
    mov     [dap + 2], ax            /* sectores de esta llamada */
    mov     word ptr [chs_reintentos], 0
.Lchs_intento:
    call    read_chs
    jnc     .Lchs_ok
    mov     [ultimo_error], ah
    call    reset_bios
    inc     word ptr [chs_reintentos]
    cmp     word ptr [chs_reintentos], 3
    jb      .Lchs_intento
    lea     si, [msg_fallo_chs]
    call    imprime
    mov     ax, [dap + 8]
    call    hex16
    lea     si, [msg_codigo]
    call    imprime
    mov     al, [ultimo_error]
    call    hex8
    lea     si, [msg_crlf]
    call    imprime
    lea     si, [msg_paro]
    call    imprime
    cli
.Lalto:
    hlt
    jmp     .Lalto

.Lchs_ok:
    mov     ax, [dap + 2]            /* sectores transferidos */
    add     word ptr [dap + 8], ax   /* cada sector, su LBA */
    mov     bx, ax
    shl     ax, 9
    add     word ptr [dap + 4], ax   /* y su hueco dentro del rebote */
    sub     word ptr [chs_restantes], bx
    jnz     .Lchs_uno
    mov     cx, [pedido]             /* lote completo: se copia entero */
    jmp     .Lbien

.Lbien:
    mov     [leidos], cx
    movzx   eax, word ptr [hecho]   /* destino = 0x100000 + cargado * 512 */
    shl     eax, 9
    add     eax, 0x100000
    mov     edi, eax
    mov     esi, BOUNCE_OFF
    movzx   ecx, cx
    shl     ecx, 9
    call    copiar_pm
    mov     cx, [leidos]
    add     [hecho], cx
    jmp     .Lsiguiente

.Lfin_carga:
    ret

/* Reinicia el controlador de disco (INT 13h AH=00) de la unidad de arranque.
 * Varias BIOS USB pierden el estado tras un error y solo vuelven a
 * funcionar tras un reset. */
reset_bios:
    xor     ah, ah
    mov     dl, byte ptr [0x7F00]
    int     0x13
    ret

/* Copia `ecx` bytes desde el rebote a `edi` (lineal, >= 0x100000) entrando y
 * saliendo de modo protegido de 32 bits. En modo real no se puede direccionar
 * 1 MiB (segmento:desplazamiento llega justo hasta 0xFFFFF), asi que este es
 * el unico camino para dejar el kernel en su sitio.
 *
 * Se usan los descriptores planos de la GDT que ya esta cargada. La pila real
 * (SS:SP) tiene un descriptor de 16 bits cacheado que sigue siendo valido en
 * modo protegido; aqui no se hace push ni pop y no se toca SS/SP, asi que la
 * direccion de retorno del `call` sigue intacta al volver.
 */
copiar_pm:
    mov     [copia_destino], edi
    mov     [copia_bytes], ecx
    lgdt    [gdt_desc]
    mov     eax, cr0
    or      eax, 1
    mov     cr0, eax
    .byte   0xEA
    .word   copia_pm32
    .word   0x08

.code32
copia_pm32:
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     esi, BOUNCE_OFF
    mov     edi, [copia_destino]
    mov     ecx, [copia_bytes]
    shr     ecx, 2
    cld
    rep     movsd
    /* Se cambia CS a un descriptor de codigo de 16 bits **sin salir de modo
     * protegido**. Es imprescindible: al limpiar PE la CPU conserva el bit D
     * del CS cacheado, asi que un `ljmp` a selector 0 despues de limpiar PE
     * deja el procesador decodificando en 32 bits (se veia en el trace de
     * QEMU: `ljmpl` y luego `movl $0x5ab003f8,%edx`). Con CS.D=0 ya puesto,
     * el codigo de 16 bits se ejecuta bien en cuanto se limpia PE. */
    .byte   0xEA
    .long   copia_pm16
    .word   0x28

.code16
copia_pm16:
    mov     eax, cr0
    and     eax, 0xFFFFFFFE
    mov     cr0, eax
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    /* CS sigue siendo el selector 0x28 con el que se entro en modo
     * protegido. Al limpiar PE se conserva el descriptor cacheado (base 0),
     * pero en cuanto la BIOS recargue CS (el iret de cualquier int 10h/13h)
     * lo interpretara como segmento real: base 0x280. A partir de ahi el
     * cargador ejecuta basura y se cuelga. Hay que devolver CS a 0 aqui,
     * antes de volver a llamar al firmware. */
    .byte   0xEA
    .word   copia_pm_real
    .word   0x0000
copia_pm_real:
    ret

/* Geometria para el fallback CHS. En USB moderno se usa AH=42h; aqui solo
 * importa que la BIOS nos diga como presenta la unidad.
 *
 * Si AH=08h falla o devuelve valores imposibles se usan geometrias tipicas:
 * 18/2 para disquete (DL < 0x80) y 255/63 para disco duro (DL >= 0x80).
 * Antes se usaba siempre 18/2: en un pendrive eso es una geometria
 * inventada y el fallback CHS leia sectores equivocados en silencio. */
probe_chs:
    mov     byte ptr [spt], 63
    mov     byte ptr [heads], 255
    mov     dl, byte ptr [0x7F00]
    cmp     dl, 0x80
    jae     .Lprobe
    mov     byte ptr [spt], 18
    mov     byte ptr [heads], 2
.Lprobe:
    mov     ah, 0x08
    int     0x13
    jc      .Lprobe_chs_done
    and     cl, 0x3F
    jz      .Lprobe_spt
    mov     [spt], cl
.Lprobe_spt:
    inc     dh                      /* DH = ultima cabeza (0-based) */
    jz      .Lprobe_chs_done        /* 256 cabezas: no cabe en un byte */
    mov     [heads], dh
.Lprobe_chs_done:
    ret

/* Lee los [dap+2] sectores que empiezan en el LBA [dap+8] con INT 13h AH=02
 * al buffer ES:BX = [dap+6]:[dap+4]. Quien llama garantiza que la tanda no
 * cruza de pista.
 *
 * El LBA se parte en 16 bits: con el limite de `MAX_KERNEL_SECTORES` de
 * `xtask` (2043 sectores, LBA <= 2047) el cociente nunca pasa de 255 y las
 * dos divisiones no desbordan. Si el kernel creciera mucho mas habria que
 * hacer la conversion con 32 bits. */
read_chs:
    mov     ax, word ptr [dap + 8]   /* LBA */
    xor     dx, dx
    movzx   bx, byte ptr [spt]
    div     bx                       /* AX = pista, DX = sector (0-based) */
    mov     [sect_idx], dl
    xor     dx, dx
    movzx   bx, byte ptr [heads]
    div     bx                       /* AX = cilindro, DX = cabeza */
    mov     ch, al                   /* cilindro */
    mov     cl, [sect_idx]
    inc     cl                       /* sector (1-based) */
    mov     dh, dl                   /* cabeza */
    mov     ax, word ptr [dap + 6]
    mov     es, ax
    mov     bx, word ptr [dap + 4]
    mov     dl, byte ptr [0x7F00]
    mov     ax, word ptr [dap + 2]   /* sectores de esta llamada */
    or      ah, 0x02                 /* AH = 02h: leer */
    int     0x13
    ret

/* ---- Impresion por pantalla y serie (teletype de la BIOS) ---- */
imprime:                            /* DS:SI = cadena acabada en cero */
    push    ax
    push    si
.Lbucle:
    lodsb
    test    al, al
    jz      .Lfin
    call    putc
    jmp     .Lbucle
.Lfin:
    pop     si
    pop     ax
    ret

/* ---- Puerto de serie (COM1) ----
 * En un PC de verdad nadie lee esto, pero al emular deja el rastro del
 * arranque en un fichero: asi se puede comprobar sin mirar la pantalla. */
serie_init:
    mov     dx, 0x3FB
    mov     al, 0x80
    out     dx, al                  /* DLAB = 1 */
    mov     dx, 0x3F8
    mov     al, 1
    out     dx, al                  /* divisor 1: 115200 bps */
    mov     dx, 0x3F9
    xor     al, al
    out     dx, al
    mov     dx, 0x3FB
    mov     al, 3
    out     dx, al                  /* 8N1, DLAB = 0 */
    mov     dx, 0x3FA
    mov     al, 0xC7
    out     dx, al                  /* FIFO encendida y limpia */
    mov     dx, 0x3FC
    mov     al, 0x0B
    out     dx, al                  /* DTR, RTS y OUT2 */
    ret

putc:                               /* AL = caracter (pantalla y serie) */
    push    ax
    push    bx
    push    dx
    mov     ah, 0x0E
    mov     bx, 0x0007              /* pagina 0, atributo normal */
    int     0x10
    mov     dx, 0x3F8
    out     dx, al
    pop     dx
    pop     bx
    pop     ax
    ret

hex16:                              /* AX = palabra -> cuatro digitos */
    push    ax
    mov     al, ah
    call    hex8
    pop     ax
    call    hex8
    ret

hex8:                               /* AL = byte -> dos digitos */
    push    ax
    mov     ah, al
    shr     al, 4
    call    hexd
    mov     al, ah
    call    hexd
    pop     ax
    ret

hexd:                               /* AL = 0..15 -> un digito */
    and     al, 0x0F
    cmp     al, 10
    jb      .Ldigito
    add     al, 'A' - 10
    jmp     putc
.Ldigito:
    add     al, '0'
    jmp     putc

/* ---- Modo protegido de 32 bits ---- */
.code32
pm_entry:
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     fs, ax
    mov     gs, ax
    /* El kernel ya esta entero en 0x100000: `carga_kernel` copio cada lote
     * desde el rebote en cuanto lo leyo. Aqui solo quedan las tablas de
     * pagina y el salto a modo largo. */
    cld
    mov     edi, 0x1000
    xor     eax, eax
    mov     ecx, 0x1800
    rep     stosd

    mov     dword ptr [0x1000], 0x2003
    mov     dword ptr [0x2000], 0x3003
    mov     dword ptr [0x2008], 0x4003
    mov     dword ptr [0x2010], 0x5003
    mov     dword ptr [0x2018], 0x6003

    mov     edi, 0x3000
    mov     eax, 0x0083
    mov     ecx, 2048
.Lpd:
    mov     [edi], eax
    add     edi, 8
    add     eax, 0x200000
    dec     ecx
    jnz     .Lpd

    mov     eax, cr4
    or      eax, 0x20
    mov     cr4, eax
    mov     eax, 0x1000
    mov     cr3, eax
    mov     ecx, 0xC0000080
    rdmsr
    or      eax, 0x100
    wrmsr
    mov     eax, cr0
    or      eax, 0x80000001
    mov     cr0, eax

    /* jmp far 0x18:lm_entry (m16:32, codificado a mano) */
    .byte   0xEA
    .long   lm_entry
    .word   0x18

/* ---- Modo largo de 64 bits ---- */
.code64
lm_entry:
    /* Boot info en 0x7E00 para el kernel:
     *   +0x00 u32 magia "CORK"
     *   +0x04 u32 unidad de arranque (DL del BIOS)
     *   +0x08 u32 LBA donde empieza el volumen Estrato
     *   +0x0C u32 sectores que ocupa ese volumen */
    mov     dword ptr [0x7E00], 0x4B524F43
    xor     rax, rax
    mov     al, byte ptr [0x7F00]
    mov     dword ptr [0x7E04], eax
    mov     eax, dword ptr [estrato_lba]
    mov     dword ptr [0x7E08], eax
    mov     eax, dword ptr [estrato_sectores]
    mov     dword ptr [0x7E0C], eax

    mov     eax, 0x100000
    jmp     rax

/* ---- GDT ----
 * Descriptores emitidos como pares de `.long` (little-endian) para que la
 * seccion no suba de alineacion 4. */
.section .text.boot, "ax"
.align 4
gdt:
    .long 0x00000000, 0x00000000    /* 0x00 nulo */
    .long 0x0000FFFF, 0x00CF9A00    /* 0x08 codigo 32 */
    .long 0x0000FFFF, 0x00CF9200    /* 0x10 datos 32 */
    .long 0x0000FFFF, 0x00AF9A00    /* 0x18 codigo 64 */
    .long 0x0000FFFF, 0x00AF9200    /* 0x20 datos 64 */
    .long 0x0000FFFF, 0x00009A00    /* 0x28 codigo 16 (D=0), para volver de PM */
gdt_end:
gdt_desc:
    .word gdt_end - gdt - 1
    .long gdt

/* ---- Variables ----
 * La unidad de arranque que da la BIOS la deja el primer tramo en 0x7F00:
 * aqui no hay copia propia (antes la habia y llegaba a cero). */
.align 2
lote:           .word BOUNCE_SECTORES   /* sectores por lectura (adaptativo) */
usar_chs:       .byte 0                /* 1 = la BIOS no lee por LBA */
spt:            .byte 18               /* por defecto, disquete; lo ajusta probe_chs */
heads:          .byte 2
sect_idx:       .byte 0
chs_reintentos: .word 0
chs_restantes:  .word 0
pedido:         .word 0
leidos:         .word 0
ultimo_error:   .byte 0

/* Estado de la carga (ver `carga_kernel`). */
.align 2
hecho:          .word 0                /* sectores ya copiados a 1 MiB */
total:          .word 0                /* sectores de la imagen */
copia_origen:   .long 0                /* copiar_pm: origen lineal */
copia_destino:  .long 0                /* copiar_pm: destino lineal */
copia_bytes:    .long 0                /* copiar_pm: bytes a copiar */
kernel_lba:     .long 5                /* LBA donde empieza el kernel */

/* Marca que busca `xtask` para parchear la configuracion de arranque. Va
 * justo delante de los valores que reescribe: los sectores que ocupa el
 * kernel, y el LBA y el tamano del volumen Estrato del pendrive. */
cfg_marca:      .ascii "RKYCFG"
kernel_sectors: .word 0
estrato_lba:    .long 0
estrato_sectores: .long 0

/* Disk Address Packet (INT 13h AH=42h). El LBA 5 es donde empieza el kernel
 * en el pendrive: el MBR ocupa el LBA 0 y este tramo, los LBA 1..4 (ver
 * `USB_KERNEL_LBA` en xtask). `carga_kernel` lo reconstruye en cada lote;
 * aqui solo valen los valores de partida. */
.align 2
dap:            .byte 0x10
                .byte 0x00
                .word BOUNCE_SECTORES
                .word BOUNCE_OFF
                .word BOUNCE_SEG
                .long 5
                .long 0

/* ---- Mensajes ---- */
msg_tramo:      .asciz "loboswolfos:arranque: unidad 0x"
msg_kernel:     .asciz ", kernel de 0x"
msg_sectores:   .asciz " sectores.\r\n"
msg_cargado:    .asciz "loboswolfos:kernel cargado, entrando en modo largo.\r\n"
msg_fallo:      .asciz "loboswolfos:la BIOS no lee por LBA en el sector 0x"
msg_fallo_chs:  .asciz "loboswolfos:la BIOS no lee por CHS en el sector 0x"
msg_codigo:     .asciz ", codigo de error 0x"
msg_crlf:       .asciz "\r\n"
msg_chs:        .asciz "loboswolfos:se sigue con lecturas CHS.\r\n"
msg_paro:       .asciz "loboswolfos:no se puede cargar el kernel; revisa la unidad de arranque.\r\n"

.global __fin_tramo2
__fin_tramo2:
