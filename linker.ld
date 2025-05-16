OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)
PHDRS {
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}
SECTIONS {
    . = 1M;
    .text : {
        *(.text._start)
        *(.text*)
    } :text
    .rodata : {
        *(.rodata*)
    } :rodata
    .data : {
        *(.data*)
        *(.bss*)
    } :data
    /DISCARD/ : { *(.eh_frame*) *(.comment*) }
}