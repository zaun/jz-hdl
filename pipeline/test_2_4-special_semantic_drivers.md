# Test Plan: 2.4 Special Semantic Drivers

**Specification Reference:** Section 2.4 of jz-hdl-specification.md

## 1. Objective

Verify that `GND` (Logic 0) and `VCC` (Logic 1) semantic drivers correctly expand to match target width (polymorphic expansion), are valid only as standalone assignment tokens (atomic assignment), are prohibited in expressions/concatenations/slicing, are permitted as register reset values, and participate in the Exclusive Assignment Rule.

## 2. Instrumentation Strategy

- **Span: `sem.semantic_driver`** — Trace semantic driver resolution; attributes: `driver` (GND/VCC), `target_width`, `expanded_value`.
- **Event: `semantic_driver.in_expr`** — GND/VCC used in expression (error).
- **Event: `semantic_driver.in_concat`** — GND/VCC used in concatenation (error).
- **Event: `semantic_driver.sliced`** — GND/VCC sliced or indexed (error).
- **Coverage Hook:** Ensure both GND and VCC are tested in all valid and invalid contexts.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | GND as register reset | `REGISTER { data [8] = GND; }` — expands to `8'b0000_0000` |
| 2 | VCC as register reset | `REGISTER { data [8] = VCC; }` — expands to `8'b1111_1111` |
| 3 | GND as wire assignment | `enable <= GND;` — expands to match wire width |
| 4 | VCC as wire assignment | `enable <= VCC;` — expands to match wire width |
| 5 | GND as OUT port driver | `signal <= GND;` — tie-off |
| 6 | VCC to 1-bit signal | `flag <= VCC;` — expands to `1'b1` |
| 7 | GND to wide signal | `bus [32] <= GND;` — expands to 32 zeros |
| 8 | GND in IF/ELSE branch | `IF (en) { w <= data; } ELSE { w <= GND; }` — valid |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit GND | Target is 1-bit → GND = `1'b0` |
| 2 | 1-bit VCC | Target is 1-bit → VCC = `1'b1` |
| 3 | 256-bit GND | Target is 256-bit → all zeros |
| 4 | GND/VCC in exclusive paths | `IF(c) { w <= GND; } ELSE { w <= VCC; }` — valid, exclusive |
| 5 | GND as latch value | `latch <= GND;` — valid for latch |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | GND in expression | `a + GND` — Error: may not appear in expressions |
| 2 | VCC in expression | `VCC + 1'b1` — Error |
| 3 | GND in concatenation | `{ 1'b1, GND }` — Error: may not appear in concatenations |
| 4 | VCC sliced | `VCC[1:0]` — Error: may not be sliced |
| 5 | GND in index | `bus[GND]` — Error: may not appear in slice index |
| 6 | GND + VCC same path | `w <= GND; w <= VCC;` — Error: exclusive assignment violation |
| 7 | GND and other driver | `w <= GND; w <= data;` — Error: multi-driver |
| 8 | VCC in ternary | `w <= cond ? VCC : data;` — Error: in expression |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `a + GND` | Error | SEMANTIC_DRIVER_IN_EXPR | S2.4 expression proscription |
| 2 | `{ 1'b1, GND }` | Error | SEMANTIC_DRIVER_IN_CONCAT | S2.4 concatenation |
| 3 | `VCC[1:0]` | Error | SEMANTIC_DRIVER_SLICED | S2.4 slicing |
| 4 | `bus[GND]` | Error | SEMANTIC_DRIVER_IN_SLICE_INDEX | S2.4 index |
| 5 | `REGISTER { r [8] = GND; }` | Valid: reset = 0 | — | Polymorphic expansion |
| 6 | `w <= VCC;` (8-bit w) | Valid: w = 8'hFF | — | Polymorphic expansion |
| 7 | `w <= GND; w <= VCC;` | Error: multi-driver | ASSIGN_MULTI_DRIVER | Exclusive assignment |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_literal.c` | Semantic driver detection and validation | Feed AST with GND/VCC nodes |
| `driver_expr.c` | Expression analysis (reject GND/VCC in expr) | Provide expression trees containing GND/VCC |
| `driver_assign.c` | Exclusive assignment with GND/VCC | Verify multi-driver detection |
| `driver_width.c` | Width expansion of GND/VCC | Verify expanded width matches target |
| `sem_type.c` | Type resolution for GND/VCC | Verify polymorphic type |
| `ir_build_expr.c` | IR generation for GND/VCC | Verify constant folding |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| SEMANTIC_DRIVER_IN_EXPR | GND/VCC in expression | Neg 1, 2, 8 |
| SEMANTIC_DRIVER_IN_CONCAT | GND/VCC in concatenation | Neg 3 |
| SEMANTIC_DRIVER_SLICED | GND/VCC sliced or indexed | Neg 4 |
| SEMANTIC_DRIVER_IN_SLICE_INDEX | GND/VCC used as slice index | Neg 5 |
| ASSIGN_MULTI_DRIVER | GND/VCC and another driver same path | Neg 6, 7 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| SEMANTIC_DRIVER_IN_TERNARY | S2.4 "may not appear in expressions" | Ternary is an expression; should be caught by SEMANTIC_DRIVER_IN_EXPR but worth explicit test |
| SEMANTIC_DRIVER_AS_CONDITION | S2.4 | Using GND/VCC as IF condition (expression context) |
