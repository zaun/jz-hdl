# Test Plan: 7.5 Initialization

**Specification Reference:** Section 7.5 of jz-hdl-specification.md

## 1. Objective

Verify literal and file-based MEM initialization, including inline hex values, @file directives, zero-padding for undersized files, overflow detection, x/z prohibition, and missing file handling.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Literal init | `m [8] [4] = 32'h00000000 { ... };` | Valid, memory initialized with literal |
| 2 | File init | `m [8] [256] = @file("data.hex") { ... };` | Valid, memory initialized from file |
| 3 | CONST string path | `@file(ROM_FILE)` with CONST definition | Valid, resolved at compile time |
| 4 | File exactly matches depth | 256 entries for 256-deep MEM | Valid, no padding needed |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Init literal overflow | Literal value exceeds word width | Error | MEM_INIT_LITERAL_OVERFLOW |
| 2 | Missing initialization | MEM with no `= ...` clause | Error | MEM_MISSING_INIT |
| 3 | Init file not found | `@file("nonexistent.hex")` | Error | MEM_INIT_FILE_NOT_FOUND |
| 4 | Init literal contains x | `8'hxx` in init | Error | MEM_INIT_CONTAINS_X |
| 5 | Init file contains x | File with x values | Error | MEM_INIT_FILE_CONTAINS_X |
| 6 | File larger than depth | File has more entries than MEM depth | Error | MEM_INIT_FILE_TOO_LARGE |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | File smaller than depth | Fewer entries than depth, zero-padded | Warning: MEM_WARN_PARTIAL_INIT |
| 2 | Single-entry memory with literal | `m [8] [1] = 8'hFF { ... };` | Valid |
| 3 | Empty file | @file with 0 entries | Warning: MEM_WARN_PARTIAL_INIT |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Literal value exceeds word width | Error | MEM_INIT_LITERAL_OVERFLOW | error | S7.5 |
| 2 | No initialization expression | Error | MEM_MISSING_INIT | error | S7.5 |
| 3 | Init file does not exist | Error | MEM_INIT_FILE_NOT_FOUND | error | S7.5.2 |
| 4 | x in literal init values | Error | MEM_INIT_CONTAINS_X | error | S7.5 |
| 5 | x in file init values | Error | MEM_INIT_FILE_CONTAINS_X | error | S7.5.2 |
| 6 | File has more entries than depth | Error | MEM_INIT_FILE_TOO_LARGE | error | S7.5.2 |
| 7 | File has fewer entries than depth | Warning | MEM_WARN_PARTIAL_INIT | warning | S7.5.2: zero-padded |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_5_MEM_INIT_CONTAINS_X-x_in_literal_init.jz | MEM_INIT_CONTAINS_X | x value in literal initialization |
| 7_5_MEM_INIT_FILE_NOT_FOUND-nonexistent_file.jz | MEM_INIT_FILE_NOT_FOUND | Init file does not exist |
| 7_5_MEM_INIT_FILE_TOO_LARGE-file_exceeds_depth.jz | MEM_INIT_FILE_TOO_LARGE | Init file has more entries than MEM depth |
| 7_5_MEM_INIT_LITERAL_OVERFLOW-value_exceeds_width.jz | MEM_INIT_LITERAL_OVERFLOW | Literal init value exceeds word width |
| 7_5_MEM_WARN_PARTIAL_INIT-file_smaller_than_depth.jz | MEM_WARN_PARTIAL_INIT | Init file smaller than MEM depth, zero-padded |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_INIT_LITERAL_OVERFLOW | error | S7.5 Literal init value exceeds word width | 7_5_MEM_INIT_LITERAL_OVERFLOW-value_exceeds_width.jz |
| MEM_MISSING_INIT | error | S7.5 No initialization expression on MEM | (declaration-level, covered in 7_1 scope) |
| MEM_INIT_FILE_NOT_FOUND | error | S7.5.2 Init file does not exist | 7_5_MEM_INIT_FILE_NOT_FOUND-nonexistent_file.jz |
| MEM_INIT_CONTAINS_X | error | S7.5 x value in literal initialization | 7_5_MEM_INIT_CONTAINS_X-x_in_literal_init.jz |
| MEM_INIT_FILE_CONTAINS_X | error | S7.5.2 x value in file initialization | (no validation test yet) |
| MEM_INIT_FILE_TOO_LARGE | error | S7.5.2 File has more entries than MEM depth | 7_5_MEM_INIT_FILE_TOO_LARGE-file_exceeds_depth.jz |
| MEM_WARN_PARTIAL_INIT | warning | S7.5.2 File smaller than depth, zero-padded | 7_5_MEM_WARN_PARTIAL_INIT-file_smaller_than_depth.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| MEM_INIT_FILE_CONTAINS_X | error | No validation test yet for x values in file-based initialization |
