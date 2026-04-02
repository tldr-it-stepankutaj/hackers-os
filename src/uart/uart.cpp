#include "uart.h"
#include "../arch/mmio.h"

// PL011 UART base address on QEMU virt machine
static constexpr u64 UART_BASE = 0x09000000;

// PL011 register offsets
static constexpr u64 UART_DR   = UART_BASE + 0x000;  // Data Register
static constexpr u64 UART_FR   = UART_BASE + 0x018;  // Flag Register
static constexpr u64 UART_IBRD = UART_BASE + 0x024;  // Integer Baud Rate
static constexpr u64 UART_FBRD = UART_BASE + 0x028;  // Fractional Baud Rate
static constexpr u64 UART_LCRH = UART_BASE + 0x02C;  // Line Control
static constexpr u64 UART_CR   = UART_BASE + 0x030;  // Control Register
static constexpr u64 UART_IMSC = UART_BASE + 0x038;  // Interrupt Mask
static constexpr u64 UART_ICR  = UART_BASE + 0x044;  // Interrupt Clear

// Flag register bits
static constexpr u32 FR_TXFF = (1 << 5);  // TX FIFO full
static constexpr u32 FR_RXFE = (1 << 4);  // RX FIFO empty

namespace UART {

void init() {
    // Disable UART
    MMIO::write32(UART_CR, 0);

    // Clear pending interrupts
    MMIO::write32(UART_ICR, 0x7FF);

    // Set baud rate: 115200 @ 24MHz clock
    // Divisor = 24000000 / (16 * 115200) = 13.0208
    MMIO::write32(UART_IBRD, 13);
    MMIO::write32(UART_FBRD, 1);

    // 8 bits, FIFO enabled, no parity
    MMIO::write32(UART_LCRH, (1 << 4) | (3 << 5));  // FEN | WLEN 8bit

    // Enable RX interrupt
    MMIO::write32(UART_IMSC, (1 << 4));  // RXIM

    // Enable UART, TX, RX
    MMIO::write32(UART_CR, (1 << 0) | (1 << 8) | (1 << 9));
}

void putc(char c) {
    // Wait until TX FIFO is not full
    while (MMIO::read32(UART_FR) & FR_TXFF) {}
    MMIO::write32(UART_DR, static_cast<u32>(c));
}

char getc() {
    // Wait until RX FIFO is not empty
    while (MMIO::read32(UART_FR) & FR_RXFE) {}
    return static_cast<char>(MMIO::read32(UART_DR) & 0xFF);
}

void puts(const char *s) {
    while (*s) {
        if (*s == '\n') putc('\r');
        putc(*s++);
    }
}

static const char hex_chars[] = "0123456789abcdef";

void puthex(u64 value) {
    puts("0x");
    bool leading = true;
    for (int i = 60; i >= 0; i -= 4) {
        u8 nibble = (value >> i) & 0xF;
        if (nibble == 0 && leading && i > 0) continue;
        leading = false;
        putc(hex_chars[nibble]);
    }
    if (leading) putc('0');
}

void putdec(i64 value) {
    if (value < 0) {
        putc('-');
        value = -value;
    }
    if (value == 0) {
        putc('0');
        return;
    }
    char buf[20];
    int i = 0;
    u64 v = static_cast<u64>(value);
    while (v > 0) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    while (i > 0) putc(buf[--i]);
}

static void putuint(u64 value) {
    if (value == 0) {
        putc('0');
        return;
    }
    char buf[20];
    int i = 0;
    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }
    while (i > 0) putc(buf[--i]);
}

void printf(const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': {
                    const char *s = __builtin_va_arg(args, const char*);
                    puts(s ? s : "(null)");
                    break;
                }
                case 'd': {
                    i64 v = __builtin_va_arg(args, i64);
                    putdec(v);
                    break;
                }
                case 'u': {
                    u64 v = __builtin_va_arg(args, u64);
                    putuint(v);
                    break;
                }
                case 'x':
                case 'p': {
                    u64 v = __builtin_va_arg(args, u64);
                    puthex(v);
                    break;
                }
                case 'c': {
                    int c = __builtin_va_arg(args, int);
                    putc(static_cast<char>(c));
                    break;
                }
                case '%':
                    putc('%');
                    break;
                default:
                    putc('%');
                    putc(*fmt);
                    break;
            }
        } else {
            if (*fmt == '\n') putc('\r');
            putc(*fmt);
        }
        fmt++;
    }

    __builtin_va_end(args);
}

} // namespace UART
