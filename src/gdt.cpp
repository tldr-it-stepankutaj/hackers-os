// gdt.cpp
#include "gdt.h"

namespace GDT {
    // GDT entries
    static GDTEntry gdt[5];
    static GDTPtr gp;

    // External assembly function to load the GDT
    extern "C" void gdt_flush(u32 gdt_ptr);

    // Set a GDT entry
    static void set_gate(int num, u32 base, u32 limit, u8 access, u8 gran) {
        gdt[num].base_low = (base & 0xFFFF);
        gdt[num].base_middle = (base >> 16) & 0xFF;
        gdt[num].base_high = (base >> 24) & 0xFF;

        gdt[num].limit_low = (limit & 0xFFFF);
        gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);

        gdt[num].access = access;
    }

    void init() {
        // Set up the GDT pointer
        gp.limit = (sizeof(GDTEntry) * 5) - 1;
        gp.base = (u32)&gdt;

        // NULL descriptor
        set_gate(0, 0, 0, 0, 0);

        // Code segment
        set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

        // Data segment
        set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

        // User mode code segment
        set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

        // User mode data segment
        set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

        // Flush the GDT
        gdt_flush((u32)&gp);
        VGA::write_string("GDT initialized\n");
    }
}

// Assembly code for gdt_flush (this would be in a separate .asm file in a real implementation)
// For completeness, here's what it would look like:
/*
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
*/