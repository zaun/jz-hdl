# Test Plan: 6.8 BUS Aggregation

**Specification Reference:** Section 6.8 of jz-hdl-specification.md

## 1. Objective

Verify BUS definition in project scope, signal declarations within BUS (IN/OUT/INOUT with widths), BUS port usage in modules (SOURCE/TARGET roles), and role-based direction resolution.

## 2. Instrumentation Strategy

- **Span: `sem.bus_definition`** — attributes: `bus_id`, `signal_count`, `signal_directions`.

## 3. Test Scenarios

### 3.1 Happy Path
1. Define BUS with IN/OUT/INOUT signals
2. Module with BUS SOURCE port
3. Module with BUS TARGET port
4. Bus-to-bus aggregation between SOURCE and TARGET

### 3.2 Boundary/Edge Cases
1. BUS with single signal
2. BUS with many signals (20+)

### 3.3 Negative Testing
1. Duplicate BUS name — Error
2. BUS signal name conflict within BUS
3. Two SOURCE ports on same BUS without tri-state — Error

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Duplicate BUS name | Error | BUS_DUPLICATE_NAME | S6.8 |
| 2 | Two SOURCE same BUS | Error (multi-driver) | — | S4.4.1/S1.5 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_project_blocks.c` | BUS definition parsing | Token stream |
| `driver_project.c` | BUS validation | Integration test |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| BUS_DUPLICATE_NAME | Duplicate BUS definition | Neg 1 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| BUS_SIGNAL_DUPLICATE | S6.8 | Duplicate signal name within BUS |
