# Test Plan: 10.1 Template Purpose
**Specification Reference:** Section 10.1 of jz-hdl-specification.md

## 1. Objective
Verify templates provide compile-time reusable logic blocks that expand inline without introducing new hardware structure. Templates expand before semantic analysis.

## 2. Test Scenarios

### 2.1 Happy Path
1. Simple template defined and applied once — expands correctly
2. Template reduces code duplication across multiple call sites
3. Template with parameters substituted at expansion time

### 2.2 Error Cases
1. Template creating modules — Error (covered by TEMPLATE_FORBIDDEN_DIRECTIVE in 10.4)
2. Template with external references — Error (covered by TEMPLATE_EXTERNAL_REF in 10.3)

### 2.3 Edge Cases
1. Template applied zero times (count=0) — no-op expansion
2. Template body contains only comments — valid but no effect

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Simple template define + apply | Clean expansion | — | S10.1 overview |
| 2 | Template reducing duplication | Clean expansion | — | S10.1 overview |

## 4. Existing Validation Tests
No validation tests specific to 10.1. Template behavior is tested through sections 10.2-10.8.

## 5. Rules Matrix

### 5.1 Rules Tested
No rules are defined specifically for section 10.1. This section provides conceptual overview only.

### 5.2 Rules Not Tested
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | — | All template rules are defined in sections 10.2-10.8 |
