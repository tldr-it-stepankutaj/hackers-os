// memory.cpp
#include "memory.h"
#include "vga.h"

namespace Memory {
    // Very basic memory allocator
    // In a real OS, this would be much more sophisticated

    // Memory region structure
    struct MemoryRegion {
        bool used;
        size_t size;
        MemoryRegion* next;
    };

    // Start of heap
    static const size_t HEAP_START = 0x100000; // 1MB
    static const size_t HEAP_SIZE = 0x100000;  // 1MB

    // First memory region
    static MemoryRegion* first_region = nullptr;

    void init() {
        // Initialize the heap
        first_region = (MemoryRegion*)HEAP_START;
        first_region->used = false;
        first_region->size = HEAP_SIZE - sizeof(MemoryRegion);
        first_region->next = nullptr;

        VGA::write_string("Memory management initialized\n");
    }

    void* kmalloc(size_t size) {
        // Align size to 4 bytes
        size = (size + 3) & ~3;

        // Find a free region of sufficient size
        MemoryRegion* region = first_region;
        while (region) {
            if (!region->used && region->size >= size) {
                // Found a suitable region

                // Split the region if it's much larger than needed
                if (region->size > size + sizeof(MemoryRegion) + 4) {
                    MemoryRegion* new_region = (MemoryRegion*)((u8*)region + sizeof(MemoryRegion) + size);
                    new_region->used = false;
                    new_region->size = region->size - size - sizeof(MemoryRegion);
                    new_region->next = region->next;

                    region->size = size;
                    region->next = new_region;
                }

                region->used = true;
                return (void*)((u8*)region + sizeof(MemoryRegion));
            }

            region = region->next;
        }

        // Out of memory
        return nullptr;
    }

    void kfree(void* ptr) {
        if (!ptr) return;

        // Get the memory region header
        MemoryRegion* region = (MemoryRegion*)((u8*)ptr - sizeof(MemoryRegion));
        region->used = false;

        // Coalesce with next region if possible
        if (region->next && !region->next->used) {
            region->size += sizeof(MemoryRegion) + region->next->size;
            region->next = region->next->next;
        }

        // Coalesce with previous region if possible
        MemoryRegion* prev = first_region;
        while (prev && prev->next != region) {
            prev = prev->next;
        }

        if (prev && !prev->used) {
            prev->size += sizeof(MemoryRegion) + region->size;
            prev->next = region->next;
        }
    }

    size_t get_used_memory() {
        size_t used = 0;
        MemoryRegion* region = first_region;

        while (region) {
            if (region->used) {
                used += region->size;
            }
            region = region->next;
        }

        return used;
    }

    size_t get_free_memory() {
        size_t free = 0;
        MemoryRegion* region = first_region;

        while (region) {
            if (!region->used) {
                free += region->size;
            }
            region = region->next;
        }

        return free;
    }
}