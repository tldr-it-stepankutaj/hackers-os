// isr_handlers.cpp
#include "kernel.h"
#include "vga.h"
#include "interrupts.h"

// ISR messages
static const char* exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

// ISR handler structure
struct registers {
    u32 ds;
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
    u32 int_no, err_code; // Interrupt number and error code
    u32 eip, cs, eflags, useresp, ss; // Pushed by the processor automatically
};

// ISR handler
extern "C" void isr_handler(registers regs) {
    // Handle the exception
    VGA::write_string("Exception: ");
    VGA::write_string(exception_messages[regs.int_no]);
    VGA::write_string("\n");

    // For serious exceptions, we might want to halt the system
    if (regs.int_no == 8 || regs.int_no == 13 || regs.int_no == 14) {
        VGA::write_string("System halted\n");
        while (true) {
            asm volatile("cli; hlt");
        }
    }
}

// IRQ handler function pointers
static void (*irq_handlers[16])() = {0};

// Register an IRQ handler
void register_irq_handler(u8 irq, void (*handler)()) {
    irq_handlers[irq] = handler;
}

// IRQ handler
extern "C" void irq_handler(registers regs) {
    // Call the handler if it exists
    if (irq_handlers[regs.int_no - 32]) {
        irq_handlers[regs.int_no - 32]();
    }

    // Send EOI
    Interrupts::send_eoi(regs.int_no - 32);
}