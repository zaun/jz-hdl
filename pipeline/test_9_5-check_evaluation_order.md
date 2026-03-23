# Test Plan: 9.5 @check Evaluation Order
**Specification Reference:** Section 9.5 of jz-hdl-specification.md
## 1. Objective
Verify @check evaluates after CONST, CONFIG, and OVERRIDE resolution.
## 2. Instrumentation Strategy
- **Span: `sem.check_order`** — attributes: `resolved_consts`, `resolved_configs`.
## 3. Test Scenarios
### 3.1 Happy Path
1. @check references CONST defined earlier
2. @check references CONFIG from project
3. @check sees OVERRIDE values
### 3.3 Negative Testing
1. @check references undefined CONST — Error
## 4-6. Cross-reference with Section 4.3 (CONST) and 6.3 (CONFIG).
