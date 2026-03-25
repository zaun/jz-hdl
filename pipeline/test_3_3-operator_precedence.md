# Test Plan: 3.3 Operator Precedence

**Spec Ref:** Section 3.3 of jz-hdl-specification.md

## 1. Objective

Verify that the parser respects the 15-level operator precedence hierarchy: parentheses (highest) -> concatenation -> unary NOT -> unary arithmetic -> multiply/divide/modulus -> shifts -> add/subtract -> relational -> equality -> bitwise AND -> bitwise XOR -> bitwise OR -> logical AND -> logical OR -> ternary (lowest).

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected Parse Tree |
|---|-----------|-------|---------------------|
| 1 | Multiply before add | `a + b * c` | `a + (b * c)` |
| 2 | Add before compare | `a + b == c + d` | `(a + b) == (c + d)` |
| 3 | Compare before AND | `a == b & c == d` | `(a == b) & (c == d)` |
| 4 | AND before XOR | `a & b ^ c & d` | `(a & b) ^ (c & d)` |
| 5 | XOR before OR | `a ^ b \| c ^ d` | `(a ^ b) \| (c ^ d)` |
| 6 | OR before logical AND | `a \| b && c \| d` | `(a \| b) && (c \| d)` |
| 7 | Logical AND before OR | `a && b \|\| c && d` | `(a && b) \|\| (c && d)` |
| 8 | Logical OR before ternary | `a \|\| b ? c : d` | `(a \|\| b) ? c : d` |
| 9 | Shift before add | `a << b + c` | Verify correct binding per precedence level 6 vs 7 |
| 10 | Parentheses override | `(a + b) * c` | `(a + b) * c` |
| 11 | Concatenation high prec | `{a, b} + {c, d}` | `({a,b}) + ({c,d})` |
| 12 | Unary NOT before binary | `~a & b` | `(~a) & b` |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Missing operand | `a + ` -- parse error |
| 2 | Double operator | `a ++ b` -- parse error (not valid in JZ-HDL) |
| 3 | Mismatched parens | `(a + b` -- parse error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Deeply nested parens | `((((a + b))))` -- valid |
| 2 | All 15 levels in one expr | Expression using one operator from each level |
| 3 | Right-associative ternary | `a ? b : c ? d : e` -> `a ? b : (c ? d : e)` |
| 4 | Left-associative arithmetic | `a - b - c` -> `(a - b) - c` |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|-----------------|---------|-------|
| 1 | `a + b * c` | AST: `add(a, mul(b,c))` | -- | Mul binds tighter than add |
| 2 | `a == b & c` | AST: `bitand(eq(a,b), c)` | -- | Eq binds tighter than bitand |
| 3 | `a ? b : c ? d : e` | AST: `ternary(a, b, ternary(c, d, e))` | -- | Right-associative ternary |

## 4. Existing Validation Tests

None directly mapped. Precedence is verified by AST structure rather than diagnostic rules.

## 5. Rules Matrix

### 5.1 Rules Tested

None specific -- operator precedence is verified by AST structure comparison, not diagnostic rules.

### 5.2 Rules Not Tested

All rules for this section are tested. S3.3 defines parse behavior, not diagnostic rules; expression-level diagnostic rules are tested in S3.1 and S3.2 test plans.
