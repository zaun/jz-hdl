# Test Plan: 11.4 Transformation Algorithm (11.4.1-11.4.2)
**Specification Reference:** Section 11.4 of jz-hdl-specification.md
## 1. Objective
Verify priority-chain conversion (z replaced with default, drivers chained via enable conditions) and whole-signal transformation policy.
## 2. Instrumentation Strategy
- **Span: `ir.tristate_chain`** — attributes: `chain_depth`, `enable_conditions`.
## 3. Test Scenarios
### 3.1 Happy Path
1. Two-driver net → priority chain with one enable
2. Three-driver net → two-level priority chain
### 3.3 Negative Testing
1. Per-bit tri-state pattern — Error (TRISTATE_TRANSFORM_PER_BIT_FAIL)
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Per-bit z pattern | Error | TRISTATE_TRANSFORM_PER_BIT_FAIL | S11.7 |
## 5-6. See 11.1 plan.
