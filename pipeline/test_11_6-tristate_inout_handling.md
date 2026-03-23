# Test Plan: 11.6 Handling of INOUT Ports and External Pins
**Specification Reference:** Section 11.6 of jz-hdl-specification.md
## 1. Objective
Verify INOUT ports are not transformed (preserve tri-state for external interface) and internal tri-state driving INOUT is handled correctly.
## 2. Instrumentation Strategy
- **Span: `ir.tristate_inout`** — attributes: `port_name`, `is_preserved`.
## 3. Test Scenarios
### 3.1 Happy Path
1. INOUT port preserved with z capability
2. Internal driver to INOUT with output-enable extraction
### 3.3 Negative Testing
1. OE extraction failure — Error (TRISTATE_TRANSFORM_OE_EXTRACT_FAIL)
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | OE extract fail | Error | TRISTATE_TRANSFORM_OE_EXTRACT_FAIL | S11.7 |
## 5-6. See 11.1 plan.
