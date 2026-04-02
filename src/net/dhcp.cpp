#include "net.h"
#include "virtio_net.h"
#include "../uart/uart.h"
#include "../timer/timer.h"

// DHCP packet structure (RFC 2131)
struct DhcpPacket {
    u8  op;         // 1=BOOTREQUEST, 2=BOOTREPLY
    u8  htype;      // 1=Ethernet
    u8  hlen;       // 6
    u8  hops;
    u32 xid;        // transaction ID
    u16 secs;
    u16 flags;
    u32 ciaddr;     // client IP
    u32 yiaddr;     // 'your' IP
    u32 siaddr;     // server IP
    u32 giaddr;     // gateway IP
    u8  chaddr[16]; // client hardware address
    u8  sname[64];  // server name
    u8  file[128];  // boot filename
    u32 magic_cookie; // 0x63825363
    u8  options[308]; // DHCP options
} __attribute__((packed));

static constexpr u32 DHCP_MAGIC = 0x63538263;  // network byte order of 0x63825363
static constexpr u8 DHCP_DISCOVER = 1;
static constexpr u8 DHCP_OFFER    = 2;
static constexpr u8 DHCP_REQUEST  = 3;
static constexpr u8 DHCP_ACK      = 5;

static u32 dhcp_xid = 0x12345678;

// DHCP option helpers
static u8 *add_option(u8 *p, u8 code, u8 len, const void *data) {
    *p++ = code;
    *p++ = len;
    memcpy(p, data, len);
    return p + len;
}

static u8 *add_option_byte(u8 *p, u8 code, u8 val) {
    return add_option(p, code, 1, &val);
}

// Find option in DHCP packet
static const u8 *find_option(const DhcpPacket *pkt, u8 code) {
    const u8 *p = pkt->options;
    const u8 *end = p + sizeof(pkt->options);
    while (p < end && *p != 0xFF) {
        if (*p == 0) { p++; continue; }  // pad
        u8 opt_code = *p++;
        if (p >= end) break;
        u8 opt_len = *p++;
        if (opt_code == code) return p;
        p += opt_len;
    }
    return nullptr;
}

static bool send_dhcp(u8 msg_type, u32 server_ip, u32 requested_ip) {
    // Build UDP packet: Ethernet + IP + UDP + DHCP
    u32 total = sizeof(EthHeader) + sizeof(IPv4Header) + sizeof(UdpHeader) + sizeof(DhcpPacket);
    u8 pkt[1500];
    memset(pkt, 0, total);

    EthHeader *eth = reinterpret_cast<EthHeader*>(pkt);
    IPv4Header *ip = reinterpret_cast<IPv4Header*>(pkt + sizeof(EthHeader));
    UdpHeader *udp = reinterpret_cast<UdpHeader*>(pkt + sizeof(EthHeader) + sizeof(IPv4Header));
    DhcpPacket *dhcp = reinterpret_cast<DhcpPacket*>(pkt + sizeof(EthHeader) + sizeof(IPv4Header) + sizeof(UdpHeader));

    // Ethernet: broadcast
    memset(eth->dst.bytes, 0xFF, 6);
    eth->src = Net::get_mac();
    eth->ethertype = htons(ETH_TYPE_IPV4);

    // IP: 0.0.0.0 → 255.255.255.255
    ip->version_ihl = 0x45;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP;
    ip->total_length = htons(sizeof(IPv4Header) + sizeof(UdpHeader) + sizeof(DhcpPacket));
    ip->dst.addr = 0xFFFFFFFF;
    ip->checksum = 0;
    ip->checksum = inet_checksum(ip, sizeof(IPv4Header));

    // UDP: 68 → 67
    udp->src_port = htons(68);
    udp->dst_port = htons(67);
    udp->length = htons(sizeof(UdpHeader) + sizeof(DhcpPacket));

    // DHCP
    dhcp->op = 1;  // BOOTREQUEST
    dhcp->htype = 1;
    dhcp->hlen = 6;
    dhcp->xid = dhcp_xid;
    dhcp->flags = htons(0x8000);  // broadcast flag
    memcpy(dhcp->chaddr, Net::get_mac().bytes, 6);
    dhcp->magic_cookie = DHCP_MAGIC;

    // Options
    u8 *opt = dhcp->options;
    opt = add_option_byte(opt, 53, msg_type);  // DHCP Message Type

    if (msg_type == DHCP_REQUEST) {
        if (requested_ip) opt = add_option(opt, 50, 4, &requested_ip);  // Requested IP
        if (server_ip) opt = add_option(opt, 54, 4, &server_ip);        // Server ID
    }

    // Parameter request list: subnet, router, DNS, domain
    u8 param_list[] = {1, 3, 6, 15};
    opt = add_option(opt, 55, sizeof(param_list), param_list);

    *opt = 0xFF;  // End

    return Net::send_frame(pkt, total);
}

// Receive and parse DHCP reply
static bool receive_dhcp(u8 expected_type, DhcpPacket *out, int timeout_ticks) {
    u64 start = Timer::get_ticks();
    u8 buf[1500];

    while (Timer::get_ticks() - start < static_cast<u64>(timeout_ticks)) {
        Net::poll();

        // We need to sniff raw frames for DHCP replies since we have no IP yet
        // The poll() already processes packets, but DHCP replies to broadcast
        // go through handle_udp → socket_udp_input. We need a different approach:
        // Check if virtio_net has a packet for us directly.
        u32 len;
        if (!VirtioNet::receive(buf, sizeof(buf), &len)) continue;
        if (len < sizeof(EthHeader) + sizeof(IPv4Header) + sizeof(UdpHeader) + 240) continue;

        // Verify it's a UDP packet to port 68
        IPv4Header *ip = reinterpret_cast<IPv4Header*>(buf + sizeof(EthHeader));
        if (ip->protocol != IP_PROTO_UDP) continue;

        UdpHeader *udp = reinterpret_cast<UdpHeader*>(buf + sizeof(EthHeader) + sizeof(IPv4Header));
        if (ntohs(udp->dst_port) != 68) continue;

        DhcpPacket *dhcp = reinterpret_cast<DhcpPacket*>(buf + sizeof(EthHeader) + sizeof(IPv4Header) + sizeof(UdpHeader));
        if (dhcp->op != 2) continue;  // Not a reply
        if (dhcp->xid != dhcp_xid) continue;
        if (dhcp->magic_cookie != DHCP_MAGIC) continue;

        // Check message type
        const u8 *mt = find_option(dhcp, 53);
        if (!mt || *mt != expected_type) continue;

        memcpy(out, dhcp, sizeof(DhcpPacket));
        return true;
    }
    return false;
}

namespace DHCP {

bool discover() {
    UART::puts("  DHCP: discovering...\n");

    // Increment xid for each attempt
    dhcp_xid++;

    // Step 1: DISCOVER
    if (!send_dhcp(DHCP_DISCOVER, 0, 0)) {
        UART::puts("  DHCP: failed to send DISCOVER\n");
        return false;
    }

    // Step 2: Wait for OFFER
    DhcpPacket offer;
    if (!receive_dhcp(DHCP_OFFER, &offer, 500)) {
        UART::puts("  DHCP: no OFFER received\n");
        return false;
    }

    u32 offered_ip = offer.yiaddr;
    u32 server_ip = offer.siaddr;

    // Get server ID from options (may differ from siaddr)
    const u8 *sid = find_option(&offer, 54);
    if (sid) memcpy(&server_ip, sid, 4);

    // Step 3: REQUEST
    if (!send_dhcp(DHCP_REQUEST, server_ip, offered_ip)) {
        UART::puts("  DHCP: failed to send REQUEST\n");
        return false;
    }

    // Step 4: Wait for ACK
    DhcpPacket ack;
    if (!receive_dhcp(DHCP_ACK, &ack, 500)) {
        UART::puts("  DHCP: no ACK received\n");
        return false;
    }

    // Configure network from ACK
    IPv4Addr ip = {ack.yiaddr};
    Net::set_ip(ip);

    // Subnet mask (option 1)
    const u8 *mask = find_option(&ack, 1);
    if (mask) {
        IPv4Addr nm;
        memcpy(&nm.addr, mask, 4);
        Net::set_netmask(nm);
    }

    // Router/gateway (option 3)
    const u8 *router = find_option(&ack, 3);
    if (router) {
        IPv4Addr gw;
        memcpy(&gw.addr, router, 4);
        Net::set_gateway(gw);
    }

    // DNS server (option 6)
    const u8 *dns = find_option(&ack, 6);
    if (dns) {
        IPv4Addr dns_ip;
        memcpy(&dns_ip.addr, dns, 4);
        Net::set_dns(dns_ip);
    }

    IPv4Addr got_ip = Net::get_ip();
    IPv4Addr got_gw = Net::get_gateway();
    IPv4Addr got_dns = Net::get_dns();
    UART::printf("  DHCP: IP=%u.%u.%u.%u GW=%u.%u.%u.%u DNS=%u.%u.%u.%u\n",
                 (u64)(got_ip.addr & 0xFF), (u64)((got_ip.addr >> 8) & 0xFF),
                 (u64)((got_ip.addr >> 16) & 0xFF), (u64)((got_ip.addr >> 24) & 0xFF),
                 (u64)(got_gw.addr & 0xFF), (u64)((got_gw.addr >> 8) & 0xFF),
                 (u64)((got_gw.addr >> 16) & 0xFF), (u64)((got_gw.addr >> 24) & 0xFF),
                 (u64)(got_dns.addr & 0xFF), (u64)((got_dns.addr >> 8) & 0xFF),
                 (u64)((got_dns.addr >> 16) & 0xFF), (u64)((got_dns.addr >> 24) & 0xFF));

    return true;
}

} // namespace DHCP
