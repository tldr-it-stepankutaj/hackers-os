#pragma once
#include "../kernel/kernel.h"
#include "net.h"

// BSD-style socket API mapping to kernel FDs
namespace Socket {

void init();

// Socket operations (return FD or negative error)
i32 create(u32 domain, u32 type, u32 protocol);
i32 bind(i32 fd, const void *addr, u32 addrlen);
i32 listen(i32 fd, u32 backlog);
i32 accept(i32 fd, void *addr, u32 *addrlen);
i32 connect(i32 fd, const void *addr, u32 addrlen);
i64 sendto(i32 fd, const void *buf, u32 len, u32 flags,
           const void *dest_addr, u32 addrlen);
i64 recvfrom(i32 fd, void *buf, u32 len, u32 flags,
             void *src_addr, u32 *addrlen);
i32 setsockopt(i32 fd, u32 level, u32 optname, const void *optval, u32 optlen);
i32 getsockopt(i32 fd, u32 level, u32 optname, void *optval, u32 *optlen);
i32 getsockname(i32 fd, void *addr, u32 *addrlen);
i32 getpeername(i32 fd, void *addr, u32 *addrlen);
i32 close(i32 fd);

} // namespace Socket

// Called from net stack when UDP data arrives
void socket_udp_input(IPv4Addr src_ip, u16 src_port, u16 dst_port,
                      const void *data, u32 len);
