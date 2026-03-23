# Test Plan: 11.3 Tri-State Net Identification
**Specification Reference:** Section 11.3 of jz-hdl-specification.md
## 1. Objective
Verify compiler correctly identifies internal tri-state nets (nets with z assignments from multiple drivers).
## 2. Instrumentation Strategy
- **Span: `ir.tristate_identify`** — attributes: `net_name`, `driver_count`, `z_driver_count`.
## 3. Test Scenarios
### 3.1 Happy Path
1. Net with conditional z assignment identified as tri-state
2. Net with no z not identified
### 3.2 Boundary/Edge Cases
1. Net with z in only one path
## 4-6. See 11.1 plan.
