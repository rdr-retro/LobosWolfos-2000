/* nucleo64.s - nucleo del motor egine en ensamblador x86_64.
 *
 * Esta es la variante x86_64 del nucleo (fase 6 del PLAN.md): el mismo
 * motor que nucleo32.s (misma interfaz, ver motor/motor.h del editor y
 * motor.rs del kernel) corriendo sobre el kernel loboswolfos. El lado
 * del juego (juego.rs) solo ve las funciones de abajo, nunca el
 * ensamblador.
 *
 * Convencion: SysV AMD64 con sintaxis AT&T (gnu as). Los argumentos
 * enteros y punteros llegan en rdi, rsi, rdx, rcx, r8 y r9 (del septimo
 * en adelante, apilados tras la direccion de retorno) y rax, rcx, rdx,
 * rsi, rdi y r8-r11 son del llamador; rbx y r12-r15 se guardan aqui. En
 * ELF de 64 bits los simbolos no llevan guion bajo inicial.
 *
 * En Rust se incluye con global_asm!(include_str!("nucleo64.s"),
 * options(att_syntax)): el dialecto por defecto de global_asm! es
 * Intel y este fichero es AT&T, como nucleo32.s del editor.
 *
 * Cambios respecto a nucleo32.s: SSE2 (garantizada en x86_64) en vez
 * de MMX, y punteros de 8 bytes, con lo que MotorImagen pasa de 12 a 16
 * bytes (pixeles en un qword + ancho + alto); MotorCelda sigue en 12.
 * Los recortes, el sello del tile y la AABB son identicos.
 *
 * Pixel: u32 0xAARRGGBB. El alpha es mascara en el blit con
 * transparencia; el framebuffer de destino ignora su byte alpha.
 *
 * Contenido:
 *   motor_limpiar      relleno solido con SSE2 (4 pixeles por golpe)
 *   motor_blit         blit opaco con recorte, SSE2 (2 pixeles por golpe)
 *   motor_blit_alpha   blit con mascara de transparencia
 *   motor_mapa_render  bucle de celdas visibles del mapa + sello del tile
 *   motor_aabb         colision rectangulo contra rectangulo
 */

    .text
    .code64

/* ------------------------------------------------------------------ */
/* void motor_limpiar(u32 *dst, int ancho, int alto, u32 color)         */
/* rdi=dst esi=ancho edx=alto ecx=color                                */
/* ------------------------------------------------------------------ */
    .globl motor_limpiar
motor_limpiar:
    mov     %esi, %eax
    imul    %edx, %eax            /* total de pixeles */
    test    %eax, %eax
    jle     .limpiar_fin          /* sin area: nada que rellenar */

    movd    %ecx, %xmm0           /* color */
    punpckldq %xmm0, %xmm0        /* color duplicado (2 pixeles) */
    punpckldq %xmm0, %xmm0        /* color repetido (4 pixeles) */

    mov     %eax, %ecx
    shrl    $2, %ecx              /* grupos de 4 pixeles */
    jz      .limpiar_cola
.limpiar_grupo:
    movdqu  %xmm0, (%rdi)
    add     $16, %rdi
    dec     %ecx
    jnz     .limpiar_grupo
.limpiar_cola:
    andl    $3, %eax              /* pixeles sueltos */
    jz      .limpiar_fin
.limpiar_cola_px:
    movd    %xmm0, (%rdi)
    add     $4, %rdi
    decl    %eax
    jnz     .limpiar_cola_px
.limpiar_fin:
    ret

/* ------------------------------------------------------------------ */
/* void motor_blit(u32 *dst, int dst_w, int dst_h, int dx, int dy,      */
/*                 const u32 *src, int src_w, int src_h)               */
/* rdi=dst esi=dst_w edx=dst_h ecx=dx r8d=dy r9=src                    */
/* 16(%rbp)=src_w 24(%rbp)=src_h                                       */
/* Recorta el rectangulo fuente a lo que cabe en el destino y copia    */
/* fila a fila con SSE2.                                               */
/* Locales (ranuras de 8 bytes): -4=sx -8=sy -12=sw -16=sh             */
/*                              -20=dx -24=dy -32=dst_w                */
/* ------------------------------------------------------------------ */
    .globl motor_blit
