# SENG21213-OS — Stage 2: Threads & Synchronization

> **Course**: SENG 21213 – Computer Architecture & Operating Systems  
> **Year**: 2nd Year, Software Engineering  
> **Assignment**: Build your own x86 Operating System

---

## What Is This?

This is **Stage 2** of your semester-long OS assignment. Over 5 lecture milestones
(Lectures 8–12), your team will transform this minimal kernel into a functioning
operating system with process management, threading, memory management, and a
file system.

```
seng21213-os/
├── boot/
│   ├── boot.asm          ← MBR Bootloader (NASM, 16-bit → 32-bit transition)
│   └── switch.asm        ← IRQ0 context switch stub (pushad/iretd)
├── kernel/
│   ├── kernel_entry.asm  ← Protected-mode entry, calls kernel_main()
│   ├── kernel.c          ← Main kernel: shell loop, command dispatch, demos
│   ├── vga.c / vga.h     ← VGA 80×25 text-mode driver
│   ├── keyboard.c / .h   ← PS/2 keyboard polling driver
│   ├── process.c / .h    ← Stage 1: PCB table, process creation
│   ├── scheduler.c / .h  ← Stage 1+2: IDT, PIC, PIT, round-robin scheduler
│   ├── thread.c / .h     ← Stage 2: kernel threads with own stacks
│   ├── mutex.c / .h      ← Stage 2: blocking mutex with wait queue
│   └── semaphore.c / .h  ← Stage 2: counting semaphore
├── include/
│   └── types.h           ← Primitive types (no libc!)
├── linker.ld             ← Linker script (kernel at 0x10000)
├── Makefile              ← Build system
├── Dockerfile            ← Reproducible build environment
└── README.md             ← You are here
```

---

## Milestone Schedule

| Lecture | Milestone | Status | Files |
|---------|-----------|--------|-------|
| L08 | Stage 0 – Boot + VGA + Shell | Done | *Given* |
| L09 | Process Management | Done | `kernel/process.c`, `kernel/scheduler.c`, `boot/switch.asm` |
| L10 | Threads & Synchronisation | Done | `kernel/thread.c`, `kernel/mutex.c`, `kernel/semaphore.c` |
| L11 | Memory Management | TODO | `kernel/pmm.c`, `kernel/vmm.c` |
| L12 | File System | TODO | `kernel/fs.c`, `kernel/ramdisk.c` |

---

## Quick Start

### Option A: Docker (Recommended for all platforms)

```bash
# 1. Install Docker Desktop (Windows/Mac) or Docker Engine (Linux)
# 2. Build the image once:
docker build -t seng21213-os-builder .

# 3. Build the OS:
docker run --rm -v "$(pwd)":/os seng21213-os-builder

# 4. Run in QEMU (install QEMU locally):
qemu-system-i386 -drive format=raw,file=seng21213-os.img -m 32M
```

### Option B: Native Linux/WSL2

```bash
# Ubuntu/Debian
sudo apt install nasm gcc gcc-multilib binutils qemu-system-x86 make

# Build
make all

# Run
make run
```

### Option C: macOS (Homebrew)

```bash
brew install nasm x86_64-elf-binutils qemu

# You also need an i686-elf-gcc cross-compiler:
# See: https://wiki.osdev.org/GCC_Cross-Compiler
make all
make run
```

---

## Understanding the Boot Process

```
Power On
  │
  ▼
BIOS (firmware in ROM)
  │  Loads 512-byte MBR from disk sector 1 into RAM at 0x7C00
  ▼
boot/boot.asm  (Real Mode, 16-bit)
  │  Prints "Loading SENG21213-OS..."
  │  Reads 64 sectors (kernel) from disk into RAM at 0x10000
  │  Sets up GDT (Global Descriptor Table)
  │  Switches CPU to 32-bit Protected Mode
  │  Far-jumps to 0x10000
  ▼
kernel/kernel_entry.asm  (Protected Mode, 32-bit)
  │  Calls kernel_main()
  ▼
kernel/kernel.c  →  kernel_main()
  │  vga_init()     – set up text display
  │  kb_init()      – set up keyboard
  │  print_splash() – welcome screen
  │  shell_run()    – interactive shell (infinite loop)
  ▼
Your code from here...
```

---

## Building Lecture 9: Process Management

When you reach Lecture 9, you'll add process support. Here's the interface to implement:

