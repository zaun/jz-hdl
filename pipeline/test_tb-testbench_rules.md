# Test Plan: Testbench Rules (TESTBENCH group)
**Specification Reference:** Testbench validation rules from `compiler/src/rules.c`

> **Important: Test Mode Note**
> All TESTBENCH rules fire during `--test` mode, not `--info --lint`. The standard validation test harness uses `--info --lint`, so these tests require special handling. The one exception is `TB_WRONG_TOOL`, which fires when a file containing `@testbench` blocks is run with `--lint` instead of `--test`. Validation `.jz`/`.out` pairs for these rules must invoke the compiler with `--test` (or `--lint` for TB_WRONG_TOOL) accordingly.

## 1. Objective

Verify all TESTBENCH diagnostic rules are correctly defined and enforced. These rules apply to `@testbench` blocks processed via `--test` mode and ensure correct testbench structure, port connectivity, type safety, and directive ordering.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Valid testbench | Well-formed @testbench with @new, @setup, @clock, @update, @expect | Clean run via --test |
| 2 | Multiple TEST blocks | @testbench with several TEST blocks, each with one @new | Clean run via --test |
| 3 | Minimal TEST | TEST with @new, @setup, and @clock only (no @update/@expect) | Clean run via --test |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Wrong tool | File with @testbench run via --lint | Error directing user to --test | TB_WRONG_TOOL |
| 2 | Project mixed | File with both @project and @testbench | Error: cannot mix top-level block types | TB_PROJECT_MIXED |
| 3 | Module not found | @testbench references non-existent module name | Error: module not in scope | TB_MODULE_NOT_FOUND |
| 4 | Port not connected | @new omits one or more module ports | Error: all ports must be connected | TB_PORT_NOT_CONNECTED |
| 5 | Port width mismatch | @new port width differs from module PORT declaration | Error: width mismatch | TB_PORT_WIDTH_MISMATCH |
| 6 | Invalid @new RHS | @new RHS references identifier that is not a CLOCK or WIRE | Error: invalid RHS type | TB_NEW_RHS_INVALID |
| 7 | @setup position (before @new) | @setup appears before @new in a TEST block | Error: misplaced @setup | TB_SETUP_POSITION |
| 8 | @setup position (duplicated) | Two @setup directives in the same TEST block | Error: duplicate @setup | TB_SETUP_POSITION |
| 9 | Clock not declared | @clock references identifier not in CLOCK block | Error: undeclared clock | TB_CLOCK_NOT_DECLARED |
| 10 | Clock cycle not positive (zero) | @clock with cycle=0 | Error: cycle must be positive | TB_CLOCK_CYCLE_NOT_POSITIVE |
| 11 | Clock cycle not positive (negative) | @clock with cycle=-1 | Error: cycle must be positive | TB_CLOCK_CYCLE_NOT_POSITIVE |
| 12 | Update not wire | @update assigns to a CLOCK identifier | Error: can only assign WIREs | TB_UPDATE_NOT_WIRE |
| 13 | Update clock assign | @update assigns to the clock signal directly | Error: cannot assign clocks | TB_UPDATE_CLOCK_ASSIGN |
| 14 | Expect width mismatch | @expect_equal value literal width differs from signal width | Error: width mismatch | TB_EXPECT_WIDTH_MISMATCH |
| 15 | No test blocks | @testbench with CLOCK and WIRE but no TEST blocks | Error: no TEST blocks | TB_NO_TEST_BLOCKS |
| 16 | Multiple @new | TEST block contains two @new instantiations | Error: duplicate @new | TB_MULTIPLE_NEW |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Multiple TEST blocks | @testbench with several TEST blocks, each with exactly one @new | Valid if each TEST is well-formed |
| 2 | @setup with no updates | @setup present but no @update directives follow | Valid (minimal test) |
| 3 | Module with no ports | @testbench for a module with empty PORT block, @new with no connections | Valid (vacuously all ports connected) |

## 3. Detailed Test Scenarios

### 3.1 TB_WRONG_TOOL

**Description:** When a file containing `@testbench` blocks is processed with `--info --lint` instead of `--test`, the compiler must emit TB_WRONG_TOOL to direct the user to the correct mode.

