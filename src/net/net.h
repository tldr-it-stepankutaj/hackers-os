#pragma once
#include "../kernel/kernel.h"

// Network byte order helpers (ARM64 is little-endian)
static inline u16 htons(u16 v) { return __builtin_bswap16(v); }
static inline u16 ntohs(u16 v) { return __builtin_bswap16(v); }
static inline u32 htonl(u32 v) { return __builtin_bswap32(v); }
static inline u32 ntohl(u32 v) { return __builtin_bswap32(v); }

// MAC address
struct MacAddr {
    u8 bytes[6];
    bool operator==(const MacAddr &o) const {
        return memcmp(bytes, o.bytes, 6) == 0;
    }
};

static constexpr MacAddr MAC_BROADCAST = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
static constexpr MacAddr MAC_ZERO = {{0, 0, 0, 0, 0, 0}};

// IPv4 address
struct IPv4Addr {
    u32 addr;  // network byte order
    bool operator==(const IPv4Addr &o) const { return addr == o.addr; }
};

static inline IPv4Addr make_ip(u8 a, u8 b, u8 c, u8 d) {
    return {static_cast<u32>(a) | (static_cast<u32>(b) << 8) |
            (static_cast<u32>(c) << 16) | (static_cast<u32>(d) << 24)};
}

// Ethernet header
struct EthHeader {
    MacAddr dst;
    MacAddr src;
    u16 ethertype;  // network byte order
} __attribute__((packed));

static constexpr u16 ETH_TYPE_ARP  = 0x0806;
static constexpr u16 ETH_TYPE_IPV4 = 0x0800;

// ARP header
struct ArpHeader {
    u16 hw_type;
    u16 proto_type;
    u8  hw_size;
    u8  proto_size;
    u16 opcode;
    MacAddr  sender_mac;
    IPv4Addr sender_ip;
    MacAddr  target_mac;
    IPv4Addr target_ip;
} __attribute__((packed));

static constexpr u16 ARP_REQUEST = 1;
static constexpr u16 ARP_REPLY   = 2;

// IPv4 header
struct IPv4Header {
    u8  version_ihl;
    u8  tos;
    u16 total_length;
    u16 identification;
    u16 flags_fragment;
    u8  ttl;
    u8  protocol;
    u16 checksum;
    IPv4Addr src;
    IPv4Addr dst;
} __attribute__((packed));

static constexpr u8 IP_PROTO_ICMP = 1;
static constexpr u8 IP_PROTO_TCP  = 6;
static constexpr u8 IP_PROTO_UDP  = 17;

// ICMP header
struct IcmpHeader {
    u8  type;
    u8  code;
    u16 checksum;
    u16 identifier;
    u16 sequence;
} __attribute__((packed));

static constexpr u8 ICMP_ECHO_REPLY   = 0;
static constexpr u8 ICMP_ECHO_REQUEST = 8;

// UDP header
struct UdpHeader {
    u16 src_port;
    u16 dst_port;
    u16 length;
    u16 checksum;
} __attribute__((packed));

// TCP header
struct TcpHeader {
    u16 src_port;
    u16 dst_port;
    u32 seq_num;
    u32 ack_num;
    u8  data_offset;  // upper 4 bits = offset in 32-bit words
    u8  flags;
    u16 window;
    u16 checksum;
    u16 urgent;
} __attribute__((packed));

static constexpr u8 TCP_FIN = 0x01;
static constexpr u8 TCP_SYN = 0x02;
static constexpr u8 TCP_RST = 0x04;
static constexpr u8 TCP_PSH = 0x08;
static constexpr u8 TCP_ACK = 0x10;

// Maximum packet size
static constexpr u32 MTU = 1500;
static constexpr u32 MAX_PACKET = MTU + sizeof(EthHeader) + 4;

// Internet checksum
u16 inet_checksum(const void *data, u32 len);

namespace Net {

void init();
void poll();  // Process received packets

// Configuration
void set_ip(IPv4Addr ip);
void set_gateway(IPv4Addr gw);
void set_netmask(IPv4Addr mask);
IPv4Addr get_ip();
IPv4Addr get_gateway();
MacAddr get_mac();

// Send raw ethernet frame
bool send_frame(const void *data, u32 len);

// Send IP packet
bool send_ip(IPv4Addr dst, u8 protocol, const void *payload, u32 len);

// ARP
MacAddr arp_resolve(IPv4Addr ip);

} // namespace Net
