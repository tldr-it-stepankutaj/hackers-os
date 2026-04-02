#include "gic.h"
#include "../arch/mmio.h"
#include "../uart/uart.h"

// GICv2 addresses on QEMU virt machine
static constexpr u64 GICD_BASE = 0x08000000;
static constexpr u64 GICC_BASE = 0x08010000;

// Distributor registers
static constexpr u64 GICD_CTLR       = GICD_BASE + 0x000;
static constexpr u64 GICD_ISENABLER  = GICD_BASE + 0x100;  // +n*4
static constexpr u64 GICD_ICENABLER  = GICD_BASE + 0x180;  // +n*4
static constexpr u64 GICD_IPRIORITYR = GICD_BASE + 0x400;  // +n*4
static constexpr u64 GICD_ITARGETSR  = GICD_BASE + 0x800;  // +n*4
static constexpr u64 GICD_ICFGR      = GICD_BASE + 0xC00;  // +n*4

// CPU interface registers
static constexpr u64 GICC_CTLR = GICC_BASE + 0x000;
static constexpr u64 GICC_PMR  = GICC_BASE + 0x004;
static constexpr u64 GICC_IAR  = GICC_BASE + 0x00C;
static constexpr u64 GICC_EOIR = GICC_BASE + 0x010;

namespace GIC {

void init() {
    // Disable distributor
    MMIO::write32(GICD_CTLR, 0);

    // Configure all SPIs: level-triggered, target CPU0, priority 0xa0
    for (u32 i = 32; i < 256; i += 16) {
        MMIO::write32(GICD_ICFGR + (i / 16) * 4, 0);
    }
    for (u32 i = 32; i < 256; i += 4) {
        MMIO::write32(GICD_IPRIORITYR + (i / 4) * 4, 0xa0a0a0a0);
        MMIO::write32(GICD_ITARGETSR + (i / 4) * 4, 0x01010101);
    }

    // Disable all SPIs initially
    for (u32 i = 32; i < 256; i += 32) {
        MMIO::write32(GICD_ICENABLER + (i / 32) * 4, 0xFFFFFFFF);
    }

    // Enable distributor
    MMIO::write32(GICD_CTLR, 1);

    // CPU interface: enable, priority mask allows all
    MMIO::write32(GICC_PMR, 0xFF);
    MMIO::write32(GICC_CTLR, 1);
}

void enable_irq(u32 intid) {
    u32 reg = intid / 32;
    u32 bit = intid % 32;
    MMIO::write32(GICD_ISENABLER + reg * 4, 1 << bit);
}

void disable_irq(u32 intid) {
    u32 reg = intid / 32;
    u32 bit = intid % 32;
    MMIO::write32(GICD_ICENABLER + reg * 4, 1 << bit);
}

u32 acknowledge() {
    return MMIO::read32(GICC_IAR);
}

void end_of_interrupt(u32 intid) {
    MMIO::write32(GICC_EOIR, intid);
}

void set_priority(u32 intid, u8 priority) {
    u32 reg_offset = (intid / 4) * 4;
    u32 byte_offset = intid % 4;
    u32 val = MMIO::read32(GICD_IPRIORITYR + reg_offset);
    val &= ~(0xFF << (byte_offset * 8));
    val |= static_cast<u32>(priority) << (byte_offset * 8);
    MMIO::write32(GICD_IPRIORITYR + reg_offset, val);
}

void set_target(u32 intid, u8 cpu_mask) {
    u32 reg_offset = (intid / 4) * 4;
    u32 byte_offset = intid % 4;
    u32 val = MMIO::read32(GICD_ITARGETSR + reg_offset);
    val &= ~(0xFF << (byte_offset * 8));
    val |= static_cast<u32>(cpu_mask) << (byte_offset * 8);
    MMIO::write32(GICD_ITARGETSR + reg_offset, val);
}

} // namespace GIC
