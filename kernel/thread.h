#ifndef THREAD_H
#define THREAD_H

#include "../include/types.h"
#include "process.h"

#define MAX_THREADS 32
#define THREAD_STACK_SIZE 4096

typedef enum {
    THREAD_UNUSED = 0,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

typedef struct thread {
    uint32_t tid;
    thread_state_t state;

    /* Saved CPU stack pointer */
    uint32_t esp;

    /* Thread entry function and argument */
    void (*entry)(void *);
    void *arg;

    /* Process whose address space/resources this thread shares */
    pcb_t *owner;

    void *stack_top;

    struct thread *next;
} thread_t;


/* Initialise the Stage 2 thread subsystem. */
void thread_init(void);

/* Lecture L10: create a kernel thread in the current process. */
thread_t *thread_create(void (*fn)(void *), void *arg);

/* Current thread helpers. */
thread_t *thread_current(void);
void thread_set_current(thread_t *thread);

/* Finish the currently running thread. */
void thread_exit(void);

/* Display the thread table. */
void thread_list(void);

#endif
