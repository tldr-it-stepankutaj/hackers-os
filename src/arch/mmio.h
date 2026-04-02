#pragma once
#include "../kernel/kernel.h"

namespace MMIO {

static inline void write32(u64 addr, u32 value) {
    asm volatile("str %w[val], [%[reg]]"
        : : [reg] "r"(addr), [val] "r"(value)
        : "memory");
}

static inline u32 read32(u64 addr) {
    u32 value;
    asm volatile("ldr %w[val], [%[reg]]"
        : [val] "=r"(value)
        : [reg] "r"(addr)
        : "memory");
    return value;
}

static inline void write8(u64 addr, u8 value) {
    asm volatile("strb %w[val], [%[reg]]"
        : : [reg] "r"(addr), [val] "r"(value)
        : "memory");
}

static inline u8 read8(u64 addr) {
    u8 value;
    asm volatile("ldrb %w[val], [%[reg]]"
        : [val] "=r"(value)
        : [reg] "r"(addr)
        : "memory");
    return value;
}

static inline void write16(u64 addr, u16 value) {
    asm volatile("strh %w[val], [%[reg]]"
        : : [reg] "r"(addr), [val] "r"(value)
        : "memory");
}

static inline u16 read16(u64 addr) {
    u16 value;
    asm volatile("ldrh %w[val], [%[reg]]"
        : [val] "=r"(value)
        : [reg] "r"(addr)
        : "memory");
    return value;
}

} // namespace MMIO
