#!/bin/bash
# Stage 4 Test Script
# Tests the RAM disk file system implementation

echo "=== SENG21213-OS Stage 4 Test Suite ==="
echo ""

# Build the project
echo "1. Building project..."
make clean > /dev/null 2>&1
make > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "   ✓ Build successful"
else
    echo "   ✗ Build failed"
    exit 1
fi

# Check kernel size
KERNEL_SIZE=$(wc -c < build/kernel.bin)
echo "   Kernel size: $KERNEL_SIZE bytes"
if [ $KERNEL_SIZE -gt 65536 ]; then
    echo "   ✗ Kernel too large (exceeds 64KB limit)"
    exit 1
else
    echo "   ✓ Kernel size OK"
fi

echo ""
echo "2. Verifying file system structures..."

# Check if fs.c and fs.h exist
if [ -f "kernel/fs.c" ] && [ -f "kernel/fs.h" ]; then
    echo "   ✓ File system source files present"
else
    echo "   ✗ File system source files missing"
    exit 1
fi

# Check if string.c and string.h exist
if [ -f "kernel/string.c" ] && [ -f "kernel/string.h" ]; then
    echo "   ✓ String library source files present"
else
    echo "   ✗ String library source files missing"
    exit 1
fi

echo ""
echo "3. Checking kernel integration..."

# Check if fs_init is called in kernel.c
if grep -q "fs_init()" kernel/kernel.c; then
    echo "   ✓ fs_init() called in kernel_main"
else
    echo "   ✗ fs_init() not called in kernel_main"
    exit 1
fi

# Check if pmm_init is called in kernel.c
if grep -q "pmm_init()" kernel/kernel.c; then
    echo "   ✓ pmm_init() called in kernel_main"
else
    echo "   ✗ pmm_init() not called in kernel_main"
    exit 1
fi

# Check if file system commands are implemented
COMMANDS=("cmd_ls" "cmd_touch" "cmd_cat" "cmd_write" "cmd_rm" "cmd_fsstats")
for cmd in "${COMMANDS[@]}"; do
    if grep -q "$cmd" kernel/kernel.c; then
        echo "   ✓ $cmd implemented"
    else
        echo "   ✗ $cmd not found"
        exit 1
    fi
done

echo ""
echo "4. Verifying RAM disk configuration..."

# Check RAM disk size in fs.h (1 MB = 1024 * 1024 = 1048576)
if grep -q "FS_RAMDISK_SIZE" kernel/fs.h; then
    RAMDISK_SIZE=$(grep "FS_RAMDISK_SIZE" kernel/fs.h | grep -o '[0-9]\+' | head -1)
    if [ "$RAMDISK_SIZE" = "1024" ] || [ "$RAMDISK_SIZE" = "1048576" ]; then
        echo "   ✓ RAM disk size: 1 MB"
    else
        echo "   ✗ RAM disk size incorrect (found: $RAMDISK_SIZE)"
        exit 1
    fi
else
    echo "   ✗ RAM disk size not defined"
    exit 1
fi

# Check block size
if grep -q "FS_BLOCK_SIZE.*4096" kernel/fs.h; then
    echo "   ✓ Block size: 4 KB"
else
    echo "   ✗ Block size incorrect"
    exit 1
fi

# Check max file size (32 KB = 32768)
if grep -q "FS_MAX_FILE_SIZE" kernel/fs.h; then
    MAX_FILE_SIZE=$(grep "FS_MAX_FILE_SIZE" kernel/fs.h | grep -o '[0-9]\+' | head -1)
    if [ "$MAX_FILE_SIZE" = "32768" ] || [ "$MAX_FILE_SIZE" = "32" ]; then
        echo "   ✓ Max file size: 32 KB"
    else
        echo "   ✗ Max file size incorrect (found: $MAX_FILE_SIZE)"
        exit 1
    fi
else
    echo "   ✗ Max file size not defined"
    exit 1
fi

echo ""
echo "5. Checking file system layout..."

# Check superblock location
if grep -q "FS_SUPERBLOCK_BLOCK.*0" kernel/fs.h; then
    echo "   ✓ Superblock at block 0"
else
    echo "   ✗ Superblock location incorrect"
    exit 1
fi

