// keyboard.cpp
#include "keyboard.h"
#include "vga.h"
#include "interrupts.h"

namespace Keyboard {
    // US keyboard layout
    static const char kbd_us[128] = {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    // Current key buffer
    static char buffer[256];
    static int buffer_pos = 0;

    // External IRQ handler function
    extern "C" void irq1_handler() {
        handle_interrupt();
        Interrupts::send_eoi(1);
    }

    void init() {
        // The keyboard is already initialized by the BIOS
        // We just need to set up the IRQ handler
        VGA::write_string("Keyboard initialized\n");
    }

    void handle_interrupt() {
        u8 scancode = inb(PORT);

        // Check if key was released (bit 7 set)
        if (scancode & 0x80) {
            // Key release, we don't care for now
        } else {
            // Key press
            char c = kbd_us[scancode];
            if (c) {
                // Add character to buffer if it's printable
                if (buffer_pos < 255) {
                    buffer[buffer_pos++] = c;
                    buffer[buffer_pos] = 0; // Null terminate
                }

                // Echo key to screen
                VGA::putchar(c);
            }
        }
    }

    char read() {
        if (buffer_pos == 0) {
            return 0; // No characters to read
        }

        // Pop the first character from the buffer
        char c = buffer[0];
        for (int i = 0; i < buffer_pos - 1; i++) {
            buffer[i] = buffer[i + 1];
        }
        buffer_pos--;

        return c;
    }
}