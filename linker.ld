ENTRY(_start)

KERNEL_OFFSET = 0x40000000;

SECTIONS
{
    . = KERNEL_OFFSET;

    .text.boot : {
        KEEP(*(.text.boot))
    }

    .text : {
        *(.text .text.*)
    }

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4096) {
        *(.data .data.*)
    }

    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(4096);
    __kernel_end = .;

    /* 64KB kernel stack */
    . = . + 0x10000;
    __stack_top = .;

    /* Heap starts after stack */
    __heap_start = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
