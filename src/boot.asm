; boot.asm - Základní multiboot entrypoint
bits 32
section .text
align 4
; Multiboot hlavička musí být na začátku .text sekce
multiboot_header:
    dd 0x1BADB002               ; Magic number
    dd 0x00000003               ; Flags: align modules to 4KB boundaries, provide memory map
    dd -(0x1BADB002 + 0x00000003) ; Checksum

global _start
extern kernel_main

_start:
    ; Nastavení zásobníku
    mov esp, stack_top

    ; Předání informací o Multibootu kernelu
    push ebx

    ; Volání kernelu
    call kernel_main

    ; Pokud se kernel vrátí, zastavíme systém
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KiB
stack_top: