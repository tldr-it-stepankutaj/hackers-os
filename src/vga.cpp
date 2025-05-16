// vga.cpp
#include "vga.h"

namespace VGA {
    // VGA state
    static size_t cursor_x = 0;
    static size_t cursor_y = 0;
    static Color current_fg = Color::WHITE;
    static Color current_bg = Color::BLACK;

    void init() {
        cursor_x = 0;
        cursor_y = 0;
        current_fg = Color::WHITE;
        current_bg = Color::BLACK;
    }

    void clear(Color fg, Color bg) {
        current_fg = fg;
        current_bg = bg;

        for (size_t y = 0; y < HEIGHT; y++) {
            for (size_t x = 0; x < WIDTH; x++) {
                BUFFER[y * WIDTH + x] = entry(' ', fg, bg);
            }
        }

        cursor_x = 0;
        cursor_y = 0;
        set_cursor(cursor_x, cursor_y);
    }

    void scroll() {
        // Move all rows up by one
        for (size_t y = 1; y < HEIGHT; y++) {
            for (size_t x = 0; x < WIDTH; x++) {
                BUFFER[(y - 1) * WIDTH + x] = BUFFER[y * WIDTH + x];
            }
        }

        // Clear the bottom row
        for (size_t x = 0; x < WIDTH; x++) {
            BUFFER[(HEIGHT - 1) * WIDTH + x] = entry(' ', current_fg, current_bg);
        }
    }

    void putchar(char c, Color fg, Color bg) {
        // Handle special characters
        if (c == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else if (c == '\r') {
            cursor_x = 0;
        } else if (c == '\t') {
            cursor_x = (cursor_x + 8) & ~(8 - 1);
        } else if (c == '\b') {
            if (cursor_x > 0) cursor_x--;
        } else {
            // Regular character
            BUFFER[cursor_y * WIDTH + cursor_x] = entry(c, fg, bg);
            cursor_x++;
        }

        // Handle wrapping
        if (cursor_x >= WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }

        // Handle scrolling
        if (cursor_y >= HEIGHT) {
            scroll();
            cursor_y = HEIGHT - 1;
        }

        // Update the hardware cursor
        set_cursor(cursor_x, cursor_y);
    }

    void write_string(const char* str, Color fg, Color bg) {
        for (size_t i = 0; str[i] != '\0'; i++) {
            putchar(str[i], fg, bg);
        }
    }

    void set_cursor(size_t x, size_t y) {
        const u16 pos = y * WIDTH + x;

        outb(0x3D4, 0x0F);
        outb(0x3D5, (u8)(pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (u8)((pos >> 8) & 0xFF));

        cursor_x = x;
        cursor_y = y;
    }
}