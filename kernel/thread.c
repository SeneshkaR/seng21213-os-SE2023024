/* =============================================================================
 * SENG21213-OS :: Kernel Threads
 * Stage 2 - Lecture L10
 *
 * Kernel threads have their own stacks and CPU contexts, but share the
 * resources/address space of their owning process.
 * =============================================================================
 */

#include "thread.h"
#include "process.h"
#include "scheduler.h"
#include "vga.h"


static thread_t thread_table[MAX_THREADS];

static uint8_t thread_stacks[MAX_THREADS][THREAD_STACK_SIZE];

static uint32_t next_tid = 1;

static thread_t *current_thread = 0;


/* -------------------------------------------------------------------------
 * Find an unused thread table entry.
 * ------------------------------------------------------------------------- */
static thread_t *find_free_thread(void)
{
    int i;

    for (i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].state == THREAD_UNUSED ||
            thread_table[i].state == THREAD_TERMINATED) {
            return &thread_table[i];
        }
    }

    return 0;
}


/* -------------------------------------------------------------------------
 * Thread subsystem initialisation.
 * ------------------------------------------------------------------------- */
void thread_init(void)
{
    int i;

    for (i = 0; i < MAX_THREADS; i++) {
        thread_table[i].tid = 0;
        thread_table[i].state = THREAD_UNUSED;
        thread_table[i].esp = 0;
        thread_table[i].entry = 0;
        thread_table[i].arg = 0;
        thread_table[i].owner = 0;
        thread_table[i].stack_top = 0;
        thread_table[i].next = 0;
    }

    next_tid = 1;
    current_thread = 0;

    vga_puts("[ OK ] Thread subsystem initialized\n");
}


/* -------------------------------------------------------------------------
 * Called if a thread function returns normally.
 * ------------------------------------------------------------------------- */
static void thread_return(void)
{
    thread_exit();

    /* thread_exit should never return. */
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}


/* -------------------------------------------------------------------------
 * Create a kernel thread.
 *
 * Each thread receives its own 4 KB stack.
 * The stack is prepared so that POPAD + IRETD can start execution at fn.
 * ------------------------------------------------------------------------- */
thread_t *thread_create(void (*fn)(void *), void *arg)
{
    thread_t *thread;
    int slot;
    uint32_t *sp;

    if (!fn) {
        return 0;
    }

    thread = find_free_thread();

    if (!thread) {
        vga_puts("[ERROR] No free thread slots\n");
        return 0;
    }

    slot = (int)(thread - thread_table);

    thread->tid = next_tid++;
    thread->state = THREAD_READY;
    thread->entry = fn;
    thread->arg = arg;

    /*
     * The new thread belongs to the process that creates it.
     * All threads of that process therefore share its address space/resources.
     */
    thread->owner = proc_current();

    thread->stack_top =
        &thread_stacks[slot][THREAD_STACK_SIZE];

    sp = (uint32_t *)thread->stack_top;

    /*
     * Normal C stack frame for:
     *
     *     fn(arg)
     *
     * When fn eventually returns, execution reaches thread_return().
     */
    *--sp = (uint32_t)arg;
    *--sp = (uint32_t)thread_return;

    /*
     * IRET frame.
     *
     * This matches the context format already used by proc_create().
     */
    *--sp = 0x202;             /* EFLAGS: interrupts enabled */
    *--sp = 0x08;              /* CS: kernel code segment */
    *--sp = (uint32_t)fn;      /* EIP */

    /*
     * Register frame consumed by POPAD in switch.asm.
     */
    *--sp = 0;                 /* EAX */
    *--sp = 0;                 /* ECX */
    *--sp = 0;                 /* EDX */
    *--sp = 0;                 /* EBX */
    *--sp = 0;                 /* ESP - ignored by POPAD */
    *--sp = 0;                 /* EBP */
    *--sp = 0;                 /* ESI */
    *--sp = 0;                 /* EDI */

    thread->esp = (uint32_t)sp;
    thread->next = 0;

    /* Add the new thread to the scheduler. */
    scheduler_add_thread(thread);

    return thread;
}


/* -------------------------------------------------------------------------
 * Current thread helpers.
 * ------------------------------------------------------------------------- */
thread_t *thread_current(void)
{
    return current_thread;
}


void thread_set_current(thread_t *thread)
{
    current_thread = thread;
}


/* -------------------------------------------------------------------------
 * Terminate the current thread.
 * ------------------------------------------------------------------------- */
void thread_exit(void)
{
    if (!current_thread) {
        return;
    }

    current_thread->state = THREAD_TERMINATED;

    scheduler_yield();

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}


/* -------------------------------------------------------------------------
 * Convert thread state to text.
 * ------------------------------------------------------------------------- */
static const char *thread_state_name(thread_state_t state)
{
    switch (state) {
        case THREAD_UNUSED:
            return "UNUSED";

        case THREAD_READY:
            return "READY";

        case THREAD_RUNNING:
            return "RUNNING";

        case THREAD_BLOCKED:
            return "BLOCKED";

        case THREAD_TERMINATED:
            return "TERMINATED";

        default:
            return "UNKNOWN";
    }
}


/* -------------------------------------------------------------------------
 * Display thread table.
 * ------------------------------------------------------------------------- */
void thread_list(void)
{
    int i;

    vga_puts("\nTID   STATE        OWNER\n");
    vga_puts("---------------------------\n");

    for (i = 0; i < MAX_THREADS; i++) {

        if (thread_table[i].tid != 0) {

            vga_printf(
                "%d     %s",
                thread_table[i].tid,
                thread_state_name(thread_table[i].state)
            );

            if (thread_table[i].owner) {
                vga_printf(
                    "     PID %d\n",
                    thread_table[i].owner->pid
                );
            } else {
                vga_puts("     KERNEL\n");
            }
        }
    }
}
