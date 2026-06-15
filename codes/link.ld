ENTRY(_start)

SECTIONS {
    . = 0x00000000;

    .text   : { *(.text*) }
    .rodata : { *(.rodata*) *(.srodata*) . = ALIGN(4); }

    . = ALIGN(4);
    .data   : { *(.data*) }
    .bss    : { *(.bss*) }
}
