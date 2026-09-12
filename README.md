# SENG21213-OS — Stage 4: RAM Disk File System

> **Course**: SENG 21213 – Computer Architecture & Operating Systems  
> **Year**: 2nd Year, Software Engineering  
> **Assignment**: Build your own x86 Operating System

---

## What Is This?

This is **Stage 4** of your semester-long OS assignment. Over 5 lecture milestones
(Lectures 8–12), your team will transform this minimal kernel into a functioning
operating system with process management, threading, memory management, and a
file system.

```
seng21213-os/
├── boot/
│   ├── boot.asm          ← MBR Bootloader (NASM, 16-bit → 32-bit transition + E820 map)
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
│   ├── semaphore.c / .h  ← Stage 2: counting semaphore
│   ├── pmm.c / pmm.h     ← Stage 3: Physical Memory Manager (bitmap allocator)
│   ├── fs.c / fs.h       ← Stage 4: RAM Disk File System (inode-based)
│   └── string.c / string.h ← String library (freestanding environment)
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
| L11 | Memory Management | Done | `kernel/pmm.c`, `kernel/pmm.h` |
| L12 | File System | **Done** | `kernel/fs.c`, `kernel/fs.h`, `kernel/string.c`, `kernel/string.h` |

---

## Shell Commands Reference

The `ksh>` shell provides the following commands across all stages:

### Stage 0 – Boot, VGA & Shell

| Command | Description |
|---------|-------------|
| `help` | Show all shell commands |
| `clear` | Clear the screen |
| `about` | Show OS/course information |
| `echo <text>` | Print text back to the screen |
| `version` | Show kernel version |
| `colour <fg> <bg>` | Change foreground/background colour |
| `mem` | Show memory map (stub for Stage 3) |
| `halt` | Halt the CPU |

### Stage 1 – Process Management

| Command | Description |
|---------|-------------|
| `ps` | List processes and CPU ticks |
| `kill <pid>` | Terminate a process |

### Stage 2 – Threads & Synchronization

| Command | Description |
|---------|-------------|
| `threads` | Create and run the kernel thread demo |
| `threadlist` | Show the thread table |
| `race` | Demonstrate the race condition without a mutex |
| `mutexdemo` | Demonstrate mutex-protected shared data |
| `semdemo` | Run producer-consumer using semaphores |

### Stage 3 – Physical Memory Manager

| Command | Description |
|---------|-------------|
| `meminfo` | Show physical memory information and run the 100-frame PMM self-test |

### Stage 4 – RAM Disk File System

| Command | Description |
|---------|-------------|
| `ls` | List all files in the file system |
| `touch <name>` | Create an empty file |
| `cat <name>` | Display file contents |
| `write <name> <text>` | Write text to a file (creates if doesn't exist) |
| `rm <name>` | Remove a file |
| `fsstats` | Show file system statistics |

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
  │  Collects BIOS E820 memory map → stores at 0x4FFC (count) and 0x5000 (entries)
  │  Reads 128 sectors (kernel) from disk into RAM at 0x10000
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
  │  proc_init()    – initialize process table
  │  thread_init()  – initialize thread system
  │  scheduler_init() – set up round-robin scheduler
  │  pmm_init()     – parse E820 map, build page-frame bitmap
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

## Stage 3: Physical Memory Manager (Lecture 11)

Stage 3 implements a bitmap-based physical memory manager that tracks 4 KB page frames.

### Implementation Details

**BIOS E820 Memory Map Collection** (`boot/boot.asm`)
- Bootloader collects memory map using INT 0x15, EAX=0xE820 in Real Mode
- Entry count stored at `0x4FFC` (16-bit)
- Entries stored at `0x5000` (24 bytes each: base, length, type, ACPI flags)
- Map passed to kernel before switching to Protected Mode

**Bitmap Allocator** (`kernel/pmm.c`)
- 1 bit per 4 KB physical page frame
- Supports up to 128 MB (32,768 frames, 4 KB bitmap)
- Initialization: marks all frames as used, then frees usable E820 regions
- Reserves first 1 MB for BIOS, bootloader, kernel, stacks

**Key Functions**
```c
void pmm_init(void);              // Parse E820 map, build bitmap
uint32_t pmm_alloc_frame(void);   // First-fit allocation, returns physical address
void pmm_free_frame(uint32_t);    // Free frame by physical address
uint32_t pmm_total_frames(void);  // Total allocatable frames
uint32_t pmm_used_frames(void);   // Currently allocated frames
uint32_t pmm_free_frames(void);   // Available frames
```

### Shell Commands

| Command | Description |
|---------|-------------|
| `meminfo` | Display physical memory statistics and run self-test |

### Key Concepts Demonstrated

- **E820 Memory Map**: BIOS interrupt 0x15 reports which physical address ranges are usable RAM, reserved, or ACPI reclaimable
- **Bitmap Allocation**: Each bit represents one 4 KB page frame. Bit 0 = free, bit 1 = allocated
- **First-Fit Algorithm**: `pmm_alloc_frame()` scans from frame 0 upward, returning the first free frame
- **Frame Validation**: `pmm_free_frame()` checks alignment, range, and prevents double-free
- **Self-Test**: `meminfo` allocates 100 frames, verifies the count increases by 100, frees them, and verifies no leaks

### Testing Stage 3

After building and running (`make run`), verify the deliverable:

| Test | Command | Expected Result |
|------|---------|-----------------|
| Memory info | `meminfo` | Shows total/used/free frames and KB/MB |
| Self-test | (automatic in `meminfo`) | "PASS: 100 frames allocated and freed, no leaks." |

### What to look for

- **`meminfo`**: Should show ~7168 total frames (28 MB) for QEMU with `-m 32M`
- **Used frames**: Initially 0 (no allocations yet)
- **Free frames**: Matches total frames
- **Self-test**: Must show "PASS" — proves allocation and deallocation work correctly with no memory leaks

### Example Output

```
Physical Memory Manager (L11 §3)
---------------------------------------------
Frame size       : 4 KB (4096 bytes)
Total frames     : 7168
Used frames      : 0
Free frames      : 7168

