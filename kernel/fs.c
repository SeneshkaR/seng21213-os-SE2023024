/* =============================================================================
 * SENG21213-OS :: RAM Disk File System (Stage 4 – L12)
 * File   : kernel/fs.c
 *
 * PURPOSE
 *   Implementation of the in-memory RAM disk file system.
 *   Provides file creation, reading, writing, deletion, and directory listing.
 *
 * REFERENCE
 *   L12 §2 – File system structures and inode-based allocation
 * ============================================================================*/

#include "fs.h"
#include "vga.h"
#include "../include/string.h"

/* ---------------------------------------------------------------------------
 * RAM Disk - 1 MB byte array in BSS
 * --------------------------------------------------------------------------*/
static uint8_t ramdisk[FS_RAMDISK_SIZE];

/* File descriptor table */
static file_desc_t open_files[FS_MAX_OPEN_FILES];

/* ---------------------------------------------------------------------------
 * Helper: Get pointer to block in RAM disk
 * --------------------------------------------------------------------------*/
static inline void *fs_block_ptr(uint32_t block_num)
{
    return &ramdisk[block_num * FS_BLOCK_SIZE];
}

/* ---------------------------------------------------------------------------
 * Helper: Get superblock pointer
 * --------------------------------------------------------------------------*/
static inline superblock_t *fs_superblock(void)
{
    return (superblock_t *)fs_block_ptr(FS_SUPERBLOCK_BLOCK);
}

/* ---------------------------------------------------------------------------
 * Helper: Get directory entries pointer
 * --------------------------------------------------------------------------*/
static inline dir_entry_t *fs_directory(void)
{
    return (dir_entry_t *)fs_block_ptr(FS_DIRECTORY_BLOCK);
}

/* ---------------------------------------------------------------------------
 * Helper: Get block bitmap pointer
 * --------------------------------------------------------------------------*/
static inline uint8_t *fs_block_bitmap(void)
{
    return (uint8_t *)fs_block_ptr(FS_BLOCK_BITMAP_BLOCK);
}

/* ---------------------------------------------------------------------------
 * Helper: Get inode bitmap pointer
 * --------------------------------------------------------------------------*/
static inline uint8_t *fs_inode_bitmap(void)
{
    return (uint8_t *)fs_block_ptr(FS_INODE_BITMAP_BLOCK);
}

/* ---------------------------------------------------------------------------
 * Helper: Get inode pointer by inode number
 * --------------------------------------------------------------------------*/
static inline inode_t *fs_get_inode(uint32_t inode_num)
{
    uint32_t block_offset = inode_num / FS_INODES_PER_BLOCK;
    uint32_t index_in_block = inode_num % FS_INODES_PER_BLOCK;
    inode_t *inode_block = (inode_t *)fs_block_ptr(FS_INODE_START_BLOCK + block_offset);
    return &inode_block[index_in_block];
}

/* ---------------------------------------------------------------------------
 * Helper: Check if bit is set in bitmap
 * --------------------------------------------------------------------------*/
static inline int bitmap_test(uint8_t *bitmap, uint32_t bit)
{
    return bitmap[bit / 8] & (1 << (bit % 8));
}

/* ---------------------------------------------------------------------------
 * Helper: Set bit in bitmap
 * --------------------------------------------------------------------------*/
static inline void bitmap_set(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8] |= (1 << (bit % 8));
}

/* ---------------------------------------------------------------------------
 * Helper: Clear bit in bitmap
 * --------------------------------------------------------------------------*/
