#ifndef PROCESS_H
#define PROCESS_H

#include "../include/types.h"

#define MAX_PROCESSES 16
#define PROCESS_STACK_SIZE 4096

typedef enum {
    PROC_STATE_READY,
    PROC_STATE_RUNNING,
    PROC_STATE_BLOCKED,
    PROC_STATE_ZOMBIE
} proc_state_t;

typedef struct pcb {
    uint32_t pid;
    proc_state_t state;

    char name[32];

    uint32_t esp;
    void *stack_top;

    void (*entry)(void);

    uint32_t total_ticks;

    struct pcb *next;
} pcb_t;

void proc_init(void);

pcb_t *proc_create(const char *name, void (*entry)(void));

pcb_t *proc_find(uint32_t pid);

pcb_t *proc_current(void);

void proc_set_current(pcb_t *proc);

void proc_exit(void);

void proc_list(void);

#endif
