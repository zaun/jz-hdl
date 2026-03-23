# Test Plan: 2.3 Bit-Width Constraints

**Specification Reference:** Section 2.3 of jz-hdl-specification.md

## 1. Objective

Verify the Strict Matching Rule: for binary operators (`+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, `==`, `!=`, `<`, `>`, `<=`, `>=`), operands must have identical bit-widths. Also verify that exceptions exist for operators with specialized width rules (Section 3.2).

## 2. Instrumentation Strategy

- **Span: `sem.width_check`** — Trace width validation per binary expression; attributes: `operator`, `lhs_width`, `rhs_width`, `match`.
- **Event: `width.mismatch`** — Fires when operand widths differ; includes `operator`, `lhs_width`, `rhs_width`.
- **Coverage Hook:** Ensure every binary operator type is tested with both matching and mismatching widths.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Same width addition | `8'd10 + 8'd20` — both 8-bit |
| 2 | Same width comparison | `4'b1010 == 4'b1100` — both 4-bit |
| 3 | Same width bitwise | `8'hFF & 8'h0F` — both 8-bit |
| 4 | Same width subtraction | `16'd1000 - 16'd500` |
| 5 | All binary operators same width | Each of `+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, `==`, `!=`, `<`, `>`, `<=`, `>=` with matching widths |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit operands | `1'b0 + 1'b1` — valid |
| 2 | Wide operands | `256'd0 + 256'd1` — valid |
| 3 | Ternary same width | `cond ? 8'd1 : 8'd2` — both arms 8-bit |
| 4 | Shift operator (exception) | `8'd1 << 3'd2` — LHS and shift amount may differ per S3.2 |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Width mismatch addition | `8'd10 + 4'd5` — 8 vs 4 |
| 2 | Width mismatch comparison | `8'd10 == 4'd10` — 8 vs 4 |
| 3 | Width mismatch bitwise | `8'hFF & 4'hF` — 8 vs 4 |
| 4 | Ternary arm mismatch | `cond ? 8'd1 : 4'd1` — 8 vs 4 |
| 5 | Assignment width mismatch | `out [8] <= 4'd5;` — 8-bit target, 4-bit source |
| 6 | Concatenation width mismatch | `{ a, b }` assigned to wrong width target |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `8'd10 + 4'd5` | Error: width mismatch | WIDTH_BINOP_MISMATCH | 8 ≠ 4 |
| 2 | `8'd10 == 4'd10` | Error: width mismatch | WIDTH_BINOP_MISMATCH | Comparison width |
| 3 | `cond ? 8'd1 : 4'd1` | Error: ternary mismatch | WIDTH_TERNARY_MISMATCH | Arms differ |
| 4 | `out[8] <= 4'd5` | Error: assignment mismatch | WIDTH_ASSIGN_MISMATCH_NO_EXT | Target ≠ source |
| 5 | `8'd10 + 8'd20` | Valid | — | Matching widths |
| 6 | `{4'd1, 4'd2}` to 8-bit | Valid | — | Concat width = 4+4 = 8 |
| 7 | `{4'd1, 4'd2}` to 6-bit | Error | ASSIGN_CONCAT_WIDTH_MISMATCH | 8 ≠ 6 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_width.c` | Width validation for all expressions | Feed expression AST with known widths |
| `driver_operators.c` | Operator-specific width rules | Verify each operator enforces matching |
| `driver_expr.c` | Expression tree traversal | Provide crafted expression trees |
| `sem_type.c` | Type/width resolution | Verify width attributes on AST nodes |
| `diagnostic.c` | Error collection | Verify correct rule IDs |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| WIDTH_BINOP_MISMATCH | Binary operator operands differ in width | Neg 1, 2, 3 |
| WIDTH_TERNARY_MISMATCH | Ternary arms differ in width | Neg 4 |
| WIDTH_ASSIGN_MISMATCH_NO_EXT | Assignment target/source width mismatch | Neg 5 |
| ASSIGN_CONCAT_WIDTH_MISMATCH | Concatenation width mismatch in assignment | Neg 6 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| WIDTH_SHIFT_RULES | S2.3/S3.2 "specialized width rules" | Shift operators have different width rules; verify they are exempt from strict matching |
| WIDTH_MULTIPLY_RULES | S3.2 | Multiply/divide/modulus have specialized rules; need to verify exemption |
