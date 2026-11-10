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

    call    rust_main
.Lhang:
    cli
    hlt
    jmp     .Lhang
