# Test Plan: 9.4 @check Expression Rules
**Specification Reference:** Section 9.4 of jz-hdl-specification.md
## 1. Objective
Verify allowed operands (literals, CONST, CONFIG, clog2, comparisons, logical) and forbidden (ports, wires, registers, memory, slices, runtime).
## 2. Instrumentation Strategy
- **Span: `sem.check_expr`** — attributes: `operand_types`, `is_constant`.
## 3. Test Scenarios
### 3.1 Happy Path
1. Integer literal, CONST, CONFIG, clog2, logical operators
### 3.3 Negative Testing
1. Port reference — Error
2. Wire reference — Error
3. Register reference — Error
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Port in @check | Error | CHECK_RUNTIME_OPERAND | S9.4 |
## 5-6. Cross-reference with 9.7.
