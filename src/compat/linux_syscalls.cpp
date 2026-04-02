#include "linux_syscalls.h"
#include "../uart/uart.h"
#include "../mm/pages.h"
#include "../mm/mmu.h"
#include "../mm/heap.h"
#include "../timer/timer.h"
#include "../process/process.h"
#include "../process/scheduler.h"
#include "../arch/aarch64.h"

// Forward declarations for networking syscalls
namespace NetSyscall {
    i64 sys_socket(u64 domain, u64 type, u64 protocol);
    i64 sys_bind(u64 fd, u64 addr, u64 addrlen);
    i64 sys_listen(u64 fd, u64 backlog);
    i64 sys_accept(u64 fd, u64 addr, u64 addrlen);
    i64 sys_connect(u64 fd, u64 addr, u64 addrlen);
    i64 sys_sendto(u64 fd, u64 buf, u64 len, u64 flags, u64 addr, u64 addrlen);
    i64 sys_recvfrom(u64 fd, u64 buf, u64 len, u64 flags, u64 addr, u64 addrlen);
    i64 sys_setsockopt(u64 fd, u64 level, u64 optname, u64 optval, u64 optlen);
    i64 sys_getsockopt(u64 fd, u64 level, u64 optname, u64 optval, u64 optlen);
    i64 sys_getsockname(u64 fd, u64 addr, u64 addrlen);
    i64 sys_getpeername(u64 fd, u64 addr, u64 addrlen);
}

// ============================================================
// Memory management: brk, mmap, munmap, mprotect
// ============================================================

// Per-process brk tracking (simplified: single process for now)
static u64 current_brk = 0;
static u64 brk_base = 0;
static constexpr u64 BRK_BASE_ADDR = 0x10000000ULL;  // User heap region
static constexpr u64 BRK_MAX_SIZE = 64 * 1024 * 1024; // 64MB max

// mmap region tracking
static constexpr u64 MMAP_BASE = 0x20000000ULL;
static constexpr u64 MMAP_MAX  = 0x30000000ULL;
static u64 mmap_next = MMAP_BASE;

