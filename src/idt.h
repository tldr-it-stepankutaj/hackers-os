// idt.h
#ifndef IDT_H
#define IDT_H

#include "kernel.h"

namespace IDT {
    // IDT entry structure
    struct IDTEntry {
        u16 base_low;
        u16 selector;
        u8 always0;
        u8 flags;
        u16 base_high;
    } __attribute__((packed));

    // IDT pointer structure
    struct IDTPtr {
        u16 limit;
        u32 base;
    } __attribute__((packed));

    // Set an interrupt handler
    void set_gate(u8 num, u32 base, u16 selector, u8 flags);

    // Initialize the IDT
    void init();
}

#endif // IDT_H