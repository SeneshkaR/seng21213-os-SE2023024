/* =============================================================================
 * SENG21213-OS :: Round-Robin Scheduler
 * Stage 2 - Lecture L09
 * =============================================================================
 */

#include "scheduler.h"
#include "process.h"
#include "vga.h"
#include "../include/types.h"
#include "thread.h"

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

/* Stage 1 process run queue */
static pcb_t *ready_head = 0;
static pcb_t *current = 0;

/* Stage 2 thread run queue */
static thread_t *thread_head = 0;
static thread_t *current_sched_thread = 0;

/*
 * What kind of execution context is currently on the CPU?
 *
 * 0 = boot/kernel_main
 * 1 = process
 * 2 = thread
 */
#define SCHED_NONE     0
#define SCHED_PROCESS  1
#define SCHED_THREAD   2

static int current_kind = SCHED_NONE;

/*
 * Used to alternate between processes and threads.
 */
static int prefer_thread = 0;

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

/* -------------------------------------------------------------------------
 * Stage 2 - Add a kernel thread to the round-robin thread queue.
 * ------------------------------------------------------------------------- */
void scheduler_add_thread(thread_t *thread) {
    thread_t *last;

    if (!thread) {
        return;
    }

    if (!thread_head) {
        thread_head = thread;
        thread->next = thread;
        return;
    }

    last = thread_head;

    while (last->next != thread_head) {
        last = last->next;
    }

    last->next = thread;
    thread->next = thread_head;
}

/* -------------------------------------------------------------------------
 * Blocking helpers used by mutexes and semaphores.
 * ------------------------------------------------------------------------- */
void scheduler_block_thread(thread_t *thread)
{
    if (!thread) {
        return;
    }

    thread->state = THREAD_BLOCKED;
}


void scheduler_unblock_thread(thread_t *thread)
{
    if (!thread) {
        return;
    }

    if (thread->state == THREAD_BLOCKED) {
        thread->state = THREAD_READY;
    }
}

void scheduler_init(void)
{
    ready_head = 0;
    current = 0;

    thread_head = 0;
    current_sched_thread = 0;

    current_kind = SCHED_NONE;
    prefer_thread = 0;

    system_ticks = 0;

    idt_init();
    pic_remap();
    pit_init();
}

static pcb_t *find_next_process(void)
{
    pcb_t *next;
    pcb_t *start;

    if (!ready_head) {
        return 0;
    }

    if (current) {
        next = current->next;
    } else {
        next = ready_head;
    }

    if (!next) {
        return 0;
    }

    start = next;

    do {
        if (next->state == PROC_STATE_READY) {
            return next;
        }

        next = next->next;

    } while (next && next != start);

    return 0;
}


static thread_t *find_next_thread(void)
{
    thread_t *next;
    thread_t *start;

    if (!thread_head) {
        return 0;
    }

    if (current_sched_thread) {
        next = current_sched_thread->next;
    } else {
        next = thread_head;
    }

    if (!next) {
        return 0;
    }

    start = next;

    do {
        if (next->state == THREAD_READY) {
            return next;
        }

        next = next->next;

    } while (next && next != start);

    return 0;
}

uint32_t scheduler_irq(uint32_t saved_esp)
{
    pcb_t *next_proc = 0;
    thread_t *next_thread = 0;

    system_ticks++;

    /* -------------------------------------------------------------
     * Save the currently running context.
     * ------------------------------------------------------------- */

    if (current_kind == SCHED_PROCESS && current) {

        current->esp = saved_esp;
        current->total_ticks++;

        if (current->state == PROC_STATE_RUNNING) {
            current->state = PROC_STATE_READY;
        }

    } else if (current_kind == SCHED_THREAD &&
               current_sched_thread) {

        current_sched_thread->esp = saved_esp;

        if (current_sched_thread->state == THREAD_RUNNING) {
            current_sched_thread->state = THREAD_READY;
        }
    }


    /* -------------------------------------------------------------
     * Alternate between processes and threads.
     * ------------------------------------------------------------- */

    if (prefer_thread) {

        next_thread = find_next_thread();

        if (!next_thread) {
            next_proc = find_next_process();
        }

    } else {

        next_proc = find_next_process();

        if (!next_proc) {
            next_thread = find_next_thread();
        }
    }

    prefer_thread = !prefer_thread;


    /* -------------------------------------------------------------
     * Run a thread.
     * ------------------------------------------------------------- */

    if (next_thread) {

        current_sched_thread = next_thread;

        current_sched_thread->state = THREAD_RUNNING;

        current_kind = SCHED_THREAD;

        thread_set_current(current_sched_thread);

        /*
         * The thread shares its owning process.
         * proc_current() therefore refers to that owner while the
         * thread executes.
         */
        proc_set_current(current_sched_thread->owner);

        return current_sched_thread->esp;
    }


    /* -------------------------------------------------------------
     * Run a process.
     * ------------------------------------------------------------- */

    if (next_proc) {

        current = next_proc;

        current->state = PROC_STATE_RUNNING;

        current_kind = SCHED_PROCESS;

        proc_set_current(current);

        /* No kernel thread is executing now. */
        thread_set_current(0);

        return current->esp;
    }


    /*
     * No runnable process/thread was found.
     * Continue the current context.
     */
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

