# Test Plan: 5.2 SYNCHRONOUS Assignments (Sequential)

**Specification Reference:** Section 5.2 of jz-hdl-specification.md

## 1. Objective

Verify SYNCHRONOUS assignment forms (receive `<=`, with modifiers `<=z`/`<=s`, sliced, concatenation decomposition), single-path assignment rule, register hold behavior (zero-assignment path), alias prohibition in SYNC blocks, and width rules for register assignments.

## 2. Instrumentation Strategy

- **Span: `sem.sync_assign`** — Per-assignment in SYNC; attributes: `reg_name`, `operator`, `is_sliced`, `rhs_width`.
- **Event: `sync.path_conflict`** — Same register bits assigned twice in same path.
- **Event: `sync.alias_in_sync`** — Alias operator used in SYNC.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Simple register load | `counter <= counter + 8'd1;` |
| 2 | Conditional load | `IF (load) { data <= input; }` |
| 3 | Zero-extend load | `wide_reg <=z narrow_val;` |
| 4 | Sign-extend load | `signed_reg <=s narrow_val;` |
| 5 | Sliced assignment | `reg[3:2] <= 2'b10;` |
| 6 | Non-overlapping slices | `reg[7:4] <= a; reg[3:0] <= b;` |
| 7 | Concat decomposition | `{carry, sum} <= expr;` |
| 8 | Register hold | IF without ELSE — register holds in else path |
| 9 | SELECT-based assign | `SELECT (state) { CASE 0 { reg <= val; } }` |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Empty SYNC body | All registers hold — valid |
| 2 | All paths assign | Every IF/ELSE branch assigns register |
| 3 | Deeply nested conditionals | 10+ nesting levels with register assignments |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Alias in SYNC | `reg = expr;` — Error |
| 2 | Path conflict | Root assignment + conditional assignment to same reg — Error |
| 3 | Double assignment same path | `reg <= a; reg <= b;` — Error |
| 4 | Overlapping slices | `reg[7:4] <= a; reg[5:2] <= b;` — Error |
| 5 | Width mismatch no modifier | `reg[8] <= 4'd5;` — Error |
| 6 | Wire write in SYNC | `wire <= data;` — Error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `reg = expr;` in SYNC | Error | ASSIGN_OP_WRONG_BLOCK | Alias not allowed |
| 2 | Root + conditional same reg | Error | ASSIGN_SHADOW_OUTER | Path conflict |
| 3 | `reg <= a; reg <= b;` | Error | ASSIGN_MULTI_DRIVER | Double assignment |
| 4 | Overlapping slices | Error | ASSIGN_MULTI_DRIVER | Bit overlap |
| 5 | Valid conditional assign | Valid: hold in else | — | Standard pattern |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_assign.c` | Assignment validation in SYNC context | Integration test |
| `driver_flow.c` | Path analysis for single-path rule | Integration test |
| `parser_statements.c` | Parse SYNC statements | Token stream |
| `ir_build_stmt.c` | IR generation for register loads | IR verification |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| ASSIGN_OP_WRONG_BLOCK | Alias in SYNC block | Neg 1 |
| ASSIGN_SHADOW_OUTER | Root + conditional assignment conflict | Neg 2 |
| ASSIGN_MULTI_DRIVER | Double assignment same path | Neg 3, 4 |
| WIDTH_ASSIGN_MISMATCH_NO_EXT | Width mismatch without modifier | Neg 5 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| SYNC_WIRE_ASSIGN | S5.2 | Wire written in SYNC; covered by ASSIGN_OP_WRONG_BLOCK |