```c
/* kernel/process.h  — you write this! */

#define MAX_PROCESSES    16
#define STACK_SIZE     4096

typedef enum { READY, RUNNING, BLOCKED, TERMINATED } proc_state_t;

typedef struct pcb {
    uint32_t      pid;
    proc_state_t  state;
    uint32_t      esp;          /* Saved stack pointer */
    uint32_t      eip;          /* Saved instruction pointer */
    uint32_t      stack[STACK_SIZE / 4];
    struct pcb   *next;         /* For linked-list ready queue */
} pcb_t;

void   process_init(void);
pcb_t *process_create(void (*entry)(void));
void   process_yield(void);        /* Trigger context switch */
void   process_exit(void);
void   scheduler_tick(void);       /* Called by timer IRQ (Lecture 10) */
```

---

## Stage 2: Threads & Synchronization (Lecture 10)

Stage 2 adds kernel-level threading and synchronization primitives.

### Shell Commands

| Command | Description |
|---------|-------------|
| `threads` | Create two kernel threads that print interleaved output |
| `threadlist` | Display the thread table (TID, state, owner PID) |
| `race` | Demonstrate a race condition on `myglobal` (no mutex) |
| `mutexdemo` | Same race, but protected by a blocking mutex |
| `semdemo` | Producer/consumer with counting semaphores and bounded buffer |

### Key Concepts Demonstrated

- **Kernel threads**: Each thread has its own 4 KB stack and CPU context,
  but shares the owning process's address space.
- **Round-robin scheduling**: The scheduler alternates between processes and
  threads on each PIT tick (100 Hz).
- **Race conditions**: `race` shows lost updates when two threads read-modify-write
  a shared variable with a forced context switch between read and write.
- **Mutex**: `mutexdemo` shows the same scenario with a blocking mutex.
  The mutex maintains a FIFO wait queue; ownership transfers directly on unlock.
- **Semaphore**: `semdemo` implements a bounded-buffer producer/consumer.
  `empty_sem` and `full_sem` coordinate access; `buffer_mutex` protects the
  shared buffer.

---

## Testing Stage 2

After building and running (`make run`), verify each deliverable:

| Test | Command | Expected Result |
|------|---------|-----------------|
| Kernel threads | `threads` | Two threads print `[T1]` and `[T2]` interleaved, then report "finished" |
| Thread table | `threadlist` | Lists all threads with TID, state, and owner PID |
| Race condition | `race` | Two threads increment `myglobal` 1000 times each. Final value < 2000 (lost updates) |
| Mutex protection | `mutexdemo` | Same test with mutex. Final value = 2000 (no lost updates). Shows "PASS" |
| Semaphore coordination | `semdemo` | 2 producers insert 5 items each, 1 consumer extracts all 10. Buffer empty at end. Shows "PASS" |

### What to look for

- **`race`**: The actual value should be significantly less than 2000, clearly showing lost updates
- **`mutexdemo`**: The actual value must equal 2000, proving the mutex prevented data corruption
- **`semdemo`**: All 10 items produced and consumed without corruption, buffer count = 0 at end

---

## Debugging Tips

```bash
# Debug with GDB
make run-debug
# In another terminal:
gdb
(gdb) target remote :1234
(gdb) set architecture i386
(gdb) symbol-file build/kernel.elf
(gdb) break kernel_main
(gdb) continue

# Inspect the disk image
xxd seng21213-os.img | head -32    # View MBR
xxd seng21213-os.img | grep -c aa55  # Verify boot signature
```

---

## Key Learning Resources

| Topic | Reference |
|-------|-----------|
| x86 Protected Mode | Intel IA-32 Manual, Vol 3, Chapter 3 |
| VGA Text Mode | OSDev Wiki: Text UI |
| Interrupts / IDT | Stallings Ch.1; OSDev: IDT |
| Process Management | Stallings Ch.3–4 (your lecture notes) |
| Memory Management | Stallings Ch.7–8 (your lecture notes) |
| OSDev community | https://wiki.osdev.org |

---

## Assessment Rubric (per milestone)

| Criterion | Weight |
|-----------|--------|
| Code compiles and kernel boots in QEMU | 30% |
| Feature implementation (correct behaviour) | 40% |
| Code quality and comments | 20% |
| Lab demo and viva questions | 10% |

---

*Happy hacking! Remember: every commercial OS started exactly like this.*
