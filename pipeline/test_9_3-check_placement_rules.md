# Test Plan: 9.3 @check Placement Rules
**Specification Reference:** Section 9.3 of jz-hdl-specification.md
## 1. Objective
Verify @check placement: valid inside @project and @module, invalid inside blocks.
## 2. Instrumentation Strategy
- **Span: `parser.check_placement`** — attributes: `context`.
## 3. Test Scenarios
### 3.1 Happy Path
1. @check inside @module
2. @check inside @project
### 3.3 Negative Testing
1. @check inside ASYNCHRONOUS — Error
2. @check inside SYNCHRONOUS — Error
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | @check in ASYNC | Error | CHECK_WRONG_CONTEXT | S9.3 |
## 5-6. Cross-reference with 9.7.
