/* bios.s - puente al BIOS (INT 10h / VESA VBE) desde el kernel de 64 bits.
 *
 * rockyOS corre en modo largo, donde el BIOS ya no es accesible. Para hablar
 * con el firmware (VBE, fuente de la VGA, modo texto) se hace un viaje de ida y
 * vuelta a modo real:
 *
 *   1. se salta a un segmento de codigo de 16 bits (modo compatibilidad),
 *   2. se desactiva la paginacion (CR0.PG) y la proteccion (CR0.PE),
 *   3. se ejecuta el INT 10h en modo real,
 *   4. se reactiva PE, PAE y el modo largo y se vuelve al kernel.
 *
 * El codigo de 16/32 bits vive en la seccion .lowtext, que el kernel copia a
 * 0x7200 (por debajo de 1 MiB) antes de usarla. El codigo de 64 bits permanece
 * en el kernel.
 *
 * Bufer / buzon en memoria baja:
 *   0x7200  trampolin de 16/32 bits (copiado desde .lowtext; hasta 0x7400)
 *   0x7400  buzon de registros de E/S (lo rellena el kernel)
 *   0x7454  puntero de pila de 64 bits guardado durante la llamada
 *   0x7460  IDTR del kernel guardada durante la llamada
 */

.section .text
.code64

/* Carga la GDT unificada del kernel (incluye descriptores de 16 bits). */
.global rocky_load_gdt
rocky_load_gdt:
    lea     rax, [rip + rocky_gdt_ptr]
    lgdt    [rax]
    mov     ax, 0x20
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     fs, ax
    mov     gs, ax
    ret

/* Ejecuta INT 10h con los registros depositados en el buzon de 0x7000.
 * Conserva los registros callee-saved y regresa con `ret`. */
.global rocky_bios_call
rocky_bios_call:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    pushfq
    cli
    mov     [0x7454], rsp
    /* Guardamos la IDTR del kernel: en modo real el BIOS (y QEMU) usan la
     * IDTR para localizar la tabla de vectores (base + vector*4). Si dejasemos
     * la IDTR del kernel, el INT del trampolin saltaria a basura. */
    mov     eax, 0x7460
    sidt    [rax]
    /* Baja a modo compatibilidad 16 bits (CS=0x28) via push+lretq, porque
     * el salto lejano inmediato (opcode 0xEA) no es valido en 64 bits. */
    push    0x28
    push    0x7200
    .byte   0x48, 0xCB              /* retfq */

/* Punto de retorno desde el trampolin (64 bits). Lo usa el puntero lejano
 * grabado en el blob de 0x7200, por eso es global. */
.global rocky_lm_resume
rocky_lm_resume:
    mov     ax, 0x20
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     fs, ax
    mov     gs, ax
    mov     rsp, [0x7454]
    /* Restauramos la IDTR del kernel antes de devolver el control. */
    mov     eax, 0x7460
    lidt    [rax]
    /* Reponemos RFLAGS del llamante en vez de un `sti` a pelo: si el kernel
     * tenia las interrupciones apagadas, deben seguir apagadas. */
    popfq
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

/* ---- GDT unificada ----
 *   0x08/0x10  codigo/datos 32 bits (nucleo)
 *   0x18/0x20  codigo/datos 64 bits (nucleo)
 *   0x28/0x30  codigo/datos 16 bits (trampolin del BIOS)
 *   0x38/0x40  codigo/datos 64 bits de **usuario** (DPL 3)
 *   0x48       TSS de 64 bits (doble entrada; su base la parchea el kernel)
 */
.section .rodata
.align 16
.global rocky_gdt
rocky_gdt:
    .quad 0x0000000000000000    /* 0x00 nulo                     */
    .quad 0x00CF9A000000FFFF    /* 0x08 codigo 32 bits           */
    .quad 0x00CF92000000FFFF    /* 0x10 datos 32 bits            */
    .quad 0x00AF9A000000FFFF    /* 0x18 codigo 64 bits           */
    .quad 0x00AF92000000FFFF    /* 0x20 datos 64 bits            */
    .quad 0x00009A000000FFFF    /* 0x28 codigo 16 bits           */
    .quad 0x000092000000FFFF    /* 0x30 datos 16 bits            */
    .quad 0x00AFFA000000FFFF    /* 0x38 codigo de usuario 64 b   */
    .quad 0x00CFF2000000FFFF    /* 0x40 datos  de usuario        */
    .quad 0x0000000000000000    /* 0x48 TSS (base via kernel)    */
    .quad 0x0000000000000000    /* 0x50 TSS (base via kernel)    */
rocky_gdt_end:
rocky_gdt_ptr:
    .word rocky_gdt_end - rocky_gdt - 1
    .quad rocky_gdt

