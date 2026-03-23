# Test Plan: 10.6 Template Exclusive Assignment Compatibility
**Specification Reference:** Section 10.6 of jz-hdl-specification.md

## 1. Objective
Verify template expansion does not bypass the exclusive assignment rule. Multiple @apply to the same identifier must target structurally exclusive paths or non-overlapping bit slices. Violations are detected after expansion using standard assignment rules.

## 2. Test Scenarios

### 2.1 Happy Path
1. @apply in exclusive IF/ELSE branches targeting same signal — valid (structurally exclusive)
2. @apply with IDX targeting non-overlapping bit slices — valid
3. Single @apply to a signal — valid (no conflict possible)

### 2.2 Error Cases
1. Two @apply writing same wire bits in same async path — Error (ASSIGN_MULTIPLE_SAME_BITS)
2. Two @apply writing same register bits in same sync path — Error (SYNC_MULTI_ASSIGN_SAME_REG_BITS)

### 2.3 Edge Cases
1. @apply with count=2 where IDX slices overlap — Error after expansion
2. @apply in nested IF branches with partial exclusivity — depends on path analysis

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Two @apply same wire bits, async block | Error | ASSIGN_MULTIPLE_SAME_BITS | S10.6 via S1.5/S5.2 |
| 2 | Two @apply same register bits, sync block | Error | SYNC_MULTI_ASSIGN_SAME_REG_BITS | S10.6 via S5.2/S8.1 |

## 4. Existing Validation Tests
| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `10_6_ASSIGN_MULTIPLE_SAME_BITS-template_double_apply_async.jz` | ASSIGN_MULTIPLE_SAME_BITS | Double @apply to same wire in ASYNCHRONOUS block |
| `10_6_SYNC_MULTI_ASSIGN_SAME_REG_BITS-template_double_apply_sync.jz` | SYNC_MULTI_ASSIGN_SAME_REG_BITS | Double @apply to same register in SYNCHRONOUS block |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| ASSIGN_MULTIPLE_SAME_BITS | Same bits assigned more than once on a single execution path (via template expansion) | Error 1 |
| SYNC_MULTI_ASSIGN_SAME_REG_BITS | Same register bits assigned more than once along any execution path in SYNCHRONOUS block (via template expansion) | Error 2 |

### 5.2 Rules Not Tested
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | — | All S10.6 rules covered |
