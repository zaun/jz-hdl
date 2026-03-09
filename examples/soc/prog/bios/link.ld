/* Linker script for JZ-HDL SOC RV32I firmware */

MEMORY
{
    ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 16K  /* 4096 x 32-bit words */
    RAM (rwx) : ORIGIN = 0x10000000, LENGTH = 20K  /* 5120 x 32-bit words */
}

SECTIONS
{
    .text :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    } > ROM

    .data :
    {
        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)
    } > RAM AT > ROM

    .bss :
    {
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
    } > RAM
}

ENTRY(_start)
