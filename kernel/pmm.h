#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

#define PMM_FRAME_SIZE 4096

void pmm_init(void);

/* Allocate one 4 KB physical frame.
 * Returns physical address, or 0 if none available.
 */
uint32_t pmm_alloc_frame(void);

/* Free one previously allocated physical frame. */
void pmm_free_frame(uint32_t paddr);

/* Memory statistics */
uint32_t pmm_total_frames(void);
uint32_t pmm_used_frames(void);
uint32_t pmm_free_frames(void);

#endif