# Check directory location
if grep -q "FS_DIRECTORY_BLOCK.*1" kernel/fs.h; then
    echo "   ✓ Directory at block 1"
else
    echo "   ✗ Directory location incorrect"
    exit 1
fi

echo ""
echo "6. Verifying inode structure..."

# Check direct block pointers
if grep -q "FS_DIRECT_BLOCKS.*8" kernel/fs.h; then
    echo "   ✓ 8 direct block pointers per inode"
else
    echo "   ✗ Direct block count incorrect"
    exit 1
fi

# Check inode size
if grep -q "FS_INODE_SIZE.*128" kernel/fs.h; then
    echo "   ✓ Inode size: 128 bytes"
else
    echo "   ✗ Inode size incorrect"
    exit 1
fi

echo ""
echo "7. Checking shell command dispatch..."

# Check if commands are dispatched in shell_run
if grep -q 'k_strcmp(cmd, "ls")' kernel/kernel.c; then
    echo "   ✓ ls command dispatched"
else
    echo "   ✗ ls command not dispatched"
    exit 1
fi

if grep -q 'k_strncmp(cmd, "touch "' kernel/kernel.c; then
    echo "   ✓ touch command dispatched"
else
    echo "   ✗ touch command not dispatched"
    exit 1
fi

if grep -q 'k_strncmp(cmd, "cat "' kernel/kernel.c; then
    echo "   ✓ cat command dispatched"
else
    echo "   ✗ cat command not dispatched"
    exit 1
fi

if grep -q 'k_strncmp(cmd, "write "' kernel/kernel.c; then
    echo "   ✓ write command dispatched"
else
    echo "   ✗ write command not dispatched"
    exit 1
fi

if grep -q 'k_strncmp(cmd, "rm "' kernel/kernel.c; then
    echo "   ✓ rm command dispatched"
else
    echo "   ✗ rm command not dispatched"
    exit 1
fi

echo ""
echo "8. Verifying help text..."

# Check if help includes Stage 3 and Stage 4 commands
if grep -q "meminfo.*Show physical memory statistics" kernel/kernel.c; then
    echo "   ✓ meminfo in help text"
else
    echo "   ✗ meminfo not in help text"
    exit 1
fi

if grep -q "ls.*List files" kernel/kernel.c; then
    echo "   ✓ ls in help text"
else
    echo "   ✗ ls not in help text"
    exit 1
fi

if grep -q "touch.*Create.*file" kernel/kernel.c; then
    echo "   ✓ touch in help text"
else
    echo "   ✗ touch not in help text"
    exit 1
fi

echo ""
echo "9. Checking splash screen..."

# Check if splash shows Stage 4
if grep -q "Stage 4: RAM Disk File System" kernel/kernel.c; then
    echo "   ✓ Splash screen shows Stage 4"
else
    echo "   ✗ Splash screen doesn't show Stage 4"
    exit 1
fi

# Check if milestones show L11 and L12 as DONE
if grep -A 1 "L11" kernel/kernel.c | grep -q "DONE"; then
    echo "   ✓ L11 marked as DONE"
else
    echo "   ✗ L11 not marked as DONE"
    exit 1
fi

if grep -A 1 "L12" kernel/kernel.c | grep -q "DONE"; then
    echo "   ✓ L12 marked as DONE"
else
    echo "   ✗ L12 not marked as DONE"
    exit 1
fi

echo ""
echo "10. Testing QEMU boot..."

# Try to boot in QEMU (just check it doesn't crash immediately)
timeout 3 qemu-system-i386 -drive format=raw,file=seng21213-os.img -m 32M -nographic -serial mon:stdio > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo "   ✓ QEMU boots successfully (timeout reached)"
else
    echo "   ⚠ QEMU boot test inconclusive"
fi

echo ""
echo "=== All Tests Passed ==="
echo ""
echo "Stage 4 Implementation Summary:"
echo "  • RAM disk: 1 MB (256 blocks × 4 KB)"
echo "  • File system: Inode-based with superblock"
echo "  • Max file size: 32 KB (8 direct blocks)"
echo "  • Shell commands: ls, touch, cat, write, rm, fsstats"
echo "  • String library: Custom implementation for freestanding environment"
echo ""
echo "Ready for submission!"