motor_blit:
    push    %rbp
    mov     %rsp, %rbp
    sub     $40, %rsp
    push    %rbx

    movl    %esi, -32(%rbp)       /* dst_w */
    movl    $0, -4(%rbp)          /* sx */
    movl    $0, -8(%rbp)          /* sy */
    movl    16(%rbp), %eax
    movl    %eax, -12(%rbp)       /* sw = src_w */
    movl    24(%rbp), %eax
    movl    %eax, -16(%rbp)       /* sh = src_h */
    movl    %ecx, -20(%rbp)       /* dx */
    movl    %r8d, -24(%rbp)       /* dy */

    /* --- recorte por la izquierda y arriba --- */
    movl    -20(%rbp), %eax       /* dx */
    testl   %eax, %eax
    jns     .blit_clip_x
    negl    %eax                  /* sx = -dx */
    movl    %eax, -4(%rbp)
    subl    %eax, -12(%rbp)       /* sw -= sx */
    movl    $0, -20(%rbp)         /* dx = 0 */
.blit_clip_x:
    movl    -24(%rbp), %ecx       /* dy */
    testl   %ecx, %ecx
    jns     .blit_clip_y
    negl    %ecx                  /* sy = -dy */
    movl    %ecx, -8(%rbp)
    subl    %ecx, -16(%rbp)       /* sh -= sy */
    movl    $0, -24(%rbp)         /* dy = 0 */
.blit_clip_y:
    /* --- recorte por la derecha y abajo --- */
    movl    -20(%rbp), %eax       /* dx */
    addl    -12(%rbp), %eax       /* dx + sw */
    cmpl    -32(%rbp), %eax       /* vs dst_w */
    jle     .blit_clip_x_fin
    movl    -32(%rbp), %eax
    subl    -20(%rbp), %eax       /* sw = dst_w - dx */
    movl    %eax, -12(%rbp)
.blit_clip_x_fin:
    cmpl    $1, -12(%rbp)
    jl      .blit_fin
    movl    -24(%rbp), %eax       /* dy */
    addl    -16(%rbp), %eax       /* dy + sh */
    cmpl    %edx, %eax            /* vs dst_h */
    jle     .blit_clip_y_fin
    movl    %edx, %eax
    subl    -24(%rbp), %eax       /* sh = dst_h - dy */
    movl    %eax, -16(%rbp)
.blit_clip_y_fin:
    cmpl    $1, -16(%rbp)
    jl      .blit_fin

    /* --- punteros de la primera fila (offset en pixeles x 4) --- */
    movl    -32(%rbp), %eax       /* dst_w */
    imull   -24(%rbp), %eax       /* dst_w * dy */
    addl    -20(%rbp), %eax       /* + dx */
    movslq  %eax, %rax
    leaq    (%rdi,%rax,4), %rdi   /* dstp */
    movl    16(%rbp), %eax        /* src_w */
    imull   -8(%rbp), %eax        /* src_w * sy */
    addl    -4(%rbp), %eax        /* + sx */
    movslq  %eax, %rax
    leaq    (%r9,%rax,4), %rsi    /* srcp */

    movl    -16(%rbp), %ebx       /* filas por copiar */
.blit_fila:
    testl   %ebx, %ebx
    jle     .blit_fin

    movl    -12(%rbp), %edx       /* pixeles de la fila */
    movl    %edx, %eax
    shrl    $1, %eax              /* pares */
    jz      .blit_cola
.blit_par:
    movq    (%rsi), %xmm0         /* 2 pixeles */
    movq    %xmm0, (%rdi)
    add     $8, %rsi
    add     $8, %rdi
    decl    %eax
    jnz     .blit_par
.blit_cola:
    testl   $1, %edx              /* fila impar: un pixel suelto */
    jz      .blit_avanza
    movl    (%rsi), %eax
    movl    %eax, (%rdi)
    add     $4, %rsi
    add     $4, %rdi
.blit_avanza:
    /* saltar al renglon siguiente: lo recorrido ya es sw pixeles */
    movl    16(%rbp), %eax        /* src_w */
    subl    -12(%rbp), %eax       /* - sw */
    movslq  %eax, %rax
    leaq    (%rsi,%rax,4), %rsi
    movl    -32(%rbp), %eax       /* dst_w */
    subl    -12(%rbp), %eax       /* - sw */
    movslq  %eax, %rax
    leaq    (%rdi,%rax,4), %rdi
    decl    %ebx
    jnz     .blit_fila
