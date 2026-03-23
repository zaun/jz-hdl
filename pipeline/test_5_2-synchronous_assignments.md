# Test Plan: 5.2 Synchronous Assignments

**Specification Reference:** Section 5.2 of jz-hdl-specification.md

## 1. Objective

Verify SYNC assignment forms (receive `<=`, with modifiers `<=z`/`<=s`, sliced, concatenation decomposition), single-path assignment rule, register hold behavior (zero-assignment path), alias prohibition in SYNC blocks, and width rules for register assignments.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Simple register load | `counter <= counter + 8'd1;` | Valid, register updated |
| 2 | Conditional load | `IF (load) { data <= input; }` | Valid, register holds in else path |
| 3 | Zero-extend load | `wide_reg <=z narrow_val;` | Valid, zero-extended |
| 4 | Sign-extend load | `signed_reg <=s narrow_val;` | Valid, sign-extended |
| 5 | Sliced assignment | `reg[3:2] <= 2'b10;` | Valid, partial register write |
| 6 | Non-overlapping slices | `reg[7:4] <= a; reg[3:0] <= b;` | Valid, disjoint bits |
| 7 | Concat decomposition | `{carry, sum} <= expr;` | Valid, decomposes into registers |
| 8 | Register hold via no ELSE | `IF (en) { reg <= val; }` | Valid, register holds when not enabled |
| 9 | SELECT-based assign | `SELECT (state) { CASE 0 { reg <= val; } }` | Valid, register holds in unmatched cases |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Alias in SYNC | `reg = expr;` in SYNC | Error | SYNC_NO_ALIAS |
| 2 | Root plus conditional assign | Root-level `reg <= a;` and conditional `IF (c) { reg <= b; }` | Error | SYNC_ROOT_AND_CONDITIONAL_ASSIGN |
| 3 | Double assign same path | `reg <= a; reg <= b;` in SYNC | Error | SYNC_MULTI_ASSIGN_SAME_REG_BITS |
| 4 | Duplicate register in concat | `{reg, reg} <= expr;` | Error | SYNC_CONCAT_DUP_REG |
| 5 | Non-register target in SYNC | `wire <= data;` in SYNC | Error | ASSIGN_TO_NON_REGISTER_IN_SYNC |
| 6 | Wire write in SYNC | Wire assigned with `<=` in SYNC | Error | WRITE_WIRE_IN_SYNC |
| 7 | Slice width mismatch | `reg[7:4] <= 3'b101;` (4 vs 3) | Error | SYNC_SLICE_WIDTH_MISMATCH |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Empty SYNC body | `SYNCHRONOUS(clk) { }` | Valid, all registers hold |
| 2 | All paths assign | Every IF/ELSE branch assigns register | Valid |
| 3 | Deeply nested conditionals | 10+ nesting levels with register assignments | Valid if single-path rule respected |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `reg = expr;` in SYNC | Compile error | SYNC_NO_ALIAS | error | Alias forbidden in SYNC |
| 2 | Root + conditional same reg | Compile error | SYNC_ROOT_AND_CONDITIONAL_ASSIGN | error | Conflicting assignment paths |
| 3 | `reg <= a; reg <= b;` | Compile error | SYNC_MULTI_ASSIGN_SAME_REG_BITS | error | Same bits assigned twice |
| 4 | `{reg, reg} <= expr;` | Compile error | SYNC_CONCAT_DUP_REG | error | Duplicate register in concat LHS |
| 5 | Wire target in SYNC | Compile error | ASSIGN_TO_NON_REGISTER_IN_SYNC | error | Only registers allowed |
| 6 | Wire `<=` in SYNC | Compile error | WRITE_WIRE_IN_SYNC | error | Wire not writable in SYNC |
| 7 | Slice width mismatch in SYNC | Compile error | SYNC_SLICE_WIDTH_MISMATCH | error | Slice and expression widths must match |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 5_2_ASSIGN_TO_NON_REGISTER_IN_SYNC-non_register_in_sync.jz | ASSIGN_TO_NON_REGISTER_IN_SYNC | Non-register target assigned in SYNCHRONOUS block |
| 5_2_SYNC_CONCAT_DUP_REG-duplicate_reg_in_concat.jz | SYNC_CONCAT_DUP_REG | Same register appears more than once in concat LHS |
| 5_2_SYNC_MULTI_ASSIGN_SAME_REG_BITS-double_assign_sync.jz | SYNC_MULTI_ASSIGN_SAME_REG_BITS | Same register bits assigned twice in same execution path |
| 5_2_SYNC_NO_ALIAS-alias_in_sync_block.jz | SYNC_NO_ALIAS | Alias operator `=` used in SYNCHRONOUS block |
| 5_2_SYNC_ROOT_AND_CONDITIONAL_ASSIGN-root_plus_conditional.jz | SYNC_ROOT_AND_CONDITIONAL_ASSIGN | Root-level assignment combined with conditional assignment to same register |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| SYNC_MULTI_ASSIGN_SAME_REG_BITS | error | S5.2/S8.1 Same register bits assigned more than once along any execution path | 5_2_SYNC_MULTI_ASSIGN_SAME_REG_BITS-double_assign_sync.jz |
| SYNC_ROOT_AND_CONDITIONAL_ASSIGN | error | S5.2/S1.5/S8.1 Root-level register assignment combined with nested conditional assignment to same bits | 5_2_SYNC_ROOT_AND_CONDITIONAL_ASSIGN-root_plus_conditional.jz |
| SYNC_CONCAT_DUP_REG | error | S5.2 Concatenation LHS includes same register more than once | 5_2_SYNC_CONCAT_DUP_REG-duplicate_reg_in_concat.jz |
| SYNC_NO_ALIAS | error | S5.2 Aliasing `=` is forbidden in SYNCHRONOUS blocks | 5_2_SYNC_NO_ALIAS-alias_in_sync_block.jz |
| ASSIGN_TO_NON_REGISTER_IN_SYNC | error | S5.2 Only REGISTERs may be assigned in SYNCHRONOUS blocks | 5_2_ASSIGN_TO_NON_REGISTER_IN_SYNC-non_register_in_sync.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| WRITE_WIRE_IN_SYNC | error | No dedicated 5_2-prefixed test; related to ASSIGN_TO_NON_REGISTER_IN_SYNC |
| SYNC_SLICE_WIDTH_MISMATCH | error | No dedicated validation test for slice width mismatch in SYNC block |
