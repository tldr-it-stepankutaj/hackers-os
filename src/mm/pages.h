#pragma once
#include "../kernel/kernel.h"

static constexpr u64 PAGE_SIZE = 4096;
static constexpr u64 PAGE_SHIFT = 12;

namespace Pages {

void init(u64 total_memory);
u64 alloc_page();
void free_page(u64 phys_addr);
u64 alloc_pages(u64 count);
u64 get_free_count();
u64 get_total_count();

} // namespace Pages
