---
title: Templates
lang: en-US

layout: doc
outline: deep
---

# Templates (`@template` / `@apply`)

## Purpose

Templates provide **compile-time reusable logic blocks** that expand inline into statements without introducing new hardware structure. Templates do **not** create modules, ports, wires, registers, memories, or storage of any kind. They exist solely to reduce code duplication while preserving the structural determinism of JZ-HDL.

Templates expand before semantic analysis. The expanded code must independently satisfy all JZ-HDL rules (Exclusive Assignment, driver determinism, width rules).

Templates are strictly limited to prevent them from becoming "mini-modules."

## Template Definition

**Syntax:**
```text
@template <template_id> (<param_0>, <param_1>, ..., <param_n>)
  <template_body>
@endtemplate
```

**Placement:**
- **Module-scoped:** Inside a `@module` block, alongside other declaration blocks (CONST, PORT, WIRE, etc.), but outside any `ASYNCHRONOUS` or `SYNCHRONOUS` body. Module-scoped templates are visible only within that module.
- **File-scoped:** At file scope, outside any `@module` or `@project`, similar to `@global`. File-scoped templates are visible to all modules in the compilation unit.

**Rules:**
- `<template_id>` follows identifier rules.
- Parameter list may contain zero or more identifiers.
- Parameters are placeholders for identifiers or expressions at the callsite.
- Template bodies do **not** create a new namespace; after expansion the statements belong to the enclosing scope.

## Allowed Content

A template may contain only:

### Statements

- Directional assignments: `<=`, `=>`, `<=z`, `<=s`, `=>z`, `=>s`
- Alias assignments: `=`, `=z`, `=s` (subject to the same alias rules as outside templates — no aliases in SYNCHRONOUS blocks, no aliases in conditionals, no literal RHS)
- All identifiers on either side of an assignment must be template parameters or scratch wires. A template may not reference identifiers outside its parameter list; all external signals must be passed in as arguments.
- **Exception:** Compile-time constants (`CONST`, `CONFIG`) and `@global` values (e.g., `CMD.READ`) may appear in expressions without being passed as parameters.
- Conditional logic: `IF` / `ELIF` / `ELSE`, `SELECT` / `CASE`

### Expressions

- Any valid JZ-HDL expression.
- Template parameters may appear in expressions, slice indices, and concatenations.

### Scratch Wires

Defined via a restricted syntax:

```text
@scratch <scratch_id> [<width>];
```

**Scratch rules:**
- Scratch wires exist only inside a single expansion site.
- They cannot be referenced outside the template.
- They are implicitly allocated as internal, anonymous nets.
- They may participate in `<=`, `=>`, and `=` assignments.
- They do not appear in module namespace.
- They may not shadow any existing identifier.
- Width expressions must be a compile-time constant expression after parameter and IDX substitution. Compile-time intrinsics such as `widthof()` and `clog2()` may be used (e.g., `@scratch sum [widthof(a)+1]`).

**Usage example:**
```text
@scratch tmp [8];
tmp <= a + b;
out <= tmp;
```

## Forbidden Content

Templates may **not** contain:

### Declarations

- `WIRE`, `REGISTER`, `PORT`, `CONST`, `MEM`, `MUX`
- `@new`, `@module`, `@project`
- `CDC`
- `SYNCHRONOUS` or `ASYNCHRONOUS` block headers

### Other Restrictions

- No nested `@template` definitions
- No clock or reset logic
- No `@feature` blocks
- Template parameters cannot represent widths; they represent *identifiers* or *expressions* only
- Scratch wires cannot be sliced outside the template

## Template Application (`@apply`)

**Syntax:**
```text
@apply <template_id> (<arg_0>, <arg_1>, ..., <arg_n>);
@apply [count] <template_id> (<arg_0>, <arg_1>, ..., <arg_n>);
```

**Rules:**
- Present only inside `ASYNCHRONOUS` or `SYNCHRONOUS` bodies.
- Argument count must match the template's parameter count.
- `<count>` is the number of times to apply the template:
  - Must be a compile-time nonnegative integer constant; if omitted it defaults to `1`.
  - If `count == 1`, the template is expanded once and `IDX` is implicitly bound to `0`.
  - If `count > 1`, the template is expanded `count` times; in expansion k (0 <= k < count) `IDX` is substituted with the integer literal `k`.
  - If `count == 0`, the apply is a no-op.

