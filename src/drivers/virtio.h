#pragma once
#include "../kernel/kernel.h"

// Virtio MMIO register offsets
static constexpr u64 VIRTIO_MMIO_MAGIC         = 0x000;
static constexpr u64 VIRTIO_MMIO_VERSION       = 0x004;
static constexpr u64 VIRTIO_MMIO_DEVICE_ID     = 0x008;
static constexpr u64 VIRTIO_MMIO_VENDOR_ID     = 0x00C;
static constexpr u64 VIRTIO_MMIO_HOST_FEATURES = 0x010;
static constexpr u64 VIRTIO_MMIO_GUEST_FEATURES= 0x020;
static constexpr u64 VIRTIO_MMIO_QUEUE_SEL     = 0x030;
static constexpr u64 VIRTIO_MMIO_QUEUE_NUM_MAX = 0x034;
static constexpr u64 VIRTIO_MMIO_QUEUE_NUM     = 0x038;
static constexpr u64 VIRTIO_MMIO_QUEUE_READY   = 0x044;
static constexpr u64 VIRTIO_MMIO_QUEUE_NOTIFY  = 0x050;
static constexpr u64 VIRTIO_MMIO_INT_STATUS    = 0x060;
static constexpr u64 VIRTIO_MMIO_INT_ACK       = 0x064;
static constexpr u64 VIRTIO_MMIO_STATUS        = 0x070;
static constexpr u64 VIRTIO_MMIO_QUEUE_DESC_LOW  = 0x080;
static constexpr u64 VIRTIO_MMIO_QUEUE_DESC_HIGH = 0x084;
static constexpr u64 VIRTIO_MMIO_QUEUE_AVAIL_LOW = 0x090;
static constexpr u64 VIRTIO_MMIO_QUEUE_AVAIL_HIGH= 0x094;
static constexpr u64 VIRTIO_MMIO_QUEUE_USED_LOW  = 0x0A0;
static constexpr u64 VIRTIO_MMIO_QUEUE_USED_HIGH = 0x0A4;

// Status bits
static constexpr u32 VIRTIO_STATUS_ACK       = 1;
static constexpr u32 VIRTIO_STATUS_DRIVER    = 2;
static constexpr u32 VIRTIO_STATUS_FEATURES  = 8;
static constexpr u32 VIRTIO_STATUS_DRIVER_OK = 4;

// Virtqueue descriptor
struct VirtqDesc {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
} __attribute__((packed));

static constexpr u16 VRING_DESC_F_NEXT     = 1;
static constexpr u16 VRING_DESC_F_WRITE    = 2;

// Virtqueue available ring
struct VirtqAvail {
    u16 flags;
    u16 idx;
    u16 ring[];
} __attribute__((packed));

// Virtqueue used element
struct VirtqUsedElem {
    u32 id;
    u32 len;
} __attribute__((packed));

// Virtqueue used ring
struct VirtqUsed {
    u16 flags;
    u16 idx;
    VirtqUsedElem ring[];
} __attribute__((packed));

// QEMU virt: virtio devices at 0x0a000000 + n*0x200
static constexpr u64 VIRTIO_MMIO_BASE = 0x0a000000;
static constexpr u64 VIRTIO_MMIO_STRIDE = 0x200;
static constexpr u32 VIRTIO_MMIO_COUNT = 32;

// Device IDs
static constexpr u32 VIRTIO_DEV_BLK = 2;
