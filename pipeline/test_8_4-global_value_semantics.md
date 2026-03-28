# Test Plan: 8.4 Global Value Semantics

**Specification Reference:** Section 8.4 of jz-hdl-specification.md

## 1. Objective
Verify that global constants may be used anywhere a value expression is permitted (RHS of ASYNC/SYNC assignments, in expressions, concatenations, conditionals), and that standard width rules apply -- same-width assignment succeeds, width mismatch without a modifier produces an error.

## 2. Test Scenarios

### 2.1 Happy Path
1. Global constant as RHS of ASYNCHRONOUS `=` assignment with matching width (`dout_a = ISA.INST_ADD;`)
2. Global constant as RHS of SYNCHRONOUS `<=` assignment with matching width (`r <= ISA.INST_SUB;`)
3. Global constant used in helper module and top module simultaneously
4. Global constant with exact 8-bit match in both ASYNC and SYNC contexts (`FLAGS.ACTIVE`, `FLAGS.DONE`)

### 2.2 Error Cases
1. Global too narrow for target in ASYNC `=` assignment (4-bit global to 8-bit target) produces ASSIGN_WIDTH_NO_MODIFIER
2. Global too wide for target in SYNC `<=` assignment (16-bit global to 8-bit target) produces ASSIGN_WIDTH_NO_MODIFIER
3. Global too wide for target in ASYNC `<=` alias (16-bit global to 8-bit target) produces ASSIGN_WIDTH_NO_MODIFIER
4. Global too narrow for target in SYNC `<=` assignment (4-bit global to 8-bit target) produces ASSIGN_WIDTH_NO_MODIFIER
5. Width mismatch in top-module ASYNC block produces ASSIGN_WIDTH_NO_MODIFIER
6. Width mismatch in top-module SYNC block produces ASSIGN_WIDTH_NO_MODIFIER

### 2.3 Edge Cases
1. Global constant used in comparison where widths differ but comparison is legal
2. Global constant as both operands in an expression
3. Same-width global alongside a mismatched global in the same module -- only the mismatch triggers

## 3. Input/Output Matrix
| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | 4-bit global to 8-bit target (ASYNC =) | `out_a = CMD.NARROW;` | ASSIGN_WIDTH_NO_MODIFIER | error |
| 2 | 16-bit global to 8-bit target (SYNC <=) | `r <= CMD.WIDE;` | ASSIGN_WIDTH_NO_MODIFIER | error |
| 3 | 16-bit global to 8-bit target (ASYNC <=) | `out_a <= CMD.WIDE;` | ASSIGN_WIDTH_NO_MODIFIER | error |
| 4 | 4-bit global to 8-bit target (SYNC <=) | `r <= CMD.NARROW;` | ASSIGN_WIDTH_NO_MODIFIER | error |
| 5 | 4-bit global to 8-bit target (top ASYNC =) | `out_c = CMD.NARROW;` | ASSIGN_WIDTH_NO_MODIFIER | error |
| 6 | 16-bit global to 8-bit target (top SYNC <=) | `r <= CMD.WIDE;` | ASSIGN_WIDTH_NO_MODIFIER | error |

## 4. Existing Validation Tests
| Test File | Rule Tested | Triggers |
|-----------|-------------|----------|
| `8_4_HAPPY_PATH-global_value_semantics_ok.jz` | (none -- clean) | 0 diagnostics |
| `8_4_ASSIGN_WIDTH_NO_MODIFIER-global_width_mismatch.jz` | ASSIGN_WIDTH_NO_MODIFIER | 6 triggers |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| ASSIGN_WIDTH_NO_MODIFIER | error | `8_4_ASSIGN_WIDTH_NO_MODIFIER-global_width_mismatch.jz` |

### 5.2 Rules Not Tested

All rules for this section are tested.
