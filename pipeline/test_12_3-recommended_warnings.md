# Test Plan: 12.3 Recommended Warnings
**Specification Reference:** Section 12.3 of jz-hdl-specification.md
## 1. Objective
Verify all recommended warnings: unused register, unsinked register, undriven register, unconnected output, incomplete SELECT, dead code, unused module, unused wire, unused port, internal tristate.
## 2. Instrumentation Strategy
- **Span: `diagnostic.warnings`** — Trace all warning-level diagnostics.
## 3. Test Scenarios
### 3.1 Happy Path
1. Clean design with no warnings
### 3.3 Warning Conditions
1. Unused register — WARN_UNUSED_REGISTER
2. Written never read — WARN_UNSINKED_REGISTER
3. Read never written — WARN_UNDRIVEN_REGISTER
4. Unconnected output — WARN_UNCONNECTED_OUTPUT
5. SELECT without DEFAULT in ASYNC — WARN_INCOMPLETE_SELECT_ASYNC
6. Dead code — WARN_DEAD_CODE_UNREACHABLE
7. Unused module — WARN_UNUSED_MODULE
8. Unused wire — WARN_UNUSED_WIRE
9. Unused port — WARN_UNUSED_PORT
10. Internal tristate — WARN_INTERNAL_TRISTATE
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Unused register | Warning | WARN_UNUSED_REGISTER | S12.3 |
| 2 | Written never read | Warning | WARN_UNSINKED_REGISTER | S12.3 |
| 3 | Read never written | Warning | WARN_UNDRIVEN_REGISTER | S12.3 |
| 4 | Unconnected output | Warning | WARN_UNCONNECTED_OUTPUT | S12.3 |
| 5 | Incomplete SELECT | Warning | WARN_INCOMPLETE_SELECT_ASYNC | S12.3 |
| 6 | Dead code | Warning | WARN_DEAD_CODE_UNREACHABLE | S12.3 |
| 7 | Unused module | Warning | WARN_UNUSED_MODULE | S12.3 |
| 8 | Unused wire | Warning | WARN_UNUSED_WIRE | S12.3 |
| 9 | Unused port | Warning | WARN_UNUSED_PORT | S12.3 |
| 10 | Internal tristate | Warning | WARN_INTERNAL_TRISTATE | S12.3 |
## 5. Integration Points
| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver.c` | Warning generation | Full pipeline test |
| `diagnostic.c` | Warning formatting | Capture output |
## 6. Rules Matrix
### 6.1 Rules Tested
All 10 warning rules listed above.
### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | All recommended warnings appear covered in rules.c | — |
