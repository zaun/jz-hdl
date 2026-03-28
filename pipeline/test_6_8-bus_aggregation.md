# Test Plan: 6.8 BUS Aggregation

**Specification Reference:** Section 6.8 of jz-hdl-specification.md

## 1. Objective

Verify BUS definitions at project scope: signal declarations within BUS (IN/OUT/INOUT with widths), direction validation, name uniqueness for BUS definitions and signals, SOURCE/TARGET role assignment on module ports, role-based direction resolution, and bulk BUS-to-BUS assignment semantics.

Note: BUS port-level rules (BUS_PORT_UNKNOWN_BUS, BUS_PORT_NOT_BUS, BUS_PORT_ARRAY_COUNT_INVALID, etc.) are S4.4.1 rules tested in the Section 4.4 test plan. BUS signal access rules (BUS_SIGNAL_UNDEFINED, BUS_SIGNAL_READ_FROM_WRITABLE, BUS_SIGNAL_WRITE_TO_READABLE) are also S4.4.1 rules. This plan covers only the S6.8 project-level BUS definition and bulk assignment rules.

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
| 1 | Duplicate BUS name | Two BUS definitions with same name in project |
| 2 | Duplicate signal name in BUS | Two signals with same name inside one BUS definition |
| 3 | Invalid signal direction | Signal direction not IN/OUT/INOUT |
| 4 | Bulk assign mismatched BUS IDs | Bulk assignment between ports referencing different BUS types |
| 5 | Bulk assign same role | SOURCE-to-SOURCE or TARGET-to-TARGET bulk assignment |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | BUS with single signal | Minimal BUS containing one signal |
| 2 | BUS with many signals | Large BUS definition (20+ signals) |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|------------------|----------|
| 1 | Duplicate BUS name | Two `BUS foo { ... }` in project | BUS_DEF_DUP_NAME | error |
| 2 | Duplicate signal name in BUS | `BUS foo { IN [8] data; OUT [8] data; }` | BUS_DEF_SIGNAL_DUP_NAME | error |
| 3 | Invalid signal direction | Non-IN/OUT/INOUT direction in BUS definition | BUS_DEF_INVALID_DIR | error |
| 4 | Bulk assign different BUS IDs | Bulk assign port with BUS_A to port with BUS_B | BUS_BULK_BUS_MISMATCH | error |
| 5 | Bulk assign same role | SOURCE-to-SOURCE bulk assignment | BUS_BULK_ROLE_CONFLICT | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_8_HAPPY_PATH-bus_ok.jz | -- | Valid BUS definition, SOURCE/TARGET ports, bulk assignment (clean compile) |
| 6_8_BUS_BULK_BUS_MISMATCH-bus_mismatch.jz | BUS_BULK_BUS_MISMATCH | Bulk assignment between ports referencing different BUS types |
| 6_8_BUS_BULK_ROLE_CONFLICT-role_conflict.jz | BUS_BULK_ROLE_CONFLICT | Bulk assignment between ports with same role |
| 6_8_BUS_DEF_DUP_NAME-duplicate_bus_name.jz | BUS_DEF_DUP_NAME | Duplicate BUS definition name in project |
| 6_8_BUS_DEF_SIGNAL_DUP_NAME-duplicate_signal_in_bus.jz | BUS_DEF_SIGNAL_DUP_NAME | Duplicate signal name inside BUS definition |
| 6_8_BUS_DEF_INVALID_DIR-invalid_direction.jz | BUS_DEF_INVALID_DIR | BUS definition with invalid signal direction (not IN/OUT/INOUT) |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| BUS_DEF_DUP_NAME | error | S6.8 Duplicate BUS definition name in project | 6_8_BUS_DEF_DUP_NAME-duplicate_bus_name.jz |
| BUS_DEF_SIGNAL_DUP_NAME | error | S6.8 Duplicate signal name inside BUS definition | 6_8_BUS_DEF_SIGNAL_DUP_NAME-duplicate_signal_in_bus.jz |
| BUS_BULK_BUS_MISMATCH | error | S6.8 Bulk BUS assignment requires both sides to reference the same BUS id | 6_8_BUS_BULK_BUS_MISMATCH-bus_mismatch.jz |
| BUS_BULK_ROLE_CONFLICT | error | S6.8 Bulk BUS assignment between instances with the same BUS role is not allowed | 6_8_BUS_BULK_ROLE_CONFLICT-role_conflict.jz |
### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| BUS_DEF_INVALID_DIR | error | Dead code: test exists (`6_8_BUS_DEF_INVALID_DIR-invalid_direction.jz`) but rule is dead code |

### 5.3 Rules in BUS_RULES Group Tested Elsewhere (S4.4.1)

The following BUS_RULES are S4.4.1 port-level and signal-access rules. They are covered in the Section 4.4 test plan, not here:

| Rule ID | Severity | Description | Test File |
|---------|----------|-------------|-----------|
| BUS_PORT_UNKNOWN_BUS | error | BUS port references BUS name not declared in project | 4_4_BUS_PORT_UNKNOWN_BUS-unknown_bus_name.jz |
| BUS_PORT_NOT_BUS | error | BUS member access used on non-BUS port | 4_4_BUS_PORT_NOT_BUS-member_on_non_bus.jz |
| BUS_PORT_ARRAY_COUNT_INVALID | error | BUS array count must be a positive integer constant expression | 4_4_BUS_PORT_ARRAY_COUNT_INVALID-bad_array_count.jz |
| BUS_PORT_INVALID_ROLE | error | BUS port role must be SOURCE or TARGET | 4_4_BUS_PORT_INVALID_ROLE-invalid_bus_role.jz |
| BUS_PORT_INDEX_REQUIRED | error | Arrayed BUS access requires an explicit index or wildcard | 4_4_BUS_PORT_INDEX_REQUIRED-arrayed_bus_no_index.jz |
| BUS_PORT_INDEX_NOT_ARRAY | error | Indexed BUS access requires an arrayed BUS port | 4_4_BUS_PORT_INDEX_NOT_ARRAY-indexed_scalar_bus.jz |
| BUS_PORT_INDEX_OUT_OF_RANGE | error | BUS port index is outside the declared range | 4_4_BUS_PORT_INDEX_OUT_OF_RANGE-bus_index_bounds.jz |
| BUS_SIGNAL_UNDEFINED | error | BUS signal does not exist in BUS definition | -- (no test file) |
| BUS_SIGNAL_READ_FROM_WRITABLE | error | Read access to writable BUS signal is not allowed | -- (no test file) |
| BUS_SIGNAL_WRITE_TO_READABLE | error | Write access to readable BUS signal is not allowed | -- (no test file) |
| BUS_WILDCARD_WIDTH_MISMATCH | error | BUS wildcard assignment requires RHS width of 1 or array count | -- (no test file) |
| BUS_TRISTATE_MISMATCH | error | Only writable BUS signals may be assigned 'z' for tri-state | -- (no test file) |
