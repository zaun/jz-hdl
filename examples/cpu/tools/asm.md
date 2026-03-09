# Assembly Language Reference

## Syntax Overview

The assembly language uses a simple line-based format with mnemonics, operands, and labels.

```
label:           ; Define a label at current address
MNEMONIC arg     ; Instruction with optional operand
                 ; Comments start with semicolon
```

## Instructions

### No Operand Instructions

- **NOP** - No operation
- **LDIN** - Load input (button state) into register A
- **STOU** - Store register A to output (LEDs)
- **HLT** - Halt execution

### Single Operand Instructions

- **LDI arg** - Load immediate value into register A
- **LDA arg** - Load from memory address into register A
- **STA arg** - Store register A to memory address
- **ADD arg** - Add value at memory address to register A
- **SUB arg** - Subtract value at memory address from register A
- **JMP arg** - Jump to address
- **JZ arg** - Jump to address if zero flag is set
- **WAI arg** - Wait for N cycles

## Operand Formats

Arguments can be specified as:

- **Decimal** - `LDI 42`
- **Hexadecimal** - `LDI 0x2A`
- **Labels** - `JMP loop`

All values are 8-bit (0-255).

## Special Directives

- **@MEMSIZE(n)** - Pad output to N bytes (can appear anywhere with comment syntax)

## Example Program

```
loop:
    LDIN
    JZ end
    STOU
    WAI 0xFF
    JMP loop
end:
    HLT
```

## Notes

- Labels must end with `:` and occupy their own line
- The zero flag is set after LDI, LDA, ADD, and SUB operations
- Register A is the accumulator; memory is 256 bytes
- All output is hex-encoded with two characters per byte
