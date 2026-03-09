    .section .text
    .global _start

_start:
    /* Stack is already set up by BIOS in RAM */

    /* Zero BSS */
    la      t0, __bss_start
    la      t1, __bss_end
1:  bge     t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b
2:

    /* Call main */
    jal     ra, main

    /* Halt if main returns */
3:  j       3b
