# Test Plan: 8.3 Global Semantics
**Specification Reference:** Section 8.3 of jz-hdl-specification.md
## 1. Objective
Verify namespace root creation (`<global>.<id>`), sized literal width rules, overflow detection, uniqueness within block.
## 2. Instrumentation Strategy
- **Span: `sem.global_resolve`** — attributes: `global_name`, `const_id`, `width`, `value`.
## 3. Test Scenarios
### 3.1 Happy Path
1. Reference via `ISA.INST_ADD` — resolves correctly
2. Width matches target in assignment
### 3.2 Boundary/Edge Cases
1. Multiple blocks with disjoint namespaces
### 3.3 Negative Testing
1. Duplicate const_id in same block — Error
2. Undefined global reference — Error
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Dup const in block | Error | GLOBAL_DUPLICATE_CONST | S8.5 |
| 2 | Undefined global ref | Error | — | Undefined identifier |
## 5. Integration Points
| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver.c` | Global resolution | Integration test |
## 6. Rules Matrix
### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| GLOBAL_DUPLICATE_CONST | Dup const in block | Neg 1 |
### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | — | — |
