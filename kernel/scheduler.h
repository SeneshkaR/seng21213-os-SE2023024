#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

/* Initialize scheduler, IDT, PIC and PIT. */
void scheduler_init(void);

/* Add PCB to round-robin queue. */
void scheduler_add_process(pcb_t *proc);

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
