#include "net.h"
#include "virtio_net.h"
#include "../timer/timer.h"

extern "C" void net_handle_packet(const void *data, u32 len);

// DNS header (RFC 1035)
struct DnsHeader {
    u16 id;
    u16 flags;
    u16 qdcount;
    u16 ancount;
    u16 nscount;
    u16 arcount;
} __attribute__((packed));

static constexpr u16 DNS_FLAG_QR     = 0x8000;  // Response
static constexpr u16 DNS_FLAG_RD     = 0x0100;  // Recursion Desired
static constexpr u16 DNS_FLAG_RA     = 0x0080;  // Recursion Available
static constexpr u16 DNS_RCODE_MASK  = 0x000F;

static constexpr u16 DNS_TYPE_A      = 1;
static constexpr u16 DNS_CLASS_IN    = 1;

static u16 dns_txid = 1;

// Encode hostname into DNS wire format (e.g., "www.example.com" → "\3www\7example\3com\0")
static u32 encode_name(const char *name, u8 *buf, u32 max_len) {
    u32 pos = 0;
    const char *p = name;

    while (*p && pos < max_len - 2) {
        // Find next dot
        const char *dot = p;
        while (*dot && *dot != '.') dot++;

        u8 label_len = static_cast<u8>(dot - p);
        if (pos + 1 + label_len >= max_len) break;

        buf[pos++] = label_len;
        while (p < dot) buf[pos++] = static_cast<u8>(*p++);
        if (*p == '.') p++;
    }
    buf[pos++] = 0;  // root label
    return pos;
}

// Skip a DNS name (handles compression pointers)
static const u8 *skip_name(const u8 *p, const u8 *end) {
    while (p < end) {
        if (*p == 0) return p + 1;
        if ((*p & 0xC0) == 0xC0) return p + 2;  // compression pointer
        u8 len = *p;
        p += 1 + len;
    }
    return end;
}

namespace DNS {

IPv4Addr resolve(const char *hostname) {
    IPv4Addr dns_server = Net::get_dns();
    IPv4Addr result = {0};

    if (dns_server.addr == 0) {
        // No DNS configured, try QEMU default
        dns_server = make_ip(10, 0, 2, 3);
    }

    // Build DNS query
    u8 query[512];
    u32 qlen = 0;

    // Header
    DnsHeader *hdr = reinterpret_cast<DnsHeader*>(query);
    u16 txid = dns_txid++;
    hdr->id = htons(txid);
    hdr->flags = htons(DNS_FLAG_RD);
    hdr->qdcount = htons(1);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;
    qlen += sizeof(DnsHeader);

    // Question: encoded name + type + class
    qlen += encode_name(hostname, query + qlen, sizeof(query) - qlen);

    // Type A
    query[qlen++] = 0; query[qlen++] = DNS_TYPE_A;
    // Class IN
    query[qlen++] = 0; query[qlen++] = DNS_CLASS_IN;

    // Send as UDP to port 53
    u8 udp_pkt[sizeof(UdpHeader) + 512];
    UdpHeader *udp = reinterpret_cast<UdpHeader*>(udp_pkt);
    udp->src_port = htons(10053 + (txid & 0xFF));
    udp->dst_port = htons(53);
    udp->length = htons(sizeof(UdpHeader) + qlen);
    udp->checksum = 0;
    memcpy(udp_pkt + sizeof(UdpHeader), query, qlen);

    // Try up to 3 times
    for (int attempt = 0; attempt < 3; attempt++) {
        Net::send_ip(dns_server, IP_PROTO_UDP, udp_pkt, sizeof(UdpHeader) + qlen);

        // Poll for response
        u64 start = Timer::get_ticks();
        while (Timer::get_ticks() - start < 300) {  // 3 sec timeout
            u8 buf[1500];
            u32 len;

            Net::poll();

            // Check for DNS response by sniffing raw packets
            if (!VirtioNet::receive(buf, sizeof(buf), &len)) continue;
            if (len < sizeof(EthHeader) + sizeof(IPv4Header) + sizeof(UdpHeader) + sizeof(DnsHeader))
                continue;

            IPv4Header *ip = reinterpret_cast<IPv4Header*>(buf + sizeof(EthHeader));
            if (ip->protocol != IP_PROTO_UDP) {
                // Forward non-UDP packets to normal stack
                net_handle_packet(buf, len);
                continue;
            }

            UdpHeader *rudp = reinterpret_cast<UdpHeader*>(buf + sizeof(EthHeader) + sizeof(IPv4Header));
            if (ntohs(rudp->src_port) != 53) continue;

            const u8 *dns_data = buf + sizeof(EthHeader) + sizeof(IPv4Header) + sizeof(UdpHeader);
            u32 dns_len = len - sizeof(EthHeader) - sizeof(IPv4Header) - sizeof(UdpHeader);

            if (dns_len < sizeof(DnsHeader)) continue;

            const DnsHeader *resp = reinterpret_cast<const DnsHeader*>(dns_data);
            if (ntohs(resp->id) != txid) continue;
            if (!(ntohs(resp->flags) & DNS_FLAG_QR)) continue;  // Not a response
            if (ntohs(resp->flags) & DNS_RCODE_MASK) continue;  // Error

            u16 ancount = ntohs(resp->ancount);
            if (ancount == 0) continue;

            // Skip question section
            const u8 *p = dns_data + sizeof(DnsHeader);
            const u8 *end = dns_data + dns_len;
            for (u16 i = 0; i < ntohs(resp->qdcount); i++) {
                p = skip_name(p, end);
                p += 4;  // type + class
            }

            // Parse answer records
            for (u16 i = 0; i < ancount && p < end; i++) {
                p = skip_name(p, end);
                if (p + 10 > end) break;

                u16 rtype = (p[0] << 8) | p[1];
                // u16 rclass = (p[2] << 8) | p[3];
                // u32 ttl = (p[4]<<24)|(p[5]<<16)|(p[6]<<8)|p[7];
                u16 rdlength = (p[8] << 8) | p[9];
                p += 10;

                if (rtype == DNS_TYPE_A && rdlength == 4 && p + 4 <= end) {
                    memcpy(&result.addr, p, 4);
                    return result;
                }
                p += rdlength;
            }
        }
    }

    return result;  // {0} on failure
}

} // namespace DNS
