# Test Plan: 6.6 MAP Block (including 6.6.1 Differential)

**Specification Reference:** Section 6.6 of jz-hdl-specification.md

## 1. Objective

Verify MAP block: pin-to-board-pin mapping, per-bit mapping for buses, chip-specific pin validation, differential pin mapping, and fixed pin handling.

## 2. Instrumentation Strategy

- **Span: `sem.map_validate`** — attributes: `pin_name`, `board_pin`, `is_differential`.

## 3. Test Scenarios

### 3.1 Happy Path
1. Scalar pin mapping: `clk = 52;`
2. Bus bit mapping: `led[0] = 10; led[1] = 11;`
3. Differential pin mapping (positive pin only, negative auto-resolved)

### 3.2 Boundary/Edge Cases
1. All pins mapped — complete design
2. Optional pin unmapped (no-connect)

### 3.3 Negative Testing
1. Unknown board pin ID — Error
2. Pin mapped twice — Error
3. Board pin used twice — Error
4. Unmapped required pin — Error
5. Differential pair: positive pin without valid negative — Error

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Unknown board pin | Error | MAP_PIN_INVALID | S6.6 |
| 2 | Duplicate mapping | Error | MAP_DUPLICATE | S6.6 |
| 3 | Board pin reused | Error | MAP_BOARD_PIN_CONFLICT | S6.6 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_project_hw.c` | Pin validation against chip data | Mock chip_data |
| `chip_data.c` | Board pin definitions | Mock |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| MAP_PIN_INVALID | Unknown board pin | Neg 1 |
| MAP_DUPLICATE | Pin mapped twice | Neg 2 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| MAP_REQUIRED_UNMAPPED | S6.6 | Required pin not in MAP block |
| MAP_DIFF_NO_PAIR | S6.6.1 | Differential pin without valid pair |
