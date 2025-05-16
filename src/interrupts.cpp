// interrupts.cpp
#include "interrupts.h"
#include "idt.h"
#include "vga.h"

namespace Interrupts {
    // ISR handlers (would be defined in assembly)
    extern "C" {
        void isr0();
        void isr1();
        // ... other ISRs ...
        void isr31();

        void irq0();
        void irq1();
        // ... other IRQs ...
        void irq15();
    }

    // Remap the PIC
    static void pic_remap() {
        // Save masks
        u8 a1 = inb(PIC1_DATA);
        u8 a2 = inb(PIC2_DATA);

        // Start the initialization sequence
        outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
        io_wait();
        outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
        io_wait();

        // Set vector offsets
        outb(PIC1_DATA, 0x20); // ICW2: Master PIC vector offset (32)
        io_wait();
        outb(PIC2_DATA, 0x28); // ICW2: Slave PIC vector offset (40)
        io_wait();

        // Tell Master PIC that there is a slave PIC at IRQ2
        outb(PIC1_DATA, 4); // ICW3: tell Master PIC that there is a slave PIC at IRQ2
        io_wait();
        outb(PIC2_DATA, 2); // ICW3: tell Slave PIC its cascade identity
        io_wait();

        // Set operation mode
        outb(PIC1_DATA, ICW4_8086);
        io_wait();
        outb(PIC2_DATA, ICW4_8086);
        io_wait();

        // Restore masks
        outb(PIC1_DATA, a1);
        outb(PIC2_DATA, a2);
    }

    void init() {
        // Remap the PICs
        pic_remap();

        // Install the ISRs
        IDT::set_gate(0, (u32)isr0, 0x08, 0x8E);
        IDT::set_gate(1, (u32)isr1, 0x08, 0x8E);
        // ... Install other ISRs ...
        IDT::set_gate(31, (u32)isr31, 0x08, 0x8E);

        // Install the IRQs
        IDT::set_gate(32, (u32)irq0, 0x08, 0x8E);
        IDT::set_gate(33, (u32)irq1, 0x08, 0x8E);
        // ... Install other IRQs ...
        IDT::set_gate(47, (u32)irq15, 0x08, 0x8E);

        // Enable interrupts
        enable();

        VGA::write_string("Interrupts initialized\n");
    }

    void send_eoi(u8 irq) {
        if (irq >= 8) {
            // If this was from the Slave PIC, we need to send an EOI to it too
            outb(PIC2_COMMAND, 0x20);
        }

        // Send EOI to Master PIC
        outb(PIC1_COMMAND, 0x20);
    }
}

// ISR handlers would be implemented in assembly code
// For example:
/*
global isr0
isr0:
    cli
    push byte 0    ; Push a dummy error code
    push byte 0    ; Push the interrupt number
    jmp isr_common_stub

; Common ISR handler stub
extern isr_handler
isr_common_stub:
    pusha         ; Pushes edi, esi, ebp, esp, ebx, edx, ecx, eax

    mov ax, ds
    push eax      ; Save the data segment descriptor

    mov ax, 0x10  ; Load the kernel data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call isr_handler

    pop eax       ; Reload the original data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa          ; Pops edi, esi, ebp, esp, ebx, edx, ecx, eax
    add esp, 8    ; Cleans up the pushed error code and ISR number
    sti
    iret          ; Pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP
*/