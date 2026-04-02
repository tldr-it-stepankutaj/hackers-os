#include "process.h"
#include "../mm/pages.h"
#include "../mm/mmu.h"
#include "../mm/heap.h"
#include "../uart/uart.h"

static Process process_table[MAX_PROCESSES];
static u32 next_pid = 1;
static Process *current_process = nullptr;

// Defined in context.S
extern "C" void context_switch(ProcessContext *old_ctx, ProcessContext *new_ctx);

// Wrapper: kernel thread starts here
static void kernel_thread_wrapper() {
    // The actual entry function is stored in x19 by create()
    void (*entry)();
    asm volatile("mov %0, x19" : "=r"(entry));
    entry();
    // If thread returns, mark as zombie
    if (current_process) {
        current_process->state = ProcessState::ZOMBIE;
    }
    // Yield to scheduler
    for (;;) asm volatile("wfi");
}

namespace Process_ {

void init() {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].state = ProcessState::UNUSED;
        process_table[i].pid = 0;
    }

    // Create process 0 (kernel/idle) — represents the boot context
    process_table[0].pid = 0;
    process_table[0].state = ProcessState::RUNNING;
    process_table[0].name = "idle";
    process_table[0].kernel_stack = 0;  // uses boot stack
    current_process = &process_table[0];

    UART::puts("  [ok] Process subsystem initialized\n");
}

Process *create(const char *name, void (*entry)()) {
    // Find free slot
    Process *proc = nullptr;
    for (u32 i = 1; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == ProcessState::UNUSED) {
            proc = &process_table[i];
            break;
        }
    }
    if (!proc) return nullptr;

    // Allocate kernel stack
    u64 kstack = Pages::alloc_pages(KERNEL_STACK_SIZE / PAGE_SIZE);
    if (!kstack) return nullptr;

    proc->pid = next_pid++;
    proc->state = ProcessState::READY;
    proc->name = name;
    proc->kernel_stack = kstack;
    proc->kernel_stack_top = kstack + KERNEL_STACK_SIZE;
    proc->page_table = 0;  // uses kernel page table
    proc->user_entry = 0;
    proc->user_stack = 0;

    // Set up context so context_switch lands in kernel_thread_wrapper
    memset(&proc->context, 0, sizeof(ProcessContext));
    proc->context.x30 = reinterpret_cast<u64>(kernel_thread_wrapper);
    proc->context.sp = proc->kernel_stack_top;
    proc->context.x19 = reinterpret_cast<u64>(entry);

    return proc;
}

Process *create_user(const char *name, u64 entry_point, u64 page_table) {
    // Find free slot
    Process *proc = nullptr;
    for (u32 i = 1; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == ProcessState::UNUSED) {
            proc = &process_table[i];
            break;
        }
    }
    if (!proc) return nullptr;

    // Allocate kernel stack
    u64 kstack = Pages::alloc_pages(KERNEL_STACK_SIZE / PAGE_SIZE);
    if (!kstack) return nullptr;

    // Allocate user stack pages and map them
    u64 ustack_phys = Pages::alloc_pages(USER_STACK_SIZE / PAGE_SIZE);
    if (!ustack_phys) {
        Pages::free_page(kstack);
        return nullptr;
    }

    // Map user stack in user page table
    u64 ustack_virt = USER_STACK_BASE - USER_STACK_SIZE;
    for (u64 off = 0; off < USER_STACK_SIZE; off += PAGE_SIZE) {
        MMU::map_user_page(page_table, ustack_virt + off,
                           ustack_phys + off, PTE_USER_RWX);
    }

    proc->pid = next_pid++;
    proc->state = ProcessState::READY;
    proc->name = name;
    proc->kernel_stack = kstack;
    proc->kernel_stack_top = kstack + KERNEL_STACK_SIZE;
    proc->page_table = page_table;
    proc->user_entry = entry_point;
    proc->user_stack = USER_STACK_BASE;

    // Context: return to user mode via eret
    // The scheduler will set up SPSR/ELR for EL0 entry
    memset(&proc->context, 0, sizeof(ProcessContext));
    proc->context.sp = proc->kernel_stack_top;

    return proc;
}

void destroy(u32 pid) {
    for (u32 i = 1; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid) {
            if (process_table[i].kernel_stack) {
                // Free stack pages
                for (u64 off = 0; off < KERNEL_STACK_SIZE; off += PAGE_SIZE) {
                    Pages::free_page(process_table[i].kernel_stack + off);
                }
            }
            process_table[i].state = ProcessState::UNUSED;
            process_table[i].pid = 0;
            return;
        }
    }
}

Process *get_current() { return current_process; }

Process *get(u32 pid) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid &&
            process_table[i].state != ProcessState::UNUSED) {
            return &process_table[i];
        }
    }
    return nullptr;
}

u32 count() {
    u32 n = 0;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != ProcessState::UNUSED) n++;
    }
    return n;
}

} // namespace Process_

// Used by scheduler
Process *process_table_ptr() { return process_table; }
Process *&current_process_ref() { return current_process; }