static i64 sys_brk(u64 addr) {
    if (brk_base == 0) {
        brk_base = BRK_BASE_ADDR;
        current_brk = brk_base;

        // Pre-map some pages for brk region
        Process *proc = Process_::get_current();
        u64 pgd = proc ? proc->page_table : 0;
        if (pgd) {
            for (u64 off = 0; off < PAGE_SIZE * 16; off += PAGE_SIZE) {
                u64 page = Pages::alloc_page();
                if (page) {
                    MMU::map_user_page(pgd, BRK_BASE_ADDR + off, page, PTE_USER_RWX);
                }
            }
        }
    }

    if (addr == 0) {
        return static_cast<i64>(current_brk);
    }

    if (addr < brk_base || addr > brk_base + BRK_MAX_SIZE) {
        return static_cast<i64>(current_brk);
    }

    // Expand: allocate new pages
    u64 old_page_end = (current_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    u64 new_page_end = (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    Process *proc = Process_::get_current();
    u64 pgd = proc ? proc->page_table : 0;

    if (new_page_end > old_page_end && pgd) {
        for (u64 va = old_page_end; va < new_page_end; va += PAGE_SIZE) {
            u64 page = Pages::alloc_page();
            if (!page) return static_cast<i64>(current_brk);
            MMU::map_user_page(pgd, va, page, PTE_USER_RWX);
        }
    }

    current_brk = addr;
    return static_cast<i64>(current_brk);
}

static i64 sys_mmap(u64 addr, u64 length, u64 prot, u64 flags, u64 fd, u64 offset) {
    (void)prot; (void)fd; (void)offset;

    if (length == 0) return LinuxSyscall::LINUX_EINVAL;

    // Align length to page boundary
    length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Choose address
    u64 vaddr;
    if ((flags & LinuxSyscall::MAP_FIXED) && addr != 0) {
        vaddr = addr & ~(PAGE_SIZE - 1);
    } else {
        vaddr = mmap_next;
        mmap_next += length;
        if (mmap_next > MMAP_MAX) return LinuxSyscall::LINUX_ENOMEM;
    }

    // Allocate and map pages
    Process *proc = Process_::get_current();
    u64 pgd = proc ? proc->page_table : 0;

    for (u64 off = 0; off < length; off += PAGE_SIZE) {
        u64 page = Pages::alloc_page();
        if (!page) return LinuxSyscall::LINUX_ENOMEM;

        if (pgd) {
            MMU::map_user_page(pgd, vaddr + off, page, PTE_USER_RWX);
        }
    }

    return static_cast<i64>(vaddr);
}

static i64 sys_munmap(u64 addr, u64 length) {
    (void)addr; (void)length;
    // Simplified: just return success, don't actually unmap
    // (proper implementation would free pages and remove PTEs)
    return 0;
}

static i64 sys_mprotect(u64 addr, u64 len, u64 prot) {
    (void)addr; (void)len; (void)prot;
    // Simplified: accept but don't change permissions
    return 0;
}

static i64 sys_madvise(u64 addr, u64 len, u64 advice) {
    (void)addr; (void)len; (void)advice;
    return 0;
}

// ============================================================
// I/O: read, write, writev, openat, close, fstat, ioctl
// ============================================================

// Simple file descriptor table
static constexpr u32 MAX_FDS = 256;
enum FdType : u8 { FD_NONE = 0, FD_CONSOLE, FD_FILE, FD_SOCKET };

struct FdEntry {
    FdType type;
    u32 socket_id;  // for FD_SOCKET
    // Could add file position, etc.
};

static FdEntry fd_table[MAX_FDS];

static i64 sys_write(u64 fd, u64 buf, u64 count) {
    if (fd > 2 && (fd >= MAX_FDS || fd_table[fd].type == FD_NONE))
        return LinuxSyscall::LINUX_ENOENT;

    // stdout (1), stderr (2)
    if (fd <= 2) {
        const char *s = reinterpret_cast<const char*>(buf);
        for (u64 i = 0; i < count; i++) {
            UART::putc(s[i]);
        }
        return static_cast<i64>(count);
    }

    return LinuxSyscall::LINUX_EIO;
}

static i64 sys_read(u64 fd, u64 buf, u64 count) {
    // stdin (0)
    if (fd == 0) {
        char *s = reinterpret_cast<char*>(buf);
        for (u64 i = 0; i < count; i++) {
            s[i] = UART::getc();
            if (s[i] == '\r' || s[i] == '\n') {
                s[i] = '\n';
                return static_cast<i64>(i + 1);
            }
        }
        return static_cast<i64>(count);
    }
    return LinuxSyscall::LINUX_ENOENT;
}

static i64 sys_writev(u64 fd, u64 iov_ptr, u64 iovcnt) {
    const LinuxSyscall::linux_iovec *iov =
        reinterpret_cast<const LinuxSyscall::linux_iovec*>(iov_ptr);
    i64 total = 0;
    for (u64 i = 0; i < iovcnt; i++) {
        i64 ret = sys_write(fd, iov[i].iov_base, iov[i].iov_len);
        if (ret < 0) return ret;
        total += ret;
    }
    return total;
}

static i64 sys_openat(u64 dirfd, u64 pathname, u64 flags, u64 mode) {
    (void)dirfd; (void)pathname; (void)flags; (void)mode;
    // Simplified: only support /dev/null, /dev/urandom
    const char *path = reinterpret_cast<const char*>(pathname);
    if (strcmp(path, "/dev/null") == 0) {
        // Find free fd
        for (u32 i = 3; i < MAX_FDS; i++) {
            if (fd_table[i].type == FD_NONE) {
                fd_table[i].type = FD_CONSOLE;
                return static_cast<i64>(i);
            }
        }
    }
    return LinuxSyscall::LINUX_ENOENT;
}

static i64 sys_close(u64 fd) {
    if (fd < 3) return 0;  // Don't close stdin/stdout/stderr
    if (fd < MAX_FDS) {
        fd_table[fd].type = FD_NONE;
        return 0;
    }
    return LinuxSyscall::LINUX_ENOENT;
}

static i64 sys_fstat(u64 fd, u64 statbuf) {
    LinuxSyscall::linux_stat *st = reinterpret_cast<LinuxSyscall::linux_stat*>(statbuf);
    memset(st, 0, sizeof(LinuxSyscall::linux_stat));

    if (fd <= 2) {
        st->st_mode = 0020666;  // char device
        st->st_blksize = 1024;
        return 0;
    }
    return LinuxSyscall::LINUX_ENOENT;
}

static i64 sys_fstatat(u64 dirfd, u64 pathname, u64 statbuf, u64 flags) {
    (void)dirfd; (void)pathname; (void)flags;
    LinuxSyscall::linux_stat *st = reinterpret_cast<LinuxSyscall::linux_stat*>(statbuf);
    memset(st, 0, sizeof(LinuxSyscall::linux_stat));
    return LinuxSyscall::LINUX_ENOENT;
}

static i64 sys_ioctl(u64 fd, u64 cmd, u64 arg) {
    (void)fd; (void)cmd; (void)arg;
    // Most ioctls can be safely ignored
    return 0;
}

// ============================================================
// Process: exit, getpid, gettid, set_tid_address, clone
// ============================================================

static i64 sys_exit(u64 code) {
    Process *proc = Process_::get_current();
    if (proc) {
        UART::printf("[pid %u] exit(%u)\n", (u64)proc->pid, code);
        proc->state = ProcessState::ZOMBIE;
    }
    Scheduler::yield();
    // Should not return
    Arch::halt();
    return 0;
}

static i64 sys_exit_group(u64 code) {
    return sys_exit(code);
}

static i64 sys_getpid() {
    Process *proc = Process_::get_current();
    return proc ? proc->pid : 1;
}

static i64 sys_gettid() {
    return sys_getpid();
}

static i64 sys_getuid()  { return 0; }  // root
static i64 sys_geteuid() { return 0; }
static i64 sys_getgid()  { return 0; }
static i64 sys_getegid() { return 0; }
static i64 sys_getppid() { return 0; }

static u64 tid_address = 0;
static i64 sys_set_tid_address(u64 tidptr) {
    tid_address = tidptr;
    return sys_gettid();
}

// ============================================================
// Time: clock_gettime
// ============================================================

static i64 sys_clock_gettime(u64 clk_id, u64 tp_ptr) {
    (void)clk_id;
    LinuxSyscall::linux_timespec *tp =
        reinterpret_cast<LinuxSyscall::linux_timespec*>(tp_ptr);

    u64 ticks = Timer::get_ticks();
    u64 freq = Timer::get_frequency();

    // Read CNTPCT for precise time
    u64 cnt;
    asm volatile("mrs %0, cntpct_el0" : "=r"(cnt));

    if (freq > 0) {
        tp->tv_sec = static_cast<i64>(cnt / freq);
        tp->tv_nsec = static_cast<i64>((cnt % freq) * 1000000000ULL / freq);
    } else {
        tp->tv_sec = static_cast<i64>(ticks / 100);
        tp->tv_nsec = static_cast<i64>((ticks % 100) * 10000000);
    }
    return 0;
}

// ============================================================
// Signals: rt_sigaction, rt_sigprocmask, sigaltstack
// ============================================================

static i64 sys_rt_sigaction(u64 signum, u64 act, u64 oldact, u64 sigsetsize) {
    (void)signum; (void)act; (void)oldact; (void)sigsetsize;
    // Ignore signal setup — we don't deliver signals
    return 0;
}

static i64 sys_rt_sigprocmask(u64 how, u64 set, u64 oldset, u64 sigsetsize) {
    (void)how; (void)set; (void)sigsetsize;
    if (oldset) {
        memset(reinterpret_cast<void*>(oldset), 0, 8);
    }
    return 0;
}

static i64 sys_sigaltstack(u64 ss, u64 old_ss) {
    (void)ss;
    if (old_ss) {
        memset(reinterpret_cast<void*>(old_ss), 0, 24);
    }
    return 0;
}

// ============================================================
// Misc: uname, getrandom, futex, rseq
// ============================================================

static i64 sys_uname(u64 buf) {
    LinuxSyscall::linux_utsname *u =
        reinterpret_cast<LinuxSyscall::linux_utsname*>(buf);
    memset(u, 0, sizeof(LinuxSyscall::linux_utsname));
    strcpy(u->sysname, "Linux");
    strcpy(u->nodename, "hackers-os");
    strcpy(u->release, "6.1.0-hackers");
    strcpy(u->version, "#1 SMP HackersOS");
    strcpy(u->machine, "aarch64");
    strcpy(u->domainname, "(none)");
    return 0;
}

static i64 sys_getrandom(u64 buf, u64 buflen, u64 flags) {
    (void)flags;
    // Simple PRNG (xorshift64)
    static u64 state = 0x12345678DEADBEEFULL;
    u8 *out = reinterpret_cast<u8*>(buf);
    for (u64 i = 0; i < buflen; i++) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        out[i] = static_cast<u8>(state);
    }
    return static_cast<i64>(buflen);
}

static i64 sys_futex(u64 uaddr, u64 futex_op, u64 val, u64 timeout, u64 uaddr2, u64 val3) {
    (void)uaddr; (void)futex_op; (void)val; (void)timeout; (void)uaddr2; (void)val3;
    // Simplified: single-threaded, so futex ops are no-ops
    return 0;
}

static i64 sys_sched_yield() {
    Scheduler::yield();
    return 0;
}

static i64 sys_faccessat(u64 dirfd, u64 pathname, u64 mode, u64 flags) {
    (void)dirfd; (void)pathname; (void)mode; (void)flags;
    return LinuxSyscall::LINUX_ENOENT;
}

static i64 sys_readlinkat(u64 dirfd, u64 pathname, u64 buf, u64 bufsiz) {
    (void)dirfd; (void)pathname; (void)buf; (void)bufsiz;
    return LinuxSyscall::LINUX_ENOENT;
}

static i64 sys_kill(u64 pid, u64 sig) {
    (void)pid; (void)sig;
    return 0;
}

static i64 sys_rseq(u64 rseq, u64 rseq_len, u64 flags, u64 sig) {
    (void)rseq; (void)rseq_len; (void)flags; (void)sig;
    return LinuxSyscall::LINUX_ENOSYS;
}

// ============================================================
// Main dispatcher
// ============================================================

namespace LinuxSyscall {

void init() {
    memset(fd_table, 0, sizeof(fd_table));
    // Setup stdin/stdout/stderr
    fd_table[0].type = FD_CONSOLE;
    fd_table[1].type = FD_CONSOLE;
    fd_table[2].type = FD_CONSOLE;

    current_brk = 0;
    brk_base = 0;
    mmap_next = MMAP_BASE;

    UART::puts("  [ok] Linux syscall compatibility layer\n");
}

i64 dispatch(u64 nr, u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    switch (nr) {
        // Memory
        case SYS_brk:             return sys_brk(a0);
        case SYS_mmap:            return sys_mmap(a0, a1, a2, a3, a4, a5);
        case SYS_munmap:          return sys_munmap(a0, a1);
        case SYS_mprotect:        return sys_mprotect(a0, a1, a2);
        case SYS_madvise:         return sys_madvise(a0, a1, a2);

        // I/O
        case SYS_read:            return sys_read(a0, a1, a2);
        case SYS_write:           return sys_write(a0, a1, a2);
        case SYS_writev:          return sys_writev(a0, a1, a2);
        case SYS_openat:          return sys_openat(a0, a1, a2, a3);
        case SYS_close:           return sys_close(a0);
        case SYS_fstat:           return sys_fstat(a0, a1);
        case SYS_fstatat:         return sys_fstatat(a0, a1, a2, a3);
        case SYS_ioctl:           return sys_ioctl(a0, a1, a2);
        case SYS_faccessat:       return sys_faccessat(a0, a1, a2, a3);
        case SYS_readlinkat:      return sys_readlinkat(a0, a1, a2, a3);

        // Process
        case SYS_exit:            return sys_exit(a0);
        case SYS_exit_group:      return sys_exit_group(a0);
        case SYS_getpid:          return sys_getpid();
        case SYS_getppid:         return sys_getppid();
        case SYS_gettid:          return sys_gettid();
        case SYS_getuid:          return sys_getuid();
        case SYS_geteuid:         return sys_geteuid();
        case SYS_getgid:          return sys_getgid();
        case SYS_getegid:         return sys_getegid();
        case SYS_set_tid_address: return sys_set_tid_address(a0);
        case SYS_clone:           return LINUX_ENOSYS;  // No threading
        case SYS_sched_yield:     return sys_sched_yield();
        case SYS_kill:            return sys_kill(a0, a1);
        case SYS_tkill:           return sys_kill(a0, a1);

        // Time
        case SYS_clock_gettime:   return sys_clock_gettime(a0, a1);

        // Signals
        case SYS_rt_sigaction:    return sys_rt_sigaction(a0, a1, a2, a3);
        case SYS_rt_sigprocmask:  return sys_rt_sigprocmask(a0, a1, a2, a3);
        case SYS_rt_sigreturn:    return 0;
        case SYS_sigaltstack:     return sys_sigaltstack(a0, a1);

        // Misc
        case SYS_uname:           return sys_uname(a0);
        case SYS_getrandom:       return sys_getrandom(a0, a1, a2);
        case SYS_futex:           return sys_futex(a0, a1, a2, a3, a4, a5);
        case SYS_rseq:            return sys_rseq(a0, a1, a2, a3);

        // Networking (delegated to net stack)
        case SYS_socket:          return NetSyscall::sys_socket(a0, a1, a2);
        case SYS_bind:            return NetSyscall::sys_bind(a0, a1, a2);
        case SYS_listen:          return NetSyscall::sys_listen(a0, a1);
        case SYS_accept:          return NetSyscall::sys_accept(a0, a1, a2);
        case SYS_connect:         return NetSyscall::sys_connect(a0, a1, a2);
        case SYS_sendto:          return NetSyscall::sys_sendto(a0, a1, a2, a3, a4, a5);
        case SYS_recvfrom:        return NetSyscall::sys_recvfrom(a0, a1, a2, a3, a4, a5);
        case SYS_setsockopt:      return NetSyscall::sys_setsockopt(a0, a1, a2, a3, a4);
        case SYS_getsockopt:      return NetSyscall::sys_getsockopt(a0, a1, a2, a3, a4);
        case SYS_getsockname:     return NetSyscall::sys_getsockname(a0, a1, a2);
        case SYS_getpeername:     return NetSyscall::sys_getpeername(a0, a1, a2);

        default:
            // Log unknown syscalls but don't crash
            UART::printf("[compat] unknown syscall %u\n", nr);
            return LINUX_ENOSYS;
    }
}

} // namespace LinuxSyscall
