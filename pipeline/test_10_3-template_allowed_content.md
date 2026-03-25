# Test Plan: 10.3 Template Allowed Content
**Specification Reference:** Section 10.3 of jz-hdl-specification.md

## 1. Objective
Verify allowed content inside templates: directional/alias assignments, IF/ELIF/ELSE, SELECT/CASE, expressions, and @scratch wires. Verify that external signal references and invalid @scratch usage are rejected.

## 2. Test Scenarios

### 2.1 Happy Path
1. Assignments using parameters — valid
2. IF/ELSE inside template — valid
3. @scratch wire declaration and use inside template — valid
4. CONST/CONFIG/@global references without passing as params — valid (compile-time constants allowed)
5. SELECT/CASE inside template — valid

### 2.2 Error Cases
1. External signal reference not passed as param — Error (TEMPLATE_EXTERNAL_REF)
2. @scratch declared outside template body — Error (TEMPLATE_SCRATCH_OUTSIDE)
3. @scratch width not a constant expression — Error (TEMPLATE_SCRATCH_WIDTH_INVALID)

### 2.3 Edge Cases
1. @scratch with widthof() in width expression — valid if resolvable at compile time
2. @scratch with width of 1 — minimal valid width
3. Multiple @scratch declarations in same template — valid if distinct names

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | External signal ref in template | Error | TEMPLATE_EXTERNAL_REF | S10.3 |
| 2 | @scratch outside template | Error | TEMPLATE_SCRATCH_OUTSIDE | S10.3 |
| 3 | @scratch with non-constant width | Error | TEMPLATE_SCRATCH_WIDTH_INVALID | S10.3 |

## 4. Existing Validation Tests
| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `10_3_HAPPY_PATH-template_allowed_content_ok.jz` | — | Happy-path: assignments, IF/ELSE, @scratch, CONST refs inside template |
| `10_3_TEMPLATE_EXTERNAL_REF-external_signal_reference.jz` | TEMPLATE_EXTERNAL_REF | References signal not passed as parameter |
| `10_3_TEMPLATE_SCRATCH_OUTSIDE-scratch_outside_template.jz` | TEMPLATE_SCRATCH_OUTSIDE | @scratch used outside a template body |
| `10_3_TEMPLATE_SCRATCH_WIDTH_INVALID-scratch_width_not_constant.jz` | TEMPLATE_SCRATCH_WIDTH_INVALID | @scratch width is not a positive integer constant |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| TEMPLATE_EXTERNAL_REF | Identifier in template body must be a parameter, @scratch wire, or compile-time constant | `10_3_TEMPLATE_EXTERNAL_REF-external_signal_reference.jz` |
| TEMPLATE_SCRATCH_OUTSIDE | @scratch may only appear inside a @template body | `10_3_TEMPLATE_SCRATCH_OUTSIDE-scratch_outside_template.jz` |
| TEMPLATE_SCRATCH_WIDTH_INVALID | @scratch width must be a positive integer constant expression | `10_3_TEMPLATE_SCRATCH_WIDTH_INVALID-scratch_width_not_constant.jz` |

### 5.2 Rules Not Tested
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | — | All S10.3 rules covered |
