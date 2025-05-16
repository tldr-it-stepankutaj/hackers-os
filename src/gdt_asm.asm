; gdt_asm.asm
bits 32
global gdt_flush

gdt_flush:
    mov eax, [esp + 4]  ; Get the pointer to the GDT
    lgdt [eax]          ; Load the GDT

    mov ax, 0x10        ; 0x10 is the offset to our data segment in the GDT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush     ; 0x08 is the offset to our code segment, far jump to update CS
.flush:
    ret