# Test Plan: 11.8 Tristate Portability Guidelines
**Specification Reference:** Section 11.8 of jz-hdl-specification.md
## 1. Objective
Verify best practices for portable designs: internal tri-state with flag, INOUT for external only.
## 2. Instrumentation Strategy
- **Event: `tristate.portability_warning`** — Internal tri-state without flag.
## 3. Test Scenarios
### 3.1 Happy Path
1. Design with --tristate-default compiles for FPGA
2. Same design without flag compiles for ASIC/sim
### 3.2 Boundary/Edge Cases
1. Design with no internal tri-state — flag is no-op
## 4-6. See 11.1 plan.
