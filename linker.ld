/* =============================================================================
 * SENG21213-OS :: Linker Script
 * File   : linker.ld
 *
 * Places the kernel binary starting at physical address 0x10000.
 * The bootloader loads the kernel to 0x1000:0x0000 = physical 0x10000.
 * kernel_entry.asm is the very first object, so _start is at 0x10000.
 * ============================================================================*/

OUTPUT_FORMAT(elf32-i386)

ENTRY(_start)

PHDRS
{
    text    PT_LOAD FLAGS(5);  /* R_X: Read + Execute */
    rodata  PT_LOAD FLAGS(4);  /* R__: Read only */
    data    PT_LOAD FLAGS(6);  /* RW_: Read + Write */
}

SECTIONS {
    . = 0x10000;          /* Load address must match bootloader's jump target */

    .text : {
        *(.text)          /* All code sections */
    } :text

    .rodata : {
        *(.rodata*)       /* Read-only data (string literals, const tables) */
    } :rodata

    .data : {
        *(.data)          /* Initialised global/static variables */
    } :data

    .bss : {
        *(COMMON)
        *(.bss)           /* Uninitialised data – zeroed by convention */
    } :data

    /DISCARD/ : {
        *(.eh_frame)      /* Exception frames – not needed in a bare kernel */
        *(.comment)
    }
}
