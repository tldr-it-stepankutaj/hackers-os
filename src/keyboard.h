// keyboard.h
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "kernel.h"

namespace Keyboard {
    // PS/2 keyboard port
    constexpr u16 PORT = 0x60;

    // Initialize the keyboard
    void init();

    // Handle a keyboard interrupt
    void handle_interrupt();

    // Read a character (non-blocking)
    char read();
}

#endif // KEYBOARD_H