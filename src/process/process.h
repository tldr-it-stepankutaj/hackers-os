#pragma once
#include "../kernel/kernel.h"

static constexpr u32 MAX_PROCESSES = 64;
static constexpr u64 USER_STACK_SIZE = 4096 * 4;    // 16KB user stack
static constexpr u64 KERNEL_STACK_SIZE = 4096 * 4;  // 16KB kernel stack
static constexpr u64 USER_STACK_BASE = 0x7FFFFFF000ULL;  // Top of user space

enum class ProcessState : u8 {
    UNUSED = 0,
    READY,
    RUNNING,
    BLOCKED,
    ZOMBIE,
};

struct ProcessContext {
    u64 x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    u64 x29;  // frame pointer
    u64 x30;  // link register (return address)
    u64 sp;   // stack pointer
};

struct Process {
    u32 pid;
    ProcessState state;
    ProcessContext context;
    u64 kernel_stack;       // base of kernel stack allocation
    u64 kernel_stack_top;   // top of kernel stack (SP value)
    u64 page_table;         // TTBR0 value
    u64 user_entry;         // EL0 entry point
    u64 user_stack;         // EL0 stack page
    const char *name;
};

namespace Process_ {

void init();
Process *create(const char *name, void (*entry)());
Process *create_user(const char *name, u64 entry_point, u64 page_table);
void destroy(u32 pid);
Process *get_current();
Process *get(u32 pid);
u32 count();

} // namespace Process_
