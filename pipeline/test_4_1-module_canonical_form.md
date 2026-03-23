# Test Plan: 4.1 Module Canonical Form

**Specification Reference:** Section 4.1 of jz-hdl-specification.md

## 1. Objective

Verify module canonical form, section ordering, structural directives. Confirm that modules accept all optional sections (CONST, PORT, WIRE, REGISTER, MEM, @template, ASYNCHRONOUS, SYNCHRONOUS), enforce correct ordering, reject duplicate blocks, require at least one PORT, and warn on input-only modules.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Full canonical form | Module with all sections: CONST, PORT, WIRE, REGISTER, MEM, @template, ASYNCHRONOUS, SYNCHRONOUS |
| 2 | Minimal module | `@module m PORT { IN [1] clk; } @endmod` -- just a port |
| 3 | Module without CONST | No CONST block, but PORT + WIRE + ASYNC |
| 4 | Module without REGISTER | No registers, pure combinational |
| 5 | Module without WIRE | Only ports and registers |
| 6 | Multiple SYNCHRONOUS blocks | Different clocks, each with own SYNC block |
| 7 | SYNCHRONOUS header variants | All optional properties (EDGE, RESET, RESET_ACTIVE, RESET_TYPE) |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Missing @endmod | Module without closing directive -- parse error |
| 2 | Missing PORT | Module with no PORT block -- compile error |
| 3 | Duplicate SYNCHRONOUS block | Two SYNCHRONOUS blocks for the same clock in one module |
| 4 | Nested @module | @module inside @module -- structural directive in invalid location |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Empty ASYNC body | `ASYNCHRONOUS { }` -- valid but may warn |
| 2 | Empty SYNC body | `SYNCHRONOUS(CLK=clk) { }` -- valid, registers hold |
| 3 | Module with only IN ports | Valid but warns (likely dead code) |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|-----------------|---------|----------|-------|
| 1 | Full canonical module | AST with all sections | -- | -- | Valid |
| 2 | Nested @module inside @module | Error: structural directive in wrong location | DIRECTIVE_INVALID_CONTEXT | error | Modules cannot nest |
| 3 | Two SYNCHRONOUS blocks for same clock | Error: duplicate block | DUPLICATE_BLOCK | error | Only one SYNC per clock per module |
| 4 | Module with no PORT block | Error: missing port block | MODULE_MISSING_PORT | error | Every module needs at least one port |
| 5 | Module with only IN ports | Warning: only input ports | MODULE_PORT_IN_ONLY | warning | Likely dead code |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_1_DIRECTIVE_INVALID_CONTEXT-nested_structural_directives.jz | DIRECTIVE_INVALID_CONTEXT | Structural directives (@module etc.) used in invalid nesting context |
| 4_1_DUPLICATE_BLOCK-duplicate_sync_block.jz | DUPLICATE_BLOCK | Two SYNCHRONOUS blocks for the same clock signal |
| 4_1_MODULE_MISSING_PORT-no_port_block.jz | MODULE_MISSING_PORT | Module declared without a PORT block |
| 4_1_MODULE_PORT_IN_ONLY-only_input_ports.jz | MODULE_PORT_IN_ONLY | Module declares only IN ports, no OUT or INOUT |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| DIRECTIVE_INVALID_CONTEXT | error | S1.1/S6.2 Structural directives (@project/@module/@endproj/@endmod/@blackbox/@new/@import) used in invalid location | 4_1_DIRECTIVE_INVALID_CONTEXT-nested_structural_directives.jz |
| DUPLICATE_BLOCK | error | S4.11/S4.12 More than one SYNCHRONOUS block declared for the same clock signal in a module | 4_1_DUPLICATE_BLOCK-duplicate_sync_block.jz |
| MODULE_MISSING_PORT | error | S4.2/S4.4/S8.1 Module missing required PORT block or PORT block is empty | 4_1_MODULE_MISSING_PORT-no_port_block.jz |
| MODULE_PORT_IN_ONLY | warning | S4.2/S4.4/S8.3 Module declares only IN ports (no OUT/INOUT; likely dead code) | 4_1_MODULE_PORT_IN_ONLY-only_input_ports.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All rules for this section are covered by existing tests |
