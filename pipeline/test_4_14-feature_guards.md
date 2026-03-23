# Test Plan: 4.14 Feature Guards

**Specification Reference:** Section 4.14 of jz-hdl-specification.md

## 1. Objective

Verify @feature/@else/@endfeat conditional compilation: expression evaluation (CONFIG/CONST only), both-configuration validation (enabled AND disabled must pass semantic checks), no nesting, scope transparency (identifiers inside are module-visible), and error detection for missing @else when needed, reference to feature-guarded identifiers outside guards, and runtime signal references in feature expressions.

## 2. Instrumentation Strategy

- **Span: `sem.feature_guard`** — Trace feature evaluation; attributes: `expression`, `evaluated_value`, `is_enabled`.
- **Span: `sem.feature_both_check`** — Validation of both configurations; attributes: `config_name`, `enabled_valid`, `disabled_valid`.
- **Event: `feature.nested`** — Nested @feature detected.
- **Event: `feature.runtime_ref`** — Runtime signal in feature expression.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Feature with @else | Both branches provide valid semantics |
| 2 | Feature in REGISTER block | Conditional register declaration |
| 3 | Feature in ASYNCHRONOUS | Conditional assignments |
| 4 | Feature in SYNCHRONOUS | Conditional sequential logic |
| 5 | Feature with CONST expr | `@feature INC == 1` using module CONST |
| 6 | Feature with CONFIG expr | `@feature CONFIG.DEBUG == 1` using project CONFIG |
| 7 | Logical operators | `@feature A == 1 && B == 0` |
| 8 | Comparison operators | `@feature WIDTH > 16` |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Feature always true | CONST makes condition always true |
| 2 | Feature always false | CONST makes condition always false |
| 3 | Empty feature body | `@feature X == 1 @endfeat` — valid |
| 4 | Feature around entire module body | Guards declarations + logic |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Nested @feature | `@feature X @feature Y ... @endfeat @endfeat` — Error |
| 2 | Missing @else (OUT undriven) | Feature guards driver of OUT port, no @else — Error in disabled config |
| 3 | Reference outside guard | Feature declares wire, used outside guard — Error in disabled config |
| 4 | Runtime signal in expr | `@feature wire_val == 1` — Error: must be CONFIG/CONST |
| 5 | Missing @endfeat | `@feature X` without closing — Error |
| 6 | @else without @feature | Standalone `@else` — Error |
| 7 | Feature expr not boolean | `@feature 8'd255` — must be width-1 |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Nested @feature | Error | FEATURE_NESTED | S4.14 |
| 2 | OUT undriven when disabled | Error | PORT_OUTPUT_NOT_DRIVEN | Both configs must pass |
| 3 | Ref to guarded wire outside | Error | — | Undefined identifier in disabled config |
| 4 | Runtime signal in expr | Error | FEATURE_RUNTIME_REF | CONFIG/CONST only |
| 5 | Missing @endfeat | Error | — | Parse error |
| 6 | Valid @feature with @else | Valid in both configurations | — | Happy path |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_core.c` | Parses @feature directives | Token stream |
| `const_eval.c` | Evaluates feature expressions | Unit test with CONFIG/CONST |
| `driver.c` | Semantic validation of both configs | Run analysis twice |
| `diagnostic.c` | Error reporting | Capture diagnostics |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| FEATURE_NESTED | Nested @feature blocks | Neg 1 |
| FEATURE_RUNTIME_REF | Runtime signal in feature expression | Neg 4 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| FEATURE_EXPR_NOT_BOOLEAN | S4.14 "width-1 boolean" | Expression must evaluate to boolean |
| FEATURE_MISSING_ENDFEAT | S4.14 | Unclosed feature block |
| FEATURE_ORPHAN_ELSE | S4.14 | @else without @feature |
| FEATURE_BOTH_CONFIG_INVALID | S4.14 "both enabled and disabled" | Needs rule for semantic failure in alternate config |