/* ------------------------------------------------------------------ */
/* Trampolin de bajo nivel (se copia a 0x7200).                       */
/* ------------------------------------------------------------------ */
.set BLOB_BASE, 0x7200      /* el blob puede llegar hasta 0x7400 (buzon) */
.set OFF_PGF,   0x40        /* tras recargar CS al bajar de PG   */
.set OFF_RM,    0x80        /* inicio del codigo de modo real   */
.set OFF_PM,    0x160       /* inicio del codigo de 32 bits     */
.set OFF_TBL,   0x1D0       /* tabla de punteros lejanos        */
.set OFF_IVT,   0x1E0       /* puntero IDTR de modo real        */
/* Valor del GDTR (limite + base) guardado en memoria baja. En modo real el
 * limite del segmento (0xFFFF) SI se comprueba: `lgdt [rocky_gdt_ptr]` apunta a
 * .rodata (~0x48xxx), fuera de ese limite, y provoca un #GP en hardware real
 * (QEMU no comprueba limites en modo real, por eso alli no se ve). La BASE del
 * GDTR puede ser alta: en modo protegido los descriptores se leen por el GDTR,
 * no por el limite de un segmento. */
.set OFF_GDTPTR, 0x1E8

.section .lowtext, "ax"

/* --- 1) Modo compatibilidad 16 bits: apaga paginacion y proteccion --- */
.code16
low16_entry:
    mov     ax, 0x30
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     sp, 0x9000          /* pila baja propia del trampolin */
    mov     fs, ax
    mov     gs, ax

    mov     eax, cr0
    and     eax, 0x7FFFFFFF     /* CR0.PG = 0 (deja el modo largo) */
    mov     cr0, eax

    /* Al desactivar CR0.PG el procesador sale del modo largo, pero tanto el
     * manual de Intel (18.8.1.1) como el de AMD exigen recargar CS con un
     * salto lejano enseguida. Sin el, la CPU sigue prebuscando y descodificando
     * instrucciones del modo anterior: en QEMU no se nota porque no hay cola
     * de prebusqueda, pero en hardware antiguo (el Penryn de este Mac, por
     * ejemplo) se acaba ejecutando basura. */
    .byte   0xEA                /* jmp far inmediato m16:m16 */
    .word   BLOB_BASE + OFF_PGF
    .word   0x0028              /* CS = codigo de 16 bits */

    .org    OFF_PGF
.code16
    /* Hay que abandonar el modo largo por completo: si EFER.LME sigue a 1
     * (o CR4.PAE), el propio BIOS, al activar la paginacion para pasar a su
     * modo protegido de 32 bits, volveria a entrar en modo largo/#GP. */
    mov     ecx, 0xC0000080     /* IA32_EFER */
    rdmsr
    and     eax, 0xFFFFFEFF     /* EFER.LME = 0 */
    wrmsr
    mov     eax, cr4
    and     eax, 0xFFFFFFDF     /* CR4.PAE = 0 */
    mov     cr4, eax

    mov     eax, cr0
    and     eax, 0xFFFFFFFE     /* CR0.PE = 0 -> modo real */
    mov     cr0, eax

    .byte   0xFF, 0x2E          /* jmp far [far_rm] */
    .word   BLOB_BASE + OFF_TBL

    .org    OFF_RM
/* --- 2) Modo real: ejecuta INT 10h y guarda el resultado --- */
.code16
rm_entry:
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     sp, 0x9F00

    mov     eax, [0x7400]
    mov     ebx, [0x7404]
    mov     ecx, [0x7408]
    mov     edx, [0x740C]
    mov     si,  [0x7418]
    mov     es,  si
    mov     esi, [0x7410]
    mov     edi, [0x7414]
    mov     ebp, [0x7430]

    /* En modo real la tabla de vectores vive en 0x0000 (256 entradas de 4
     * bytes). Tanto el hardware real como QEMU la localizan via IDTR.base
     * (QEMU: `ptr = idt.base + intno * 4`), asi que hay que apuntar la IDTR
     * a cero antes de ejecutar el INT. */
    .byte   0x0F, 0x01, 0x1E    /* lidt [BLOB_BASE + OFF_IVT] */
    .word   BLOB_BASE + OFF_IVT

    /* Muchos manejadores del firmware (sobre todo el de INT 15h/E820) esperan a
     * que avance el contador de "ticks" de la BIOS, que solo se actualiza si el
     * temporizador (IRQ0) puede atender. Con IF=0 ese bucle nunca termina y la
     * maquina se cuelga. El kernel decide si se enciende IF durante el INT
     * (banderin en el buzon): solo lo pide para el E820, que es cuando el PIC y
     * la IVT siguen siendo los del firmware. */
    cmp     byte ptr [0x743A], 0
    je      1f
    sti
