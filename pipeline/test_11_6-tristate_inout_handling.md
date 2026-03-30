# Test Plan: 11.6 Handling of INOUT Ports and External Pins

**Specification Reference:** Section 11.6 of jz-hdl-specification.md

## 1. Objective

Verify that INOUT ports are not transformed by `--tristate-default` (preserving tri-state for external interfaces), that internal tri-state drivers connected to INOUT ports have their output-enable conditions correctly extracted, and that blackbox port tri-state connections are handled properly.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | INOUT port preserved | INOUT port with z capability and flag | Tri-state preserved, not transformed |
| 2 | OE extraction success | Internal driver to INOUT with clear enable condition | Output-enable correctly extracted |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | OE extraction failure | Internal driver to INOUT with ambiguous enable | Error: TRISTATE_TRANSFORM_OE_EXTRACT_FAIL |
| 2 | Blackbox port tri-state | Tri-state signal driven by blackbox port | Error: TRISTATE_TRANSFORM_BLACKBOX_PORT |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Bidirectional bus | INOUT port used for bidirectional data | Preserved with correct OE |
| 2 | Nested INOUT through hierarchy | INOUT propagated through module instances | Correctly preserved at all levels |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | OE extraction failure | Error | TRISTATE_TRANSFORM_OE_EXTRACT_FAIL | error | S11.7 |
| 2 | Blackbox port connection | Error | TRISTATE_TRANSFORM_BLACKBOX_PORT | error | S11.7 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `11_GND_6_HAPPY_PATH-tristate_inout_ok.jz` | — | Happy-path: INOUT port tri-state preserved correctly under GND transform |

## 5. Rules Matrix

### 5.1 Rules Tested

The happy-path test `11_GND_6_HAPPY_PATH-tristate_inout_ok.jz` validates correct INOUT handling. No new diagnostic rules are introduced by this subsection directly; error rules are defined in Section 11.7.
| TRISTATE_TRANSFORM_OE_EXTRACT_FAIL | error | S11.7 Could not extract output-enable condition from tri-state port; _oe driven high as fallback | 11_GND_7_TRISTATE_TRANSFORM_OE_EXTRACT_FAIL-ambiguous_oe.jz |
### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| TRISTATE_TRANSFORM_BLACKBOX_PORT | error | Bug: test exists (`11_GND_7_TRISTATE_TRANSFORM_BLACKBOX_PORT-blackbox_tristate.jz`) but rule has a known compiler bug |
