# Test Plan: 1.4 Comments

**Specification Reference:** Section 1.4 of jz-hdl-specification.md

## 1. Objective

Verify that the lexer correctly handles line comments (`//`), block comments (`/* */`), rejects nested block comments, rejects comments appearing inside tokens, and allows comments anywhere whitespace is permitted.

## 2. Instrumentation Strategy

- **Span: `lexer.skip_comment`** — Trace comment processing; attributes: `comment.type` (line/block), `comment.start_line`, `comment.end_line`.
- **Event: `comment.nested_detected`** — Fires when `/*` appears inside an existing block comment.
- **Event: `comment.in_token`** — Fires when a comment marker appears inside a token.
- **Coverage Hook:** Ensure both line and block comment paths are exercised, including multi-line block comments.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Line comment | `WIRE { x [1]; } // comment` | Comment ignored |
| 2 | Block comment | `WIRE { /* comment */ x [1]; }` | Comment ignored |
| 3 | Multi-line block | `/* line1\nline2 */` | Comment ignored |
| 4 | Comment between tokens | `WIRE /* sep */ { x [1]; }` | Valid, comment is whitespace |
| 5 | Comment in directive header | `@module /* name */ foo` | Valid |
| 6 | Multiple line comments | `// a\n// b\nWIRE { x [1]; }` | All comments ignored |
| 7 | Empty block comment | `/**/` | Valid, ignored |
| 8 | Block comment with stars | `/* ** star ** */` | Valid, ignored |

### 3.2 Boundary/Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Comment at start of file | `// header\n@module foo` | Valid |
| 2 | Comment at end of file | `@endmod // end` | Valid |
| 3 | Line comment with no newline at EOF | `@endmod // end` (no trailing newline) | Valid |
| 4 | Block comment spanning many lines | 100-line block comment | Valid |
| 5 | Adjacent block comments | `/* a *//* b */` | Both ignored |
| 6 | Comment-only file | `// nothing here` | Valid (empty compilation unit) |

### 3.3 Negative Testing

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Nested block comment | `/* outer /* inner */ */` | Error: COMMENT_NESTED_BLOCK |
| 2 | Comment inside identifier | `my/*comment*/Wire` | Error: COMMENT_IN_TOKEN |
| 3 | Comment inside number | `8'b11/**/00` | Error: COMMENT_IN_TOKEN |
| 4 | Comment inside operator | `//*` (ambiguous) | Treated as line comment starting with `*` |
| 5 | Unterminated block comment | `/* no end` | Lexer error: unterminated comment |
| 6 | Comment inside string-like literal | `8'b1100//01` | Line comment starts, truncates literal |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `// line comment` | Ignored by lexer | — | Standard line comment |
| 2 | `/* block */` | Ignored by lexer | — | Standard block comment |
| 3 | `/* /* nested */ */` | Error | COMMENT_NESTED_BLOCK | Nested not allowed |
| 4 | `my/**/Wire` | Error | COMMENT_IN_TOKEN | Comment inside token |
| 5 | `8'b11/**/00` | Error | COMMENT_IN_TOKEN | Comment inside literal |
| 6 | `/* unterminated` | Lexer error | — | EOF in block comment |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `lexer.c` | Comment detection and skipping | Direct unit test with source strings; no mocks (operates on in-memory buffers) |
| `diagnostic.c` | Error reporting for invalid comments | Capture diagnostic list |
| `rules.c` | Rule ID lookup | No mock needed |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| COMMENT_IN_TOKEN | S1.4 Comment appears inside a token | Neg 2, 3 |
| COMMENT_NESTED_BLOCK | S1.4 Nested block comment detected | Neg 1 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| COMMENT_UNTERMINATED | S1.4 Block comment without closing `*/` | No explicit rule ID; may be a generic lexer error |
