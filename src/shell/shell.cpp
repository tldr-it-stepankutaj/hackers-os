#include "shell.h"
#include "../uart/uart.h"
#include "../process/process.h"
#include "../process/scheduler.h"
#include "../mm/pages.h"
#include "../mm/heap.h"
#include "../timer/timer.h"
#include "../fs/fat32.h"
#include "../elf/elf.h"
#include "../net/net.h"

static constexpr u32 CMD_BUF_SIZE = 256;
static constexpr u32 MAX_ARGS = 16;

static char cmd_buf[CMD_BUF_SIZE];
static u32 cmd_pos = 0;

static void print_prompt() {
    UART::puts("\x1b[32mhackers-os\x1b[0m> ");
}

static void cmd_help() {
    UART::puts("Available commands:\n");
    UART::puts("  help          - Show this help\n");
    UART::puts("  clear         - Clear screen\n");
    UART::puts("  mem           - Show memory usage\n");
    UART::puts("  ps            - List processes\n");
    UART::puts("  uptime        - Show system uptime\n");
    UART::puts("  ls [path]     - List directory\n");
    UART::puts("  cat <file>    - Print file contents\n");
    UART::puts("  run <file>    - Execute ELF binary\n");
    UART::puts("  ifconfig      - Show network config\n");
    UART::puts("  ping <ip>     - Ping an IP address\n");
    UART::puts("  info          - System information\n");
    UART::puts("  reboot        - Reboot system\n");
}

static void cmd_clear() {
    UART::puts("\x1b[2J\x1b[H");
}

static void cmd_mem() {
    u64 free_pages = Pages::get_free_count();
    u64 total_pages = Pages::get_total_count();
    u64 used_pages = total_pages - free_pages;

    UART::puts("Memory:\n");
    UART::printf("  Pages: %u / %u used (%u KB / %u KB)\n",
                 used_pages, total_pages,
                 used_pages * 4, total_pages * 4);
    UART::printf("  Heap:  %u bytes used, %u bytes free\n",
                 Heap::get_used(), Heap::get_free());
}

static void cmd_ps() {
    UART::puts("PID  STATE    NAME\n");
    UART::puts("---  -------  ----\n");
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        Process *p = Process_::get(i);
        if (!p) continue;

        const char *state = "???    ";
        switch (p->state) {
            case ProcessState::RUNNING: state = "RUNNING"; break;
            case ProcessState::READY:   state = "READY  "; break;
            case ProcessState::BLOCKED: state = "BLOCKED"; break;
            case ProcessState::ZOMBIE:  state = "ZOMBIE "; break;
            default: break;
        }
        UART::printf("%u    %s  %s\n", (u64)p->pid, state, p->name);
    }
}

static void cmd_uptime() {
    u64 secs = Timer::uptime_seconds();
    u64 mins = secs / 60;
    u64 hours = mins / 60;
    UART::printf("Uptime: %u:%u:%u (%u ticks)\n",
                 hours, mins % 60, secs % 60, Timer::get_ticks());
}

static void cmd_ls(const char *path) {
    if (!path || path[0] == '\0') path = "/";

    Fat32DirEntry entries[32];
    u32 count = 0;

    if (!FAT32::list_dir(path, entries, 32, &count)) {
        UART::printf("Cannot list directory: %s\n", path);
        return;
    }

    for (u32 i = 0; i < count; i++) {
        if (entries[i].is_dir) {
            UART::printf("  [DIR] %s\n", entries[i].name);
        } else {
            UART::printf("  %u\t%s\n", (u64)entries[i].size, entries[i].name);
        }
    }
    UART::printf("%u entries\n", (u64)count);
}

static void cmd_cat(const char *path) {
    if (!path || path[0] == '\0') {
        UART::puts("Usage: cat <file>\n");
        return;
    }

    u32 fsize = FAT32::file_size(path);
    if (fsize == 0) {
        UART::printf("File not found: %s\n", path);
        return;
    }

    // Limit to 64KB for display
    if (fsize > 65536) fsize = 65536;

    void *buf = Heap::kmalloc(fsize + 1);
    if (!buf) {
        UART::puts("Out of memory\n");
        return;
    }

    u32 bytes_read;
    if (FAT32::read_file(path, buf, fsize, &bytes_read)) {
        static_cast<char*>(buf)[bytes_read] = '\0';
        UART::puts(static_cast<const char*>(buf));
        UART::putc('\n');
    } else {
        UART::printf("Error reading: %s\n", path);
    }

    Heap::kfree(buf);
}

static void cmd_run(const char *path) {
    if (!path || path[0] == '\0') {
        UART::puts("Usage: run <file>\n");
        return;
    }

    u32 pid = ELF::load_and_exec(path, path);
    if (pid) {
        UART::printf("Started process %u\n", (u64)pid);
    } else {
        UART::printf("Failed to run: %s\n", path);
    }
}

static void cmd_info() {
    UART::puts("\n");
    UART::puts("  Hackers-OS v0.1\n");
    UART::puts("  Architecture: AArch64 (ARM64)\n");
    UART::printf("  Processes: %u / %u\n", (u64)Process_::count(), (u64)MAX_PROCESSES);
    UART::printf("  Timer: %u Hz\n", (u64)100);
    UART::puts("  UART: PL011 @ 0x09000000\n");
    UART::puts("  GIC: GICv2\n");
    UART::puts("\n");
    UART::puts("  Developed by TLDR;IT\n");
    UART::puts("  https://www.tldr-it.com\n");
    UART::puts("\n");
}

