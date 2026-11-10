/* Script de enlazado del sector de arranque USB de loboswolfos
 * (copiado de rocky: el trazado de la imagen es identico).
 * El sector vive en 0x7C00, mide exactamente 512 bytes y termina en 0xAA55.
 *
 * La tabla de particiones del MBR empieza en el offset 446 (0x7DBE); ahi la
 * escribe `xtask`. El codigo de este tramo tiene que acabar antes. */
ENTRY(_start)

SECTIONS
{
    . = 0x7C00;

    .text : {
        *(.text.boot)
        *(.text .text.*)
    }

    .rodata : { *(.rodata .rodata.*) }
    .data   : { *(.data .data.*) }

    . = 0x7DFE;
    .bootsig : { SHORT(0xAA55) }

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame.*)
        *(.bss .bss.*)
        *(COMMON)
    }
}

/* El codigo arranca justo detras del BPB (0x7C00 + 0x54) y acaba antes de la
 * tabla de particiones (offset 446). */
ASSERT(__codigo - _start == 0x54, "el BPB del MBR no mide 0x54 bytes")
ASSERT(__fin_codigo <= 0x7DBE, "el sector de arranque invade la tabla de particiones (offset 446)")
ASSERT(. <= 0x7E00, "el sector de arranque excede 512 bytes")
