    .section .text
    .global _start

_start:
    /* Set stack pointer to top of RAM (byte addr 0x10005000) */
    lui     sp, 0x10005         /* sp = 0x10005000 */

    /* Set trap vector */
    la      t0, _trap_handler
    csrw    mtvec, t0

    /* Enable external interrupts (mie bit 11) */
    li      t0, 0x800
    csrw    mie, t0

    /* Enable global interrupts (mstatus bit 3) */
    csrsi   mstatus, 0x8

    /* Call main */
    jal     ra, main

    /* Halt */
    ecall

    .align 4
    .global _trap_handler
_trap_handler:
    csrr    t0, mcause
    li      t1, 11
    beq     t0, t1, _ecall_entry
    /* Hardware shadow registers handle context save/restore.
       Just set shadow stack pointer and call C handler. */
    lui     sp, 0x10005         /* sp = 0x10005000 (shadow bank) */
    jal     ra, trap_handler
    mret

_ecall_entry:
    /* ECALL does NOT activate shadow registers - we share caller's bank.
       a0-a3 already hold syscall args per C calling convention.
       Save mepc so nested ECALLs (e.g. kernel handler calling BIOS
       sd_read_sector) don't clobber the outer return address. */
    addi    sp, sp, -8
    sw      ra, 4(sp)
    csrr    t0, mepc
    sw      t0, 0(sp)
    jal     ra, ecall_dispatch
    lw      t0, 0(sp)
    addi    t0, t0, 4           /* advance past ECALL instruction */
    csrw    mepc, t0
    lw      ra, 4(sp)
    addi    sp, sp, 8
    mret