static void cmd_reboot() {
    UART::puts("Rebooting via PSCI...\n");
    // PSCI SYSTEM_RESET via HVC
    asm volatile(
        "ldr x0, =0x84000009\n"  // PSCI SYSTEM_RESET
        "hvc #0\n"
    );
}

static void cmd_ifconfig() {
    IPv4Addr ip = Net::get_ip();
    IPv4Addr gw = Net::get_gateway();
    MacAddr mac = Net::get_mac();

    UART::puts("eth0:\n");
    UART::printf("  MAC:     %x:%x:%x:%x:%x:%x\n",
                 (u64)mac.bytes[0], (u64)mac.bytes[1], (u64)mac.bytes[2],
                 (u64)mac.bytes[3], (u64)mac.bytes[4], (u64)mac.bytes[5]);
    UART::printf("  IPv4:    %u.%u.%u.%u\n",
                 (u64)(ip.addr & 0xFF), (u64)((ip.addr >> 8) & 0xFF),
                 (u64)((ip.addr >> 16) & 0xFF), (u64)((ip.addr >> 24) & 0xFF));
    UART::printf("  Gateway: %u.%u.%u.%u\n",
                 (u64)(gw.addr & 0xFF), (u64)((gw.addr >> 8) & 0xFF),
                 (u64)((gw.addr >> 16) & 0xFF), (u64)((gw.addr >> 24) & 0xFF));
}

static u8 parse_ip_octet(const char *&s) {
    u8 val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    if (*s == '.') s++;
    return val;
}

static void cmd_ping(const char *target) {
    if (!target) {
        UART::puts("Usage: ping <ip>\n");
        return;
    }

    const char *s = target;
    u8 a = parse_ip_octet(s);
    u8 b = parse_ip_octet(s);
    u8 c = parse_ip_octet(s);
    u8 d = parse_ip_octet(s);
    IPv4Addr dst = make_ip(a, b, c, d);

    UART::printf("PING %u.%u.%u.%u\n", (u64)a, (u64)b, (u64)c, (u64)d);

    // Resolve MAC first
    MacAddr dst_mac = Net::arp_resolve(
        ((dst.addr & Net::get_gateway().addr) == (Net::get_ip().addr & Net::get_gateway().addr))
        ? dst : Net::get_gateway());

    if (dst_mac == MAC_ZERO) {
        UART::puts("ARP resolution failed (no network?)\n");
        return;
    }

    for (int seq = 0; seq < 4; seq++) {
        // Build ICMP echo request
        IcmpHeader icmp;
        icmp.type = ICMP_ECHO_REQUEST;
        icmp.code = 0;
        icmp.identifier = htons(0x1234);
        icmp.sequence = htons(seq);
        icmp.checksum = 0;
        icmp.checksum = inet_checksum(&icmp, sizeof(icmp));

        u64 start_tick = Timer::get_ticks();
        Net::send_ip(dst, IP_PROTO_ICMP, &icmp, sizeof(icmp));

        // Wait for reply (poll)
        bool got_reply = false;
        while (Timer::get_ticks() - start_tick < 200) {  // 2 sec timeout
            Net::poll();
            // We'd need a callback for ICMP replies, simplified: just wait
        }

        if (!got_reply) {
            UART::printf("  seq=%u: sent (reply check not implemented yet)\n", (u64)seq);
        }
    }
}

static void process_command() {
    cmd_buf[cmd_pos] = '\0';

    // Skip empty commands
    char *cmd = cmd_buf;
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;

    // Split command and first argument
    char *arg = cmd;
    while (*arg && *arg != ' ') arg++;
    if (*arg == ' ') {
        *arg++ = '\0';
        while (*arg == ' ') arg++;
    }
    if (*arg == '\0') arg = nullptr;

    if (strcmp(cmd, "help") == 0) cmd_help();
    else if (strcmp(cmd, "clear") == 0) cmd_clear();
    else if (strcmp(cmd, "mem") == 0) cmd_mem();
    else if (strcmp(cmd, "ps") == 0) cmd_ps();
    else if (strcmp(cmd, "uptime") == 0) cmd_uptime();
    else if (strcmp(cmd, "ls") == 0) cmd_ls(arg);
    else if (strcmp(cmd, "cat") == 0) cmd_cat(arg);
    else if (strcmp(cmd, "run") == 0) cmd_run(arg);
    else if (strcmp(cmd, "ifconfig") == 0) cmd_ifconfig();
    else if (strcmp(cmd, "ping") == 0) cmd_ping(arg);
    else if (strcmp(cmd, "info") == 0) cmd_info();
    else if (strcmp(cmd, "reboot") == 0) cmd_reboot();
    else UART::printf("Unknown command: %s (type 'help' for commands)\n", cmd);
}

namespace Shell {

void init() {
    UART::puts("  [ok] Shell ready\n");
}

void run() {
    UART::puts("\nType 'help' for available commands.\n\n");
    print_prompt();

    while (true) {
        char c = UART::getc();

        if (c == '\r' || c == '\n') {
            UART::puts("\r\n");
            process_command();
            cmd_pos = 0;
            print_prompt();
        } else if (c == 127 || c == '\b') {
            // Backspace
            if (cmd_pos > 0) {
                cmd_pos--;
                UART::puts("\b \b");
            }
        } else if (c == 3) {
            // Ctrl+C
            UART::puts("^C\r\n");
            cmd_pos = 0;
            print_prompt();
        } else if (c >= 32 && cmd_pos < CMD_BUF_SIZE - 1) {
            cmd_buf[cmd_pos++] = c;
            UART::putc(c);
        }
    }
}

} // namespace Shell
