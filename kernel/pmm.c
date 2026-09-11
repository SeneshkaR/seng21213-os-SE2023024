#include "pmm.h"

/* BIOS E820 map written by bootloader */
#define E820_COUNT_ADDR 0x4FFC
#define E820_MAP_ADDR   0x5000

/* Support up to 128 MB physical memory (matches QEMU -m 32M with headroom):
 * 128 MB / 4 KB = 32,768 frames
 * bitmap size = 32,768 bits = 4,096 bytes
 * L11 §3 — Bitmap allocator: 1 bit per 4 KB page frame
 */
#define PMM_MAX_FRAMES 32768
#define PMM_BITMAP_SIZE (PMM_MAX_FRAMES / 8)

/* E820 entry structure */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi;
} __attribute__((packed)) e820_entry_t;

static uint8_t bitmap[PMM_BITMAP_SIZE];

static uint32_t total_frames_count = 0;
static uint32_t used_frames_count = 0;

static void bitmap_set(uint32_t frame)
{
    bitmap[frame / 8] |= (1 << (frame % 8));
}

static void bitmap_clear(uint32_t frame)
{
    bitmap[frame / 8] &= ~(1 << (frame % 8));
}

static int bitmap_test(uint32_t frame)
{
    return bitmap[frame / 8] & (1 << (frame % 8));
}

void pmm_init(void)
{
    uint16_t e820_count = *(volatile uint16_t *)E820_COUNT_ADDR;
    e820_entry_t *entries = (e820_entry_t *)E820_MAP_ADDR;

    /* Initially mark every frame as used */
    for (uint32_t i = 0; i < PMM_BITMAP_SIZE; i++) {
        bitmap[i] = 0xFF;
    }

    total_frames_count = 0;
    used_frames_count = 0;

    /* Mark usable E820 regions as free */
    for (uint16_t i = 0; i < e820_count; i++) {

        if (entries[i].type != 1) {
            continue;
        }

        uint64_t start = entries[i].base;
        uint64_t end = entries[i].base + entries[i].length;

        /* This kernel is 32-bit, ignore memory above 4 GB */
        if (start >= 0x100000000ULL) {
            continue;
        }

        if (end > 0x100000000ULL) {
            end = 0x100000000ULL;
        }

        uint32_t first_frame =
            (uint32_t)((start + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE);

        uint32_t last_frame =
            (uint32_t)(end / PMM_FRAME_SIZE);

        if (last_frame > PMM_MAX_FRAMES) {
            last_frame = PMM_MAX_FRAMES;
        }

        for (uint32_t frame = first_frame;
             frame < last_frame;
             frame++) {

            if (bitmap_test(frame)) {
                bitmap_clear(frame);
                total_frames_count++;
            }
        }
    }

    /*
     * Reserve low memory used by BIOS, bootloader,
     * kernel, stacks, E820 table, etc.
     *
     * Reserve first 1 MB for safety.
     */
    for (uint32_t frame = 0;
         frame < (0x100000 / PMM_FRAME_SIZE);
         frame++) {

        if (!bitmap_test(frame)) {
            bitmap_set(frame);

            if (total_frames_count > 0) {
                total_frames_count--;
            }
        }
    }

    /*
     * At this point all remaining clear bits represent
     * allocatable physical page frames.
     */
    used_frames_count = 0;
}

uint32_t pmm_alloc_frame(void)
{
    for (uint32_t frame = 0;
         frame < PMM_MAX_FRAMES;
         frame++) {

        if (!bitmap_test(frame)) {
            bitmap_set(frame);
            used_frames_count++;

            return frame * PMM_FRAME_SIZE;
        }
    }

    return 0;
}

void pmm_free_frame(uint32_t paddr)
{
    if (paddr == 0) {
        return;
    }

    if (paddr % PMM_FRAME_SIZE != 0) {
        return;
    }

    uint32_t frame = paddr / PMM_FRAME_SIZE;

    if (frame >= PMM_MAX_FRAMES) {
        return;
    }

    if (bitmap_test(frame)) {
        bitmap_clear(frame);

        if (used_frames_count > 0) {
            used_frames_count--;
        }
    }
}

uint32_t pmm_total_frames(void)
{
    return total_frames_count;
}

uint32_t pmm_used_frames(void)
{
    return used_frames_count;
}

uint32_t pmm_free_frames(void)
{
    if (total_frames_count >= used_frames_count) {
        return total_frames_count - used_frames_count;
    }

    return 0;
}
