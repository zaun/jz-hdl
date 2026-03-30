# Test Plan: 10.2 Template Definition
**Specification Reference:** Section 10.2 of jz-hdl-specification.md

## 1. Objective
Verify @template syntax, parameter list, module-scoped and file-scoped placement, identifier rules, and duplicate detection.

## 2. Test Scenarios

### 2.1 Happy Path
1. Module-scoped template with parameters — valid
2. File-scoped template (outside @module) — valid
3. Template with zero parameters — valid
4. Multiple templates with distinct names in same scope — valid

### 2.2 Error Cases
1. Duplicate template name in same scope — Error (TEMPLATE_DUP_NAME)
2. Duplicate parameter name in template definition — Error (TEMPLATE_DUP_PARAM)

### 2.3 Edge Cases
1. Same template name in different module scopes — valid (separate scopes)
2. Template name matches a wire/register name — valid (different namespaces)

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Duplicate template name | Error | TEMPLATE_DUP_NAME | S10.2 |
| 2 | Duplicate param name | Error | TEMPLATE_DUP_PARAM | S10.2 |

## 4. Existing Validation Tests
| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `10_2_HAPPY_PATH-template_definition_ok.jz` | — | Happy-path: valid template definitions (module-scoped, file-scoped, zero params) |
| `10_2_TEMPLATE_DUP_NAME-duplicate_template_names.jz` | TEMPLATE_DUP_NAME | Two templates with the same name in one scope |
| `10_2_TEMPLATE_DUP_PARAM-duplicate_param_names.jz` | TEMPLATE_DUP_PARAM | Repeated parameter identifier in definition |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| TEMPLATE_DUP_NAME | Duplicate template name in the same scope | `10_2_TEMPLATE_DUP_NAME-duplicate_template_names.jz` |
| TEMPLATE_DUP_PARAM | Duplicate parameter name in template definition | `10_2_TEMPLATE_DUP_PARAM-duplicate_param_names.jz` |

### 5.2 Rules Not Tested

All rules for this section are tested.
