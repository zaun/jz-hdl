# Test Plan: 4.6 MUX (Signal Aggregation and Slicing)

**Specification Reference:** Section 4.6 of jz-hdl-specification.md

## 1. Objective

Verify MUX aggregation/slicing, read-only semantics, dynamic indexing. Confirm that assigning to a MUX is rejected, aggregation sources must have equal widths, aggregation sources must be valid readable signals, auto-slicing requires the wide source width to be an exact multiple of element width, static out-of-range indices are caught at compile time, and duplicate MUX names are rejected.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Aggregation form | `mux = byte_a, byte_b;` -- two 8-bit sources |
| 2 | Auto-slicing form | `mux [8] = flat_data;` on 32-bit signal -- 4 elements |
| 3 | Dynamic selection | `out = mux[sel];` in ASYNCHRONOUS |
| 4 | Constant index | `out = mux[2'd1];` -- valid static index |
| 5 | MUX in SYNCHRONOUS | `reg <= mux[sel];` -- read in SYNC block |
| 6 | Many sources | Aggregation with 8+ sources |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Assign to MUX | `mux[0] = data;` -- error: MUX is read-only |
| 2 | Source width mismatch | `mux = byte_a, nibble;` (8-bit vs 4-bit) -- error |
| 3 | Invalid aggregation source | Source is not a valid readable signal in module scope -- error |
| 4 | Non-divisible auto-slice | `mux [3] = 8-bit_signal;` (8 % 3 != 0) -- error |
| 5 | Static out-of-range index | `mux[4]` on 4-element MUX (valid 0-3) -- error |
| 6 | Duplicate MUX name | MUX name conflicts with another identifier -- error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Two elements | Minimum aggregation (2 sources) |
| 2 | Large aggregation | 256 elements |
| 3 | Selector width exact | clog2(N) bits for N elements |
| 4 | Runtime out-of-range | Index > N-1 at runtime -- result is zero |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|-----------------|---------|----------|-------|
| 1 | `mux[0] = data;` | Error: assigning to MUX is forbidden | MUX_ASSIGN_LHS | error | S4.6 read-only |
| 2 | Sources with different widths | Error: sources must have identical bit-width | MUX_AGG_SOURCE_WIDTH_MISMATCH | error | S4.6 aggregation |
| 3 | Invalid source in aggregation | Error: source not a valid readable signal | MUX_AGG_SOURCE_INVALID | error | S4.6 aggregation |
| 4 | Wide source not evenly divisible | Error: width must be exact multiple of element width | MUX_SLICE_WIDTH_NOT_DIVISOR | error | S4.6 auto-slicing |
| 5 | Static index outside valid range | Error: selector outside valid index range | MUX_SELECTOR_OUT_OF_RANGE_CONST | error | S4.6 compile-time |
| 6 | MUX name duplicates another identifier | Error: duplicate identifier | MUX_NAME_DUPLICATE | error | S4.6 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_6_MUX_AGG_SOURCE_INVALID-invalid_source.jz | MUX_AGG_SOURCE_INVALID | Aggregation source not a valid readable signal in module scope |
| 4_6_MUX_AGG_SOURCE_WIDTH_MISMATCH-source_width_mismatch.jz | MUX_AGG_SOURCE_WIDTH_MISMATCH | Aggregation form sources must all have identical bit-width |
| 4_6_MUX_ASSIGN_LHS-assign_to_mux.jz | MUX_ASSIGN_LHS | Assigning to MUX id or its indexed form on LHS is forbidden |
| 4_6_MUX_NAME_DUPLICATE-duplicate_name.jz | MUX_NAME_DUPLICATE | MUX identifier duplicates another identifier in module |
| 4_6_MUX_SELECTOR_OUT_OF_RANGE_CONST-static_index_oob.jz | MUX_SELECTOR_OUT_OF_RANGE_CONST | Selector statically provable outside valid index range |
| 4_6_MUX_SLICE_WIDTH_NOT_DIVISOR-non_divisible_width.jz | MUX_SLICE_WIDTH_NOT_DIVISOR | Auto-slicing form requires wide source width to be exact multiple of element width |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MUX_ASSIGN_LHS | error | S4.6 Assigning to MUX id or its indexed form on LHS is forbidden (MUX is read-only) | 4_6_MUX_ASSIGN_LHS-assign_to_mux.jz |
| MUX_AGG_SOURCE_WIDTH_MISMATCH | error | S4.6 Aggregation form sources must all have identical bit-width | 4_6_MUX_AGG_SOURCE_WIDTH_MISMATCH-source_width_mismatch.jz |
| MUX_AGG_SOURCE_INVALID | error | S4.6 Aggregation source not a valid readable signal in module scope | 4_6_MUX_AGG_SOURCE_INVALID-invalid_source.jz |
| MUX_SLICE_WIDTH_NOT_DIVISOR | error | S4.6 Auto-slicing form requires wide_source width to be exact multiple of element_width | 4_6_MUX_SLICE_WIDTH_NOT_DIVISOR-non_divisible_width.jz |
| MUX_SELECTOR_OUT_OF_RANGE_CONST | error | S4.6 Selector statically provable outside valid index range | 4_6_MUX_SELECTOR_OUT_OF_RANGE_CONST-static_index_oob.jz |
| MUX_NAME_DUPLICATE | error | S4.6 MUX identifier duplicates another identifier in module | 4_6_MUX_NAME_DUPLICATE-duplicate_name.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All rules for this section are covered by existing tests |
