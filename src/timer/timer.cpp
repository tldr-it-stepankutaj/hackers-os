#include "timer.h"
#include "../interrupts/gic.h"
#include "../interrupts/exception.h"
#include "../uart/uart.h"

// ARM Generic Timer: non-secure physical timer
// On QEMU virt, this is INTID 30 (PPI, 1-N model)
static constexpr u32 TIMER_IRQ = 30;
static constexpr u32 TIMER_HZ = 100;  // 100 ticks per second

static volatile u64 tick_count = 0;
static u64 timer_interval = 0;

static void timer_handler(u32 intid) {
    (void)intid;

    tick_count++;

    // Re-arm timer
    asm volatile("msr cntp_tval_el0, %0" :: "r"(timer_interval));
}

namespace Timer {

void init() {
    // Read timer frequency
    u64 freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));

    timer_interval = freq / TIMER_HZ;

    // Set timer value
    asm volatile("msr cntp_tval_el0, %0" :: "r"(timer_interval));

    // Enable timer (ENABLE=1, IMASK=0)
    u64 ctl = 1;
    asm volatile("msr cntp_ctl_el0, %0" :: "r"(ctl));

    // Register IRQ handler and enable in GIC
    IRQ::register_handler(TIMER_IRQ, timer_handler);
    GIC::set_priority(TIMER_IRQ, 0x80);
    GIC::enable_irq(TIMER_IRQ);

    UART::printf("  [ok] Timer: %u Hz (freq=%u)\n", (u64)TIMER_HZ, freq);
}

u64 get_ticks() {
    return tick_count;
}

u64 get_frequency() {
    u64 freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
}

u64 uptime_seconds() {
    return tick_count / TIMER_HZ;
}

} // namespace Timer
