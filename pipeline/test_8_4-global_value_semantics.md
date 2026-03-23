# Test Plan: 8.4 Global Value Semantics
**Specification Reference:** Section 8.4 of jz-hdl-specification.md
## 1. Objective
Verify global constants usable as runtime values in assignments, expressions, operators, conditionals, concatenations.
## 2. Instrumentation Strategy
- **Span: `sem.global_usage`** — attributes: `context`.
## 3. Test Scenarios
### 3.1 Happy Path
1. RHS of ASYNC assignment: `opcode <= ISA.INST_ADD;`
2. In expression: `ISA.INST_ADD == opcode`
3. In concatenation: `{ISA.INST_ADD, 15'b0}`
4. In SYNCHRONOUS: `reg <= ISA.INST_ADD;`
### 3.3 Negative Testing
1. Width mismatch without modifier — Error
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Width mismatch | Error | WIDTH_ASSIGN_MISMATCH_NO_EXT | S2.3 |
## 5-6. Standard integration with driver_expr.c and driver_width.c.