.blit_fin:
    pop     %rbx
    leave
    ret

/* ------------------------------------------------------------------ */
/* void motor_blit_alpha(u32 *dst, int dst_w, int dst_h, int dx,        */
/*                       int dy, const u32 *src, int src_w, int src_h) */
/* Igual que motor_blit pero saltando los pixeles con alpha 0 (la      */
/* transparencia de los PNG). Pixel a pixel: cada uno decide.          */
/* Locales: -4=sx -8=sy -12=sw -16=sh -20=dx -24=dy -32=dstp           */
/*          -40=srcp -48=fila -56=dst_w                                */
/* ------------------------------------------------------------------ */
    .globl motor_blit_alpha
motor_blit_alpha:
    push    %rbp
    mov     %rsp, %rbp
    sub     $64, %rsp

    movl    %esi, -56(%rbp)       /* dst_w */
    movl    $0, -4(%rbp)          /* sx */
    movl    $0, -8(%rbp)          /* sy */
    movl    16(%rbp), %eax
    movl    %eax, -12(%rbp)       /* sw = src_w */
    movl    24(%rbp), %eax
    movl    %eax, -16(%rbp)       /* sh = src_h */
    movl    %ecx, -20(%rbp)       /* dx */
    movl    %r8d, -24(%rbp)       /* dy */

    /* --- mismo recorte que motor_blit --- */
    movl    -20(%rbp), %eax       /* dx */
    testl   %eax, %eax
    jns     .alpha_clip_x
    negl    %eax
    movl    %eax, -4(%rbp)        /* sx = -dx */
    subl    %eax, -12(%rbp)
    movl    $0, -20(%rbp)         /* dx = 0 */
.alpha_clip_x:
    movl    -24(%rbp), %ecx       /* dy */
    testl   %ecx, %ecx
    jns     .alpha_clip_y
    negl    %ecx
    movl    %ecx, -8(%rbp)        /* sy = -dy */
    subl    %ecx, -16(%rbp)
    movl    $0, -24(%rbp)         /* dy = 0 */
.alpha_clip_y:
    movl    -20(%rbp), %eax
    addl    -12(%rbp), %eax       /* dx + sw */
    cmpl    -56(%rbp), %eax       /* vs dst_w */
    jle     .alpha_x_fin
    movl    -56(%rbp), %eax
    subl    -20(%rbp), %eax
    movl    %eax, -12(%rbp)
.alpha_x_fin:
    cmpl    $1, -12(%rbp)
    jl      .alpha_fin
    movl    -24(%rbp), %eax
    addl    -16(%rbp), %eax       /* dy + sh */
    cmpl    %edx, %eax            /* vs dst_h */
    jle     .alpha_y_fin
    movl    %edx, %eax
    subl    -24(%rbp), %eax
    movl    %eax, -16(%rbp)
.alpha_y_fin:
    cmpl    $1, -16(%rbp)
    jl      .alpha_fin

    /* --- punteros de la primera fila --- */
    movl    -56(%rbp), %eax       /* dst_w */
    imull   -24(%rbp), %eax       /* dst_w * dy */
    addl    -20(%rbp), %eax       /* + dx */
    movslq  %eax, %rax
    leaq    (%rdi,%rax,4), %rax
    movq    %rax, -32(%rbp)       /* dstp fila */
    movl    16(%rbp), %eax        /* src_w */
    imull   -8(%rbp), %eax        /* src_w * sy */
    addl    -4(%rbp), %eax        /* + sx */
    movslq  %eax, %rax
    leaq    (%r9,%rax,4), %rax
    movq    %rax, -40(%rbp)       /* srcp fila */

    movl    $0, -48(%rbp)         /* fila actual */

.alpha_fila:
    movl    -48(%rbp), %eax
    cmpl    -16(%rbp), %eax      /* fila >= sh */
    jge     .alpha_fin

    movq    -40(%rbp), %rsi      /* src cursor */
    movq    -32(%rbp), %rdi      /* dst cursor */
    movl    -12(%rbp), %edx      /* pixeles de la fila */
