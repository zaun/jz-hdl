# Test Plan: 4.13 Module Instantiation

**Specification Reference:** Section 4.13 (including 4.13.1 Instance Arrays) of jz-hdl-specification.md

## 1. Objective

Verify @new syntax, port binding (IN/OUT/INOUT/BUS), OVERRIDE block, width rules with modifiers (=, =z, =s), no-connect (`_`), port-width validation workflow, instance arrays with IDX keyword, broadcast vs indexed mapping, non-overlap rule for OUT ports, and exclusive assignment compliance.

## 2. Instrumentation Strategy

- **Span: `sem.instantiate`** — Trace module instantiation; attributes: `instance_name`, `module_name`, `port_count`, `is_array`, `array_count`.
- **Span: `sem.override`** — OVERRIDE evaluation; attributes: `child_const`, `parent_value`.
- **Span: `sem.port_bind`** — Port binding; attributes: `port_name`, `direction`, `width_match`, `modifier`.
- **Event: `instance.undefined_module`** — Referenced module doesn't exist.
- **Event: `instance.port_missing`** — Not all ports listed.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Simple instantiation | `@new inst mod { IN [8] a = sig; OUT [8] b = res; }` |
| 2 | OVERRIDE constants | `OVERRIDE { WIDTH = 16; }` — child CONST override |
| 3 | No-connect output | `OUT [8] debug = _;` |
| 4 | Width modifier =z | `IN [16] wide_port =z narrow_sig;` |
| 5 | Width modifier =s | `IN [16] signed_port =s narrow_sig;` |
| 6 | Instance array | `@new inst[4] mod { ... }` — 4 instances |
| 7 | IDX in mapping | `OUT [8] data = result[(IDX+1)*8-1:IDX*8];` |
| 8 | Broadcast mapping | `IN [1] clk = clk;` (no IDX → broadcast to all) |
| 9 | Port referencing | `inst.port_name` in ASYNC — valid read |
| 10 | BUS port binding | `BUS SPI TARGET spi = parent_spi;` |
| 11 | Literal tie-off | `IN [2] sel = 2'b11;` |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Array count = 1 | Single instance in array notation |
| 2 | Large array | `@new inst[256] mod { ... }` |
| 3 | CONST in array count | `@new inst[COUNT] mod { ... }` |
| 4 | IDX in complex expression | Bit-slicing with IDX arithmetic |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Undefined module | `@new inst unknown_mod { ... }` — Error |
| 2 | Missing port | Not all ports listed in @new — Error |
| 3 | Width mismatch no modifier | `IN [16] p = 8-bit-sig;` — Error |
| 4 | Truncation (parent wider) | `IN [8] p = 16-bit-sig;` — Error |
| 5 | IDX in OVERRIDE | `OVERRIDE { WIDTH = IDX; }` — Error |
| 6 | Overlapping OUT bits | Two array instances drive same parent bits — Error |
| 7 | Duplicate instance name | Two `@new same_name` — Error |
| 8 | Self-instantiation | Module instantiates itself — Error |
| 9 | Port width mismatch | Parent width ≠ child port width — Error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Undefined module | Error | INSTANCE_UNDEFINED_MODULE | S4.13 |
| 2 | Missing port in @new | Error | INSTANCE_PORT_MISSING | S4.13 |
| 3 | Width mismatch | Error | INSTANCE_PORT_WIDTH_MISMATCH | S4.13 |
| 4 | IDX in OVERRIDE | Error | INSTANCE_IDX_IN_OVERRIDE | S4.13.1 |
| 5 | Overlapping OUT bits | Error | INSTANCE_OVERLAP | S4.13.1 |
| 6 | Self-instantiation | Error | MODULE_SELF_INSTANTIATION | S4.2 |
| 7 | Duplicate instance name | Error | SCOPE_DUPLICATE_SIGNAL | S4.2 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_instance.c` | Parses @new blocks | Token stream |
| `driver_instance.c` | Instance semantic validation | Integration test with multi-module AST |
| `const_eval.c` | OVERRIDE evaluation | Mock parent/child CONST scopes |
| `driver_assign.c` | Exclusive assignment for OUT ports | Integration test |
| `driver_width.c` | Port width validation | Unit test |
| `chip_data.c` | BUS definitions for BUS port binding | Mock BUS defs |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| INSTANCE_UNDEFINED_MODULE | Module not found | Neg 1 |
| INSTANCE_PORT_MISSING | Port not connected | Neg 2 |
| INSTANCE_PORT_WIDTH_MISMATCH | Width mismatch in binding | Neg 3, 9 |
| MODULE_SELF_INSTANTIATION | Self-instantiation | Neg 8 |
| SCOPE_DUPLICATE_SIGNAL | Duplicate instance name | Neg 7 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| INSTANCE_IDX_IN_OVERRIDE | S4.13.1 "IDX prohibited in OVERRIDE" | Explicit rule for IDX in OVERRIDE |
| INSTANCE_ARRAY_OVERLAP | S4.13.1 "Non-Overlap Rule" | Overlapping bit ranges from array OUT mapping |
| INSTANCE_TRUNCATION | S4.13 "no truncation" | Parent wider than child with no extension |
