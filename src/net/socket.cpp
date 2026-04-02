#include "socket.h"
#include "tcp.h"
#include "net.h"
#include "../compat/linux_syscalls.h"
#include "../uart/uart.h"

static constexpr u32 MAX_SOCKETS = 64;
static constexpr u32 UDP_BUF_SIZE = 4096;
static constexpr u16 SOCKET_FD_BASE = 100;  // Socket FDs start at 100

enum class SockType : u8 { NONE = 0, TCP, UDP, RAW };

struct UdpBuffer {
    u8 data[UDP_BUF_SIZE];
    u32 head;
    u32 tail;
    u32 count;
    // Store source info for recvfrom
    IPv4Addr last_src_ip;
    u16 last_src_port;
};

struct SocketEntry {
    bool active;
    SockType type;
    u32 domain;
    u16 local_port;
    IPv4Addr remote_ip;
    u16 remote_port;
    i32 tcp_conn_id;  // for TCP sockets
    UdpBuffer *udp_buf;  // for UDP sockets
};

static SocketEntry sockets[MAX_SOCKETS];
static u16 next_udp_port = 50000;

// Helper: fd to socket index
static i32 fd_to_idx(i32 fd) {
    i32 idx = fd - SOCKET_FD_BASE;
    if (idx < 0 || idx >= static_cast<i32>(MAX_SOCKETS)) return -1;
    if (!sockets[idx].active) return -1;
    return idx;
}

void socket_udp_input(IPv4Addr src_ip, u16 src_port, u16 dst_port,
                      const void *data, u32 len) {
    // Find matching UDP socket
    for (u32 i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i].active && sockets[i].type == SockType::UDP &&
            sockets[i].local_port == dst_port) {
            UdpBuffer *buf = sockets[i].udp_buf;
            if (!buf) continue;

            u32 space = UDP_BUF_SIZE - buf->count;
            u32 copy = len < space ? len : space;
            const u8 *src = static_cast<const u8*>(data);
            for (u32 j = 0; j < copy; j++) {
                buf->data[buf->head] = src[j];
                buf->head = (buf->head + 1) % UDP_BUF_SIZE;
            }
            buf->count += copy;
            buf->last_src_ip = src_ip;
            buf->last_src_port = src_port;
            return;
        }
    }
}

