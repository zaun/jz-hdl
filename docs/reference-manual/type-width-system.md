---
title: Types and Widths
lang: en-US

layout: doc
outline: deep
---

# Types and Widths

## Overview

JZ-HDL enforces precise bit widths everywhere. Sizes are explicit (no unsized literals) and most binary operators require equal operand widths. When widths differ, explicit extension modifiers (zero/sign) or intrinsic operators must be used. Unknown (`x`) and tri‑state (`z`) bits propagate with strict rules: they may never reach observable sinks unless provably masked.

---

## Literals

### Syntax
A sized literal has form:
```
<width>'<base><value>
```
- **width**: positive integer, `CONST` name, or `CONFIG.<name>` (compile‑time constant)
- **base**: one of `b`, `d`, `h`
- **value**: digits (underscores `_` allowed for readability, not leading/trailing)

Examples:
```text
8'b1010_0001
16'hFF00
12'd4095
```

### Bases and allowed digits
- `b` (binary): digits `0`, `1`, `x`, `z`, and `_` (underscores ignored)
- `d` (decimal): digits `0–9`, `_` only (no `x`/`z`)
- `h` (hex): digits `0–9`, `A–F`/`a–f`, `_` only (no `x`/`z`)

Unsized literals (e.g., `'hFF`) are illegal and cause compile errors. Bare decimal integers (e.g., `1`, `42`) are also unsized and are not permitted in runtime expressions — use a sized literal (`1'b1`, `8'd42`) or `lit(width, value)`. Bare integers remain valid in compile-time contexts such as `CONST`/`CONFIG` values, width brackets, slice indices, and `CASE` labels.

### Intrinsic bit‑width and extension
- Intrinsic width:
  - Binary: number of digits (excluding `_`) in the value field
  - Decimal/Hex: minimal bits needed to represent the integer (value 0 → width 1)
- If intrinsic width is less than declared width, the literal is extended based on the MSB digit of the intrinsic value:
  - MSB = `0` or `1`: zero‑extend (pad with `0`)
  - MSB = `x`: extend with `x` bits
  - MSB = `z`: extend with `z` bits (binary only)
- If intrinsic width exceeds declared width → compile error (overflow).

Examples:
```text
4'bx         // intrinsic 1 → extends to 4'bxxxx
8'hF        // intrinsic 4 → zero-extended to 8'h0F
25'h1FFFFFF // intrinsic 25 → fits; OK
25'h3FFFFFF // intrinsic 26 -> ERROR: overflow
```

### `x` and `z` semantics in literals
- `x`: intentional don't‑care; allowed only in binary literals.
  - Any expression containing `x` bits is considered x‑dependent.
  - X‑dependent values may only be used if every `x` bit is provably masked before reaching any observable sink.
- `z`: high‑impedance (tri‑state). Allowed only in binary literals.
  - Tri‑state bits are permitted for INOUT/tri‑state uses, but nets read must have at least one active (0/1) driver for each bit.

Forbidden uses:
- Register reset literals and MEM initializers must not contain `x` or `z`.
- `x`/`z` digits used in `h` or `d` literals → compile error.

---

## Bit‑Width Constraints

### Strict matching rule
Most binary operators and many constructs require identical operand widths. Violations produce compile errors.

Operators requiring equal widths include:
- Arithmetic: `+`, `-` (binary)
- Bitwise: `&`, `|`, `^`
- Comparisons: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Equality/relational operators
- Many intrinsics where specified (exceptions below)

### Extension modifiers
When you intentionally connect signals of different widths, use explicit modifiers so the compiler and synthesizer know how to extend bits:

Operator suffixes (applied to `=`, `=>`, `<=`):
- `z` — zero‑extend the narrower side
- `s` — sign‑extend the narrower side

Valid forms:
```
a =z b;     // alias: b zero-extended to a's width, then aliased
a <=s b;    // receive with sign-extend
driver =>z sink;
```

Rules:
- If widths are equal, bare operator is fine; `z`/`s` are allowed but redundant.
- If widths differ and no modifier is present → compile error.
- Truncation is never implicit. If RHS wider than LHS → compile error.
- Alias operators (`=` family) may not have a bare literal RHS (use directional assign to drive constants).

