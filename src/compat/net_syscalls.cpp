#include "linux_syscalls.h"
#include "../net/socket.h"

namespace NetSyscall {

i64 sys_socket(u64 domain, u64 type, u64 protocol) {
    return Socket::create(static_cast<u32>(domain),
                          static_cast<u32>(type & 0xFF),  // mask out SOCK_CLOEXEC etc
                          static_cast<u32>(protocol));
}

i64 sys_bind(u64 fd, u64 addr, u64 addrlen) {
    return Socket::bind(static_cast<i32>(fd),
                        reinterpret_cast<const void*>(addr),
                        static_cast<u32>(addrlen));
}

i64 sys_listen(u64 fd, u64 backlog) {
    return Socket::listen(static_cast<i32>(fd), static_cast<u32>(backlog));
}

i64 sys_accept(u64 fd, u64 addr, u64 addrlen) {
    return Socket::accept(static_cast<i32>(fd),
                          reinterpret_cast<void*>(addr),
                          reinterpret_cast<u32*>(addrlen));
}

i64 sys_connect(u64 fd, u64 addr, u64 addrlen) {
    return Socket::connect(static_cast<i32>(fd),
                           reinterpret_cast<const void*>(addr),
                           static_cast<u32>(addrlen));
}

i64 sys_sendto(u64 fd, u64 buf, u64 len, u64 flags, u64 addr, u64 addrlen) {
    return Socket::sendto(static_cast<i32>(fd),
                          reinterpret_cast<const void*>(buf),
                          static_cast<u32>(len),
                          static_cast<u32>(flags),
                          reinterpret_cast<const void*>(addr),
                          static_cast<u32>(addrlen));
}

i64 sys_recvfrom(u64 fd, u64 buf, u64 len, u64 flags, u64 addr, u64 addrlen) {
    return Socket::recvfrom(static_cast<i32>(fd),
                            reinterpret_cast<void*>(buf),
                            static_cast<u32>(len),
                            static_cast<u32>(flags),
                            reinterpret_cast<void*>(addr),
                            reinterpret_cast<u32*>(addrlen));
}

i64 sys_setsockopt(u64 fd, u64 level, u64 optname, u64 optval, u64 optlen) {
    return Socket::setsockopt(static_cast<i32>(fd),
                              static_cast<u32>(level),
                              static_cast<u32>(optname),
                              reinterpret_cast<const void*>(optval),
                              static_cast<u32>(optlen));
}

i64 sys_getsockopt(u64 fd, u64 level, u64 optname, u64 optval, u64 optlen) {
    return Socket::getsockopt(static_cast<i32>(fd),
                              static_cast<u32>(level),
                              static_cast<u32>(optname),
                              reinterpret_cast<void*>(optval),
                              reinterpret_cast<u32*>(optlen));
}

i64 sys_getsockname(u64 fd, u64 addr, u64 addrlen) {
    return Socket::getsockname(static_cast<i32>(fd),
                               reinterpret_cast<void*>(addr),
                               reinterpret_cast<u32*>(addrlen));
}

i64 sys_getpeername(u64 fd, u64 addr, u64 addrlen) {
    return Socket::getpeername(static_cast<i32>(fd),
                               reinterpret_cast<void*>(addr),
                               reinterpret_cast<u32*>(addrlen));
}

} // namespace NetSyscall