Total memory     : 28672 KB (28 MB)
Used memory      : 0 KB
Free memory      : 28672 KB (28 MB)

Self-test: allocating 100 frames...
PASS: 100 frames allocated and freed, no leaks.
```

---

## Stage 4: RAM Disk File System (Lecture 12)

Stage 4 implements a complete inode-based file system on a 1 MB RAM disk.

### Implementation Details

**RAM Disk** (`kernel/fs.c`)
- 1 MB byte array in BSS segment (256 blocks × 4 KB)
- Persistent across file operations within a boot session
- Formatted on initialization with superblock, bitmaps, and directory

**File System Layout**
```
Block 0:     Superblock (magic number, metadata)
Block 1:     Directory entries (filename → inode mapping)
Block 2:     Block bitmap (tracks free data blocks)
Block 3:     Inode bitmap (tracks free inodes)
Blocks 4-35: Inodes (32 blocks × 32 inodes = 1024 inodes)
Blocks 36-255: Data blocks (220 blocks × 4 KB = 880 KB)
```

**Superblock Structure**
```c
typedef struct {
    uint32_t magic;          // Magic number (0x53454E47 = "SENG")
    uint32_t total_blocks;   // Total blocks in file system
    uint32_t inode_blocks;   // Number of blocks for inodes
    uint32_t data_blocks;    // Number of data blocks
    uint32_t free_blocks;    // Count of free data blocks
    uint32_t free_inodes;    // Count of free inodes
} superblock_t;
```

**Inode Structure**
```c
typedef struct {
    uint32_t size;              // File size in bytes
    uint32_t blocks[8];         // 8 direct block pointers
    uint8_t  reserved[92];      // Padding to 128 bytes
} inode_t;
```

**Directory Entry Structure**
```c
typedef struct {
    char     name[28];      // Filename (null-terminated)
    uint32_t inode;         // Inode number (0 = unused)
} dir_entry_t;
```

**Key Functions**
```c
void fs_init(void);                          // Initialize file system
int  fs_open(const char *filename, int create); // Open file (create if needed)
int  fs_close(int fd);                       // Close file descriptor
int  fs_read(int fd, void *buffer, uint32_t count);  // Read from file
int  fs_write(int fd, const void *buffer, uint32_t count); // Write to file
int  fs_unlink(const char *filename);        // Delete file
int  fs_create_file(const char *filename);   // Create empty file
int  fs_list_dir(void);                      // List all files
void fs_print_stats(void);                   // Print file system statistics
```

**String Library** (`kernel/string.c`)
- Custom implementation for freestanding environment (no libc)
- Memory operations: `memset`, `memcpy`, `memmove`, `memcmp`
- String operations: `strlen`, `strcpy`, `strncpy`, `strcmp`, `strncmp`, `strcat`, `strchr`, `strrchr`

### Shell Commands

| Command | Description |
|---------|-------------|
| `ls` | List all files with sizes |
| `touch <name>` | Create an empty file |
| `cat <name>` | Display file contents |
| `write <name> <text>` | Write text to file (creates if doesn't exist) |
| `rm <name>` | Delete a file |
| `fsstats` | Show file system statistics |

### Key Concepts Demonstrated

- **Inode-based File System**: Each file has an inode with metadata and direct block pointers
- **Bitmap Allocation**: Separate bitmaps track free data blocks and free inodes
- **Superblock**: Stores file system metadata and magic number for validation
- **Flat Directory**: Simple directory structure mapping filenames to inode numbers
- **File Descriptors**: Track open files with current read/write position
- **POSIX-style API**: Familiar open/read/write/close/unlink interface
- **Direct Block Pointers**: Each inode has 8 direct pointers (max 32 KB per file)

### Testing Stage 4

After building and running (`make run`), verify the deliverables:

| Test | Command | Expected Result |
|------|---------|-----------------|
| List files | `ls` | Shows empty directory or existing files |
| Create file | `touch test.txt` | "Created file: test.txt" |
| Write to file | `write test.txt Hello World` | "Wrote 11 bytes to test.txt" |
| Read file | `cat test.txt` | Displays "Hello World" |
| List again | `ls` | Shows "test.txt" with size 11 bytes |
| Delete file | `rm test.txt` | "Removed: test.txt" |
| Verify deletion | `ls` | File no longer appears |
| File system stats | `fsstats` | Shows total/used/free blocks and inodes |

### What to look for

- **`ls`**: Should display filename and size for each file
- **`touch`**: Creates file with 0 bytes
- **`write`**: Creates file if it doesn't exist, appends if it does
- **`cat`**: Displays exact contents that were written
- **`rm`**: Frees inode and all data blocks
- **`fsstats`**: Shows accurate block and inode usage

### Example Output

```
ksh> touch hello.txt
Created file: hello.txt

