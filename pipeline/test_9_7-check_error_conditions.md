# Test Plan: 9.7 @check Error Conditions
**Specification Reference:** Section 9.7 of jz-hdl-specification.md
## 1. Objective
Verify all @check error conditions: false expression, runtime signal, undefined identifier, non-integer, disallowed operators.
## 2. Instrumentation Strategy
- **Span: `diagnostic.check`** — Trace all @check diagnostics.
## 3. Test Scenarios
### 3.3 Negative Testing
1. Expression evaluates to zero — Error with message
2. Runtime signal — Error
3. Undefined identifier — Error
4. Non-integer result — Error
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `@check (0, "fail")` | Error: CHECK FAILED | CHECK_FAILED | S9.7 |
| 2 | Runtime signal | Error | CHECK_NON_CONSTANT | S9.7 |
| 3 | Undefined ID | Error | CHECK_UNDEFINED | S9.7 |
## 5-6. See earlier @check plans.
