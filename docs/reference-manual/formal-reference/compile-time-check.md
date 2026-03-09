---
title: Compile-time Assertions (@check)
lang: en-US

layout: doc
outline: deep
---

# Compile-time Assertions (@check)

## Purpose

Validate compile-time constraints (widths, config sanity, address ranges, parameter compatibility) during elaboration. A `@check` failure aborts compilation. `@check` never generates hardware, simulation logic, or runtime behavior.

## Syntax

```text
@check (<constant_expression>, <string_message>);
```

Parentheses are required.

## Semantics

- If the expression is **true** (nonzero integer): compilation continues.
- If the expression is **false** (zero): compile error with message "CHECK FAILED: message".
- If the expression cannot be evaluated as a constant: compile error (non-constant expression).

## Placement rules

`@check` may appear in:
- Inside `@project`
- Inside `@module`

`@check` may **not** appear inside:
- Blocks (ASYNCHRONOUS, SYNCHRONOUS, PORT, WIRE, REGISTER, MEM, etc.)
- Other directives

## Expression rules

The expression must evaluate to a constant, nonnegative integer at compile time.

**Allowed operands:**
- Integer literals
- `CONST` identifiers
- `CONFIG.NAME` identifiers
- Compile-time integer operators
- `clog2()`
- Parentheses
- Comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`)
- Logical operators (`&&`, `||`, `!`)

**Forbidden operands:**
- Module ports, wires, registers
- Memory ports
- Signal slices
- Any runtime expression

## Evaluation order

`@check` is evaluated after:
1. Resolution of all preceding `CONST` definitions
2. Resolution of all preceding `CONFIG` entries
3. OVERRIDE evaluation (if applicable)

`@check` can reference `CONFIG` from the same project and `CONST` from the same module, including values after any OVERRIDE has been applied.

## Examples

```text
// Width constraint
CONST { WIDTH = 32; }
@check (WIDTH % 8 == 0, "Width must be a multiple of 8.");

// Project config constraint
@check (CONFIG.DATA_WIDTH >= 8, "Data width must be at least 8.");

// Address width sanity
CONST { DEPTH = 256; ADDR_W = 8; }
@check (ADDR_W == clog2(DEPTH), "Address width does not match depth.");

// Invalid: runtime signal
@check (select == 3, "...");
// ERROR: non-constant expression in @check
```

## Error conditions

- Expression evaluates to zero
- Expression contains any runtime signal
- Expression contains undefined identifiers
- Expression produces a non-integer
- Expression uses operators disallowed in constant expressions
