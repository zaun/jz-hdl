---
title: Statements
lang: en-US

layout: doc
outline: deep
---

# Statements

## Assignment operators

| Operator | Meaning | Aliases? | Width change allowed? |
| --- | --- | --- | --- |
| `=` | Alias | yes | only with `=z`/`=s` |
| `=>` | Drive | no | only with `=>z`/`=>s` |
| `<=` | Receive | no | only with `<=z`/`<=s` |

- Suffix `z` zero-extends the narrower side.
- Suffix `s` sign-extends the narrower side.
- Truncation is never implicit.

## ASYNCHRONOUS assignments

- Allows alias, drive, receive, slices, and concatenation LHS.
- Alias operators are forbidden inside conditional control flow.
- Alias operators may not have a bare literal RHS.
- `WIRE` and `PORT` assignments must satisfy Exclusive Assignment Rule.
- Reading a net that is undriven (all drivers are `z`) is a compile error.
- Combinational loops are detected and forbidden unless only in mutually exclusive branches.

## SYNCHRONOUS assignments

- Only directional operators (`<=` / `=>` and modifiers) are allowed. Alias (`=`) is forbidden (`reg = expr;` is a compile error).
- Registers may only be written in `SYNCHRONOUS` blocks.
- Each register bit may be assigned zero or one time per execution path.
- Unassigned registers hold their current value (clock gating semantics).
- Sliced register writes are allowed (non-overlapping per execution path).
- Concatenation decomposition on the LHS is permitted: `{hi, lo} <= expr;`.
- Width modifiers (`<=z`, `<=s`, `=>z`, `=>s`) are available for extension when widths differ. Redundant modifiers (equal widths) are optional but harmless.

## Conditionals (IF / ELIF / ELSE)

```text
IF (<expr>) {
  <stmt>;
} ELIF (<expr>) {
  <stmt>;
} ELSE {
  <stmt>;
}
```

- Condition must be width-1 (nonzero = true; zero = false).
- Parentheses required around condition.
- `ELIF` and `ELSE` are optional; zero-or-more `ELIF` blocks allowed.
- Branches are mutually exclusive for PED (Exclusive Assignment Rule) analysis.
- Nested blocks permitted at deeper nesting levels.

### Flow-sensitive loop detection

The combinational loop detector is flow-sensitive: mutually exclusive paths cannot create cycles even if they reference the same nets.

```text
// Valid — no loop:
ASYNCHRONOUS {
  IF (sel) { a = b; }
  ELSE     { b = a; }
}

// Invalid — unconditional loop:
ASYNCHRONOUS {
  a = b;
  b = a;  // ERROR
}
```

## SELECT / CASE

```text
SELECT (<expr>) {
  CASE <value1> { <stmt>; }
  CASE <value2>
  CASE <value3> { <stmt>; }
  DEFAULT { <stmt>; }
}
```

- CASE labels are sized integer literals or `@global` constants.
- `x` may appear in CASE patterns (don't-care matching); x bits do not propagate into results.
- Multiple CASE labels matching the same value are a compile error.
- DEFAULT is optional in SYNCHRONOUS (register holds state); recommended in ASYNCHRONOUS (prevents floating nets).

### Fall-through

- **Naked CASE labels** (without braces) fall through to the next label or block.
- **CASE labels with braces** execute statements and do not fall through.

```text
SELECT (state) {
  CASE 0
  CASE 1 { counter = counter + 1; }  // Executes for state == 0 or 1
  CASE 2 { counter = 8'h00; }
  DEFAULT { counter = 8'hFF; }
}
```

### Incomplete coverage

- If no CASE matches and no DEFAULT exists:
  - SYNCHRONOUS: register retains current value (hold state).
  - ASYNCHRONOUS: net has no driver from this statement (may float if no other driver).

## LATCH assignments

Latches are written only in ASYNCHRONOUS blocks using guarded syntax:

- D latch: `latch_name <= enable : data;`
- SR latch: `latch_name <= set : reset;`

The Exclusive Assignment Rule applies to latch assignments. Bit-slicing rules apply to latch identifiers exactly as they do to registers and wires.

**Power-up state:** A latch has no reset mechanism and no mandatory initialization value. Its power-up state is **indeterminate**. The designer is responsible for ensuring the latch is driven to a known state by logic before its output is consumed at an observable sink. Reading an uninitialized latch whose value may reach an observable sink is a design error.

## Template application (`@apply`)

Templates are applied inside `ASYNCHRONOUS` or `SYNCHRONOUS` bodies:

```text
@apply <template_id> (<arg_0>, <arg_1>, ...);
@apply [count] <template_id> (<arg_0>, <arg_1>, ...);
```

- Argument count must match the template's parameter count.
- When `[count]` is specified, the template is expanded `count` times with `IDX` substituted as a compile-time literal (0 to count-1).
- After expansion, the resulting statements are subject to all normal semantic rules (Exclusive Assignment, width matching, driver determinism).
- See the [Templates reference](/reference-manual/formal-reference/templates) for full details.

## Intrinsic operators

JZ-HDL provides 28 intrinsic operators for common hardware operations. See the [complete intrinsic operators table](/reference-manual/formal-reference/expressions-and-operators#intrinsic-operators) in the Expressions and Operators reference for the full list with result widths and semantics.

Common intrinsics include: `uadd`, `sadd`, `usub`, `ssub`, `umul`, `smul`, `clog2`, `widthof`, `lit`, `gbit`, `sbit`, `gslice`, `sslice`, `oh2b`, `b2oh`, `prienc`, `lzc`, `abs`, `umin`, `umax`, `smin`, `smax`, `popcount`, `reverse`, `bswap`, `reduce_and`, `reduce_or`, `reduce_xor`.

Key notes:

- `clog2` and `widthof` are compile-time only.
- `lit` is a runtime literal materializer; it is not valid where a compile-time integer is required.
- `gbit`/`gslice` out-of-range at runtime returns 0; `sbit`/`sslice` out-of-range returns source unchanged.
