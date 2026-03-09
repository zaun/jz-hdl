---
title: Expressions and Operators
lang: en-US

layout: doc
outline: deep
---

# Expressions and Operators

## Operator categories

| Category | Operators | Result width |
| --- | --- | --- |
| Unary arithmetic | `-`, `+` | Input width (must parenthesize) |
| Binary arithmetic (add/sub) | `+`, `-` | input width |
| Binary arithmetic (mul/div/mod) | `*`, `/`, `%` | see definitions |
| Bitwise | `&`, `\|`, `^`, `~` | input width |
| Logical | `&&`, `\|\|`, `!` | 1 |
| Comparison | `==`, `!=`, `<`, `>`, `<=`, `>=` | 1 |
| Shift | `<<`, `>>`, `>>>` | LHS width |
| Ternary | `? :` | operand width |
| Concatenation | `{ , }` | sum of input widths |

## Key operator rules

- Unary `-` / `+` must be parenthesized (e.g., `(-a)`). Result width equals input width.
- `+` / `-` require equal widths; result width equals operand width. Overflow bits are truncated.
- `*` requires equal widths; result width is `2 * N` (full product).
- `/` and `%` require equal widths; result width equals dividend width. Compile-time zero divisor is an error; runtime zero is indeterminate (implementation-defined). See [Division by zero](#division-by-zero).
- Bitwise and comparison operators require equal widths.
- Logical operators (`&&`, `||`, `!`) require width-1 operands.
- Shifts keep LHS width; shift amount can be any width.
- `>>>` is arithmetic right shift (sign-extends from MSB).
- Ternary requires width-1 condition and equal-width branches.
- Concatenation order is MSB-first.
- Any operator producing `x` bits is subject to the observability rule. Masking is structural, not algebraic. The unused branch of a ternary (`? :`) is considered structurally masked only if the condition is provably constant at compile time.

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

## Assignment operators

| Operator | Meaning | Aliases? | Width change allowed? |
| --- | --- | --- | --- |
| `=` | Alias | yes | only with `=z`/`=s` |
| `=>` | Drive | no | only with `=>z`/`=>s` |
| `<=` | Receive | no | only with `<=z`/`<=s` |

- Suffix `z` zero-extends the narrower side.
- Suffix `s` sign-extends the narrower side.
- Truncation is never implicit.

## Bit slicing and indexing

**Syntax:** `signal[MSB:LSB]`

- MSB and LSB are nonnegative integers or `CONST` names.
- `MSB >= LSB` is required.
- Indices are inclusive; width = MSB - LSB + 1.
- Indices must be within declared width.

## Concatenation

- `{a, b, c}` produces width = sum(width(a), width(b), width(c)).
- MSB-first ordering: `a` occupies the most-significant bits.
- Can be used on LHS for decomposition: `{hi, lo} = expr;`

## Intrinsic operators

| Intrinsic | Result width | Description |
| --- | --- | --- |
| `uadd(a, b)` | max(a, b) + 1 | Unsigned add with carry |
| `sadd(a, b)` | max(a, b) + 1 | Signed add with carry (sign-extends) |
| `umul(a, b)` | 2 * max(a, b) | Unsigned full product |
| `smul(a, b)` | 2 * max(a, b) | Signed full product (sign-extends) |
| `clog2(value)` | compile-time int | Ceiling log2; compile-time contexts only |
| `widthof(signal)` | compile-time int | Declared width of local wire/register |
| `gbit(source, index)` | 1 | Dynamic single-bit extract |
| `sbit(source, index, set)` | width(source) | Dynamic single-bit set/clear |
| `gslice(source, index, width)` | width (constant) | Dynamic multi-bit extraction |
| `sslice(source, index, width, value)` | width(source) | Dynamic multi-bit overwrite |
| `lit(width, value)` | width | Compile-time integer to sized literal |
| `oh2b(source)` | clog2(width(source)) | One-hot to binary encoder |
| `b2oh(index, width)` | width (constant) | Binary to one-hot decoder |
| `prienc(source)` | clog2(width(source)) | Priority encoder (MSB-first) |
| `lzc(source)` | clog2(width(source)+1) | Leading zero count |
| `usub(a, b)` | max(a, b) + 1 | Unsigned subtract with borrow |
| `ssub(a, b)` | max(a, b) + 1 | Signed subtract with overflow |
| `abs(a)` | width(a) + 1 | Signed absolute value (MSB=overflow) |
| `umin(a, b)` | max(a, b) | Unsigned minimum |
| `umax(a, b)` | max(a, b) | Unsigned maximum |
| `smin(a, b)` | max(a, b) | Signed minimum |
| `smax(a, b)` | max(a, b) | Signed maximum |
| `popcount(source)` | clog2(width(source)+1) | Population count (number of set bits) |
| `reverse(source)` | width(source) | Bit reversal |
| `bswap(source)` | width(source) | Byte swap (width must be multiple of 8) |
| `reduce_and(source)` | 1 | AND reduction |
| `reduce_or(source)` | 1 | OR reduction |
| `reduce_xor(source)` | 1 | XOR reduction |

**Notes:**
- `clog2` and `widthof` are compile-time only. `widthof` accepts wire, register, or bus identifiers.
- `lit` produces a runtime literal; not valid where a compile-time integer is required.
- `gbit`/`gslice` out-of-range at runtime returns 0.
- `sbit`/`sslice` out-of-range at runtime returns source unchanged / ignores out-of-range bits.
- `oh2b` source must be >= 2 bits wide. If no bit is set, result is 0. If multiple bits are set, behavior is hardware-defined.
- `b2oh` width must be a compile-time constant >= 2. Index out of range returns all zeros.
- `prienc` source must be >= 2 bits wide. No bits set returns 0.
- `lzc` all zeros returns width(source).
- `abs` MSB of result is overflow flag (1 only for most-negative input). Lower bits are always the correct magnitude.
- `bswap` source width must be a multiple of 8 (compile-time error otherwise).
- `reduce_and`/`reduce_or`/`reduce_xor` map directly to Verilog reduction operators `&()`, `|()`, `^()`.

## Division by zero

- If the divisor is a compile-time constant `0`, the expression is a compile-time error.
- If the divisor may be `0` at runtime, the result is indeterminate (implementation-defined): no trap, interrupt, or exception is generated; the hardware divider may produce an arbitrary bit pattern, stall indefinitely, or exhibit undefined timing behavior. Behavior differs between synthesis tools and FPGA vendors.
- Simulation tools must abort on runtime division by zero.
- Division by zero is a design error. The designer is responsible for ensuring the divisor is non-zero via architectural design (guards, masks, or proven invariants). Tools may warn when the divisor is non-constant, but this is advisory only.
- **Best practice:** Guard divisions with explicit conditions:

```text
IF (divisor != 0) {
  result = numerator / divisor;
} ELSE {
  result = safe_fallback;  // Designer-specified behavior
}
```
