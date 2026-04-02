#pragma once
#include "../kernel/kernel.h"

namespace Heap {

void init();
void *kmalloc(usize size);
void kfree(void *ptr);
usize get_used();
usize get_free();

} // namespace Heap
