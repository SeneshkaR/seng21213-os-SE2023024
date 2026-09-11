#include "mutex.h"
#include "scheduler.h"

void mutex_init(mutex_t *mutex)
{
    if (!mutex) {
        return;
    }

    mutex->locked = 0;
    mutex->owner = 0;
    mutex->wait_head = 0;
    mutex->wait_tail = 0;
}

void mutex_lock(mutex_t *mutex)
{
    thread_t *current;

    if (!mutex) {
        return;
    }

    current = thread_current();

    if (!current) {
        return;
    }

    /*
     * Mutex is free.
     */
    if (!mutex->locked) {
        mutex->locked = 1;
        mutex->owner = current;
        return;
    }

    /*
     * Current thread already owns it.
     */
    if (mutex->owner == current) {
        return;
    }

    /*
     * Add current thread to waiting queue.
     */
    current->wait_next = 0;

    if (!mutex->wait_head) {
        mutex->wait_head = current;
        mutex->wait_tail = current;
    } else {
        mutex->wait_tail->wait_next = current;
        mutex->wait_tail = current;
    }

    /*
     * Block current thread and switch away.
     */
    scheduler_block_thread(current);
    scheduler_yield();

    /*
     * When this thread runs again,
     * mutex_unlock() has transferred ownership to it.
     */
}

void mutex_unlock(mutex_t *mutex)
{
    thread_t *next;

    if (!mutex) {
        return;
    }

    if (!mutex->locked) {
        return;
    }

    /*
     * Only the owner can unlock.
     */
    if (mutex->owner != thread_current()) {
        return;
    }

    /*
     * No waiter: mutex becomes free.
     */
    if (!mutex->wait_head) {
        mutex->locked = 0;
        mutex->owner = 0;
        return;
    }

    /*
     * Remove first waiting thread.
     */
    next = mutex->wait_head;
    mutex->wait_head = next->wait_next;

    if (!mutex->wait_head) {
        mutex->wait_tail = 0;
    }

    next->wait_next = 0;

    /*
     * Transfer ownership directly.
     */
    mutex->owner = next;
    mutex->locked = 1;

    scheduler_unblock_thread(next);
}
