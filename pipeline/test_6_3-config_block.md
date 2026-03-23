# Test Plan: 6.3 CONFIG Block (including 6.3.1)

**Specification Reference:** Section 6.3 and 6.3.1 of jz-hdl-specification.md

## 1. Objective

Verify CONFIG block: numeric and string entries, project-wide visibility via `CONFIG.<name>`, compile-time-only usage restriction, type restrictions (string in numeric context, numeric in string context), forward reference/circular dependency detection, and runtime usage prohibition.

## 2. Instrumentation Strategy

- **Span: `sem.config_eval`** — attributes: `config_name`, `value`, `type`.
- **Event: `config.runtime_use`** — CONFIG used in runtime expression.
- **Event: `config.circular`** — Circular dependency detected.

## 3. Test Scenarios

### 3.1 Happy Path
1. Numeric CONFIG: `XLEN = 32;`
2. String CONFIG: `FIRMWARE = "out/fw.bin";`
3. CONFIG in port width: `IN [CONFIG.XLEN] data;`
4. CONFIG in MEM depth: `MEM { m [8] [CONFIG.DEPTH] ... }`
5. CONFIG referencing earlier CONFIG: `TOTAL = CONFIG.A + CONFIG.B;`
6. CONFIG.name in CONST initializer

### 3.2 Boundary/Edge Cases
1. CONFIG value = 0 — valid but may not work as width
2. Single CONFIG entry
3. CONFIG and CONST same name — no shadowing, disjoint

### 3.3 Negative Testing
1. Forward reference: `A = CONFIG.B; B = 5;`
2. Circular: `A = CONFIG.B; B = CONFIG.A;`
3. Runtime use: `data <= CONFIG.XLEN;` — Error
4. String in numeric: `[CONFIG.FIRMWARE]` — Error
5. Numeric in string: `@file(CONFIG.XLEN)` — Error
6. Duplicate CONFIG name
7. CONFIG outside @project

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `A = CONFIG.B; B = 5;` | Error | CONFIG_FORWARD_REF | S6.3 |
| 2 | Circular CONFIG | Error | CONFIG_CIRCULAR_DEP | S6.3 |
| 3 | `data <= CONFIG.X;` | Error | CONFIG_RUNTIME_USE | S6.3.1 |
| 4 | String in numeric | Error | CONST_STRING_IN_NUMERIC_CONTEXT | S6.3 |
| 5 | Numeric in string | Error | CONST_NUMERIC_IN_STRING_CONTEXT | S6.3 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `const_eval.c` | CONFIG evaluation | Unit test |
| `driver_project.c` | CONFIG scope validation | Integration test |
| `parser_project.c` | CONFIG parsing | Token stream |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| CONFIG_FORWARD_REF | Forward reference in CONFIG | Neg 1 |
| CONFIG_CIRCULAR_DEP | Circular CONFIG dependency | Neg 2 |
| CONST_STRING_IN_NUMERIC_CONTEXT | String used as number | Neg 4 |
| CONST_NUMERIC_IN_STRING_CONTEXT | Number used as string | Neg 5 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| CONFIG_RUNTIME_USE | S6.3.1 "may not be used as runtime literal" | Explicit rule needed |
