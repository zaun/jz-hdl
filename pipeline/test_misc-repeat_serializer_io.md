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

### 2.4 INFO_SERIALIZER_CASCADE

**Description:** Verify that the compiler emits an informational diagnostic when a differential output pin is configured with a serialization ratio that requires cascaded serializers (master+slave pair). This occurs during project-level analysis when the requested serialization ratio exceeds what a single serializer can provide, so the toolchain automatically pairs a master and slave serializer to achieve the extended ratio.

**Triggering Construct:** A differential output port declared with a serialization ratio (e.g., 10:1) that exceeds the single-serializer capability of the target chip. The compiler detects that cascaded (master+slave) serializers are needed and emits the info diagnostic.

```
chip "pa35t" {
    module top(
        output diff tx_pair : 1 @serialize(10)
    ) {
        // Serialization ratio 10:1 exceeds single serializer limit,
        // triggering cascaded master+slave configuration
    }
}
```

**Expected Rule ID:** INFO_SERIALIZER_CASCADE
**Expected Severity:** info
**Expected Message:** Differential output uses cascaded serializers (master+slave) for extended serialization ratio
**Test File:** `misc_INFO_SERIALIZER_CASCADE-cascaded_serializers.jz`

### 2.5 SERIALIZER_WIDTH_EXCEEDS_RATIO

**Description:** Verify that the compiler emits an error when a differential output port width exceeds the chip's maximum serializer ratio and the chip does not support cascading serializers to extend the ratio. This fires during project-level analysis when the declared port width combined with serialization parameters would require more serializer stages than the hardware can provide.

**Triggering Construct:** A differential output port whose width exceeds the chip's maximum serializer ratio on a chip that does not support serializer cascade. The compiler cannot map the output to available serializer resources and must reject the configuration.

```
chip "pa35t" {
    module top(
        output diff tx_wide : 16 @serialize(4)
        // Port width 16 with ratio 4 exceeds chip serializer
        // capability, and cascade is not supported for this config
    ) {
    }
}
```

**Expected Rule ID:** SERIALIZER_WIDTH_EXCEEDS_RATIO
**Expected Severity:** error
**Expected Message:** Differential output port width exceeds chip serializer ratio and cascade is not supported
**Test File:** `misc_SERIALIZER_WIDTH_EXCEEDS_RATIO-width_exceeds_ratio.jz`

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Non-positive repeat count | `@repeat 0` | RPT_COUNT_INVALID | error |
| 2 | Missing @end for @repeat | `@repeat N` without `@end` | RPT_NO_MATCHING_END | error |
| 3 | Cascaded serializers | Differential output using master+slave serializers for extended serialization ratio | INFO_SERIALIZER_CASCADE | info |
| 4 | Width exceeds serializer ratio | Differential output port width exceeds chip serializer ratio and cascade is not supported | SERIALIZER_WIDTH_EXCEEDS_RATIO | error |
| 5 | Backend output write failure | I/O error writing backend file | IO_BACKEND | error |
| 6 | IR output write failure | I/O error writing IR file | IO_IR | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| misc_HAPPY_PATH-repeat_ok.jz | -- | Happy path: valid @repeat block usage |
| misc_RPT_COUNT_INVALID-non_numeric_count.jz | RPT_COUNT_INVALID | Non-numeric @repeat count |
| misc_RPT_COUNT_INVALID-zero_count.jz | RPT_COUNT_INVALID | Zero @repeat count |
| misc_RPT_NO_MATCHING_END-missing_end.jz | RPT_NO_MATCHING_END | @repeat without matching @end |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| RPT_COUNT_INVALID | error | RPT-001 @repeat requires a positive integer count | misc_RPT_COUNT_INVALID-non_numeric_count.jz, misc_RPT_COUNT_INVALID-zero_count.jz |
| RPT_NO_MATCHING_END | error | RPT-002 @repeat without matching @end | misc_RPT_NO_MATCHING_END-missing_end.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| INFO_SERIALIZER_CASCADE | info | Not testable: backend-only diagnostic, not reachable via `--lint` |
| SERIALIZER_WIDTH_EXCEEDS_RATIO | error | Not testable: backend-only diagnostic, not reachable via `--lint` |
| IO_BACKEND | error | Not testable: runtime I/O error, not reachable via `--lint` |
| IO_IR | error | Not testable: runtime I/O error, not reachable via `--lint` |