**Triggering construct:**
```jz
@module counter
    PORT { IN [1] clk; OUT [8] count; }
    WIRE { tmp [8]; }
    ASYNCHRONOUS { count <= tmp; tmp <= 8'h00; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { count [8]; }
    TEST "basic" {
        @new dut counter { clk [1] = clk; count [8] = count; }
        @setup { }
        @clock(clk, cycle=1)
    }
@endtb
```

**Invocation:** `jz-hdl --info --lint file.jz` (wrong tool -- should be `--test`)

**Expected output:** Error TB_WRONG_TOOL: "File contains @testbench blocks; use --test to run testbenches"

---

### 3.2 TB_PROJECT_MIXED

**Description:** A single file must not contain both `@project` and `@testbench` top-level blocks. The compiler rejects files that mix these two compilation units.

**Triggering construct:**
```jz
@project my_project
    CONFIG { CHIP = "GW1NR-LV9QN88PC6/I5"; }
    CLOCKS { sys_clk FREQ=27MHz PIN=52; }
@endproj

@module counter
    PORT { IN [1] clk; OUT [8] count; }
    WIRE { tmp [8]; }
    ASYNCHRONOUS { count <= tmp; tmp <= 8'h00; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { count [8]; }
    TEST "basic" {
        @new dut counter { clk [1] = clk; count [8] = count; }
        @setup { }
        @clock(clk, cycle=1)
    }
@endtb
```

**Expected output:** Error TB_PROJECT_MIXED: "TB-020 A file may not contain both @project and @testbench"

---

### 3.3 TB_MODULE_NOT_FOUND

**Description:** The module name in `@testbench <name>` must refer to a module defined (or in scope) within the same compilation unit. If the name does not match any known module, the compiler emits this error.

**Triggering construct:**
```jz
@module counter
    PORT { IN [1] clk; OUT [8] count; }
    WIRE { tmp [8]; }
    ASYNCHRONOUS { count <= tmp; tmp <= 8'h00; }
@endmod

@testbench nonexistent_module
    CLOCK { clk; }
    WIRE { count [8]; }
    TEST "basic" {
        @new dut nonexistent_module { clk [1] = clk; count [8] = count; }
        @setup { }
        @clock(clk, cycle=1)
    }
@endtb
```

**Expected output:** Error TB_MODULE_NOT_FOUND: "TB-001 @testbench module name must refer to a module in scope"

---

### 3.4 TB_PORT_NOT_CONNECTED

**Description:** Every port declared in the module's PORT block must have a corresponding connection in the `@new` instantiation. Omitting any port is an error.

**Triggering construct:**
```jz
@module counter
    PORT {
        IN  [1] clk;
        IN  [1] rst_n;
        OUT [8] count;
    }
    REGISTER { cnt [8] = 8'h00; }
    ASYNCHRONOUS { count <= cnt; }
    SYNCHRONOUS(CLK=clk RESET=rst_n RESET_ACTIVE=Low) { cnt <= cnt + 8'h01; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { rst_n [1]; count [8]; }
    TEST "missing port" {
        @new dut counter {
            clk [1] = clk;
            count [8] = count;
        }
        // rst_n is not connected -- error
        @setup { rst_n <= 1'b0; }
        @clock(clk, cycle=1)
    }
@endtb
```

**Expected output:** Error TB_PORT_NOT_CONNECTED: "TB-002 All module ports must be connected in @new"

---

### 3.5 TB_PORT_WIDTH_MISMATCH

**Description:** The width specified for a port in the `@new` instantiation must exactly match the width declared in the module's PORT block.

**Triggering construct:**
```jz
@module counter
    PORT {
        IN  [1] clk;
        OUT [8] count;
    }
    WIRE { tmp [8]; }
    ASYNCHRONOUS { count <= tmp; tmp <= 8'h00; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { count [4]; }
    TEST "width mismatch" {
        @new dut counter {
            clk [1] = clk;
            count [4] = count;
        }
        // count is [4] in @new but [8] in module -- error
        @setup { }
        @clock(clk, cycle=1)
    }
@endtb
```

**Expected output:** Error TB_PORT_WIDTH_MISMATCH: "TB-003 Port width must match module declared width"

---

### 3.6 TB_NEW_RHS_INVALID

**Description:** The right-hand side of each port mapping in `@new` must refer to a testbench CLOCK or WIRE identifier. References to other kinds of identifiers (or undeclared names) are rejected.

**Triggering construct:**
```jz
@module counter
    PORT {
        IN  [1] clk;
        OUT [8] count;
    }
    WIRE { tmp [8]; }
    ASYNCHRONOUS { count <= tmp; tmp <= 8'h00; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { count [8]; }
    TEST "invalid RHS" {
        @new dut counter {
            clk [1] = clk;
            count [8] = undeclared_name;
        }
        // undeclared_name is not a CLOCK or WIRE -- error
        @setup { }
        @clock(clk, cycle=1)
    }
@endtb
```

**Expected output:** Error TB_NEW_RHS_INVALID: "TB-004 @new RHS must be a testbench CLOCK or WIRE"

---

### 3.7 TB_SETUP_POSITION

**Description:** Each TEST block must contain exactly one `@setup` directive, and it must appear after `@new` but before any `@clock`, `@update`, or `@expect` directives. A missing, duplicated, or misplaced `@setup` triggers this error.

**Triggering construct (before @new):**
```jz
@module counter
    PORT { IN [1] clk; OUT [8] count; }
    WIRE { tmp [8]; }
    ASYNCHRONOUS { count <= tmp; tmp <= 8'h00; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { count [8]; }
    TEST "setup before new" {
        @setup { }
        @new dut counter {
            clk [1] = clk;
            count [8] = count;
        }
        // @setup before @new -- error
        @clock(clk, cycle=1)
    }
@endtb
```

**Triggering construct (duplicated):**
```jz
@module counter
    PORT { IN [1] clk; OUT [8] count; }
    WIRE { tmp [8]; }
    ASYNCHRONOUS { count <= tmp; tmp <= 8'h00; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { count [8]; }
    TEST "duplicate setup" {
        @new dut counter {
            clk [1] = clk;
            count [8] = count;
        }
        @setup { }
        @setup { }
        // two @setup directives -- error
        @clock(clk, cycle=1)
    }
@endtb
```

**Expected output:** Error TB_SETUP_POSITION: "TB-005 @setup must appear exactly once per TEST, after @new, before other directives"

---

### 3.8 TB_CLOCK_NOT_DECLARED

**Description:** The clock identifier used in `@clock(name, cycle=N)` must refer to a clock declared in the testbench's CLOCK block.

**Triggering construct:**
```jz
@module counter
    PORT { IN [1] clk; OUT [8] count; }
    WIRE { tmp [8]; }
    ASYNCHRONOUS { count <= tmp; tmp <= 8'h00; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { count [8]; }
    TEST "undeclared clock" {
        @new dut counter {
            clk [1] = clk;
            count [8] = count;
        }
        @setup { }
        @clock(fast_clk, cycle=5)
        // fast_clk is not in CLOCK block -- error
    }
@endtb
```

**Expected output:** Error TB_CLOCK_NOT_DECLARED: "TB-007 @clock clock identifier must refer to a declared CLOCK"

---

### 3.9 TB_CLOCK_CYCLE_NOT_POSITIVE

**Description:** The cycle count parameter in `@clock(name, cycle=N)` must be a positive integer (>= 1). Zero or negative values are rejected.

**Triggering construct (zero):**
```jz
@module counter
    PORT { IN [1] clk; OUT [8] count; }
    WIRE { tmp [8]; }
    ASYNCHRONOUS { count <= tmp; tmp <= 8'h00; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { count [8]; }
    TEST "zero cycles" {
        @new dut counter {
            clk [1] = clk;
            count [8] = count;
        }
        @setup { }
        @clock(clk, cycle=0)
        // cycle=0 -- error
    }
@endtb
```

**Triggering construct (negative):**
```jz
// Same structure as above but with:
        @clock(clk, cycle=-1)
        // cycle=-1 -- error
```

**Expected output:** Error TB_CLOCK_CYCLE_NOT_POSITIVE: "TB-008 @clock cycle count must be a positive integer"

---

### 3.10 TB_UPDATE_NOT_WIRE

**Description:** The `@update` directive may only assign to testbench WIRE identifiers. Assigning to any other kind of identifier (e.g., a port name, register, or undeclared name) is an error.

