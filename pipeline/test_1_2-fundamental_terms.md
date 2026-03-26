# Test Plan: 1.2 Fundamental Terms

**Specification Reference:** Section 1.2 of jz-hdl-specification.md

## 1. Objective

Verify that the compiler correctly implements the semantic concepts defined in Section 1.2: signals, nets, deterministic nets, drivers, active drivers, sinks, observable sinks, the observability rule, execution paths, assignable identifiers, registers, latches, and home domains. These are foundational definitions that underpin all subsequent semantic analysis.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Single-driver net | WIRE driven by one ASYNCHRONOUS assignment -- deterministic net |
| 2 | Register as driver | REGISTER current value read in ASYNCHRONOUS block |
| 3 | Register as sink | REGISTER next-state assigned in SYNCHRONOUS block |
| 4 | Port as observable sink | OUT port driven by expression -- value visible externally |
| 5 | Execution path exclusivity | IF/ELSE branches assign same wire -- valid (mutually exclusive) |
| 6 | Latch as driver | LATCH read after guarded assignment in ASYNCHRONOUS block |

### 2.2 Error Cases

| # | Test Case | Description | Rule ID |
|---|-----------|-------------|---------|
| 1 | Floating net read | Wire declared but never driven, then read | NET_FLOATING_WITH_SINK |
| 2 | All z drivers read | All drivers assign z, net is read | NET_TRI_STATE_ALL_Z_READ |
| 3 | x-bit reaching observable sink | Expression with x bits drives register/output | OBS_X_TO_OBSERVABLE_SINK |
| 4 | x in register init | Register reset literal contains x bits | REG_INIT_CONTAINS_X |
| 5 | Register in wrong domain | Register assigned in SYNCHRONOUS block with non-home clock | DOMAIN_CONFLICT |
| 6 | Register multi-clock assign | Same register assigned in blocks with different clocks | MULTI_CLK_ASSIGN |
| 7 | Latch assigned in SYNCHRONOUS | Latch write in sequential block | LATCH_ASSIGN_IN_SYNC |
| 8 | Multiple active drivers | Two non-z drivers active on same net in same execution path | NET_MULTIPLE_ACTIVE_DRIVERS |
| 9 | Dangling unused signal | Signal declared but never driven or read | NET_DANGLING_UNUSED |
| 10 | Unconditional combinational loop | Signal feeds back to itself through ASYNCHRONOUS assignments | COMB_LOOP_UNCONDITIONAL |
| 11 | Conditional combinational loop (safe) | Cycle only within mutually exclusive branches | COMB_LOOP_CONDITIONAL_SAFE |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | x-bit masked before sink | Binary literal with x, masked by bitwise AND before OUT port -- valid |
| 2 | Single-bit signal as driver | 1-bit wire driving 1-bit port |
| 3 | Net with z driver only, not read | All drivers z but net has no sinks -- permitted |
| 4 | Three-signal combinational cycle | A->B->C->A forms unconditional loop |
| 5 | Conditional loop in exclusive branches | Self-feedback only in mutually exclusive IF/ELSE arms |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Wire with no driver, then read | Error: floating net | NET_FLOATING_WITH_SINK | error | S1.2/S4.10 |
| 2 | All z drivers, net read | Error: floating z read | NET_TRI_STATE_ALL_Z_READ | error | S4.10 |
| 3 | x-bits to observable sink | Error: observability violation | OBS_X_TO_OBSERVABLE_SINK | error | S1.2/S2.1/S3.2 |
| 4 | x-bits in register init | Error: x in reset | REG_INIT_CONTAINS_X | error | S2.1/S4.7 |
| 5 | Register in wrong sync block | Error: domain conflict | DOMAIN_CONFLICT | error | S4.11/S4.12 |
| 6 | Register in two clock domains | Error: multi-clock assign | MULTI_CLK_ASSIGN | error | S4.11/S4.12 |
| 7 | Latch written in SYNCHRONOUS | Error: wrong block type | LATCH_ASSIGN_IN_SYNC | error | S4.8/S4.11 |
| 8 | Two non-z drivers on same net | Error: multiple active drivers | NET_MULTIPLE_ACTIVE_DRIVERS | error | S1.2/S1.5/S4.10 |
| 9 | Signal declared, never used | Warning: dangling unused | NET_DANGLING_UNUSED | warning | S5.1/S8.3 |
| 10 | Signal feeds back to itself unconditionally | Error: combinational loop | COMB_LOOP_UNCONDITIONAL | error | S5.3/S8.2 |
| 11 | Cycle within exclusive branches only | Warning: conditional safe loop | COMB_LOOP_CONDITIONAL_SAFE | warning | S5.3/S8.2 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 1_2_FUNDAMENTAL_TERMS-happy_path_ok.jz | -- | Happy path: fundamental term semantics accepted |
| 1_2_ASYNC_FLOATING_Z_READ-all_z_drivers_read.jz | NET_TRI_STATE_ALL_Z_READ | Net has sinks but all drivers assign z (tri-state bus fully released while read) |
| 1_2_DOMAIN_CONFLICT-register_wrong_domain.jz | DOMAIN_CONFLICT, MULTI_CLK_ASSIGN | Register or CDC alias used in SYNCHRONOUS block whose CLK does not match its home-domain clock |
| 1_2_LATCH_ASSIGN_IN_SYNC-latch_written_in_sync.jz | LATCH_ASSIGN_IN_SYNC | LATCH may not be written in SYNCHRONOUS blocks |
| 1_2_MULTI_CLK_ASSIGN-register_multi_clock.jz | MULTI_CLK_ASSIGN, DOMAIN_CONFLICT | Same register assigned in SYNCHRONOUS blocks driven by different clocks |
| 1_2_NET_DANGLING_UNUSED-unused_signal.jz | NET_DANGLING_UNUSED | Signal declared but never used (neither driven nor read) |
| 1_2_NET_FLOATING_WITH_SINK-floating_net_read.jz | NET_FLOATING_WITH_SINK | Signal is read but never driven (floating net) |
| 1_2_OBS_X_TO_OBSERVABLE_SINK-x_bits_to_observable.jz | OBS_X_TO_OBSERVABLE_SINK | Expression containing x bits drives REGISTER, MEM, or output |
| 1_2_REG_INIT_CONTAINS_X-x_in_register_init.jz | REG_INIT_CONTAINS_X | Register initialization literal must not contain x bits |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| NET_FLOATING_WITH_SINK | error | S1.2/S4.10 Signal is read but never driven (floating net) | 1_2_NET_FLOATING_WITH_SINK-floating_net_read.jz |
| NET_MULTIPLE_ACTIVE_DRIVERS | error | S1.2/S1.5/S4.10 Multiple active drivers on same signal | 11_3_NET_MULTIPLE_ACTIVE_DRIVERS-multi_driver_non_tristate.jz (cross-ref S11) |
| NET_DANGLING_UNUSED | warning | S5.1/S8.3 Signal declared but never used (neither driven nor read) | 1_2_NET_DANGLING_UNUSED-unused_signal.jz |
| OBS_X_TO_OBSERVABLE_SINK | error | S1.2/S2.1/S3.2 Expression containing x bits drives observable sink | 1_2_OBS_X_TO_OBSERVABLE_SINK-x_bits_to_observable.jz |
| COMB_LOOP_UNCONDITIONAL | error | S5.3/S8.2 Combinational loop: signal feeds back to itself | 12_2_COMB_LOOP_UNCONDITIONAL-two_signal_cycle.jz, 12_2_COMB_LOOP_UNCONDITIONAL-three_signal_cycle.jz (cross-ref S12.2) |
| COMB_LOOP_CONDITIONAL_SAFE | warning | S5.3/S8.2 Cycle only within mutually exclusive branches | 12_2_COMB_LOOP_CONDITIONAL_SAFE-mutually_exclusive_cycle.jz (cross-ref S12.2) |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| NET_TRI_STATE_ALL_Z_READ | error | Suppressed by ASYNC_FLOATING_Z_READ: test exists (`1_2_ASYNC_FLOATING_Z_READ-all_z_drivers_read.jz`) but rule is suppressed |
