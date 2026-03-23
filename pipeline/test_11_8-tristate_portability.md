# Test Plan: 11.8 Tristate Portability Guidelines

**Specification Reference:** Section 11.8 of jz-hdl-specification.md

## 1. Objective

Verify best practices for portable designs: use `--tristate-default` for FPGA targets, reserve INOUT for external interfaces only, and ensure designs compile cleanly for both FPGA and ASIC/simulation targets.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | FPGA compile with flag | Design with `--tristate-default=GND` | Compiles with transforms applied |
| 2 | ASIC/sim compile without flag | Same design without flag | Compiles with WARN_INTERNAL_TRISTATE |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| — | No specific error rules for this subsection | — | — |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | No internal tri-state | Design with no internal tri-state, flag present | Flag is no-op, TRISTATE_TRANSFORM_UNUSED_DEFAULT warning |
| 2 | INOUT-only design | Design using only INOUT for tri-state | No transformation needed, no warnings |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| — | No specific diagnostic rules for this subsection | — | — | — | Best practices guidance |

## 4. Existing Validation Tests

No validation tests specific to this subsection. Section 11.8 provides portability best practices rather than introducing new diagnostic rules.

## 5. Rules Matrix

### 5.1 Rules Tested

No specific rules defined for this subsection. Section 11.8 is a best practices section that cross-references rules from other subsections (WARN_INTERNAL_TRISTATE from 11.1, TRISTATE_TRANSFORM_UNUSED_DEFAULT from 11.7).

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| — | — | No new rules defined for this subsection |
