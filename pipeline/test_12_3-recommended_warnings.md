# Test Plan: 12.3 Recommended Warnings

**Specification Reference:** Section 12.3 of jz-hdl-specification.md

## 1. Objective

Verify all recommended warnings are correctly detected and reported: unused register, unsinked register, undriven register, unconnected output, incomplete SELECT in async, dead code, unused module, unused wire, unused port, and internal tristate.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Clean design | Design with no warning conditions | No warnings emitted |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Unused register | Register never read or written | Warning: WARN_UNUSED_REGISTER |
| 2 | Written never read | Register written but value never read | Warning: WARN_UNSINKED_REGISTER |
| 3 | Read never written | Register read but never written | Warning: WARN_UNDRIVEN_REGISTER |
| 4 | Unconnected output | Output port with no assignment | Warning: WARN_UNCONNECTED_OUTPUT |
| 5 | Incomplete SELECT in async | SELECT without DEFAULT in ASYNCHRONOUS block | Warning: WARN_INCOMPLETE_SELECT_ASYNC |
| 6 | Dead code | Unreachable statements | Warning: WARN_DEAD_CODE_UNREACHABLE |
| 7 | Unused module | Module declared but never instantiated | Warning: WARN_UNUSED_MODULE |
| 8 | Unused wire | WIRE declared but never driven or read | Warning: WARN_UNUSED_WIRE |
| 9 | Unused port | PORT declared but never used | Warning: WARN_UNUSED_PORT |
| 10 | Internal tristate | Internal tri-state without --tristate-default flag | Warning: WARN_INTERNAL_TRISTATE |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Register used only in self-assignment | `r <= r;` | May trigger WARN_UNSINKED_REGISTER |
| 2 | Wire driven but only read in dead code | Wire appears used but only in unreachable path | May trigger both WARN_UNUSED_WIRE and WARN_DEAD_CODE_UNREACHABLE |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Unused register | Warning | WARN_UNUSED_REGISTER | warning | S8.3 |
| 2 | Written never read | Warning | WARN_UNSINKED_REGISTER | warning | S8.3 |
| 3 | Read never written | Warning | WARN_UNDRIVEN_REGISTER | warning | S8.3 |
| 4 | Unconnected output | Warning | WARN_UNCONNECTED_OUTPUT | warning | S8.3 |
| 5 | Incomplete SELECT | Warning | WARN_INCOMPLETE_SELECT_ASYNC | warning | S5.4/S8.3 |
| 6 | Dead code | Warning | WARN_DEAD_CODE_UNREACHABLE | warning | S7.7.3/S8.3 |
| 7 | Unused module | Warning | WARN_UNUSED_MODULE | warning | S8.3 |
| 8 | Unused wire | Warning | WARN_UNUSED_WIRE | warning | S12.3 |
| 9 | Unused port | Warning | WARN_UNUSED_PORT | warning | S12.3 |
| 10 | Internal tristate | Warning | WARN_INTERNAL_TRISTATE | warning | S11 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `12_3_WARN_UNUSED_REGISTER-unused_register.jz` | WARN_UNUSED_REGISTER | Register never read or written |
| `12_3_WARN_UNSINKED_REGISTER-written_never_read.jz` | WARN_UNSINKED_REGISTER | Register written but value never read |
| `12_3_WARN_UNDRIVEN_REGISTER-read_never_written.jz` | WARN_UNDRIVEN_REGISTER | Register read but never written |
| `12_3_WARN_UNCONNECTED_OUTPUT-unconnected_output_port.jz` | WARN_UNCONNECTED_OUTPUT | Output port with no assignment |
| `12_3_WARN_INCOMPLETE_SELECT_ASYNC-incomplete_select.jz` | WARN_INCOMPLETE_SELECT_ASYNC | SELECT without DEFAULT in ASYNCHRONOUS block |
| `12_3_WARN_DEAD_CODE_UNREACHABLE-unreachable_branches.jz` | WARN_DEAD_CODE_UNREACHABLE | Unreachable branches detected |
| `12_3_WARN_UNUSED_MODULE-unused_module.jz` | WARN_UNUSED_MODULE | Module declared but never instantiated |
| `12_3_WARN_UNUSED_WIRE-unused_wire.jz` | WARN_UNUSED_WIRE | WIRE declared but never driven or read |
| `12_3_WARN_UNUSED_PORT-unused_port.jz` | WARN_UNUSED_PORT | PORT declared but never used |
| `12_3_WARN_INTERNAL_TRISTATE-internal_tristate.jz` | WARN_INTERNAL_TRISTATE | Internal tri-state without --tristate-default flag |
| `12_3_HAPPY_PATH-recommended_warnings_ok.jz` | — | Happy path: clean design with no warnings |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| WARN_UNUSED_REGISTER | warning | Register never read or written | `12_3_WARN_UNUSED_REGISTER-unused_register.jz` |
| WARN_UNSINKED_REGISTER | warning | Register written but value never read | `12_3_WARN_UNSINKED_REGISTER-written_never_read.jz` |
| WARN_UNDRIVEN_REGISTER | warning | Register read but never written | `12_3_WARN_UNDRIVEN_REGISTER-read_never_written.jz` |
| WARN_UNCONNECTED_OUTPUT | warning | Output port unconnected | `12_3_WARN_UNCONNECTED_OUTPUT-unconnected_output_port.jz` |
| WARN_INCOMPLETE_SELECT_ASYNC | warning | Incomplete SELECT coverage without DEFAULT in ASYNCHRONOUS block | `12_3_WARN_INCOMPLETE_SELECT_ASYNC-incomplete_select.jz` |
| WARN_DEAD_CODE_UNREACHABLE | warning | Dead code (unreachable statements) | `12_3_WARN_DEAD_CODE_UNREACHABLE-unreachable_branches.jz` |
| WARN_UNUSED_MODULE | warning | Module declared but never instantiated | `12_3_WARN_UNUSED_MODULE-unused_module.jz` |
| WARN_UNUSED_WIRE | warning | WIRE declared but never driven or read | `12_3_WARN_UNUSED_WIRE-unused_wire.jz` |
| WARN_UNUSED_PORT | warning | PORT declared but never used | `12_3_WARN_UNUSED_PORT-unused_port.jz` |
| WARN_INTERNAL_TRISTATE | warning | Internal tri-state without --tristate-default flag | `12_3_WARN_INTERNAL_TRISTATE-internal_tristate.jz` |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| — | — | All recommended warning rules are tested |
