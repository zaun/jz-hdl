# Test Plan: 1.1 Identifiers

**Specification Reference:** Section 1.1 of jz-hdl-specification.md

## 1. Objective

Verify that the lexer and parser correctly accept valid identifiers, reject invalid identifiers, enforce the 255-character maximum length, treat identifiers as case-sensitive, reject reserved keywords used as identifiers, and handle the single-underscore (`_`) no-connect placeholder rule.

## 2. Instrumentation Strategy

- **Span: `lexer.tokenize`** — Trace each token produced by the lexer; attribute `token.type` = `IDENTIFIER` or `KEYWORD`.
- **Span: `lexer.identifier_validate`** — Fires on each identifier candidate; attributes include `identifier.length`, `identifier.value`, `identifier.is_keyword`.
- **Event: `identifier.rejected`** — Emitted when an identifier fails regex or keyword checks; includes `reason` attribute.
- **Coverage Hook:** Instrument the identifier regex check path and keyword lookup table to confirm every reserved word is tested.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Simple lowercase | `foo` | Valid identifier |
| 2 | Simple uppercase | `Foo` | Valid identifier |
| 3 | Mixed case | `mySignal_01` | Valid identifier |
| 4 | Leading underscore | `_data` | Valid identifier |
| 5 | Maximum length (255 chars) | `a` × 255 | Valid identifier |
| 6 | Single char | `x` | Valid identifier |
| 7 | Underscore-separated | `clk_en_out` | Valid identifier |
| 8 | Case sensitivity | `Foo` vs `foo` | Two distinct identifiers |

### 3.2 Boundary/Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Exactly 255 chars | `a` × 255 | Valid |
| 2 | 256 chars (over limit) | `a` × 256 | Error: ID_SYNTAX_INVALID |
| 3 | Single underscore `_` in non-port context | `WIRE { _ [1]; }` | Error: ID_SINGLE_UNDERSCORE |
| 4 | Single underscore in port no-connect | `@new mod(.port(_))` | Valid |
| 5 | Identifier starting with digit | `0abc` | Lexer error (not an identifier) |
| 6 | Empty string as identifier | `` | Lexer error |
| 7 | Identifier with only underscores | `__` | Valid (not single underscore) |

### 3.3 Negative Testing

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Reserved keyword as identifier | `WIRE { REGISTER [1]; }` | Error: KEYWORD_AS_IDENTIFIER |
| 2 | All keywords tested | Each keyword from spec used as signal name | Error: KEYWORD_AS_IDENTIFIER |
| 3 | Reserved identifier as signal | `WIRE { VCC [1]; }` | Error: KEYWORD_AS_IDENTIFIER |
| 4 | Reserved identifier GND | `WIRE { GND [1]; }` | Error: KEYWORD_AS_IDENTIFIER |
| 5 | Directive used as identifier | `WIRE { project [1]; }` | Valid (directives are @-prefixed, lowercase ok) |
| 6 | Non-ASCII characters | `WIRE { café [1]; }` | Error: ID_SYNTAX_INVALID |
| 7 | Special characters | `WIRE { my-signal [1]; }` | Lexer error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `myWire` as signal name | Accepted, AST node created | — | Standard valid identifier |
| 2 | `REGISTER` as signal name | Compile error | KEYWORD_AS_IDENTIFIER | Reserved keyword |
| 3 | `_` as wire name | Compile error | ID_SINGLE_UNDERSCORE | Reserved for no-connect |
| 4 | `_` in `@new` port list | Accepted as no-connect | — | Only valid context for `_` |
| 5 | 256-char identifier | Compile error | ID_SYNTAX_INVALID | Exceeds 255-char limit |
| 6 | `PLL` as signal name | Compile error | KEYWORD_AS_IDENTIFIER | Reserved identifier |
| 7 | `IDX` as signal name | Compile error | KEYWORD_AS_IDENTIFIER | Reserved for templates |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `lexer.c` | Tokenizes source, produces IDENTIFIER tokens | Direct unit test with in-memory buffers; no mocks needed (pure function) |
| `parser_core.c` | Consumes tokens, validates identifier context | Feed pre-built token streams; mock lexer output |
| `rules.c` | Provides rule IDs for diagnostics | No mock needed (static lookup table) |
| `diagnostic.c` | Collects and formats errors | Capture diagnostic list; verify rule IDs and messages |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| KEYWORD_AS_IDENTIFIER | S1.1 Reserved keyword used as identifier | Neg 1-4 |
| ID_SYNTAX_INVALID | S1.1 Identifier doesn't match regex or exceeds 255 chars | Boundary 2, 6; Neg 6 |
| ID_SINGLE_UNDERSCORE | S1.1 Single underscore used outside no-connect context | Boundary 3 |
| DIRECTIVE_INVALID_CONTEXT | S1.1/S6.2 Structural directives in invalid location | (Tested in Section 6 plans) |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| ID_STARTS_WITH_DIGIT | S1.1 (regex) | No explicit rule ID for digit-leading identifiers; handled by lexer as non-identifier token |
| ID_NON_ASCII | S1.1 (ASCII only) | No explicit rule ID for non-ASCII chars; handled by lexer |