namespace Socket {

void init() {
    memset(sockets, 0, sizeof(sockets));
    TCP::init();
}

i32 create(u32 domain, u32 type, u32 protocol) {
    (void)protocol;

    if (domain != LinuxSyscall::AF_INET) return LinuxSyscall::LINUX_EAFNOSUPPORT;

    SockType st;
    if (type == LinuxSyscall::SOCK_STREAM) st = SockType::TCP;
    else if (type == LinuxSyscall::SOCK_DGRAM) st = SockType::UDP;
    else if (type == LinuxSyscall::SOCK_RAW) st = SockType::RAW;
    else return LinuxSyscall::LINUX_EINVAL;

    for (u32 i = 0; i < MAX_SOCKETS; i++) {
        if (!sockets[i].active) {
            memset(&sockets[i], 0, sizeof(SocketEntry));
            sockets[i].active = true;
            sockets[i].type = st;
            sockets[i].domain = domain;
            sockets[i].tcp_conn_id = -1;

            if (st == SockType::TCP) {
                sockets[i].tcp_conn_id = TCP::open();
                if (sockets[i].tcp_conn_id < 0) {
                    sockets[i].active = false;
                    return LinuxSyscall::LINUX_ENOMEM;
                }
            } else if (st == SockType::UDP) {
                // Allocate UDP buffer
                static UdpBuffer udp_bufs[MAX_SOCKETS];
                memset(&udp_bufs[i], 0, sizeof(UdpBuffer));
                sockets[i].udp_buf = &udp_bufs[i];
            }

            return static_cast<i32>(SOCKET_FD_BASE + i);
        }
    }
    return LinuxSyscall::LINUX_ENOMEM;
}

i32 bind(i32 fd, const void *addr, u32 addrlen) {
    i32 idx = fd_to_idx(fd);
    if (idx < 0) return LinuxSyscall::LINUX_ENOTSOCK;
    if (addrlen < sizeof(LinuxSyscall::linux_sockaddr_in))
        return LinuxSyscall::LINUX_EINVAL;

    const LinuxSyscall::linux_sockaddr_in *sa =
        static_cast<const LinuxSyscall::linux_sockaddr_in*>(addr);

    sockets[idx].local_port = ntohs(sa->sin_port);

    if (sockets[idx].type == SockType::TCP) {
        TCP::bind(sockets[idx].tcp_conn_id, sockets[idx].local_port);
    }
    return 0;
}

i32 listen(i32 fd, u32 backlog) {
    (void)backlog;
    i32 idx = fd_to_idx(fd);
    if (idx < 0) return LinuxSyscall::LINUX_ENOTSOCK;
    if (sockets[idx].type != SockType::TCP) return LinuxSyscall::LINUX_EINVAL;

    return TCP::listen(sockets[idx].tcp_conn_id, sockets[idx].local_port);
}

i32 accept(i32 fd, void *addr, u32 *addrlen) {
    (void)addr; (void)addrlen;
    i32 idx = fd_to_idx(fd);
    if (idx < 0) return LinuxSyscall::LINUX_ENOTSOCK;
    if (sockets[idx].type != SockType::TCP) return LinuxSyscall::LINUX_EINVAL;

    i32 new_conn = TCP::accept(sockets[idx].tcp_conn_id);
    if (new_conn < 0) return LinuxSyscall::LINUX_EAGAIN;

    // Create new socket for accepted connection
    for (u32 i = 0; i < MAX_SOCKETS; i++) {
        if (!sockets[i].active) {
            sockets[i].active = true;
            sockets[i].type = SockType::TCP;
            sockets[i].domain = sockets[idx].domain;
            sockets[i].tcp_conn_id = new_conn;
            sockets[i].local_port = sockets[idx].local_port;
            return static_cast<i32>(SOCKET_FD_BASE + i);
        }
    }
    return LinuxSyscall::LINUX_ENOMEM;
}

i32 connect(i32 fd, const void *addr, u32 addrlen) {
    i32 idx = fd_to_idx(fd);
    if (idx < 0) return LinuxSyscall::LINUX_ENOTSOCK;
    if (addrlen < sizeof(LinuxSyscall::linux_sockaddr_in))
        return LinuxSyscall::LINUX_EINVAL;

    const LinuxSyscall::linux_sockaddr_in *sa =
        static_cast<const LinuxSyscall::linux_sockaddr_in*>(addr);

    IPv4Addr remote_ip = {sa->sin_addr};
    u16 remote_port = ntohs(sa->sin_port);

    sockets[idx].remote_ip = remote_ip;
    sockets[idx].remote_port = remote_port;

    if (sockets[idx].type == SockType::TCP) {
        return TCP::connect(sockets[idx].tcp_conn_id, remote_ip, remote_port);
    }
    // UDP: connect just sets the default destination
    return 0;
}

i64 sendto(i32 fd, const void *buf, u32 len, u32 flags,
           const void *dest_addr, u32 addrlen) {
    (void)flags;
    i32 idx = fd_to_idx(fd);
    if (idx < 0) return LinuxSyscall::LINUX_ENOTSOCK;

    if (sockets[idx].type == SockType::TCP) {
        return TCP::send(sockets[idx].tcp_conn_id, buf, len);
    }

    if (sockets[idx].type == SockType::UDP) {
        IPv4Addr dst_ip = sockets[idx].remote_ip;
        u16 dst_port = sockets[idx].remote_port;

        if (dest_addr && addrlen >= sizeof(LinuxSyscall::linux_sockaddr_in)) {
            const LinuxSyscall::linux_sockaddr_in *sa =
                static_cast<const LinuxSyscall::linux_sockaddr_in*>(dest_addr);
            dst_ip = {sa->sin_addr};
            dst_port = ntohs(sa->sin_port);
        }

        if (sockets[idx].local_port == 0) {
            sockets[idx].local_port = next_udp_port++;
        }

        // Build UDP packet
        u8 udp_pkt[sizeof(UdpHeader) + MTU];
        UdpHeader *udp = reinterpret_cast<UdpHeader*>(udp_pkt);
        udp->src_port = htons(sockets[idx].local_port);
        udp->dst_port = htons(dst_port);
        udp->length = htons(sizeof(UdpHeader) + len);
        udp->checksum = 0;  // optional for UDP over IPv4

        u32 copy = len;
        if (copy > MTU - sizeof(UdpHeader)) copy = MTU - sizeof(UdpHeader);
        memcpy(udp_pkt + sizeof(UdpHeader), buf, copy);

        if (Net::send_ip(dst_ip, IP_PROTO_UDP, udp_pkt,
                         sizeof(UdpHeader) + copy)) {
            return static_cast<i64>(copy);
        }
        return LinuxSyscall::LINUX_EIO;
    }

    return LinuxSyscall::LINUX_EINVAL;
}

i64 recvfrom(i32 fd, void *buf, u32 len, u32 flags,
             void *src_addr, u32 *addrlen) {
    (void)flags;
    i32 idx = fd_to_idx(fd);
    if (idx < 0) return LinuxSyscall::LINUX_ENOTSOCK;

    if (sockets[idx].type == SockType::TCP) {
        return TCP::recv(sockets[idx].tcp_conn_id, buf, len);
    }

    if (sockets[idx].type == SockType::UDP) {
        UdpBuffer *ubuf = sockets[idx].udp_buf;
        if (!ubuf) return LinuxSyscall::LINUX_EIO;

        // Poll until data available
        for (int t = 0; t < 1000000 && ubuf->count == 0; t++) {
            Net::poll();
        }
        if (ubuf->count == 0) return LinuxSyscall::LINUX_EAGAIN;

        u32 copy = ubuf->count < len ? ubuf->count : len;
        u8 *dst = static_cast<u8*>(buf);
        for (u32 i = 0; i < copy; i++) {
            dst[i] = ubuf->data[ubuf->tail];
            ubuf->tail = (ubuf->tail + 1) % UDP_BUF_SIZE;
        }
        ubuf->count -= copy;

        // Fill source address
        if (src_addr && addrlen && *addrlen >= sizeof(LinuxSyscall::linux_sockaddr_in)) {
            LinuxSyscall::linux_sockaddr_in *sa =
                static_cast<LinuxSyscall::linux_sockaddr_in*>(src_addr);
            sa->sin_family = LinuxSyscall::AF_INET;
            sa->sin_port = htons(ubuf->last_src_port);
            sa->sin_addr = ubuf->last_src_ip.addr;
            *addrlen = sizeof(LinuxSyscall::linux_sockaddr_in);
        }
        return static_cast<i64>(copy);
    }

    return LinuxSyscall::LINUX_EINVAL;
}

i32 setsockopt(i32 fd, u32 level, u32 optname, const void *optval, u32 optlen) {
    (void)level; (void)optname; (void)optval; (void)optlen;
    if (fd_to_idx(fd) < 0) return LinuxSyscall::LINUX_ENOTSOCK;
    return 0;  // Accept all options silently
}

i32 getsockopt(i32 fd, u32 level, u32 optname, void *optval, u32 *optlen) {
    (void)level; (void)optname; (void)optval; (void)optlen;
    if (fd_to_idx(fd) < 0) return LinuxSyscall::LINUX_ENOTSOCK;
    return 0;
}

i32 getsockname(i32 fd, void *addr, u32 *addrlen) {
    i32 idx = fd_to_idx(fd);
    if (idx < 0) return LinuxSyscall::LINUX_ENOTSOCK;
    if (!addr || !addrlen || *addrlen < sizeof(LinuxSyscall::linux_sockaddr_in))
        return LinuxSyscall::LINUX_EINVAL;

    LinuxSyscall::linux_sockaddr_in *sa =
        static_cast<LinuxSyscall::linux_sockaddr_in*>(addr);
    sa->sin_family = LinuxSyscall::AF_INET;
    sa->sin_port = htons(sockets[idx].local_port);
    sa->sin_addr = Net::get_ip().addr;
    *addrlen = sizeof(LinuxSyscall::linux_sockaddr_in);
    return 0;
}

i32 getpeername(i32 fd, void *addr, u32 *addrlen) {
    i32 idx = fd_to_idx(fd);
    if (idx < 0) return LinuxSyscall::LINUX_ENOTSOCK;
    if (!addr || !addrlen || *addrlen < sizeof(LinuxSyscall::linux_sockaddr_in))
        return LinuxSyscall::LINUX_EINVAL;

    LinuxSyscall::linux_sockaddr_in *sa =
        static_cast<LinuxSyscall::linux_sockaddr_in*>(addr);
    sa->sin_family = LinuxSyscall::AF_INET;
    sa->sin_port = htons(sockets[idx].remote_port);
    sa->sin_addr = sockets[idx].remote_ip.addr;
    *addrlen = sizeof(LinuxSyscall::linux_sockaddr_in);
    return 0;
}

i32 close(i32 fd) {
    i32 idx = fd_to_idx(fd);
    if (idx < 0) return LinuxSyscall::LINUX_ENOTSOCK;

    if (sockets[idx].type == SockType::TCP && sockets[idx].tcp_conn_id >= 0) {
        TCP::close(sockets[idx].tcp_conn_id);
    }
    sockets[idx].active = false;
    return 0;
}

} // namespace Socket
