# Test Plan: 1.3 Bit Slicing and Indexing

**Specification Reference:** Section 1.3 of jz-hdl-specification.md

## 1. Objective

Verify that the compiler correctly parses and validates bit-slice syntax `signal[MSB:LSB]`, enforces MSB >= LSB, computes width as MSB - LSB + 1, validates index ranges against declared signal width, and supports CONST names as indices.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Full-width slice | `bus[15:0]` on 16-bit wire | Width = 16, valid |
| 2 | Partial slice | `bus[7:4]` on 16-bit wire | Width = 4, valid |
| 3 | Single bit | `bus[0:0]` on 16-bit wire | Width = 1, valid |
| 4 | MSB equals LSB | `bus[5:5]` on 8-bit wire | Width = 1, valid |
| 5 | CONST in indices | `bus[H:L]` where H=15, L=8 | Width = 8, valid |
| 6 | Slice in ASYNCHRONOUS | `out = bus[7:0];` | Valid combinational slice |
| 7 | Slice in SYNCHRONOUS | `reg <= bus[3:0];` | Valid sequential slice |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | MSB < LSB | `bus[3:7]` | Error: MSB must be >= LSB | SLICE_MSB_LESS_THAN_LSB |
| 2 | MSB >= signal width | `bus[23:4]` on 16-bit | Error: index out of range | SLICE_INDEX_OUT_OF_RANGE |
| 3 | LSB >= signal width | `bus[7:16]` on 16-bit | Error: index out of range | SLICE_INDEX_OUT_OF_RANGE |
| 4 | Both out of range | `bus[31:0]` on 16-bit | Error: index out of range | SLICE_INDEX_OUT_OF_RANGE |
| 5 | Undefined CONST | `bus[UNDEF:0]` | Error: undefined CONST | CONST_UNDEFINED_IN_WIDTH_OR_SLICE |
| 6 | Slice on VCC/GND | `VCC[1:0]` | Error: semantic driver cannot be sliced | SPECIAL_DRIVER_SLICED |
| 7 | Non-integer/non-CONST index | `bus[expr:0]` where expr is not an integer or CONST | Error: invalid slice index | SLICE_INDEX_INVALID |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Index at max (width-1) | `bus[15:0]` on 16-bit | Valid (15 < 16) |
| 2 | Index equals width | `bus[16:0]` on 16-bit | Error: out of range |
| 3 | Both indices zero | `bus[0:0]` on 1-bit | Valid |
| 4 | 1-bit signal sliced | `sig[0:0]` on 1-bit wire | Valid |
| 5 | Large CONST indices | `bus[254:0]` on 255-bit wire | Valid |
| 6 | CONST evaluating to 0 | `bus[C:0]` where C=0 | Width = 1, valid |
| 7 | Negative index | `bus[-1:0]` | Error: nonnegative required |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `bus[7:0]` (16-bit) | Valid slice, width=8 | -- | -- | Happy path |
| 2 | `bus[16:0]` (16-bit) | Error: index 16 >= width 16 | SLICE_INDEX_OUT_OF_RANGE | error | S1.3/S8.1 |
| 3 | `bus[3:7]` | Error: MSB < LSB | SLICE_MSB_LESS_THAN_LSB | error | S1.3/S8.1 |
| 4 | `bus[31:0]` (16-bit) | Error: index 31 >= width 16 | SLICE_INDEX_OUT_OF_RANGE | error | S1.3/S8.1 |
| 5 | `VCC[1:0]` | Error: slicing semantic driver | SPECIAL_DRIVER_SLICED | error | S2.3 |
| 6 | `bus[H:L]` (H=15,L=8) | Valid slice, width=8 | -- | -- | CONST indices |
| 7 | `bus[UNDEF:0]` | Error: undefined CONST | CONST_UNDEFINED_IN_WIDTH_OR_SLICE | error | S1.3/S2.1/S7.10 |
| 8 | `bus[expr:0]` (non-integer index) | Error: invalid index | SLICE_INDEX_INVALID | error | S1.3/S8.1 Non-const expression in slice |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 1_3_SLICE_HAPPY_PATH-valid_slicing_ok.jz | -- | Happy path: valid bit-slice patterns accepted |
| 1_3_SLICE_INDEX_OUT_OF_RANGE-index_exceeds_width.jz | SLICE_INDEX_OUT_OF_RANGE | Slice indices are < 0 or >= signal width |
| 1_3_SLICE_MSB_LESS_THAN_LSB-reversed_indices.jz | SLICE_MSB_LESS_THAN_LSB | Slice uses MSB < LSB |
| 1_3_SPECIAL_DRIVER_SLICED-vcc_gnd_sliced.jz | SPECIAL_DRIVER_SLICED | GND/VCC may not be sliced or indexed |
| 1_3_CONST_UNDEFINED_IN_WIDTH_OR_SLICE-non_const_in_slice.jz | CONST_UNDEFINED_IN_WIDTH_OR_SLICE | CONST used in width/slice not declared or evaluates invalidly |
| *(planned)* 1_3_SLICE_INDEX_INVALID-non_integer_index.jz | SLICE_INDEX_INVALID | Slice index is not an integer/CONST or CONST is undefined/negative |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| SLICE_MSB_LESS_THAN_LSB | error | S1.3/S8.1 Slice uses MSB < LSB | 1_3_SLICE_MSB_LESS_THAN_LSB-reversed_indices.jz |
| SLICE_INDEX_OUT_OF_RANGE | error | S1.3/S8.1 Slice indices are < 0 or >= signal width | 1_3_SLICE_INDEX_OUT_OF_RANGE-index_exceeds_width.jz |
| CONST_UNDEFINED_IN_WIDTH_OR_SLICE | error | S1.3/S2.1/S7.10 CONST used in width/slice not declared or evaluates invalidly | 1_3_CONST_UNDEFINED_IN_WIDTH_OR_SLICE-non_const_in_slice.jz |
| SPECIAL_DRIVER_SLICED | error | S2.3 GND/VCC may not be sliced or indexed | 1_3_SPECIAL_DRIVER_SLICED-vcc_gnd_sliced.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| SLICE_INDEX_INVALID | error | Validation test file 1_3_SLICE_INDEX_INVALID-non_integer_index.jz does not yet exist |
