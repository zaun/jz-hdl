# Test Plan: 2.3 Bit-Width Constraints

**Spec Ref:** Section 2.3 of jz-hdl-specification.md

## 1. Objective

Verify the Strict Matching Rule for binary operators (`+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, `==`, `!=`, `<`, `>`, `<=`, `>=`): operands must have identical bit-widths. Also verify assignment width matching, ternary arm width matching, and concatenation width matching.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Same width addition | `8'd10 + 8'd20` -- both 8-bit |
| 2 | Same width comparison | `4'b1010 == 4'b1100` -- both 4-bit |
| 3 | Same width bitwise | `8'hFF & 8'h0F` -- both 8-bit |
| 4 | Same width subtraction | `16'd1000 - 16'd500` |
| 5 | All binary operators same width | Each of `+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, `==`, `!=`, `<`, `>`, `<=`, `>=` with matching widths |
| 6 | Ternary same width | `cond ? 8'd1 : 8'd2` -- both arms 8-bit |
| 7 | Concat matches target | `{4'd1, 4'd2}` assigned to 8-bit target |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Width mismatch addition | `8'd10 + 4'd5` -- 8 vs 4, TYPE_BINOP_WIDTH_MISMATCH |
| 2 | Width mismatch comparison | `8'd10 == 4'd10` -- 8 vs 4, TYPE_BINOP_WIDTH_MISMATCH |
| 3 | Width mismatch bitwise | `8'hFF & 4'hF` -- 8 vs 4, TYPE_BINOP_WIDTH_MISMATCH |
| 4 | Ternary arm mismatch | `cond ? 8'd1 : 4'd1` -- TERNARY_BRANCH_WIDTH_MISMATCH |
| 5 | Assignment width mismatch | `out [8] <= 4'd5;` -- ASSIGN_WIDTH_NO_MODIFIER |
| 6 | Assignment no modifier | Width mismatch without `<=z`/`<=s`/`<=t` -- ASSIGN_WIDTH_NO_MODIFIER |
| 7 | Concatenation width mismatch | `{4'd1, 4'd2}` assigned to 6-bit target -- ASSIGN_CONCAT_WIDTH_MISMATCH |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit operands | `1'b0 + 1'b1` -- valid |
| 2 | Wide operands | `256'd0 + 256'd1` -- valid |
| 3 | Shift operator (exception) | `8'd1 << 3'd2` -- LHS and shift amount may differ per S3.2 |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `8'd10 + 4'd5` | Error: width mismatch | TYPE_BINOP_WIDTH_MISMATCH | error | 8 != 4 |
| 2 | `8'd10 == 4'd10` | Error: width mismatch | TYPE_BINOP_WIDTH_MISMATCH | error | Comparison width |
| 3 | `cond ? 8'd1 : 4'd1` | Error: ternary mismatch | TERNARY_BRANCH_WIDTH_MISMATCH | error | Arms differ |
| 4 | `out[8] <= 4'd5` | Error: assignment mismatch | ASSIGN_WIDTH_NO_MODIFIER | error | Target != source, no modifier |
| 5 | `8'd10 + 8'd20` | Valid | -- | -- | Matching widths |
| 6 | `{4'd1, 4'd2}` to 8-bit | Valid | -- | -- | Concat width = 4+4 = 8 |
| 7 | `{4'd1, 4'd2}` to 6-bit | Error: concat mismatch | ASSIGN_CONCAT_WIDTH_MISMATCH | error | 8 != 6 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 2_3_BIT_WIDTH_CONSTRAINTS-valid_operations_ok.jz | -- | Happy path: valid bit-width operations accepted |
| 2_3_ASSIGN_CONCAT_WIDTH_MISMATCH-concat_width_mismatch.jz | ASSIGN_CONCAT_WIDTH_MISMATCH | Concatenation width does not match target |
| 2_3_ASSIGN_WIDTH_NO_MODIFIER-assign_width_mismatch.jz | ASSIGN_WIDTH_NO_MODIFIER, WIDTH_ASSIGN_MISMATCH_NO_EXT | Assignment width mismatch without extend/truncate modifier |
| 2_3_TERNARY_BRANCH_WIDTH_MISMATCH-ternary_arm_widths.jz | TERNARY_BRANCH_WIDTH_MISMATCH | Ternary true/false arms have mismatched widths |
| 2_3_TYPE_BINOP_WIDTH_MISMATCH-mismatched_operand_widths.jz | TYPE_BINOP_WIDTH_MISMATCH | Binary operator operands differ in width |
| 2_3_WIDTH_NONPOSITIVE_OR_NONINT-zero_width.jz | WIDTH_NONPOSITIVE_OR_NONINT | Declared width of zero (non-positive) |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| TYPE_BINOP_WIDTH_MISMATCH | error | Binary operator requires equal operand widths | Error 1, 2, 3; 2_3_TYPE_BINOP_WIDTH_MISMATCH-mismatched_operand_widths.jz |
| WIDTH_ASSIGN_MISMATCH_NO_EXT | error | S4.10/S5.0/S5.1 Width mismatch in assignment without extend/slice | 2_3_ASSIGN_WIDTH_NO_MODIFIER-assign_width_mismatch.jz (same test file triggers both WIDTH_ASSIGN_MISMATCH_NO_EXT and ASSIGN_WIDTH_NO_MODIFIER) |
| ASSIGN_WIDTH_NO_MODIFIER | error | S4.10/S5.0 Width mismatch without `<=z`/`<=s`/`<=t` modifier | Error 5, 6; 2_3_ASSIGN_WIDTH_NO_MODIFIER-assign_width_mismatch.jz |
| ASSIGN_CONCAT_WIDTH_MISMATCH | error | Concatenation width sum does not match paired expression | Error 7; 2_3_ASSIGN_CONCAT_WIDTH_MISMATCH-concat_width_mismatch.jz |
| TERNARY_BRANCH_WIDTH_MISMATCH | error | Ternary true/false branches have mismatched widths | Error 4; 2_3_TERNARY_BRANCH_WIDTH_MISMATCH-ternary_arm_widths.jz |
| WIDTH_NONPOSITIVE_OR_NONINT | error | Declared width <= 0 or not an integer | 2_3_WIDTH_NONPOSITIVE_OR_NONINT-zero_width.jz; also tested in test_2_2-signedness_model.md |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| — | — | All mapped rules have validation test coverage |
