#include "virtio_net.h"
#include "../drivers/virtio.h"
#include "../arch/mmio.h"
#include "../arch/aarch64.h"
#include "../mm/pages.h"
#include "../uart/uart.h"

static constexpr u32 QUEUE_SIZE = 16;
static constexpr u32 VIRTIO_DEV_NET = 1;

// Virtio net header (prepended to all packets)
struct VirtioNetHeader {
    u8  flags;
    u8  gso_type;
    u16 hdr_len;
    u16 gso_size;
    u16 csum_start;
    u16 csum_offset;
    // u16 num_buffers;  // mergeable buffers, not used
} __attribute__((packed));

static u64 device_base = 0;
static MacAddr device_mac = {};

// TX virtqueue (queue 1)
static VirtqDesc *tx_desc = nullptr;
static VirtqAvail *tx_avail = nullptr;
static VirtqUsed *tx_used = nullptr;
static u16 tx_last_used = 0;

// RX virtqueue (queue 0)
static VirtqDesc *rx_desc = nullptr;
static VirtqAvail *rx_avail = nullptr;
static VirtqUsed *rx_used = nullptr;
static u16 rx_last_used = 0;

// RX buffers
static constexpr u32 RX_BUF_COUNT = 8;
static u8 *rx_buffers[RX_BUF_COUNT];

static bool probe_device() {
    for (u32 i = 0; i < VIRTIO_MMIO_COUNT; i++) {
        u64 base = VIRTIO_MMIO_BASE + i * VIRTIO_MMIO_STRIDE;
        u32 magic = MMIO::read32(base + VIRTIO_MMIO_MAGIC);
        u32 dev_id = MMIO::read32(base + VIRTIO_MMIO_DEVICE_ID);

        if (magic == 0x74726976 && dev_id == VIRTIO_DEV_NET) {
            device_base = base;
            return true;
        }
    }
    return false;
}

static bool setup_queue(u64 base, u32 queue_num,
                        VirtqDesc **desc_out, VirtqAvail **avail_out,
                        VirtqUsed **used_out) {
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_SEL, queue_num);
    u32 max = MMIO::read32(base + VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0) return false;

    u32 qsz = QUEUE_SIZE < max ? QUEUE_SIZE : max;
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_NUM, qsz);

    u64 dp = Pages::alloc_page();
    u64 ap = Pages::alloc_page();
    u64 up = Pages::alloc_page();
    if (!dp || !ap || !up) return false;

    *desc_out = reinterpret_cast<VirtqDesc*>(dp);
    *avail_out = reinterpret_cast<VirtqAvail*>(ap);
    *used_out = reinterpret_cast<VirtqUsed*>(up);

    MMIO::write32(base + VIRTIO_MMIO_QUEUE_DESC_LOW, dp & 0xFFFFFFFF);
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_DESC_HIGH, dp >> 32);
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_AVAIL_LOW, ap & 0xFFFFFFFF);
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_AVAIL_HIGH, ap >> 32);
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_USED_LOW, up & 0xFFFFFFFF);
    MMIO::write32(base + VIRTIO_MMIO_QUEUE_USED_HIGH, up >> 32);

    MMIO::write32(base + VIRTIO_MMIO_QUEUE_READY, 1);
    return true;
}

static void populate_rx_queue() {
    for (u32 i = 0; i < RX_BUF_COUNT && i < QUEUE_SIZE; i++) {
        rx_buffers[i] = reinterpret_cast<u8*>(Pages::alloc_page());
        if (!rx_buffers[i]) break;

        rx_desc[i].addr = reinterpret_cast<u64>(rx_buffers[i]);
        rx_desc[i].len = PAGE_SIZE;
        rx_desc[i].flags = VRING_DESC_F_WRITE;
        rx_desc[i].next = 0;

        rx_avail->ring[rx_avail->idx % QUEUE_SIZE] = i;
        Arch::dmb();
        rx_avail->idx++;
    }
    Arch::dmb();
    MMIO::write32(device_base + VIRTIO_MMIO_QUEUE_NOTIFY, 0);
}

