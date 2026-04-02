#pragma once
#include "../kernel/kernel.h"

// Exception frame pushed by vectors.S
struct ExceptionFrame {
    u64 regs[30];       // x0-x29
    u64 lr;             // x30
    u64 elr;            // saved PC
    u64 spsr;           // saved PSTATE
    u64 esr;            // exception syndrome
};

namespace Exception {

void init();

} // namespace Exception

// IRQ handler function type
using irq_handler_t = void (*)(u32 intid);

namespace IRQ {

void register_handler(u32 intid, irq_handler_t handler);

} // namespace IRQ
