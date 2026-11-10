/* nucleo32.s - nucleo del motor egine en ensamblador x86 de 32 bits.
 *
 * Esta es la parte esencial del motor para game.exe (Windows 2000/XP,
 * MinGW i686). La misma interfaz (motor.h) tendra su variante x86_64 para
 * el kernel loboswolfos en nucleo64.s (fase 6 del PLAN.md).
 *
 * Convencion: cdecl con sintaxis AT&T (gnu as). Los argumentos llegan en
 * la pila a partir de 8(%ebp) y el llamador la limpia; eax/ecx/edx son
 * del llamador y ebx/esi/edi se guardan aqui. En PE de 32 bits los
 * simbolos C llevan guion bajo inicial (_motor_blit).
 *
 * Pixel: u32 0xAARRGGBB. El alpha es mascara en el blit con
 * transparencia; el framebuffer de destino ignora su byte alpha.
 *
 * Contenido:
 *   motor_limpiar      relleno solido con MMX (8 pixeles por golpe)
 *   motor_blit         blit opaco con recorte, MMX (2 pixeles por golpe)
 *   motor_blit_alpha   blit con mascara de transparencia
 *   motor_mapa_render  bucle de celdas visibles del mapa + sello del tile
 *   motor_aabb         colision rectangulo contra rectangulo
 */

    .text

/* ------------------------------------------------------------------ */
/* void motor_limpiar(u32 *dst, int ancho, int alto, u32 color)         */
/* 8(%ebp)=dst 12(%ebp)=ancho 16(%ebp)=alto 20(%ebp)=color             */
/* ------------------------------------------------------------------ */
    .globl _motor_limpiar
_motor_limpiar:
    pushl %ebp
    movl %esp, %ebp
    pushl %edi

    movl 8(%ebp), %edi      /* dst */
    movl 12(%ebp), %eax     /* ancho */
    imull 16(%ebp), %eax    /* total de pixeles */
    jle .limpiar_fin        /* sin area: nada que rellenar */

    movd 20(%ebp), %mm0     /* color */
    punpckldq %mm0, %mm0    /* color duplicado (8 bytes) */

    movl %eax, %ecx
    shrl $1, %ecx           /* pares de pixeles */
    jz .limpiar_cola
.limpiar_par:
    movq %mm0, (%edi)
    addl $8, %edi
    decl %ecx
    jnz .limpiar_par
.limpiar_cola:
    testl $1, %eax          /* ancho*alto impar: un pixel suelto */
    jz .limpiar_cerrar
    movd %mm0, (%edi)
.limpiar_cerrar:
    emms
.limpiar_fin:
    popl %edi
    popl %ebp
    ret

/* ------------------------------------------------------------------ */
/* void motor_blit(u32 *dst, int dst_w, int dst_h, int dx, int dy,      */
/*                 const u32 *src, int src_w, int src_h)               */
/* 8=dst 12=dst_w 16=dst_h 20=dx 24=dy 28=src 32=src_w 36=src_h         */
/* Recorta el rectangulo fuente a lo que cabe en el destino y copia    */
/* fila a fila con MMX.                                                */
/* ------------------------------------------------------------------ */
    .globl _motor_blit
_motor_blit:
    pushl %ebp
    movl %esp, %ebp
    subl $16, %esp          /* -4=sx -8=sy -12=sw -16=sh */
    pushl %ebx
    pushl %esi
    pushl %edi

    movl $0, -4(%ebp)       /* sx */
    movl $0, -8(%ebp)       /* sy */
    movl 32(%ebp), %eax     /* src_w */
    movl %eax, -12(%ebp)    /* sw */
    movl 36(%ebp), %eax     /* src_h */
    movl %eax, -16(%ebp)    /* sh */

    /* --- recorte por la izquierda y arriba --- */
    movl 20(%ebp), %eax     /* dx */
    testl %eax, %eax
    jns .blit_clip_x
    negl %eax               /* sx = -dx */
    movl %eax, -4(%ebp)
    subl %eax, -12(%ebp)    /* sw -= sx */
    xorl %eax, %eax         /* dx = 0 */
    movl %eax, 20(%ebp)
.blit_clip_x:
    movl 24(%ebp), %ecx     /* dy */
    testl %ecx, %ecx
    jns .blit_clip_y
    negl %ecx               /* sy = -dy */
    movl %ecx, -8(%ebp)
    subl %ecx, -16(%ebp)    /* sh -= sy */
    xorl %ecx, %ecx         /* dy = 0 */
    movl %ecx, 24(%ebp)
