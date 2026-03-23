# Test Plan: 6.6 MAP Block

**Specification Reference:** Section 6.6 of jz-hdl-specification.md

## 1. Objective

Verify MAP block: pin-to-board-pin mapping, per-bit mapping for buses, differential pin mapping (P/N pairs), completeness checks, and physical location uniqueness.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Scalar pin mapping | `clk = 52;` -- single pin to board location |
| 2 | Bus bit mapping | `led[0] = 10; led[1] = 11;` -- per-bit assignment |
| 3 | Differential pin mapping | `{ P=10, N=11 }` -- positive/negative pair |
| 4 | All pins mapped | Complete design with every pin mapped |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Declared pin not mapped | Pin in PIN blocks but missing from MAP |
| 2 | Mapped pin not declared | MAP entry references undeclared pin name |
| 3 | Duplicate physical location | Two logical pins mapped to same board pin |
| 4 | Invalid board pin ID | Board pin format invalid for target device |
| 5 | Differential pin with scalar map | Differential pin must use { P, N } syntax |
| 6 | Single-ended pin with pair map | Single-ended pin must not use { P, N } syntax |
| 7 | Differential missing P or N | Differential MAP entry missing one identifier |
| 8 | Differential same pin for P and N | P and N resolve to same physical pin |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Large bus fully mapped | 32-bit bus with all bits individually mapped |
| 2 | Optional pin unmapped | No-connect pin not in MAP -- valid if allowed |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|-----------------|---------|----------|-------|
| 1 | Declared pin not mapped | Error | MAP_PIN_DECLARED_NOT_MAPPED | error | S6.6/S6.10 |
| 2 | Mapped pin not declared | Error | MAP_PIN_MAPPED_NOT_DECLARED | error | S6.6 |
| 3 | Duplicate board pin | Warning | MAP_DUP_PHYSICAL_LOCATION | warning | S6.6 |
| 4 | Invalid board pin ID | Error | MAP_INVALID_BOARD_PIN_ID | error | S6.6 |
| 5 | Diff pin with scalar map | Error | MAP_DIFF_EXPECTED_PAIR | error | S6.6 |
| 6 | Single pin with pair map | Error | MAP_SINGLE_UNEXPECTED_PAIR | error | S6.6 |
| 7 | Diff missing P or N | Error | MAP_DIFF_MISSING_PN | error | S6.6 |
| 8 | Diff same pin for P and N | Error | MAP_DIFF_SAME_PIN | error | S6.6 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_6_MAP_DIFF_EXPECTED_PAIR-diff_scalar_map.jz | MAP_DIFF_EXPECTED_PAIR | Differential pin must use { P, N } MAP syntax |
| 6_6_MAP_DIFF_MISSING_PN-missing_pn.jz | MAP_DIFF_MISSING_PN | Differential MAP entry missing P or N identifier |
| 6_6_MAP_DIFF_SAME_PIN-same_pn.jz | MAP_DIFF_SAME_PIN | Differential MAP entry has same physical pin for P and N |
| 6_6_MAP_DUP_PHYSICAL_LOCATION-duplicate_board_pin.jz | MAP_DUP_PHYSICAL_LOCATION | Two logical pins mapped to same physical board pin |
| 6_6_MAP_PIN_DECLARED_NOT_MAPPED-unmapped_pins.jz | MAP_PIN_DECLARED_NOT_MAPPED | Pin declared in PIN blocks but not mapped in MAP |
| 6_6_MAP_PIN_MAPPED_NOT_DECLARED-undeclared_map_entry.jz | MAP_PIN_MAPPED_NOT_DECLARED | MAP entry references undeclared pin |
| 6_6_MAP_SINGLE_UNEXPECTED_PAIR-single_pair_map.jz | MAP_SINGLE_UNEXPECTED_PAIR | Single-ended pin must not use { P, N } MAP syntax |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MAP_PIN_DECLARED_NOT_MAPPED | error | S6.6/S6.10/S6.9 Pin declared in PIN blocks but not mapped in MAP | 6_6_MAP_PIN_DECLARED_NOT_MAPPED-unmapped_pins.jz |
| MAP_PIN_MAPPED_NOT_DECLARED | error | S6.6/S6.9 MAP entry references undeclared pin | 6_6_MAP_PIN_MAPPED_NOT_DECLARED-undeclared_map_entry.jz |
| MAP_DUP_PHYSICAL_LOCATION | warning | S6.6/S6.9 Two logical pins mapped to same physical board pin | 6_6_MAP_DUP_PHYSICAL_LOCATION-duplicate_board_pin.jz |
| MAP_INVALID_BOARD_PIN_ID | error | S6.6/S6.9 Board pin ID format invalid for target device | -- (no dedicated test file) |
| MAP_DIFF_EXPECTED_PAIR | error | S6.6/S6.9 Differential pin must use { P, N } MAP syntax | 6_6_MAP_DIFF_EXPECTED_PAIR-diff_scalar_map.jz |
| MAP_SINGLE_UNEXPECTED_PAIR | error | S6.6/S6.9 Single-ended pin must not use { P, N } MAP syntax | 6_6_MAP_SINGLE_UNEXPECTED_PAIR-single_pair_map.jz |
| MAP_DIFF_MISSING_PN | error | S6.6/S6.9 Differential MAP entry missing P or N identifier | 6_6_MAP_DIFF_MISSING_PN-missing_pn.jz |
| MAP_DIFF_SAME_PIN | error | S6.6/S6.9 Differential MAP entry has same physical pin for P and N | 6_6_MAP_DIFF_SAME_PIN-same_pn.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| MAP_INVALID_BOARD_PIN_ID | error | No dedicated validation test file exists |
