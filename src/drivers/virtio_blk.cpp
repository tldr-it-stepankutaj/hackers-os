#include "virtio_blk.h"
#include "virtio.h"
#include "../arch/mmio.h"
#include "../arch/aarch64.h"
#include "../mm/pages.h"
#include "../uart/uart.h"

static constexpr u32 QUEUE_SIZE = 16;
static constexpr u32 SECTOR_SIZE = 512;

// Virtio block request types
static constexpr u32 VIRTIO_BLK_T_IN  = 0;  // read
static constexpr u32 VIRTIO_BLK_T_OUT = 1;  // write

struct VirtioBlkReqHeader {
    u32 type;
    u32 reserved;
    u64 sector;
} __attribute__((packed));

static u64 device_base = 0;
static u64 capacity_sectors = 0;

// Virtqueue structures
static VirtqDesc *desc = nullptr;
static VirtqAvail *avail = nullptr;
static VirtqUsed *used_ring = nullptr;
static u16 last_used_idx = 0;

static bool probe_device() {
    for (u32 i = 0; i < VIRTIO_MMIO_COUNT; i++) {
        u64 base = VIRTIO_MMIO_BASE + i * VIRTIO_MMIO_STRIDE;
        u32 magic = MMIO::read32(base + VIRTIO_MMIO_MAGIC);
        u32 dev_id = MMIO::read32(base + VIRTIO_MMIO_DEVICE_ID);

        if (magic == 0x74726976 && dev_id == VIRTIO_DEV_BLK) {
            device_base = base;
            return true;
        }
    }
    return false;
}

namespace VirtioBlk {

bool init() {
    if (!probe_device()) {
        UART::puts("  [--] No virtio-blk device found\n");
        return false;
    }

    u64 base = device_base;

    // Reset device
    MMIO::write32(base + VIRTIO_MMIO_STATUS, 0);

    // Acknowledge
    u32 status = VIRTIO_STATUS_ACK;
    MMIO::write32(base + VIRTIO_MMIO_STATUS, status);

    // Driver
    status |= VIRTIO_STATUS_DRIVER;
    MMIO::write32(base + VIRTIO_MMIO_STATUS, status);

    // Read features (we don't need any special ones)
    MMIO::read32(base + VIRTIO_MMIO_HOST_FEATURES);
    MMIO::write32(base + VIRTIO_MMIO_GUEST_FEATURES, 0);

    // Features OK
    status |= VIRTIO_STATUS_FEATURES;
    MMIO::write32(base + VIRTIO_MMIO_STATUS, status);

    // Setup virtqueue 0
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_SEL, 0);
    u32 max_size = MMIO::read32(base + VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_size == 0) return false;

    u32 qsize = QUEUE_SIZE < max_size ? QUEUE_SIZE : max_size;
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_NUM, qsize);

    // Allocate descriptor table, available ring, used ring
    u64 desc_page = Pages::alloc_page();
    u64 avail_page = Pages::alloc_page();
    u64 used_page = Pages::alloc_page();
    if (!desc_page || !avail_page || !used_page) return false;

    desc = reinterpret_cast<VirtqDesc*>(desc_page);
    avail = reinterpret_cast<VirtqAvail*>(avail_page);
    used_ring = reinterpret_cast<VirtqUsed*>(used_page);

    MMIO::write32(base + VIRTIO_MMIO_QUEUE_DESC_LOW, desc_page & 0xFFFFFFFF);
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_DESC_HIGH, desc_page >> 32);
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_AVAIL_LOW, avail_page & 0xFFFFFFFF);
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_AVAIL_HIGH, avail_page >> 32);
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_USED_LOW, used_page & 0xFFFFFFFF);
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_USED_HIGH, used_page >> 32);

    MMIO::write32(base + VIRTIO_MMIO_QUEUE_READY, 1);

    // Driver OK
    status |= VIRTIO_STATUS_DRIVER_OK;
    MMIO::write32(base + VIRTIO_MMIO_STATUS, status);

    // Read capacity from device config (offset 0x100)
    u64 cap_lo = MMIO::read32(base + 0x100);
    u64 cap_hi = MMIO::read32(base + 0x104);
    capacity_sectors = cap_lo | (cap_hi << 32);

    UART::printf("  [ok] virtio-blk: %u sectors (%u MB)\n",
                 capacity_sectors, capacity_sectors * SECTOR_SIZE / (1024 * 1024));
    return true;
}

static bool do_request(u32 type, u64 sector, void *buf) {
    // Three-descriptor chain: header, data, status
    static VirtioBlkReqHeader header __attribute__((aligned(16)));
    static u8 status_byte __attribute__((aligned(16)));

    header.type = type;
    header.reserved = 0;
    header.sector = sector;
    status_byte = 0xFF;

    // Descriptor 0: header (device reads)
    desc[0].addr = reinterpret_cast<u64>(&header);
    desc[0].len = sizeof(header);
    desc[0].flags = VRING_DESC_F_NEXT;
    desc[0].next = 1;

    // Descriptor 1: data buffer
    desc[1].addr = reinterpret_cast<u64>(buf);
    desc[1].len = SECTOR_SIZE;
    desc[1].flags = VRING_DESC_F_NEXT |
                    (type == VIRTIO_BLK_T_IN ? VRING_DESC_F_WRITE : 0);
    desc[1].next = 2;

    // Descriptor 2: status byte (device writes)
    desc[2].addr = reinterpret_cast<u64>(&status_byte);
    desc[2].len = 1;
    desc[2].flags = VRING_DESC_F_WRITE;
    desc[2].next = 0;

    // Add to available ring
    avail->ring[avail->idx % QUEUE_SIZE] = 0;  // head descriptor index
    Arch::dmb();
    avail->idx++;
    Arch::dmb();

    // Notify device
    MMIO::write32(device_base + VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    // Poll for completion
    while (used_ring->idx == last_used_idx) {
        Arch::dmb();
    }
    last_used_idx = used_ring->idx;

    // Acknowledge interrupt
    u32 isr = MMIO::read32(device_base + VIRTIO_MMIO_INT_STATUS);
    MMIO::write32(device_base + VIRTIO_MMIO_INT_ACK, isr);

    return status_byte == 0;
}

bool read_sector(u64 sector, void *buf) {
    if (sector >= capacity_sectors) return false;
    return do_request(VIRTIO_BLK_T_IN, sector, buf);
}

bool write_sector(u64 sector, const void *buf) {
    if (sector >= capacity_sectors) return false;
    return do_request(VIRTIO_BLK_T_OUT, sector, const_cast<void*>(buf));
}

u64 get_capacity() {
    return capacity_sectors;
}

} // namespace VirtioBlk
