# Test Plan: 4.7 REGISTER (Storage Elements)

**Specification Reference:** Section 4.7 of jz-hdl-specification.md

## 1. Objective

Verify REGISTER declaration (width + mandatory reset value), read/write semantics (current-value readable everywhere, next-state written only in SYNCHRONOUS), reset literal restrictions (no x or z), single-dimensional constraint, and error detection for invalid register usage.

## 2. Instrumentation Strategy

- **Span: `sem.register_check`** — Trace register validation; attributes: `reg_name`, `width`, `reset_value`, `home_domain`.
- **Event: `reg.write_in_async`** — Register assigned in ASYNCHRONOUS block.
- **Event: `reg.reset_has_x`** — Reset literal contains x.
- **Event: `reg.multi_dimensional`** — Multi-dimensional register attempted.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Standard register | `counter [8] = 8'h00;` |
| 2 | Read in ASYNC | `output = counter;` in ASYNCHRONOUS |
| 3 | Write in SYNC | `counter <= counter + 8'd1;` in SYNCHRONOUS |
| 4 | GND reset | `data [8] = GND;` — all zeros |
| 5 | VCC reset | `flags [8] = VCC;` — all ones |
| 6 | Read current, write next | RHS reads current, LHS schedules next in same SYNC block |
| 7 | Multiple registers | Several registers in one REGISTER block |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit register | `flag [1] = 1'b0;` |
| 2 | Wide register | `data [256] = 256'd0;` |
| 3 | Register not written | Declared but never assigned in SYNC — Warning |
| 4 | Register not read | Written but never read — Warning |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Write in ASYNC | `reg <= data;` in ASYNCHRONOUS — Error |
| 2 | Reset has x | `r [8] = 8'bxxxx_0000;` — Error |
| 3 | Reset has z | `r [8] = 8'bzzzz_zzzz;` — Error |
| 4 | Multi-dimensional | `r [8] [4];` — Error |
| 5 | Missing reset value | `r [8];` — Error |
| 6 | Unused register | Declared, never read or written — Warning |
| 7 | Write in wrong domain | Register from clk_a written in SYNC(CLK=clk_b) — Error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Register in ASYNC LHS | Error | ASSIGN_OP_WRONG_BLOCK | S4.7 |
| 2 | Reset with x bits | Error | LIT_RESET_HAS_X | S2.1/S4.7 |
| 3 | Reset with z bits | Error | LIT_RESET_HAS_Z | S2.1/S4.7 |
| 4 | Multi-dimensional decl | Error | — | Parse error |
| 5 | Unused register | Warning | WARN_UNUSED_REGISTER | S8.3 |
| 6 | Written never read | Warning | WARN_UNSINKED_REGISTER | S8.3 |
| 7 | Read never written | Warning | WARN_UNDRIVEN_REGISTER | S8.3 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_register.c` | Parses REGISTER declarations | Token stream |
| `driver_assign.c` | Assignment context validation | Integration test |
| `sem_literal.c` | Reset literal validation | Unit test |
| `driver_clocks.c` | Home domain checking | Integration test |
| `diagnostic.c` | Error/warning reporting | Capture diagnostics |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| ASSIGN_OP_WRONG_BLOCK | Register assigned in ASYNC | Neg 1 |
| LIT_RESET_HAS_X | x in register reset | Neg 2 |
| LIT_RESET_HAS_Z | z in register reset | Neg 3 |
| WARN_UNUSED_REGISTER | Register never read or written | Neg 6 |
| WARN_UNSINKED_REGISTER | Written but never read | Boundary 4 |
| WARN_UNDRIVEN_REGISTER | Read but never written | Boundary 3 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| REG_MISSING_RESET | S4.7 "Mandatory reset value" | Missing reset literal in declaration |
| REG_MULTI_DIMENSIONAL | S4.7 "single-dimensional" | May be parse error |
