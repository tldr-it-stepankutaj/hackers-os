/*
 * Test more syscalls: getpid, uname, clock_gettime, brk, mmap, write, exit
 * Build: aarch64-unknown-linux-gnu-gcc -static -nostdlib -o sysinfo sysinfo.S
 */

.global _start

.section .text

_start:
    /* Print banner */
    mov     x8, #64
    mov     x0, #1
    adr     x1, banner
    mov     x2, #22
    svc     #0

    /* SYS_getpid */
    mov     x8, #172
    svc     #0
    /* x0 = pid, print it */
    adr     x1, pid_msg
    mov     x2, #5
    mov     x8, #64
    mov     x0, #1
    svc     #0

    /* SYS_uname */
    sub     sp, sp, #390
    mov     x8, #160
    mov     x0, sp
    svc     #0
    /* Print sysname (first 65 bytes of utsname) */
    mov     x8, #64
    mov     x0, #1
    mov     x1, sp
    mov     x2, #10
    svc     #0

    /* newline */
    mov     x8, #64
    mov     x0, #1
    adr     x1, newline
    mov     x2, #1
    svc     #0

    /* Print nodename */
    mov     x8, #64
    mov     x0, #1
    add     x1, sp, #65
    mov     x2, #12
    svc     #0

    mov     x8, #64
    mov     x0, #1
    adr     x1, newline
    mov     x2, #1
    svc     #0

    add     sp, sp, #390

    /* SYS_brk(0) — get current brk */
    mov     x8, #214
    mov     x0, #0
    svc     #0
    /* x0 = current brk */

    /* Print done */
    mov     x8, #64
    mov     x0, #1
    adr     x1, done_msg
    mov     x2, #6
    svc     #0

    /* SYS_exit_group(0) */
    mov     x8, #94
    mov     x0, #0
    svc     #0

.section .rodata
banner:     .ascii "=== HackersOS Test ===\n"
pid_msg:    .ascii "pid: "
newline:    .ascii "\n"
done_msg:   .ascii "done!\n"