### IDX Rules

- `IDX` is a compile-time integer literal available inside the template after substitution.
- `IDX` is **not** a runtime signal and may be used **only** in compile-time contexts:
  - Slice bounds
  - Instance naming
  - `OVERRIDE` expressions
  - CONST initializers
  - Array/instance indices
- Using `IDX` in a runtime expression (e.g., as a value assigned to a register or as a combinational operand) is a compile-time error.

### Arguments

Each `<arg_i>` must be:
- An identifier
- A slice (slice indices may include `IDX` since it is compile-time substituted)
- A valid JZ-HDL expression (subject to `IDX` rules)
- A port/reference (`inst.port`)

### Post-expansion Validation

After expansion the generated code is validated by the normal semantic checks (Exclusive Assignment, widths, name uniqueness). If expansion produces duplicate identifiers, the template author must include `IDX` in names to ensure uniqueness.

### Expansion Semantics

- The template is expanded **inline** at the callsite.
- Parameters are replaced with their provided arguments via capture-avoiding substitution.
- Scratch wires become compiler-generated nets unique per callsite.
- After expansion, the resulting statements undergo full semantic analysis.

## Exclusive Assignment Compatibility

Template expansion does **not** bypass or weaken the Exclusive Assignment Rule.

After expansion:
- Every assignment in the template behaves exactly as if written by the author at the callsite.
- Multiple `@apply` calls that assign the same identifier must still be exclusive by structure or path.
- Violations inside expanded templates produce normal assignment-conflict errors.

## Examples

### Simple arithmetic pattern (saturating add)

```text
@template SAT_ADD(a, b, out)
  @scratch sum [widthof(a)+1];
  sum <=z uadd(a, b);
  out <= sum[widthof(a)] ? {widthof(a){1'b1}} : sum[widthof(a)-1:0];
@endtemplate
```

Usage:
```text
ASYNCHRONOUS {
  @apply SAT_ADD(x, y, result);
}
```

### Multi-line logic (clamping)

```text
@template CLAMP(val, lo, hi, out)
  IF (val < lo) {
    out <= lo;
  } ELIF (val > hi) {
    out <= hi;
  } ELSE {
    out <= val;
  }
@endtemplate
```

### Scratch wire with operations

```text
@template XOR_THEN_SHIFT(a, b, out)
  @scratch t [widthof(a)];
  t <= a ^ b;
  out <= t << 1;
@endtemplate
```

### Template in a SYNCHRONOUS block

```text
SYNCHRONOUS(CLK=clk) {
  @apply CLAMP(counter, 16'h0004, 16'h0FFF, counter);
}
```

Still subject to Exclusive Assignment analysis after expansion.

### Unrolling with IDX

```text
@template grant(pbus_in, grant_reg)
  IF (pbus_in[IDX].REQ == 1'b1) {
    grant_reg[IDX+1:0] <= 1'b1;
  } ELSE {
    grant_reg[IDX+1:0] <= 1'b0;
  }
@endtemplate
```

Usage:
```text
SYNCHRONOUS(CLK=clk, RESET=reset, RESET_ACTIVE=High) {
  // Expand this template CONFIG.SOURCES times,
  // substituting IDX = 0..CONFIG.SOURCES-1
  @apply[CONFIG.SOURCES] grant(pbus, reg);
}
```

## Error Cases

### Illegal declaration inside template
```text
@template BAD(a, b)
  WIRE { t[8]; }  // ERROR: declarations not allowed in templates
@endtemplate
```

### Scratch wire used outside template
```text
@scratch t [8];
x <= t;  // ERROR: scratch t not visible here
```

### Illegal nested template
```text
@template A()
  @template B()  // ERROR: nested templates not allowed
  @endtemplate
@endtemplate
```

## Rationale

### Safety
- No new structural declarations except scratch wires
- No storage
- No clocks
- No namespace pollution
- No way to create implicit modules

### Power
- Multi-line code reuse
- Scratch wires enable non-trivial logic
- Combinational logic patterns become reusable
- Inline expansion ensures determinism and traceability
- Works in both ASYNCHRONOUS and SYNCHRONOUS blocks

### Clarity
- Templates are clearly not modules
- No state
- No driver surprises
- Always visible in expanded form
