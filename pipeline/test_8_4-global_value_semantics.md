# Test Plan: 8.4 Global Value Semantics

**Specification Reference:** Section 8.4 of jz-hdl-specification.md

## 1. Objective
Verify global constant usage in expressions, assignments, conditionals, and concatenations, including width matching enforcement.

## 2. Test Scenarios

### 2.1 Happy Path
1. RHS of ASYNC assignment: `opcode <= ISA.INST_ADD;`
2. In expression: `ISA.INST_ADD == opcode`
3. In concatenation: `{ISA.INST_ADD, 15'b0}`
4. In SYNCHRONOUS assignment: `reg <= ISA.INST_ADD;`

### 2.2 Error Cases
1. Width mismatch without modifier produces ASSIGN_WIDTH_NO_MODIFIER error

### 2.3 Edge Cases
1. Global constant used in comparison where widths differ but comparison is legal
2. Global constant as both operands in an expression

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Width mismatch without modifier | Error | ASSIGN_WIDTH_NO_MODIFIER | S2.3 width rules apply |

## 4. Existing Validation Tests
| Test File | Rule Tested |
|-----------|-------------|
| `8_4_ASSIGN_WIDTH_NO_MODIFIER-global_width_mismatch.jz` | ASSIGN_WIDTH_NO_MODIFIER |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| ASSIGN_WIDTH_NO_MODIFIER | error | `8_4_ASSIGN_WIDTH_NO_MODIFIER-global_width_mismatch.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| — | — | All section 8.4 rules covered |
