---
title: Type System
lang: en-US

layout: doc
outline: deep
---

# Type System

## Literals

**Syntax:** `width'base_value`

- **width**: positive integer or `CONST` name
- **base**: `b`, `d`, or `h`
- Binary allows `0`, `1`, `x`, `z` (underscore `_` for readability)
- Decimal/hex do not allow `x` or `z`
- Unsized literals are not permitted
- `CONFIG.NAME` and `CONST` identifiers are compile-time integers and are not literals

**Intrinsic width rules:**

- Binary: count digits excluding underscores
- Decimal/hex: minimum bits needed to represent the value (leading zeros do not increase intrinsic width; value 0 has intrinsic width 1)

**Extension rules:**

- MSB `0` or `1`: zero-extend
- MSB `x`: extend with `x`
- MSB `z`: extend with `z` (binary only)

Overflow (intrinsic width > declared width) is a compile error.

**Examples:**
```text
8'b1010_0001    // binary, 8 bits
16'hFF00        // hex, 16 bits
12'd4095        // decimal, 12 bits
4'bx            // intrinsic 1, extends to 4'bxxxx
8'hF            // intrinsic 4, zero-extended to 8'h0F
```

**Extension table:**

| MSB | Extension | Example |
|-----|-----------|---------|
| `0` | zero-extend | `4'b0010` → `8'b0000_0010` |
| `1` | zero-extend | `4'b1010` → `8'b0000_1010` |
| `x` | x-extend | `4'bx` → `8'bxxxx_xxxx` |
| `z` | z-extend | `4'bz` → `8'bzzzz_zzzz` |

**Error conditions:**
- Unsized literals (`'hFF`) → error
- Decimal with `x`/`z` (`8'd10x`) → error
- Hex with `x`/`z` (`8'hFx`, `8'hz0`) → error
- Undefined CONST in width → error
- Overflow (intrinsic width > declared width) → error

## Signedness model

All signals, literals, and operators in JZ-HDL are **unsigned by default**. There is no implicit signed type. For signed arithmetic, use the explicit signed intrinsic operators `sadd` and `smul`.

## Bit-width constraints

- Binary operators generally require identical operand widths.
- Some operators have specialized width rules (see Expressions and Operators).
- When widths differ, use explicit extension modifiers (`=z`, `=s`, `<=z`, `<=s`, `=>z`, `=>s`).
- Truncation is never implicit; RHS wider than LHS is a compile error.

## x and z semantics

- `x`: intentional don't-care. Allowed only in binary literals.
  - Expressions containing `x` bits are x-dependent.
  - X-dependent values must be provably masked before reaching any observable sink.
- `z`: high-impedance (tri-state). Allowed only in binary literals.
  - Nets read must have at least one active (0/1) driver per bit.

**Allowed uses of `x`:**
- `x` in CASE/SELECT patterns (don't-care matching); `x` bits do not propagate.
- `x` in internal combinational values when structurally masked before observable sinks.

**Forbidden uses:**
- Register reset literals must not contain `x` or `z`.
- MEM initialization must not contain `x` or `z`.
- `x`/`z` digits in `h` or `d` literals are compile errors.

## Special semantic drivers

Two reserved semantic drivers provide width-agnostic constants:

- `GND`: Logic 0 — expands to match the target bit-width.
- `VCC`: Logic 1 — expands to match the target bit-width.

**Rules:**
- GND/VCC are valid only as standalone assignment tokens (not in expressions, concatenations, slices, or indices).
- GND/VCC are permitted as register reset values.
- GND/VCC participate fully in the Exclusive Assignment Rule.

**GND/VCC examples:**
```text
// Register reset:
REGISTER { counter [16] = GND; }    // resets to 16'h0000
REGISTER { flags   [8]  = VCC; }    // resets to 8'hFF

// Wire tie-off:
ASYNCHRONOUS { unused_out <= GND; }

// Port tie-off:
@new inst module {
  IN [8] data = GND;   // tie input low
}

// Illegal:
// result <= GND + 1;    // ERROR: GND not allowed in expressions
// {a, GND} <= expr;     // ERROR: GND not allowed in concatenation
// data[0] <= GND;       // ERROR: GND not allowed in slices
// IF (GND) { ... }      // ERROR: GND not allowed as condition
```

## Constants hierarchy

| Kind | Scope | Sized? | Usable at runtime? | Reference syntax |
| --- | --- | --- | --- | --- |
| `CONST` | Module-local | No (integer) | No | bare name |
| `CONFIG` | Project-wide | No (integer) | No | `CONFIG.NAME` |
| `@global` | All modules | Yes (sized literal) | Yes | `global_name.CONST_ID` |

- CONST/CONFIG are compile-time integers for widths, depths, and OVERRIDE.
- @global entries are sized literals usable in signal expressions.
- To use a constant value at runtime, use `@global` or `lit(width, value)`.
