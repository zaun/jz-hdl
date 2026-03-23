# Test Plan: 7.9 MEM in Module Instantiation

**Specification Reference:** Section 7.9 of jz-hdl-specification.md

## 1. Objective

Verify MEM behavior when modules containing MEM are instantiated: port binding, OVERRIDE effects on MEM dimensions, and hierarchical access.

## 2. Instrumentation Strategy

- **Span: `sem.mem_instance`** — attributes: `parent_module`, `child_mem`, `effective_depth`, `effective_width`.

## 3. Test Scenarios

### 3.1 Happy Path
1. Module with MEM instantiated normally
2. OVERRIDE changes MEM depth via CONST
3. Multiple instances of MEM-containing module

### 3.2 Negative Testing
1. Direct access to child MEM from parent — Error (must go through ports)

## 4-6. Cross-reference with Section 4.13 (Module Instantiation) test plans.
