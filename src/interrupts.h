// interrupts.h
#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "kernel.h"

namespace Interrupts {
    // PIC constants
    constexpr u8 PIC1 = 0x20;     // IO base address for master PIC
    constexpr u8 PIC2 = 0xA0;     // IO base address for slave PIC
    constexpr u8 PIC1_COMMAND = PIC1;
    constexpr u8 PIC1_DATA = (PIC1 + 1);
    constexpr u8 PIC2_COMMAND = PIC2;
    constexpr u8 PIC2_DATA = (PIC2 + 1);

    // PIC commands
    constexpr u8 ICW1_ICW4 = 0x01;      // ICW4 (not) needed
    constexpr u8 ICW1_SINGLE = 0x02;    // Single (cascade) mode
    constexpr u8 ICW1_INTERVAL4 = 0x04; // Call address interval 4 (8)
    constexpr u8 ICW1_LEVEL = 0x08;     // Level triggered (edge) mode
    constexpr u8 ICW1_INIT = 0x10;      // Initialization - required!

    constexpr u8 ICW4_8086 = 0x01;      // 8086/88 (MCS-80/85) mode
    constexpr u8 ICW4_AUTO = 0x02;      // Auto (normal) EOI
    constexpr u8 ICW4_BUF_SLAVE = 0x08; // Buffered mode/slave
    constexpr u8 ICW4_BUF_MASTER = 0x0C;// Buffered mode/master
    constexpr u8 ICW4_SFNM = 0x10;      // Special fully nested (not)

    // Initialize the PICs and interrupt system
    void init();

    // Send an EOI (End of Interrupt) to the PICs
    void send_eoi(u8 irq);

    // Enable/disable interrupts
    inline void enable() { asm volatile("sti"); }
    inline void disable() { asm volatile("cli"); }
}

#endif // INTERRUPTS_H