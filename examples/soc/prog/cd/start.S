    .section .text
    .global _start

_start:
    /* Save kernel's return address, call main, then return to kernel */
    addi    sp, sp, -4
    sw      ra, 0(sp)
    jal     ra, main
    lw      ra, 0(sp)
    addi    sp, sp, 4
    ret
