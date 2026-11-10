/* Stubs de interrupciones y excepciones de rockyOS.
 *
 * Hay un stub por vector porque la CPU no empuja el numero de vector: cada uno
 * lo deja en la pila. Se cubren los vectores 0..47, que son todos los que
 * pueden llegar solos:
 *   - 0..31  : excepciones de la CPU.
 *   - 32..47 : IRQ del PIC ya remapeado a 0x20..0x2F.
 * El resto de puertas se rellenan en Rust con `isr_espurio`, de modo que
 * ninguna excepcion pueda quedarse sin puerta (una puerta ausente acaba en
 * #DF y enmascara el error de verdad).
 *
 * Los vectores que empujan codigo de error son 8, 10, 11, 12, 13, 14, 17, 21,
 * 29 y 30. Si uno de ellos lo dispara una IRQ (que NO empuja codigo de error),
 * el marco llega desplazado una posicion; `isr_dispatch` lo detecta y lo
 * recoloca antes de informar. */

.section .text, "ax"
.code64

.macro ISR_NOERR num
.global isr\num
isr\num:
    push    0
    push    \num
    jmp     isr_common
.endm

.macro ISR_ERR num
.global isr\num
isr\num:
    push    \num
    jmp     isr_common
.endm

ISR_NOERR   0   /* division por cero */
ISR_NOERR   1   /* depuracion */
ISR_NOERR   2   /* NMI */
ISR_NOERR   3   /* punto de ruptura */
ISR_NOERR   4   /* desbordamiento */
ISR_NOERR   5   /* limite de rango */
ISR_NOERR   6   /* codigo invalido */
ISR_NOERR   7   /* dispositivo no disponible */
ISR_ERR     8   /* doble fallo */
ISR_NOERR   9   /* (obsoleto) */
ISR_ERR    10   /* TSS invalido */
ISR_ERR    11   /* segmento ausente */
ISR_ERR    12   /* falta de pila */
ISR_ERR    13   /* fallo de proteccion */
ISR_ERR    14   /* fallo de pagina */
ISR_NOERR  15   /* reservado */
ISR_NOERR  16   /* coma flotante */
ISR_ERR    17   /* alineacion */
ISR_NOERR  18   /* comprobacion de maquina */
ISR_NOERR  19   /* error SIMD */
ISR_NOERR  20   /* virtualizacion */
ISR_ERR    21   /* proteccion de control */
ISR_NOERR  22
ISR_NOERR  23
ISR_NOERR  24
ISR_NOERR  25
ISR_NOERR  26
ISR_NOERR  27
ISR_NOERR  28
ISR_ERR    29
ISR_ERR    30
ISR_NOERR  31
ISR_NOERR  32   /* IRQ0: temporizador */
ISR_NOERR  33   /* IRQ1: teclado */
ISR_NOERR  34   /* IRQ2: cascada */
ISR_NOERR  35   /* IRQ3: COM2 */
ISR_NOERR  36   /* IRQ4: COM1 */
ISR_NOERR  37   /* IRQ5 */
ISR_NOERR  38   /* IRQ6 */
ISR_NOERR  39   /* IRQ7 */
ISR_NOERR  40   /* IRQ8: reloj de tiempo real */
ISR_NOERR  41   /* IRQ9 */
ISR_NOERR  42   /* IRQ10 */
ISR_NOERR  43   /* IRQ11 */
ISR_NOERR  44   /* IRQ12: raton */
ISR_NOERR  45   /* IRQ13: coprocesador */
ISR_NOERR  46   /* IRQ14: disco */
ISR_NOERR  47   /* IRQ15 */

/* Vector sin stub propio (48..255): marca 0x1000 para reconocerlo. */
.global isr_espurio
isr_espurio:
    push    0
    push    0x1000
    jmp     isr_common

isr_common:
    push    rax
    push    rcx
    push    rdx
    push    rbx
    push    rbp
    push    rsi
    push    rdi
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rdi, rsp
    mov     rbp, rsp
    and     rsp, -16
    cld
    call    isr_dispatch
    mov     rsp, rbp

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rbx
    pop     rdx
    pop     rcx
    pop     rax
    add     rsp, 16
    iretq

/* Direcciones de los stubs, para que el kernel rellene la IDT. */
.section .rodata
.p2align 3
.global isr_vectors
isr_vectors:
    .quad   isr0
    .quad   isr1
    .quad   isr2
    .quad   isr3
    .quad   isr4
    .quad   isr5
    .quad   isr6
    .quad   isr7
    .quad   isr8
    .quad   isr9
    .quad   isr10
    .quad   isr11
    .quad   isr12
    .quad   isr13
    .quad   isr14
    .quad   isr15
    .quad   isr16
    .quad   isr17
    .quad   isr18
    .quad   isr19
    .quad   isr20
    .quad   isr21
    .quad   isr22
    .quad   isr23
    .quad   isr24
    .quad   isr25
    .quad   isr26
    .quad   isr27
    .quad   isr28
    .quad   isr29
    .quad   isr30
    .quad   isr31
    .quad   isr32
    .quad   isr33
    .quad   isr34
    .quad   isr35
    .quad   isr36
    .quad   isr37
    .quad   isr38
    .quad   isr39
    .quad   isr40
    .quad   isr41
    .quad   isr42
    .quad   isr43
    .quad   isr44
    .quad   isr45
    .quad   isr46
    .quad   isr47
