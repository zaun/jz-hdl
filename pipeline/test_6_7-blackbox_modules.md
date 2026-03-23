# Test Plan: 6.7 Blackbox (Opaque) Modules

**Specification Reference:** Section 6.7 of jz-hdl-specification.md

## 1. Objective

Verify @blackbox declaration within @project, PORT-only interface (no internal logic visible), instantiation via @new, and name uniqueness with regular modules.

## 2. Instrumentation Strategy

- **Span: `parser.blackbox`** — attributes: `name`, `port_count`.
- **Event: `blackbox.has_body`** — Blackbox with logic body.

## 3. Test Scenarios

### 3.1 Happy Path
1. Simple blackbox: `@blackbox uart { PORT { IN [1] clk; OUT [8] data; } }`
2. Blackbox instantiated via @new
3. Multiple blackboxes in project

### 3.2 Boundary/Edge Cases
1. Blackbox with only IN ports
2. Blackbox with INOUT ports

### 3.3 Negative Testing
1. Blackbox name conflicts with module — Error
2. Blackbox with logic body (ASYNCHRONOUS/SYNCHRONOUS) — Error
3. Blackbox outside @project — Error
4. Instantiation of undeclared blackbox — Error

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Blackbox name = module name | Error | MODULE_DUPLICATE_NAME | S4.2/S6.7 |
| 2 | Blackbox with ASYNC | Error | BLACKBOX_HAS_BODY | S6.7 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_project.c` | Blackbox parsing | Token stream |
| `driver_instance.c` | Blackbox instantiation validation | Integration test |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| MODULE_DUPLICATE_NAME | Name conflict with module | Neg 1 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| BLACKBOX_HAS_BODY | S6.7 | Blackbox with logic — should be opaque only |
