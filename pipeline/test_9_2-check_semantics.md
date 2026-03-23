# Test Plan: 9.2 @check Semantics
**Specification Reference:** Section 9.2 of jz-hdl-specification.md
## 1. Objective
Verify @check evaluation: true (nonzero) passes, false (zero) emits error with message, non-constant expression fails.
## 2. Instrumentation Strategy
- **Span: `sem.check_eval`** — attributes: `expr_value`, `is_constant`, `passed`.
## 3. Test Scenarios
### 3.1 Happy Path
1. True condition passes
2. CONST-based expression
### 3.3 Negative Testing
1. False condition — Error with message
2. Runtime signal in expr — Error
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | False @check | Error: "CHECK FAILED: ..." | CHECK_FAILED | S9.2 |
| 2 | Runtime signal | Error | CHECK_NON_CONSTANT | S9.2 |
## 5-6. See Section 9.7 for comprehensive error list.
