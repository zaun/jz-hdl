# Test Plan: 1.1 Identifiers

**Specification Reference:** Section 1.1 of jz-hdl-specification.md

## 1. Objective

Verify that the lexer and parser correctly accept valid identifiers, reject invalid identifiers, enforce the 255-character maximum length, treat identifiers as case-sensitive, reject reserved keywords used as identifiers, and handle the single-underscore (`_`) no-connect placeholder rule.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Simple lowercase | `foo` | Valid identifier |
| 2 | Simple uppercase | `Foo` | Valid identifier |
| 3 | Mixed case | `mySignal_01` | Valid identifier |
| 4 | Leading underscore | `_data` | Valid identifier |
| 5 | Maximum length (255 chars) | `a` x 255 | Valid identifier |
| 6 | Single char | `x` | Valid identifier |
| 7 | Underscore-separated | `clk_en_out` | Valid identifier |
| 8 | Case sensitivity | `Foo` vs `foo` | Two distinct identifiers |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Reserved keyword as identifier | `WIRE { REGISTER [1]; }` | Error | KEYWORD_AS_IDENTIFIER |
| 2 | All keywords tested | Each keyword from spec used as signal name | Error | KEYWORD_AS_IDENTIFIER |
| 3 | Reserved identifier as signal | `WIRE { VCC [1]; }` | Error | KEYWORD_AS_IDENTIFIER |
| 4 | Reserved identifier GND | `WIRE { GND [1]; }` | Error | KEYWORD_AS_IDENTIFIER |
| 5 | Non-ASCII characters | `WIRE { cafe [1]; }` | Error | ID_SYNTAX_INVALID |
| 6 | Single underscore in non-connect context | `WIRE { _ [1]; }` | Error | ID_SINGLE_UNDERSCORE |
| 7 | 256+ char identifier | `a` x 256 | Error | ID_SYNTAX_INVALID |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Exactly 255 chars | `a` x 255 | Valid |
| 2 | 256 chars (over limit) | `a` x 256 | Error: ID_SYNTAX_INVALID |
| 3 | Single underscore `_` in port no-connect | `@new mod(.port(_))` | Valid |
| 4 | Identifier starting with digit | `0abc` | Lexer error (not an identifier) |
| 5 | Identifier with only underscores | `__` | Valid (not single underscore) |
| 6 | Directive used as identifier | `WIRE { project [1]; }` | Valid (directives are @-prefixed, lowercase ok) |
| 7 | Special characters | `WIRE { my-signal [1]; }` | Lexer error |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `myWire` as signal name | Accepted, AST node created | -- | -- | Standard valid identifier |
| 2 | `REGISTER` as signal name | Compile error | KEYWORD_AS_IDENTIFIER | error | Reserved keyword |
| 3 | `_` as wire name | Compile error | ID_SINGLE_UNDERSCORE | error | Reserved for no-connect |
| 4 | `_` in `@new` port list | Accepted as no-connect | -- | -- | Only valid context for `_` |
| 5 | 256-char identifier | Compile error | ID_SYNTAX_INVALID | error | Exceeds 255-char limit |
| 6 | `PLL` as signal name | Compile error | KEYWORD_AS_IDENTIFIER | error | Reserved identifier |
| 7 | `IDX` as signal name | Compile error | KEYWORD_AS_IDENTIFIER | error | Reserved for templates |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 1_1_ID_SINGLE_UNDERSCORE-non_connect_contexts.jz | ID_SINGLE_UNDERSCORE | Single underscore `_` used as regular identifier outside no-connect context |
| 1_1_ID_SYNTAX_INVALID-length_exceeded.jz | ID_SYNTAX_INVALID | Identifier does not match identifier regex or exceeds 255 chars |
| 1_1_KEYWORD_AS_IDENTIFIER-keyword_in_declarations.jz | KEYWORD_AS_IDENTIFIER | Reserved keyword used as identifier in declarations |
| 1_1_KEYWORD_AS_IDENTIFIER-reserved_identifier.jz | KEYWORD_AS_IDENTIFIER | Reserved identifier (VCC, GND, PLL, etc.) used as identifier |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| ID_SYNTAX_INVALID | error | S1.1 Identifier does not match identifier regex or exceeds 255 chars | 1_1_ID_SYNTAX_INVALID-length_exceeded.jz |
| ID_SINGLE_UNDERSCORE | error | S1.1 Single underscore `_` used as regular identifier outside no-connect context | 1_1_ID_SINGLE_UNDERSCORE-non_connect_contexts.jz |
| KEYWORD_AS_IDENTIFIER | error | S1.1 Reserved keyword used as identifier | 1_1_KEYWORD_AS_IDENTIFIER-keyword_in_declarations.jz, 1_1_KEYWORD_AS_IDENTIFIER-reserved_identifier.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| (none) | -- | All identifier-related rules have validation tests |
