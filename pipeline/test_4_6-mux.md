# Test Plan: 4.6 MUX (Signal Aggregation and Slicing)

**Specification Reference:** Section 4.6 of jz-hdl-specification.md

## 1. Objective

Verify MUX declaration forms (aggregation and auto-slicing), read-only semantics, dynamic indexing, width rules (equal source widths for aggregation, exact divisibility for auto-slicing), out-of-range index handling (compile-time error vs. runtime zero), and selector width requirements.

## 2. Instrumentation Strategy

- **Span: `parser.mux`** — Trace MUX parsing; attributes: `mux_name`, `form` (aggregate/slice), `element_count`.
- **Span: `sem.mux_validate`** — Semantic validation; attributes: `source_widths`, `element_width`, `selector_width`.
- **Event: `mux.width_mismatch`** — Sources have different widths.
- **Event: `mux.index_out_of_range`** — Static index exceeds element count.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Aggregation form | `mux = byte_a, byte_b;` — two 8-bit sources |
| 2 | Auto-slicing form | `mux [8] = flat_data;` on 32-bit signal → 4 elements |
| 3 | Dynamic selection | `out = mux[sel];` in ASYNCHRONOUS |
| 4 | Constant index | `out = mux[2'd1];` — valid static index |
| 5 | MUX in SYNCHRONOUS | `reg <= mux[sel];` — read in SYNC block |
| 6 | Many sources | Aggregation with 8+ sources |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Two elements | Minimum aggregation (2 sources) |
| 2 | 256 elements | Large aggregation |
| 3 | Selector width exact | clog2(N) bits for N elements |
| 4 | Selector width narrow | Narrower than clog2(N) — zero-extended |
| 5 | Single wide signal | 256-bit sliced into 32 8-bit elements |
| 6 | Runtime out-of-range | Index > N-1 at runtime → result is zero |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Assign to MUX | `mux[0] = data;` — Error: read-only |
| 2 | Mismatched source widths | `mux = byte_a, nibble;` (8-bit vs 4-bit) — Error |
| 3 | Non-divisible auto-slice | `mux [3] = 8-bit_signal;` (8 % 3 ≠ 0) — Error |
| 4 | Static out-of-range index | `mux[4]` on 4-element MUX (valid 0-3) — Error |
| 5 | Duplicate MUX name | MUX name conflicts with wire/port — Error |
| 6 | Element width zero | `mux [0] = signal;` — Error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `mux[0] = data;` | Error: assign to MUX | MUX_ASSIGN_READONLY | S4.6 |
| 2 | Mismatched widths in sources | Error | MUX_WIDTH_MISMATCH | S4.6 |
| 3 | 8 % 3 ≠ 0 | Error | MUX_SLICE_NOT_DIVISIBLE | S4.6 |
| 4 | `mux[4]` on 4 elements | Error | MUX_INDEX_OUT_OF_RANGE | S4.6 |
| 5 | Valid aggregation read | Valid result | — | Happy path |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_mux.c` | Parses MUX declarations | Token stream |
| `driver_expr.c` | MUX access in expressions | Expression AST |
| `driver_width.c` | Width validation | Unit test |
| `ir_build_expr.c` | MUX to multiplexer logic | IR verification |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| MUX_ASSIGN_READONLY | Assigning to read-only MUX | Neg 1 |
| MUX_WIDTH_MISMATCH | Aggregated sources differ in width | Neg 2 |
| MUX_SLICE_NOT_DIVISIBLE | Wide source not evenly divisible | Neg 3 |
| MUX_INDEX_OUT_OF_RANGE | Static index exceeds element count | Neg 4 |
| SCOPE_DUPLICATE_SIGNAL | MUX name conflicts with other identifier | Neg 5 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| MUX_ELEMENT_WIDTH_ZERO | S4.6 "positive integer" | Element width = 0 in auto-slicing |
| MUX_SELECTOR_TOO_NARROW | S4.6 "implicitly zero-extend" | May need info/warning for narrow selector |
