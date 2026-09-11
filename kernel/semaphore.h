#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "thread.h"

typedef struct semaphore {
    int value;

    /* Threads waiting for this semaphore */
    thread_t *wait_head;
    thread_t *wait_tail;
} semaphore_t;

/* Initialise semaphore with starting count. */
void sem_init(semaphore_t *sem, int value);

/* Decrement count or block if unavailable. */
void sem_wait(semaphore_t *sem);

/* Increment count or wake one blocked thread. */
void sem_signal(semaphore_t *sem);

#endif
