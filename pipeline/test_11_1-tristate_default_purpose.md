# Test Plan: 11.1 --tristate-default Purpose and Overview
**Specification Reference:** Section 11.1 of jz-hdl-specification.md
## 1. Objective
Verify --tristate-default flag converts internal tri-state nets to priority-chained conditional logic with GND or VCC default.
## 2. Instrumentation Strategy
- **Span: `ir.tristate_transform`** — attributes: `flag_value` (GND/VCC/none), `nets_transformed`.
## 3. Test Scenarios
### 3.1 Happy Path
1. `--tristate-default=GND` transforms z to 0
2. `--tristate-default=VCC` transforms z to 1
3. No flag: z preserved (ASIC/sim mode)
### 3.3 Negative Testing
1. Invalid flag value — Error
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `--tristate-default=GND` | z→0 transform | — | S11.1 |
| 2 | No flag, internal tristate | Warning | WARN_INTERNAL_TRISTATE | S11 |
## 5. Integration Points
| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `ir_tristate_transform.c` | Transform engine | Integration test |
## 6. Rules Matrix
### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| WARN_INTERNAL_TRISTATE | Internal tri-state without flag | Happy 3 |
| INFO_TRISTATE_TRANSFORM | Net transformed | Happy 1, 2 |
### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | — | — |
