# Test Plan: 10.1 Template Purpose
**Specification Reference:** Section 10.1 of jz-hdl-specification.md
## 1. Objective
Verify templates provide compile-time reusable logic blocks that expand inline without introducing new hardware structure. Templates expand before semantic analysis.
## 2. Instrumentation Strategy
- **Span: `template.expand`** — attributes: `template_id`, `expansion_count`.
## 3. Test Scenarios
### 3.1 Happy Path
1. Simple template defined and applied
2. Template reduces code duplication
### 3.3 Negative Testing
1. Template creating modules — Error
## 4-6. See Sections 10.2-10.8 for detailed plans.
