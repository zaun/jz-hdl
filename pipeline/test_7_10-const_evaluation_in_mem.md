# Test Plan: 7.10 CONST Evaluation in MEM

**Specification Reference:** Section 7.10 of jz-hdl-specification.md

## 1. Objective

Verify CONST and CONFIG usage in MEM dimensions (width, depth), address width derivation from depth, and clog2-based address width computation.

## 2. Instrumentation Strategy

- **Span: `sem.mem_const_eval`** — attributes: `const_name`, `resolved_value`, `context`.

## 3. Test Scenarios

### 3.1 Happy Path
1. CONST width: `MEM { m [WIDTH] [DEPTH] ... }`
2. CONFIG depth: `MEM { m [8] [CONFIG.DEPTH] ... }`
3. Address width = clog2(depth)

### 3.2 Negative Testing
1. Undefined CONST in MEM — Error
2. Negative CONST value for depth — Error

## 4-6. Cross-reference with Section 4.3 (CONST) and 6.3 (CONFIG) test plans.
