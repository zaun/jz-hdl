# Test Plan: 1.2 Fundamental Terms

**Specification Reference:** Section 1.2 of jz-hdl-specification.md

## 1. Objective

Verify that the compiler correctly implements the semantic concepts defined in Section 1.2: signals, nets, deterministic nets, drivers, active drivers, sinks, observable sinks, the observability rule, execution paths, assignable identifiers, registers, latches, and home domains. These are foundational definitions that underpin all subsequent semantic analysis.

## 2. Instrumentation Strategy

- **Span: `sem.resolve_nets`** — Trace net resolution after elaboration; attributes: `net.driver_count`, `net.is_deterministic`.
- **Span: `sem.check_observability`** — Verify x-bit propagation to observable sinks; attributes: `sink.type`, `has_x_bits`.
- **Event: `net.multi_driver_detected`** — Fires when a net has more than one driver candidate.
- **Event: `observability.violation`** — Fires when x-bits reach an observable sink.
- **Coverage Hook:** Ensure every driver type (REGISTER, WIRE, PORT, LATCH, GND, VCC) is exercised as both driver and sink.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Single-driver net | WIRE driven by one ASYNCHRONOUS assignment → deterministic net |
| 2 | Register as driver | REGISTER current value read in ASYNCHRONOUS block |
| 3 | Register as sink | REGISTER next-state assigned in SYNCHRONOUS block |
| 4 | Port as observable sink | OUT port driven by expression → value visible externally |
| 5 | Execution path exclusivity | IF/ELSE branches assign same wire → valid (mutually exclusive) |
| 6 | Latch as driver | LATCH read after guarded assignment in ASYNCHRONOUS block |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Zero-driver net | Wire declared but never assigned → compile error |
| 2 | Net with z driver only | All drivers assign z, net is read → floating net error |
| 3 | x-bit masked before sink | Binary literal with x, masked by bitwise AND before OUT port → valid |
| 4 | Single-bit signal as driver | 1-bit wire driving 1-bit port |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | x-bit reaching register | `reg <= 8'bxxxx_xxxx;` as next-state → compile error |
| 2 | x-bit reaching OUT port | `out_port <= 4'bxx00;` without masking → compile error |
| 3 | Multiple active drivers | Two assignments to same wire in same path → compile error |
| 4 | Register written outside home domain | Register assigned in wrong SYNCHRONOUS block → compile error |
| 5 | Latch assigned in SYNCHRONOUS | Latch write in sequential block → compile error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Wire with no driver | Error: undriven net | WIRE_UNDRIVEN | Section 4.5 cross-ref |
| 2 | x-bits to register reset | Error: reset has x | LIT_RESET_HAS_X | Section 2.1 cross-ref |
| 3 | x-bits to OUT port | Error: observability violation | X_OBSERVABLE_SINK | Section 1.2 |
| 4 | Two drivers same path | Error: multi-driver | ASSIGN_MULTI_DRIVER | Section 1.5 cross-ref |
| 5 | Register in wrong domain | Error: cross-domain write | CDC rule | Section 4.12 cross-ref |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver.c` | Semantic analysis entry point | Feed parsed AST; verify diagnostics |
| `driver_net.c` | Net resolution and driver counting | Unit test with crafted AST nodes |
| `driver_assign.c` | Assignment validation | Unit test with assignment AST nodes |
| `driver_expr.c` | Expression analysis for x-bit tracking | Unit test with expression trees |
| `diagnostic.c` | Error collection | Capture and verify diagnostic list |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| ASSIGN_MULTI_DRIVER | Multiple active drivers on same net | Neg 3 |
| WIRE_UNDRIVEN | Net has no driver | Boundary 1 |
| TRISTATE_FLOATING_NET_READ | All drivers z, net is read | Boundary 2 |
| LIT_RESET_HAS_X | x in register reset value | Neg 1 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| X_OBSERVABLE_SINK | S1.2 Observability Rule | No dedicated rule for x-bits reaching observable sinks; may be covered by existing width/literal rules but needs explicit rule |
| NET_NON_DETERMINISTIC | S1.2 Deterministic Net | No explicit rule for non-deterministic net detection as separate check |
