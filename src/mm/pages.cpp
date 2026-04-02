#include "pages.h"
#include "../uart/uart.h"

extern "C" u64 __kernel_end;
extern "C" u64 __heap_start;

// Bitmap-based page allocator
// One bit per 4KB page, supports up to 256MB (65536 pages)
static constexpr u64 MAX_PAGES = 65536;
static constexpr u64 BITMAP_SIZE = MAX_PAGES / 8;

static u8 bitmap[BITMAP_SIZE];
static u64 total_pages = 0;
static u64 free_pages = 0;
static u64 memory_base = 0x40000000;  // QEMU virt RAM start

static inline void bitmap_set(u64 page) {
    bitmap[page / 8] |= (1 << (page % 8));
}

static inline void bitmap_clear(u64 page) {
    bitmap[page / 8] &= ~(1 << (page % 8));
}

static inline bool bitmap_test(u64 page) {
    return bitmap[page / 8] & (1 << (page % 8));
}

namespace Pages {

void init(u64 total_memory) {
    total_pages = total_memory / PAGE_SIZE;
    if (total_pages > MAX_PAGES) total_pages = MAX_PAGES;

    // Mark all pages as free
    memset(bitmap, 0, BITMAP_SIZE);
    free_pages = total_pages;

    // Mark kernel pages as used (from RAM start to heap_start)
    u64 kernel_end_page = (reinterpret_cast<u64>(&__heap_start) - memory_base + PAGE_SIZE - 1) / PAGE_SIZE;
    for (u64 i = 0; i < kernel_end_page && i < total_pages; i++) {
        bitmap_set(i);
        free_pages--;
    }

    UART::printf("  [ok] Pages: %u total, %u free, %u used\n",
                 total_pages, free_pages, total_pages - free_pages);
}

u64 alloc_page() {
    for (u64 i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages--;
            u64 addr = memory_base + i * PAGE_SIZE;
            // Zero the page
            memset(reinterpret_cast<void*>(addr), 0, PAGE_SIZE);
            return addr;
        }
    }
    return 0;  // Out of memory
}

void free_page(u64 phys_addr) {
    if (phys_addr < memory_base) return;
    u64 page = (phys_addr - memory_base) / PAGE_SIZE;
    if (page < total_pages && bitmap_test(page)) {
        bitmap_clear(page);
        free_pages++;
    }
}

u64 alloc_pages(u64 count) {
    // Find contiguous free pages
    u64 run = 0;
    u64 start = 0;
    for (u64 i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (run == 0) start = i;
            run++;
            if (run == count) {
                for (u64 j = start; j < start + count; j++) {
                    bitmap_set(j);
                    free_pages--;
                }
                u64 addr = memory_base + start * PAGE_SIZE;
                memset(reinterpret_cast<void*>(addr), 0, count * PAGE_SIZE);
                return addr;
            }
        } else {
            run = 0;
        }
    }
    return 0;
}

u64 get_free_count() { return free_pages; }
u64 get_total_count() { return total_pages; }

} // namespace Pages
