# Test Plan: 5.3 Conditional Statements

**Specification Reference:** Section 5.3 of jz-hdl-specification.md

## 1. Objective

Verify IF/ELIF/ELSE syntax (parenthesized conditions, width-1 condition requirement), branch exclusivity for assignments, nesting, combinational loop detection with flow-sensitivity, and interaction with exclusive assignment rule.

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

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Missing parens on IF | `IF c { ... }` | Error | IF_COND_MISSING_PARENS |
| 2 | Missing parens on ELIF | `ELIF c { ... }` | Error | IF_COND_MISSING_PARENS |
| 3 | Condition not 1-bit | `IF (8'd5) { ... }` | Error | IF_COND_WIDTH_NOT_1 |
| 4 | Control flow outside block | IF/SELECT outside ASYNC/SYNC block | Error | CONTROL_FLOW_OUTSIDE_BLOCK |
| 5 | Unconditional combinational loop | `a = b; b = a;` in ASYNC | Error | COMB_LOOP_UNCONDITIONAL |
| 6 | Conditional safe cycle | Cycle only within mutually exclusive branches | Warning | COMB_LOOP_CONDITIONAL_SAFE |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Deeply nested (10 levels) | 10+ nesting levels of IF | Valid |
| 2 | Empty IF body | `IF (c) { }` | Valid |
| 3 | IF with only ELSE | Missing IF before ELSE | Parse error |
| 4 | IF without ELSE in ASYNC | Missing driver for net in else path | Error: ASYNC_UNDEFINED_PATH_NO_DRIVER |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `IF c { ... }` | Compile error | IF_COND_MISSING_PARENS | error | Parentheses required |
| 2 | `ELIF c { ... }` | Compile error | IF_COND_MISSING_PARENS | error | ELIF also requires parens |
| 3 | `IF (8'd5) { ... }` | Compile error | IF_COND_WIDTH_NOT_1 | error | Condition must be 1 bit |
| 4 | IF outside ASYNC/SYNC | Compile error | CONTROL_FLOW_OUTSIDE_BLOCK | error | Must be inside block |
| 5 | `a = b; b = a;` in ASYNC | Compile error | COMB_LOOP_UNCONDITIONAL | error | Unconditional feedback |
| 6 | Cycle in exclusive branches | Warning | COMB_LOOP_CONDITIONAL_SAFE | warning | Safe if mutually exclusive |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 5_3_IF_COND_MISSING_PARENS-missing_parentheses.jz | IF_COND_MISSING_PARENS | IF condition missing required parentheses |
| 5_3_IF_COND_MISSING_PARENS-elif_missing_parens.jz | IF_COND_MISSING_PARENS | ELIF condition missing required parentheses |
| 5_3_IF_COND_WIDTH_NOT_1-condition_not_1bit.jz | IF_COND_WIDTH_NOT_1 | IF/ELIF condition wider than 1 bit |
| 5_3_COMB_LOOP_UNCONDITIONAL-unconditional_loop.jz | COMB_LOOP_UNCONDITIONAL | Unconditional combinational feedback loop in ASYNC |
| 5_3_COMB_LOOP_CONDITIONAL_SAFE-conditional_safe_cycle.jz | COMB_LOOP_CONDITIONAL_SAFE | Cycle only within mutually exclusive branches (safe warning) |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| IF_COND_MISSING_PARENS | error | S5.3 IF/ELIF condition missing required parentheses | 5_3_IF_COND_MISSING_PARENS-missing_parentheses.jz, 5_3_IF_COND_MISSING_PARENS-elif_missing_parens.jz |
| IF_COND_WIDTH_NOT_1 | error | S5.3 IF/ELIF condition must be 1 bit wide | 5_3_IF_COND_WIDTH_NOT_1-condition_not_1bit.jz |
| COMB_LOOP_UNCONDITIONAL | error | S5.3/S8.2 Combinational loop: signal feeds back to itself | 5_3_COMB_LOOP_UNCONDITIONAL-unconditional_loop.jz |
| COMB_LOOP_CONDITIONAL_SAFE | warning | S5.3/S8.2 Cycles only within mutually exclusive branches | 5_3_COMB_LOOP_CONDITIONAL_SAFE-conditional_safe_cycle.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| CONTROL_FLOW_OUTSIDE_BLOCK | error | No dedicated 5_3-prefixed test for control flow outside ASYNC/SYNC block |
