# Test Plan: 1.3 Bit Slicing and Indexing

**Specification Reference:** Section 1.3 of jz-hdl-specification.md

## 1. Objective

Verify that the compiler correctly parses and validates bit-slice syntax `signal[MSB:LSB]`, enforces MSB ≥ LSB, computes width as MSB − LSB + 1, validates index ranges against declared signal width, and supports CONST names as indices.

## 2. Instrumentation Strategy

- **Span: `parser.bit_slice`** — Trace parsing of slice expressions; attributes: `msb`, `lsb`, `signal_width`.
- **Span: `sem.slice_validate`** — Semantic validation of slice bounds; attributes: `msb_value`, `lsb_value`, `signal_declared_width`, `result_width`.
- **Event: `slice.out_of_range`** — Emitted when index ≥ signal width or < 0.
- **Event: `slice.msb_lt_lsb`** — Emitted when MSB < LSB.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Full-width slice | `bus[15:0]` on 16-bit wire | Width = 16, valid |
| 2 | Partial slice | `bus[7:4]` on 16-bit wire | Width = 4, valid |
| 3 | Single bit | `bus[0:0]` on 16-bit wire | Width = 1, valid |
| 4 | MSB equals LSB | `bus[5:5]` on 8-bit wire | Width = 1, valid |
| 5 | CONST in indices | `bus[H:L]` where H=15, L=8 | Width = 8, valid |
| 6 | Slice in ASYNCHRONOUS | `out = bus[7:0];` | Valid combinational slice |
| 7 | Slice in SYNCHRONOUS | `reg <= bus[3:0];` | Valid sequential slice |

### 3.2 Boundary/Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Index at max (width-1) | `bus[15:0]` on 16-bit | Valid (15 < 16) |
| 2 | Index equals width | `bus[16:0]` on 16-bit | Error: out of range |
| 3 | Both indices zero | `bus[0:0]` on 1-bit | Valid |
| 4 | 1-bit signal sliced | `sig[0:0]` on 1-bit wire | Valid |
| 5 | Large CONST indices | `bus[254:0]` on 255-bit wire | Valid |
| 6 | CONST evaluating to 0 | `bus[C:0]` where C=0 | Width = 1, valid |

### 3.3 Negative Testing

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | MSB < LSB | `bus[3:7]` | Error: MSB must be ≥ LSB |
| 2 | MSB ≥ signal width | `bus[23:4]` on 16-bit | Error: index out of range |
| 3 | LSB ≥ signal width | `bus[7:16]` on 16-bit | Error: index out of range |
| 4 | Both out of range | `bus[31:0]` on 16-bit | Error: index out of range |
| 5 | Negative index | `bus[-1:0]` | Error: nonnegative required |
| 6 | Undefined CONST | `bus[UNDEF:0]` | Error: undefined CONST |
| 7 | Slice on VCC/GND | `VCC[1:0]` | Error: semantic driver cannot be sliced |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `bus[7:0]` (16-bit) | Valid slice, width=8 | — | Happy path |
| 2 | `bus[16:0]` (16-bit) | Error: index 16 ≥ width 16 | SLICE_OUT_OF_RANGE | Index = width |
| 3 | `bus[3:7]` | Error: MSB < LSB | SLICE_MSB_LT_LSB | Reversed indices |
| 4 | `bus[31:0]` (16-bit) | Error: index 31 ≥ width 16 | SLICE_OUT_OF_RANGE | Far out of range |
| 5 | `VCC[1:0]` | Error: slicing semantic driver | SEMANTIC_DRIVER_SLICED | S2.4 |
| 6 | `bus[H:L]` (H=15,L=8) | Valid slice, width=8 | — | CONST indices |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_expressions.c` | Parses slice syntax | Feed token stream with slice expressions |
| `const_eval.c` | Evaluates CONST expressions in indices | Mock with known CONST values |
| `driver_width.c` | Width validation of slice result | Unit test with declared widths |
| `sem_type.c` | Type checking of slice operand | Verify signal type is sliceable |
| `diagnostic.c` | Error reporting | Capture diagnostics |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| SLICE_OUT_OF_RANGE | Index ≥ signal width or < 0 | Neg 2-5, Boundary 2 |
| SLICE_MSB_LT_LSB | MSB less than LSB | Neg 1 |
| SEMANTIC_DRIVER_SLICED | VCC/GND cannot be sliced | Neg 7 |
| CONST_UNDEFINED | Undefined CONST in slice index | Neg 6 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| SLICE_NEGATIVE_INDEX | S1.3 "nonnegative integers" | No explicit rule for negative indices; may be handled by parser as syntax error |
