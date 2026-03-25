# Test Plan: Repeat, Serializer, and IO Rules
**Specification Reference:** REPEAT, SERIALIZER, and IO rule groups from `compiler/src/rules.c`

## 1. Objective

Verify all REPEAT, SERIALIZER, and IO diagnostic rules are correctly defined and documented. REPEAT rules apply to `@repeat`/`@end` constructs. SERIALIZER rules apply to differential output serialization. IO rules are runtime I/O errors not reachable via lint.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Valid @repeat block | `@repeat N` ... `@end` with valid positive N | Expands correctly |
| 2 | Valid serializer config | Differential output within chip serializer ratio | Clean compilation |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Invalid repeat count | `@repeat 0` or `@repeat -1` | Error: RPT_COUNT_INVALID |
| 2 | No matching @end | `@repeat N` without `@end` | Error: RPT_NO_MATCHING_END |
| 3 | Width exceeds ratio | Differential output width exceeds chip serializer ratio | Error: SERIALIZER_WIDTH_EXCEEDS_RATIO |
| 4 | Backend write failure | Cannot write backend output file | Error: IO_BACKEND |
| 5 | IR write failure | Cannot write IR output file | Error: IO_IR |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | @repeat with count=1 | Single expansion | Valid, equivalent to no repeat |
| 2 | Nested @repeat | @repeat inside another @repeat | Depends on compiler support |
| 3 | Serializer cascade | Differential output using cascaded serializers | Info: INFO_SERIALIZER_CASCADE |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Non-positive repeat count | `@repeat 0` | RPT_COUNT_INVALID | error |
| 2 | Missing @end for @repeat | `@repeat N` without `@end` | RPT_NO_MATCHING_END | error |
| 3 | Cascaded serializers | Differential output using master+slave serializers | INFO_SERIALIZER_CASCADE | info |
| 4 | Width exceeds serializer ratio | Differential port width too wide for chip | SERIALIZER_WIDTH_EXCEEDS_RATIO | error |
| 5 | Backend output write failure | I/O error writing backend file | IO_BACKEND | error |
| 6 | IR output write failure | I/O error writing IR file | IO_IR | error |

## 4. Existing Validation Tests

No validation tests exist for REPEAT, SERIALIZER, or IO rules.

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| RPT_COUNT_INVALID | error | RPT-001 @repeat requires a positive integer count | misc_RPT_COUNT_INVALID-bad_repeat_count.jz |
| RPT_NO_MATCHING_END | error | RPT-002 @repeat without matching @end | misc_RPT_NO_MATCHING_END-missing_end.jz |
| INFO_SERIALIZER_CASCADE | info | Differential output uses cascaded serializers (master+slave) for extended serialization ratio | misc_INFO_SERIALIZER_CASCADE-cascaded_serializers.jz |
| SERIALIZER_WIDTH_EXCEEDS_RATIO | error | Differential output port width exceeds chip serializer ratio and cascade is not supported | misc_SERIALIZER_WIDTH_EXCEEDS_RATIO-width_exceeds_ratio.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| INFO_SERIALIZER_CASCADE | info | Requires chip-specific differential serializer configuration, may not be reachable via --lint alone |
| SERIALIZER_WIDTH_EXCEEDS_RATIO | error | Requires chip-specific differential serializer configuration, may not be reachable via --lint alone |
| IO_BACKEND | error | Not Testable: runtime I/O error, not reachable via --info --lint |
| IO_IR | error | Not Testable: runtime I/O error, not reachable via --info --lint |