.blit_clip_y:
    /* --- recorte por la derecha y abajo --- */
    movl 20(%ebp), %eax     /* dx */
    addl -12(%ebp), %eax    /* dx + sw */
    cmpl 12(%ebp), %eax     /* vs dst_w: eax - dst_w */
    jle .blit_clip_x_fin
    movl 12(%ebp), %eax
    subl 20(%ebp), %eax     /* sw = dst_w - dx */
    movl %eax, -12(%ebp)
.blit_clip_x_fin:
    cmpl $1, -12(%ebp)
    jl .blit_fin
    movl 24(%ebp), %eax     /* dy */
    addl -16(%ebp), %eax    /* dy + sh */
    cmpl 16(%ebp), %eax     /* vs dst_h */
    jle .blit_clip_y_fin
    movl 16(%ebp), %eax
    subl 24(%ebp), %eax     /* sh = dst_h - dy */
    movl %eax, -16(%ebp)
.blit_clip_y_fin:
    cmpl $1, -16(%ebp)
    jl .blit_fin

    /* --- punteros de la primera fila (offset en pixeles x 4 bytes) --- */
    movl 12(%ebp), %edx     /* dst_w */
    imull 24(%ebp), %edx    /* dst_w * dy */
    addl 20(%ebp), %edx     /* + dx: offset en pixeles */
    movl 8(%ebp), %eax      /* dst */
    leal (%eax,%edx,4), %edi /* dstp = dst + offset*4 */

    movl 32(%ebp), %edx     /* src_w */
    imull -8(%ebp), %edx    /* src_w * sy */
    addl -4(%ebp), %edx    /* + sx: offset en pixeles */
    movl 28(%ebp), %eax     /* src */
    leal (%eax,%edx,4), %esi /* srcp = src + offset*4 */

    movl -16(%ebp), %ebx    /* filas por copiar */
.blit_fila:
    testl %ebx, %ebx
    jle .blit_cerrar

    movl -12(%ebp), %edx    /* pixeles de la fila */
    movl %edx, %eax
    shrl $1, %eax           /* pares */
    jz .blit_cola
.blit_par:
    movq (%esi), %mm0
    movq %mm0, (%edi)
    addl $8, %esi
    addl $8, %edi
    decl %eax
    jnz .blit_par
.blit_cola:
    testl $1, %edx          /* fila impar: un pixel suelto */
    jz .blit_avanza
    movl (%esi), %eax
    movl %eax, (%edi)
    addl $4, %esi
    addl $4, %edi
.blit_avanza:
    /* saltar al renglon siguiente: lo recorrido ya es sw pixeles */
    movl 32(%ebp), %eax     /* src_w */
    subl -12(%ebp), %eax    /* - sw */
    leal (%esi,%eax,4), %esi
    movl 12(%ebp), %eax     /* dst_w */
    subl -12(%ebp), %eax    /* - sw */
    leal (%edi,%eax,4), %edi
    decl %ebx
    jnz .blit_fila
.blit_cerrar:
    emms
.blit_fin:
    popl %edi
    popl %esi
    popl %ebx
    movl %ebp, %esp
    popl %ebp
    ret

/* ------------------------------------------------------------------ */
/* void motor_blit_alpha(u32 *dst, int dst_w, int dst_h, int dx, int dy,*/
/*                       const u32 *src, int src_w, int src_h)         */
/* Igual que motor_blit pero saltando los pixeles con alpha 0 (la      */
/* transparencia de los PNG). Sin MMX: cada pixel decide su mascara.   */
/* ------------------------------------------------------------------ */
    .globl _motor_blit_alpha
_motor_blit_alpha:
    pushl %ebp
    movl %esp, %ebp
    subl $32, %esp          /* -4=sx -8=sy -12=sw -16=sh -20=dstp
                               -24=srcp -28=fila */
    pushl %ebx
    pushl %esi
    pushl %edi

    movl $0, -4(%ebp)
    movl $0, -8(%ebp)
    movl 32(%ebp), %eax
    movl %eax, -12(%ebp)    /* sw = src_w */
    movl 36(%ebp), %eax
    movl %eax, -16(%ebp)    /* sh = src_h */

    movl 20(%ebp), %eax     /* dx */
    testl %eax, %eax
    jns .alpha_clip_x
    negl %eax
    movl %eax, -4(%ebp)     /* sx = -dx */
    subl %eax, -12(%ebp)
    xorl %eax, %eax
    movl %eax, 20(%ebp)
.alpha_clip_x:
    movl 24(%ebp), %ecx     /* dy */
    testl %ecx, %ecx
    jns .alpha_clip_y
    negl %ecx
    movl %ecx, -8(%ebp)     /* sy = -dy */
    subl %ecx, -16(%ebp)
    xorl %ecx, %ecx
    movl %ecx, 24(%ebp)
