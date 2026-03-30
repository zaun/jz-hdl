# Test Plan: 8.3 Global Semantics

**Specification Reference:** Section 8.3 of jz-hdl-specification.md

## 1. Objective
Verify that `@global` blocks create namespace roots (`<global_name>.<const_id>`), enforce uniqueness of `const_id` within a single block, reject references to undeclared global constants, and reject global references in forbidden contexts (CONFIG, CONST, OVERRIDE blocks).

## 2. Test Scenarios

### 2.1 Happy Path
1. Reference via `ISA.INST_ADD` resolves correctly in SYNCHRONOUS block
2. Identically named constants in different `@global` blocks (`ISA.INST_ADD` vs `FLAGS.INST_ADD`) coexist without conflict
3. Multiple blocks with disjoint namespaces, each used in different modules
4. Valid reads of globals in both ASYNCHRONOUS and SYNCHRONOUS contexts

### 2.2 Error Cases
1. Duplicate `const_id` (`OP_ADD`) in same `@global OPCODES` block produces GLOBAL_CONST_NAME_DUPLICATE (1 trigger)
2. Multiple duplicate pairs (`IDLE` and `ACTIVE`) in same `@global STATUS` block produces GLOBAL_CONST_NAME_DUPLICATE (2 triggers)
3. Reference to undefined constant in valid namespace (`OPCODES.MUL`) produces GLOBAL_CONST_USE_UNDECLARED
4. Reference to another undefined constant (`OPCODES.XOR`) in a different module produces GLOBAL_CONST_USE_UNDECLARED
5. Global reference in CONFIG expression (`WIDTH = VALS.MAGIC;`) produces GLOBAL_USED_WHERE_FORBIDDEN (+ CONFIG_INVALID_EXPR_TYPE)
6. Global reference in module CONST initializer (`MY_VAL = VALS.MAGIC;`) produces GLOBAL_USED_WHERE_FORBIDDEN
7. Global reference in OVERRIDE expression (`SIZE = VALS.MAGIC;`) produces GLOBAL_USED_WHERE_FORBIDDEN

### 2.3 Edge Cases
1. Same `const_id` name across different blocks (e.g., `OPCODES.OP_ADD` and `ALU.OP_ADD`) -- no collision
2. Valid global reads on RHS coexist with forbidden-context uses in the same file

## 3. Input/Output Matrix
| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Duplicate const in block | `OP_ADD = 4'b0011;` (second) | GLOBAL_CONST_NAME_DUPLICATE | error |
| 2 | Multiple dup pairs in block | `IDLE = 3'b010;`, `ACTIVE = 3'b100;` | GLOBAL_CONST_NAME_DUPLICATE | error |
| 3 | Undefined const in valid namespace (sync) | `OPCODES.MUL` | GLOBAL_CONST_USE_UNDECLARED | error |
| 4 | Undefined const in valid namespace (sync) | `OPCODES.XOR` | GLOBAL_CONST_USE_UNDECLARED | error |
| 5 | Global in CONFIG expression | `WIDTH = VALS.MAGIC;` | GLOBAL_USED_WHERE_FORBIDDEN | error |
| 6 | Global in CONST initializer | `MY_VAL = VALS.MAGIC;` | GLOBAL_USED_WHERE_FORBIDDEN | error |
| 7 | Global in OVERRIDE expression | `SIZE = VALS.MAGIC;` | GLOBAL_USED_WHERE_FORBIDDEN | error |

## 4. Existing Validation Tests
| Test File | Rule Tested | Triggers |
|-----------|-------------|----------|
| `8_3_HAPPY_PATH-global_semantics_ok.jz` | (none -- clean) | 0 diagnostics |
| `8_3_GLOBAL_CONST_NAME_DUPLICATE-dup_const_in_block.jz` | GLOBAL_CONST_NAME_DUPLICATE | 3 triggers |
| `8_3_GLOBAL_CONST_USE_UNDECLARED-undeclared_global_ref.jz` | GLOBAL_CONST_USE_UNDECLARED | 2 triggers |
| `8_3_GLOBAL_USED_WHERE_FORBIDDEN-forbidden_context.jz` | GLOBAL_USED_WHERE_FORBIDDEN | 3 triggers |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| GLOBAL_CONST_NAME_DUPLICATE | error | `8_3_GLOBAL_CONST_NAME_DUPLICATE-dup_const_in_block.jz` |
| GLOBAL_CONST_USE_UNDECLARED | error | `8_3_GLOBAL_CONST_USE_UNDECLARED-undeclared_global_ref.jz` |
| GLOBAL_USED_WHERE_FORBIDDEN | error | `8_3_GLOBAL_USED_WHERE_FORBIDDEN-forbidden_context.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| -- | -- | All section 8.3 rules have validation tests |