**Triggering construct:**
```jz
@module counter
    PORT { IN [1] clk; IN [1] rst_n; OUT [8] count; }
    REGISTER { cnt [8] = 8'h00; }
    ASYNCHRONOUS { count <= cnt; }
    SYNCHRONOUS(CLK=clk RESET=rst_n RESET_ACTIVE=Low) { cnt <= cnt + 8'h01; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { rst_n [1]; count [8]; }
    TEST "update non-wire" {
        @new dut counter {
            clk [1] = clk;
            rst_n [1] = rst_n;
            count [8] = count;
        }
        @setup { rst_n <= 1'b0; }
        @clock(clk, cycle=1)
        @update {
            not_a_wire <= 1'b1;
        }
        // not_a_wire is not declared as a WIRE -- error
    }
@endtb
```

**Expected output:** Error TB_UPDATE_NOT_WIRE: "TB-009 @update may only assign testbench WIRE identifiers"

---

### 3.11 TB_UPDATE_CLOCK_ASSIGN

**Description:** The `@update` directive may not assign to clock signals. Clocks are driven by the simulation engine; user code must not override them.

**Triggering construct:**
```jz
@module counter
    PORT { IN [1] clk; OUT [8] count; }
    WIRE { tmp [8]; }
    ASYNCHRONOUS { count <= tmp; tmp <= 8'h00; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { count [8]; }
    TEST "assign clock" {
        @new dut counter {
            clk [1] = clk;
            count [8] = count;
        }
        @setup { }
        @clock(clk, cycle=1)
        @update {
            clk <= 1'b1;
        }
        // clk is a CLOCK, not assignable -- error
    }
@endtb
```

**Expected output:** Error TB_UPDATE_CLOCK_ASSIGN: "TB-010 @update may not assign clock signals"

---

### 3.12 TB_EXPECT_WIDTH_MISMATCH

**Description:** The value literal in `@expect_equal(signal, value)` must have a width that matches the declared width of the signal being checked.

**Triggering construct:**
```jz
@module counter
    PORT { IN [1] clk; OUT [8] count; }
    WIRE { tmp [8]; }
    ASYNCHRONOUS { count <= tmp; tmp <= 8'h00; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { count [8]; }
    TEST "expect width mismatch" {
        @new dut counter {
            clk [1] = clk;
            count [8] = count;
        }
        @setup { }
        @clock(clk, cycle=1)
        @expect_equal(count, 4'h0)
        // count is [8] but value is 4-bit -- error
    }
@endtb
```

**Expected output:** Error TB_EXPECT_WIDTH_MISMATCH: "TB-011 @expect value width must match signal width"

---

### 3.13 TB_NO_TEST_BLOCKS

**Description:** A `@testbench` block must contain at least one TEST block. An empty testbench with only CLOCK/WIRE declarations is an error.

**Triggering construct:**
```jz
@module counter
    PORT { IN [1] clk; OUT [8] count; }
    WIRE { tmp [8]; }
    ASYNCHRONOUS { count <= tmp; tmp <= 8'h00; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { count [8]; }
    // No TEST blocks -- error
@endtb
```

**Expected output:** Error TB_NO_TEST_BLOCKS: "TB-012 @testbench must contain at least one TEST block"

---

### 3.14 TB_MULTIPLE_NEW

**Description:** Each TEST block must contain exactly one `@new` instantiation. Having two or more `@new` directives in the same TEST is an error.

**Triggering construct:**
```jz
@module counter
    PORT { IN [1] clk; OUT [8] count; }
    WIRE { tmp [8]; }
    ASYNCHRONOUS { count <= tmp; tmp <= 8'h00; }
@endmod

@testbench counter
    CLOCK { clk; }
    WIRE { count [8]; }
    TEST "two instantiations" {
        @new dut1 counter {
            clk [1] = clk;
            count [8] = count;
        }
        @new dut2 counter {
            clk [1] = clk;
            count [8] = count;
        }
        // Two @new in one TEST -- error
        @setup { }
        @clock(clk, cycle=1)
    }
@endtb
```

**Expected output:** Error TB_MULTIPLE_NEW: "TB-013 Each TEST must contain exactly one @new instantiation"

