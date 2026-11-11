/* Punto de entrada del kernel loboswolfos (64 bits, modo largo).
 * El segundo tramo del arranque deja la memoria identidad-mapeada y salta
 * aqui, a 1 MiB, tras copiar el kernel a su sitio. */

.section .text.boot, "ax"
.code64
.global _start
_start:
    cli
    cld
    mov     rsp, 0x400000
    xor     rbp, rbp

    /* Pone a cero .bss. */
    lea     rdi, [rip + __bss_start]
    lea     rcx, [rip + __bss_end]
    sub     rcx, rdi
    xor     eax, eax
    rep     stosb

    /* El nucleo del motor usa SSE2 (nucleo64.s): sin el bit OSFXSR de CR4
     * cualquier instruccion que toque un registro XMM lanza #UD. Se
     * activa tambien OSXMMEXCPT, su pareja obligada si algun dia se
     * desenmascara una excepcion SIMD. El trampolin del BIOS solo borra
     * CR4.PAE, asi que estos bits sobreviven a las llamadas al firmware. */
    mov     rax, cr4
    or      rax, 0x600
    mov     cr4, rax

    call    rust_main
.Lhang:
    cli
    hlt
    jmp     .Lhang
