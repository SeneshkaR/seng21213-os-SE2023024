; =============================================================================
; SENG21213-OS :: IRQ0 Context Switch Stub
; Stage 1 
; =============================================================================

[BITS 32]

[GLOBAL irq0_stub]
[EXTERN scheduler_irq]

irq0_stub:
    ; Save all general-purpose registers.
    pushad

    ; Give the scheduler the current saved ESP.
    push esp
    call scheduler_irq
    add esp, 4

    ; scheduler_irq returns the ESP of the process
    ; that should run next in EAX.
    mov esp, eax

    ; Tell the master PIC that IRQ0 has been handled.
    mov al, 0x20
    out 0x20, al

    ; Restore the selected process registers.
    popad

    ; Restore EIP, CS and EFLAGS.
    iretd
