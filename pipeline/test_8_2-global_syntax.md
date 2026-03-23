# Test Plan: 8.2 Global Syntax
**Specification Reference:** Section 8.2 of jz-hdl-specification.md
## 1. Objective
Verify @global/@endglob syntax, sized literal requirement for values, and namespace root creation.
## 2. Instrumentation Strategy
- **Span: `parser.global`** — attributes: `global_name`, `entries`.
## 3. Test Scenarios
### 3.1 Happy Path
1. `@global ISA INST_ADD = 17'b0110011_0000000_000; @endglob`
2. Multiple entries in one block
### 3.2 Boundary/Edge Cases
1. Empty @global block
### 3.3 Negative Testing
1. Bare integer as value: `CONST_A = 42;` — Error (not sized)
2. CONFIG reference as value — Error
3. Missing @endglob — Error
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Bare integer in global | Error | GLOBAL_NOT_SIZED_LITERAL | S8.5 |
## 5. Integration Points
| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_core.c` | @global parsing | Token stream |
## 6. Rules Matrix
### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| GLOBAL_NOT_SIZED_LITERAL | Unsized value in @global | Neg 1 |
### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | — | — |
