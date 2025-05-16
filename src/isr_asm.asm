; isr_asm.asm
bits 32

; Define all ISRs
%macro ISR_NO_ERR 1
global isr%1
isr%1:
    cli                 ; Disable interrupts
    push byte 0         ; Push a dummy error code
    push byte %1        ; Push the interrupt number
    jmp isr_common_stub ; Go to the common handler
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    cli                 ; Disable interrupts
    push byte %1        ; Push the interrupt number
    jmp isr_common_stub ; Go to the common handler
%endmacro

; Define IRQs
%macro IRQ 2
global irq%1
irq%1:
    cli                 ; Disable interrupts
    push byte 0         ; Push a dummy error code
    push byte %2        ; Push the interrupt number (32 + IRQ number)
    jmp irq_common_stub ; Go to the common handler
%endmacro

; Define ISRs
ISR_NO_ERR 0  ; Division by zero
ISR_NO_ERR 1  ; Debug
ISR_NO_ERR 2  ; Non-maskable interrupt
ISR_NO_ERR 3  ; Breakpoint
ISR_NO_ERR 4  ; Overflow
ISR_NO_ERR 5  ; Bound range exceeded
ISR_NO_ERR 6  ; Invalid opcode
ISR_NO_ERR 7  ; Device not available
ISR_ERR    8  ; Double fault
ISR_NO_ERR 9  ; Coprocessor segment overrun
ISR_ERR    10 ; Invalid TSS
ISR_ERR    11 ; Segment not present
ISR_ERR    12 ; Stack-segment fault
ISR_ERR    13 ; General protection fault
ISR_ERR    14 ; Page fault
ISR_NO_ERR 15 ; Reserved
ISR_NO_ERR 16 ; x87 floating-point exception
ISR_ERR    17 ; Alignment check
ISR_NO_ERR 18 ; Machine check
ISR_NO_ERR 19 ; SIMD floating-point exception
ISR_NO_ERR 20 ; Virtualization exception
ISR_NO_ERR 21 ; Reserved
ISR_NO_ERR 22 ; Reserved
ISR_NO_ERR 23 ; Reserved
ISR_NO_ERR 24 ; Reserved
ISR_NO_ERR 25 ; Reserved
ISR_NO_ERR 26 ; Reserved
ISR_NO_ERR 27 ; Reserved
ISR_NO_ERR 28 ; Reserved
ISR_NO_ERR 29 ; Reserved
ISR_NO_ERR 30 ; Reserved
ISR_NO_ERR 31 ; Reserved

; Define IRQs
IRQ 0, 32   ; Timer
IRQ 1, 33   ; Keyboard
IRQ 2, 34   ; Cascade for slave PIC
IRQ 3, 35   ; COM2
IRQ 4, 36   ; COM1
IRQ 5, 37   ; LPT2
IRQ 6, 38   ; Floppy disk
IRQ 7, 39   ; LPT1
IRQ 8, 40   ; CMOS real-time clock
IRQ 9, 41   ; Free for peripherals
IRQ 10, 42  ; Free for peripherals
IRQ 11, 43  ; Free for peripherals
IRQ 12, 44  ; PS/2 mouse
IRQ 13, 45  ; FPU/coprocessor
IRQ 14, 46  ; Primary ATA hard disk
IRQ 15, 47  ; Secondary ATA hard disk

; C function declarations
extern isr_handler
extern irq_handler

; ISR common stub
isr_common_stub:
    pusha           ; Pushes edi, esi, ebp, esp, ebx, edx, ecx, eax

    mov ax, ds      ; Save the data segment descriptor
    push eax

    mov ax, 0x10    ; Load the kernel data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call isr_handler

    pop eax         ; Reload the original data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa            ; Pops edi, esi, ebp, esp, ebx, edx, ecx, eax
    add esp, 8      ; Cleans up the pushed error code and ISR number
    sti             ; Enable interrupts
    iret            ; Pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP

; IRQ common stub
irq_common_stub:
    pusha           ; Pushes edi, esi, ebp, esp, ebx, edx, ecx, eax

    mov ax, ds      ; Save the data segment descriptor
    push eax

    mov ax, 0x10    ; Load the kernel data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call irq_handler

    pop eax         ; Reload the original data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa            ; Pops edi, esi, ebp, esp, ebx, edx, ecx, eax
    add esp, 8      ; Cleans up the pushed error code and IRQ number
    sti             ; Enable interrupts
    iret            ; Pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP