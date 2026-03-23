# Test Plan: 11.2 --tristate-default Applicability and Scope
**Specification Reference:** Section 11.2 of jz-hdl-specification.md
## 1. Objective
Verify applicable contexts (internal WIREs with tri-state drivers) and non-applicable contexts (INOUT ports, external pins, top-level).
## 2. Instrumentation Strategy
- **Span: `ir.tristate_scope`** — attributes: `net_type`, `is_applicable`.
## 3. Test Scenarios
### 3.1 Happy Path
1. Internal wire with tri-state → transformed
2. INOUT port → NOT transformed (external)
### 3.3 Negative Testing
1. Transform applied to INOUT port — should be skipped, not error
## 4-6. See 11.1 for integration.
