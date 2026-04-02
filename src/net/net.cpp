#include "net.h"
#include "virtio_net.h"
#include "tcp.h"
#include "../uart/uart.h"
#include "../mm/heap.h"

// Network configuration
static IPv4Addr local_ip = {};
static IPv4Addr gateway_ip = {};
static IPv4Addr netmask = {};
static MacAddr local_mac = {};
static u16 ip_id_counter = 1;

// ARP cache
static constexpr u32 ARP_CACHE_SIZE = 32;
struct ArpEntry {
    IPv4Addr ip;
    MacAddr mac;
    bool valid;
};
static ArpEntry arp_cache[ARP_CACHE_SIZE];

u16 inet_checksum(const void *data, u32 len) {
    const u16 *words = static_cast<const u16*>(data);
    u32 sum = 0;
    while (len > 1) {
        sum += *words++;
        len -= 2;
    }
    if (len == 1) {
        sum += *reinterpret_cast<const u8*>(words);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<u16>(~sum);
}

// ARP cache lookup
static MacAddr arp_lookup(IPv4Addr ip) {
    for (u32 i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            return arp_cache[i].mac;
        }
    }
    return MAC_ZERO;
}

static void arp_cache_add(IPv4Addr ip, MacAddr mac) {
    // Update existing
    for (u32 i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            arp_cache[i].mac = mac;
            return;
        }
    }
    // Add new
    for (u32 i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            arp_cache[i].ip = ip;
            arp_cache[i].mac = mac;
            arp_cache[i].valid = true;
            return;
        }
    }
    // Overwrite first
    arp_cache[0].ip = ip;
    arp_cache[0].mac = mac;
}

// Handle ARP
static void handle_arp(const u8 *pkt, u32 len) {
    if (len < sizeof(EthHeader) + sizeof(ArpHeader)) return;

    const ArpHeader *arp = reinterpret_cast<const ArpHeader*>(pkt + sizeof(EthHeader));

    // Cache sender
    arp_cache_add(arp->sender_ip, arp->sender_mac);

    if (ntohs(arp->opcode) == ARP_REQUEST && arp->target_ip == local_ip) {
        // Send ARP reply
        u8 reply[sizeof(EthHeader) + sizeof(ArpHeader)];
        EthHeader *eth = reinterpret_cast<EthHeader*>(reply);
        ArpHeader *rarp = reinterpret_cast<ArpHeader*>(reply + sizeof(EthHeader));

        eth->dst = arp->sender_mac;
        eth->src = local_mac;
        eth->ethertype = htons(ETH_TYPE_ARP);

        rarp->hw_type = htons(1);
        rarp->proto_type = htons(ETH_TYPE_IPV4);
        rarp->hw_size = 6;
        rarp->proto_size = 4;
        rarp->opcode = htons(ARP_REPLY);
        rarp->sender_mac = local_mac;
        rarp->sender_ip = local_ip;
        rarp->target_mac = arp->sender_mac;
        rarp->target_ip = arp->sender_ip;

        VirtioNet::send(reply, sizeof(reply));
    }
}

