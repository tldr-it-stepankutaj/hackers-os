/*
 * Minimal AArch64 Linux "Hello World" using raw syscalls.
 * Tests: SYS_write (64), SYS_exit_group (94)
 *
 * Build: aarch64-unknown-linux-gnu-gcc -static -nostdlib -o hello hello.S
 */

.global _start

.section .text

_start:
    /* SYS_write(1, msg, 14) */
    mov     x8, #64         /* SYS_write */
    mov     x0, #1          /* fd = stdout */
    adr     x1, msg         /* buf = &msg */
    mov     x2, #14         /* len = 14 */
    svc     #0

    /* SYS_exit_group(0) */
    mov     x8, #94         /* SYS_exit_group */
    mov     x0, #0          /* code = 0 */
    svc     #0

.section .rodata
msg:
    .ascii "Hello, World!\n"
