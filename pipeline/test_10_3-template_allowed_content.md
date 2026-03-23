# Test Plan: 10.3 Template Allowed Content
**Specification Reference:** Section 10.3 of jz-hdl-specification.md
## 1. Objective
Verify allowed content: directional/alias assignments, IF/ELIF/ELSE, SELECT/CASE, expressions, and @scratch wires. Verify TEMPLATE_EXTERNAL_REF rule.
## 2. Instrumentation Strategy
- **Span: `sem.template_content`** — attributes: `statement_types`, `scratch_count`.
## 3. Test Scenarios
### 3.1 Happy Path
1. Assignments using parameters
2. IF/ELSE inside template
3. @scratch wire declaration and use
4. CONST/CONFIG/@global references without passing as params
### 3.2 Boundary/Edge Cases
1. @scratch with widthof() in width expression
### 3.3 Negative Testing
1. @scratch outside template — Error (TEMPLATE_SCRATCH_OUTSIDE)
2. @scratch width not constant — Error (TEMPLATE_SCRATCH_WIDTH_INVALID)
3. External signal ref without passing as param — Error (TEMPLATE_EXTERNAL_REF)
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | @scratch outside template | Error | TEMPLATE_SCRATCH_OUTSIDE | S10.3 |
| 2 | Invalid scratch width | Error | TEMPLATE_SCRATCH_WIDTH_INVALID | S10.3 |
| 3 | External ref | Error | TEMPLATE_EXTERNAL_REF | S10.3 |
## 5-6. See template integration points in 10.2.
