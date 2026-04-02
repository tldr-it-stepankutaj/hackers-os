/*
 * enter_usermode(entry_point, user_stack, page_table)
 *   x0 = user entry point (ELR_EL1)
 *   x1 = user stack pointer (SP_EL0)
 *   x2 = page table base (TTBR0_EL1)
 *
 * Drops from EL1 to EL0 by setting up SPSR/ELR and doing eret.
 * When the user process calls exit(), the syscall handler modifies
 * the exception frame to return to kernel_return_from_user instead
 * of back to EL0.
 */

.global enter_usermode
.global kernel_return_from_user
.global kernel_return_addr
.global kernel_return_sp

/* Saved kernel state for returning after user process exits */
.section .bss
.balign 8
kernel_return_sp:   .skip 8
kernel_return_addr: .skip 8

.section .text

enter_usermode:
    /* Save kernel return context */
    adr     x3, kernel_return_from_user
    adr     x4, kernel_return_addr
    str     x3, [x4]
    adr     x4, kernel_return_sp
    mov     x3, sp
    str     x3, [x4]

    /* Save callee-saved registers on kernel stack */
    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!
    stp     x27, x28, [sp, #-16]!
    stp     x29, x30, [sp, #-16]!

    /* Update saved SP to include pushed regs */
    adr     x4, kernel_return_sp
    mov     x3, sp
    str     x3, [x4]

    /* Switch page table */
    msr     ttbr0_el1, x2
    tlbi    vmalle1
    dsb     sy
    isb

    /* Set SP_EL0 (user stack) */
    msr     sp_el0, x1

    /* Set ELR_EL1 = entry point */
    msr     elr_el1, x0

    /* Set SPSR_EL1: EL0t, interrupts enabled */
    mov     x0, #0
    msr     spsr_el1, x0

    /* Clear registers for clean user state */
    mov     x0, xzr
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    mov     x4, xzr
    mov     x5, xzr
    mov     x6, xzr
    mov     x7, xzr
    mov     x8, xzr
    mov     x9, xzr
    mov     x10, xzr
    mov     x11, xzr
    mov     x12, xzr
    mov     x13, xzr
    mov     x14, xzr
    mov     x15, xzr
    mov     x16, xzr
    mov     x17, xzr
    mov     x18, xzr
    mov     x19, xzr
    mov     x20, xzr
    mov     x21, xzr
    mov     x22, xzr
    mov     x23, xzr
    mov     x24, xzr
    mov     x25, xzr
    mov     x26, xzr
    mov     x27, xzr
    mov     x28, xzr
    mov     x29, xzr
    mov     x30, xzr

    eret

/*
 * kernel_return_from_user:
 * The exit syscall handler sets ELR_EL1 to this address and
 * SPSR_EL1 to EL1h so that eret returns here in kernel mode.
 */
kernel_return_from_user:
    /* Restore kernel page table */
    /* (MMU::init stored kernel pgd at a known address) */

    /* Restore callee-saved registers */
    adr     x4, kernel_return_sp
    ldr     x3, [x4]
    mov     sp, x3

    ldp     x29, x30, [sp], #16
    ldp     x27, x28, [sp], #16
    ldp     x25, x26, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16

    /* Return to caller of enter_usermode (shell cmd_run) */
    ret