.alpha_clip_y:
    movl 20(%ebp), %eax
    addl -12(%ebp), %eax
    cmpl 12(%ebp), %eax
    jle .alpha_x_fin
    movl 12(%ebp), %eax
    subl 20(%ebp), %eax
    movl %eax, -12(%ebp)
.alpha_x_fin:
    cmpl $1, -12(%ebp)
    jl .alpha_fin
    movl 24(%ebp), %eax
    addl -16(%ebp), %eax
    cmpl 16(%ebp), %eax
    jle .alpha_y_fin
    movl 16(%ebp), %eax
    subl 24(%ebp), %eax
    movl %eax, -16(%ebp)
.alpha_y_fin:
    cmpl $1, -16(%ebp)
    jl .alpha_fin

    movl 12(%ebp), %edx     /* dst_w */
    imull 24(%ebp), %edx    /* dst_w * dy */
    addl 20(%ebp), %edx     /* + dx: offset en pixeles */
    movl 8(%ebp), %eax      /* dst */
    leal (%eax,%edx,4), %eax
    movl %eax, -20(%ebp)    /* dstp fila */

    movl 32(%ebp), %edx     /* src_w */
    imull -8(%ebp), %edx    /* src_w * sy */
    addl -4(%ebp), %edx    /* + sx: offset en pixeles */
    movl 28(%ebp), %eax     /* src */
    leal (%eax,%edx,4), %eax
    movl %eax, -24(%ebp)    /* srcp fila */

    movl $0, -28(%ebp)      /* fila actual */
.alpha_fila:
    movl -28(%ebp), %eax
    cmpl -16(%ebp), %eax
    jge .alpha_fin

    movl -24(%ebp), %esi    /* src cursor */
    movl -20(%ebp), %edi    /* dst cursor */
    movl -12(%ebp), %edx    /* pixeles de la fila */
.alpha_px:
    testl %edx, %edx
    jz .alpha_avanza
    movl (%esi), %eax       /* pixel */
    testl $0xFF000000, %eax /* alpha 0 = transparente */
    jz .alpha_sig_px
    movl %eax, (%edi)
.alpha_sig_px:
    addl $4, %esi
    addl $4, %edi
    decl %edx
    jnz .alpha_px
.alpha_avanza:
    movl 32(%ebp), %eax     /* src_w */
    shll $2, %eax
    addl %eax, -24(%ebp)    /* srcp fila += src_w */
    movl 12(%ebp), %eax     /* dst_w */
    shll $2, %eax
    addl %eax, -20(%ebp)    /* dstp fila += dst_w */
    incl -28(%ebp)
    jmp .alpha_fila
.alpha_fin:
    popl %edi
    popl %esi
    popl %ebx
    movl %ebp, %esp
    popl %ebp
    ret

/* ------------------------------------------------------------------ */
/* void motor_mapa_render(u32 *dst, int dst_w, int dst_h,               */
/*                        int cam_x, int cam_y, int lado,              */
/*                        const MotorCelda *celdas, int num,           */
/*                        const MotorImagen *imagenes)                */
/* 8=dst 12=dst_w 16=dst_h 20=cam_x 24=cam_y 28=lado 32=celdas         */
/* 36=num 40=imagenes                                                   */
/*                                                                      */
/* Bucle de celdas visibles del mapa + sello del tile: cada celda es un */
/* cuadrado de `lado` pixeles y el tile se estampa dentro escalado y    */
/* centrado (el mismo criterio que el editor de mapas).                */
/* MotorCelda = { int x, y, imagen } (12 bytes).                        */
/* MotorImagen = { u32 *pixeles, int ancho, alto } (12 bytes).          */
/* ------------------------------------------------------------------ */
    .globl _motor_mapa_render
_motor_mapa_render:
    pushl %ebp
    movl %esp, %ebp
    subl $88, %esp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl 8(%ebp), %eax
    movl %eax, -24(%ebp)    /* dst */
    movl 12(%ebp), %eax
    movl %eax, -28(%ebp)    /* dst_w */
    movl 16(%ebp), %eax
    movl %eax, -32(%ebp)    /* dst_h */
    movl 20(%ebp), %eax
    movl %eax, -16(%ebp)    /* cam_x */
    movl 24(%ebp), %eax
    movl %eax, -20(%ebp)    /* cam_y */
    movl 28(%ebp), %eax
    movl %eax, -12(%ebp)    /* lado */
    movl 32(%ebp), %eax
    movl %eax, -8(%ebp)     /* celda cursor */
    movl 36(%ebp), %eax
    movl %eax, -4(%ebp)     /* celdas restantes */
    movl 40(%ebp), %eax
    movl %eax, -36(%ebp)    /* imagenes */

