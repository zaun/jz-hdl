# Test Plan: 6.8 BUS Aggregation

**Specification Reference:** Section 6.8 of jz-hdl-specification.md

## 1. Objective

Verify BUS definitions at project scope, signal declarations within BUS (IN/OUT/INOUT with widths), SOURCE/TARGET role assignment on module ports, role-based direction resolution, and bulk BUS-to-BUS assignment semantics.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | BUS with IN/OUT/INOUT signals | Define BUS with mixed signal directions |
| 2 | Module with BUS SOURCE port | Module port declared as `BUS <bus_id> SOURCE` |
| 3 | Module with BUS TARGET port | Module port declared as `BUS <bus_id> TARGET` |
| 4 | Bulk BUS assignment | SOURCE-to-TARGET bulk connection between instances |
| 5 | BUS with CONFIG width references | Signal widths referencing `CONFIG.<name>` |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Duplicate BUS name | Two BUS definitions with same name in project -- Error |
| 2 | Duplicate signal name in BUS | Two signals with same name inside one BUS definition -- Error |
| 3 | Invalid signal direction | Signal direction not IN/OUT/INOUT -- Error |
| 4 | Bulk assign mismatched BUS IDs | Bulk assignment between ports referencing different BUS types -- Error |
| 5 | Bulk assign same role | SOURCE-to-SOURCE or TARGET-to-TARGET bulk assignment -- Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | BUS with single signal | Minimal BUS containing one signal |
| 2 | BUS with many signals | Large BUS definition (20+ signals) |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|-----------------|---------|----------|-------|
| 1 | Duplicate BUS name | Error | BUS_DEF_DUP_NAME | error | S6.8 |
| 2 | Duplicate signal name in BUS | Error | BUS_DEF_SIGNAL_DUP_NAME | error | S6.8 |
| 3 | Invalid signal direction | Error | BUS_DEF_INVALID_DIR | error | S6.8 |
| 4 | Bulk assign different BUS IDs | Error | BUS_BULK_BUS_MISMATCH | error | S6.8 |
| 5 | Bulk assign same role | Error | BUS_BULK_ROLE_CONFLICT | error | S6.8 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_8_BUS_DEF_DUP_NAME-duplicate_bus_name.jz | BUS_DEF_DUP_NAME | Duplicate BUS definition name in project |
| 6_8_BUS_DEF_SIGNAL_DUP_NAME-duplicate_signal_in_bus.jz | BUS_DEF_SIGNAL_DUP_NAME | Duplicate signal name inside BUS definition |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| BUS_DEF_DUP_NAME | error | S6.8 Duplicate BUS definition name in project | 6_8_BUS_DEF_DUP_NAME-duplicate_bus_name.jz |
| BUS_DEF_SIGNAL_DUP_NAME | error | S6.8 Duplicate signal name inside BUS definition | 6_8_BUS_DEF_SIGNAL_DUP_NAME-duplicate_signal_in_bus.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| BUS_DEF_INVALID_DIR | error | No dedicated validation test file exists |
| BUS_BULK_BUS_MISMATCH | error | No dedicated validation test file exists |
| BUS_BULK_ROLE_CONFLICT | error | No dedicated validation test file exists |