### Concatenation & decomposition
- Concatenation `{a, b, c}` produces width = sum(width(a), width(b), ...)
- When assigning to a concatenation, the RHS expression width must equal the concatenation width, unless `=z`/`=s` modifier is used to extend the RHS.
- In synchronous assignments the decomposition `{r_hi, r_lo} <= expr;` distributes MSB→first element.

---

## Operator Width Rules

Summary (result width relative to operand widths):

- Unary arithmetic (`-`, `+`): only on width‑1 operands; result width = 1
- Binary add/sub (`+`, `-`): operands must match; result width = operand width (overflow truncated)
- Multiply (`*`): operands must match; result width = 2 * width (full product)
- Divide/Modulus (`/`, `%`): operands must match; result width = dividend width; divisor = 0 at compile time → error
- Bitwise (`&`, `|`, `^`, `~`): operands must match; result width = operand width
- Logical (`&&`, `||`, `!`): operands width‑1; result width = 1
- Comparison (`==`, `!=`, `<`, `>`, `<=`, `>=`): operands must match; result width = 1
- Shift (`<<`, `>>`, `>>>`): LHS width retained
- Ternary `cond ? a : b`: cond width‑1; a and b widths must match; result = operand width
- Concatenation `{}`: result width = sum of inputs

Important notes:
- Add/Sub truncate silently — to capture carry extend operands or use uadd/sadd.
- Many intrinsics handle width extension automatically (see Intrinsics section).

---

## Special Operators & Intrinsics (width behavior)

- uadd(a,b): unsigned add; result width = max(width(a),width(b)) + 1 (zero‑extend operands)
- sadd(a,b): signed add; result width = max(width(a),width(b)) + 1 (sign‑extend operands)
- umul(a,b): unsigned multiply; result width = 2 * max(width(a),width(b)) (zero‑extend)
- smul(a,b): signed multiply; result width = 2 * max(width(a),width(b)) (sign‑extend)
- gbit(source,index): returns 1 bit; index must be wide enough (>= clog2(width(source))); out‑of‑range at runtime → returns 0
- sbit(source,index,set): returns full source width with one bit updated
- gslice/sslice: dynamic slice/overwrite with compile‑time constant slice width; index bounds behavior defined (out‑of‑range bits → 0 / ignored)

Use intrinsics when you need automatic, safe extension or dynamic indexing semantics.

### lit(width, value) — compile-time integer literal

- Usage: `lit(width, value)`
- Materializes a compile-time integer into a sized, runtime-legal bit-vector
- Both width and value must be compile-time nonnegative integer expressions (may reference `CONST`, `CONFIG`, `clog2()`, `widthof()`)
- The value is zero-extended to the given width
- `lit()` is valid in ASYNCHRONOUS RHS, SYNCHRONOUS RHS, REGISTER reset values, and `@global` blocks
- `lit()` is **not** valid in width brackets, CONFIG blocks, CONST blocks, or OVERRIDE expressions

```text
// Compare a register to a computed constant
IF (count == lit(ADDR, WIDTH - 1)) { ... }

// Use as a register reset value
REGISTER {
  limit [ADDR] = lit(ADDR, WIDTH - 1);
}

// Errors:
lit(0, 5)            // ERROR: width must be >= 1
lit(4, -1)           // ERROR: value < 0
lit(4, 16)           // ERROR: overflow (16 >= 2^4)
CONST { X = lit(4,3); } // ERROR: not a compile-time integer
```

---

## Operator precedence (highest to lowest)

1. Parentheses `( )`
2. Concatenation `{ }`
3. Unary NOT `~` `!`
4. Unary arithmetic `-` `+`
5. `*` `/` `%`
6. `<<` `>>` `>>>`
7. Binary `+` `-`
8. Relational `<` `>` `<=` `>=`
9. Equality `==` `!=`
10. Bitwise `&`
11. Bitwise `^`
12. Bitwise `|`
13. Logical `&&`
14. Logical `||`
15. Ternary `? :`

---

## Compile‑time helpers

### clog2(value)
- Usage: `clog2(expr)`
- Evaluated at compile time; returns smallest bit count >= value (ceil(log2(value)))
- Argument must be a positive integer constant (CONST, CONFIG, or literal)
- Typical use: address widths for memories, selector widths for MUX