.mapa_siguiente:
    cmpl $0, -4(%ebp)
    jle .mapa_fin
    movl -8(%ebp), %esi     /* celda */

    /* esquina de la celda en pantalla */
    movl (%esi), %eax        /* cx */
    imull -12(%ebp), %eax
    subl -16(%ebp), %eax    /* sx = cx*lado - cam_x */
    movl %eax, -44(%ebp)
    movl 4(%esi), %eax      /* cy */
    imull -12(%ebp), %eax
    subl -20(%ebp), %eax    /* sy = cy*lado - cam_y */
    movl %eax, -48(%ebp)

    /* descartar la celda si no toca la pantalla */
    movl -44(%ebp), %eax
    cmpl -28(%ebp), %eax    /* sx >= dst_w */
    jge .mapa_celda_sig
    addl -12(%ebp), %eax    /* sx + lado */
    jle .mapa_celda_sig     /* <= 0 */
    movl -48(%ebp), %eax
    cmpl -32(%ebp), %eax    /* sy >= dst_h */
    jge .mapa_celda_sig
    addl -12(%ebp), %eax
    jle .mapa_celda_sig

    /* imagen de la celda */
    movl 8(%esi), %eax      /* indice de imagen */
    imull $12, %eax         /* sizeof(MotorImagen) */
    addl -36(%ebp), %eax
    movl %eax, -40(%ebp)    /* imagen */
    movl (%eax), %ecx       /* pixeles */
    testl %ecx, %ecx
    jz .mapa_celda_sig     /* sin imagen: celda hueca */
    movl %ecx, -76(%ebp)    /* src */
    movl 4(%eax), %ecx
    movl %ecx, -68(%ebp)    /* tw */
    movl 8(%eax), %ecx
    movl %ecx, -72(%ebp)    /* th */
    cmpl $0, -68(%ebp)
    jle .mapa_celda_sig
    cmpl $0, -72(%ebp)
    jle .mapa_celda_sig

    /* dw/dh: escala del editor, el lado manda sobre la dimension
     * mayor: dw = (tw*lado + M/2)/M con M = max(tw,th), recortado a
     * [1, lado]. */
    movl -68(%ebp), %eax    /* tw */
    movl -72(%ebp), %ecx    /* th */
    movl %eax, %edx
    cmpl %ecx, %eax
    jge .mapa_m             /* tw >= th: M = tw */
    movl %ecx, %edx         /* M = th */
.mapa_m:
    movl %edx, -84(%ebp)    /* M */
    movl -68(%ebp), %eax    /* tw */
    imull -12(%ebp), %eax   /* tw*lado */
    movl -84(%ebp), %ecx
    shrl $1, %ecx           /* M/2 */
    addl %ecx, %eax
    xorl %edx, %edx
    movl -84(%ebp), %ecx
    divl %ecx               /* eax = dw */
    cmpl $1, %eax
    jge .mapa_dw_ok
    movl $1, %eax
.mapa_dw_ok:
    cmpl -12(%ebp), %eax
    jle .mapa_dw_ok2
    movl -12(%ebp), %eax
.mapa_dw_ok2:
    movl %eax, -60(%ebp)    /* dw */

    movl -72(%ebp), %eax    /* th */
    imull -12(%ebp), %eax   /* th*lado */
    movl -84(%ebp), %ecx
    shrl $1, %ecx
    addl %ecx, %eax
    xorl %edx, %edx
    movl -84(%ebp), %ecx
    divl %ecx               /* eax = dh */
    cmpl $1, %eax
    jge .mapa_dh_ok
    movl $1, %eax
.mapa_dh_ok:
    cmpl -12(%ebp), %eax
    jle .mapa_dh_ok2
    movl -12(%ebp), %eax
.mapa_dh_ok2:
    movl %eax, -64(%ebp)    /* dh */

    /* origen del sello: centrado en la celda */
    movl -12(%ebp), %eax    /* lado */
    subl -60(%ebp), %eax    /* - dw */
    sarl $1, %eax
    addl -44(%ebp), %eax    /* + sx */
    movl %eax, -52(%ebp)    /* ox */
    movl -12(%ebp), %eax    /* lado */
    subl -64(%ebp), %eax    /* - dh */
    sarl $1, %eax
    addl -48(%ebp), %eax    /* + sy */
    movl %eax, -56(%ebp)    /* oy */

    /* bucle de pixeles del sello (vecino mas cercano + mascara) */
    movl $0, -80(%ebp)      /* dy */
