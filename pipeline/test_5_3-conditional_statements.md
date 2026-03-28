# Test Plan: 5.3 Conditional Statements

**Specification Reference:** Section 5.3 of jz-hdl-specification.md

## 1. Objective

Verify IF/ELIF/ELSE syntax (parenthesized conditions, width-1 condition requirement), branch exclusivity for assignments, nesting, combinational loop detection with flow-sensitivity, alias-in-conditional prohibition, control-flow-outside-block detection, and interaction with exclusive assignment rule.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Simple IF/ELSE | `IF (c) { w <= a; } ELSE { w <= b; }` | Valid, both paths drive w |
| 2 | IF/ELIF/ELSE | Three branches, each assigns output | Valid |
| 3 | Nested IF | IF inside IF, deeper nesting | Valid |
| 4 | IF without ELSE in SYNC | `IF (load) { reg <= val; }` | Valid, register holds |
| 5 | Flow-sensitive no loop | `IF (sel) { a = b; } ELSE { b = a; }` | Valid, no cycle in any single path |
| 6 | Multiple ELIFs | `IF ... ELIF ... ELIF ... ELSE ...` | Valid |
| 7 | Nested IF in both ASYNC and SYNC | IF inside ASYNC and SYNC blocks | Valid |
| 8 | Empty ELSE body | `IF (c) { w <= a; } ELSE { }` | Valid |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Missing parens on IF | `IF c { ... }` | Error | IF_COND_MISSING_PARENS |
| 2 | Missing parens on ELIF | `ELIF c { ... }` | Error | IF_COND_MISSING_PARENS |
| 3 | Missing parens on IF in SYNC | `IF c { ... }` in SYNC block | Error | IF_COND_MISSING_PARENS |
| 4 | Condition not 1-bit | `IF (8'd5) { ... }` | Error | IF_COND_WIDTH_NOT_1 |
| 5 | Control flow outside block | IF statement outside ASYNC/SYNC block | Error | CONTROL_FLOW_OUTSIDE_BLOCK |
| 6 | Alias inside conditional in ASYNC | `IF (c) { a = b; }` alias in branch | Error | ASYNC_ALIAS_IN_CONDITIONAL |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Deeply nested (10 levels) | 10+ nesting levels of IF | Valid |
| 2 | Empty IF body | `IF (c) { }` | Valid |
| 3 | IF with only ELSE | Missing IF before ELSE | Parse error |
| 4 | IF without ELSE in ASYNC | Missing driver for net in else path | Error: ASYNC_UNDEFINED_PATH_NO_DRIVER |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | IF condition missing parentheses | `IF c { ... }` | IF_COND_MISSING_PARENS | error |
| 2 | ELIF condition missing parentheses | `ELIF c { ... }` | IF_COND_MISSING_PARENS | error |
| 3 | IF parens missing in SYNC | `IF c { ... }` in SYNCHRONOUS | IF_COND_MISSING_PARENS | error |
| 4 | IF condition wider than 1 bit | `IF (8'd5) { ... }` | IF_COND_WIDTH_NOT_1 | error |
| 5 | IF/SELECT outside any block | IF at module scope | CONTROL_FLOW_OUTSIDE_BLOCK | error |
| 6 | Alias operator inside IF branch in ASYNC | `IF (c) { a = b; }` | ASYNC_ALIAS_IN_CONDITIONAL | error |
| 7 | Valid IF/ELSE in ASYNC | `IF (sel) { a <= x; } ELSE { a <= y; }` | -- | pass |
| 8 | Flow-sensitive mutually exclusive | `IF (s) { a=b; } ELSE { b=a; }` | -- | pass |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 5_3_HAPPY_PATH-conditional_statements_ok.jz | -- | Valid IF/ELIF/ELSE forms accepted |
| 5_3_ASYNC_ALIAS_IN_CONDITIONAL-alias_in_conditional.jz | ASYNC_ALIAS_IN_CONDITIONAL | Alias operator inside conditional branch in ASYNC |
| 5_3_COMB_LOOP_UNCONDITIONAL-unconditional_loop.jz | COMB_LOOP_UNCONDITIONAL | Unconditional combinational feedback loop in ASYNC |
| 5_3_COMB_LOOP_CONDITIONAL_SAFE-conditional_safe_cycle.jz | COMB_LOOP_CONDITIONAL_SAFE | Cycle only within mutually exclusive branches (safe warning) |
| 5_3_CONTROL_FLOW_OUTSIDE_BLOCK-if_outside_block.jz | CONTROL_FLOW_OUTSIDE_BLOCK | IF statement placed outside any ASYNC/SYNC block |
| 5_3_IF_COND_MISSING_PARENS-missing_parentheses.jz | IF_COND_MISSING_PARENS | IF condition missing required parentheses |
| 5_3_IF_COND_MISSING_PARENS-elif_missing_parens.jz | IF_COND_MISSING_PARENS | ELIF condition missing required parentheses |
| 5_3_IF_COND_MISSING_PARENS-sync_missing_parens.jz | IF_COND_MISSING_PARENS | IF condition missing parentheses in SYNC block |
| 5_3_IF_COND_WIDTH_NOT_1-condition_not_1bit.jz | IF_COND_WIDTH_NOT_1 | IF/ELIF condition wider than 1 bit |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| ASYNC_ALIAS_IN_CONDITIONAL | error | S4.10/S5.3 Alias operator inside conditional branch in ASYNC | 5_3_ASYNC_ALIAS_IN_CONDITIONAL-alias_in_conditional.jz |
| CONTROL_FLOW_OUTSIDE_BLOCK | error | S5.3/S5.4/S8.1 IF/SELECT outside ASYNC/SYNC block | 5_3_CONTROL_FLOW_OUTSIDE_BLOCK-if_outside_block.jz |
| IF_COND_MISSING_PARENS | error | S5.3 IF/ELIF condition missing required parentheses | 5_3_IF_COND_MISSING_PARENS-missing_parentheses.jz, 5_3_IF_COND_MISSING_PARENS-elif_missing_parens.jz, 5_3_IF_COND_MISSING_PARENS-sync_missing_parens.jz |
| IF_COND_WIDTH_NOT_1 | error | S5.3 IF/ELIF condition must be 1 bit wide | 5_3_IF_COND_WIDTH_NOT_1-condition_not_1bit.jz |

### 5.2 Rules Not Tested (in this section)

All rules for this section are tested.

### 5.3 Additional Rules Tested (not in primary S5.3 assignment)

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| COMB_LOOP_UNCONDITIONAL | error | S12.2 Unconditional combinational feedback loop | 5_3_COMB_LOOP_UNCONDITIONAL-unconditional_loop.jz |
| COMB_LOOP_CONDITIONAL_SAFE | warning | S12.2 Cycle only within mutually exclusive branches | 5_3_COMB_LOOP_CONDITIONAL_SAFE-conditional_safe_cycle.jz |
