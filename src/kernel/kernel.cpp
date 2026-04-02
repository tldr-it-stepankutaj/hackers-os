#include "kernel.h"
#include "../uart/uart.h"
#include "../logo/logo.h"
#include "../interrupts/exception.h"
#include "../interrupts/gic.h"
#include "../timer/timer.h"
#include "../mm/pages.h"
#include "../mm/mmu.h"
#include "../mm/heap.h"
#include "../process/process.h"
#include "../process/scheduler.h"
#include "../syscall/syscall.h"
#include "../drivers/virtio_blk.h"
#include "../fs/fat32.h"
#include "../shell/shell.h"
#include "../net/net.h"
#include "../net/socket.h"
#include "../arch/aarch64.h"

// RAM size for QEMU virt (256 MB)
static constexpr u64 RAM_SIZE = 256 * 1024 * 1024;

static void init_subsystems() {
    UART::puts("Initializing subsystems...\n\n");

    // Phase 3: Exception handling + GIC
    GIC::init();
    UART::puts("  [ok] GIC initialized\n");
    Exception::init();

    // Phase 5: Memory management
    Pages::init(RAM_SIZE);
    MMU::init();
    Heap::init();

    // Phase 6: Process management
    Process_::init();
    Scheduler::init();

    // Phase 7: Syscalls
    Syscall::init();

    // Phase 4: Timer
    Timer::init();

    // Enable interrupts
    Arch::enable_interrupts();
    UART::puts("  [ok] Interrupts enabled\n");

    // Phase 8-9: Storage (optional — only if disk is present)
    if (VirtioBlk::init()) {
        FAT32::init();
    }

    // Networking (if NIC is present)
    Net::init();
    Socket::init();
    UART::puts("  [ok] Network + Socket layer ready\n");

    // Shell
    Shell::init();

    UART::puts("\nAll subsystems initialized.\n");
}

extern "C" void kernel_main() {
    // Phase 1: UART is our primary I/O
    UART::init();

    // Phase 2: Boot logo
    Logo::display();

    // Initialize everything
    init_subsystems();

    // Enter interactive shell
    Shell::run();

    // Should never reach here
    UART::puts("Shell exited. Halting.\n");
    Arch::halt();
}