.alpha_px:
    testl   %edx, %edx
    jz      .alpha_avanza
    movl    (%rsi), %eax         /* pixel */
    testl   $0xFF000000, %eax    /* alpha 0 = transparente */
    jz      .alpha_sig_px
    movl    %eax, (%rdi)
.alpha_sig_px:
    add     $4, %rsi
    add     $4, %rdi
    decl    %edx
    jnz     .alpha_px
.alpha_avanza:
    movl    16(%rbp), %eax       /* src_w */
    shll    $2, %eax             /* a bytes */
    movslq  %eax, %rax
    addq    %rax, -40(%rbp)      /* srcp fila += src_w */
    movl    -56(%rbp), %eax      /* dst_w */
    shll    $2, %eax
    movslq  %eax, %rax
    addq    %rax, -32(%rbp)      /* dstp fila += dst_w */
    incl    -48(%rbp)
    jmp     .alpha_fila
.alpha_fin:
    leave
    ret

/* ------------------------------------------------------------------ */
/* void motor_mapa_render(u32 *dst, int dst_w, int dst_h,               */
/*                        int cam_x, int cam_y, int lado,               */
/*                        const MotorCelda *celdas, int num,           */
/*                        const MotorImagen *imagenes)                 */
/* rdi=dst esi=dst_w edx=dst_h ecx=cam_x r8d=cam_y r9d=lado            */
/* 16(%rbp)=celdas 24(%rbp)=num 32(%rbp)=imagenes                      */
/*                                                                      */
/* Bucle de celdas visibles del mapa + sello del tile: cada celda es un */
/* cuadrado de `lado` pixeles y el tile se estampa dentro escalado y    */
/* centrado (el mismo criterio que el editor de mapas).                */
/* MotorCelda: int x, y, imagen (12 bytes).                            */
/* MotorImagen: u32 *pixeles, int ancho, alto (16 bytes en             */
/* x86_64: puntero de 8 bytes).                                        */
/* Locales: -8=celda -16=restantes -24=lado -32=cam_x -40=cam_y         */
/*          -48=dst -56=dst_w -64=dst_h -72=imagenes -80=imagen        */
/*          -88=sx -96=sy -104=ox -112=oy -120=dw -128=dh              */
/*          -136=tw -144=th -152=src -160=dy -168=M                     */
/* ------------------------------------------------------------------ */
    .globl motor_mapa_render
motor_mapa_render:
    push    %rbp
    mov     %rsp, %rbp
    sub     $176, %rsp
    push    %rbx

    movq    %rdi, -48(%rbp)      /* dst */
    movl    %esi, -56(%rbp)      /* dst_w */
    movl    %edx, -64(%rbp)      /* dst_h */
    movl    %ecx, -32(%rbp)      /* cam_x */
    movl    %r8d, -40(%rbp)      /* cam_y */
    movl    %r9d, -24(%rbp)      /* lado */
    movq    16(%rbp), %rax
    movq    %rax, -8(%rbp)       /* celda cursor */
    movl    24(%rbp), %eax
    movl    %eax, -16(%rbp)      /* celdas restantes */
    movq    32(%rbp), %rax
    movq    %rax, -72(%rbp)      /* imagenes */

