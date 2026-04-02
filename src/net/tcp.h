#pragma once
#include "net.h"

// TCP connection states
enum class TcpState : u8 {
    CLOSED = 0,
    LISTEN,
    SYN_SENT,
    SYN_RECEIVED,
    ESTABLISHED,
    FIN_WAIT_1,
    FIN_WAIT_2,
    CLOSE_WAIT,
    CLOSING,
    LAST_ACK,
    TIME_WAIT,
};

// TCP connection block
static constexpr u32 TCP_BUF_SIZE = 8192;

struct TcpConnection {
    bool active;
    TcpState state;

    IPv4Addr local_ip;
    u16 local_port;
    IPv4Addr remote_ip;
    u16 remote_port;

    u32 snd_nxt;    // next sequence number to send
    u32 snd_una;    // oldest unacknowledged
    u32 rcv_nxt;    // next expected to receive

    // Receive buffer (ring)
    u8 rx_buf[TCP_BUF_SIZE];
    u32 rx_head;
    u32 rx_tail;
    u32 rx_count;

    // TX buffer
    u8 tx_buf[TCP_BUF_SIZE];
    u32 tx_len;
};

static constexpr u32 MAX_TCP_CONNECTIONS = 16;

// Called from net.cpp when a TCP segment arrives
void tcp_input(const IPv4Header *ip, const u8 *payload, u32 len);

namespace TCP {

void init();
i32 open();                     // allocate connection, returns conn_id
void close(i32 conn_id);
i32 connect(i32 conn_id, IPv4Addr addr, u16 port);
i32 listen(i32 conn_id, u16 port);
i32 accept(i32 listen_id);     // returns new conn_id
i32 send(i32 conn_id, const void *data, u32 len);
i32 recv(i32 conn_id, void *buf, u32 len);
TcpState get_state(i32 conn_id);
i32 bind(i32 conn_id, u16 port);

} // namespace TCP
