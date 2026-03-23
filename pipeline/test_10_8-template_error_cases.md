# Test Plan: 10.8 Template Error Cases
**Specification Reference:** Section 10.8 of jz-hdl-specification.md
## 1. Objective
Verify canonical error cases: illegal declaration, scratch outside template, illegal nested template.
## 2. Instrumentation Strategy
- **Span: `diagnostic.template`** — Trace all template diagnostics.
## 3. Test Scenarios
### 3.3 Negative Testing
1. WIRE inside template — Error (TEMPLATE_FORBIDDEN_DECL)
2. @scratch outside template — Error (TEMPLATE_SCRATCH_OUTSIDE)
3. Nested @template — Error (TEMPLATE_NESTED_DEF)
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | WIRE in template | Error | TEMPLATE_FORBIDDEN_DECL | S10.8 |
| 2 | @scratch outside | Error | TEMPLATE_SCRATCH_OUTSIDE | S10.8 |
| 3 | Nested template | Error | TEMPLATE_NESTED_DEF | S10.8 |
## 5-6. See 10.2 for integration.
