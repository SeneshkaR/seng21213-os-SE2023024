/* =============================================================================
 * SENG21213-OS :: Main Kernel  (Stage 3 – Physical Memory Manager)
 * File   : kernel/kernel.c
 *
 * PURPOSE
 *   This is the heart of your operating system. It now includes:
 *     1. Initialises VGA text-mode display
 *     2. Initialises the keyboard driver
 *     3. Initialises process table, threads, and scheduler
 *     4. Initialises the Physical Memory Manager (Stage 3)
 *     5. Prints a splash screen
 *     6. Runs a minimal interactive shell ("ksh") as a scheduled process
 *     7. Two demo processes run concurrently with visible output
 *     8. Kernel thread, race condition, and mutex demos (Stage 2)
 *     9. Memory info command showing PMM statistics (Stage 3)
 *
 * ASSIGNMENT MILESTONES
 *   [done] Lecture  9  – Process Management  →  process.c, scheduler.c, switch.asm
 *   [done] Lecture 10  – Threads & Sync       →  thread.c, mutex.c, semaphore.c
 *   [done] Lecture 11  – Memory Management    →  pmm.c
 *   [todo] Lecture 12  – File System          →  fs.c, ramdisk.c
 *
 * CODING CONVENTION
 *   - Prefix kernel-internal functions with k_ (e.g. k_strcmp)
 *   - All driver APIs live in their own .h/.c pair
 *   - NEVER call malloc – use the PMM you build in Lecture 11
 * ============================================================================*/

#include "vga.h"
#include "keyboard.h"
#include "process.h"
#include "scheduler.h"
#include "../include/types.h"
#include "thread.h"
#include "mutex.h"
#include "semaphore.h"
#include "pmm.h"

/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_meminfo(void);
static void cmd_version(void);
static void cmd_colour(const char *args);
static void cmd_halt(void);
static void cmd_kill(const char *args);
static void cmd_ps(void);
static void test_process_a(void);
static void test_process_b(void);
static void cmd_threads(void);

static void test_thread_a(void *arg);
static void test_thread_b(void *arg);

static void cmd_race(void);
static void race_thread(void *arg);

static void cmd_mutexdemo(void);
static void mutex_thread(void *arg);

static void cmd_semdemo(void);
static void sem_producer(void *arg);
static void sem_consumer(void *arg);

static void cmd_threadlist(void);

/* ---------------------------------------------------------------------------
 * Stage 2 - Race condition demonstration
 * --------------------------------------------------------------------------*/

/*
 * Both race-demo threads modify this same variable.
 * volatile forces actual memory reads/writes so the race is visible.
 */
static volatile int myglobal = 0;

/* Number of race-demo threads that have finished. */
static volatile int race_done = 0;

#define RACE_ITERATIONS 1000

static mutex_t myglobal_mutex;
static volatile int mutex_done = 0;

/* ---------------------------------------------------------------------------
 * Stage 2 - Semaphore demonstration (producer/consumer with a bounded buffer)
 * --------------------------------------------------------------------------*/

#define BUFFER_SIZE 4

static volatile int buffer[BUFFER_SIZE];
static volatile int buffer_count = 0;

static semaphore_t empty_sem;   /* Counts empty slots  */
static semaphore_t full_sem;    /* Counts filled slots */
static semaphore_t buffer_mutex; /* Binary semaphore for buffer access */

static volatile int sem_done = 0;

/* ---------------------------------------------------------------------------
 * Utility: minimal string helpers (no libc in a freestanding kernel!)
 * --------------------------------------------------------------------------*/
static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

/* Skip leading spaces */
static const char *k_ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

/* Simple atoi for number parsing */
static int k_atoi(const char *s) {
    int result = 0;
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return result;
}

/* Parse two integers from a string (for colour command) */
static int parse_two_ints(const char *s, int *a, int *b) {
    while (*s == ' ') s++;
    if (*s < '0' || *s > '9') return 0;
    *a = k_atoi(s);
    while (*s >= '0' && *s <= '9') s++;
    while (*s == ' ') s++;
    if (*s < '0' || *s > '9') return 0;
    *b = k_atoi(s);
    return 1;
}

/* ---------------------------------------------------------------------------
 * Splash Screen
 * --------------------------------------------------------------------------*/
