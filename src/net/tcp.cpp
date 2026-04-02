#include "tcp.h"
#include "net.h"
#include "../uart/uart.h"

static TcpConnection connections[MAX_TCP_CONNECTIONS];
static u16 next_ephemeral_port = 49152;

static u16 tcp_checksum(IPv4Addr src, IPv4Addr dst, const void *tcp_data, u32 tcp_len) {
    // Pseudo-header + TCP segment
    u32 sum = 0;
    // Pseudo-header
    const u16 *s = reinterpret_cast<const u16*>(&src.addr);
    sum += s[0]; sum += s[1];
    s = reinterpret_cast<const u16*>(&dst.addr);
    sum += s[0]; sum += s[1];
    sum += htons(IP_PROTO_TCP);
    sum += htons(static_cast<u16>(tcp_len));

    // TCP data
    const u16 *data = static_cast<const u16*>(tcp_data);
    u32 remaining = tcp_len;
    while (remaining > 1) {
        sum += *data++;
        remaining -= 2;
    }
    if (remaining == 1) {
        sum += *reinterpret_cast<const u8*>(data);
    }

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return static_cast<u16>(~sum);
}

static bool send_tcp_segment(TcpConnection *conn, u8 flags, const void *data, u32 data_len) {
    u8 segment[sizeof(TcpHeader) + TCP_BUF_SIZE];
    TcpHeader *tcp = reinterpret_cast<TcpHeader*>(segment);

    tcp->src_port = htons(conn->local_port);
    tcp->dst_port = htons(conn->remote_port);
    tcp->seq_num = htonl(conn->snd_nxt);
    tcp->ack_num = htonl(conn->rcv_nxt);
    tcp->data_offset = (5 << 4);  // 20 bytes, no options
    tcp->flags = flags;
    tcp->window = htons(TCP_BUF_SIZE - conn->rx_count);
    tcp->checksum = 0;
    tcp->urgent = 0;

    if (data && data_len > 0) {
        memcpy(segment + sizeof(TcpHeader), data, data_len);
    }

    u32 total = sizeof(TcpHeader) + data_len;
    tcp->checksum = tcp_checksum(conn->local_ip, conn->remote_ip, segment, total);

    bool ok = Net::send_ip(conn->remote_ip, IP_PROTO_TCP, segment, total);

    if (ok && (flags & TCP_SYN)) conn->snd_nxt++;
    if (ok && (flags & TCP_FIN)) conn->snd_nxt++;
    if (ok && data_len > 0) conn->snd_nxt += data_len;

    return ok;
}