.mapa_siguiente:
    cmpl    $0, -16(%rbp)
    jle     .mapa_fin
    movq    -8(%rbp), %rsi       /* celda */

    /* esquina de la celda en pantalla */
    movl    (%rsi), %eax         /* cx */
    imull   -24(%rbp), %eax      /* cx*lado */
    subl    -32(%rbp), %eax      /* - cam_x */
    movl    %eax, -88(%rbp)      /* sx */
    movl    4(%rsi), %eax        /* cy */
    imull   -24(%rbp), %eax
    subl    -40(%rbp), %eax      /* - cam_y */
    movl    %eax, -96(%rbp)      /* sy */

    /* descartar la celda si no toca la pantalla */
    movl    -88(%rbp), %eax
    cmpl    -56(%rbp), %eax      /* sx >= dst_w */
    jge     .mapa_celda_sig
    addl    -24(%rbp), %eax       /* sx + lado */
    jle     .mapa_celda_sig      /* <= 0 */
    movl    -96(%rbp), %eax
    cmpl    -64(%rbp), %eax      /* sy >= dst_h */
    jge     .mapa_celda_sig
    addl    -24(%rbp), %eax
    jle     .mapa_celda_sig

    /* imagen de la celda (MotorImagen de 16 bytes en x86_64) */
    movl    8(%rsi), %eax        /* indice de imagen */
    imull   $16, %eax            /* sizeof(MotorImagen) */
    movslq  %eax, %rax
    addq    -72(%rbp), %rax
    movq    %rax, -80(%rbp)      /* imagen */
    movq    (%rax), %rcx         /* pixeles */
    testq   %rcx, %rcx
    jz      .mapa_celda_sig      /* sin imagen: celda hueca */
    movq    %rcx, -152(%rbp)     /* src */
    movl    8(%rax), %ecx
    movl    %ecx, -136(%rbp)     /* tw */
    movl    12(%rax), %ecx
    movl    %ecx, -144(%rbp)     /* th */
    cmpl    $0, -136(%rbp)
    jle     .mapa_celda_sig
    cmpl    $0, -144(%rbp)
    jle     .mapa_celda_sig

    /* dw/dh: escala del editor, el lado manda sobre la dimension
     * mayor: dw = (tw*lado + M/2)/M con M = max(tw,th), recortado a
     * [1, lado]. */
    movl    -136(%rbp), %eax     /* tw */
    movl    -144(%rbp), %ecx     /* th */
    movl    %eax, %edx
    cmpl    %ecx, %eax
    jge     .mapa_m             /* tw >= th: M = tw */
    movl    %ecx, %edx           /* M = th */
.mapa_m:
    movl    %edx, -168(%rbp)     /* M */
    movl    -136(%rbp), %eax     /* tw */
    imull   -24(%rbp), %eax     /* tw*lado */
    movl    -168(%rbp), %ecx
    shrl    $1, %ecx             /* M/2 */
    addl    %ecx, %eax
    xorl    %edx, %edx
    movl    -168(%rbp), %ecx
    divl    %ecx                /* eax = dw */
    cmpl    $1, %eax
    jge     .mapa_dw_ok
    movl    $1, %eax
.mapa_dw_ok:
    cmpl    -24(%rbp), %eax
    jle     .mapa_dw_ok2
    movl    -24(%rbp), %eax
.mapa_dw_ok2:
    movl    %eax, -120(%rbp)     /* dw */

    movl    -144(%rbp), %eax     /* th */
    imull   -24(%rbp), %eax     /* th*lado */
    movl    -168(%rbp), %ecx
    shrl    $1, %ecx
    addl    %ecx, %eax
    xorl    %edx, %edx
    movl    -168(%rbp), %ecx
    divl    %ecx                /* eax = dh */
    cmpl    $1, %eax
    jge     .mapa_dh_ok
    movl    $1, %eax
.mapa_dh_ok:
    cmpl    -24(%rbp), %eax
    jle     .mapa_dh_ok2
    movl    -24(%rbp), %eax
.mapa_dh_ok2:
    movl    %eax, -128(%rbp)     /* dh */

    /* origen del sello: centrado en la celda */
    movl    -24(%rbp), %eax      /* lado */
    subl    -120(%rbp), %eax     /* - dw */
    sarl    $1, %eax
    addl    -88(%rbp), %eax      /* + sx */
    movl    %eax, -104(%rbp)     /* ox */
    movl    -24(%rbp), %eax      /* lado */
    subl    -128(%rbp), %eax     /* - dh */
    sarl    $1, %eax
    addl    -96(%rbp), %eax      /* + sy */
    movl    %eax, -112(%rbp)     /* oy */

    /* bucle de pixeles del sello (vecino mas cercano + mascara) */
    movl    $0, -160(%rbp)      /* dy */
.mapa_fila:
    movl    -160(%rbp), %eax
    cmpl    -128(%rbp), %eax     /* dy >= dh */
    jge     .mapa_celda_sig
    movl    -112(%rbp), %ecx
    addl    %eax, %ecx           /* py = oy + dy */
    cmpl    $0, %ecx
    jl      .mapa_fila_sig
    cmpl    -64(%rbp), %ecx      /* py >= dst_h */
    jge     .mapa_fila_sig

    /* fila fuente: src + (dy*th/dh)*tw */
    movl    -160(%rbp), %eax
    imull   -144(%rbp), %eax     /* dy*th */
    xorl    %edx, %edx
    movl    -128(%rbp), %ecx     /* dh */
    divl    %ecx                /* ssy */
    cmpl    -144(%rbp), %eax
    jl      .mapa_ssy_ok
    movl    -144(%rbp), %eax
    decl    %eax
