# Test Plan: 10.4 Template Forbidden Content
**Specification Reference:** Section 10.4 of jz-hdl-specification.md
## 1. Objective
Verify forbidden content: WIRE, REGISTER, PORT, CONST, MEM, MUX, @new, @module, @project, CDC, SYNC/ASYNC headers, nested @template, @feature.
## 2. Instrumentation Strategy
- **Span: `sem.template_forbidden`** — attributes: `forbidden_type`.
## 3. Test Scenarios
### 3.3 Negative Testing
1. WIRE in template — Error (TEMPLATE_FORBIDDEN_DECL)
2. REGISTER in template — Error
3. @new in template — Error (TEMPLATE_FORBIDDEN_DIRECTIVE)
4. SYNCHRONOUS header in template — Error (TEMPLATE_FORBIDDEN_BLOCK_HEADER)
5. Nested @template — Error (TEMPLATE_NESTED_DEF)
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | WIRE in template | Error | TEMPLATE_FORBIDDEN_DECL | S10.4 |
| 2 | SYNC header | Error | TEMPLATE_FORBIDDEN_BLOCK_HEADER | S10.4 |
| 3 | @new in template | Error | TEMPLATE_FORBIDDEN_DIRECTIVE | S10.4 |
| 4 | Nested @template | Error | TEMPLATE_NESTED_DEF | S10.4 |
## 5-6. See 10.2 for integration.
