# Test Plan: 7.5 Initialization

**Specification Reference:** Section 7.5 of jz-hdl-specification.md

## 1. Objective

Verify literal and file-based MEM initialization, including inline sized literals, replication/concatenation expressions, @file directives with string paths and CONST/CONFIG references, zero-padding for undersized files, overflow detection, x/z prohibition in both literal and file-based init, missing file handling, and numeric-in-string-context errors.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Literal init (sized literal) | `m [8] [256] = 8'h00 { ... };` | Valid, all words initialized to 0x00 |
| 2 | Literal init (replication) | `m [8] [256] = {8{1'b0}} { ... };` | Valid, all words initialized to 0x00 |
| 3 | Literal init (concatenation) | `m [8] [256] = {4'hF, 4'h0} { ... };` | Valid, all words initialized to 0xF0 |
| 4 | File init (string path) | `m [8] [256] = @file("data.hex") { ... };` | Valid, loaded from file |
| 5 | File init (CONST string path) | `@file(ROM_FILE)` with `CONST { ROM_FILE = "data.hex"; }` | Valid |
| 6 | File init (CONFIG string path) | `@file(CONFIG.BOOT_IMAGE)` | Valid |
| 7 | File exactly matches depth | 256 entries for 256-deep MEM | Valid, no padding needed |
| 8 | Literal width equals word width | `m [8] [256] = 8'hFF { ... };` | Valid, exact match |
| 9 | Literal width less than word width | `m [16] [256] = 8'hFF { ... };` | Valid, zero-extended |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Init literal overflow | Literal value exceeds word width | Error | MEM_INIT_LITERAL_OVERFLOW |
| 2 | Missing initialization | MEM with no `= ...` clause | Error | MEM_MISSING_INIT |
| 3 | Init file not found | `@file("nonexistent.hex")` | Error | MEM_INIT_FILE_NOT_FOUND |
| 4 | Init literal contains x | `8'hxx` in init | Error | MEM_INIT_CONTAINS_X |
| 5 | Init file contains x | File with x/z values | Error | MEM_INIT_FILE_CONTAINS_X |
| 6 | File larger than depth | File has more entries than MEM depth | Error | MEM_INIT_FILE_TOO_LARGE |
| 7 | Numeric CONST in @file path | `@file(DEPTH)` where DEPTH is numeric | Error | CONST_NUMERIC_IN_STRING_CONTEXT |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | File smaller than depth | Fewer entries than depth, zero-padded | Warning: MEM_WARN_PARTIAL_INIT |
| 2 | Single-entry memory with literal | `m [8] [1] = 8'hFF { ... };` | Valid |
| 3 | Empty file | @file with 0 entries | Warning: MEM_WARN_PARTIAL_INIT |
| 4 | Multiple supported file formats | `.bin`, `.hex`, `.mif`, `.coe`, `.mem` | Valid |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Literal value exceeds word width | `m [4] [256] = 8'hFF { ... };` | MEM_INIT_LITERAL_OVERFLOW | error |
| 2 | No initialization expression | `m [8] [256] { OUT rd SYNC; };` | MEM_MISSING_INIT | error |
| 3 | Init file does not exist | `@file("nonexistent.hex")` | MEM_INIT_FILE_NOT_FOUND | error |
| 4 | x in literal init values | `m [8] [256] = 8'hxx { ... };` | MEM_INIT_CONTAINS_X | error |
| 5 | x/z in file init values | File containing undefined bits | MEM_INIT_FILE_CONTAINS_X | error |
| 6 | File has more entries than depth | 512-word file for 256-deep MEM | MEM_INIT_FILE_TOO_LARGE | error |
| 7 | File has fewer entries than depth | 100-word file for 256-deep MEM | MEM_WARN_PARTIAL_INIT | warning |
| 8 | Numeric CONST used as @file path | `@file(DEPTH)` where DEPTH is numeric | CONST_NUMERIC_IN_STRING_CONTEXT | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_5_HAPPY_PATH-initialization_ok.jz | — | Happy path: valid literal and file-based initialization |
| 7_5_MEM_INIT_CONTAINS_X-x_in_literal_init.jz | MEM_INIT_CONTAINS_X | x value in literal initialization |
| 7_5_MEM_INIT_FILE_NOT_FOUND-nonexistent_file.jz | MEM_INIT_FILE_NOT_FOUND | Init file does not exist |
| 7_5_MEM_INIT_FILE_TOO_LARGE-file_exceeds_depth.jz | MEM_INIT_FILE_TOO_LARGE | Init file has more entries than MEM depth |
| 7_5_MEM_INIT_LITERAL_OVERFLOW-value_exceeds_width.jz | MEM_INIT_LITERAL_OVERFLOW | Literal init value exceeds word width |
| 7_5_MEM_WARN_PARTIAL_INIT-file_smaller_than_depth.jz | MEM_WARN_PARTIAL_INIT | Init file smaller than MEM depth, zero-padded |
| 7_5_MEM_INIT_FILE_CONTAINS_X-x_in_file_init.jz | MEM_INIT_FILE_CONTAINS_X | Init file contains x or z values |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_INIT_LITERAL_OVERFLOW | error | S7.5.1/S7.7.1 Literal init value exceeds word width | 7_5_MEM_INIT_LITERAL_OVERFLOW-value_exceeds_width.jz |
| MEM_MISSING_INIT | error | S7.5.1/S7.7.1/S7.7.3 No initialization expression on MEM | 7_1_MEM_MISSING_INIT-missing_init.jz |
| MEM_INIT_FILE_NOT_FOUND | error | S7.5.2/S7.7.1 Init file does not exist | 7_5_MEM_INIT_FILE_NOT_FOUND-nonexistent_file.jz |
| MEM_INIT_CONTAINS_X | error | S2.1/S7.5.1 x value in literal initialization | 7_5_MEM_INIT_CONTAINS_X-x_in_literal_init.jz |
| MEM_INIT_FILE_TOO_LARGE | error | S7.5.2/S7.7.1 File has more entries than MEM depth | 7_5_MEM_INIT_FILE_TOO_LARGE-file_exceeds_depth.jz |
| MEM_WARN_PARTIAL_INIT | warning | S7.5.2/S7.7.3 File smaller than depth, zero-padded | 7_5_MEM_WARN_PARTIAL_INIT-file_smaller_than_depth.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| MEM_INIT_FILE_CONTAINS_X | error | Unimplemented: no validation test exists; rule is not yet implemented in the compiler |
