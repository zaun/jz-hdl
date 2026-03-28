# Test Plan: 11.3 Tri-State Net Identification

**Specification Reference:** Section 11.3 of jz-hdl-specification.md

## 1. Objective

Verify the compiler correctly identifies internal tri-state nets (nets with z assignments from multiple drivers) and detects error conditions: multiple active (non-z) drivers on a signal, and all drivers assigning z while the signal is read.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Tri-state net identified | Net with conditional z assignment from multiple drivers | Correctly identified as tri-state candidate |
| 2 | Non-tri-state net | Net with single driver, no z | Not flagged as tri-state |
| 3 | Valid tri-state pattern | Multiple drivers where all but one assign z | Valid tri-state net |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Multiple active drivers | Two drivers both assign non-z values to same signal | Error: NET_MULTIPLE_ACTIVE_DRIVERS |
| 2 | All drivers z but signal read | Every driver assigns z, signal is read | Error: NET_TRI_STATE_ALL_Z_READ |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Net with z in only one path | Single conditional z among multiple paths | Identified correctly based on analysis |
| 2 | Net with z in all conditional branches | z assigned in every branch of IF/ELSE | Potential all-z condition |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Multiple active (non-z) drivers on same signal | Error | NET_MULTIPLE_ACTIVE_DRIVERS | error | S1.2/S1.5/S4.10 |
| 2 | All drivers assign z but signal is read | Error | NET_TRI_STATE_ALL_Z_READ | error | S4.10 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `11_3_TRISTATE_NET_IDENTIFICATION-valid_tristate_ok.jz` | — | Happy-path: valid tri-state net identified correctly |
| `11_3_NET_MULTIPLE_ACTIVE_DRIVERS-multi_driver_non_tristate.jz` | NET_MULTIPLE_ACTIVE_DRIVERS | Multiple active drivers on non-tristate net |
| `11_3_NET_TRI_STATE_ALL_Z_READ-all_drivers_z_but_read.jz` | NET_TRI_STATE_ALL_Z_READ | All drivers assign z but signal is read |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) | Also Tested In |
|---------|----------|-------------|--------------|----------------|
| NET_MULTIPLE_ACTIVE_DRIVERS | error | Multiple active drivers on same signal; for tri-state, all but one must assign z | `11_3_NET_MULTIPLE_ACTIVE_DRIVERS-multi_driver_non_tristate.jz` | test_1_6-high_impedance_and_tristate.md |
### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| NET_TRI_STATE_ALL_Z_READ | error | Suppressed by ASYNC_FLOATING_Z_READ: test exists (`11_3_NET_TRI_STATE_ALL_Z_READ-all_drivers_z_but_read.jz`) but rule is suppressed |
