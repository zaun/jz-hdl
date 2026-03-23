# Test Plan: 7.6 Complete Examples

**Specification Reference:** Section 7.6 (7.6.1-7.6.9) of jz-hdl-specification.md

## 1. Objective

Verify that all 9 canonical MEM examples from the specification compile without errors and produce valid IR. No new rules are introduced; all MEM rules from Sections 7.1-7.5 apply.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Simple ROM | Read-only MEM with ASYNC read port (S7.6.1) | Valid, compiles without errors |
| 2 | Dual-Port Register File | 1 read port, 1 write port (S7.6.2) | Valid, compiles without errors |
| 3 | Synchronous FIFO | FIFO with read/write ports (S7.6.3) | Valid, compiles without errors |
| 4 | Registered Read Cache | SYNC read port (S7.6.4) | Valid, compiles without errors |
| 5 | Triple-Port | 2 read ports, 1 write port (S7.6.5) | Valid, compiles without errors |
| 6 | Quad-Port | 2 read ports, 2 write ports (S7.6.6) | Valid, compiles without errors |
| 7 | Configurable Memory | CONST/CONFIG parameters for dimensions (S7.6.7) | Valid, compiles without errors |
| 8 | Single Port INOUT | Single INOUT port (S7.6.8) | Valid, compiles without errors |
| 9 | True Dual Port | 2 INOUT ports (S7.6.9) | Valid, compiles without errors |

### 2.2 Error Cases

No error cases specific to this section. Examples are canonical happy-path demonstrations. Error cases are covered by Sections 7.1-7.5 and 7.7.

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Configurable memory at minimum dimensions | CONFIG.WIDTH=1, CONFIG.DEPTH=1 | Valid, compiles without errors |
| 2 | True Dual Port simultaneous access pattern | Both INOUT ports active | Valid, no conflict with distinct port names |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1-9 | Canonical spec examples | No errors, valid IR | (none) | — | Examples exercise rules from S7.1-7.5 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| (none) | — | No validation tests specific to S7.6; examples are integration-level |

## 5. Rules Matrix

### 5.1 Rules Tested

No new rules introduced. This section exercises rules from Sections 7.1-7.5 in combination.

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| — | — | No new rules in this section |
