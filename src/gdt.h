// gdt.h
#ifndef GDT_H
#define GDT_H

#include "kernel.h"

namespace GDT {
    // GDT entry structure
    struct GDTEntry {
        u16 limit_low;
        u16 base_low;
        u8 base_middle;
        u8 access;
        u8 granularity;
        u8 base_high;
    } __attribute__((packed));

    // GDT pointer structure
    struct GDTPtr {
        u16 limit;
        u32 base;
    } __attribute__((packed));

    // Initialize the GDT
    void init();
}

#endif // GDT_H