static inline void bitmap_clear(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

/* ---------------------------------------------------------------------------
 * Helper: Allocate a data block
 * Returns block number or 0 if none available
 * --------------------------------------------------------------------------*/
static uint32_t fs_alloc_block(void)
{
    uint8_t *bitmap = fs_block_bitmap();
    superblock_t *sb = fs_superblock();

    for (uint32_t i = 0; i < FS_DATA_BLOCKS; i++) {
        if (!bitmap_test(bitmap, i)) {
            bitmap_set(bitmap, i);
            sb->free_blocks--;
            return FS_DATA_START_BLOCK + i;
        }
    }
    return 0; /* No free blocks */
}

/* ---------------------------------------------------------------------------
 * Helper: Free a data block
 * --------------------------------------------------------------------------*/
static void fs_free_block(uint32_t block_num)
{
    if (block_num < FS_DATA_START_BLOCK || block_num >= FS_TOTAL_BLOCKS) {
        return;
    }

    uint8_t *bitmap = fs_block_bitmap();
    superblock_t *sb = fs_superblock();
    uint32_t bit = block_num - FS_DATA_START_BLOCK;

    if (bitmap_test(bitmap, bit)) {
        bitmap_clear(bitmap, bit);
        sb->free_blocks++;
    }
}

/* ---------------------------------------------------------------------------
 * Helper: Allocate an inode
 * Returns inode number or 0 if none available
 * --------------------------------------------------------------------------*/
static uint32_t fs_alloc_inode(void)
{
    uint8_t *bitmap = fs_inode_bitmap();
    superblock_t *sb = fs_superblock();

    for (uint32_t i = 1; i < FS_MAX_INODES; i++) { /* Start at 1, 0 is reserved */
        if (!bitmap_test(bitmap, i)) {
            bitmap_set(bitmap, i);
            sb->free_inodes--;

            /* Initialize inode */
            inode_t *inode = fs_get_inode(i);
            memset(inode, 0, sizeof(inode_t));

            return i;
        }
    }
    return 0; /* No free inodes */
}

/* ---------------------------------------------------------------------------
 * Helper: Free an inode
 * --------------------------------------------------------------------------*/
static void fs_free_inode(uint32_t inode_num)
{
    if (inode_num == 0 || inode_num >= FS_MAX_INODES) {
        return;
    }

    uint8_t *bitmap = fs_inode_bitmap();
    superblock_t *sb = fs_superblock();

    if (bitmap_test(bitmap, inode_num)) {
        bitmap_clear(bitmap, inode_num);
        sb->free_inodes++;
    }
}

/* ---------------------------------------------------------------------------
 * Helper: Find inode number for filename
 * Returns inode number or 0 if not found
 * --------------------------------------------------------------------------*/
static uint32_t fs_find_inode(const char *filename)
{
    dir_entry_t *dir = fs_directory();

    for (int i = 0; i < FS_MAX_DIR_ENTRIES; i++) {
        if (dir[i].inode != 0 && strcmp(dir[i].name, filename) == 0) {
            return dir[i].inode;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * Helper: Find free directory entry slot
 * Returns index or -1 if none available
 * --------------------------------------------------------------------------*/
static int fs_find_free_dir_entry(void)
{
    dir_entry_t *dir = fs_directory();

    for (int i = 0; i < FS_MAX_DIR_ENTRIES; i++) {
        if (dir[i].inode == 0) {
            return i;
        }
    }
    return -1;
}

/* ---------------------------------------------------------------------------
 * Helper: Find free file descriptor
 * Returns fd or -1 if none available
 * --------------------------------------------------------------------------*/
static int fs_find_free_fd(void)
{
    for (int i = 0; i < FS_MAX_OPEN_FILES; i++) {
        if (!open_files[i].valid) {
            return i;
        }
    }
    return -1;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================*/

/* ---------------------------------------------------------------------------
 * fs_init - Initialize the file system
 * L12 §2 – Format the RAM disk with superblock, bitmaps, and empty directory
 * --------------------------------------------------------------------------*/
void fs_init(void)
{
    /* Clear entire RAM disk */
    memset(ramdisk, 0, FS_RAMDISK_SIZE);

    /* Initialize superblock */
    superblock_t *sb = fs_superblock();
    sb->magic = FS_MAGIC;
    sb->total_blocks = FS_TOTAL_BLOCKS;
    sb->inode_blocks = FS_INODE_BLOCKS;
    sb->data_blocks = FS_DATA_BLOCKS;
    sb->free_blocks = FS_DATA_BLOCKS;
    sb->free_inodes = FS_MAX_INODES - 1; /* Inode 0 is reserved */

    /* Initialize file descriptor table */
    for (int i = 0; i < FS_MAX_OPEN_FILES; i++) {
        open_files[i].valid = 0;
    }

    vga_puts_color("  [FS] RAM disk file system initialized\n", VGA_LIGHT_GREEN, VGA_BLACK);
    vga_printf("       %u KB total, %u data blocks, %u inodes max\n",
               FS_RAMDISK_SIZE / 1024, FS_DATA_BLOCKS, FS_MAX_INODES);
}

/* ---------------------------------------------------------------------------
 * fs_create_file - Create a new empty file
 * L12 §2 – Allocate inode and directory entry
 * --------------------------------------------------------------------------*/
int fs_create_file(const char *filename)
{
    if (!filename || strlen(filename) == 0 || strlen(filename) >= FS_FILENAME_LEN) {
        return -1;
    }

    /* Check if file already exists */
    if (fs_find_inode(filename) != 0) {
        return -1; /* File exists */
    }

    /* Find free directory entry */
    int dir_idx = fs_find_free_dir_entry();
    if (dir_idx < 0) {
        return -1; /* Directory full */
    }

    /* Allocate inode */
    uint32_t inode_num = fs_alloc_inode();
    if (inode_num == 0) {
        return -1; /* No free inodes */
    }

    /* Create directory entry */
    dir_entry_t *dir = fs_directory();
    strncpy(dir[dir_idx].name, filename, FS_FILENAME_LEN - 1);
    dir[dir_idx].name[FS_FILENAME_LEN - 1] = '\0';
    dir[dir_idx].inode = inode_num;

    return 0;
}

/* ---------------------------------------------------------------------------
 * fs_open - Open a file
 * L12 §2 – Find inode and allocate file descriptor
 * --------------------------------------------------------------------------*/
int fs_open(const char *filename, int create)
{
    if (!filename) {
        return -1;
    }

    uint32_t inode_num = fs_find_inode(filename);

    /* Create file if it doesn't exist and create flag is set */
    if (inode_num == 0 && create) {
        if (fs_create_file(filename) != 0) {
            return -1;
        }
        inode_num = fs_find_inode(filename);
    }

    if (inode_num == 0) {
        return -1; /* File not found */
    }

    /* Find free file descriptor */
    int fd = fs_find_free_fd();
    if (fd < 0) {
        return -1; /* No free descriptors */
    }

    /* Initialize file descriptor */
    open_files[fd].inode = inode_num;
    open_files[fd].offset = 0;
    open_files[fd].valid = 1;

    return fd;
}

/* ---------------------------------------------------------------------------
 * fs_close - Close a file descriptor
 * --------------------------------------------------------------------------*/
int fs_close(int fd)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !open_files[fd].valid) {
        return -1;
    }

    open_files[fd].valid = 0;
    return 0;
}

/* ---------------------------------------------------------------------------
 * fs_read - Read from a file
 * L12 §2 – Read data from inode's data blocks
 * --------------------------------------------------------------------------*/
int fs_read(int fd, void *buffer, uint32_t count)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !open_files[fd].valid) {
        return -1;
    }

    if (!buffer || count == 0) {
        return 0;
    }

    file_desc_t *desc = &open_files[fd];
    inode_t *inode = fs_get_inode(desc->inode);

    /* Check if reading past end of file */
    if (desc->offset >= inode->size) {
        return 0;
    }

    /* Limit read to file size */
    if (desc->offset + count > inode->size) {
        count = inode->size - desc->offset;
    }

    uint32_t bytes_read = 0;
    uint8_t *buf = (uint8_t *)buffer;

    while (bytes_read < count) {
        uint32_t block_index = desc->offset / FS_BLOCK_SIZE;
        uint32_t offset_in_block = desc->offset % FS_BLOCK_SIZE;

        if (block_index >= FS_DIRECT_BLOCKS) {
            break; /* Past end of direct blocks */
        }

        uint32_t data_block = inode->blocks[block_index];
        if (data_block == 0) {
            break; /* Unallocated block */
        }

        /* Calculate how much to read from this block */
        uint32_t to_read = FS_BLOCK_SIZE - offset_in_block;
        if (to_read > count - bytes_read) {
            to_read = count - bytes_read;
        }

        /* Copy data from RAM disk to buffer */
        void *block_ptr = fs_block_ptr(data_block);
        memcpy(buf + bytes_read, (uint8_t *)block_ptr + offset_in_block, to_read);

        bytes_read += to_read;
        desc->offset += to_read;
    }

    return bytes_read;
}

/* ---------------------------------------------------------------------------
 * fs_write - Write to a file
 * L12 §2 – Write data to inode's data blocks, allocating as needed
 * --------------------------------------------------------------------------*/
int fs_write(int fd, const void *buffer, uint32_t count)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !open_files[fd].valid) {
        return -1;
    }

    if (!buffer || count == 0) {
        return 0;
    }

    file_desc_t *desc = &open_files[fd];
    inode_t *inode = fs_get_inode(desc->inode);

    uint32_t bytes_written = 0;
    const uint8_t *buf = (const uint8_t *)buffer;

    while (bytes_written < count) {
        uint32_t block_index = desc->offset / FS_BLOCK_SIZE;
        uint32_t offset_in_block = desc->offset % FS_BLOCK_SIZE;

        if (block_index >= FS_DIRECT_BLOCKS) {
            break; /* File too large */
        }

        /* Allocate block if needed */
        if (inode->blocks[block_index] == 0) {
            uint32_t new_block = fs_alloc_block();
            if (new_block == 0) {
                break; /* No free blocks */
            }
            inode->blocks[block_index] = new_block;
        }

        /* Calculate how much to write to this block */
        uint32_t to_write = FS_BLOCK_SIZE - offset_in_block;
        if (to_write > count - bytes_written) {
            to_write = count - bytes_written;
        }

        /* Copy data from buffer to RAM disk */
        void *block_ptr = fs_block_ptr(inode->blocks[block_index]);
        memcpy((uint8_t *)block_ptr + offset_in_block, buf + bytes_written, to_write);

        bytes_written += to_write;
        desc->offset += to_write;

        /* Update file size if we wrote past the end */
        if (desc->offset > inode->size) {
            inode->size = desc->offset;
        }
    }

    return bytes_written;
}

