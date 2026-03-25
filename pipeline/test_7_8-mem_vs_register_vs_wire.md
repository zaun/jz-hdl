# Test Plan: 7.8 MEM vs REGISTER vs WIRE

**Specification Reference:** Section 7.8 of jz-hdl-specification.md

## 1. Objective

Verify the comparison/documentation section describing behavioral differences between MEM, REGISTER, and WIRE. This section serves as a reference for storage type selection. No new rules are introduced; the test confirms that all three storage types can coexist in a single module with correct semantics.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | MEM for indexed storage | MEM with address-based read/write | Valid, compiles without errors |
| 2 | REGISTER for single flip-flop | Single-bit or multi-bit register | Valid, compiles without errors |
| 3 | WIRE for combinational logic | Wire used as intermediate signal | Valid, compiles without errors |
| 4 | All three in one module | MEM + REGISTER + WIRE coexisting | Valid, compiles without errors |
| 5 | DISTRIBUTED MEM as register file | Small MEM (depth <= 16) with LUT-based storage | Valid, compiles without errors |

### 2.2 Error Cases

No error cases specific to this section. Misuse of storage types is caught by rules in Sections 4.5 (WIRE), 4.7 (REGISTER), and 7.1 (MEM declaration).

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Single-element MEM vs REGISTER | MEM with depth=1 vs equivalent REGISTER | Both valid, different semantics (MEM requires port syntax) |
| 2 | WIRE vs ASYNC MEM read | Combinational read from MEM vs wire assignment | Both valid, MEM requires port syntax |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| — | — | — | — | — |

No new rules; comparison section only.

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_8_HAPPY_PATH-mem_vs_register_vs_wire.jz | — | Happy path: demonstrates MEM, REGISTER, and WIRE usage side-by-side |

## 5. Rules Matrix

### 5.1 Rules Tested

No new rules introduced. This is a happy-path-only plan. Cross-reference Sections 4.5, 4.7, and 7.1 for relevant rules.

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| — | — | No new rules in this section |
