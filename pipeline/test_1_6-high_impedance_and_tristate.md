# Test Plan: 1.6 High-Impedance and Tri-State Logic

**Specification Reference:** Section 1.6 of jz-hdl-specification.md

## 1. Objective

Verify the compiler correctly handles high-impedance (`z`) semantics: z literal form restrictions (1.6.1), active vs. high-impedance driver classification (1.6.2), tri-state resolution rules (1.6.3), multi-driver validity with structural proof (1.6.4), tri-state port behavior (1.6.5), register/latch z prohibition (1.6.6), observability rules for z-carrying nets (1.6.7), and the canonical examples (1.6.8).

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Binary literal with z | `8'bzzzz_zzzz` -- valid z literal |
| 2 | z extension | `4'bz` extends to `4'bzzzz` |
| 3 | Conditional tri-state OUT | `out_port <= enable ? data : 8'bzzzz_zzzz;` |
| 4 | Conditional tri-state INOUT | `inout_bus <= wr_en ? data : 8'bzzzz_zzzz;` |
| 5 | Two drivers, mutually exclusive | `IF(en_a) { w <= a; } ELSE { w <= 8'bz; }` + separate driver with inverse guard |
| 6 | z drivers don't conflict | Multiple drivers all assigning z on same path -- valid |
| 7 | INOUT read when released | `IF (!wr_en) { reg <= inout_port; }` with external driver |
| 8 | Three drivers, pairwise exclusive | Three instances each with guard, at most one active |

### 2.2 Error Cases

| # | Test Case | Description | Rule ID |
|---|-----------|-------------|---------|
| 1 | z in decimal literal | `8'dz` -- z not allowed in decimal | LIT_DECIMAL_HAS_XZ |
| 2 | z in hex literal | `8'hz0` -- z not allowed in hex | LIT_INVALID_DIGIT_FOR_BASE |
| 3 | z in register init | `REGISTER { r [8] = 8'bzzzz_zzzz; }` -- z in reset | REG_INIT_CONTAINS_Z |
| 4 | z on non-INOUT port | OUT port driven with z value | PORT_TRISTATE_MISMATCH |
| 5 | Two active drivers same path | Both drive non-z in same execution path | NET_MULTIPLE_ACTIVE_DRIVERS |
| 6 | Floating net read | All drivers z, net is read | NET_TRI_STATE_ALL_Z_READ |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Single-bit z literal | `1'bz` -- valid |
| 2 | Mixed z and data bits | `8'b1100_zzzz` -- upper 4 active, lower 4 z |
| 3 | z in concatenation | `{ 4'b1100, 4'bzzzz }` -- valid |
| 4 | z resolution z/z = z | All drivers z, net not read -- permitted |
| 5 | Hierarchical driver analysis | Child module's OUT port drives parent wire |
| 6 | BUS signal tri-state | Multiple instances on same BUS, mutually exclusive |
| 7 | z in latch | `latch <= 4'bzzzz;` -- Error |
| 8 | z reaching register input | `reg <= tri_net;` where tri_net may carry z without masking |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `8'dz` | Error: z in decimal | LIT_DECIMAL_HAS_XZ | error | S2.1 |
| 2 | `8'hz0` | Error: z in hex | LIT_INVALID_DIGIT_FOR_BASE | error | S2.1 |
| 3 | `REGISTER { r [8] = 8'bz; }` | Error: z in reset | REG_INIT_CONTAINS_Z | error | S2.1/S4.7 |
| 4 | z assigned to OUT port | Error: tristate mismatch | PORT_TRISTATE_MISMATCH | error | S4.4/S4.10/S8.1 |
| 5 | Two active drivers same path | Error: multi active | NET_MULTIPLE_ACTIVE_DRIVERS | error | S1.6.4 |
| 6 | All z, net read | Error: floating net | NET_TRI_STATE_ALL_Z_READ | error | S1.6.4 |
| 7 | `out <= en ? data : 8'bz;` | Valid tri-state | -- | -- | S1.6.5 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 1_6_LIT_DECIMAL_HAS_XZ-z_in_decimal_literal.jz | LIT_DECIMAL_HAS_XZ | Decimal literal with x or z digits |
| 1_6_LIT_INVALID_DIGIT_FOR_BASE-z_in_hex_literal.jz | LIT_INVALID_DIGIT_FOR_BASE | Literal contains digit not allowed for its base (b/d/h) |
| 1_6_PORT_TRISTATE_MISMATCH-z_on_non_inout_port.jz | PORT_TRISTATE_MISMATCH | Only INOUT ports may use z for tri-state; IN/OUT ports must drive 0/1/x |
| 1_6_REG_INIT_CONTAINS_Z-z_in_register_init.jz | REG_INIT_CONTAINS_Z | Register initialization literal must not contain z bits |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| LIT_DECIMAL_HAS_XZ | error | S2.1 Decimal literal with x or z digits | 1_6_LIT_DECIMAL_HAS_XZ-z_in_decimal_literal.jz |
| LIT_INVALID_DIGIT_FOR_BASE | error | S2.1 Literal contains digit not allowed for its base | 1_6_LIT_INVALID_DIGIT_FOR_BASE-z_in_hex_literal.jz |
| PORT_TRISTATE_MISMATCH | error | S4.4/S4.10/S8.1 Only INOUT ports may use z for tri-state | 1_6_PORT_TRISTATE_MISMATCH-z_on_non_inout_port.jz |
| REG_INIT_CONTAINS_Z | error | S2.1/S4.7 Register initialization literal must not contain z bits | 1_6_REG_INIT_CONTAINS_Z-z_in_register_init.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| NET_MULTIPLE_ACTIVE_DRIVERS | error | No dedicated 1.6 test file; may be covered by other section tests |
| NET_TRI_STATE_ALL_Z_READ | error | No dedicated 1.6 test file; related scenario tested by 1_2_ASYNC_FLOATING_Z_READ |