1:
    /* El kernel parchea el inmediato de este INT con el numero de vector que
     * quiera ejecutar (0x10 para el video, 0x15 para el mapa E820, etc.). El
     * simbolo rocky_int_imm apunta justo al byte del inmediato. */
    .byte   0xCD                /* opcode de INT imm8 */
    .global rocky_int_imm
rocky_int_imm:
    .byte   0x10                /* inmediato (parcheable) */
    cli

    pushf
    push    eax
    push    es                 /* ES tal y como lo deja el BIOS */

    /* El BIOS no esta obligado a conservar DS. Si lo deja apuntando a su
     * propio segmento (cosa normal en firmware real; SeaBIOS siempre lo
     * devuelve a cero, por eso en QEMU nunca se ve), las tiendas de abajo
     * escribirian fuera del buzon y corromperian memoria: con DS=0x1000
     * caerian dentro del propio kernel. DS=0 antes de escribir nada. */
    xor     ax, ax
    mov     ds, ax

    pop     es
    mov     [0x7438], es        /* OUT_ES */
    xor     ax, ax
    mov     es, ax
    mov     [0x7434], ebp       /* OUT_EBP */
    pop     eax
    mov     [0x741C], eax       /* OUT_EAX */
    mov     [0x7420], ebx       /* OUT_EBX */
    mov     [0x7424], ecx       /* OUT_ECX */
    mov     [0x7428], edx       /* OUT_EDX */
    pop     ax
    mov     [0x742C], ax        /* OUT_FLAGS */

    /* El BIOS puede haber cargado su propia GDT: restauramos la nuestra.
     *
     * OJO: el operando de este `lgdt` tiene que caer DENTRO del limite del
     * segmento en modo real (0xFFFF). `rocky_gdt_ptr` vive en .rodata (~0x48xxx),
     * fuera de ese limite, y en hardware real un acceso cuyo offset lo rebasa
     * provoca un #GP (en QEMU no, porque no comprueba limites en modo real).
     * Por eso el valor del GDTR se guarda copiado en memoria baja
     * (BLOB + OFF_GDTPTR, 0x73E8) y aqui se hace `lgdt [disp16]`. El prefijo 0x66
     * hace que se cargue la base completa de 32 bits (estructura m16&32). */
    .byte   0x66, 0x0F, 0x01, 0x16    /* lgdt [disp16] */
    .word   BLOB_BASE + OFF_GDTPTR

    mov     eax, cr0
    or      eax, 1              /* CR0.PE = 1 -> modo protegido */
    mov     cr0, eax

    .byte   0xFF, 0x2E          /* jmp far [far_pm] */
    .word   BLOB_BASE + OFF_TBL + 4

    .org    OFF_PM
/* --- 3) Modo protegido 32 bits: reactiva PAE, paging y modo largo --- */
.code32
pm_resume:
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     fs, ax
    mov     gs, ax

    mov     eax, cr4
    or      eax, 0x20           /* CR4.PAE = 1 */
    mov     cr4, eax

    mov     eax, 0x1000         /* CR3 = PML4 */
    mov     cr3, eax

    mov     ecx, 0xC0000080
    rdmsr
    or      eax, 0x100          /* EFER.LME = 1 */
    wrmsr

    mov     eax, cr0
    or      eax, 0x80000001     /* CR0.PE | CR0.PG */
    mov     cr0, eax

    .byte   0xFF, 0x2D          /* jmp far [far_lm] -> rocky_lm_resume */
    .long   BLOB_BASE + OFF_TBL + 0x08

    .org    OFF_TBL
/* --- 4) Tabla de punteros lejanos usados por los saltos anteriores ---
 * El tamano del operando debe coincidir con el modo del salto:
 *   - saltos de 16 bits  -> m16:16 (offset16 + selector16)
 *   - saltos de 32 bits  -> m16:32 (offset32 + selector16)
 */
    .word   BLOB_BASE + OFF_RM          /* [BLOB+0x00] rm (m16:16) */
    .word   0x0000
    .word   BLOB_BASE + OFF_PM          /* [BLOB+0x04] pm (m16:16) */
    .word   0x0008
    .long   rocky_lm_resume             /* [BLOB+0x08] lm (m16:32) */
    .word   0x0018

    .org    OFF_IVT
/* --- 5) Puntero a la tabla de vectores de modo real (IDTR base=0, limit=FFFF) --- */
    .word   0xFFFF
    .long   0x00000000

    .org    OFF_GDTPTR
/* --- 6) Valor del GDTR del kernel, copiado a memoria baja ---
 * Lo carga `lgdt [disp16]` de rm_entry. Aqui abajo (0x73E8) el offset cae dentro
 * del limite de 64 KiB del modo real, que es lo que exige el hardware. */
    .word   rocky_gdt_end - rocky_gdt - 1
    .long   rocky_gdt
