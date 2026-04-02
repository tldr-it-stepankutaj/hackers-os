/*
 * AArch64 Exception Vector Table
 *
 * Each entry is 128 bytes (0x80 aligned, max 32 instructions).
 * We branch to shared handler stubs to stay within the limit.
 */

.section .text

/* ==========================================
 * Shared save/restore/handler stubs
 * ========================================== */

.macro SAVE_ALL
    sub     sp, sp, #272
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    mrs     x21, elr_el1
    mrs     x22, spsr_el1
    stp     x30, x21, [sp, #240]
    stp     x22, xzr, [sp, #256]
.endm

.macro RESTORE_ALL
    ldp     x30, x21, [sp, #240]
    ldp     x22, x23, [sp, #256]
    msr     elr_el1, x21
    msr     spsr_el1, x22
    ldp     x0, x1, [sp, #0]
    ldp     x2, x3, [sp, #16]
    ldp     x4, x5, [sp, #32]
    ldp     x6, x7, [sp, #48]
    ldp     x8, x9, [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    add     sp, sp, #272
    eret
.endm

/* Exception stub: save regs, call C handler, restore, eret */
_exc_handler_stub:
    SAVE_ALL
    mov     x1, sp          /* x1 = frame pointer */
    bl      handle_exception
    RESTORE_ALL

_irq_handler_stub:
    SAVE_ALL
    mov     x1, sp
    bl      handle_irq
    RESTORE_ALL

_sync_lower_stub:
    SAVE_ALL
    mov     x1, sp
    bl      handle_sync_lower
    RESTORE_ALL

/* ==========================================
 * Vector table — must be 2KB aligned
 * Each entry: set type in x0, branch to stub
 * ========================================== */

.balign 2048
.global exception_vector_table
exception_vector_table:

/* Current EL with SP_EL0 */
.balign 0x80
    mov     x0, #0
    b       _exc_handler_stub
.balign 0x80
    mov     x0, #1
    b       _irq_handler_stub
.balign 0x80
    mov     x0, #2
    b       _exc_handler_stub
.balign 0x80
    mov     x0, #3
    b       _exc_handler_stub

/* Current EL with SP_ELx (kernel mode) */
.balign 0x80
    mov     x0, #4
    b       _exc_handler_stub
.balign 0x80
    mov     x0, #5
    b       _irq_handler_stub
.balign 0x80
    mov     x0, #6
    b       _exc_handler_stub
.balign 0x80
    mov     x0, #7
    b       _exc_handler_stub

/* Lower EL using AArch64 (usermode) */
.balign 0x80
    mov     x0, #8
    b       _sync_lower_stub
.balign 0x80
    mov     x0, #9
    b       _irq_handler_stub
.balign 0x80
    mov     x0, #10
    b       _exc_handler_stub
.balign 0x80
    mov     x0, #11
    b       _exc_handler_stub

/* Lower EL using AArch32 (not supported) */
.balign 0x80
    mov     x0, #12
    b       _exc_handler_stub
.balign 0x80
    mov     x0, #13
    b       _exc_handler_stub
.balign 0x80
    mov     x0, #14
    b       _exc_handler_stub
.balign 0x80
    mov     x0, #15
    b       _exc_handler_stub
