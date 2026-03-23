# Test Plan: 6.5 PIN Blocks (6.5.1–6.5.4)

**Specification Reference:** Section 6.5 of jz-hdl-specification.md

## 1. Objective

Verify IN_PINS, OUT_PINS, INOUT_PINS declarations with electrical standards, drive strength (OUT/INOUT), bus syntax, pin name uniqueness across all categories, differential signaling modes, and chip-specific standard validation.

## 2. Instrumentation Strategy

- **Span: `sem.pin_validate`** — attributes: `pin_name`, `direction`, `standard`, `drive`, `width`.
- **Event: `pin.duplicate`** — Pin name in multiple categories.

## 3. Test Scenarios

### 3.1 Happy Path
1. IN_PIN with LVCMOS33: `clk = { standard=LVCMOS33 };`
2. OUT_PIN with drive: `led = { standard=LVCMOS18, drive=8 };`
3. INOUT_PIN: `sda = { standard=LVCMOS33, drive=4 };`
4. Bus pins: `button[4] = { standard=LVCMOS33 };`
5. Differential: `lvds_in = { standard=LVDS25 };`

### 3.2 Boundary/Edge Cases
1. Width = 1 (scalar pin)
2. Large bus: `data[32] = { ... }`
3. All supported I/O standards tested

### 3.3 Negative Testing
1. Pin in both IN_PINS and OUT_PINS — Error
2. OUT_PIN missing drive — Error
3. IN_PIN with drive — Error (inputs are passive)
4. Unsupported standard for chip — Error
5. Duplicate pin name within block

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Pin in IN and OUT | Error | PIN_DUPLICATE_ACROSS | S6.5 |
| 2 | OUT without drive | Error | PIN_MISSING_DRIVE | S6.5.2 |
| 3 | Invalid standard | Error | PIN_STANDARD_INVALID | S6.5 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_project.c` | Pin validation | Integration test |
| `driver_project_hw.c` | Chip-specific standard validation | Mock chip_data |
| `parser_project_blocks.c` | Pin block parsing | Token stream |
| `chip_data.c` | Supported standards per chip | Mock |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| PIN_DUPLICATE_ACROSS | Pin in multiple categories | Neg 1 |
| PIN_MISSING_DRIVE | OUT/INOUT without drive | Neg 2 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| PIN_STANDARD_INVALID | S6.5 | Standard not supported by chip |
| PIN_IN_HAS_DRIVE | S6.5.1 "inputs are passive" | IN_PIN with drive property |