### widthof(identifier)
- Usage: `widthof(identifier)`
- Returns the declared static bit width of a BUS or local WIRE or REGISTER
- Compile‑time only; name must be module‑local and resolvable
- Not usable at run time inside ASYNCHRONOUS/SYNCHRONOUS expressions

### CONST vs CONFIG vs @global
- CONST: module‑local integer constants, usable in widths and compile‑time contexts
- CONFIG: project‑wide integer constants accessed as `CONFIG.NAME`, compile‑time only
- @global: named sized literals (bit patterns) visible across modules and usable at run time in expressions

Remember: CONST/CONFIG are compile‑time integers; @global entries are sized value literals usable in signal expressions.

### Special Semantic Drivers

To ensure electrical clarity and prevent manual bit-width matching errors for common constants, JZ-HDL defines two reserved semantic drivers: `GND` (Logic 0), `VCC` (Logic 1).

* **Polymorphic Expansion**: Special drivers automatically expand to match the bit-width of the target identifier, satisfying the "No Implicit Width Conversion" rule by being width-agnostic by definition. The width of a special driver is determined solely by the width of the driven target, never by surrounding syntax.
* **Atomic Assignment**: Special drivers are valid only as standalone assignment tokens.
* **Expression Proscription**: To prevent ambiguous width inference in math, special drivers cannot be used as operands in arithmetic or logical expressions (e.g., `GND + 1` is illegal).
  - may not appear in expressions
  - may not appear in concatenations
  - may not be sliced or indexed
  - may not appear in slice or index
* **Initialization**: Special drivers are permitted as reset values in register declarations.

**Target Constraints**:
* `GND` and `VCC` may drive any valid sink (wire, port, register, latch, etc.).

**Driver Interaction:**
- `GND` and `VCC` drivers participate fully in the Exclusive Assignment Rule.
- Driving a signal with both a `GND` or `VCC` semantic driver and any other driver in the same execution path is illegal.


---

## Common errors and how to fix them

- Unsized literal (e.g., `'hFF`): add width, e.g., `8'hFF`.
- Literal overflow: increase declared width or fix value to fit.
- Operator width mismatch: make operand widths equal via explicit extension (`=z`, `=s`, concatenation, intrinsics) or adjust declarations.
- Implicit truncation attempt: ensure LHS is wide enough or slice intentionally.
- Using `x` or `z` in register resets / MEM init: replace with concrete 0/1 literal.
- Unary `-` on multi‑bit: legal only for width‑1; for multi‑bit two's complement negate use subtraction: `zero - val` or use `sadd` patterns.
- Division by zero (constant 0): fix divisor; if dynamic divisor may be zero, guard it with IF/ELSE.

---

## Practical examples

Examples with expected behavior:

```text
// Valid: zero-extend 8->16 explicitly
wide_reg =z narrow_val;       // aliases (ASYNCHRONOUS) or use <=z in SYNCHRONOUS

// Capturing carry safely
{carry, sum} = uadd(a, b);    // result width = max(w(a),w(b)) + 1

// Full product
prod = umul(a, b);            // if a,b=8 -> prod is 16 bits

// Illegal: unsized literal
data = 'hFF;                  // ERROR: unsized literal

// Illegal: overflow
4'b10101;                     // ERROR: intrinsic width 5 > declared 4

// Binary literal with x: allowed but must be masked before observable sink
temp = 4'b10x1;               // temp contains x; ensure downstream masking
```

Memory/address helpers:
```text
CONST { DEPTH = 256; }
ADDR_W = clog2(DEPTH);        // ADDR_W = 8
```

widthof use:
```text
WIRE { bus [32]; }
CONST { W = widthof(bus); }   // valid compile-time query inside same module
```

---

## Quick checklist

- Always size literals: use the `width'base_value` form.
- Match operand widths for strict operators or use explicit extension (z/s).
- Do not rely on implicit truncation — the compiler forbids it.
- Avoid `x`/`z` reaching registers, MEM init, OUT/INOUT ports, top pins.
- Use intrinsics (uadd, umul, ...) for safe automatic sizing when appropriate.
- Use `clog2()` and `widthof()` for robust compile‑time sizing.
- Ensure unary arithmetic is parenthesized (e.g., `(-a)`).
