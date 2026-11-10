/* Script de enlazado del segundo tramo del arranque por USB de loboswolfos
 * (copiado de rocky: el trazado de la imagen es identico).
 * Vive en 0x8000 (ahi lo carga el sector de arranque) y tiene que caber en
 * los sectores que el MBR lee de golpe (`USB_TRAMO2_SECTORES` en xtask). */
ENTRY(_start)

SECTIONS
{
    . = 0x8000;

    .text : {
        *(.text.boot)
        *(.text .text.*)
    }

    .rodata : { *(.rodata .rodata.*) }
    .data   : { *(.data .data.*) }

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame.*)
        *(.bss .bss.*)
        *(COMMON)
    }
}

ASSERT(. <= 0x8000 + 4 * 512, "el segundo tramo no cabe en los cuatro sectores que carga el MBR")
