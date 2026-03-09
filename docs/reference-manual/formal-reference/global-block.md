---
title: Global Constant Blocks (@global)
lang: en-US

layout: doc
outline: deep
---

# Global Constant Blocks (@global)

## Purpose

Define named collections of sized literal constants visible to all modules and projects within the compilation unit. Useful for instruction encodings, CSR numbers, protocol constants, and other bit-patterns referenced throughout the design.

## Syntax

```text
@global <global_name>
  <const_id> = <sized_literal>;
  <const_id> = <sized_literal>;
  ...
@endglob
```

Where each sized literal follows the literal syntax: `width'base_value`

## Semantics

- The global name creates a namespace root.
- Constants are referenced as `global_name.CONST_ID`.
- Each constant is a sized literal with explicit, unambiguous bit-width.
- Width must be >= intrinsic width of the value (standard overflow rules apply).
- Multiple `@global` blocks are allowed; each must have a unique name.

## Value semantics

Unlike CONFIG and CONST, global constants **are values** and may be used anywhere a value expression is permitted:

- RHS of assignments (ASYNCHRONOUS or SYNCHRONOUS)
- Operands to operators
- Arguments to intrinsic operators
- Part of expressions, concatenations, conditionals

Standard width rules apply: same width as target for direct assignment; use `=z`/`=s` modifiers for extension.

## Errors

- Duplicate global name across the compilation
- Duplicate const_id inside a single block
- Invalid identifier syntax
- Forward reference inside a block
- Non-integer or negative expressions
- Value that is not a properly-sized literal (bare decimal, CONST reference, or CONFIG reference)
- Literal overflow (intrinsic width exceeds declared width)
- Attempting to assign to a global constant (read-only)

## Example

```text
@global ISA
  INST_ADD   = 17'b0110011_0000000_000;
  INST_SUB   = 17'b0110011_0100000_000;
  INST_SLL   = 17'b0110011_0000000_001;
  INST_AUIPC = 10'b0010111;
  INST_LUI   = 10'b0110111;
@endglob
```

Usage inside a module:
```text
ASYNCHRONOUS {
  IF (opcode == ISA.INST_ADD) {
    alu_op <= 4'b0001;
  }
}
```
