# Test Plan: 1.6 High-Impedance and Tri-State Logic

**Specification Reference:** Section 1.6 (1.6.1–1.6.8) of jz-hdl-specification.md

## 1. Objective

Verify the compiler correctly handles high-impedance (`z`) semantics: z literal form restrictions (1.6.1), active vs. high-impedance driver classification (1.6.2), tri-state resolution rules (1.6.3), multi-driver validity with structural proof (1.6.4), tri-state port behavior (1.6.5), register/latch z prohibition (1.6.6), observability rules for z-carrying nets (1.6.7), and the canonical examples (1.6.8).

## 2. Instrumentation Strategy

- **Span: `sem.tristate_analysis`** — Trace tri-state net analysis; attributes: `net.name`, `driver_count`, `active_driver_count`.
- **Span: `sem.tristate_proof`** — Structural mutual-exclusion proof; attributes: `guard_condition`, `proof_result`.
- **Event: `tristate.floating_net`** — All drivers assign z, net is read.
- **Event: `tristate.multi_active`** — Two active drivers in same path.
- **Event: `tristate.z_in_register`** — z assigned to register.
- **Coverage Hook:** Ensure all resolution table entries (0/z, 1/z, z/z, 0/1-prohibited) are tested.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Binary literal with z | `8'bzzzz_zzzz` — valid z literal |
| 2 | z extension | `4'bz` extends to `4'bzzzz` |
| 3 | Conditional tri-state OUT | `out_port <= enable ? data : 8'bzzzz_zzzz;` |
| 4 | Conditional tri-state INOUT | `inout_bus <= wr_en ? data : 8'bzzzz_zzzz;` |
| 5 | Two drivers, mutually exclusive | `IF(en_a) { w <= a; } ELSE { w <= 8'bz; }` + separate driver with inverse guard |
| 6 | z drivers don't conflict | Multiple drivers all assigning z on same path — valid |
| 7 | INOUT read when released | `IF (!wr_en) { reg <= inout_port; }` with external driver |
| 8 | Three drivers, pairwise exclusive | Three instances each with guard, at most one active |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Single-bit z literal | `1'bz` — valid |
| 2 | Mixed z and data bits | `8'b1100_zzzz` — upper 4 active, lower 4 z |
| 3 | z in concatenation | `{ 4'b1100, 4'bzzzz }` — valid |
| 4 | z resolution z/z = z | All drivers z, net not read — permitted |
| 5 | Hierarchical driver analysis | Child module's OUT port drives parent wire |
| 6 | BUS signal tri-state | Multiple instances on same BUS, mutually exclusive |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | z in hex literal | `8'hz0` — Error: z not allowed in hex |
| 2 | z in decimal literal | `8'dz` — Error: z not allowed in decimal |
| 3 | z in register reset | `REGISTER { r [8] = 8'bzzzz_zzzz; }` — Error |
| 4 | z assigned to register | `reg <= 8'bzzzz_zzzz;` in SYNCHRONOUS — Error |
| 5 | Two active drivers same path | Both drive non-z in same execution path — Error |
| 6 | Floating net read | All drivers z, net is read — Error |
| 7 | Non-provable mutual exclusion | Two drivers with conditions compiler can't prove exclusive |
| 8 | z in latch | `latch <= 4'bzzzz;` — Error |
| 9 | z reaching register input | `reg <= tri_net;` where tri_net may carry z without masking |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `8'hz0` | Error: z in hex | LIT_HEX_HAS_XZ | S1.6.1 |
| 2 | `REGISTER { r [8] = 8'bz; }` | Error: z in reset | LIT_RESET_HAS_Z | S1.6.6 |
| 3 | Two active drivers same path | Error: multi active | TRISTATE_MULTIPLE_ACTIVE_DRIVERS | S1.6.4 |
| 4 | All z, net read | Error: floating net | TRISTATE_FLOATING_NET_READ | S1.6.4 |
| 5 | `out <= en ? data : 8'bz;` | Valid tri-state | — | S1.6.5 |
| 6 | Non-provable exclusion | Error: can't prove | TRISTATE_MULTIPLE_ACTIVE_DRIVERS | S1.6.4 |
| 7 | z reaching reg next-state | Error: observability | — | S1.6.7 |
| 8 | `reg <= 8'bz;` in SYNC | Error: z to register | REG_Z_ASSIGN | S1.6.6 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_tristate.c` | Tri-state net analysis and proof engine | Feed crafted multi-driver AST; verify diagnostics |
| `driver_assign.c` | Assignment analysis (driver enumeration) | Provide known driver sets |
| `driver_instance.c` | Hierarchical driver extraction from instances | Mock child module with known port drivers |
| `driver_net.c` | Net resolution | Provide net with multiple driver entries |
| `sem_literal.c` | Literal validation (z in wrong base) | Unit test with literal AST nodes |
| `ir_tristate_transform.c` | Tri-state IR transformation | Integration test post-semantic |
| `sim_eval.c` | Runtime tri-state resolution | Simulation test with z values |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| LIT_HEX_HAS_XZ | z not allowed in hex literals | Neg 1 |
| LIT_DECIMAL_HAS_XZ | z not allowed in decimal literals | Neg 2 |
| LIT_RESET_HAS_Z | z in register reset literal | Neg 3 |
| TRISTATE_MULTIPLE_ACTIVE_DRIVERS | Two active drivers in same path | Neg 5, 7 |
| TRISTATE_FLOATING_NET_READ | All drivers z, net is read | Neg 6 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| REG_Z_ASSIGN | S1.6.6 "Registers cannot store z" | No explicit rule for z assigned as next-state to register (may be covered by type checking) |
| LATCH_Z_ASSIGN | S1.6.6 "Latches cannot store z" | No explicit rule for z assigned to latch |
| TRISTATE_Z_OBSERVABILITY | S1.6.7 "z bits reaching observable sink" | May be covered by general observability checks but no explicit rule |
| TRISTATE_HIERARCHY_UNPROVEN | S1.6.4 "across entire elaborated hierarchy" | No explicit rule for hierarchical proof failure vs. single-module proof |
