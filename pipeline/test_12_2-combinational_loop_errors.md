# Test Plan: 12.2 Combinational Loop Errors
**Specification Reference:** Section 12.2 of jz-hdl-specification.md
## 1. Objective
Verify combinational loop detection: unconditional cycles (a=b; b=a), flow-sensitive exclusion (IF/ELSE branches don't create cycles), and multi-signal cycles.
## 2. Instrumentation Strategy
- **Span: `sem.comb_loop`** — attributes: `cycle_signals`, `cycle_length`, `is_conditional`.
- **Event: `comb_loop.detected`** — Fires when unconditional cycle found.
## 3. Test Scenarios
### 3.1 Happy Path
1. Flow-sensitive no loop: `IF(sel) { a=b; } ELSE { b=a; }` — valid
2. Linear chain: `a=b; c=a;` — no cycle
3. Complex valid graph with no cycles
### 3.2 Boundary/Edge Cases
1. Self-assignment through expression: `a = a + 1'b0;` — may be cycle
2. Long chain (10+ signals) without cycle
### 3.3 Negative Testing
1. Direct cycle: `a = b; b = a;` — Error
2. Three-signal cycle: `a = b; b = c; c = a;` — Error
3. Cycle through instance port: instance OUT drives its own IN — Error
4. Conditional cycle visible on same path: `a = b; IF(c) { b = a; }` — Error (b=a is conditional but a=b is unconditional)
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `a=b; b=a;` | Error | COMB_LOOP_DIRECT | S12.2 |
| 2 | Three-signal cycle | Error | COMB_LOOP_INDIRECT | S12.2 |
| 3 | `IF(s){a=b;}ELSE{b=a;}` | Valid | — | Flow-sensitive |
## 5. Integration Points
| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_comb.c` | Combinational loop detection | Integration test |
| `driver_flow.c` | Flow-sensitive analysis | Integration test |
| `driver_net.c` | Net/alias graph | Integration test |
## 6. Rules Matrix
### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| COMB_LOOP_DIRECT | Direct 2-signal cycle | Neg 1 |
| COMB_LOOP_INDIRECT | Multi-signal cycle | Neg 2 |
### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| COMB_LOOP_THROUGH_INSTANCE | S12.2 | Cycle through instance ports |
