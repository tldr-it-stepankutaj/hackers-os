; boot.asm
bits 32
section .multiboot
    align 4
    ; Multiboot header format
    dd 0x1BADB002               ; Magic number
    dd 0x00000003               ; Flags: align modules to 4KB boundaries, provide memory map
    dd -(0x1BADB002 + 0x00000003) ; Checksum

section .text
global _start
extern kernel_main

_start:
    ; Set up the stack
    mov esp, stack_top

    ; Pass Multiboot info to kernel
    push ebx

    ; Call the kernel
    call kernel_main

    ; If the kernel returns, just hang
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KiB
stack_top: