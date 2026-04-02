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
- **Processes** — Process table (64 slots), round-robin scheduler, context switch
- **Syscalls** — SVC-based EL0→EL1 trap, Linux AArch64 syscall ABI compatible (~45 syscalls)
- **Filesystem** — FAT32 read-only (directory listing, file read, path resolution) via virtio-blk
- **ELF Loader** — Loads and executes AArch64 ELF64 binaries in usermode (EL0)
- **Networking** — Full TCP/IP stack: virtio-net driver, Ethernet, ARP, IPv4, ICMP, UDP, TCP (16 connections), BSD socket API
- **Linux Compat** — mmap, brk, writev, openat, fstat, ioctl, clock_gettime, uname, getrandom, rt_sigaction, futex, and more — enables running statically-linked Linux binaries
- **Shell** — Interactive command line: `help`, `mem`, `ps`, `uptime`, `ls`, `cat`, `run`, `ifconfig`, `ping`, `info`, `clear`, `reboot`
- **Boot Logo** — ASCII skull with TLDR;IT branding

## Quick Start

### Prerequisites

**macOS:**

```bash
# Cross-compiler
brew tap messense/macos-cross-toolchains
brew install aarch64-unknown-linux-gnu

# Emulator
brew install qemu
```

**Linux (Debian/Ubuntu):**

```bash
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
                 binutils-aarch64-linux-gnu qemu-system-aarch64 cmake
```

### Build & Run

```bash
# Build
make build

# Run in QEMU
make run
```

That's it. You'll see the skull boot logo and a `hackers-os>` prompt.

### Docker (no toolchain needed)

```bash
docker build -t hackers-os .
docker run --rm -it hackers-os
```

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
  info          - System information
  reboot        - Reboot system
```

## Build Targets

| Target | Description |
|--------|-------------|
| `make build` | Build kernel (`build/kernel.bin`) |
| `make run` | Run in QEMU (serial console) |
| `make run-disk` | Run in QEMU with FAT32 disk image |
| `make debug` | Run in QEMU with GDB server on `:1234` |
| `make clean` | Remove build directory |
| `make disk-image` | Create FAT32 disk image with apps |
| `make install-deps` | Install cross-compiler and QEMU (macOS) |

## QEMU Options

```bash
# Basic boot (serial console)
qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M -nographic \
  -kernel build/kernel.bin

# With FAT32 disk image (enables ls, cat, run commands)
qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M -nographic \
  -kernel build/kernel.bin \
  -drive file=disk.img,if=none,format=raw,id=hd0 \
  -device virtio-blk-device,drive=hd0

# With networking (enables ifconfig, ping, socket API)
qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M -nographic \
  -kernel build/kernel.bin \
  -device virtio-net-device,netdev=net0 \
  -netdev user,id=net0

# With both disk and network
qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M -nographic \
  -kernel build/kernel.bin \
  -drive file=disk.img,if=none,format=raw,id=hd0 \
  -device virtio-blk-device,drive=hd0 \
  -device virtio-net-device,netdev=net0 \
  -netdev user,id=net0

# GDB debugging
qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M -nographic \
  -kernel build/kernel.bin -s -S
# Then: aarch64-linux-gnu-gdb -ex 'target remote :1234' build/kernel.elf
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
  ├── Scheduler::init()      Round-robin
  ├── Syscall::init()        Linux AArch64 ABI (~45 syscalls)
  ├── Timer::init()          ARM Generic Timer, 100Hz, INTID 30
  ├── VirtioBlk::init()      Block device (if present)
  ├── FAT32::init()          Filesystem (if disk present)
  ├── Net::init()            virtio-net + TCP/IP stack (if NIC present)
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
├── process/        Process table, scheduler, context switch (context.S)
├── syscall/        SVC handler → Linux syscall dispatch
├── compat/         Linux syscall implementations + socket bridge
├── drivers/        virtio-blk MMIO driver
├── fs/             FAT32 read-only filesystem
├── net/            virtio-net, Ethernet/ARP/IPv4/ICMP/UDP/TCP, socket API
├── elf/            ELF64 loader
├── shell/          Interactive shell (commands)
└── lib/            memset, memcpy, strlen, strcmp, etc.
```

## Raspberry Pi

The build produces `build/kernel8.img` alongside `kernel.bin`. To boot on a Raspberry Pi 3/4/5:

1. Format an SD card as FAT32
2. Copy Raspberry Pi firmware files (`bootcode.bin`, `start.elf`, `fixup.dat`) from the [official repo](https://github.com/raspberrypi/firmware/tree/master/boot)
3. Copy `build/kernel8.img` to the SD card root
4. Add `config.txt`:
   ```
   arm_64bit=1
   kernel=kernel8.img
   ```
5. Connect a USB-to-serial adapter to GPIO 14 (TX) and GPIO 15 (RX)
6. Open serial terminal at 115200 baud

> **Note:** The kernel loads at `0x40000000` (QEMU virt). Raspberry Pi loads `kernel8.img` at `0x80000` by default. A separate RPi-specific linker script may be needed for full RPi compatibility.

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
