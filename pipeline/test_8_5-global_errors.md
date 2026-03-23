# Test Plan: 8.5 Global Errors
**Specification Reference:** Section 8.5 of jz-hdl-specification.md
## 1. Objective
Verify all @global error conditions: dup name, dup const, invalid ID, forward ref, non-integer, unsized literal, overflow, assign to global.
## 2. Instrumentation Strategy
- **Span: `diagnostic.global`** — Trace all @global diagnostics.
## 3. Test Scenarios
### 3.3 Negative Testing
1. Duplicate @global name — Error
2. Duplicate const_id — Error
3. Forward reference — Error
4. Unsized literal — Error
5. Literal overflow — Error
6. Write to global — Error
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Dup name | Error | GLOBAL_DUPLICATE_NAME | S8.5 |
| 2 | Dup const | Error | GLOBAL_DUPLICATE_CONST | S8.5 |
| 3 | Forward ref | Error | GLOBAL_FORWARD_REF | S8.5 |
| 4 | Unsized | Error | GLOBAL_NOT_SIZED_LITERAL | S8.5 |
| 5 | Overflow | Error | LIT_OVERFLOW | S2.1 |
| 6 | Write | Error | GLOBAL_READONLY | S8.5 |
## 5-6. See Section 8.1 plan.
