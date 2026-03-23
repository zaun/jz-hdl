# Test Plan: 6.9 Top-Level Module Instantiation (@top)

**Specification Reference:** Section 6.9 (including 6.9.1) of jz-hdl-specification.md

## 1. Objective

Verify @top directive: module/blackbox reference, port-to-pin binding, no-connect (`_`), width matching, and logical-to-physical expansion.

## 2. Instrumentation Strategy

- **Span: `sem.top_validate`** — attributes: `module_name`, `port_bindings`, `unconnected_count`.

## 3. Test Scenarios

### 3.1 Happy Path
1. @top with all ports bound to pins
2. @top with no-connect on debug port: `OUT [8] debug = _;`
3. @top referencing imported module
4. Port bound to pin with matching width

### 3.2 Boundary/Edge Cases
1. Top module with only IN ports
2. Top module with INOUT ports bound to INOUT_PINS

### 3.3 Negative Testing
1. @top references undefined module — Error
2. Port not bound — Error
3. Width mismatch between port and pin — Error
4. Multiple @top directives — Error
5. No @top in project — Error

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Undefined module in @top | Error | INSTANCE_UNDEFINED_MODULE | S6.9 |
| 2 | Port width ≠ pin width | Error | — | Width mismatch |
| 3 | Multiple @top | Error | PROJECT_MULTIPLE_TOP | S6.9 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_project.c` | @top validation | Integration test |
| `parser_project.c` | @top parsing | Token stream |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| INSTANCE_UNDEFINED_MODULE | Unknown module in @top | Neg 1 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| PROJECT_MULTIPLE_TOP | S6.9 | Multiple @top directives |
| PROJECT_NO_TOP | S6.9 | Missing @top in project |
| TOP_PORT_UNBOUND | S6.9 | Port not connected in @top |
