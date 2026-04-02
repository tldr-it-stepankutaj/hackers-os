#pragma once
#include "../kernel/kernel.h"

// Linux AArch64 syscall numbers
// From: arch/arm64/include/asm/unistd.h (Linux kernel)
namespace LinuxSyscall {

static constexpr u64 SYS_io_setup        = 0;
static constexpr u64 SYS_setxattr        = 5;
static constexpr u64 SYS_ioctl           = 29;
static constexpr u64 SYS_faccessat       = 48;
static constexpr u64 SYS_openat          = 56;
static constexpr u64 SYS_close           = 57;
static constexpr u64 SYS_read            = 63;
static constexpr u64 SYS_write           = 64;
static constexpr u64 SYS_writev          = 66;
static constexpr u64 SYS_readlinkat      = 78;
static constexpr u64 SYS_fstatat         = 79;
static constexpr u64 SYS_fstat           = 80;
static constexpr u64 SYS_exit            = 93;
static constexpr u64 SYS_exit_group      = 94;
static constexpr u64 SYS_set_tid_address = 96;
static constexpr u64 SYS_futex           = 98;
static constexpr u64 SYS_clock_gettime   = 113;
static constexpr u64 SYS_sched_yield     = 124;
static constexpr u64 SYS_kill            = 129;
static constexpr u64 SYS_tkill           = 130;
static constexpr u64 SYS_rt_sigaction    = 134;
static constexpr u64 SYS_rt_sigprocmask  = 135;
static constexpr u64 SYS_rt_sigreturn    = 139;
static constexpr u64 SYS_sigaltstack     = 132;
static constexpr u64 SYS_uname           = 160;
static constexpr u64 SYS_getpid          = 172;
static constexpr u64 SYS_getppid         = 173;
static constexpr u64 SYS_getuid          = 174;
static constexpr u64 SYS_geteuid         = 175;
static constexpr u64 SYS_getgid          = 176;
static constexpr u64 SYS_getegid         = 177;
static constexpr u64 SYS_gettid          = 178;
static constexpr u64 SYS_socket          = 198;
static constexpr u64 SYS_bind            = 200;
static constexpr u64 SYS_listen          = 201;
static constexpr u64 SYS_accept          = 202;
static constexpr u64 SYS_connect         = 203;
static constexpr u64 SYS_getsockname     = 204;
static constexpr u64 SYS_getpeername     = 205;
static constexpr u64 SYS_sendto          = 206;
static constexpr u64 SYS_recvfrom        = 207;
static constexpr u64 SYS_setsockopt      = 208;
static constexpr u64 SYS_getsockopt      = 209;
static constexpr u64 SYS_brk             = 214;
static constexpr u64 SYS_munmap          = 215;
static constexpr u64 SYS_clone           = 220;
static constexpr u64 SYS_mmap            = 222;
static constexpr u64 SYS_mprotect        = 226;
static constexpr u64 SYS_madvise         = 233;
static constexpr u64 SYS_rseq            = 293;
static constexpr u64 SYS_getrandom       = 278;

// Error codes
static constexpr i64 LINUX_EPERM   = -1;
static constexpr i64 LINUX_ENOENT  = -2;
static constexpr i64 LINUX_ESRCH   = -3;
static constexpr i64 LINUX_EINTR   = -4;
static constexpr i64 LINUX_EIO     = -5;
static constexpr i64 LINUX_ENOMEM  = -12;
static constexpr i64 LINUX_EACCES  = -13;
static constexpr i64 LINUX_EFAULT  = -14;
static constexpr i64 LINUX_EEXIST  = -17;
static constexpr i64 LINUX_ENODEV  = -19;
static constexpr i64 LINUX_EINVAL  = -22;
static constexpr i64 LINUX_ENOSYS  = -38;
static constexpr i64 LINUX_ENOTSOCK = -88;
static constexpr i64 LINUX_EAFNOSUPPORT = -97;
static constexpr i64 LINUX_ECONNREFUSED = -111;
static constexpr i64 LINUX_EAGAIN = -11;

// mmap flags
static constexpr u64 MAP_ANONYMOUS = 0x20;
static constexpr u64 MAP_PRIVATE   = 0x02;
static constexpr u64 MAP_FIXED     = 0x10;
static constexpr u64 PROT_NONE  = 0;
static constexpr u64 PROT_READ  = 1;
static constexpr u64 PROT_WRITE = 2;
static constexpr u64 PROT_EXEC  = 4;

// openat flags
static constexpr u64 AT_FDCWD = static_cast<u64>(-100);
static constexpr u64 O_RDONLY = 0;
static constexpr u64 O_WRONLY = 1;
static constexpr u64 O_RDWR   = 2;

// clock IDs
static constexpr u64 CLOCK_REALTIME  = 0;
static constexpr u64 CLOCK_MONOTONIC = 1;

// socket domains
static constexpr u64 AF_UNIX  = 1;
static constexpr u64 AF_INET  = 2;
static constexpr u64 AF_INET6 = 10;

// socket types
static constexpr u64 SOCK_STREAM = 1;
static constexpr u64 SOCK_DGRAM  = 2;
static constexpr u64 SOCK_RAW    = 3;

// Linux structures
struct linux_timespec {
    i64 tv_sec;
    i64 tv_nsec;
} __attribute__((packed));

struct linux_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
} __attribute__((packed));

struct linux_stat {
    u64 st_dev;
    u64 st_ino;
    u32 st_mode;
    u32 st_nlink;
    u32 st_uid;
    u32 st_gid;
    u64 st_rdev;
    u64 __pad1;
    i64 st_size;
    i32 st_blksize;
    i32 __pad2;
    i64 st_blocks;
    i64 st_atime_sec;
    i64 st_atime_nsec;
    i64 st_mtime_sec;
    i64 st_mtime_nsec;
    i64 st_ctime_sec;
    i64 st_ctime_nsec;
    u32 __unused[2];
} __attribute__((packed));

struct linux_iovec {
    u64 iov_base;
    u64 iov_len;
} __attribute__((packed));

struct linux_sockaddr_in {
    u16 sin_family;
    u16 sin_port;     // network byte order
    u32 sin_addr;     // network byte order
    u8  sin_zero[8];
} __attribute__((packed));

void init();
i64 dispatch(u64 nr, u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5);

} // namespace LinuxSyscall
