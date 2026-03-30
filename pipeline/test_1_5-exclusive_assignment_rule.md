# Test Plan: 1.5 The Exclusive Assignment Rule

**Specification Reference:** Section 1.5 of jz-hdl-specification.md

## 1. Objective

Verify that the compiler enforces the Exclusive Assignment Rule: for any assignable identifier, every execution path through a block contains zero or one assignment to every bit. This covers the formal definition (1.5.1), Path-Exclusive Determinism analysis (1.5.2), register vs. combinational zero-assignment semantics (1.5.3), and the scope boundary with tri-state nets (1.5.4).

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Single assignment per path | `ASYNCHRONOUS { w <= a; }` -- one assignment to `w` |
| 2 | Branch-exclusive IF/ELSE | `IF (c) { w <= a; } ELSE { w <= b; }` -- mutually exclusive paths |
| 3 | Branch-exclusive SELECT | `SELECT (sel) { CASE 2'b00: w <= a; CASE 2'b01: w <= b; DEFAULT: w <= c; }` |
| 4 | Register zero-path (clock gating) | `SYNCHRONOUS { IF (en) { reg <= val; } }` -- zero-assignment path holds value |
| 5 | Different identifiers same path | `w1 <= a; w2 <= b;` -- different targets, no conflict |
| 6 | Bit-disjoint sliced assignments | `w[7:4] <= a; w[3:0] <= b;` -- non-overlapping bits |
| 7 | Nested IF with exclusive branches | `IF (a) { IF (b) { w <= x; } ELSE { w <= y; } } ELSE { w <= z; }` |

### 2.2 Error Cases

| # | Test Case | Description | Rule ID |
|---|-----------|-------------|---------|
| 1 | Double assignment same path | `w <= a; w <= b;` -- two assignments to same wire | ASSIGN_MULTIPLE_SAME_BITS |
| 2 | Independent chain conflict | `IF (a) { w <= x; } IF (b) { w <= y; }` -- two independent IFs | ASSIGN_INDEPENDENT_IF_SELECT |
| 3 | Sequential shadow | `w <= a; IF (c) { w <= b; }` -- outer assignment shadowed by inner | ASSIGN_SHADOWING |
| 4 | Overlapping bit slices | `w[7:4] <= a; w[5:2] <= b;` -- bits 4-5 overlap | ASSIGN_SLICE_OVERLAP |
| 5 | ASYNC partial coverage | `IF (c) { w <= a; }` -- wire undriven when `c` is false | ASYNC_UNDEFINED_PATH_NO_DRIVER |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Deeply nested branches (10 levels) | Verify PED analysis handles deep nesting |
| 2 | Many branches in SELECT | SELECT with 256 CASE arms, all assigning same wire |
| 3 | Single-bit wire | One bit assigned in IF/ELSE |
| 4 | Wide wire (256-bit) | Full-width assignment in exclusive branches |
| 5 | Combinational with z in some paths | `IF (en) { w <= data; } ELSE { w <= 8'bzzzz_zzzz; }` -- valid tri-state |
| 6 | All paths assign same value | `IF (c) { w <= GND; } ELSE { w <= GND; }` -- valid, not a conflict |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `w <= a; w <= b;` in ASYNC | Error: multi-driver | ASSIGN_MULTIPLE_SAME_BITS | error | S1.5/S5.2 Same path, same identifier |
| 2 | Two independent IFs assigning `w` | Error: independent chain | ASSIGN_INDEPENDENT_IF_SELECT | error | S1.5/S5.3/S5.4/S8.1 PED violation |
| 3 | `w <= a; IF(c) { w <= b; }` | Error: shadow | ASSIGN_SHADOWING | error | S1.5/S5.2/S8.1 Sequential shadowing |
| 4 | `IF(c) { w <= a; } ELSE { w <= b; }` | Valid | -- | -- | Branch-exclusive |
| 5 | `IF(c) { w <= a; }` in ASYNC | Error: partial coverage | ASYNC_UNDEFINED_PATH_NO_DRIVER | error | S1.5/S4.10/S5.1 Undriven in else path |
| 6 | `IF(en) { reg <= val; }` in SYNC | Valid | -- | -- | Clock gating (register holds) |
| 7 | `w[7:4] <= a; w[5:2] <= b;` | Error: overlapping bits | ASSIGN_SLICE_OVERLAP | error | S5.2/S8.1 Overlapping slices |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 1_5_EXCLUSIVE_ASSIGNMENT-valid_assignments_ok.jz | -- | Happy path: valid exclusive assignment patterns accepted |
| 1_5_ASSIGN_MULTIPLE_SAME_BITS-double_assign_same_path.jz | ASSIGN_MULTIPLE_SAME_BITS | Same bits assigned more than once on a single execution path |
| 1_5_ASSIGN_INDEPENDENT_IF_SELECT-independent_chains.jz | ASSIGN_INDEPENDENT_IF_SELECT | Same identifier assigned in multiple independent IF/SELECT chains at same nesting level |
| 1_5_ASSIGN_SHADOWING-sequential_shadow.jz | ASSIGN_SHADOWING | Assignment at higher nesting level followed by nested assignment to same bits |
| 1_5_ASSIGN_SLICE_OVERLAP-overlapping_slices.jz | ASSIGN_SLICE_OVERLAP | Overlapping part-select assignments to same identifier bits in any single execution path |
| 1_5_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage.jz | ASYNC_UNDEFINED_PATH_NO_DRIVER | Signal undriven on some ASYNCHRONOUS paths; add an ELSE branch or DEFAULT case |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| ASSIGN_MULTIPLE_SAME_BITS | error | S1.5/S5.2 Same bits assigned more than once on a single execution path | 1_5_ASSIGN_MULTIPLE_SAME_BITS-double_assign_same_path.jz |
| ASSIGN_INDEPENDENT_IF_SELECT | error | S1.5/S5.3/S5.4/S8.1 Same identifier assigned in multiple independent IF/SELECT chains | 1_5_ASSIGN_INDEPENDENT_IF_SELECT-independent_chains.jz |
| ASSIGN_SHADOWING | error | S1.5/S5.2/S8.1 Assignment at higher nesting level followed by nested assignment to same bits | 1_5_ASSIGN_SHADOWING-sequential_shadow.jz |
| ASSIGN_SLICE_OVERLAP | error | S5.2/S8.1 Overlapping part-select assignments to same identifier bits | 1_5_ASSIGN_SLICE_OVERLAP-overlapping_slices.jz |
| ASYNC_UNDEFINED_PATH_NO_DRIVER | error | S1.5/S4.10/S5.1 Signal undriven on some ASYNCHRONOUS paths | 1_5_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage.jz |

### 5.2 Rules Not Tested


All rules for this section are tested.