.mapa_ssy_ok:
    imull   -136(%rbp), %eax     /* ssy*tw: offset en pixeles */
    movslq  %eax, %rax
    movq    -152(%rbp), %rdx     /* src */
    leaq    (%rdx,%rax,4), %rbx  /* fila fuente en bytes */

    /* fila destino: dst + py*dst_w*4 */
    movl    -112(%rbp), %eax
    addl    -160(%rbp), %eax     /* py */
    imull   -56(%rbp), %eax      /* py*dst_w */
    movslq  %eax, %rax
    movq    -48(%rbp), %rdx      /* dst */
    leaq    (%rdx,%rax,4), %rdi  /* fila destino en bytes */

    xorl    %esi, %esi           /* dx */
.mapa_col:
    cmpl    -120(%rbp), %esi     /* dx >= dw */
    jge     .mapa_fila_sig
    movl    -104(%rbp), %eax
    addl    %esi, %eax           /* px = ox + dx */
    cmpl    $0, %eax
    jl      .mapa_col_sig
    cmpl    -56(%rbp), %eax      /* px >= dst_w */
    jge     .mapa_col_sig

    /* columna fuente: ssx = dx*tw/dw */
    movl    %esi, %eax
    imull   -136(%rbp), %eax     /* dx*tw */
    xorl    %edx, %edx
    movl    -120(%rbp), %ecx     /* dw */
    divl    %ecx                /* ssx */
    cmpl    -136(%rbp), %eax
    jl      .mapa_ssx_ok
    movl    -136(%rbp), %eax
    decl    %eax
.mapa_ssx_ok:
    movl    (%rbx,%rax,4), %ecx  /* pixel fuente */
    testl   $0xFF000000, %ecx    /* alpha 0 = transparente */
    jz      .mapa_col_sig
    movl    -104(%rbp), %eax
    addl    %esi, %eax           /* px */
    movl    %ecx, (%rdi,%rax,4)
.mapa_col_sig:
    incl    %esi
    jmp     .mapa_col
.mapa_fila_sig:
    incl    -160(%rbp)
    jmp     .mapa_fila
.mapa_celda_sig:
    addq    $12, -8(%rbp)        /* siguiente celda */
    decl    -16(%rbp)
    jmp     .mapa_siguiente
.mapa_fin:
    pop     %rbx
    leave
    ret

/* ------------------------------------------------------------------ */
/* int motor_aabb(int ax, int ay, int aw, int ah,                       */
/*                int bx, int by, int bw, int bh)                      */
/* edi=ax esi=ay edx=aw ecx=ah r8d=bx r9d=by 16(%rbp)=bw 24(%rbp)=bh    */
/* 1 si los rectangulos se solapan (los bordes que se tocan no        */
/* cuentan), 0 si no.                                                  */
/* ------------------------------------------------------------------ */
    .globl motor_aabb
motor_aabb:
    push    %rbp
    mov     %rsp, %rbp

    movl    16(%rbp), %eax       /* bw */
    addl    %r8d, %eax           /* bx + bw */
    cmpl    %edi, %eax            /* <= ax */
    jle     .aabb_no
    movl    %edi, %eax           /* ax */
    addl    %edx, %eax           /* ax + aw */
    cmpl    %r8d, %eax            /* <= bx */
    jle     .aabb_no
    movl    24(%rbp), %eax       /* bh */
    addl    %r9d, %eax           /* by + bh */
    cmpl    %esi, %eax            /* <= ay */
    jle     .aabb_no
    movl    %esi, %eax           /* ay */
    addl    %ecx, %eax           /* ay + ah */
    cmpl    %r9d, %eax            /* <= by */
    jle     .aabb_no
    movl    $1, %eax
    pop     %rbp
    ret
.aabb_no:
    xorl    %eax, %eax
    pop     %rbp
    ret
