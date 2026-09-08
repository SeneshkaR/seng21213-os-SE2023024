/* =============================================================================
 * SENG21213-OS :: Round-Robin Scheduler
 * Stage 1 - Lecture L09
 * =============================================================================
 */

#include "scheduler.h"
#include "process.h"
#include "vga.h"
#include "../include/types.h"

/* IRQ0 assembly entry point */
extern void irq0_stub(void);

/* -------------------------------------------------------------------------
 * Port I/O
 * ------------------------------------------------------------------------- */

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;

    __asm__ __volatile__(
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

/* -------------------------------------------------------------------------
 * IDT
 * ------------------------------------------------------------------------- */

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t idt_ptr;

static void idt_set_gate(
    uint8_t number,
    uint32_t handler,
    uint16_t selector,
    uint8_t flags
) {
    idt[number].offset_low = handler & 0xFFFF;
    idt[number].selector = selector;
    idt[number].zero = 0;
    idt[number].flags = flags;
    idt[number].offset_high = (handler >> 16) & 0xFFFF;
}

static void idt_init(void) {
    int i;

    for (i = 0; i < 256; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].flags = 0;
        idt[i].offset_high = 0;
    }

    /*
     * IRQ0 will become interrupt vector 32 (0x20)
     * after remapping the PIC.
     *
     * Code selector from your GDT = 0x08.
     */
    idt_set_gate(
        32,
        (uint32_t)irq0_stub,
        0x08,
        0x8E
    );

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt;

    __asm__ __volatile__(
        "lidt %0"
        :
        : "m"(idt_ptr)
    );
}

/* -------------------------------------------------------------------------
 * 8259 PIC
 * ------------------------------------------------------------------------- */

static void pic_remap(void) {
    uint8_t master_mask = inb(0x21);
    uint8_t slave_mask  = inb(0xA1);

    (void)master_mask;
    (void)slave_mask;

    outb(0x20, 0x11);
    io_wait();

    outb(0xA0, 0x11);
    io_wait();

    /* Master IRQs -> interrupt 0x20-0x27 */
    outb(0x21, 0x20);
    io_wait();

    /* Slave IRQs -> interrupt 0x28-0x2F */
    outb(0xA1, 0x28);
    io_wait();

    outb(0x21, 0x04);
    io_wait();

    outb(0xA1, 0x02);
    io_wait();

    outb(0x21, 0x01);
    io_wait();

    outb(0xA1, 0x01);
    io_wait();

    /*
     * Enable only IRQ0 on master PIC.
     * Keyboard stays polling-based.
     */
    outb(0x21, 0xFE);

    /* Mask all slave IRQs. */
    outb(0xA1, 0xFF);
}

/* -------------------------------------------------------------------------
 * PIT - 100 Hz
 * ------------------------------------------------------------------------- */

static void pit_init(void) {
    uint32_t frequency = 100;
    uint32_t divisor = 1193182 / frequency;

    /* Channel 0, low/high byte, mode 3 */
    outb(0x43, 0x36);

    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

/* -------------------------------------------------------------------------
 * Round-Robin queue
 * ------------------------------------------------------------------------- */

static pcb_t *ready_head = 0;
static pcb_t *current = 0;
static volatile uint32_t system_ticks = 0;

void scheduler_add_process(pcb_t *proc) {
    pcb_t *last;

    if (!proc) {
        return;
    }

    if (!ready_head) {
        ready_head = proc;
        proc->next = proc;
        return;
    }

    last = ready_head;

    while (last->next != ready_head) {
        last = last->next;
    }

    last->next = proc;
    proc->next = ready_head;
}

void scheduler_init(void) {
    ready_head = 0;
    current = 0;

    idt_init();
    pic_remap();
    pit_init();
}

/*
 * Called by irq0_stub every 10 ms.
 */
uint32_t scheduler_irq(uint32_t saved_esp) {
    system_ticks++;

    pcb_t *next;
    pcb_t *start;

    /*
     * Save context of currently running process.
     */
    if (current) {
        current->esp = saved_esp;
        current->total_ticks++;

        if (current->state == PROC_STATE_RUNNING) {
            current->state = PROC_STATE_READY;
        }

        next = current->next;
    } else {
        /*
         * First timer interrupt:
         * kernel_main itself is not a process.
         */
        next = ready_head;
    }

    if (!next) {
        return saved_esp;
    }

    start = next;

    do {
        if (next->state == PROC_STATE_READY) {
            current = next;
            current->state = PROC_STATE_RUNNING;

            proc_set_current(current);

            return current->esp;
        }

        next = next->next;

    } while (next != start);

    /*
     * Nothing runnable.
     */
    if (current) {
        current->state = PROC_STATE_RUNNING;
        return current->esp;
    }

    return saved_esp;
}

void scheduler_start(void) {
    /*
     * Bootloader entered protected mode with interrupts disabled.
     * The IDT, PIC and PIT are now ready, so interrupts can be enabled.
     */
    __asm__ __volatile__("sti");
}

void scheduler_yield(void) {
    /*
     * Use the same interrupt handler as IRQ0.
     */
    __asm__ __volatile__("int $0x20");
}

uint32_t scheduler_ticks(void) {
    return system_ticks;
}

