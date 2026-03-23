# Test Plan: 4.1 Module Canonical Form

**Specification Reference:** Section 4.1 of jz-hdl-specification.md

## 1. Objective

Verify that the parser correctly accepts modules in canonical form with all optional sections (CONST, PORT, WIRE, REGISTER, MEM, @template, ASYNCHRONOUS, SYNCHRONOUS), enforces correct ordering, and rejects malformed module structures.

## 2. Instrumentation Strategy

- **Span: `parser.module`** — Trace module parsing; attributes: `module_name`, `section_count`, `has_port`, `has_sync`, `has_async`.
- **Event: `module.section_parsed`** — Fires for each section within a module (CONST, PORT, WIRE, etc.).
- **Coverage Hook:** Ensure each optional section is tested both present and absent.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Full canonical form | Module with all sections: CONST, PORT, WIRE, REGISTER, MEM, @template, ASYNCHRONOUS, SYNCHRONOUS |
| 2 | Minimal module | `@module m PORT { IN [1] clk; } @endmod` — just a port |
| 3 | Module without CONST | No CONST block, but PORT + WIRE + ASYNC |
| 4 | Module without REGISTER | No registers, pure combinational |
| 5 | Module without WIRE | Only ports and registers |
| 6 | Multiple SYNCHRONOUS blocks | Different clocks, each with own SYNC block |
| 7 | SYNCHRONOUS header variants | All optional properties (EDGE, RESET, RESET_ACTIVE, RESET_TYPE) |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Empty ASYNC body | `ASYNCHRONOUS { }` — valid but may warn |
| 2 | Empty SYNC body | `SYNCHRONOUS(CLK=clk) { }` — valid, registers hold |
| 3 | Module with only IN ports | Valid but may warn (dead code) |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Missing @endmod | Module without closing directive — parse error |
| 2 | Missing PORT | Module with no PORT block — compile error |
| 3 | Duplicate CONST block | Two CONST blocks in same module |
| 4 | @module inside @module | Nested module definitions — error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Full canonical module | AST with all sections | — | Valid |
| 2 | Module without PORT | Error | PORT_MISSING | Every module needs PORT |
| 3 | Missing @endmod | Parse error | — | Unclosed module |
| 4 | Nested @module | Error | DIRECTIVE_INVALID_CONTEXT | Modules cannot nest |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_module.c` | Parses module structure | Feed token streams |
| `parser_core.c` | Core parsing coordination | Integration with module parser |
| `ast.c` | AST node creation | Verify tree structure |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| DIRECTIVE_INVALID_CONTEXT | Structural directives in wrong location | Neg 4 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| MODULE_NO_PORT | S4.2 "must declare at least one PORT" | May need explicit rule for empty/missing PORT |
| MODULE_ONLY_IN_PORTS | S4.2 "only IN ports" | Warning for module with no outputs |