ksh> write hello.txt Welcome to SENG21213-OS!
Wrote 26 bytes to hello.txt

ksh> cat hello.txt
Welcome to SENG21213-OS!

ksh> ls
Directory listing:
─────────────────────────────────────────────
hello.txt                      26 bytes

1 file(s)

ksh> fsstats
RAM Disk File System Statistics
─────────────────────────────────────────────
Total size       : 1024 KB
Block size       : 4096 bytes
Total blocks     : 256
Data blocks      : 220
Free data blocks : 219
Used data blocks : 1
Max inodes       : 1024
Free inodes      : 1023
Max file size    : 32 KB

ksh> rm hello.txt
Removed: hello.txt

ksh> ls
Directory listing:
─────────────────────────────────────────────
(empty)

0 file(s)
```

### File System Limits

- **Maximum file size**: 32 KB (8 direct blocks × 4 KB)
- **Maximum files**: 1024 inodes
- **Maximum filename length**: 27 characters (28 bytes including null terminator)
- **RAM disk size**: 1 MB (256 blocks × 4 KB)
- **Available data space**: 880 KB (220 data blocks)

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

# Inspect the E820 memory map (in GDB)
(gdb) x/1xh 0x4FFC          # Entry count
(gdb) x/24xb 0x5000         # First E820 entry (24 bytes)

# Inspect the PMM bitmap (in GDB)
(gdb) print pmm_total_frames()
(gdb) print pmm_used_frames()
(gdb) print pmm_free_frames()

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
| BIOS E820 Map | OSDev Wiki: Memory Map (x86) |
| Bitmap Allocator | Stallings Ch.8 §2 — Paging |
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

## Submission

This stage is tagged as `v0.4-stage3` on the `stage3` branch.

```bash
git checkout stage3
git tag v0.4-stage3
git push origin stage3 --tags
```

---

*Happy hacking! Remember: every commercial OS started exactly like this.*
