// idt.cpp
#include "idt.h"
#include "vga.h"

namespace IDT {
    // IDT entries
    static IDTEntry idt[256];
    static IDTPtr idtp;

    // External assembly function to load the IDT
    extern "C" void idt_load(u32 idt_ptr);

    void set_gate(u8 num, u32 base, u16 selector, u8 flags) {
        idt[num].base_low = base & 0xFFFF;
        idt[num].base_high = (base >> 16) & 0xFFFF;

        idt[num].selector = selector;
        idt[num].always0 = 0;
        idt[num].flags = flags;
    }

    void init() {
        // Set up the IDT pointer
        idtp.limit = (sizeof(IDTEntry) * 256) - 1;
        idtp.base = (u32)&idt;

        // Clear out the IDT
        for (int i = 0; i < 256; i++) {
            set_gate(i, 0, 0, 0);
        }

        // Load the IDT
        idt_load((u32)&idtp);
        VGA::write_string("IDT initialized\n");
    }
}

// Assembly code for idt_load (this would be in a separate .asm file in a real implementation)
// For completeness, here's what it would look like:
/*
global idt_load
idt_load:
    mov eax, [esp + 4]  ; Get the pointer to the IDT
    lidt [eax]          ; Load the IDT
    ret
*/