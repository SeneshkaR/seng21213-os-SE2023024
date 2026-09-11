#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"
#include "thread.h"

/* Initialize scheduler, IDT, PIC and PIT. */
void scheduler_init(void);

/* Stage 1: add PCB to process round-robin queue. */
void scheduler_add_process(pcb_t *proc);

/* Stage 2: add thread to thread round-robin queue. */
void scheduler_add_thread(thread_t *thread);

/* Mark a thread blocked/ready.
 * Used later by mutexes and semaphores.
 */
void scheduler_block_thread(thread_t *thread);
void scheduler_unblock_thread(thread_t *thread);

/* Called from irq0_stub.
 * Receives old ESP and returns new ESP.
 */
uint32_t scheduler_irq(uint32_t saved_esp);

/* Enable interrupts and start scheduling. */
void scheduler_start(void);

/* Voluntary scheduling point. */
void scheduler_yield(void);

uint32_t scheduler_ticks(void);

#endif
