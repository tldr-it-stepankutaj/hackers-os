.section .text.boot
.global _start

_start:
    /* Only let CPU 0 through, park others */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF
    cbz     x0, .Lprimary_cpu
.Lpark:
    wfe
    b       .Lpark

.Lprimary_cpu:
    /* Check current exception level */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    cmp     x0, #2
    b.eq    .Lfrom_el2
    cmp     x0, #3
    b.eq    .Lfrom_el3
    b       .Lat_el1

.Lfrom_el3:
    /* EL3 -> EL2: set up SCR_EL3 */
    mrs     x0, scr_el3
    orr     x0, x0, #(1 << 10)     /* RW: EL2 is AArch64 */
    orr     x0, x0, #(1 << 0)      /* NS: non-secure */
    msr     scr_el3, x0

    mov     x0, #0x3c9              /* SPSR: EL2h, DAIF masked */
    msr     spsr_el3, x0
    adr     x0, .Lfrom_el2
    msr     elr_el3, x0
    eret

.Lfrom_el2:
    /* EL2 -> EL1: configure HCR_EL2 */
    mov     x0, #(1 << 31)         /* RW: EL1 is AArch64 */
    msr     hcr_el2, x0

    /* Enable CNTP for EL1 */
    mrs     x0, cnthctl_el2
    orr     x0, x0, #3             /* EL1PCEN, EL1PCTEN */
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr

    /* SCTLR_EL1: clean state */
    mov     x0, xzr
    msr     sctlr_el1, x0

    /* SPSR: EL1h, DAIF masked */
    mov     x0, #0x3c5
    msr     spsr_el2, x0
    adr     x0, .Lat_el1
    msr     elr_el2, x0
    eret

.Lat_el1:
    /* Zero BSS */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
.Lbss_loop:
    cmp     x0, x1
    b.ge    .Lbss_done
    str     xzr, [x0], #8
    b       .Lbss_loop
.Lbss_done:

    /* Set stack pointer */
    ldr     x0, =__stack_top
    mov     sp, x0

    /* Jump to C++ kernel */
    bl      kernel_main

    /* Should not return, but halt if it does */
.Lhalt:
    wfi
    b       .Lhalt
