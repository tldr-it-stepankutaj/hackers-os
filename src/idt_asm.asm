; idt_asm.asm
bits 32
global idt_load

idt_load:
    mov eax, [esp + 4]  ; Get the pointer to the IDT
    lidt [eax]          ; Load the IDT
    ret