# Test Plan: 11.7 Tri-State Error Conditions and Warnings
**Specification Reference:** Section 11.7 of jz-hdl-specification.md
## 1. Objective
Verify all tristate transform errors and warnings.
## 2. Instrumentation Strategy
- **Span: `diagnostic.tristate`** — Trace all tristate diagnostics.
## 3. Test Scenarios
### 3.3 Negative Testing
1. Mutual exclusion fail — Error
2. Per-bit fail — Error
3. Blackbox port — Error
4. OE extraction fail — Error
5. Single driver — Warning
6. No tri-state nets found — Warning (unused default)
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Non-exclusive | Error | TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | S11.7 |
| 2 | Per-bit | Error | TRISTATE_TRANSFORM_PER_BIT_FAIL | S11.7 |
| 3 | Blackbox | Error | TRISTATE_TRANSFORM_BLACKBOX_PORT | S11.7 |
| 4 | OE fail | Error | TRISTATE_TRANSFORM_OE_EXTRACT_FAIL | S11.7 |
| 5 | Single driver | Warning | TRISTATE_TRANSFORM_SINGLE_DRIVER | S11.7 |
| 6 | No nets | Warning | TRISTATE_TRANSFORM_UNUSED_DEFAULT | S11.7 |
## 5-6. See 11.1 plan.