## 4. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity | Test Mode |
|---|----------|---------------------|-----------------|----------|-----------|
| 1 | @testbench file run with --lint | @testbench block processed via --lint | TB_WRONG_TOOL | error | --lint |
| 2 | @project + @testbench in same file | Both @project and @testbench present | TB_PROJECT_MIXED | error | --test |
| 3 | @testbench references missing module | @testbench nonexistent_module | TB_MODULE_NOT_FOUND | error | --test |
| 4 | Port omitted from @new | @new without all ports connected | TB_PORT_NOT_CONNECTED | error | --test |
| 5 | Port width differs from module | @new port width != module PORT width | TB_PORT_WIDTH_MISMATCH | error | --test |
| 6 | @new RHS not CLOCK/WIRE | @new with undeclared or invalid RHS | TB_NEW_RHS_INVALID | error | --test |
| 7 | @setup before @new | @setup placed before @new in TEST | TB_SETUP_POSITION | error | --test |
| 8 | @setup duplicated in TEST | Two @setup directives in same TEST | TB_SETUP_POSITION | error | --test |
| 9 | Undeclared clock in @clock | @clock(unknown_id, cycle=N) | TB_CLOCK_NOT_DECLARED | error | --test |
| 10 | Zero cycle count | @clock(clk, cycle=0) | TB_CLOCK_CYCLE_NOT_POSITIVE | error | --test |
| 11 | Negative cycle count | @clock(clk, cycle=-1) | TB_CLOCK_CYCLE_NOT_POSITIVE | error | --test |
| 12 | @update targets non-wire | @update assigns undeclared identifier | TB_UPDATE_NOT_WIRE | error | --test |
| 13 | @update assigns clock | @update { clk <= 1'b1; } | TB_UPDATE_CLOCK_ASSIGN | error | --test |
| 14 | @expect value width wrong | @expect_equal(count, 4'h0) where count is [8] | TB_EXPECT_WIDTH_MISMATCH | error | --test |
| 15 | No TEST blocks in @testbench | @testbench with only CLOCK/WIRE, no TEST | TB_NO_TEST_BLOCKS | error | --test |
| 16 | Two @new in one TEST | Duplicate @new directives | TB_MULTIPLE_NEW | error | --test |

## 5. Existing Validation Tests

No validation tests exist for TESTBENCH rules yet. Most rules require `--test` mode and are not reachable via `--info --lint`. The exception is TB_WRONG_TOOL, which fires under `--info --lint` when the file contains `@testbench` blocks; a validation `.jz`/`.out` pair for this rule is planned but does not yet exist. Test files for the remaining rules will need to be run with the `--test` flag rather than the standard validation harness.

## 6. Rules Matrix

### 6.1 Rules Tested

> **Note:** All TESTBENCH rules are tested via `--test` mode, not `--info --lint`. The exception is TB_WRONG_TOOL, which specifically tests the `--lint` mode rejection path. Validation test `.jz`/`.out` pairs for these rules require the test harness to invoke the compiler with the appropriate mode flag.

| Rule ID | Severity | Test Mode | Test Scenario(s) |
|---------|----------|-----------|-------------------|
| TB_PROJECT_MIXED | error | --test | 3.2, Error Cases #2 |
| TB_MODULE_NOT_FOUND | error | --test | 3.3, Error Cases #3 |
| TB_PORT_NOT_CONNECTED | error | --test | 3.4, Error Cases #4 |
| TB_PORT_WIDTH_MISMATCH | error | --test | 3.5, Error Cases #5 |
| TB_NEW_RHS_INVALID | error | --test | 3.6, Error Cases #6 |
| TB_SETUP_POSITION | error | --test | 3.7, Error Cases #7-8 |
| TB_CLOCK_NOT_DECLARED | error | --test | 3.8, Error Cases #9 |
| TB_CLOCK_CYCLE_NOT_POSITIVE | error | --test | 3.9, Error Cases #10-11 |
| TB_UPDATE_NOT_WIRE | error | --test | 3.10, Error Cases #12 |
| TB_UPDATE_CLOCK_ASSIGN | error | --test | 3.11, Error Cases #13 |
| TB_EXPECT_WIDTH_MISMATCH | error | --test | 3.12, Error Cases #14 |
| TB_NO_TEST_BLOCKS | error | --test | 3.13, Error Cases #15 |
| TB_MULTIPLE_NEW | error | --test | 3.14, Error Cases #16 |

### 6.2 Rules Not Tested

| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| TB_WRONG_TOOL | error | Validation test file for --lint rejection path does not yet exist; scenario is documented in 3.1 |