// Handle ICMP
static void handle_icmp(const IPv4Header *ip_hdr, const u8 *payload, u32 len) {
    if (len < sizeof(IcmpHeader)) return;

    const IcmpHeader *icmp = reinterpret_cast<const IcmpHeader*>(payload);

    if (icmp->type == ICMP_ECHO_REQUEST) {
        // Build echo reply
        u32 total = sizeof(EthHeader) + sizeof(IPv4Header) + len;
        u8 reply[MAX_PACKET];
        if (total > MAX_PACKET) return;

        EthHeader *eth = reinterpret_cast<EthHeader*>(reply);
        IPv4Header *rip = reinterpret_cast<IPv4Header*>(reply + sizeof(EthHeader));
        u8 *rpayload = reply + sizeof(EthHeader) + sizeof(IPv4Header);

        // Ethernet
        eth->dst = arp_lookup(ip_hdr->src);
        if (eth->dst == MAC_ZERO) eth->dst = arp_lookup(gateway_ip);
        eth->src = local_mac;
        eth->ethertype = htons(ETH_TYPE_IPV4);

        // IP
        rip->version_ihl = 0x45;
        rip->tos = 0;
        rip->total_length = htons(sizeof(IPv4Header) + len);
        rip->identification = htons(ip_id_counter++);
        rip->flags_fragment = 0;
        rip->ttl = 64;
        rip->protocol = IP_PROTO_ICMP;
        rip->checksum = 0;
        rip->src = local_ip;
        rip->dst = ip_hdr->src;
        rip->checksum = inet_checksum(rip, sizeof(IPv4Header));

        // ICMP reply
        memcpy(rpayload, payload, len);
        IcmpHeader *ricmp = reinterpret_cast<IcmpHeader*>(rpayload);
        ricmp->type = ICMP_ECHO_REPLY;
        ricmp->checksum = 0;
        ricmp->checksum = inet_checksum(rpayload, len);

        VirtioNet::send(reply, total);
    }
}

// Handle UDP
static void handle_udp(const IPv4Header *ip_hdr, const u8 *payload, u32 len) {
    if (len < sizeof(UdpHeader)) return;

    const UdpHeader *udp = reinterpret_cast<const UdpHeader*>(payload);
    u16 dst_port = ntohs(udp->dst_port);
    u16 data_len = ntohs(udp->length) - sizeof(UdpHeader);
    const u8 *data = payload + sizeof(UdpHeader);

    // Forward to socket layer
    extern void socket_udp_input(IPv4Addr src_ip, u16 src_port, u16 dst_port,
                                 const void *data, u32 len);
    socket_udp_input(ip_hdr->src, ntohs(udp->src_port), dst_port, data, data_len);
}

// Handle incoming IPv4
static void handle_ipv4(const u8 *pkt, u32 len) {
    if (len < sizeof(EthHeader) + sizeof(IPv4Header)) return;

    const IPv4Header *ip = reinterpret_cast<const IPv4Header*>(pkt + sizeof(EthHeader));
    u32 ip_hdr_len = (ip->version_ihl & 0x0F) * 4;
    u32 ip_total = ntohs(ip->total_length);

    if (ip_total > len - sizeof(EthHeader)) return;

    const u8 *payload = pkt + sizeof(EthHeader) + ip_hdr_len;
    u32 payload_len = ip_total - ip_hdr_len;

    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            handle_icmp(ip, payload, payload_len);
            break;
        case IP_PROTO_UDP:
            handle_udp(ip, payload, payload_len);
            break;
        case IP_PROTO_TCP:
            tcp_input(ip, payload, payload_len);
            break;
    }
}

// Called from virtio_net when a packet is received
extern "C" void net_handle_packet(const void *data, u32 len) {
    if (len < sizeof(EthHeader)) return;

    const u8 *pkt = static_cast<const u8*>(data);
    const EthHeader *eth = reinterpret_cast<const EthHeader*>(pkt);

    u16 ethertype = ntohs(eth->ethertype);

    switch (ethertype) {
        case ETH_TYPE_ARP:
            handle_arp(pkt, len);
            break;
        case ETH_TYPE_IPV4:
            handle_ipv4(pkt, len);
            break;
    }
}

