#include "syscall.h"
#include "../interrupts/exception.h"
#include "../compat/linux_syscalls.h"
#include "../uart/uart.h"
#include "../process/process.h"
#include "../process/scheduler.h"
#include "../arch/aarch64.h"

// All syscalls use Linux AArch64 numbers for compatibility
// with statically-linked Linux binaries.

// Called from vectors.S for sync exceptions from lower EL (EL0)
extern "C" void handle_sync_lower(u64 type, ExceptionFrame *frame) {
    (void)type;

    u64 esr;
    asm volatile("mrs %0, esr_el1" : "=r"(esr));
    u32 ec = (esr >> 26) & 0x3F;

    if (ec == 0x15) {
        // SVC from AArch64
        // Linux ABI: syscall number in x8, args in x0-x5, return in x0
        u64 nr = frame->regs[8];
        i64 ret = LinuxSyscall::dispatch(nr,
                           frame->regs[0], frame->regs[1], frame->regs[2],
                           frame->regs[3], frame->regs[4], frame->regs[5]);
        frame->regs[0] = static_cast<u64>(ret);
    } else {
        // Not a syscall — treat as unhandled exception
        u64 esr_val, far_val;
        asm volatile("mrs %0, esr_el1" : "=r"(esr_val));
        asm volatile("mrs %0, far_el1" : "=r"(far_val));
        UART::printf("\n!!! Unexpected sync exception from EL0\n");
        UART::printf("  ESR: %x  FAR: %x\n", esr_val, far_val);
        Process *p = Process_::get_current();
        if (p) p->state = ProcessState::ZOMBIE;
        Scheduler::yield();
    }
}

namespace Syscall {

void init() {
    LinuxSyscall::init();
}

} // namespace Syscall