namespace VirtioNet {

bool init() {
    if (!probe_device()) {
        UART::puts("  [--] No virtio-net device found\n");
        return false;
    }

    u64 base = device_base;

    // Reset
    MMIO::write32(base + VIRTIO_MMIO_STATUS, 0);

    u32 status = VIRTIO_STATUS_ACK;
    MMIO::write32(base + VIRTIO_MMIO_STATUS, status);

    status |= VIRTIO_STATUS_DRIVER;
    MMIO::write32(base + VIRTIO_MMIO_STATUS, status);

    // Feature negotiation — accept MAC feature (bit 5)
    u32 features = MMIO::read32(base + VIRTIO_MMIO_HOST_FEATURES);
    MMIO::write32(base + VIRTIO_MMIO_GUEST_FEATURES, features & (1 << 5));

    status |= VIRTIO_STATUS_FEATURES;
    MMIO::write32(base + VIRTIO_MMIO_STATUS, status);

    // Setup RX queue (0) and TX queue (1)
    if (!setup_queue(base, 0, &rx_desc, &rx_avail, &rx_used)) return false;
    if (!setup_queue(base, 1, &tx_desc, &tx_avail, &tx_used)) return false;

    status |= VIRTIO_STATUS_DRIVER_OK;
    MMIO::write32(base + VIRTIO_MMIO_STATUS, status);

    // Read MAC address from config space (offset 0x100)
    if (features & (1 << 5)) {
        for (int i = 0; i < 6; i++) {
            device_mac.bytes[i] = MMIO::read8(base + 0x100 + i);
        }
    } else {
        // Generate a MAC if device doesn't provide one
        device_mac = {{0x52, 0x54, 0x00, 0x12, 0x34, 0x56}};
    }

    // Populate RX queue with buffers
    populate_rx_queue();

    UART::printf("  [ok] virtio-net: MAC %x:%x:%x:%x:%x:%x\n",
                 (u64)device_mac.bytes[0], (u64)device_mac.bytes[1],
                 (u64)device_mac.bytes[2], (u64)device_mac.bytes[3],
                 (u64)device_mac.bytes[4], (u64)device_mac.bytes[5]);
    return true;
}

bool send(const void *data, u32 len) {
    if (!device_base || len == 0) return false;

    // Prepend virtio-net header
    static u8 tx_buf[MAX_PACKET + sizeof(VirtioNetHeader)]
        __attribute__((aligned(16)));
    static VirtioNetHeader hdr __attribute__((aligned(16)));

    memset(&hdr, 0, sizeof(hdr));

    // Descriptor 0: virtio-net header
    tx_desc[0].addr = reinterpret_cast<u64>(&hdr);
    tx_desc[0].len = sizeof(VirtioNetHeader);
    tx_desc[0].flags = VRING_DESC_F_NEXT;
    tx_desc[0].next = 1;

    // Descriptor 1: packet data
    memcpy(tx_buf, data, len);
    tx_desc[1].addr = reinterpret_cast<u64>(tx_buf);
    tx_desc[1].len = len;
    tx_desc[1].flags = 0;
    tx_desc[1].next = 0;

    // Add to TX available ring
    tx_avail->ring[tx_avail->idx % QUEUE_SIZE] = 0;
    Arch::dmb();
    tx_avail->idx++;
    Arch::dmb();

    // Notify device (TX queue = 1)
    MMIO::write32(device_base + VIRTIO_MMIO_QUEUE_NOTIFY, 1);

    // Poll for completion
    for (int timeout = 0; timeout < 100000; timeout++) {
        Arch::dmb();
        if (tx_used->idx != tx_last_used) {
            tx_last_used = tx_used->idx;
            u32 isr = MMIO::read32(device_base + VIRTIO_MMIO_INT_STATUS);
            MMIO::write32(device_base + VIRTIO_MMIO_INT_ACK, isr);
            return true;
        }
    }
    return false;
}

bool receive(void *buf, u32 buf_size, u32 *received) {
    if (!device_base) return false;

    Arch::dmb();
    if (rx_used->idx == rx_last_used) return false;

    u32 used_elem_id = rx_used->ring[rx_last_used % QUEUE_SIZE].id;
    u32 used_len = rx_used->ring[rx_last_used % QUEUE_SIZE].len;
    rx_last_used++;

    if (used_len <= sizeof(VirtioNetHeader)) return false;

    u8 *pkt = rx_buffers[used_elem_id];
    u32 data_len = used_len - sizeof(VirtioNetHeader);
    if (data_len > buf_size) data_len = buf_size;

    memcpy(buf, pkt + sizeof(VirtioNetHeader), data_len);
    *received = data_len;

    // Re-add buffer to RX queue
    rx_desc[used_elem_id].addr = reinterpret_cast<u64>(rx_buffers[used_elem_id]);
    rx_desc[used_elem_id].len = PAGE_SIZE;
    rx_desc[used_elem_id].flags = VRING_DESC_F_WRITE;
    rx_avail->ring[rx_avail->idx % QUEUE_SIZE] = used_elem_id;
    Arch::dmb();
    rx_avail->idx++;
    Arch::dmb();
    MMIO::write32(device_base + VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    u32 isr = MMIO::read32(device_base + VIRTIO_MMIO_INT_STATUS);
    MMIO::write32(device_base + VIRTIO_MMIO_INT_ACK, isr);

    return true;
}

MacAddr get_mac() { return device_mac; }

} // namespace VirtioNet

extern "C" void net_handle_packet(const void *data, u32 len);

namespace VirtioNet {

void poll() {
    u8 pkt[MAX_PACKET];
    u32 len;
    while (receive(pkt, sizeof(pkt), &len)) {
        net_handle_packet(pkt, len);
    }
}

} // namespace VirtioNet