namespace Net {

void init() {
    memset(arp_cache, 0, sizeof(arp_cache));

    if (!VirtioNet::init()) return;

    local_mac = VirtioNet::get_mac();

    // Default configuration (QEMU user networking: 10.0.2.x)
    local_ip = make_ip(10, 0, 2, 15);
    gateway_ip = make_ip(10, 0, 2, 2);
    netmask = make_ip(255, 255, 255, 0);

    UART::printf("  [ok] Network: %u.%u.%u.%u gw %u.%u.%u.%u\n",
                 (u64)(local_ip.addr & 0xFF),
                 (u64)((local_ip.addr >> 8) & 0xFF),
                 (u64)((local_ip.addr >> 16) & 0xFF),
                 (u64)((local_ip.addr >> 24) & 0xFF),
                 (u64)(gateway_ip.addr & 0xFF),
                 (u64)((gateway_ip.addr >> 8) & 0xFF),
                 (u64)((gateway_ip.addr >> 16) & 0xFF),
                 (u64)((gateway_ip.addr >> 24) & 0xFF));
}

void poll() {
    VirtioNet::poll();
}

void set_ip(IPv4Addr ip) { local_ip = ip; }
void set_gateway(IPv4Addr gw) { gateway_ip = gw; }
void set_netmask(IPv4Addr mask) { netmask = mask; }
IPv4Addr get_ip() { return local_ip; }
IPv4Addr get_gateway() { return gateway_ip; }
MacAddr get_mac() { return local_mac; }

bool send_frame(const void *data, u32 len) {
    return VirtioNet::send(data, len);
}

bool send_ip(IPv4Addr dst, u8 protocol, const void *payload, u32 payload_len) {
    u32 total = sizeof(EthHeader) + sizeof(IPv4Header) + payload_len;
    if (total > MAX_PACKET) return false;

    u8 pkt[MAX_PACKET];

    EthHeader *eth = reinterpret_cast<EthHeader*>(pkt);
    IPv4Header *ip = reinterpret_cast<IPv4Header*>(pkt + sizeof(EthHeader));
    u8 *data = pkt + sizeof(EthHeader) + sizeof(IPv4Header);

    // Determine next hop
    IPv4Addr next_hop = dst;
    if ((dst.addr & netmask.addr) != (local_ip.addr & netmask.addr)) {
        next_hop = gateway_ip;
    }

    // Resolve MAC
    MacAddr dst_mac = arp_resolve(next_hop);
    if (dst_mac == MAC_ZERO) {
        // ARP failed
        return false;
    }

    eth->dst = dst_mac;
    eth->src = local_mac;
    eth->ethertype = htons(ETH_TYPE_IPV4);

    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = htons(sizeof(IPv4Header) + payload_len);
    ip->identification = htons(ip_id_counter++);
    ip->flags_fragment = htons(0x4000);  // Don't fragment
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src = local_ip;
    ip->dst = dst;
    ip->checksum = inet_checksum(ip, sizeof(IPv4Header));

    memcpy(data, payload, payload_len);

    return VirtioNet::send(pkt, total);
}

MacAddr arp_resolve(IPv4Addr ip) {
    // Check cache first
    MacAddr cached = arp_lookup(ip);
    if (!(cached == MAC_ZERO)) return cached;

    // Send ARP request
    u8 req[sizeof(EthHeader) + sizeof(ArpHeader)];
    EthHeader *eth = reinterpret_cast<EthHeader*>(req);
    ArpHeader *arp = reinterpret_cast<ArpHeader*>(req + sizeof(EthHeader));

    eth->dst = MAC_BROADCAST;
    eth->src = local_mac;
    eth->ethertype = htons(ETH_TYPE_ARP);

    arp->hw_type = htons(1);
    arp->proto_type = htons(ETH_TYPE_IPV4);
    arp->hw_size = 6;
    arp->proto_size = 4;
    arp->opcode = htons(ARP_REQUEST);
    arp->sender_mac = local_mac;
    arp->sender_ip = local_ip;
    arp->target_mac = MAC_ZERO;
    arp->target_ip = ip;

    // Send and wait for reply (with retries)
    for (int attempt = 0; attempt < 3; attempt++) {
        VirtioNet::send(req, sizeof(req));

        // Poll for response
        for (int i = 0; i < 100000; i++) {
            VirtioNet::poll();
            cached = arp_lookup(ip);
            if (!(cached == MAC_ZERO)) return cached;
        }
    }

    return MAC_ZERO;
}

} // namespace Net
