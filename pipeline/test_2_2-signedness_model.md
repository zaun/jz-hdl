# Test Plan: 2.2 Signedness Model

**Spec Ref:** Section 2.2 of jz-hdl-specification.md

## 1. Objective

Verify that all signals, literals, and operators are treated as unsigned by default, that no `signed` keyword exists in the language, and that signed arithmetic requires explicit use of `sadd` and `smul` intrinsic operators (Section 5.5).

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Unsigned addition | `8'd200 + 8'd55` = `8'd255` (unsigned) |
| 2 | Unsigned subtraction | `8'd10 - 8'd20` wraps unsigned |
| 3 | Unsigned comparison | `8'd200 > 8'd100` = true (unsigned, no sign issue) |
| 4 | sadd intrinsic | `sadd(a, b)` produces signed result with carry |
| 5 | smul intrinsic | `smul(a, b)` produces signed product |
| 6 | All operators unsigned | `+`, `-`, `*`, `/`, `%`, `<`, `>`, `<=`, `>=` all unsigned |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | No signed keyword | No `signed` keyword exists; using it would be a parse error |
| 2 | Width mismatch on binary op | `8'd10 + 4'd5` triggers TYPE_BINOP_WIDTH_MISMATCH (width matching enforced for all binary operators) |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Max unsigned value | `8'd255 + 8'd1` wraps to 0 (unsigned overflow) |
| 2 | MSB = 1 in comparison | `8'd128 > 8'd127` = true (unsigned, not negative) |
| 3 | Two's complement interpretation | Value `8'b1111_1111` is 255 unsigned, not -1 |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `8'd200 + 8'd55` | `8'd255` (unsigned) | -- | -- | Normal unsigned arithmetic |
| 2 | `8'd200 + 8'd56` | `8'd0` (unsigned wrap) | -- | -- | Unsigned overflow wraps |
| 3 | `sadd(8'd200, 8'd56)` | 9-bit signed result | -- | -- | Intrinsic signed add |
| 4 | `8'd128 > 8'd127` | True | -- | -- | Unsigned comparison |
| 5 | `8'd10 + 4'd5` | Error: width mismatch | TYPE_BINOP_WIDTH_MISMATCH | error | Binary op width matching |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| -- | -- | No validation tests directly mapped to this section |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| TYPE_BINOP_WIDTH_MISMATCH | error | Binary operator requires equal operand widths | Error 2 |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| TYPE_BINOP_WIDTH_MISMATCH | error | Primarily tested in section 2.3; no dedicated 2.2 validation test exists |
