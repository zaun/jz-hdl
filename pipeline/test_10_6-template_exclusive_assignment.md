# Test Plan: 10.6 Template Exclusive Assignment Compatibility
**Specification Reference:** Section 10.6 of jz-hdl-specification.md
## 1. Objective
Verify template expansion does not bypass exclusive assignment rule. Multiple @apply to same identifier must be structurally exclusive.
## 2. Instrumentation Strategy
- **Span: `sem.template_assign`** — attributes: `expanded_assignments`, `conflicts`.
## 3. Test Scenarios
### 3.1 Happy Path
1. @apply in exclusive IF/ELSE branches — valid
2. @apply with IDX targeting non-overlapping slices
### 3.3 Negative Testing
1. Two @apply to same wire in same path — Error
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Two @apply same target | Error | ASSIGN_MULTI_DRIVER | S1.5 after expansion |
## 5-6. See 10.2 and 1.5 plans.