.mapa_fila:
    movl -80(%ebp), %eax
    cmpl -64(%ebp), %eax    /* dy >= dh */
    jge .mapa_celda_sig
    movl -56(%ebp), %ecx
    addl %eax, %ecx         /* py = oy + dy */
    cmpl $0, %ecx
    jl .mapa_fila_sig
    cmpl -32(%ebp), %ecx    /* py >= dst_h */
    jge .mapa_fila_sig

    /* fila fuente: src + (dy*th/dh)*tw */
    movl -80(%ebp), %eax
    imull -72(%ebp), %eax   /* dy*th */
    xorl %edx, %edx
    movl -64(%ebp), %ecx    /* dh */
    divl %ecx               /* ssy */
    cmpl -72(%ebp), %eax
    jl .mapa_ssy_ok
    movl -72(%ebp), %eax
    decl %eax
.mapa_ssy_ok:
    imull -68(%ebp), %eax   /* ssy*tw: offset en pixeles */
    movl -76(%ebp), %edx    /* src */
    leal (%edx,%eax,4), %ebx /* fila fuente en bytes */

    /* fila destino: dst + py*dst_w*4 */
    movl -56(%ebp), %eax
    addl -80(%ebp), %eax    /* py */
    imull -28(%ebp), %eax   /* py*dst_w: offset en pixeles */
    movl -24(%ebp), %edx    /* dst */
    leal (%edx,%eax,4), %edi /* fila destino en bytes */

    xorl %esi, %esi         /* dx */
.mapa_col:
    cmpl -60(%ebp), %esi    /* dx >= dw */
    jge .mapa_fila_sig
    movl -52(%ebp), %eax
    addl %esi, %eax         /* px = ox + dx */
    cmpl $0, %eax
    jl .mapa_col_sig
    cmpl -28(%ebp), %eax    /* px >= dst_w */
    jge .mapa_col_sig

    /* columna fuente: ssx = dx*tw/dw */
    movl %esi, %eax
    imull -68(%ebp), %eax   /* dx*tw */
    xorl %edx, %edx
    movl -60(%ebp), %ecx    /* dw */
    divl %ecx               /* ssx */
    cmpl -68(%ebp), %eax
    jl .mapa_ssx_ok
    movl -68(%ebp), %eax
    decl %eax
.mapa_ssx_ok:
    movl (%ebx,%eax,4), %ecx /* pixel fuente */
    testl $0xFF000000, %ecx  /* alpha 0 = transparente */
    jz .mapa_col_sig
    movl -52(%ebp), %eax
    addl %esi, %eax         /* px */
    movl %ecx, (%edi,%eax,4)
.mapa_col_sig:
    incl %esi
    jmp .mapa_col
.mapa_fila_sig:
    incl -80(%ebp)
    jmp .mapa_fila
.mapa_celda_sig:
    addl $12, -8(%ebp)      /* siguiente celda */
    decl -4(%ebp)
    jmp .mapa_siguiente
.mapa_fin:
    popl %edi
    popl %esi
    popl %ebx
    movl %ebp, %esp
    popl %ebp
    ret

/* ------------------------------------------------------------------ */
/* int motor_aabb(int ax, int ay, int aw, int ah,                       */
/*                int bx, int by, int bw, int bh)                      */
/* 8=ax 12=ay 16=aw 20=ah 24=bx 28=by 32=bw 36=bh                       */
/* 1 si los rectangulos se solapan (los bordes que se tocan no        */
/* cuentan), 0 si no.                                                  */
/* ------------------------------------------------------------------ */
    .globl _motor_aabb
_motor_aabb:
    pushl %ebp
    movl %esp, %ebp

    movl 24(%ebp), %eax     /* bx */
    addl 32(%ebp), %eax     /* bx + bw */
    cmpl 8(%ebp), %eax      /* <= ax */
    jle .aabb_no
    movl 8(%ebp), %eax      /* ax */
    addl 16(%ebp), %eax     /* ax + aw */
    cmpl 24(%ebp), %eax     /* <= bx */
    jle .aabb_no
    movl 28(%ebp), %eax     /* by */
    addl 36(%ebp), %eax     /* by + bh */
    cmpl 12(%ebp), %eax     /* <= ay */
    jle .aabb_no
    movl 12(%ebp), %eax     /* ay */
    addl 20(%ebp), %eax     /* ay + ah */
    cmpl 28(%ebp), %eax     /* <= by */
    jle .aabb_no
    movl $1, %eax
    popl %ebp
    ret
.aabb_no:
    xorl %eax, %eax
    popl %ebp
    ret
