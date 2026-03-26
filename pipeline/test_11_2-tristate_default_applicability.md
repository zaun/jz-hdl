# Test Plan: 11.2 --tristate-default Applicability and Scope

**Specification Reference:** Section 11.2 of jz-hdl-specification.md

## 1. Objective

Verify that `--tristate-default` applies only to internal WIREs with tri-state drivers. INOUT ports, external pins, and top-level signals must not be transformed and must preserve their tri-state behavior.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Internal wire with tri-state | Wire driven by conditional z with flag | Net transformed |
| 2 | INOUT port preserved | INOUT port with tri-state and flag | NOT transformed (external) |
| 3 | Top-level pin preserved | Top-level external pin with tri-state | NOT transformed |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| — | No specific error rules for this subsection | — | — |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Transform applied to INOUT port | INOUT port with flag present | Skipped (not error, just not transformed) |
| 2 | Mixed internal and external tri-state | Design with both internal wires and INOUT ports | Only internal wires transformed |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| — | No specific diagnostic rules for applicability | — | — | — | Scope definition only |

## 4. Existing Validation Tests

No validation tests specific to this subsection. Applicability is a scope definition rather than a diagnosable condition.

## 5. Rules Matrix

### 5.1 Rules Tested

No specific rules defined for this subsection. Section 11.2 defines the scope of applicability for the `--tristate-default` flag rather than introducing new diagnostic rules.

### 5.2 Rules Not Tested


All rules for this section are tested.
