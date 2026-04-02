#include "heap.h"
#include "pages.h"
#include "../uart/uart.h"
#include "../arch/aarch64.h"

static constexpr usize HEAP_SIZE = 1024 * 1024;  // 1MB heap
static constexpr usize HEAP_PAGES = HEAP_SIZE / PAGE_SIZE;
static constexpr usize MIN_BLOCK = 16;
static constexpr usize ALIGNMENT = 8;

struct Block {
    usize size;
    bool  used;
    Block *next;
    Block *prev;
};

static constexpr usize BLOCK_HEADER = sizeof(Block);

static Block *first_block = nullptr;

namespace Heap {

void init() {
    // Allocate heap from the page allocator to avoid conflicts with MMU tables
    u64 heap_base = Pages::alloc_pages(HEAP_PAGES);
    if (!heap_base) {
        UART::puts("  [FAIL] Cannot allocate heap pages\n");
        return;
    }

    first_block = reinterpret_cast<Block*>(heap_base);
    first_block->size = HEAP_SIZE - BLOCK_HEADER;
    first_block->used = false;
    first_block->next = nullptr;
    first_block->prev = nullptr;

    UART::printf("  [ok] Heap: %u KB at %x\n",
                 (u64)(HEAP_SIZE / 1024), heap_base);
}

void *kmalloc(usize size) {
    if (size == 0) return nullptr;

    // Align size
    size = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    if (size < MIN_BLOCK) size = MIN_BLOCK;

    Arch::disable_interrupts();

    Block *block = first_block;
    while (block) {
        if (!block->used && block->size >= size) {
            // Split if enough room for another block
            if (block->size >= size + BLOCK_HEADER + MIN_BLOCK) {
                Block *new_block = reinterpret_cast<Block*>(
                    reinterpret_cast<u8*>(block) + BLOCK_HEADER + size
                );
                new_block->size = block->size - size - BLOCK_HEADER;
                new_block->used = false;
                new_block->next = block->next;
                new_block->prev = block;
                if (block->next) block->next->prev = new_block;
                block->next = new_block;
                block->size = size;
            }
            block->used = true;
            Arch::enable_interrupts();
            return reinterpret_cast<void*>(
                reinterpret_cast<u8*>(block) + BLOCK_HEADER
            );
        }
        block = block->next;
    }

    Arch::enable_interrupts();
    return nullptr;  // Out of memory
}

void kfree(void *ptr) {
    if (!ptr) return;

    Arch::disable_interrupts();

    Block *block = reinterpret_cast<Block*>(
        reinterpret_cast<u8*>(ptr) - BLOCK_HEADER
    );
    block->used = false;

    // Coalesce with next block
    if (block->next && !block->next->used) {
        block->size += BLOCK_HEADER + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }

    // Coalesce with previous block
    if (block->prev && !block->prev->used) {
        block->prev->size += BLOCK_HEADER + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
    }

    Arch::enable_interrupts();
}

usize get_used() {
    usize used = 0;
    Block *b = first_block;
    while (b) {
        if (b->used) used += b->size;
        b = b->next;
    }
    return used;
}

usize get_free() {
    usize free = 0;
    Block *b = first_block;
    while (b) {
        if (!b->used) free += b->size;
        b = b->next;
    }
    return free;
}

} // namespace Heap
