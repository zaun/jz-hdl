/* Linker script for relocatable JZEX programs.
 * Origin 0 — the JZEX loader relocates via load address. */

MEMORY
{
    RAM (rwx) : ORIGIN = 0, LENGTH = 1M
}

SECTIONS
{
    .text :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        __help_start = .;
        *(.rodata.help)
        __help_end = .;
        *(.rodata.*)
    } > RAM

    .data :
    {
        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)
    } > RAM

    .bss :
    {
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
    } > RAM
}

ENTRY(_start)
