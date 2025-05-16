// memory.h
#ifndef MEMORY_H
#define MEMORY_H

#include "kernel.h"

namespace Memory {
    // Memory management functions
    void init();
    void* kmalloc(size_t size);
    void kfree(void* ptr);

    // Memory information
    size_t get_used_memory();
    size_t get_free_memory();
}

#endif // MEMORY_H