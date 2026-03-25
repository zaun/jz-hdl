# Test Plan: 1.4 Comments

**Specification Reference:** Section 1.4 of jz-hdl-specification.md

## 1. Objective

Verify that the lexer correctly handles line comments (`//`), block comments (`/* */`), rejects nested block comments, rejects comments appearing inside tokens, and allows comments anywhere whitespace is permitted.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Line comment | `WIRE { x [1]; } // comment` | Comment ignored |
| 2 | Block comment | `WIRE { /* comment */ x [1]; }` | Comment ignored |
| 3 | Multi-line block | `/* line1\nline2 */` | Comment ignored |
| 4 | Comment between tokens | `WIRE /* sep */ { x [1]; }` | Valid, comment is whitespace |
| 5 | Multiple line comments | `// a\n// b\nWIRE { x [1]; }` | All comments ignored |
| 6 | Empty block comment | `/**/` | Valid, ignored |
| 7 | Block comment with stars | `/* ** star ** */` | Valid, ignored |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Nested block comment | `/* outer /* inner */ */` | Error | COMMENT_NESTED_BLOCK |
| 2 | Comment inside identifier | `my/*comment*/Wire` | Error | COMMENT_IN_TOKEN |
| 3 | Comment inside number | `8'b11/**/00` | Error | COMMENT_IN_TOKEN |
| 4 | Unterminated block comment | `/* no end` | Lexer error | -- |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Comment at start of file | `// header\n@module foo` | Valid |
| 2 | Comment at end of file | `@endmod // end` | Valid |
| 3 | Line comment with no newline at EOF | `@endmod // end` (no trailing newline) | Valid |
| 4 | Block comment spanning many lines | 100-line block comment | Valid |
| 5 | Adjacent block comments | `/* a *//* b */` | Both ignored |
| 6 | Comment-only file | `// nothing here` | Valid (empty compilation unit) |
| 7 | Comment inside operator | `//*` (ambiguous) | Treated as line comment starting with `*` |
| 8 | Comment inside literal | `8'b1100//01` | Line comment starts, truncates literal |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `// line comment` | Ignored by lexer | -- | -- | Standard line comment |
| 2 | `/* block */` | Ignored by lexer | -- | -- | Standard block comment |
| 3 | `/* /* nested */ */` | Error | COMMENT_NESTED_BLOCK | error | S1.4 Nested not allowed |
| 4 | `my/**/Wire` | Error | COMMENT_IN_TOKEN | error | S1.4 Comment inside token |
| 5 | `8'b11/**/00` | Error | COMMENT_IN_TOKEN | error | S1.4 Comment inside literal |
| 6 | `/* unterminated` | Lexer error | -- | error | EOF in block comment |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 1_4_COMMENT_HAPPY_PATH-valid_comments_ok.jz | -- | Happy path: valid comment patterns accepted |
| 1_4_COMMENT_IN_TOKEN-comment_inside_token.jz | COMMENT_IN_TOKEN | Comment appears inside a token (identifier/number/operator/literal) |
| 1_4_COMMENT_NESTED_BLOCK-nested_block_comment.jz | COMMENT_NESTED_BLOCK | Nested block comment `/* ... /* ... */ ... */` detected |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| COMMENT_IN_TOKEN | error | S1.4 Comment appears inside a token (identifier/number/operator/literal) | 1_4_COMMENT_IN_TOKEN-comment_inside_token.jz |
| COMMENT_NESTED_BLOCK | error | S1.4 Nested block comment detected | 1_4_COMMENT_NESTED_BLOCK-nested_block_comment.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| (none) | -- | All comment-related rules have validation tests; unterminated block comments are handled by the lexer as a generic error |
