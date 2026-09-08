#include "process.h"
#include "scheduler.h"
#include "vga.h"

static pcb_t pcb_table[MAX_PROCESSES];
static pcb_t *current_proc = 0;
static uint32_t next_pid = 1;

/* Temporary Stage 1 stacks.
 * Stage 3 will replace this with proper physical memory allocation.
 */
static uint8_t process_stacks[MAX_PROCESSES][PROCESS_STACK_SIZE];

static pcb_t *find_free_pcb(void) {
    int i;

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (pcb_table[i].pid == 0) {
            return &pcb_table[i];
        }
    }

    return 0;
}

void proc_init(void) {
    int i;

    for (i = 0; i < MAX_PROCESSES; i++) {
        pcb_table[i].pid = 0;
        pcb_table[i].state = PROC_STATE_ZOMBIE;
        pcb_table[i].name[0] = '\0';
        pcb_table[i].esp = 0;
        pcb_table[i].stack_top = 0;
        pcb_table[i].entry = 0;
        pcb_table[i].total_ticks = 0;
        pcb_table[i].next = 0;
    }

    current_proc = 0;
    next_pid = 1;
}

pcb_t *proc_create(const char *name, void (*entry)(void)) {
    pcb_t *proc;
    int slot;
    int i;

    proc = find_free_pcb();

    if (!proc) {
        vga_puts("[ERROR] No free PCB slots\n");
        return 0;
    }

    slot = (int)(proc - pcb_table);

    proc->pid = next_pid++;
    proc->state = PROC_STATE_READY;
    proc->entry = entry;
    proc->total_ticks = 0;
    proc->next = 0;

    proc->stack_top =
    &process_stacks[slot][PROCESS_STACK_SIZE];

/*
 * Prepare the initial CPU context.
 * switch.asm will restore registers with POPAD
 * and start the process with IRETD.
 */
uint32_t *sp = (uint32_t *)proc->stack_top;

/* IRET frame */
*--sp = 0x202;              /* EFLAGS - interrupts enabled */
*--sp = 0x08;               /* CS - kernel code segment */
*--sp = (uint32_t)entry;    /* EIP - process entry function */

/* Register frame expected by POPAD */
*--sp = 0;                  /* EAX */
*--sp = 0;                  /* ECX */
*--sp = 0;                  /* EDX */
*--sp = 0;                  /* EBX */
*--sp = 0;                  /* ESP - ignored by POPAD */
*--sp = 0;                  /* EBP */
*--sp = 0;                  /* ESI */
*--sp = 0;                  /* EDI */

proc->esp = (uint32_t)sp;

    for (i = 0; i < 31 && name[i] != '\0'; i++) {
        proc->name[i] = name[i];
    }

    proc->name[i] = '\0';

    scheduler_add_process(proc);

    return proc;
}

pcb_t *proc_find(uint32_t pid) {
    int i;

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (pcb_table[i].pid == pid) {
            return &pcb_table[i];
        }
    }

    return 0;
}

pcb_t *proc_current(void) {
    return current_proc;
}

void proc_set_current(pcb_t *proc) {
    current_proc = proc;
}

void proc_exit(void) {
    if (!current_proc) {
        return;
    }

    current_proc->state = PROC_STATE_ZOMBIE;

    scheduler_yield();
}

static const char *state_name(proc_state_t state) {
    switch (state) {
        case PROC_STATE_READY:
            return "READY";
        case PROC_STATE_RUNNING:
            return "RUNNING";
        case PROC_STATE_BLOCKED:
            return "BLOCKED";
        case PROC_STATE_ZOMBIE:
            return "ZOMBIE";
        default:
            return "UNKNOWN";
    }
}

/*
 * Terminate a process by PID.
 * L09 - Process state transitions.
 *
 * Returns:
 *   1  = process killed
 *   0  = PID not found
 *  -1  = shell cannot be killed
 */
int proc_kill(uint32_t pid) {
    pcb_t *proc = proc_find(pid);

    if (!proc) {
        return 0;
    }

    /*
     * PID 1 is our interactive shell.
     * Keep it alive so the Stage 1 demo remains usable.
     */
    if (proc->pid == 1) {
        return -1;
    }

    proc->state = PROC_STATE_ZOMBIE;

    /*
     * Normally kill is issued by the shell against another process,
     * but handle the current-process case as well.
     */
    if (current_proc == proc) {
        scheduler_yield();
    }

    return 1;
}

void proc_list(void) {
    int i;

    vga_puts("\nPID   STATE      NAME          TICKS\n");
    vga_puts("--------------------------------------\n");

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (pcb_table[i].pid != 0) {
            vga_printf(
                "%d     %s     %s     %d\n",
              pcb_table[i].pid,
              state_name(pcb_table[i].state),
              pcb_table[i].name,
              pcb_table[i].total_ticks
           );
        }
    }
}
