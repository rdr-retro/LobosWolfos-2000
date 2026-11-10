/* El segundo tramo del arranque (bootusb2) carga el kernel directamente en
 * 1 MiB con la identidad de paginacion ya montada. */
ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .text : {
        *(.text.boot)
        *(.text .text.*)
    }

    .lowtext : ALIGN(16) {
        __lowtext_start = .;
        KEEP(*(.lowtext))
        __lowtext_end = .;
    }

    .rodata : { *(.rodata .rodata.*) }
    .data   : { *(.data .data.*) }

    /* .bss debe quedar a cero: en un PC real la RAM no arranca limpia (en QEMU
     * si), y las tablas que el kernel solo rellena en parte (la IDT, sobre todo)
     * se leen antes de escribirlas. `start.s` lo pone a cero al entrar. */
    . = ALIGN(16);
    __bss_start = .;
    .bss    : { *(.bss .bss.*) *(COMMON) }
    __bss_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame.*)
    }

    /* El kernel se reubica a 1 MiB. Su imagen y BSS deben acabar antes de la
     * pila persistente, reservada en [3, 4) MiB. */
    ASSERT(__bss_end <= 0x300000, "kernel/BSS invade la pila de 3-4 MiB")
}
