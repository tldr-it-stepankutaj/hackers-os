// kernel.cpp
#include "kernel.h"
#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "interrupts.h"
#include "keyboard.h"
#include "memory.h"

// Implement kernel panic
[[noreturn]] void panic(const char* message) {
    VGA::clear(VGA::Color::RED, VGA::Color::BLACK);
    VGA::write_string("KERNEL PANIC: ");
    VGA::write_string(message);

    // Hang forever
    while (true) {
        asm volatile("cli; hlt");
    }
}

void init_all() {
    // Initialize all subsystems
    GDT::init();
    IDT::init();
    Interrupts::init();
    Keyboard::init();
    Memory::init();
}

extern "C" void kernel_main(u32 multiboot_magic) {
    // Initialize the VGA buffer first so we can display messages
    VGA::init();
    VGA::clear(VGA::Color::WHITE, VGA::Color::BLACK);
    VGA::write_string("Hello from C++ OS!\n");

    // Check multiboot magic number
    if (multiboot_magic != 0x2BADB002) {
        VGA::write_string("Invalid multiboot magic number!\n");
        return;
    }

    // Initialize subsystems
    VGA::write_string("Initializing subsystems...\n");
    init_all();

    VGA::write_string("System initialized successfully!\n");

    // Main kernel loop
    while (true) {
        asm volatile("hlt");
    }
}