# Test Plan: 2.4 Special Semantic Drivers

**Spec Ref:** Section 2.4 of jz-hdl-specification.md

## 1. Objective

Verify that `GND` (Logic 0) and `VCC` (Logic 1) semantic drivers correctly expand to match target width (polymorphic expansion), are valid only as standalone assignment tokens (atomic assignment), and are prohibited in expressions, concatenations, slicing, and index expressions.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | GND as register reset | `REGISTER { data [8] = GND; }` -- expands to `8'b0000_0000` |
| 2 | VCC as register reset | `REGISTER { data [8] = VCC; }` -- expands to `8'b1111_1111` |
| 3 | GND as wire assignment | `enable <= GND;` -- expands to match wire width |
| 4 | VCC as wire assignment | `enable <= VCC;` -- expands to match wire width |
| 5 | GND as OUT port driver | `signal <= GND;` -- tie-off |
| 6 | VCC to 1-bit signal | `flag <= VCC;` -- expands to `1'b1` |
| 7 | GND to wide signal | `bus [32] <= GND;` -- expands to 32 zeros |
| 8 | GND in IF/ELSE branch | `IF (en) { w <= data; } ELSE { w <= GND; }` -- valid |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | GND in expression | `a + GND` -- Error: SPECIAL_DRIVER_IN_EXPRESSION |
| 2 | VCC in expression | `VCC + 1'b1` -- Error: SPECIAL_DRIVER_IN_EXPRESSION |
| 3 | GND in concatenation | `{ 1'b1, GND }` -- Error: SPECIAL_DRIVER_IN_CONCAT |
| 4 | VCC sliced | `VCC[1:0]` -- Error: SPECIAL_DRIVER_SLICED |
| 5 | GND in index | `bus[GND]` -- Error: SPECIAL_DRIVER_IN_INDEX |
| 6 | VCC in ternary | `w <= cond ? VCC : data;` -- Error: SPECIAL_DRIVER_IN_EXPRESSION (ternary is expression context) |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit GND | Target is 1-bit -> GND = `1'b0` |
| 2 | 1-bit VCC | Target is 1-bit -> VCC = `1'b1` |
| 3 | 256-bit GND | Target is 256-bit -> all zeros |
| 4 | GND/VCC in exclusive paths | `IF(c) { w <= GND; } ELSE { w <= VCC; }` -- valid, exclusive |
| 5 | GND as latch value | `latch <= GND;` -- valid for latch |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `a + GND` | Error | SPECIAL_DRIVER_IN_EXPRESSION | error | S2.4 expression prohibition |
| 2 | `{ 1'b1, GND }` | Error | SPECIAL_DRIVER_IN_CONCAT | error | S2.4 concatenation prohibition |
| 3 | `VCC[1:0]` | Error | SPECIAL_DRIVER_SLICED | error | S2.4 slicing prohibition |
| 4 | `bus[GND]` | Error | SPECIAL_DRIVER_IN_INDEX | error | S2.4 index prohibition |
| 5 | `REGISTER { r [8] = GND; }` | Valid: reset = 0 | -- | -- | Polymorphic expansion |
| 6 | `w <= VCC;` (8-bit w) | Valid: w = 8'hFF | -- | -- | Polymorphic expansion |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 2_4_SPECIAL_DRIVER_HAPPY_PATH-valid_gnd_vcc_ok.jz | -- | Happy path: valid GND/VCC usage patterns accepted |
| 2_4_SPECIAL_DRIVER_IN_CONCAT-gnd_vcc_in_concat.jz | SPECIAL_DRIVER_IN_CONCAT | GND/VCC used in concatenation |
| 2_4_SPECIAL_DRIVER_IN_EXPRESSION-gnd_vcc_in_expr.jz | SPECIAL_DRIVER_IN_EXPRESSION | GND/VCC used in arithmetic/logical expression |
| 2_4_SPECIAL_DRIVER_IN_INDEX-gnd_vcc_in_index.jz | SPECIAL_DRIVER_IN_INDEX | GND/VCC used as an index expression |
| 2_4_SPECIAL_DRIVER_SLICED-gnd_vcc_sliced.jz | SPECIAL_DRIVER_SLICED | GND/VCC subjected to bit-slice or index access |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| SPECIAL_DRIVER_IN_EXPRESSION | error | S2.3 GND/VCC may not appear in arithmetic/logical expressions | 2_4_SPECIAL_DRIVER_IN_EXPRESSION-gnd_vcc_in_expr.jz |
| SPECIAL_DRIVER_IN_CONCAT | error | S2.3 GND/VCC may not appear in concatenations | 2_4_SPECIAL_DRIVER_IN_CONCAT-gnd_vcc_in_concat.jz |
| SPECIAL_DRIVER_IN_INDEX | error | S2.3 GND/VCC may not be used as an index expression | 2_4_SPECIAL_DRIVER_IN_INDEX-gnd_vcc_in_index.jz |
| SPECIAL_DRIVER_SLICED | error | S2.3 GND/VCC may not be sliced or indexed | 2_4_SPECIAL_DRIVER_SLICED-gnd_vcc_sliced.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| (none) | -- | All rules assigned to this section have validation tests |
