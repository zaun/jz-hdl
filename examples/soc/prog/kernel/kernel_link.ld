/* Linker script for kernel — loaded into SDRAM program space by BIOS */

MEMORY
{
    SDRAM (rwx) : ORIGIN = 0x40000000, LENGTH = 3M
}

SECTIONS
{
    .text :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    } > SDRAM

    .data :
    {
        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)
    } > SDRAM

    .bss :
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
        __bss_end = .;
    } > SDRAM
}

ENTRY(_start)
