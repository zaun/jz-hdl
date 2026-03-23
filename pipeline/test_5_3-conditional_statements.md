# Test Plan: 5.3 Conditional Statements (IF / ELIF / ELSE)

**Specification Reference:** Section 5.3 of jz-hdl-specification.md

## 1. Objective

Verify IF/ELIF/ELSE syntax (parenthesized condition, width-1 condition), branch exclusivity for assignments, nesting, combinational loop detection with flow-sensitivity, and interaction with exclusive assignment rule.

## 2. Instrumentation Strategy

- **Span: `parser.conditional`** — Trace IF parsing; attributes: `has_elif`, `elif_count`, `has_else`.
- **Span: `sem.branch_analysis`** — Branch exclusivity; attributes: `branch_count`, `assignments_per_branch`.
- **Event: `cond.not_1bit`** — Condition expression not width-1.
- **Event: `cond.missing_parens`** — Missing parentheses.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Simple IF/ELSE | `IF (c) { w <= a; } ELSE { w <= b; }` |
| 2 | IF/ELIF/ELSE | Three branches, each assigns output |
| 3 | Nested IF | IF inside IF — deeper nesting |
| 4 | IF without ELSE | Valid in SYNC (register holds) |
| 5 | Flow-sensitive no loop | `IF (sel) { a = b; } ELSE { b = a; }` — no cycle |
| 6 | Multiple ELIFs | `IF ... ELIF ... ELIF ... ELSE ...` |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Deeply nested (10 levels) | Valid |
| 2 | Empty IF body | `IF (c) { }` — valid |
| 3 | IF with only ELSE | Not valid syntax — must have IF first |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Missing parens | `IF c { ... }` — Error |
| 2 | Condition not 1-bit | `IF (8'd5) { ... }` — Error |
| 3 | Independent IFs same target | `IF (a) { w <= x; } IF (b) { w <= y; }` — Error |
| 4 | Unconditional loop | `a = b; b = a;` — Error |
| 5 | IF without ELSE in ASYNC | Missing driver for net in else path — Error (partial coverage) |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `IF c { ... }` | Error | IF_COND_MISSING_PARENS | S5.3 |
| 2 | `IF (8'd5) { ... }` | Error | — | Condition width check |
| 3 | Two independent IFs | Error | ASSIGN_INDEPENDENT_CHAIN_CONFLICT | S1.5.2 |
| 4 | `a = b; b = a;` | Error | COMB_LOOP_DETECTED | S12.2 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_statements.c` | Parses IF/ELIF/ELSE | Token stream |
| `driver_control.c` | Control flow analysis | Integration test |
| `driver_flow.c` | Path enumeration | Integration test |
| `driver_comb.c` | Combinational loop detection | Integration test |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| IF_COND_MISSING_PARENS | Missing parentheses on IF condition | Neg 1 |
| ASSIGN_INDEPENDENT_CHAIN_CONFLICT | Independent chains assign same signal | Neg 3 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| IF_COND_NOT_1BIT | S5.3 "width-1" | Condition wider than 1 bit |
