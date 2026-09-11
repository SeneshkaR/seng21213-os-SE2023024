/* =============================================================================
 * SENG21213-OS :: Counting Semaphore
 * Stage 2 - Lecture L10 §5
 *
 * Implements a counting semaphore with a FIFO wait queue.
 * - sem_wait()   : decrement count, or block if count == 0
 * - sem_signal() : increment count, or wake one blocked thread
 *
 * Used for producer-consumer coordination and resource counting.
 * Reference: Stallings Ch.4 — Semaphores and Monitors
 * =============================================================================
 */

#include "semaphore.h"
#include "scheduler.h"

void sem_init(semaphore_t *sem, int value)
{
    if (!sem) {
        return;
    }

    sem->value = value;
    sem->wait_head = 0;
    sem->wait_tail = 0;
}

void sem_wait(semaphore_t *sem)
{
    thread_t *current;

    if (!sem) {
        return;
    }

    current = thread_current();

    if (!current) {
        return;
    }

    /*
     * Resource available.
     */
    if (sem->value > 0) {
        sem->value--;
        return;
    }

    /*
     * No resource available.
     * Put current thread into semaphore waiting queue.
     */
    current->wait_next = 0;

    if (!sem->wait_head) {
        sem->wait_head = current;
        sem->wait_tail = current;
    } else {
        sem->wait_tail->wait_next = current;
        sem->wait_tail = current;
    }

    /*
     * Block until another thread calls sem_signal().
     */
    scheduler_block_thread(current);
    scheduler_yield();

    /*
     * When this thread becomes runnable again,
     * sem_signal() has effectively granted it one resource.
     */
}

void sem_signal(semaphore_t *sem)
{
    thread_t *next;

    if (!sem) {
        return;
    }

    /*
     * If threads are waiting, wake one directly.
     */
    if (sem->wait_head) {
        next = sem->wait_head;
        sem->wait_head = next->wait_next;

        if (!sem->wait_head) {
            sem->wait_tail = 0;
        }

        next->wait_next = 0;

        scheduler_unblock_thread(next);

        return;
    }

    /*
     * Nobody waiting -> increase available count.
     */
    sem->value++;
}
