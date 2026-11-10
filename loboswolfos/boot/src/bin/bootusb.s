/* ------------------------------------------------------------------------
 * rockyOS - primer tramo del arranque por pendrive (sector de arranque)
 * ------------------------------------------------------------------------
 * Copiado de rocky para loboswolfos; el trazado de la imagen no cambia.
 * ------------------------------------------------------------------------
 * Este sector ya no carga el kernel: solo hace dos cosas.
 *
 *   1. Encabeza el disco con un BPB de disco duro (estilo NTFS). Muchas
 *      BIOS clasifican el pendrive leyendo estos primeros bytes; con un BPB
 *      valido lo toman por un disco fijo (USB-HDD) y lo arrancan.
 *   2. Carga el **segundo tramo** (`bootusb2.bin`, en el LBA 1) a 0x8000 y
 *      salta ahi. Si esa lectura falla, lo dice por pantalla y se para.
 *
 * El reparto en dos tramos es lo que da sitio para informar. Este sector
 * tiene que caber el solo hasta el offset 446 (ahi va la tabla de
 * particiones) y un mensaje de error no entraba. Antes, si la BIOS fallaba
 * al leer, el codigo seguia adelante y saltaba a memoria sin cargar: eso es
 * ejecutar basura, y en un PC real se ve como un reinicio en bucle sin
 * mensaje. Ahora, en el peor caso, se lee un mensaje en pantalla.
 * ------------------------------------------------------------------------ */

.section .text.boot, "ax"

.code16
.global _start
_start:
    .byte   0xEB, 0x52, 0x90        /* jmp +0x52 y nop: salta por encima del BPB */

/* ---- BPB (BIOS Parameter Block) estilo NTFS ----
 * No describe un sistema de ficheros real: solo le dice a la BIOS que esto
 * es un disco fijo con geometria normal (medio F8, 63 sectores, 255
 * cabezales). Va aqui porque algunas BIOS no arrancan un pendrive cuyo
 * primer sector no lo lleva. */
    .ascii  "NTFS    "              /* +0x03: firma del formateador */
    .word   512                     /* +0x0B: bytes por sector */
    .byte   8                       /* +0x0D: sectores por cluster */
    .word   0                       /* +0x0E: sectores reservados */
    .byte   0, 0, 0                 /* +0x10: sin uso */
    .word   0                       /* +0x13: sin uso */
    .byte   0xF8                    /* +0x15: medio fijo (disco duro) */
    .word   0                       /* +0x16: sin uso */
    .word   63                      /* +0x18: sectores por pista */
    .word   255                     /* +0x1A: cabezales */
    .long   0                       /* +0x1C: sectores ocultos */
    .long   0                       /* +0x20: sin uso */
    .long   0x00800080              /* +0x24: unidad 0x80 y firma 0x80 */
    .long   0                       /* +0x28: sin uso */
    .quad   0                       /* +0x2C: sectores totales */
    .quad   0                       /* +0x34: LCN del MFT */
    .quad   0                       /* +0x3C: LCN del MFT espejo */
    .byte   0xF6                    /* +0x44: clusters por registro */
    .byte   0, 0, 0                 /* +0x45: sin uso */
    .byte   0xF6                    /* +0x48: clusters por bloque de indice */
    .byte   0, 0, 0                 /* +0x49: sin uso */
    .quad   0                       /* +0x4C: numero de serie */

/* ---- Codigo (empieza en el offset 0x54) ---- */
.global __codigo
__codigo:
    cli
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     sp, 0x7C00
    cld
    mov     [boot_drive], dl
    mov     byte ptr [0x7F00], dl   /* la unidad que dio la BIOS, para el 2o tramo */
    call    serie_init

    /* Segundo tramo: los LBA 1 y 2 a la direccion lineal 0x8000, que es
     * donde esta enlazado `bootusb2`. */
    mov     dl, [boot_drive]
    lea     si, [dap]
    mov     ax, 0x4200
    int     0x13
    jnc     .Lsalta

    /* Fallback para BIOS sin INT 13h Extensions: el segundo tramo empieza
     * en el sector 2 del primer cilindro. Se carga con CHS en 0x0000:0x8000.
     * Tambien se reinicia el controlador para limpiar un posible error USB. */
    xor     ah, ah
    mov     dl, [boot_drive]
    int     0x13
    mov     ax, 0x0202
    xor     bx, bx
    mov     es, bx
    mov     bx, 0x8000
    xor     cx, cx
    mov     cl, 2
    xor     dh, dh
    mov     dl, [boot_drive]
    int     0x13
    jnc     .Lsalta

    /* La BIOS no ha leido. Antes se seguia adelante y se saltaba a ejecutar
     * lo que hubiera en 0x8000: en un PC real eso se ve como un reinicio en
     * bucle y sin mensaje. Ahora se dice lo que ha contestado la BIOS. */
    mov     byte ptr [boot_err], ah
    lea     si, [msg_lectura]
    call    imprime
    mov     al, [boot_err]
    call    hex8
    lea     si, [msg_unidad]
    call    imprime
    mov     al, [boot_drive]
    call    hex8
    lea     si, [msg_punto]
    call    imprime
.Lalto:
    cli
    hlt
    jmp     .Lalto

.Lsalta:
    .byte   0xEA                    /* jmp 0x0000:0x8000 (segundo tramo) */
    .word   0x8000
    .word   0x0000

/* ---- Puerto de serie (COM1) ----
 * En un PC de verdad nadie lee la salida de serie, pero al emular deja el
 * rastro del arranque en un fichero: asi se puede comprobar sin mirar la
 * pantalla. Los mismos caracteres van a la pantalla y al puerto. */
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

putc:                               /* AL = caracter: pantalla (INT 10h) y COM1 */
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

hex8:                               /* AL = byte -> dos digitos hexadecimales */
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

/* ---- Datos ---- */
.align 2
boot_drive:     .byte 0
                .byte 0                 /* relleno: la DAP queda alineada */
boot_err:       .byte 0                 /* codigo de error de la BIOS */
                .byte 0                 /* relleno: la DAP queda alineada */

/* Mensajes de error (pantalla y COM1). */
msg_lectura:    .asciz "loboswolfos: el pendrive no arranca (error 0x"
msg_unidad:     .asciz ", unidad 0x"
msg_punto:      .asciz ").\r\n"

/* Disk Address Packet (INT 13h AH=42h): LBA 1, 2 sectores, 0x0000:0x8000
 * (la direccion lineal 0x8000, que es donde el segundo tramo esta enlazado).
 * El kernel empieza justo detras (LBA 5); ver `USB_KERNEL_LBA` en xtask. */
dap:            .byte 0x10
                .byte 0x00
                .word 4
                .word 0x0000
                .word 0x0800
                .long 1
                .long 0

/* Fin del codigo y los datos. A partir de aqui (offset 446 = 0x7DBE) el
 * enlazador exige que no haya nada: ahi el constructor (loboswolfos/xtask)
 * escribe la tabla de particiones del MBR (la zona del juego, tipo 0x83, en
 * LBA 2048). */
.global __fin_codigo
__fin_codigo:
