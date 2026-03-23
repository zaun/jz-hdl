# Test Plan: 7.8 MEM vs REGISTER vs WIRE Comparison

**Specification Reference:** Section 7.8 of jz-hdl-specification.md

## 1. Objective

Verify the behavioral differences between MEM, REGISTER, and WIRE: storage semantics, access patterns, clock requirements, and appropriate usage for each type.

## 2. Instrumentation Strategy

- **Span: `sem.storage_type`** — Classify signal usage.

## 3. Test Scenarios

### 3.1 Happy Path
1. MEM for indexed storage
2. REGISTER for single flip-flop
3. WIRE for combinational intermediate
4. DISTRIBUTED MEM as register file equivalent

### 3.2 Negative Testing
1. MEM access without address — Error
2. REGISTER with address syntax — Error
3. WIRE with storage semantics (hold) — Error

## 4-6. Cross-reference with Section 4.5, 4.7, 7.1 test plans.
