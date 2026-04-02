#pragma once
#include "../kernel/kernel.h"

namespace Arch {

static inline void wfi() {
    asm volatile("wfi");
}

static inline void wfe() {
    asm volatile("wfe");
}

static inline void dsb() {
    asm volatile("dsb sy" ::: "memory");
}

static inline void dmb() {
    asm volatile("dmb sy" ::: "memory");
}

static inline void isb() {
    asm volatile("isb" ::: "memory");
}

static inline void enable_interrupts() {
    asm volatile("msr daifclr, #2");
}

static inline void disable_interrupts() {
    asm volatile("msr daifset, #2");
}

static inline u64 read_current_el() {
    u64 el;
    asm volatile("mrs %0, CurrentEL" : "=r"(el));
    return (el >> 2) & 3;
}

static inline u64 read_mpidr() {
    u64 val;
    asm volatile("mrs %0, mpidr_el1" : "=r"(val));
    return val;
}

static inline void halt() {
    for (;;) wfi();
}

} // namespace Arch
