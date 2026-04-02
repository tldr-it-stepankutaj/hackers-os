#include "kernel.h"
#include "../uart/uart.h"
#include "../arch/aarch64.h"

extern "C" void kernel_panic(const char *msg) {
    Arch::disable_interrupts();
    UART::puts("\n!!! KERNEL PANIC !!!\n");
    UART::puts(msg);
    UART::puts("\nSystem halted.\n");
    Arch::halt();
}
