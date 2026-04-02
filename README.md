# Hackers-OS

```
          .######################.
        .############################.
      .####    ################    ####.
     .####      ##############      ####.
    .####        ############        ####.
    ####          ##########          ####
    ####   ####   ##########   ####   ####
    ####   ####   ##########   ####   ####
    ####          ##########          ####
    .####        ############        ####.
     .####      ####....####      ####.
      .####    ##..######..##    ####.
        .########..........########.
          .####.####..####.####.
              .############.
               ............

         H A C K E R S  -  O S
          AArch64 Kernel v0.1

        powered by TLDR;IT
        https://www.tldr-it.com
```

Bare-metal AArch64 operating system kernel written in C++17 and ARM64 assembly. Runs on QEMU `virt` machine and Raspberry Pi.

Developed by **[TLDR;IT](https://www.tldr-it.com)**

---

## Features

- **Boot** — EL3→EL2→EL1 exception level drop, multicore parking, BSS clear
- **UART** — PL011 serial driver with printf-lite (primary I/O)
- **Interrupts** — GICv2 interrupt controller, full exception vector table
- **Timer** — ARM Generic Timer at 100 Hz
- **MMU** — 4-level page tables (4KB granule), identity-mapped kernel, L1 1GB blocks + L2 2MB blocks, instruction and data caches
- **Memory** — Bitmap page allocator (up to 256MB), linked-list heap with coalescing (1MB)
- **Processes** — Process table (64 slots), round-robin scheduler, context switch, zombie reaping
- **Syscalls** — SVC-based EL0→EL1 trap, Linux AArch64 syscall ABI (~45 syscalls)
- **Filesystem** — FAT32 read/write (directory listing, file read/write/create/delete) via virtio-blk
- **ELF Loader** — Loads and executes AArch64 ELF64 binaries in usermode (EL0), full EL1→EL0 transition with `eret`, kernel return on exit
- **Networking** — Full TCP/IP stack:
  - **L2:** virtio-net MMIO driver (TX/RX virtqueues)
  - **ARP:** cache (32 entries), request/reply, 3-retry resolve
  - **IPv4:** routing (local vs gateway), checksum, TTL=64
  - **ICMP:** echo request/reply
  - **UDP:** send/receive with per-socket buffers
  - **TCP:** full state machine, 16 connections, 8KB rx buffer, retransmission with exponential backoff
  - **DHCP:** full D-O-R-A client, auto-configures IP/gateway/netmask/DNS
  - **DNS:** recursive resolver (A records), 3-retry, used by `nslookup` command
  - **Sockets:** BSD API — `SOCK_STREAM`, `SOCK_DGRAM`, `SOCK_RAW`, 64 sockets
- **Linux Compat** — ~45 syscalls: mmap, brk, writev, openat, fstat, ioctl, clock_gettime, uname, getrandom, rt_sigaction, futex, socket/bind/connect/sendto/recvfrom, and more
- **Shell** — Interactive command line with 14 commands
- **Bundled Apps** — Pre-compiled AArch64 static binaries: `network-analyzer`, `securitydns`, `jwt_inspector`
- **Boot Logo** — ASCII skull with TLDR;IT branding

## Quick Start

### Prerequisites

**macOS:**

```bash
# Cross-compiler
brew tap messense/macos-cross-toolchains
brew install aarch64-unknown-linux-gnu

# Emulator + tools
brew install qemu mtools dosfstools
```

**Linux (Debian/Ubuntu):**

```bash
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
                 binutils-aarch64-linux-gnu qemu-system-aarch64 \
                 cmake mtools dosfstools
```

### Build & Run

```bash
# Build kernel
make build

# Run in QEMU
make run
```

That's it. You'll see the skull boot logo and a `hackers-os>` prompt.

### With disk image and networking

```bash
# Build companion apps + create FAT32 disk
make apps
make disk-image

# Run with disk + network
qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M -nographic \
  -kernel build/kernel.bin \
  -drive file=disk.img,if=none,format=raw,id=hd0 \
  -device virtio-blk-device,drive=hd0 \
  -device virtio-net-device,netdev=net0 \
  -netdev user,id=net0
```

### Docker (no toolchain needed)

```bash
docker build -t hackers-os .
docker run --rm -it hackers-os
```

## Bundled Applications

The OS includes cross-compiled companion tools as static AArch64 ELF binaries:

| App | Binary | Description | Size |
|-----|--------|-------------|------|
| [network-scanner](../network-scanner) | `network-analyzer` | Network sweep/scanner (ICMP, TCP probes) | 17MB |
| [dnsutils](../dnsutils) | `securitydns` | DNS reconnaissance, subdomain discovery | 10MB |
| [jwt_inspector](../jwt_inspector) | `jwt_inspector` | JWT analysis, HMAC brute-force | 16MB |

```bash
# Build all companion apps for aarch64
make apps

# Create disk image with apps, then boot
make run-disk
```

In the shell, use `run <binary>` to execute them from the FAT32 disk image.

## Shell Commands

```
hackers-os> help
  help          - Show this help
  clear         - Clear screen
  mem           - Show memory usage
  ps            - List processes
  uptime        - Show system uptime
  ls [path]     - List directory
  cat <file>    - Print file contents
  run <file>    - Execute ELF binary
  ifconfig      - Show network config
  ping <ip>     - Ping an IP address
  nslookup <h>  - DNS lookup
  info          - System information
  reboot        - Reboot system
```

## Build Targets

| Target | Description |
|--------|-------------|
| `make build` | Build kernel (`build/kernel.bin` + `build/kernel8.img`) |
| `make run` | Run in QEMU (serial console) |
| `make run-disk` | Run in QEMU with FAT32 disk image |
| `make debug` | Run in QEMU with GDB server on `:1234` |
| `make clean` | Remove build directory |
| `make apps` | Cross-compile companion apps for aarch64 |
| `make disk-image` | Create FAT32 disk image with apps |
| `make install-deps` | Install cross-compiler and QEMU (macOS) |

## Releases

Each [GitHub Release](../../releases) includes:

| Artifact | Description |
|----------|-------------|
| `hackers-os-qemu.tar.gz` | Kernel binary + run script for QEMU |
| `hackers-os-rpi-sdcard.zip` | Everything needed to boot from Raspberry Pi SD card (kernel8.img, config.txt, RPi firmware) |
| `hackers-os-disk.img` | FAT32 disk image with bundled apps |
| `kernel.bin` | Raw kernel binary |
| `kernel8.img` | Raspberry Pi kernel image |

### Raspberry Pi SD Card

1. Download `hackers-os-rpi-sdcard.zip` from the latest release
2. Format an SD card as FAT32
3. Extract the ZIP to the SD card root
4. Connect a USB-to-serial adapter to GPIO 14 (TX) and GPIO 15 (RX)
5. Open serial terminal at 115200 baud
6. Power on the Pi

### QEMU

```bash
# Download and extract hackers-os-qemu.tar.gz
tar xzf hackers-os-qemu.tar.gz
cd hackers-os-qemu
./run.sh                    # basic boot
./run.sh --disk             # with FAT32 disk image
./run.sh --disk --net       # with disk + networking
```

## Architecture

```
start.S (EL3→EL1)
  │
  ▼
kernel_main
  ├── UART::init()           PL011 @ 0x09000000
  ├── Logo::display()        ASCII skull + TLDR;IT
  ├── GIC::init()            GICv2 (GICD 0x08000000, GICC 0x08010000)
  ├── Exception::init()      VBAR_EL1 vector table
  ├── Pages::init()          Bitmap allocator, 4KB pages, 256MB
  ├── MMU::init()            4-level page tables, identity map, caches
  ├── Heap::init()           1MB linked-list allocator
  ├── Process_::init()       Process table (64 slots)
  ├── Scheduler::init()      Round-robin, zombie reaping
  ├── Syscall::init()        Linux AArch64 ABI (~45 syscalls)
  ├── Timer::init()          ARM Generic Timer, 100Hz, INTID 30
  ├── VirtioBlk::init()      Block device (if present)
  ├── FAT32::init()          Filesystem (if disk present)
  ├── Net::init()            DHCP → virtio-net + TCP/IP + DNS
  ├── Socket::init()         BSD socket API
  ├── Shell::init()
  └── Shell::run()           Interactive prompt
```

### Source Layout

```
src/
├── boot/           Entry point (start.S), exception vectors (vectors.S)
├── kernel/         kernel_main, panic handler, type definitions
├── arch/           MMIO helpers, inline asm wrappers (wfi, dsb, isb)
├── uart/           PL011 UART driver + printf-lite
├── logo/           Boot logo
├── interrupts/     GICv2 driver, exception/IRQ dispatch
├── timer/          ARM Generic Timer
├── mm/             Page allocator, MMU, heap (kmalloc/kfree)
├── process/        Process table, scheduler, context switch, EL0 entry
├── syscall/        SVC handler → Linux syscall dispatch
├── compat/         Linux syscall implementations + socket bridge
├── drivers/        virtio-blk MMIO driver
├── fs/             FAT32 filesystem (read + write)
├── net/            virtio-net, Ethernet/ARP/IPv4/ICMP/UDP/TCP, DHCP, DNS, sockets
├── elf/            ELF64 loader
├── shell/          Interactive shell (14 commands)
└── lib/            memset, memcpy, strlen, strcmp, etc.
```

## Linux Syscall Compatibility

The kernel implements ~45 Linux AArch64 syscalls, enabling statically-linked Linux binaries to run:

| Category | Syscalls |
|----------|----------|
| Memory | `mmap`, `munmap`, `brk`, `mprotect`, `madvise` |
| I/O | `read`, `write`, `writev`, `openat`, `close`, `fstat`, `ioctl` |
| Process | `exit`, `exit_group`, `getpid`, `gettid`, `set_tid_address`, `clone` |
| Time | `clock_gettime` (MONOTONIC, REALTIME) |
| Signals | `rt_sigaction`, `rt_sigprocmask`, `sigaltstack` |
| Misc | `uname`, `getrandom`, `futex`, `rseq` |
| Network | `socket`, `bind`, `connect`, `listen`, `accept`, `sendto`, `recvfrom`, `setsockopt`, `getsockopt` |

The kernel reports itself as `Linux 6.1.0-hackers aarch64` via `uname`.

## License

See [LICENSE](LICENSE) for details.