void tcp_input(const IPv4Header *ip, const u8 *payload, u32 len) {
    if (len < sizeof(TcpHeader)) return;

    const TcpHeader *tcp = reinterpret_cast<const TcpHeader*>(payload);
    u16 src_port = ntohs(tcp->src_port);
    u16 dst_port = ntohs(tcp->dst_port);
    u32 seq = ntohl(tcp->seq_num);
    u32 ack = ntohl(tcp->ack_num);
    u8 flags = tcp->flags;
    u32 hdr_len = (tcp->data_offset >> 4) * 4;
    u32 data_len = len - hdr_len;
    const u8 *data = payload + hdr_len;

    // Find matching connection
    TcpConnection *conn = nullptr;
    TcpConnection *listen_conn = nullptr;

    for (u32 i = 0; i < MAX_TCP_CONNECTIONS; i++) {
        if (!connections[i].active) continue;

        if (connections[i].state == TcpState::LISTEN &&
            connections[i].local_port == dst_port) {
            listen_conn = &connections[i];
        }

        if (connections[i].remote_ip == ip->src &&
            connections[i].remote_port == src_port &&
            connections[i].local_port == dst_port) {
            conn = &connections[i];
            break;
        }
    }

    if (!conn && listen_conn && (flags & TCP_SYN)) {
        // Incoming connection on listening socket
        // Find free slot for new connection
        for (u32 i = 0; i < MAX_TCP_CONNECTIONS; i++) {
            if (!connections[i].active) {
                conn = &connections[i];
                conn->active = true;
                conn->state = TcpState::SYN_RECEIVED;
                conn->local_ip = Net::get_ip();
                conn->local_port = dst_port;
                conn->remote_ip = ip->src;
                conn->remote_port = src_port;
                conn->rcv_nxt = seq + 1;
                conn->snd_nxt = 1000;  // ISN
                conn->snd_una = conn->snd_nxt;
                conn->rx_head = 0;
                conn->rx_tail = 0;
                conn->rx_count = 0;

                send_tcp_segment(conn, TCP_SYN | TCP_ACK, nullptr, 0);
                return;
            }
        }
        return;
    }

    if (!conn) return;

    // State machine
    switch (conn->state) {
        case TcpState::SYN_SENT:
            if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
                conn->rcv_nxt = seq + 1;
                conn->snd_una = ack;
                conn->state = TcpState::ESTABLISHED;
                send_tcp_segment(conn, TCP_ACK, nullptr, 0);
            }
            break;

        case TcpState::SYN_RECEIVED:
            if (flags & TCP_ACK) {
                conn->snd_una = ack;
                conn->state = TcpState::ESTABLISHED;
            }
            break;

        case TcpState::ESTABLISHED:
            if (flags & TCP_FIN) {
                conn->rcv_nxt = seq + 1;
                conn->state = TcpState::CLOSE_WAIT;
                send_tcp_segment(conn, TCP_ACK, nullptr, 0);
            } else {
                if (flags & TCP_ACK) conn->snd_una = ack;

                if (data_len > 0 && seq == conn->rcv_nxt) {
                    // Buffer received data
                    u32 space = TCP_BUF_SIZE - conn->rx_count;
                    u32 copy = data_len < space ? data_len : space;
                    for (u32 i = 0; i < copy; i++) {
                        conn->rx_buf[conn->rx_head] = data[i];
                        conn->rx_head = (conn->rx_head + 1) % TCP_BUF_SIZE;
                    }
                    conn->rx_count += copy;
                    conn->rcv_nxt += copy;
                    send_tcp_segment(conn, TCP_ACK, nullptr, 0);
                }
            }
            break;

        case TcpState::FIN_WAIT_1:
            if (flags & TCP_ACK) {
                conn->snd_una = ack;
                if (flags & TCP_FIN) {
                    conn->rcv_nxt = seq + 1;
                    conn->state = TcpState::TIME_WAIT;
                    send_tcp_segment(conn, TCP_ACK, nullptr, 0);
                } else {
                    conn->state = TcpState::FIN_WAIT_2;
                }
            }
            break;

        case TcpState::FIN_WAIT_2:
            if (flags & TCP_FIN) {
                conn->rcv_nxt = seq + 1;
                conn->state = TcpState::TIME_WAIT;
                send_tcp_segment(conn, TCP_ACK, nullptr, 0);
            }
            break;

        case TcpState::LAST_ACK:
            if (flags & TCP_ACK) {
                conn->active = false;
                conn->state = TcpState::CLOSED;
            }
            break;

        default:
            break;
    }
}