/* ---------------------------------------------------------------------------
 * fs_unlink - Delete a file
 * L12 §2 – Free inode, data blocks, and directory entry
 * --------------------------------------------------------------------------*/
int fs_unlink(const char *filename)
{
    if (!filename) {
        return -1;
    }

    uint32_t inode_num = fs_find_inode(filename);
    if (inode_num == 0) {
        return -1; /* File not found */
    }

    /* Free data blocks */
    inode_t *inode = fs_get_inode(inode_num);
    for (int i = 0; i < FS_DIRECT_BLOCKS; i++) {
        if (inode->blocks[i] != 0) {
            fs_free_block(inode->blocks[i]);
        }
    }

    /* Free inode */
    fs_free_inode(inode_num);

    /* Remove directory entry */
    dir_entry_t *dir = fs_directory();
    for (int i = 0; i < FS_MAX_DIR_ENTRIES; i++) {
        if (dir[i].inode == inode_num) {
            dir[i].inode = 0;
            dir[i].name[0] = '\0';
            break;
        }
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * fs_list_dir - List all files in the directory
 * L12 §2 – Display directory entries with file sizes
 * --------------------------------------------------------------------------*/
int fs_list_dir(void)
{
    dir_entry_t *dir = fs_directory();
    int count = 0;

    vga_puts_color("\n  Directory listing:\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");

    for (int i = 0; i < FS_MAX_DIR_ENTRIES; i++) {
        if (dir[i].inode != 0) {
            inode_t *inode = fs_get_inode(dir[i].inode);
            vga_printf("  %-28s  %5u bytes\n", dir[i].name, inode->size);
            count++;
        }
    }

    if (count == 0) {
        vga_puts("  (empty)\n");
    }

    vga_printf("\n  %d file(s)\n\n", count);
    return count;
}

/* ---------------------------------------------------------------------------
 * fs_print_stats - Print file system statistics
 * --------------------------------------------------------------------------*/
void fs_print_stats(void)
{
    superblock_t *sb = fs_superblock();

    vga_puts_color("\n  RAM Disk File System Statistics\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_printf("  Total size       : %u KB\n", FS_RAMDISK_SIZE / 1024);
    vga_printf("  Block size       : %u bytes\n", FS_BLOCK_SIZE);
    vga_printf("  Total blocks     : %u\n", sb->total_blocks);
    vga_printf("  Data blocks      : %u\n", sb->data_blocks);
    vga_printf("  Free data blocks : %u\n", sb->free_blocks);
    vga_printf("  Used data blocks : %u\n", sb->data_blocks - sb->free_blocks);
    vga_printf("  Max inodes       : %u\n", FS_MAX_INODES);
    vga_printf("  Free inodes      : %u\n", sb->free_inodes);
    vga_printf("  Max file size    : %u KB\n", FS_MAX_FILE_SIZE / 1024);
    vga_puts("\n");
}
