#include "scheduler.h"
#include "process.h"
#include "../uart/uart.h"
#include "../mm/mmu.h"
#include "../arch/aarch64.h"

extern "C" void context_switch(ProcessContext *old_ctx, ProcessContext *new_ctx);
extern Process *process_table_ptr();
extern Process *&current_process_ref();

static u32 current_index = 0;

namespace Scheduler {

void init() {
    UART::puts("  [ok] Scheduler initialized (round-robin)\n");
}

void schedule() {
    Process *table = process_table_ptr();
    Process *&current = current_process_ref();

    if (!current) return;

    // Mark current as ready if it was running
    if (current->state == ProcessState::RUNNING) {
        current->state = ProcessState::READY;
    }

    // Find next ready process (round-robin)
    u32 start = current_index;
    u32 next = start;
    do {
        next = (next + 1) % MAX_PROCESSES;
        if (table[next].state == ProcessState::READY) {
            break;
        }
    } while (next != start);

    Process *next_proc = &table[next];

    // If no ready process found, stay with current or idle
    if (next_proc->state != ProcessState::READY) {
        if (current->state == ProcessState::READY) {
            current->state = ProcessState::RUNNING;
        }
        return;
    }

    // Switch
    Process *old = current;
    current = next_proc;
    current->state = ProcessState::RUNNING;
    current_index = next;

    // Switch page table if needed
    if (next_proc->page_table && next_proc->page_table != old->page_table) {
        MMU::switch_page_table(next_proc->page_table);
    }

    context_switch(&old->context, &next_proc->context);
}

void yield() {
    schedule();
}

} // namespace Scheduler

