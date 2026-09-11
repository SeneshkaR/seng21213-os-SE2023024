#ifndef MUTEX_H
#define MUTEX_H

#include "thread.h"

typedef struct mutex {
    int locked;
    thread_t *owner;

    /* Threads blocked waiting for this mutex */
    thread_t *wait_head;
    thread_t *wait_tail;
} mutex_t;

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

#endif