static void print_splash(void) {
    vga_clear(VGA_BLACK);

    /* Top banner box */
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);

    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 3: Physical Memory Manager", VGA_LIGHT_CYAN, VGA_BLACK);

    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Science - Software Engineering Teaching Unit",
                   VGA_LIGHT_GREY, VGA_BLACK);

    vga_set_cursor(4, 2);
    vga_puts_color("  Built by students, for students.  Type 'help' to begin.",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  Welcome! This kernel was compiled from source and booted entirely\n");
    vga_puts("  from bare metal. There is no Linux or Windows underneath – only\n");
    vga_puts("  the code you and your team write.\n");
    vga_puts("\n");
    vga_puts("  Assignment milestones:\n");
    vga_puts_color("    [L09] ", VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("Process Management  – DONE\n");
    vga_puts_color("    [L10] ", VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("Threads & Sync      – DONE\n");
    vga_puts_color("    [L11] ", VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("Memory Management   – DONE (physical page allocator)\n");
    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("File System         – RAM disk, FAT-like directory structure\n");
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell command implementations
 * --------------------------------------------------------------------------*/

static void cmd_help(void) {
    vga_puts_color(
        "\n  SENG21213-OS Shell Commands\n",
        VGA_YELLOW,
        VGA_BLACK
    );

    vga_puts(
        "  ---------------------------------------------\n"
        "  help          Show this help message\n"
        "  clear         Clear the screen\n"
        "  about         About this OS and course\n"
        "  echo <text>   Echo text to screen\n"
        "  version       Show kernel version\n"
        "  colour <fg> <bg>  Change text colour\n"
        "  halt          Halt the CPU\n"
        "  meminfo       Show physical memory stats\n"
        "\n"
    );
    vga_puts_color(
        "  Stage 1 - Process Management\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts(
        "  ps            List processes and CPU ticks\n"
        "  kill <pid>    Terminate a process\n"
        "\n"
    );

    vga_puts_color(
        "  Stage 2 - Threads & Synchronization\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts(
        "  threads       Create and run kernel thread demo\n"
        "  threadlist    Show the thread table\n"
        "  race          Demonstrate race condition (no mutex)\n"
        "  mutexdemo     Demonstrate mutex-protected critical section\n"
        "  semdemo       Producer/consumer with semaphores\n"
        "\n"
    );

    vga_puts_color(
        "  Stage 3 - Physical Memory Manager\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts(
        "  meminfo       Show total / used / free physical frames\n"
        "\n"
    );

    vga_puts_color(
        "  Future milestones\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts(
        "  ls            [L12] List files\n"
        "  cat           [L12] Print file contents\n"
    );}

static void cmd_clear(void) {
    vga_clear(VGA_BLACK);
}

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 – Sem 2\n");
    vga_puts("  Reference    : Stallings, OS: Internals & Design Principles\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Stage 3 / L11 §3 — meminfo: Physical Memory Manager statistics
 *
 * Reads the bitmap-based PMM to report total, used, and free 4 KB frames.
 * The E820 memory map was collected by the bootloader in Real Mode and
 * stored at 0x4FFC (count) and 0x5000 (entries).
 *
 * Reference: L11 §3 — Bitmap page-frame allocator
 * --------------------------------------------------------------------------*/
static void cmd_meminfo(void)
{
    uint32_t total = pmm_total_frames();
    uint32_t used  = pmm_used_frames();
    uint32_t free  = pmm_free_frames();

    uint32_t total_kb = total * (PMM_FRAME_SIZE / 1024);
    uint32_t used_kb  = used  * (PMM_FRAME_SIZE / 1024);
    uint32_t free_kb  = free  * (PMM_FRAME_SIZE / 1024);

    vga_puts_color("\n  Physical Memory Manager (L11 §3)\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  Frame size       : 4 KB (4096 bytes)\n");
    vga_printf("  Total frames     : %u\n", total);
    vga_printf("  Used frames      : %u\n", used);
    vga_printf("  Free frames      : %u\n", free);
    vga_puts("\n");
    vga_printf("  Total memory     : %u KB (%u MB)\n", total_kb, total_kb / 1024);
    vga_printf("  Used memory      : %u KB\n", used_kb);
    vga_printf("  Free memory      : %u KB (%u MB)\n", free_kb, free_kb / 1024);
    vga_puts("\n");

    /* Self-test: allocate and free 100 frames, verify no leaks */
    vga_puts_color("  Self-test: allocating 100 frames...\n",
                   VGA_YELLOW, VGA_BLACK);

    uint32_t before_used = pmm_used_frames();
    uint32_t addrs[100];
    int alloc_ok = 1;
    int i;

    for (i = 0; i < 100; i++) {
        addrs[i] = pmm_alloc_frame();
        if (addrs[i] == 0) {
            alloc_ok = 0;
            break;
        }
    }

    if (!alloc_ok) {
        vga_puts_color("  FAIL: could not allocate 100 frames.\n",
                       VGA_LIGHT_RED, VGA_BLACK);
    } else {
        uint32_t after_alloc = pmm_used_frames();

        /* Free all 100 frames */
        for (i = 0; i < 100; i++) {
            pmm_free_frame(addrs[i]);
        }

        uint32_t after_free = pmm_used_frames();

        if (after_alloc == before_used + 100 && after_free == before_used) {
            vga_puts_color("  PASS: 100 frames allocated and freed, no leaks.\n",
                           VGA_LIGHT_GREEN, VGA_BLACK);
        } else {
            vga_puts_color("  FAIL: frame count mismatch after alloc/free.\n",
                           VGA_LIGHT_RED, VGA_BLACK);
            vga_printf("  Before: %u, After alloc: %u, After free: %u\n",
                       before_used, after_alloc, after_free);
        }
    }

    vga_puts("\n");
}

static void cmd_version(void) {
    vga_puts_color("\n  SENG21213-OS Version\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  Stage 3: Physical Memory Manager\n");
    vga_puts("  Version: 0.4.0\n");
    vga_puts("  Build Date: " __DATE__ " " __TIME__ "\n");
    vga_puts("  Compiler: GCC " __VERSION__ "\n");
    vga_puts("  Architecture: x86 (i686) 32-bit Protected Mode\n");
    vga_puts("  Target: qemu-system-i386\n\n");
}

static void cmd_colour(const char *args) {
    int fg, bg;

    if (!parse_two_ints(args, &fg, &bg)) {
        vga_puts_color("  Usage: colour <fg> <bg>\n", VGA_YELLOW, VGA_BLACK);
        vga_puts("  fg: 0-15 (foreground), bg: 0-15 (background)\n");
        vga_puts("  0=black, 1=blue, 2=green, 3=cyan, 4=red, 5=magenta\n");
        vga_puts("  6=brown, 7=light grey, 8=dark grey, 9=light blue\n");
        vga_puts("  10=light green, 11=light cyan, 12=light red\n");
        vga_puts("  13=light magenta, 14=yellow, 15=white\n\n");
        return;
    }

    if (fg < 0 || fg > 15 || bg < 0 || bg > 15) {
        vga_puts_color("  Error: Values must be between 0 and 15\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    vga_set_color((vga_color_t)fg, (vga_color_t)bg);
    vga_printf("  Colour set to foreground=%d, background=%d\n", fg, bg);
}

static void cmd_halt(void) {
    vga_puts_color("\n  Halting CPU...\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  Press Ctrl+Alt+Del or reset QEMU to restart.\n\n");

    /* Disable interrupts and halt */
    __asm__ __volatile__("cli\n"
                         "hlt");

    /* In case we ever return */
    while (1) __asm__ __volatile__("hlt");
}

static void cmd_ps(void) {
    proc_list();
}

static void cmd_kill(const char *args) {
    const char *pid_text = k_ltrim(args);

    if (k_strlen(pid_text) == 0) {
        vga_puts_color(
            "  Usage: kill <pid>\n",
            VGA_YELLOW,
            VGA_BLACK
        );
        return;
    }

    uint32_t pid = (uint32_t)k_atoi(pid_text);

    int result = proc_kill(pid);

    if (result == 1) {
        vga_printf("  Process %d killed.\n", pid);
    }
    else if (result == -1) {
        vga_puts_color(
            "  Cannot kill the shell process.\n",
            VGA_YELLOW,
            VGA_BLACK
        );
    }
    else {
        vga_puts_color(
            "  No such process.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
    }
}

/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char  shell_buf[256];
static char  prompt[] = "\n  ksh> ";

/*
 * Stage 2 / L10 §3 — Kernel Thread Demo
 *
 * Two threads that print interleaved output, demonstrating
 * concurrent execution and voluntary context switching.
 */
static void test_thread_a(void *arg)
{
    (void)arg;

    for (int i = 0; i < 8; i++) {
        vga_puts("[T1] ");
        scheduler_yield();
    }

    vga_puts("\nThread 1 finished.\n");
    thread_exit();
}

static void test_thread_b(void *arg)
{
    (void)arg;

    for (int i = 0; i < 8; i++) {
        vga_puts("[T2] ");
        scheduler_yield();
    }

    vga_puts("\nThread 2 finished.\n");
    thread_exit();
}

/*
 * Stage 2 / L10 §4 — Race Condition Demo
 *
 * Deliberately unsafe increment:
 *
 *     read myglobal
 *     yield
 *     write myglobal + 1
 *
 * Another thread can modify myglobal between the read and write,
 * producing lost updates.
 *
 * Reference: Stallings Ch.4 — Mutual Exclusion violations
 */
static void race_thread(void *arg)
{
    int i;

    (void)arg;

    for (i = 0; i < RACE_ITERATIONS; i++) {
        int temp;

        /* READ shared value */
        temp = myglobal;

        /*
         * Force a context switch between read and write.
         * This makes the race condition easy to demonstrate.
         */
        scheduler_yield();

        /* WRITE based on the old value */
        myglobal = temp + 1;
    }

    race_done++;

    thread_exit();
}

/*
 * Stage 2 / L10 §4 — Mutex-Protected Critical Section
 *
 * Same race scenario as race_thread(), but wrapped in mutex_lock/unlock.
 * The mutex ensures only one thread modifies myglobal at a time.
 */
static void mutex_thread(void *arg)
{
    int i;

    (void)arg;

    for (i = 0; i < RACE_ITERATIONS; i++) {

        mutex_lock(&myglobal_mutex);

        /*
         * Critical section.
         */
        int temp = myglobal;

        /*
         * Force scheduling while lock is held.
         * The other thread must block on the mutex.
         */
        scheduler_yield();

        myglobal = temp + 1;

        mutex_unlock(&myglobal_mutex);
    }

    mutex_done++;

    thread_exit();
}

static void cmd_mutexdemo(void)
{
    thread_t *t1;
    thread_t *t2;

    myglobal = 0;
    mutex_done = 0;

    mutex_init(&myglobal_mutex);

    vga_puts("\n");
    vga_puts("========================================\n");
    vga_puts(" Stage 2 - Mutex Demo\n");
    vga_puts(" WITH MUTEX\n");
    vga_puts("========================================\n");

    vga_printf(
        "Two threads increment myglobal %d times each.\n",
        RACE_ITERATIONS
    );

    vga_printf(
        "Expected final value: %d\n",
        RACE_ITERATIONS * 2
    );

    vga_puts("Starting protected test...\n\n");

    t1 = thread_create(mutex_thread, 0);
    t2 = thread_create(mutex_thread, 0);

    if (!t1 || !t2) {
        vga_puts_color(
            "ERROR: Could not create mutex test threads.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
        return;
    }

    while (mutex_done < 2) {
        scheduler_yield();
    }

    vga_puts("\nMutex test completed.\n");

    vga_printf(
        "Expected: %d\n",
        RACE_ITERATIONS * 2
    );

    vga_printf(
        "Actual:   %d\n",
        myglobal
    );

    if (myglobal == RACE_ITERATIONS * 2) {
        vga_puts_color(
            "RESULT: PASS - mutex prevented lost updates.\n",
            VGA_LIGHT_GREEN,
            VGA_BLACK
        );
    } else {
        vga_puts_color(
            "RESULT: FAIL - data corruption detected.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
    }

    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Stage 2 / L10 §5 — Bounded-Buffer Producer/Consumer with Three Semaphores
 *
 * Uses three semaphores:
 *   - empty_sem    : counts empty buffer slots (initialised to BUFFER_SIZE)
 *   - full_sem     : counts filled buffer slots (initialised to 0)
 *   - buffer_mutex : binary semaphore for mutual exclusion (initialised to 1)
 *
 * Reference: Stallings Ch.4 — Producer/Consumer problem
 * --------------------------------------------------------------------------*/

static void sem_producer(void *arg)
{
    int id = (int)(uint32_t)arg;
    int i;

    for (i = 0; i < 5; i++) {
        /* Wait for an empty slot */
        sem_wait(&empty_sem);

        sem_wait(&buffer_mutex);
        buffer[buffer_count] = id * 100 + i;
        buffer_count++;
        vga_printf("  Producer %d inserted item %d (buffer: %d/%d)\n",
                   id, buffer[buffer_count - 1], buffer_count, BUFFER_SIZE);
        sem_signal(&buffer_mutex);

        /* Signal that a slot is now full */
        sem_signal(&full_sem);

        scheduler_yield();
    }

    sem_done++;
    thread_exit();
}

static void sem_consumer(void *arg)
{
    int i;

    (void)arg;

    for (i = 0; i < 10; i++) {
        /* Wait for a full slot */
        sem_wait(&full_sem);

        sem_wait(&buffer_mutex);
        buffer_count--;
        int item = buffer[buffer_count];
        vga_printf("  Consumer extracted item %d (buffer: %d/%d)\n",
                   item, buffer_count, BUFFER_SIZE);
        sem_signal(&buffer_mutex);

        /* Signal that a slot is now empty */
        sem_signal(&empty_sem);

        scheduler_yield();
    }

    sem_done++;
    thread_exit();
}

static void cmd_semdemo(void)
{
    thread_t *p1, *p2, *c1;

    buffer_count = 0;
    sem_done = 0;

    sem_init(&empty_sem, BUFFER_SIZE);
    sem_init(&full_sem, 0);
    sem_init(&buffer_mutex, 1);

    vga_puts("\n");
    vga_puts("========================================\n");
    vga_puts(" Stage 2 - Semaphore Demo\n");
    vga_puts(" Producer/Consumer with bounded buffer\n");
    vga_puts("========================================\n");

    vga_printf("Buffer size: %d\n", BUFFER_SIZE);
    vga_puts("2 producers (5 items each), 1 consumer\n\n");

    p1 = thread_create(sem_producer, (void *)1);
    p2 = thread_create(sem_producer, (void *)2);
    c1 = thread_create(sem_consumer, (void *)1);

    if (!p1 || !p2 || !c1) {
        vga_puts_color(
            "ERROR: Could not create semaphore demo threads.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
        return;
    }

    /* Wait for all 3 threads to finish */
    while (sem_done < 3) {
        scheduler_yield();
    }

    vga_puts("\nSemaphore demo completed.\n");
    vga_printf("Final buffer count: %d (expected 0)\n", buffer_count);

    if (buffer_count == 0) {
        vga_puts_color(
            "RESULT: PASS - semaphore coordination correct.\n",
            VGA_LIGHT_GREEN,
            VGA_BLACK
        );
    } else {
        vga_puts_color(
            "RESULT: FAIL - buffer count mismatch.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
    }

    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Stage 2 - Thread table listing
 * --------------------------------------------------------------------------*/
static void cmd_threadlist(void)
{
    thread_list();
}

static void cmd_race(void)
{
    thread_t *t1;
    thread_t *t2;

    myglobal = 0;
    race_done = 0;

    vga_puts("\n");
    vga_puts("========================================\n");
    vga_puts(" Stage 2 - Race Condition Demo\n");
    vga_puts(" WITHOUT MUTEX\n");
    vga_puts("========================================\n");

    vga_printf(
        "Two threads increment myglobal %d times each.\n",
        RACE_ITERATIONS
    );

    vga_printf(
        "Expected final value: %d\n",
        RACE_ITERATIONS * 2
    );

    vga_puts("Starting race...\n\n");

    t1 = thread_create(race_thread, 0);
    t2 = thread_create(race_thread, 0);

    if (!t1 || !t2) {
        vga_puts_color(
            "ERROR: Could not create race threads.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
        return;
    }

    /*
     * The shell waits, but yields the CPU so the two worker
     * threads can run.
     */
    while (race_done < 2) {
        scheduler_yield();
    }

    vga_puts("\nRace completed.\n");

    vga_printf(
        "Expected: %d\n",
        RACE_ITERATIONS * 2
    );

    vga_printf(
        "Actual:   %d\n",
        myglobal
    );

    if (myglobal != RACE_ITERATIONS * 2) {
        vga_puts_color(
            "RESULT: RACE CONDITION DETECTED - lost updates occurred.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
    } else {
        vga_puts_color(
            "RESULT: No lost update observed in this run.\n",
            VGA_YELLOW,
            VGA_BLACK
        );
    }

    vga_puts("\n");
}

static void cmd_threads(void)
{
    thread_t *t1;
    thread_t *t2;

    vga_puts("\nCreating two Stage 2 kernel threads...\n");

    t1 = thread_create(test_thread_a, 0);
    t2 = thread_create(test_thread_b, 0);

    if (!t1 || !t2) {
        vga_puts_color(
            "Failed to create threads.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
        return;
    }

    vga_printf("Thread %d created.\n", t1->tid);
    vga_printf("Thread %d created.\n", t2->tid);
}

static void shell_run(void) {
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        /* Trim leading whitespace */
        const char *cmd = k_ltrim(shell_buf);
        if (k_strlen(cmd) == 0) continue;

        /* Dispatch - Stage 0 commands */
        if (k_strcmp(cmd, "help")    == 0) { cmd_help();    continue; }
        if (k_strcmp(cmd, "clear")   == 0) { cmd_clear();   continue; }
        if (k_strcmp(cmd, "about")   == 0) { cmd_about();   continue; }
        if (k_strcmp(cmd, "version") == 0) { cmd_version(); continue; }
        if (k_strcmp(cmd, "meminfo") == 0) { cmd_meminfo(); continue; }
        if (k_strcmp(cmd, "halt")    == 0) { cmd_halt();    continue; }

        /* Check for echo command with space prefix */
        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

        /* Check for colour command with space prefix */
        if (k_strncmp(cmd, "colour ", 7) == 0) {
            cmd_colour(k_ltrim(cmd + 7));
            continue;
        }

        /* Check for just "colour" without arguments */
        if (k_strcmp(cmd, "colour") == 0) {
            /* Show usage if no arguments */
            cmd_colour("");
            continue;
        }

          /* Stage 1 - process list */
        if (k_strcmp(cmd, "ps") == 0) {
            cmd_ps();
            continue;
        }

          /* Stage 1 - kill process */
        if (k_strncmp(cmd, "kill ", 5) == 0) {
            cmd_kill(k_ltrim(cmd + 5));
            continue;
        }

        if (k_strcmp(cmd, "kill") == 0) {
            cmd_kill("");
            continue;
        }
        /* Stage 2 - kernel thread demo */
        if (k_strcmp(cmd, "threads") == 0) {
            cmd_threads();
            continue;
         }

        /* Stage 2 - race condition without mutex */
        if (k_strcmp(cmd, "race") == 0) {
            cmd_race();
            continue;
         }

        if (k_strcmp(cmd, "mutexdemo") == 0) {
            cmd_mutexdemo();
            continue;
         }

        /* Stage 2 - semaphore producer/consumer demo */
        if (k_strcmp(cmd, "semdemo") == 0) {
            cmd_semdemo();
            continue;
         }

        /* Stage 2 - thread table listing */
        if (k_strcmp(cmd, "threadlist") == 0) {
            cmd_threadlist();
            continue;
         }

        /* Future milestones */
        if (k_strcmp(cmd, "ls")   == 0 ||
            k_strcmp(cmd, "cat")  == 0) {

            vga_puts_color(
                "  [TODO] This command is not yet implemented.\n",
                VGA_YELLOW,
                VGA_BLACK
            );

            vga_puts(
                "  Implement it as part of your lecture assignment.\n"
            );

            continue;
        }

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}

static void test_process_a(void) {
    int count = 0;

    while (1) {
        if (count < 10) {
            vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
            vga_puts("[A]");
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            count++;
        }

        for (volatile int i = 0; i < 3000000; i++);
    }
}

static void test_process_b(void) {
    int count = 0;

    while (1) {
        if (count < 10) {
            vga_set_color(VGA_YELLOW, VGA_BLACK);
            vga_puts("[B]");
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            count++;
        }

        for (volatile int i = 0; i < 6000000; i++);
    }
}

/* ---------------------------------------------------------------------------
 * Kernel entry point – called from kernel_entry.asm
 * -------------------------------------------------------------------------*/

void kernel_main(void) {
    vga_init();
    kb_init();

    /* Stage 1–2 initialization */
    proc_init();
    thread_init();
    scheduler_init();

    /* Stage 3 / L11 §3 — Physical Memory Manager init
     * Parses BIOS E820 map and builds the page-frame bitmap. */
    pmm_init();

    print_splash();

    /*
     * Schedule the shell together with the two test processes.
     */
    proc_create("shell", shell_run);
    proc_create("process-A", test_process_a);
    proc_create("process-B", test_process_b);

    /*
     * Start PIT IRQ0 round-robin scheduling.
     */
    scheduler_start();

    /*
     * kernel_main is no longer doing shell work.
     * Wait until the scheduler switches to a process.
     */
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

