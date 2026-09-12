/* =============================================================================
 * SENG21213-OS :: RAM Disk File System (Stage 4 – L12)
 * File   : kernel/fs.h
 *
 * PURPOSE
 *   Header for the in-memory RAM disk file system.
 *   Implements a simple file system with:
 *     - Superblock with magic number and metadata
 *     - Block and inode bitmaps for allocation tracking
 *     - Inodes with direct block pointers (max 32 KB per file)
 *     - Flat directory structure
 *     - POSIX-style API for file operations
 *
 * LAYOUT (1 MB RAM disk = 256 blocks of 4 KB each)
 *   Block 0:    Superblock
 *   Block 1:    Directory entries
 *   Block 2:    Block bitmap (256 bits = 32 bytes)
 *   Block 3:    Inode bitmap (256 bits = 32 bytes)
 *   Blocks 4-35: Inodes (32 blocks × 32 inodes = 1024 inodes max)
 *   Blocks 36-255: Data blocks (220 blocks × 4 KB = 880 KB available)
 *
 * REFERENCE
 *   L12 §2 – File system structures and inode-based allocation
 * ============================================================================*/

#ifndef FS_H
#define FS_H

#include "../include/types.h"

/* ---------------------------------------------------------------------------
 * Constants
 * --------------------------------------------------------------------------*/
#define FS_BLOCK_SIZE       4096        /* 4 KB per block */
#define FS_RAMDISK_SIZE     (1024 * 1024) /* 1 MB total */
#define FS_TOTAL_BLOCKS     (FS_RAMDISK_SIZE / FS_BLOCK_SIZE) /* 256 blocks */

#define FS_MAGIC            0x53454E47  /* "SENG" in hex */

/* Block allocation */
#define FS_SUPERBLOCK_BLOCK 0
#define FS_DIRECTORY_BLOCK  1
#define FS_BLOCK_BITMAP_BLOCK 2
#define FS_INODE_BITMAP_BLOCK 3
#define FS_INODE_START_BLOCK 4
#define FS_INODE_BLOCKS     32          /* 32 blocks for inodes */
#define FS_DATA_START_BLOCK 36          /* Data blocks start here */
#define FS_DATA_BLOCKS      (FS_TOTAL_BLOCKS - FS_DATA_START_BLOCK) /* 220 blocks */

/* Inode constants */
#define FS_INODE_SIZE       128         /* 128 bytes per inode */
#define FS_INODES_PER_BLOCK (FS_BLOCK_SIZE / FS_INODE_SIZE) /* 32 inodes per block */
#define FS_MAX_INODES       (FS_INODE_BLOCKS * FS_INODES_PER_BLOCK) /* 1024 inodes */
#define FS_DIRECT_BLOCKS    8           /* 8 direct block pointers per inode */
#define FS_MAX_FILE_SIZE    (FS_DIRECT_BLOCKS * FS_BLOCK_SIZE) /* 32 KB max */

/* Directory constants */
#define FS_FILENAME_LEN     28          /* 28 bytes for filename */
#define FS_DIR_ENTRY_SIZE   32          /* 32 bytes per directory entry */
#define FS_MAX_DIR_ENTRIES  (FS_BLOCK_SIZE / FS_DIR_ENTRY_SIZE) /* 128 entries per block */

/* ---------------------------------------------------------------------------
 * Structures
 * --------------------------------------------------------------------------*/

/* Superblock - stored at block 0 */
typedef struct {
    uint32_t magic;             /* Magic number (FS_MAGIC) */
    uint32_t total_blocks;      /* Total blocks in file system */
    uint32_t inode_blocks;      /* Number of blocks for inodes */
    uint32_t data_blocks;       /* Number of data blocks */
    uint32_t free_blocks;       /* Count of free data blocks */
    uint32_t free_inodes;       /* Count of free inodes */
} __attribute__((packed)) superblock_t;

/* Inode - file metadata */
typedef struct {
    uint32_t size;              /* File size in bytes */
    uint32_t blocks[FS_DIRECT_BLOCKS]; /* Direct block pointers */
    uint8_t  reserved[FS_INODE_SIZE - 4 - (FS_DIRECT_BLOCKS * 4)]; /* Padding */
} __attribute__((packed)) inode_t;

/* Directory entry - maps filename to inode */
typedef struct {
    char     name[FS_FILENAME_LEN]; /* Filename (null-terminated) */
    uint32_t inode;                 /* Inode number (0 = unused) */
} __attribute__((packed)) dir_entry_t;

/* File descriptor - for open files */
typedef struct {
    uint32_t inode;         /* Inode number */
    uint32_t offset;        /* Current read/write position */
    int      valid;         /* 1 if this descriptor is in use */
} file_desc_t;

#define FS_MAX_OPEN_FILES 16

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------*/

/* Initialization */
void fs_init(void);

/* File operations */
int  fs_open(const char *filename, int create);
int  fs_close(int fd);
int  fs_read(int fd, void *buffer, uint32_t count);
int  fs_write(int fd, const void *buffer, uint32_t count);
int  fs_unlink(const char *filename);

/* Directory operations */
int  fs_list_dir(void);

/* Utility */
int  fs_create_file(const char *filename);
void fs_print_stats(void);

#endif /* FS_H */
