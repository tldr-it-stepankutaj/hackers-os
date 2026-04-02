#include "syscall.h"
#include "../interrupts/exception.h"
#include "../compat/linux_syscalls.h"
#include "../uart/uart.h"
#include "../process/process.h"
#include "../process/scheduler.h"
#include "../arch/aarch64.h"

// All syscalls use Linux AArch64 numbers for compatibility
// with statically-linked Linux binaries.

// Current exception frame — accessible to syscall handlers for exit
static ExceptionFrame *current_syscall_frame = nullptr;

ExceptionFrame *get_current_syscall_frame() {
    return current_syscall_frame;
}

// From enter_user.S
extern "C" u64 kernel_return_addr;
extern "C" u64 kernel_return_sp;

// Called from vectors.S for sync exceptions from lower EL (EL0)
extern "C" void handle_sync_lower(u64 type, ExceptionFrame *frame) {
    (void)type;

    u64 esr;
    asm volatile("mrs %0, esr_el1" : "=r"(esr));
    u32 ec = (esr >> 26) & 0x3F;

    if (ec == 0x15) {
        // SVC from AArch64
        current_syscall_frame = frame;
        u64 nr = frame->regs[8];
        i64 ret = LinuxSyscall::dispatch(nr,
                           frame->regs[0], frame->regs[1], frame->regs[2],
                           frame->regs[3], frame->regs[4], frame->regs[5]);
        frame->regs[0] = static_cast<u64>(ret);
        current_syscall_frame = nullptr;
    } else {
        // Not a syscall — crash the user process, return to kernel
        u64 esr_val, far_val;
        asm volatile("mrs %0, esr_el1" : "=r"(esr_val));
        asm volatile("mrs %0, far_el1" : "=r"(far_val));
        UART::printf("\n!!! User process exception: ESR=%x FAR=%x\n", esr_val, far_val);
        Process *p = Process_::get_current();
        if (p) p->state = ProcessState::ZOMBIE;

        // Return to kernel instead of back to EL0
        if (kernel_return_addr) {
            frame->elr = kernel_return_addr;
            frame->spsr = 0x3c5;  // EL1h, DAIF masked
        }
    }
}

namespace Syscall {

void init() {
    LinuxSyscall::init();
}

} // namespace Syscall
