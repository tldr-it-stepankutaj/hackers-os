#pragma once
#include "../kernel/kernel.h"

// Page table entry flags
static constexpr u64 PTE_VALID   = (1ULL << 0);
static constexpr u64 PTE_TABLE   = (1ULL << 1);  // table descriptor (levels 0-2)
static constexpr u64 PTE_PAGE    = (1ULL << 1);   // page descriptor (level 3)
static constexpr u64 PTE_AF      = (1ULL << 10);  // Access Flag
static constexpr u64 PTE_SH_INNER = (3ULL << 8);  // Inner Shareable
static constexpr u64 PTE_AP_RW   = (0ULL << 6);   // EL1 R/W
static constexpr u64 PTE_AP_RO   = (2ULL << 6);   // EL1 R/O
static constexpr u64 PTE_AP_USER = (1ULL << 6);   // EL0 access
static constexpr u64 PTE_UXN     = (1ULL << 54);  // Unprivileged Execute Never
static constexpr u64 PTE_PXN     = (1ULL << 53);  // Privileged Execute Never

// MAIR indices
static constexpr u64 PTE_ATTR_NORMAL = (0ULL << 2);  // MAIR index 0
static constexpr u64 PTE_ATTR_DEVICE = (1ULL << 2);  // MAIR index 1

// Convenience flags
static constexpr u64 PTE_KERNEL_RWX = PTE_VALID | PTE_PAGE | PTE_AF | PTE_SH_INNER | PTE_ATTR_NORMAL | PTE_AP_RW | PTE_UXN;
static constexpr u64 PTE_KERNEL_RW  = PTE_VALID | PTE_PAGE | PTE_AF | PTE_SH_INNER | PTE_ATTR_NORMAL | PTE_AP_RW | PTE_UXN | PTE_PXN;
static constexpr u64 PTE_KERNEL_RO  = PTE_VALID | PTE_PAGE | PTE_AF | PTE_SH_INNER | PTE_ATTR_NORMAL | PTE_AP_RO | PTE_UXN | PTE_PXN;
static constexpr u64 PTE_DEVICE_RW  = PTE_VALID | PTE_PAGE | PTE_AF | PTE_ATTR_DEVICE | PTE_AP_RW | PTE_UXN | PTE_PXN;
static constexpr u64 PTE_USER_RWX   = PTE_VALID | PTE_PAGE | PTE_AF | PTE_SH_INNER | PTE_ATTR_NORMAL | PTE_AP_RW | PTE_AP_USER;
static constexpr u64 PTE_USER_RO    = PTE_VALID | PTE_PAGE | PTE_AF | PTE_SH_INNER | PTE_ATTR_NORMAL | PTE_AP_RO | PTE_AP_USER | PTE_PXN;

namespace MMU {

void init();
void map_page(u64 virt, u64 phys, u64 flags);
void map_range(u64 virt_start, u64 phys_start, u64 size, u64 flags);
u64 create_user_page_table();
void map_user_page(u64 pgd, u64 virt, u64 phys, u64 flags);
void switch_page_table(u64 pgd);

} // namespace MMU
