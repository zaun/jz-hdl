# Test Plan: 10.2 Template Definition
**Specification Reference:** Section 10.2 of jz-hdl-specification.md
## 1. Objective
Verify @template syntax, parameter list, module-scoped and file-scoped placement, identifier rules.
## 2. Instrumentation Strategy
- **Span: `parser.template`** — attributes: `template_id`, `param_count`, `scope`.
## 3. Test Scenarios
### 3.1 Happy Path
1. Module-scoped template with parameters
2. File-scoped template (outside @module)
3. Zero parameters
### 3.3 Negative Testing
1. Duplicate template name — Error (TEMPLATE_DUP_NAME)
2. Duplicate parameter name — Error (TEMPLATE_DUP_PARAM)
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Dup template name | Error | TEMPLATE_DUP_NAME | S10.2 |
| 2 | Dup param name | Error | TEMPLATE_DUP_PARAM | S10.2 |
## 5. Integration Points
| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_template.c` | Template parsing | Token stream |
| `template_expand.c` | Template expansion | Unit test |
## 6. Rules Matrix
### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| TEMPLATE_DUP_NAME | Duplicate template name | Neg 1 |
| TEMPLATE_DUP_PARAM | Duplicate parameter name | Neg 2 |
### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | — | — |
