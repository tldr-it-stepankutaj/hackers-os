#include "mmu.h"
#include "pages.h"
#include "../uart/uart.h"
#include "../arch/aarch64.h"

/*
 * ARM64 4-level page tables (4KB granule, 48-bit VA):
 *   Level 0 (PGD): bits [47:39] — 512 entries
 *   Level 1 (PUD): bits [38:30] — 512 entries, 1GB blocks
 *   Level 2 (PMD): bits [29:21] — 512 entries, 2MB blocks
 *   Level 3 (PTE): bits [20:12] — 512 entries, 4KB pages
 *
 * We use 39-bit VA space (TCR_EL1.T0SZ = 25), so only levels 1-3.
 * Level 1 serves as PGD (512GB → 512 entries × 1GB).
 */

static constexpr u64 ENTRIES_PER_TABLE = 512;
static constexpr u64 TABLE_SIZE = ENTRIES_PER_TABLE * sizeof(u64);

// Kernel page table (Level 1 / PGD)
static u64 kernel_pgd[ENTRIES_PER_TABLE] __attribute__((aligned(4096)));

static u64 *get_or_create_table(u64 *table, u64 index) {
    if (!(table[index] & PTE_VALID)) {
        u64 new_table = Pages::alloc_page();
        if (!new_table) return nullptr;
        table[index] = new_table | PTE_VALID | PTE_TABLE;
    }
    return reinterpret_cast<u64*>(table[index] & 0x0000FFFFFFFFF000ULL);
}

static void map_page_in_table(u64 *pgd, u64 virt, u64 phys, u64 flags) {
    u64 l1_idx = (virt >> 30) & 0x1FF;
    u64 l2_idx = (virt >> 21) & 0x1FF;
    u64 l3_idx = (virt >> 12) & 0x1FF;

    u64 *l2_table = get_or_create_table(pgd, l1_idx);
    if (!l2_table) return;

    u64 *l3_table = get_or_create_table(l2_table, l2_idx);
    if (!l3_table) return;

    l3_table[l3_idx] = (phys & 0x0000FFFFFFFFF000ULL) | flags;
}

namespace MMU {

void init() {
    memset(kernel_pgd, 0, sizeof(kernel_pgd));

    // Map device memory: 0x00000000 - 0x3FFFFFFF (1GB) as device
    // Use 1GB block mapping at Level 1
    kernel_pgd[0] = 0x00000000ULL | PTE_VALID | (0ULL << 1) /* block */ |
                    PTE_AF | PTE_ATTR_DEVICE | PTE_AP_RW | PTE_UXN | PTE_PXN;

    // Map RAM: 0x40000000 - 0x4FFFFFFF (256MB) as normal memory
    // Use L2 and L3 tables for fine-grained mapping
    // For simplicity, map 256MB using 2MB block descriptors at L2
    u64 *l2_table = reinterpret_cast<u64*>(Pages::alloc_page());
    if (!l2_table) {
        UART::puts("  [FAIL] Cannot allocate L2 page table\n");
        return;
    }

    // 256MB = 128 × 2MB blocks
    for (u64 i = 0; i < 128; i++) {
        u64 phys = 0x40000000ULL + i * 0x200000ULL;
        l2_table[i] = phys | PTE_VALID | (0ULL << 1) /* block */ |
                      PTE_AF | PTE_SH_INNER | PTE_ATTR_NORMAL | PTE_AP_RW | PTE_UXN;
    }
    // Clear remaining entries
    for (u64 i = 128; i < 512; i++) {
        l2_table[i] = 0;
    }

    kernel_pgd[1] = reinterpret_cast<u64>(l2_table) | PTE_VALID | PTE_TABLE;

    // Set MAIR_EL1:
    //   Attr0 = 0xFF (Normal, Write-Back, Read/Write Allocate)
    //   Attr1 = 0x00 (Device-nGnRnE)
    u64 mair = 0x00FF;
    asm volatile("msr mair_el1, %0" :: "r"(mair));

    // Set TCR_EL1:
    //   T0SZ = 25 (39-bit VA, 512GB address space)
    //   IRGN0 = 01 (Write-Back, Write-Allocate)
    //   ORGN0 = 01 (Write-Back, Write-Allocate)
    //   SH0 = 11 (Inner Shareable)
    //   TG0 = 00 (4KB granule)
    //   IPS = 001 (36-bit PA, 64GB)
    u64 tcr = (25ULL << 0)  |  // T0SZ
              (1ULL << 8)   |  // IRGN0
              (1ULL << 10)  |  // ORGN0
              (3ULL << 12)  |  // SH0
              (0ULL << 14)  |  // TG0 = 4KB
              (1ULL << 32);    // IPS = 36-bit
    asm volatile("msr tcr_el1, %0" :: "r"(tcr));

    // Set TTBR0_EL1 to kernel page table
    u64 ttbr = reinterpret_cast<u64>(kernel_pgd);
    asm volatile("msr ttbr0_el1, %0" :: "r"(ttbr));

    // Invalidate TLB
    asm volatile("tlbi vmalle1");
    Arch::dsb();
    Arch::isb();

    // Enable MMU with caches
    u64 sctlr;
    asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1 << 0);   // M: MMU enable
    sctlr |= (1 << 2);   // C: Data cache enable
    sctlr |= (1 << 12);  // I: Instruction cache enable
    asm volatile("msr sctlr_el1, %0" :: "r"(sctlr));
    Arch::isb();

    UART::puts("  [ok] MMU enabled (identity mapped)\n");
}

void map_page(u64 virt, u64 phys, u64 flags) {
    map_page_in_table(kernel_pgd, virt, phys, flags);
    asm volatile("tlbi vaae1, %0" :: "r"(virt >> 12));
    Arch::dsb();
    Arch::isb();
}

void map_range(u64 virt_start, u64 phys_start, u64 size, u64 flags) {
    for (u64 off = 0; off < size; off += PAGE_SIZE) {
        map_page(virt_start + off, phys_start + off, flags);
    }
}

u64 create_user_page_table() {
    u64 pgd_phys = Pages::alloc_page();
    if (!pgd_phys) return 0;

    u64 *pgd = reinterpret_cast<u64*>(pgd_phys);

    // Copy kernel mappings (upper entries) — for simplicity, copy all L1 entries
    // so kernel is accessible in all address spaces
    for (u64 i = 0; i < ENTRIES_PER_TABLE; i++) {
        pgd[i] = kernel_pgd[i];
    }

    return pgd_phys;
}

void map_user_page(u64 pgd_phys, u64 virt, u64 phys, u64 flags) {
    u64 *pgd = reinterpret_cast<u64*>(pgd_phys);
    map_page_in_table(pgd, virt, phys, flags);
}

void switch_page_table(u64 pgd) {
    asm volatile("msr ttbr0_el1, %0" :: "r"(pgd));
    asm volatile("tlbi vmalle1");
    Arch::dsb();
    Arch::isb();
}

} // namespace MMU
