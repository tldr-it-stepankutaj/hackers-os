#include "exception.h"
#include "gic.h"
#include "../uart/uart.h"
#include "../arch/aarch64.h"

extern "C" void exception_vector_table();

static const char *exception_type_names[] = {
    "Sync (SP_EL0)",   "IRQ (SP_EL0)",   "FIQ (SP_EL0)",   "SError (SP_EL0)",
    "Sync (ELx)",      "IRQ (ELx)",      "FIQ (ELx)",      "SError (ELx)",
    "Sync (Lower64)",  "IRQ (Lower64)",  "FIQ (Lower64)",  "SError (Lower64)",
    "Sync (Lower32)",  "IRQ (Lower32)",  "FIQ (Lower32)",  "SError (Lower32)",
};

// ESR_EL1 exception class descriptions
static const char *esr_class_name(u32 ec) {
    switch (ec) {
        case 0x00: return "Unknown reason";
        case 0x01: return "WFI/WFE trapped";
        case 0x15: return "SVC (AArch64)";
        case 0x18: return "MSR/MRS trap";
        case 0x20: return "Instruction abort (lower EL)";
        case 0x21: return "Instruction abort (same EL)";
        case 0x22: return "PC alignment fault";
        case 0x24: return "Data abort (lower EL)";
        case 0x25: return "Data abort (same EL)";
        case 0x26: return "SP alignment fault";
        case 0x2C: return "FP exception";
        case 0x30: return "Breakpoint (lower EL)";
        case 0x31: return "Breakpoint (same EL)";
        case 0x32: return "Software step (lower EL)";
        case 0x33: return "Software step (same EL)";
        case 0x34: return "Watchpoint (lower EL)";
        case 0x35: return "Watchpoint (same EL)";
        case 0x3C: return "BRK instruction";
        default:   return "Other";
    }
}

// IRQ handler table (up to 256 interrupt IDs)
static constexpr u32 MAX_IRQ = 256;
static irq_handler_t irq_handlers[MAX_IRQ] = {};

namespace IRQ {

void register_handler(u32 intid, irq_handler_t handler) {
    if (intid < MAX_IRQ) {
        irq_handlers[intid] = handler;
    }
}

} // namespace IRQ

extern "C" void handle_exception(u64 type, ExceptionFrame *frame) {
    (void)frame;

    u64 esr = 0, far = 0, elr = 0, sp = 0;
    asm volatile("mrs %0, esr_el1"  : "=r"(esr));
    asm volatile("mrs %0, far_el1"  : "=r"(far));
    asm volatile("mrs %0, elr_el1"  : "=r"(elr));
    asm volatile("mov %0, sp"       : "=r"(sp));

    u32 ec = (esr >> 26) & 0x3F;

    UART::puts("\n!!! EXCEPTION !!!\n");
    UART::puts("  Type: ");
    if (type < 16) UART::puts(exception_type_names[type]);
    UART::puts("\n  ESR:  ");
    UART::puthex(esr);
    UART::puts(" (");
    UART::puts(esr_class_name(ec));
    UART::puts(")\n  FAR:  ");
    UART::puthex(far);
    UART::puts("\n  ELR:  ");
    UART::puthex(elr);
    UART::puts("\n  SP:   ");
    UART::puthex(sp);
    UART::puts("\n");

    UART::puts("System halted.\n");
    Arch::halt();
}

extern "C" void handle_irq(u64 type, ExceptionFrame *frame) {
    (void)type;
    (void)frame;

    u32 intid = GIC::acknowledge();

    if (intid < MAX_IRQ && irq_handlers[intid]) {
        irq_handlers[intid](intid);
    } else if (intid < 1020) {
        UART::printf("Unhandled IRQ: %u\n", (u64)intid);
    }
    // intid >= 1020 is spurious

    if (intid < 1020) {
        GIC::end_of_interrupt(intid);
    }
}

extern "C" void handle_sync_lower(u64 type, ExceptionFrame *frame);

namespace Exception {

void init() {
    u64 vbar = reinterpret_cast<u64>(&exception_vector_table);
    asm volatile("msr vbar_el1, %0" :: "r"(vbar));
    asm volatile("isb");
    UART::puts("  [ok] Exception vectors installed\n");
}

} // namespace Exception
