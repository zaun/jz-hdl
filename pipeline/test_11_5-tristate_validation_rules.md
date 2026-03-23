# Test Plan: 11.5 Tri-State Validation Rules
**Specification Reference:** Section 11.5 of jz-hdl-specification.md
## 1. Objective
Verify validation rules applied before/after transformation: mutual exclusion proof, full-width z requirement.
## 2. Instrumentation Strategy
- **Span: `ir.tristate_validate`** — attributes: `validation_result`.
## 3. Test Scenarios
### 3.1 Happy Path
1. Mutually exclusive drivers → valid transformation
### 3.3 Negative Testing
1. Non-mutually-exclusive enables — Error (TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL)
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Non-exclusive enables | Error | TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | S11.7 |
## 5-6. See 11.1 plan.
