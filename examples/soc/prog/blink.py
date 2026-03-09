#!/usr/bin/env python3
"""Generate blink program hex file for the simple 32-bit SOC.

Two patterns alternate on each Tick:
  Pattern 1 (Block RAM):  Counting 0..63 on LEDs
  Pattern 2 (SDRAM):      Knight Rider sweep (runs twice per Tick)

Init phase:
  - Fill RAM[0x1000..0x103F] with counting values 0..63
  - Fill SDRAM[0x4000..0x4009] with 10-frame knight rider sweep

Memory map:
  ROM:   0x0000-0x0FFF  (1024 words)
  RAM:   0x1000-0x1FFF  (stack at 0x1FFF)
  LED:   0x2000         (lower 6 bits drive LEDs)
  UART:  0x3000         (write=TX byte, read bit0=ready)
  SDRAM: 0x4000-0x5FFF  (8KB window)

Instruction format (32-bit):
  [31:24] opcode
  [23:16] operand (unused)
  [15:0]  immediate/address
"""

import sys

# Opcodes
NOP   = 0x00
LDI_A = 0x01
LDI_X = 0x02
LD_A  = 0x03
ST_A  = 0x04
ADD   = 0x05
SUB   = 0x06
AND   = 0x07
OR    = 0x08
XOR   = 0x09
CMP   = 0x0A
JMP   = 0x0B
BEQ   = 0x0C
BNE   = 0x0D
PUSH  = 0x0E
POP   = 0x0F
CALL  = 0x10
RET   = 0x11
HLT   = 0x12
INC   = 0x13
DEC   = 0x14
SHL   = 0x15
SHR   = 0x16
LD_X  = 0x17
ST_X  = 0x18

LED_ADDR    = 0x2000
UART_ADDR   = 0x3000
RAM_BASE    = 0x1000
RAM_COUNT   = 64
SDRAM_BASE  = 0x4000
SDRAM_COUNT = 10
SCRATCH     = 0x1102  # putchar save
OUTER_CNT   = 0x1100  # delay outer counter

# Knight rider pattern: single LED bounces across 6 LEDs
# 000001 → 000010 → ... → 100000 → 010000 → ... → 000010
KNIGHT_RIDER = [0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x02]

def encode(opcode, operand=0, imm=0):
    return ((opcode & 0xFF) << 24) | ((operand & 0xFF) << 16) | (imm & 0xFFFF)

def emit_string(prog, s, putchar_addr):
    for ch in s:
        prog.append(encode(LDI_A, imm=ord(ch)))
        prog.append(encode(CALL, imm=putchar_addr))

def main():
    init_str = "Init\n"
    init2_str = "Init2\n"
    done_str = "Init done\n"
    tick_str = "Tick\n"

    # --- Pre-calculate sizes to determine subroutine addresses ---

    # Phase 1: Init
    #   print "Init\n"               = 5*2 = 10
    #   fill RAM (64 words)          = 64*2 = 128
    #   print "Init2\n"              = 6*2 = 12
    #   fill SDRAM (10 words)        = 10*2 = 20
    #   print "Init done\n"          = 10*2 = 20
    phase1_size = (len(init_str)*2 + RAM_COUNT*2
                   + len(init2_str)*2 + SDRAM_COUNT*2
                   + len(done_str)*2)

    # Phase 2: Main loop
    #   counting pattern:  64*3 = 192
    #   "Tick\n":          5*2 = 10
    #   knight rider x2:  10*3*2 = 60
    #   "Tick\n":          5*2 = 10
    #   JMP:               1
    display_start = phase1_size
    phase2_size = (RAM_COUNT*3 + len(tick_str)*2
                   + SDRAM_COUNT*3*2 + len(tick_str)*2 + 1)

    putchar_addr = display_start + phase2_size
    delay_addr = putchar_addr + 6  # putchar is 6 instructions

    # --- Emit Phase 1: Init ---
    program = []

    emit_string(program, init_str, putchar_addr)

    # Fill RAM with counting pattern
    for i in range(RAM_COUNT):
        program.append(encode(LDI_A, imm=i))
        program.append(encode(ST_A, imm=RAM_BASE + i))

    emit_string(program, init2_str, putchar_addr)

    # Fill SDRAM with knight rider pattern
    for i in range(SDRAM_COUNT):
        program.append(encode(LDI_A, imm=KNIGHT_RIDER[i]))
        program.append(encode(ST_A, imm=SDRAM_BASE + i))

    emit_string(program, done_str, putchar_addr)

    # --- Emit Phase 2: Main loop ---
    assert len(program) == display_start

    # Pattern 1: Counting from Block RAM
    for i in range(RAM_COUNT):
        program.append(encode(LD_A, imm=RAM_BASE + i))
        program.append(encode(ST_A, imm=LED_ADDR))
        program.append(encode(CALL, imm=delay_addr))

    emit_string(program, tick_str, putchar_addr)

    # Pattern 2: Knight rider from SDRAM (play twice)
    for _pass in range(2):
        for i in range(SDRAM_COUNT):
            program.append(encode(LD_A, imm=SDRAM_BASE + i))
            program.append(encode(ST_A, imm=LED_ADDR))
            program.append(encode(CALL, imm=delay_addr))

    emit_string(program, tick_str, putchar_addr)

    program.append(encode(JMP, imm=display_start))

    # --- putchar subroutine ---
    assert len(program) == putchar_addr

    program.append(encode(ST_A, imm=SCRATCH))                # save char
    poll_addr = len(program)
    program.append(encode(LD_A, imm=UART_ADDR))              # A = UART status
    program.append(encode(BEQ, imm=poll_addr))               # if A==0, retry
    program.append(encode(LD_A, imm=SCRATCH))                # restore char
    program.append(encode(ST_A, imm=UART_ADDR))              # TX byte
    program.append(encode(RET))

    assert len(program) == delay_addr

    # --- Delay subroutine (~250ms at 54MHz) ---
    program.append(encode(PUSH))                             # save A
    program.append(encode(LDI_A, imm=26))                    # outer = 26
    program.append(encode(ST_A, imm=OUTER_CNT))

    outer_top = len(program)
    program.append(encode(LDI_A, imm=0xFFFF))                # inner = 0xFFFF

    inner_loop = len(program)
    program.append(encode(DEC))                              # A--
    program.append(encode(BNE, imm=inner_loop))              # if A != 0, loop

    program.append(encode(LD_A, imm=OUTER_CNT))              # A = outer count
    program.append(encode(DEC))                              # A--
    program.append(encode(ST_A, imm=OUTER_CNT))              # store back
    program.append(encode(BNE, imm=outer_top))               # if outer != 0, repeat

    program.append(encode(POP))                              # restore A
    program.append(encode(RET))

    total = len(program)

    # Pad to 1024 words
    while len(program) < 1024:
        program.append(encode(NOP))

    # Write hex file
    outfile = sys.argv[1] if len(sys.argv) > 1 else "blink.hex"
    with open(outfile, 'w') as f:
        for word in program:
            f.write(f"{word:08X}\n")

    print(f"Generated {outfile}: {total} used / {len(program)} words, "
          f"putchar at 0x{putchar_addr:04X}, "
          f"delay at 0x{delay_addr:04X}, "
          f"display loop at 0x{display_start:04X}")

if __name__ == "__main__":
    main()
