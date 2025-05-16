// vga.h
#ifndef VGA_H
#define VGA_H

#include "kernel.h"

namespace VGA {
    // VGA buffer constants
    constexpr u16* BUFFER = (u16*)0xB8000;
    constexpr size_t WIDTH = 80;
    constexpr size_t HEIGHT = 25;

    // VGA colors
    enum class Color : u8 {
        BLACK = 0,
        BLUE = 1,
        GREEN = 2,
        CYAN = 3,
        RED = 4,
        MAGENTA = 5,
        BROWN = 6,
        LIGHT_GRAY = 7,
        DARK_GRAY = 8,
        LIGHT_BLUE = 9,
        LIGHT_GREEN = 10,
        LIGHT_CYAN = 11,
        LIGHT_RED = 12,
        LIGHT_MAGENTA = 13,
        YELLOW = 14,
        WHITE = 15
    };

    // Create a VGA entry (character + color attributes)
    inline u16 entry(char c, Color fg, Color bg) {
        return (u16)c | ((u16)fg | ((u16)bg << 4)) << 8;
    }

    // VGA interface
    void init();
    void clear(Color fg = Color::WHITE, Color bg = Color::BLACK);
    void putchar(char c, Color fg = Color::WHITE, Color bg = Color::BLACK);
    void write_string(const char* str, Color fg = Color::WHITE, Color bg = Color::BLACK);
    void set_cursor(size_t x, size_t y);
}

#endif // VGA_H