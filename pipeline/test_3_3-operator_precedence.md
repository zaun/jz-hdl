# Test Plan: 3.3 Operator Precedence

**Specification Reference:** Section 3.3 of jz-hdl-specification.md

## 1. Objective

Verify that the parser respects the 15-level operator precedence hierarchy: parentheses (highest) → concatenation → unary NOT → unary arithmetic → multiply/divide/modulus → shifts → add/subtract → relational → equality → bitwise AND → bitwise XOR → bitwise OR → logical AND → logical OR → ternary (lowest).

## 2. Instrumentation Strategy

- **Span: `parser.precedence`** — Trace operator binding during expression parsing; attributes: `operator`, `precedence_level`, `associativity`.
- **Coverage Hook:** Ensure at least one expression combining operators from adjacent precedence levels is tested.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Input | Expected Parse Tree |
|---|-----------|-------|-------------------|
| 1 | Multiply before add | `a + b * c` | `a + (b * c)` |
| 2 | Add before compare | `a + b == c + d` | `(a + b) == (c + d)` |
| 3 | Compare before AND | `a == b & c == d` | `(a == b) & (c == d)` |
| 4 | AND before XOR | `a & b ^ c & d` | `(a & b) ^ (c & d)` |
| 5 | XOR before OR | `a ^ b \| c ^ d` | `(a ^ b) \| (c ^ d)` |
| 6 | OR before logical AND | `a \| b && c \| d` | `(a \| b) && (c \| d)` |
| 7 | Logical AND before OR | `a && b \|\| c && d` | `(a && b) \|\| (c && d)` |
| 8 | Logical OR before ternary | `a \|\| b ? c : d` | `(a \|\| b) ? c : d` |
| 9 | Shift before add | `a << b + c` → `a << (b + c)` ... actually shifts before add means `(a << b) + c` | Verify correct: shifts (level 6) before add (level 7) means `(a << b) + c` |
| 10 | Parentheses override | `(a + b) * c` | `(a + b) * c` |
| 11 | Concatenation high prec | `{a, b} + {c, d}` | `({a,b}) + ({c,d})` |
| 12 | Unary NOT before binary | `~a & b` | `(~a) & b` |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Deeply nested parens | `((((a + b))))` — valid |
| 2 | All 15 levels in one expr | Expression using one operator from each level |
| 3 | Right-associative ternary | `a ? b : c ? d : e` → `a ? b : (c ? d : e)` |
| 4 | Left-associative arithmetic | `a - b - c` → `(a - b) - c` |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Missing operand | `a + ` — parse error |
| 2 | Double operator | `a ++ b` — parse error (not valid in JZ-HDL) |
| 3 | Mismatched parens | `(a + b` — parse error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `a + b * c` | AST: `add(a, mul(b,c))` | — | Mul binds tighter than add |
| 2 | `a == b & c` | AST: `bitand(eq(a,b), c)` | — | Eq binds tighter than bitand |
| 3 | `a ? b : c ? d : e` | AST: `ternary(a, b, ternary(c, d, e))` | — | Right-associative ternary |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_expressions.c` | Precedence climbing / Pratt parser | Feed token streams, verify AST structure |
| `ast.c` | AST node construction | Verify tree shape |
| `ast_json.c` | AST serialization for inspection | Serialize and compare against expected JSON |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| — | No specific rule IDs for precedence | Verified by AST structure comparison |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| PRECEDENCE_AMBIGUOUS | S3.3 | No warning for expressions that may confuse readers (e.g., mixing bitwise and logical without parens) |