namespace TCP {

void init() {
    memset(connections, 0, sizeof(connections));
}

i32 open() {
    for (u32 i = 0; i < MAX_TCP_CONNECTIONS; i++) {
        if (!connections[i].active) {
            memset(&connections[i], 0, sizeof(TcpConnection));
            connections[i].active = true;
            connections[i].state = TcpState::CLOSED;
            connections[i].local_ip = Net::get_ip();
            return static_cast<i32>(i);
        }
    }
    return -1;
}

void close(i32 id) {
    if (id < 0 || id >= static_cast<i32>(MAX_TCP_CONNECTIONS)) return;
    TcpConnection *conn = &connections[id];
    if (!conn->active) return;

    if (conn->state == TcpState::ESTABLISHED) {
        send_tcp_segment(conn, TCP_FIN | TCP_ACK, nullptr, 0);
        conn->state = TcpState::FIN_WAIT_1;
    } else if (conn->state == TcpState::CLOSE_WAIT) {
        send_tcp_segment(conn, TCP_FIN | TCP_ACK, nullptr, 0);
        conn->state = TcpState::LAST_ACK;
    } else {
        conn->active = false;
        conn->state = TcpState::CLOSED;
    }
}

i32 bind(i32 id, u16 port) {
    if (id < 0 || id >= static_cast<i32>(MAX_TCP_CONNECTIONS)) return -1;
    connections[id].local_port = port;
    return 0;
}

i32 connect(i32 id, IPv4Addr addr, u16 port) {
    if (id < 0 || id >= static_cast<i32>(MAX_TCP_CONNECTIONS)) return -1;
    TcpConnection *conn = &connections[id];

    conn->remote_ip = addr;
    conn->remote_port = port;
    if (conn->local_port == 0) conn->local_port = next_ephemeral_port++;
    conn->snd_nxt = 1000;
    conn->snd_una = conn->snd_nxt;
    conn->rcv_nxt = 0;
    conn->state = TcpState::SYN_SENT;

    send_tcp_segment(conn, TCP_SYN, nullptr, 0);

    // Wait for SYN-ACK (polling)
    for (int i = 0; i < 500000; i++) {
        Net::poll();
        if (conn->state == TcpState::ESTABLISHED) return 0;
        if (conn->state == TcpState::CLOSED) return -1;
    }
    return -1;  // timeout
}

i32 listen(i32 id, u16 port) {
    if (id < 0 || id >= static_cast<i32>(MAX_TCP_CONNECTIONS)) return -1;
    connections[id].local_port = port;
    connections[id].state = TcpState::LISTEN;
    return 0;
}

i32 accept(i32 listen_id) {
    if (listen_id < 0 || listen_id >= static_cast<i32>(MAX_TCP_CONNECTIONS)) return -1;

    // Look for SYN_RECEIVED or ESTABLISHED connections spawned from listen
    for (u32 i = 0; i < MAX_TCP_CONNECTIONS; i++) {
        if (static_cast<i32>(i) == listen_id) continue;
        if (connections[i].active &&
            connections[i].local_port == connections[listen_id].local_port &&
            (connections[i].state == TcpState::ESTABLISHED ||
             connections[i].state == TcpState::SYN_RECEIVED)) {
            return static_cast<i32>(i);
        }
    }

    // Poll and wait
    for (int t = 0; t < 1000000; t++) {
        Net::poll();
        for (u32 i = 0; i < MAX_TCP_CONNECTIONS; i++) {
            if (static_cast<i32>(i) == listen_id) continue;
            if (connections[i].active &&
                connections[i].local_port == connections[listen_id].local_port &&
                connections[i].state == TcpState::ESTABLISHED) {
                return static_cast<i32>(i);
            }
        }
    }
    return -1;
}

i32 send(i32 id, const void *data, u32 len) {
    if (id < 0 || id >= static_cast<i32>(MAX_TCP_CONNECTIONS)) return -1;
    TcpConnection *conn = &connections[id];
    if (conn->state != TcpState::ESTABLISHED) return -1;

    // Send in MSS-sized chunks
    u32 mss = 1460;
    u32 sent = 0;
    const u8 *p = static_cast<const u8*>(data);

    while (sent < len) {
        u32 chunk = len - sent;
        if (chunk > mss) chunk = mss;

        if (!send_tcp_segment(conn, TCP_ACK | TCP_PSH, p + sent, chunk)) {
            break;
        }
        sent += chunk;

        // Brief poll for ACKs
        for (int i = 0; i < 10000; i++) Net::poll();
    }
    return static_cast<i32>(sent);
}

i32 recv(i32 id, void *buf, u32 len) {
    if (id < 0 || id >= static_cast<i32>(MAX_TCP_CONNECTIONS)) return -1;
    TcpConnection *conn = &connections[id];

    // Poll until data available or connection closed
    for (int timeout = 0; timeout < 1000000 && conn->rx_count == 0; timeout++) {
        Net::poll();
        if (conn->state != TcpState::ESTABLISHED &&
            conn->state != TcpState::CLOSE_WAIT) {
            if (conn->rx_count == 0) return 0;
            break;
        }
    }

    u32 copy = conn->rx_count < len ? conn->rx_count : len;
    u8 *dst = static_cast<u8*>(buf);
    for (u32 i = 0; i < copy; i++) {
        dst[i] = conn->rx_buf[conn->rx_tail];
        conn->rx_tail = (conn->rx_tail + 1) % TCP_BUF_SIZE;
    }
    conn->rx_count -= copy;
    return static_cast<i32>(copy);
}

TcpState get_state(i32 id) {
    if (id < 0 || id >= static_cast<i32>(MAX_TCP_CONNECTIONS))
        return TcpState::CLOSED;
    return connections[id].state;
}

} // namespace TCP
