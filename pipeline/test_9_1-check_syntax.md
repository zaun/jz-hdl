# Test Plan: 9.1 @check Syntax
**Specification Reference:** Section 9.1 of jz-hdl-specification.md
## 1. Objective
Verify `@check (<expr>, <message>);` syntax with required parentheses.
## 2. Instrumentation Strategy
- **Span: `parser.check`** — attributes: `expression`, `message`.
## 3. Test Scenarios
### 3.1 Happy Path
1. `@check (WIDTH == 32, "Width must be 32");`
2. `@check (DEPTH > 0, "Depth must be positive");`
### 3.2 Boundary/Edge Cases
1. Complex expression with clog2
### 3.3 Negative Testing
1. Missing parentheses — Error
2. Missing message — Error
3. Missing semicolon — Error
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Missing parens | Parse error | — | S9.1 |
## 5-6. See Section 9.7 for comprehensive error testing.
