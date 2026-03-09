---
title: Errors and Diagnostics
lang: en-US

layout: doc
outline: deep
---

# Errors and Diagnostics

## Severity Levels

Every diagnostic emitted by the JZ-HDL compiler has one of three severity levels:

| Severity | Meaning | Effect |
| --- | --- | --- |
| **ERROR** | The design violates a language rule and cannot be compiled. | Compilation stops. No output is produced. |
| **WARNING** | The design is legal but likely contains an unintended issue (unused declarations, missing DEFAULT clauses, etc.). | Compilation continues. The design may work, but you should investigate. |
| **INFO** | Advisory information about the design (e.g., runtime division-by-zero risk, blackbox overrides that cannot be validated). | Compilation continues. Informational only — no action required unless the note is relevant to your intent. |

### Controlling Diagnostic Output

| CLI Flag | Effect |
| --- | --- |
| `--info` | Show INFO-level diagnostics in addition to errors and warnings. Without this flag, only errors and warnings are displayed. |
| `--warn-as-error` | Promote all warnings to errors. Useful for CI pipelines where you want zero tolerance for potential issues. |
| `--Wno-group=NAME` | Suppress all diagnostics in the named group. For example, `--Wno-group=GENERAL_WARNINGS` silences unused register warnings, dead code warnings, etc. |
| `--color` | Force colored output. Errors appear in red, warnings in yellow, and info in blue. |

### Listing All Rules

Use `--lint-rules` to print every diagnostic rule ID, its severity, and a short description:

```bash
jz-hdl design.jz --lint-rules
```

This is useful for discovering the exact rule ID to reference when suppressing diagnostics with `--Wno-group`, or when searching this page for details about a specific diagnostic.

## Diagnostic Reference

The sections below list all diagnostic rules organized by category. Each entry shows the rule ID, severity, typical cause, and how to fix it.

## Parse & Lexical

### PARSE.COMMENT_IN_TOKEN — Comment appears inside a token
- Severity: ERROR
- Cause: `/*` or `//` started inside an identifier/number/operator token.
- Fix: Move comment boundaries so they appear between tokens or on separate lines.

### PARSE.COMMENT_NESTED_BLOCK — Nested block comment detected
- Severity: ERROR
- Cause: `/* ... /* ... */ ... */` — nested block comments are not supported.
- Fix: Remove inner `/*` or close the outer block before starting another; use line comments instead.

### PARSE.DIRECTIVE_INVALID_CONTEXT — Directive used in invalid location
- Severity: ERROR
- Cause: Structural directives (@project, @module, @import, @new, @blackbox, @endproj, @endmod, etc.) placed where not allowed.
- Fix: Place directives only in permitted locations per the spec (e.g., @import only immediately inside @project).

### LEXICAL.ID_SYNTAX_INVALID — Identifier invalid
- Severity: ERROR
- Cause: Identifier violates regex (bad characters, starts with digit) or length > 255.
- Fix: Rename to valid identifier per rules: [A‑Za‑z_][A‑Za‑z0‑9_]{0,254} and not a single underscore.

### LEXICAL.ID_SINGLE_UNDERSCORE — Illegal use of single underscore
- Severity: ERROR
- Cause: `_` used as regular identifier outside no‑connect context (allowed only in @top/@new port bindings).
- Fix: Use a named identifier or `_` only in permitted instantiation/top-level no‑connect positions.

### PARSE.KEYWORD_AS_IDENTIFIER — Reserved keyword used as identifier
- Severity: ERROR
- Cause: Using a reserved uppercase keyword as an identifier.
- Fix: Rename; reserved keywords (IF, ELSE, CONST, IN, OUT, etc.) cannot be used as identifiers.

### PARSE.IF_COND_MISSING_PARENS — IF/ELIF condition missing parentheses
- Severity: ERROR
- Cause: `IF cond {` instead of `IF (cond) {`.
- Fix: Parenthesize the condition.

### PARSE.LIT_DECIMAL_HAS_XZ — Decimal literal contains x or z
- Severity: ERROR
- Cause: Decimal literals do not support `x`/`z` digits (e.g., `8'd10x`).
- Fix: Use hex or binary base for x/z values.

### PARSE.LIT_INVALID_DIGIT_FOR_BASE — Invalid digit for literal base
- Severity: ERROR
- Cause: Literal contains a digit not valid for its base (e.g., `8'b102`).
- Fix: Use valid digits for the base.

### PARSE.INSTANCE_UNDEFINED_MODULE — Instantiation references non-existent module
- Severity: ERROR
- Fix: Ensure the module is defined or imported before use.

---

## Literals, Types & Widths

### LITERALS_AND_TYPES.LIT_UNSIZED — Unsized literal used
- Severity: ERROR
- Cause: Literal missing width (e.g., `'hFF`).
- Fix: Provide explicit width: `8'hFF`.

### LITERALS_AND_TYPES.LIT_OVERFLOW — Literal intrinsic width > declared width
- Severity: ERROR
- Cause: Value requires more bits than declared.
- Fix: Increase literal width or shorten the value so intrinsic width ≤ declared width.

### LITERALS_AND_TYPES.LIT_UNDERSCORE_AT_EDGES — Underscore at start/end of literal
- Severity: ERROR
- Cause: Literal has leading or trailing `_`.
- Fix: Remove underscores at edges.

### LITERALS_AND_TYPES.LIT_UNDEFINED_CONST_WIDTH — Undefined CONST used as width
- Severity: ERROR
- Cause: Using a module CONST name in a literal width that does not exist.
- Fix: Declare the CONST or use a numeric width; CONST scope is module-local.

### WIDTHS_AND_SLICING.WIDTH_NONPOSITIVE_OR_NONINT — Invalid declared width
- Severity: ERROR
- Cause: Declared width <= 0 or not integer.
- Fix: Use positive integer or valid CONST resolving to positive integer.

### WIDTHS_AND_SLICING.SLICE_MSB_LESS_THAN_LSB — Slice MSB < LSB
- Severity: ERROR
- Cause: `signal[H:L]` where H < L.
- Fix: Reverse indices or correct indices so MSB ≥ LSB.

### WIDTHS_AND_SLICING.SLICE_INDEX_OUT_OF_RANGE — Slice index out of bounds
- Severity: ERROR
- Cause: Index < 0 or >= signal width.
- Fix: Use valid indices within [0, width−1].

### OPERATORS_AND_EXPRESSIONS.TYPE_BINOP_WIDTH_MISMATCH — Binary op operands differ widths
- Severity: ERROR
- Cause: Operators requiring equal widths (e.g., `+`, `&`, `==`) receive different widths.
- Fix: Make operand widths equal via explicit extension (`=z`/`=s` context) or resize using concatenation or intrinsics.

---

## Operators & Expressions

### OPERATORS_AND_EXPRESSIONS.UNARY_ARITH_MISSING_PARENS — Unary arithmetic missing parentheses
- Severity: ERROR
- Cause: Used `-flag` instead of `(-flag)`.
- Fix: Parenthesize unary arithmetic: `(-flag)`.

### OPERATORS_AND_EXPRESSIONS.LOGICAL_WIDTH_NOT_1 — `&&`/`||`/`!` used on multi-bit
- Severity: ERROR
- Cause: Logical operators require width‑1 operands.
- Fix: Reduce to a single-bit condition (e.g., `(a != 0)`), or use bitwise operators.

### OPERATORS_AND_EXPRESSIONS.TERNARY_COND_WIDTH_NOT_1 — Ternary condition not width‑1
- Severity: ERROR
- Fix: Ensure the condition expression is width‑1.

### OPERATORS_AND_EXPRESSIONS.TERNARY_BRANCH_WIDTH_MISMATCH — Branches widths differ
- Severity: ERROR
- Cause: true/false expressions have different widths.
- Fix: Make branches equal width by extending or slicing.

### OPERATORS_AND_EXPRESSIONS.CONCAT_EMPTY — Empty concatenation `{}` used
- Severity: ERROR
- Fix: Remove empty concat or supply elements.

### OPERATORS_AND_EXPRESSIONS.DIV_CONST_ZERO — Division by constant zero
- Severity: ERROR
- Cause: A compile-time divisor literal = 0.
- Fix: Remove or guard division; use conditional checks.

### OPERATORS_AND_EXPRESSIONS.DIV_UNGUARDED_RUNTIME_ZERO — Divisor may be zero at runtime
- Severity: WARNING
- Cause: A division or modulus with a non-constant divisor is not enclosed in an `IF (divisor != 0)` guard.
- Fix: Add runtime guards: `IF (divisor != 0) ... ELSE ...`.

### OPERATORS_AND_EXPRESSIONS.SPECIAL_DRIVER_IN_EXPRESSION — GND/VCC in expression
- Severity: ERROR
- Cause: GND/VCC may not appear in arithmetic/logical expressions.
- Fix: Use sized literals (`1'b0`, `1'b1`) instead.

### OPERATORS_AND_EXPRESSIONS.SPECIAL_DRIVER_IN_CONCAT — GND/VCC in concatenation
- Severity: ERROR
- Fix: Use sized literals instead of GND/VCC in concatenations.

### OPERATORS_AND_EXPRESSIONS.SPECIAL_DRIVER_SLICED — GND/VCC sliced or indexed
- Severity: ERROR
- Fix: GND/VCC are whole-signal drivers; use sized literals if you need specific bits.

### OPERATORS_AND_EXPRESSIONS.SPECIAL_DRIVER_IN_INDEX — GND/VCC in index expression
- Severity: ERROR
- Fix: Use integer literals for indices.

### OPERATORS_AND_EXPRESSIONS.SBIT_SET_WIDTH_NOT_1 — sbit() set argument not width-1
- Severity: ERROR
- Fix: The third argument to sbit() must be a 1-bit expression.

### OPERATORS_AND_EXPRESSIONS.GBIT_INDEX_OUT_OF_RANGE — gbit() index out of range
- Severity: ERROR
- Fix: Ensure index is within [0, width(source)-1].

### OPERATORS_AND_EXPRESSIONS.GSLICE_INDEX_OUT_OF_RANGE — gslice() index out of range
- Severity: ERROR
- Fix: Ensure index + width <= width(source).

### OPERATORS_AND_EXPRESSIONS.GSLICE_WIDTH_INVALID — gslice() width invalid
- Severity: ERROR
- Fix: Width parameter must be a positive integer constant.

### OPERATORS_AND_EXPRESSIONS.SSLICE_INDEX_OUT_OF_RANGE — sslice() index out of range
- Severity: ERROR
- Fix: Ensure index + width <= width(source).

### OPERATORS_AND_EXPRESSIONS.SSLICE_WIDTH_INVALID — sslice() width invalid
- Severity: ERROR
- Fix: Width parameter must be a positive integer constant.

### OPERATORS_AND_EXPRESSIONS.SSLICE_VALUE_WIDTH_MISMATCH — sslice() value width mismatch
- Severity: ERROR
- Fix: Value argument width must match the width parameter.

### OPERATORS_AND_EXPRESSIONS.OH2B_INPUT_TOO_NARROW — oh2b() source too narrow
- Severity: ERROR
- Fix: oh2b() source must be at least 2 bits wide.

### OPERATORS_AND_EXPRESSIONS.B2OH_WIDTH_INVALID — b2oh() width invalid
- Severity: ERROR
- Fix: Width must be a positive constant >= 2.

### OPERATORS_AND_EXPRESSIONS.PRIENC_INPUT_TOO_NARROW — prienc() source too narrow
- Severity: ERROR
- Fix: prienc() source must be at least 2 bits wide.

### OPERATORS_AND_EXPRESSIONS.BSWAP_WIDTH_NOT_BYTE_ALIGNED — bswap() width not byte-aligned
- Severity: ERROR
- Fix: Source width must be a multiple of 8.

---

## Identifiers & Scope

### IDENTIFIERS_AND_SCOPE.ID_DUP_IN_MODULE — Duplicate identifier in module
- Severity: ERROR
- Cause: Two declarations with same name (ports, wires, registers, consts, instances).
- Fix: Rename one declaration; ensure uniqueness in module scope.

### IDENTIFIERS_AND_SCOPE.MODULE_NAME_DUP_IN_PROJECT — Module name not unique project-wide
- Severity: ERROR
- Fix: Rename module or resolve import duplication.

### IDENTIFIERS_AND_SCOPE.INSTANCE_NAME_DUP_IN_MODULE — Duplicate instance name
- Severity: ERROR
- Fix: Use unique instance names.

### IDENTIFIERS_AND_SCOPE.INSTANCE_NAME_CONFLICT — Instance name conflicts other identifier
- Severity: ERROR
- Fix: Rename instance so it doesn't collide with port/wire/register/CONST.

### IDENTIFIERS_AND_SCOPE.UNDECLARED_IDENTIFIER — Undeclared name used
- Severity: ERROR
- Fix: Declare the identifier or correct the name.

### IDENTIFIERS_AND_SCOPE.AMBIGUOUS_REFERENCE — Ambiguous reference
- Severity: ERROR
- Cause: Name could refer to multiple things without instance/qualification.
- Fix: Qualify with `instance.port` or rename conflicting identifiers.

---

## CONST / CONFIG / GLOBAL

### CONST_RULES.CONST_NEGATIVE_OR_NONINT — CONST invalid value
- Severity: ERROR
- Fix: Initialize CONST with nonnegative integer.

### CONFIG_BLOCK.CONFIG_MULTIPLE_BLOCKS — Multiple CONFIG blocks
- Severity: ERROR
- Fix: Merge into single CONFIG block.

### CONFIG_BLOCK.CONFIG_FORWARD_REF — CONFIG forward reference
- Severity: ERROR
- Cause: CONFIG entry references a later CONFIG entry.
- Fix: Reorder entries or use computed constants appropriately.

### CONFIG_BLOCK.CONFIG_USED_WHERE_FORBIDDEN / CONST_USED_WHERE_FORBIDDEN
- Severity: ERROR
- Cause: CONFIG/CONST used at runtime (in ASYNCHRONOUS/SYNCHRONOUS expressions).
- Fix: Use @global sized literals for runtime values; CONFIG/CONST only for compile-time contexts.

### CONFIG_BLOCK.CONST_STRING_IN_NUMERIC_CONTEXT — String CONST/CONFIG in numeric context
- Severity: ERROR
- Cause: A string-valued CONST or CONFIG entry is used where a numeric expression is expected (e.g., in a width bracket `[STR_CONST]` or MEM depth).
- Fix: Use a numeric CONST/CONFIG for integer contexts. String values are only valid in `@file()` path arguments.

### CONFIG_BLOCK.CONST_NUMERIC_IN_STRING_CONTEXT — Numeric CONST/CONFIG in string context
- Severity: ERROR
- Cause: A numeric CONST or CONFIG entry is used where a string is expected (e.g., `@file(NUM_CONST)`).
- Fix: Use a string CONST/CONFIG for `@file()` paths (e.g., `INIT_FILE = "data.bin";`).

### GLOBAL_BLOCK.GLOBAL_INVALID_EXPR_TYPE — Global constant must be sized literal
- Severity: ERROR
- Fix: Define global constants as sized literals (e.g., `8'hFF`).

---

## Ports, Wires & Registers

### PORT_WIRE_REGISTER_DECLS.PORT_MISSING_WIDTH — Port declared without width
- Severity: ERROR
- Fix: Add mandatory `[N]` width.

### PORT_WIRE_REGISTER_DECLS.PORT_DIRECTION_MISMATCH_IN — Assigning to IN port
- Severity: ERROR
- Cause: Writing to an `IN` port outside allowed INOUT tri‑state patterns.
- Fix: Only read `IN` ports; if bidirectional behavior is needed use `INOUT` and tri‑state logic.

### PORT_WIRE_REGISTER_DECLS.PORT_DIRECTION_MISMATCH_OUT — Reading from OUT port
- Severity: ERROR
- Cause: Reading an `OUT` port as if it were a driver.
- Fix: Do not read module `OUT` ports inside the same module; use internal regs/wires for observation.

### PORT_WIRE_REGISTER_DECLS.PORT_TRISTATE_MISMATCH — `z` assigned to non-INOUT
- Severity: ERROR
- Fix: Only assign `z` to INOUT ports.

### PORT_WIRE_REGISTER_DECLS.WIRE_MULTI_DIMENSIONAL / REG_MULTI_DIMENSIONAL — Multi-dimensional illegal
- Severity: ERROR
- Fix: Use MEM for arrays; declare WIRE/REGISTER as single-dimensional scalars.

### PORT_WIRE_REGISTER_DECLS.REG_MISSING_INIT_LITERAL — Register missing reset literal
- Severity: ERROR
- Fix: Provide a sized literal reset value (no `x` bits).

### PORT_WIRE_REGISTER_DECLS.REG_INIT_CONTAINS_X — Register reset contains `x`
- Severity: ERROR
- Fix: Use known 0/1 bits for reset; `x` not permitted.

### PORT_WIRE_REGISTER_DECLS.REG_INIT_CONTAINS_Z — Register reset contains `z`
- Severity: ERROR
- Fix: Use known 0/1 bits for reset; `z` not permitted.

### PORT_WIRE_REGISTER_DECLS.REG_INIT_WIDTH_MISMATCH — Register init width mismatch
- Severity: ERROR
- Fix: Init literal width must match declared register width.

### PORT_WIRE_REGISTER_DECLS.WRITE_WIRE_IN_SYNC — Writing wire in SYNCHRONOUS
- Severity: ERROR
- Fix: Writes to wires must occur in ASYNCHRONOUS; registers are updated in SYNCHRONOUS blocks.

### PORT_WIRE_REGISTER_DECLS.ASSIGN_TO_NON_REGISTER_IN_SYNC — LHS not a register in sync assignment
- Severity: ERROR
- Fix: Use a REGISTER on LHS for synchronous assignments.

### PORT_WIRE_REGISTER_DECLS.MODULE_MISSING_PORT / MODULE_PORT_IN_ONLY
- Severity: ERROR / WARN
- Fix: Ensure module declares PORT block and includes outputs if expected; a module with only IN ports may be dead code.

---

## Modules & Instantiation

### MODULE_AND_INSTANTIATION.INSTANCE_MISSING_PORT — Not all child ports listed in @new
- Severity: ERROR
- Fix: Include all child ports in the `@new` instantiation block.

### MODULE_AND_INSTANTIATION.INSTANCE_PORT_WIDTH_MISMATCH — Port width mismatch
- Severity: ERROR
- Fix: Ensure evaluated widths (after OVERRIDE) match between parent binding and child port.

### MODULE_AND_INSTANTIATION.INSTANCE_PORT_DIRECTION_MISMATCH — Direction mismatch
- Severity: ERROR
- Fix: Connect IN ports to drivers, OUT ports to sinks or parent signals with correct roles; respect pin category.

### MODULE_AND_INSTANTIATION.INSTANCE_OVERRIDE_CONST_UNDEFINED — Override targets unknown
- Severity: ERROR
- Fix: Only override CONST names that exist in the child module.

### MODULE_AND_INSTANTIATION.INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH — Parent signal width wrong
- Severity: ERROR
- Fix: Ensure parent signal has the width declared in instantiation clause (check parent's CONST resolution).

---

## MUX Rules

### MUX_RULES.MUX_ASSIGN_LHS — Assigning to MUX is forbidden
- Severity: ERROR
- Fix: Treat MUX only as read-only source; create a wire/register to hold selected data if you must drive it.

### MUX_RULES.MUX_AGG_SOURCE_WIDTH_MISMATCH — Aggregation widths differ
- Severity: ERROR
- Fix: Make all aggregated sources identical width before declaring the MUX.

### MUX_RULES.MUX_SLICE_WIDTH_NOT_DIVISOR — Auto-slicing invalid partition
- Severity: ERROR
- Fix: Choose element_width that exactly divides wide_source width.

### MUX_RULES.MUX_SELECTOR_OUT_OF_RANGE_CONST — Selector statically out of range
- Severity: ERROR
- Fix: Correct selector width or bounds; ensure compile-time indices fall within valid range.

---

## Assignments & Exclusive Assignment Rule

### ASSIGNMENTS_AND_EXCLUSIVE.ASSIGN_WIDTH_NO_MODIFIER — Width mismatch without z/s
- Severity: ERROR
- Cause: `=`, `=>`, `<=` used when widths differ (modifier required).
- Fix: Use `=z`/`=s`, `=>z`/`=>s`, or resize operands so widths match.

### ASSIGNMENTS_AND_EXCLUSIVE.ASSIGN_TRUNCATES — Assignment would truncate RHS
- Severity: ERROR
- Fix: Prevent truncation by slicing explicitly or making LHS wide enough; avoid implicit truncation.

### ASSIGNMENTS_AND_EXCLUSIVE.ASSIGN_SLICE_WIDTH_MISMATCH — Slice width mismatch
- Severity: ERROR
- Fix: Ensure source expression width equals the slice width.

### ASSIGNMENTS_AND_EXCLUSIVE.ASSIGN_CONCAT_WIDTH_MISMATCH — Concatenation width mismatch
- Severity: ERROR
- Fix: Make expression width equal sum of LHS part widths (or use extension modifiers if allowed).

### ASSIGNMENTS_AND_EXCLUSIVE.ASSIGN_MULTIPLE_SAME_BITS — Exclusive assignment violation
- Severity: ERROR
- Cause: More than one assignment to same bits along some execution path.
- Fix: Rework control flow so assignments are in mutually exclusive branches or merge into a single assignment using ternary/select.

### ASSIGNMENTS_AND_EXCLUSIVE.ASSIGN_INDEPENDENT_IF_SELECT — Independent chains conflict
- Severity: ERROR
- Cause: Separate IF chains at same nesting level each assign same identifier (compiler treats as concurrent).
- Fix: Combine into single SELECT/IF/ELIF chain or restructure to make assignments exclusive.

### ASSIGNMENTS_AND_EXCLUSIVE.ASSIGN_SHADOWING — Shadowing assignment (root + nested)
- Severity: ERROR
- Fix: Avoid assigning at root and again in nested blocks to same bits; use conditional assignments consistently.

### ASSIGNMENTS_AND_EXCLUSIVE.ASSIGN_SLICE_OVERLAP — Overlapping part-selects assigned in same path
- Severity: ERROR
- Fix: Ensure part-selects are non-overlapping on any execution path or combine into single concatenation assignment.

### ASSIGNMENTS_AND_EXCLUSIVE.ASYNC_UNDEFINED_PATH_NO_DRIVER — ASYNC leaves net without driver
- Severity: ERROR
- Cause: Conditional ASYNCHRONOUS assignment path leaves a wire/port undriven (no ELSE) and no other driver exists.
- Fix: Provide DEFAULT branch or alternate driver; ensure every execution path that reads the net has a driver.

---

## ASYNCHRONOUS Block Rules

### ASYNC_BLOCK_RULES.ASYNC_INVALID_STATEMENT_TARGET — LHS not assignable
- Severity: ERROR
- Fix: LHS must be assignable (wire, port, register current-value read in ASYNC can't be written there).

### ASYNC_BLOCK_RULES.ASYNC_ASSIGN_REGISTER — Register written in ASYNCHRONOUS
- Severity: ERROR
- Fix: Move register writes into SYNCHRONOUS blocks.

### ASYNC_BLOCK_RULES.ASYNC_ALIAS_LITERAL_RHS — Alias with literal RHS in ASYNC
- Severity: ERROR
- Cause: Using `a = 1'b1;` in ASYNCHRONOUS is forbidden (alias RHS must not be bare literal).
- Fix: Use directional drive `a <= 1'b1;` or `a => 1'b1;` instead.

### ASYNC_BLOCK_RULES.ASYNC_FLOATING_Z_READ — All drivers `z` on read
- Severity: ERROR
- Fix: Ensure at least one driver supplies 0/1 for read conditions or avoid reading when bus released.

---

## SYNCHRONOUS Block Rules

### SYNC_BLOCK_RULES.SYNC_MULTI_ASSIGN_SAME_REG_BITS — Multiple reg assignments in path
- Severity: ERROR
- Fix: Ensure each register bit assigned at most once per path; move assignments into mutually exclusive branches or combine.

### SYNC_BLOCK_RULES.SYNC_ROOT_AND_CONDITIONAL_ASSIGN — Root + conditional conflict
- Severity: ERROR
- Fix: Do not assign same register at root and inside nested condition; place all domain assignments within consistent conditional structure.

### SYNC_BLOCK_RULES.SYNC_SLICE_WIDTH_MISMATCH — Register slice width mismatch
- Severity: ERROR
- Fix: Ensure RHS width equals slice width.

### SYNC_BLOCK_RULES.SYNC_CONCAT_DUP_REG — Concatenation contains same register multiple times
- Severity: ERROR
- Fix: Use unique registers in concatenation LHS.

### SYNC_BLOCK_RULES.SYNC_NO_ALIAS — Aliasing not allowed in synchronous blocks
- Severity: ERROR
- Fix: Use directional operators (`<=`, `=>`) only; alias operators are forbidden in SYNCHRONOUS.

### SYNC_BLOCK_RULES.DOMAIN_CONFLICT / MULTI_CLK_ASSIGN / DUPLICATE_BLOCK — Clock/CDC domain errors
- Severity: ERROR
- Cause: Register read/write used in block whose `CLK` doesn't match home domain; same register assigned in different clock blocks; duplicate synchronous block for a clock.
- Fix: Respect Register locality/home-domain rules; use CDC entries to cross domains; consolidate per-clock logic into single SYNCHRONOUS block.

### SYNC_BLOCK_RULES.CDC_SOURCE_NOT_REGISTER / CDC_BIT_WIDTH_NOT_1
- Severity: ERROR
- Fix: CDC sources must be `REGISTER`; BIT type registers must have width 1.

### SYNC_BLOCK_RULES.SYNC_CLK_WIDTH_NOT_1 — Clock signal not width-1
- Severity: ERROR
- Fix: SYNCHRONOUS CLK signal must have width [1].

### SYNC_BLOCK_RULES.SYNC_MISSING_CLK — Missing CLK parameter
- Severity: ERROR
- Fix: SYNCHRONOUS block must declare a CLK parameter.

### SYNC_BLOCK_RULES.SYNC_EDGE_INVALID — Invalid edge value
- Severity: ERROR
- Fix: EDGE must be Rising, Falling, or Both.

### SYNC_BLOCK_RULES.SYNC_RESET_ACTIVE_INVALID — Invalid RESET_ACTIVE
- Severity: ERROR
- Fix: RESET_ACTIVE must be High or Low.

### SYNC_BLOCK_RULES.SYNC_RESET_TYPE_INVALID — Invalid RESET_TYPE
- Severity: ERROR
- Fix: RESET_TYPE must be Clocked or Immediate.

### SYNC_BLOCK_RULES.SYNC_RESET_WIDTH_NOT_1 — Reset signal not width-1
- Severity: ERROR
- Fix: RESET signal must have width [1].

### SYNC_BLOCK_RULES.SYNC_UNKNOWN_PARAM — Unknown SYNCHRONOUS parameter
- Severity: ERROR
- Fix: Valid parameters are CLK, RESET, EDGE, RESET_ACTIVE, RESET_TYPE.

### SYNC_BLOCK_RULES.SYNC_EDGE_BOTH_WARNING — Dual-edge clocking warning
- Severity: WARN
- Cause: EDGE=Both may not be supported by all FPGA architectures.
- Fix: Use Rising or Falling unless dual-edge is intentional.

### SYNC_BLOCK_RULES.CDC_SOURCE_NOT_PLAIN_REG / CDC_DEST_ALIAS_ASSIGNED / CDC_STAGES_INVALID / CDC_TYPE_INVALID / CDC_RAW_STAGES_FORBIDDEN / CDC_PULSE_WIDTH_NOT_1 / CDC_DEST_ALIAS_DUP
- Severity: ERROR
- Fix: CDC source must be a plain register identifier (not slice/expression). Destination alias is read-only. Stages must be positive. Type must be BIT/BUS/FIFO/HANDSHAKE/PULSE/MCP/RAW. RAW has no stages parameter. PULSE source must be width 1. Dest alias name must be unique.

---

## Control Flow (IF / SELECT)

### CONTROL_FLOW_IF_SELECT.IF_COND_WIDTH_NOT_1 — IF/ELIF condition width ≠ 1
- Severity: ERROR
- Fix: Ensure condition expression reduces to 1 bit (e.g., compare equality).

### CONTROL_FLOW_IF_SELECT.CONTROL_FLOW_OUTSIDE_BLOCK — IF/SELECT used outside block
- Severity: ERROR
- Fix: Place control flow inside ASYNCHRONOUS or SYNCHRONOUS blocks.

### CONTROL_FLOW_IF_SELECT.SELECT_DUP_CASE_VALUE — Duplicate CASE label
- Severity: ERROR
- Fix: Remove or collapse duplicate CASE labels.

### CONTROL_FLOW_IF_SELECT.ASYNC_ALIAS_IN_CONDITIONAL — Alias operator inside conditional
- Severity: ERROR
- Fix: Use conditional expressions or directional assignments; aliasing is for unconditional net merges only.

### CONTROL_FLOW_IF_SELECT.SELECT_DEFAULT_RECOMMENDED_ASYNC — Missing DEFAULT in ASYNCHRONOUS SELECT
- Severity: WARN
- Cause: May leave nets floating for some selector values.
- Fix: Add DEFAULT clause and specify safe values.

---

## Functions & Compile-time Utilities

### FUNCTIONS_AND_CLOG2.CLOG2_NONPOSITIVE_ARG — clog2 arg ≤ 0
- Severity: ERROR
- Fix: Provide positive integer argument.

### FUNCTIONS_AND_CLOG2.CLOG2_INVALID_CONTEXT — clog2 used at runtime
- Severity: ERROR
- Fix: Use clog2 only in compile-time contexts (widths, CONST initializers, OVERRIDE). For runtime indexing use other constructs.

### FUNCTIONS_AND_CLOG2.WIDTHOF_INVALID_CONTEXT — widthof used at runtime
- Severity: ERROR
- Fix: Use widthof only in compile-time contexts.

### FUNCTIONS_AND_CLOG2.FUNC_RESULT_TRUNCATED_SILENTLY — Function result truncated by assignment
- Severity: ERROR
- Fix: Check sizes of intrinsic operator results and assign to matching width or slice explicitly.

---

## Net Drivers, Tri‑State & Observability

### NET_DRIVERS_AND_TRI_STATE.NET_FLOATING_WITH_SINK — Net has sinks but zero active drivers
- Severity: ERROR
- Fix: Provide a driver on all execution paths that read the net (or avoid reading when released).

### NET_DRIVERS_AND_TRI_STATE.NET_TRI_STATE_ALL_Z_READ — All drivers `z` while read
- Severity: ERROR
- Fix: Ensure at least one driver provides known 0/1 at the moment of any read.

### NET_DRIVERS_AND_TRI_STATE.NET_DANGLING_UNUSED — Net declared with no drivers and no sinks
- Severity: WARN
- Fix: Remove unused declaration or add intended drivers/sinks.

### OBSERVABILITY_X.OBS_X_TO_OBSERVABLE_SINK — x bits reach observable sink
- Severity: ERROR
- Cause: Expression containing `x` used where its `x` bits could reach REGISTER init, MEM init, OUT/INOUT port, or top-level pin.
- Fix: Mask structurally (not algebraically) before observable sinks (e.g., use conditional logic to drive defined values), or avoid `x` in those values.

---

## Combinational Loops

### COMBINATIONAL_LOOPS.COMB_LOOP_UNCONDITIONAL — Flow-sensitive comb loop (error)
- Severity: ERROR
- Cause: Unconditional cycle in ASYNCHRONOUS assignments (e.g., a = b; b = a;).
- Fix: Break cycle by introducing registers, restructuring logic, or making assignments mutually exclusive.

### COMBINATIONAL_LOOPS.COMB_LOOP_CONDITIONAL_SAFE — Cycle within mutually exclusive branches
- Severity: WARN (informational, considered safe)
- Note: Flow-sensitive analysis treats mutually exclusive branches as safe; verify intended behavior.

---

## Project, Imports & Top-level

### PROJECT_AND_IMPORTS.PROJECT_MULTIPLE_PER_FILE — Multiple @project
- Severity: ERROR
- Fix: Use a single @project per file.

### PROJECT_AND_IMPORTS.IMPORT_FILE_MULTIPLE_TIMES — Same import repeated
- Severity: ERROR
- Fix: Remove duplicate imports or normalize import graph to avoid re-importing same file.

### PROJECT_AND_IMPORTS.IMPORT_NOT_AT_PROJECT_TOP — @import in wrong place
- Severity: ERROR
- Fix: Move `@import` to immediately after `@project` header, before CONFIG/CLOCKS/PIN/blackbox/top.

### TOP_LEVEL_INSTANTIATION.TOP_PORT_NOT_LISTED — Top-file misses a port
- Severity: ERROR
- Fix: List all module ports in `@top` mapping; specify `_` if intentionally unconnected.

### CLOCKS_PINS_MAP.* — Various clock/pin/map errors
- Severity: ERROR/WARN depending on item (CLOCK_PORT_WIDTH_NOT_1, MAP_PIN_DECLARED_NOT_MAPPED, MAP_DUP_PHYSICAL_LOCATION, etc.)
- Fix: Ensure CLOCKS entries match IN_PINS (width 1), map every declared pin in MAP, avoid mapping two logical pins to same physical pin unless tri-state intended.

### CLOCKS_PINS_MAP.PIN_MODE_INVALID — Invalid pin mode value
- Severity: ERROR
- Cause: `mode` attribute is not `SINGLE` or `DIFFERENTIAL`.
- Fix: Use `mode=SINGLE` or `mode=DIFFERENTIAL`.

### CLOCKS_PINS_MAP.PIN_MODE_STANDARD_MISMATCH — Pin mode conflicts with I/O standard
- Severity: ERROR
- Cause: `mode=DIFFERENTIAL` used with a single-ended standard (e.g., LVCMOS33), or `mode=SINGLE` used with a differential standard (e.g., LVDS25).
- Fix: Use a standard that matches the pin mode. Single-ended standards (LVCMOS, LVTTL, PCI33, SSTL, HSTL) require `mode=SINGLE`; differential standards (LVDS, TMDS, BLVDS, DIFF_SSTL, etc.) require `mode=DIFFERENTIAL`.

### CLOCKS_PINS_MAP.PIN_PULL_INVALID — Invalid pull value
- Severity: ERROR
- Cause: `pull` attribute is not `UP`, `DOWN`, or `NONE`.
- Fix: Use `pull=UP`, `pull=DOWN`, or `pull=NONE`.

### CLOCKS_PINS_MAP.PIN_PULL_ON_OUTPUT — Pull resistor on output-only pin
- Severity: ERROR
- Cause: `pull=UP` or `pull=DOWN` specified on an `OUT_PINS` entry.
- Fix: Remove pull attribute from output pins. Pull resistors are only meaningful on input or bidirectional pins.

### CLOCKS_PINS_MAP.PIN_TERM_INVALID — Invalid termination value
- Severity: ERROR
- Cause: `term` attribute is not `ON` or `OFF`.
- Fix: Use `term=ON` or `term=OFF`.

### CLOCKS_PINS_MAP.PIN_TERM_INVALID_FOR_STANDARD — Termination not supported for standard
- Severity: ERROR
- Cause: `term=ON` used with a standard that does not support on-die termination (e.g., LVCMOS33, LVTTL, PCI33).
- Fix: Termination is only valid for `mode=DIFFERENTIAL` pins or for single-ended SSTL/HSTL standards. Remove `term=ON` or change to a termination-capable standard.

### CLOCKS_PINS_MAP.MAP_DIFF_EXPECTED_PAIR — Differential pin needs P, N MAP syntax
- Severity: ERROR
- Cause: A pin declared with `mode=DIFFERENTIAL` is mapped with a scalar value instead of the `{ P=pin, N=pin }` pair syntax.
- Fix: Use differential MAP syntax: `pin = { P=33, N=34 };`

### CLOCKS_PINS_MAP.MAP_SINGLE_UNEXPECTED_PAIR — Single-ended pin must not use P, N pair
- Severity: ERROR
- Cause: A single-ended pin is mapped with `{ P=pin, N=pin }` pair syntax instead of a scalar value.
- Fix: Use scalar MAP syntax: `pin = 33;`

### CLOCKS_PINS_MAP.MAP_DIFF_MISSING_PN — Differential MAP missing P or N
- Severity: ERROR
- Cause: Differential MAP entry is missing the `P` or `N` identifier.
- Fix: Provide both P and N: `pin = { P=33, N=34 };`

### CLOCKS_PINS_MAP.MAP_DIFF_SAME_PIN — P and N map to same physical pin
- Severity: ERROR
- Cause: Both P and N in a differential MAP entry refer to the same physical pin.
- Fix: Use different physical pins for P (positive) and N (negative).

---

## Blackboxes & Top-level

### BLACKBOX_RULES.BLACKBOX_BODY_DISALLOWED — Blackbox contains body
- Severity: ERROR
- Fix: Blackboxes must declare only PORT (and optional CONST) — no wires/registers/blocks.

### BLACKBOX_RULES.BLACKBOX_UNDEFINED_IN_NEW — Unknown blackbox instantiation
- Severity: ERROR
- Fix: Declare the blackbox in the project before instantiation.

### BLACKBOX_RULES.BLACKBOX_OVERRIDE_UNCHECKED — OVERRIDE unvalidated
- Severity: INFO
- Note: OVERRIDE values are passed through to vendor IP; tool cannot validate them.

---

## MEM (Memory) — Declaration & Access

### MEM_DECLARATION.MEM_UNDEFINED_NAME — Memory not declared
- Severity: ERROR
- Fix: Declare MEM block before using `mem.port[addr]` access.

### MEM_DECLARATION.MEM_INVALID_WORD_WIDTH / MEM_INVALID_DEPTH — Word width or depth invalid
- Severity: ERROR
- Fix: Use positive integer or valid CONST resolving to positive integer.

### MEM_DECLARATION.MEM_INIT_CONTAINS_X / MEM_INIT_LITERAL_OVERFLOW / MEM_INIT_FILE_TOO_LARGE — Init problems
- Severity: ERROR
- Fix: Ensure MEM init literal contains only 0/1 (no `x`), fits word width; ensure init file length ≤ depth × word_width.

### MEM_DECLARATION.MEM_EMPTY_PORT_LIST — MEM without ports
- Severity: ERROR
- Fix: Declare at least one OUT or IN port.

### MEM_ACCESS.MEM_PORT_UNDEFINED — Port name not declared in MEM
- Severity: ERROR
- Fix: Use declared MEM port names.

### MEM_ACCESS.MEM_READ_SYNC_WITH_EQUALS — Sync read used with `=` instead of `<=`
- Severity: ERROR
- Fix: Use `<=` in SYNCHRONOUS block for synchronous reads.

### MEM_ACCESS.MEM_WRITE_IN_ASYNC_BLOCK — Write used in ASYNCHRONOUS
- Severity: ERROR
- Fix: Perform writes in SYNCHRONOUS blocks only.

### MEM_ACCESS.MEM_ADDR_WIDTH_TOO_WIDE / MEM_CONST_ADDR_OUT_OF_RANGE — Address width / const address out-of-range
- Severity: ERROR
- Fix: Use address width ≤ ceil(log2(depth)); ensure constant addresses < depth.

### MEM_ACCESS.MEM_MULTIPLE_WRITES_SAME_IN — Multiple writes to same IN port in one block
- Severity: ERROR
- Fix: Ensure only one write to each IN port per SYNCHRONOUS block per execution path (use conditional exclusivity).

### MEM_ACCESS.MEM_INVALID_WRITE_MODE — Unknown write mode
- Severity: ERROR
- Fix: Use WRITE_FIRST, READ_FIRST, or NO_CHANGE.

### MEM_ACCESS.MEM_INOUT_INDEXED — INOUT port indexed
- Severity: ERROR
- Fix: INOUT ports may not use `mem.port[addr]` syntax; use `.addr`/`.data`/`.wdata` pseudo-fields.

### MEM_ACCESS.MEM_INOUT_WDATA_IN_ASYNC — INOUT .wdata in ASYNCHRONOUS
- Severity: ERROR
- Fix: INOUT port `.wdata` may only be assigned in SYNCHRONOUS blocks.

### MEM_ACCESS.MEM_INOUT_ADDR_IN_ASYNC — INOUT .addr in ASYNCHRONOUS
- Severity: ERROR
- Fix: INOUT port `.addr` may only be assigned in SYNCHRONOUS blocks.

### MEM_ACCESS.MEM_INOUT_WDATA_WRONG_OP — INOUT .wdata wrong operator
- Severity: ERROR
- Fix: INOUT port `.wdata` must be assigned with `<=` operator.

### MEM_ACCESS.MEM_MULTIPLE_ADDR_ASSIGNS — Multiple INOUT .addr assignments
- Severity: ERROR
- Fix: Assign `.addr` at most once per execution path.

### MEM_ACCESS.MEM_MULTIPLE_WDATA_ASSIGNS — Multiple INOUT .wdata assignments
- Severity: ERROR
- Fix: Assign `.wdata` at most once per execution path.

### MEM_DECLARATION.MEM_INOUT_MIXED_WITH_IN_OUT — INOUT mixed with IN/OUT
- Severity: ERROR
- Fix: INOUT ports cannot be mixed with IN or OUT ports in the same MEM declaration.

### MEM_DECLARATION.MEM_INOUT_ASYNC — INOUT with ASYNC keyword
- Severity: ERROR
- Fix: INOUT ports are always synchronous; do not specify ASYNC/SYNC keyword.

### MEM_WARNINGS.MEM_WARN_PORT_NEVER_ACCESSED — MEM port unused
- Severity: WARN
- Fix: Remove unused port or add intended accesses.

### MEM_WARNINGS.MEM_WARN_PARTIAL_INIT — Init file smaller than depth
- Severity: WARN
- Fix: Provide full init file or accept zero-padding.

---

## General Warnings & Info

### GENERAL_WARNINGS.WARN_UNUSED_REGISTER — Unused register
- Severity: WARN
- Fix: Remove register if not needed or use it in logic.

### GENERAL_WARNINGS.WARN_UNCONNECTED_OUTPUT — Unconnected module output
- Severity: WARN
- Fix: Drive the output, connect to top-level pin, or intentionally bind to `_` and document intent.

### GENERAL_WARNINGS.WARN_INCOMPLETE_SELECT_ASYNC — ASYNC SELECT missing DEFAULT
- Severity: WARN
- Fix: Add DEFAULT to prevent floating nets.

### GENERAL_WARNINGS.WARN_DEAD_CODE_UNREACHABLE — Unreachable statements
- Severity: WARN
- Fix: Remove or correct unreachable logic conditions.

### GENERAL_WARNINGS.WARN_UNUSED_MODULE — Module never instantiated
- Severity: WARN
- Fix: Instantiate module or remove it.

### GENERAL_WARNINGS.WARN_UNSINKED_REGISTER — Register written but never read
- Severity: WARN
- Fix: Read the register value or remove it if not needed.

### GENERAL_WARNINGS.WARN_UNDRIVEN_REGISTER — Register read but never written
- Severity: WARN
- Fix: Assign the register in a SYNCHRONOUS block.

### GENERAL_WARNINGS.WARN_UNUSED_WIRE — Wire never driven or read
- Severity: WARN
- Fix: Remove unused wire declaration.

### GENERAL_WARNINGS.WARN_UNUSED_PORT — Port never used
- Severity: WARN
- Fix: Remove unused port or connect it.

### GENERAL_WARNINGS.WARN_INTERNAL_TRISTATE — Internal tri-state not FPGA-compatible
- Severity: WARN
- Cause: Internal tri-state logic is not supported by most FPGAs.
- Fix: Use `--tristate-default` to enable automatic tri-state elimination.

---

## Latch Rules

### LATCH_RULES.LATCH_ASSIGN_NON_GUARDED — Latch assignment not guarded
- Severity: ERROR
- Cause: LATCH must be written via guarded assignment `name <= enable : data;` in ASYNCHRONOUS blocks.
- Fix: Use guarded syntax for D latches or `name <= set : reset;` for SR latches.

### LATCH_RULES.LATCH_ASSIGN_IN_SYNC — Latch written in SYNCHRONOUS
- Severity: ERROR
- Fix: Move latch assignments to ASYNCHRONOUS blocks; use REGISTER for edge-triggered storage.

### LATCH_RULES.LATCH_ENABLE_WIDTH_NOT_1 — D-latch enable not width-1
- Severity: ERROR
- Fix: Enable expression must have width [1].

### LATCH_RULES.LATCH_ALIAS_FORBIDDEN — Latch aliased
- Severity: ERROR
- Fix: Latches may not be aliased using `=`; they must not be merged into other nets.

### LATCH_RULES.LATCH_INVALID_TYPE — Invalid latch type
- Severity: ERROR
- Fix: LATCH type must be `D` or `SR`.

### LATCH_RULES.LATCH_WIDTH_INVALID — Invalid latch width
- Severity: ERROR
- Fix: Width must be a positive integer.

### LATCH_RULES.LATCH_SR_WIDTH_MISMATCH — SR latch set/reset width mismatch
- Severity: ERROR
- Fix: Set and reset expression widths must match the latch width.

### LATCH_RULES.LATCH_AS_CLOCK_OR_CDC — Latch used as clock or CDC source
- Severity: ERROR
- Fix: Latches may not be used as clock signals or in CDC declarations.

### LATCH_RULES.LATCH_IN_CONST_CONTEXT — Latch in compile-time context
- Severity: ERROR
- Fix: Latch identifiers may not appear in `@check`/`@feature` conditions.

### LATCH_RULES.LATCH_CHIP_UNSUPPORTED — Latch not supported by chip
- Severity: ERROR
- Fix: Selected chip does not support the latch type.

---

## Template Rules

### TEMPLATE.TEMPLATE_UNDEFINED — Undefined template
- Severity: ERROR
- Fix: Ensure the template is defined before `@apply`.

### TEMPLATE.TEMPLATE_ARG_COUNT_MISMATCH — Argument count mismatch
- Severity: ERROR
- Fix: `@apply` argument count must match template parameter count.

### TEMPLATE.TEMPLATE_COUNT_NOT_NONNEG_INT — Invalid @apply count
- Severity: ERROR
- Fix: Count must resolve to a non-negative integer.

### TEMPLATE.TEMPLATE_NESTED_DEF — Nested template definition
- Severity: ERROR
- Fix: `@template` definitions may not be nested.

### TEMPLATE.TEMPLATE_FORBIDDEN_DECL — Declaration inside template
- Severity: ERROR
- Fix: Use `@scratch` for temporary signals; WIRE/REGISTER/PORT/CONST/MEM/MUX are forbidden.

### TEMPLATE.TEMPLATE_FORBIDDEN_BLOCK_HEADER — Block header inside template
- Severity: ERROR
- Fix: SYNCHRONOUS/ASYNCHRONOUS block headers are not allowed in template body.

### TEMPLATE.TEMPLATE_FORBIDDEN_DIRECTIVE — Directive inside template
- Severity: ERROR
- Fix: @new/@module/@feature and other structural directives are forbidden.

### TEMPLATE.TEMPLATE_SCRATCH_OUTSIDE — @scratch outside template
- Severity: ERROR
- Fix: `@scratch` may only appear inside a `@template` body.

### TEMPLATE.TEMPLATE_APPLY_OUTSIDE_BLOCK — @apply outside block
- Severity: ERROR
- Fix: `@apply` may only appear inside ASYNCHRONOUS or SYNCHRONOUS blocks.

### TEMPLATE.TEMPLATE_DUP_NAME — Duplicate template name
- Severity: ERROR
- Fix: Template names must be unique within the same scope.

### TEMPLATE.TEMPLATE_DUP_PARAM — Duplicate parameter name
- Severity: ERROR
- Fix: Parameter names must be unique within the template definition.

### TEMPLATE.TEMPLATE_SCRATCH_WIDTH_INVALID — Invalid @scratch width
- Severity: ERROR
- Fix: Width must be a positive integer constant expression.

### TEMPLATE.TEMPLATE_EXTERNAL_REF — External reference in template
- Severity: ERROR
- Fix: All identifiers must be parameters, @scratch wires, or compile-time constants. Pass external signals as arguments.

---

## Feature Guard Rules

### FEATURE_GUARDS.FEATURE_COND_WIDTH_NOT_1 — Feature condition not width-1
- Severity: ERROR
- Fix: Feature guard condition must evaluate to a 1-bit value.

### FEATURE_GUARDS.FEATURE_EXPR_INVALID_CONTEXT — Invalid feature condition
- Severity: ERROR
- Fix: Feature guard condition may only reference CONFIG, module CONST, literals, and logical/comparison operators.

### FEATURE_GUARDS.FEATURE_NESTED — Nested @feature
- Severity: ERROR
- Fix: @feature guards may not be nested inside other @feature guards.

### FEATURE_GUARDS.FEATURE_VALIDATION_BOTH_PATHS — Both paths must validate
- Severity: ERROR
- Fix: Both the enabled and disabled branches must pass full semantic validation.

---

## @check Rules

### CHECK_RULES.CHECK_FAILED — Compile-time assertion failed
- Severity: ERROR
- Cause: `@check` expression evaluated to zero.
- Fix: Correct the assertion condition or the values it depends on.

### CHECK_RULES.CHECK_INVALID_EXPR_TYPE — Invalid @check expression
- Severity: ERROR
- Fix: Expression must be a nonnegative integer constant over literals, CONST, and CONFIG.

### CHECK_RULES.CHECK_INVALID_PLACEMENT — @check in invalid location
- Severity: ERROR
- Fix: @check may not appear inside conditional or @feature bodies.

---

## BUS Rules

### BUS_RULES.BUS_DEF_DUP_NAME — Duplicate BUS definition
- Severity: ERROR
- Fix: BUS definition names must be unique within the project.

### BUS_RULES.BUS_DEF_SIGNAL_DUP_NAME — Duplicate BUS signal name
- Severity: ERROR
- Fix: Signal names within a BUS definition must be unique.

### BUS_RULES.BUS_DEF_INVALID_DIR — Invalid BUS signal direction
- Severity: ERROR
- Fix: Direction must be IN, OUT, or INOUT.

### BUS_RULES.BUS_PORT_UNKNOWN_BUS — Unknown BUS reference
- Severity: ERROR
- Fix: BUS port references a BUS name not declared in the project.

### BUS_RULES.BUS_PORT_INVALID_ROLE — Invalid BUS role
- Severity: ERROR
- Fix: Role must be SOURCE or TARGET.

### BUS_RULES.BUS_PORT_ARRAY_COUNT_INVALID — Invalid BUS array count
- Severity: ERROR
- Fix: Array count must be a positive integer constant expression.

### BUS_RULES.BUS_PORT_INDEX_REQUIRED — BUS index required
- Severity: ERROR
- Fix: Arrayed BUS access requires an explicit index or wildcard `[*]`.

### BUS_RULES.BUS_PORT_INDEX_NOT_ARRAY — Indexed access on non-array BUS
- Severity: ERROR
- Fix: Only arrayed BUS ports support indexed access.

### BUS_RULES.BUS_PORT_INDEX_OUT_OF_RANGE — BUS index out of range
- Severity: ERROR
- Fix: Index must be within [0, count-1].

### BUS_RULES.BUS_PORT_NOT_BUS — Member access on non-BUS port
- Severity: ERROR
- Fix: Dot notation is only valid on BUS ports.

### BUS_RULES.BUS_SIGNAL_UNDEFINED — Undefined BUS signal
- Severity: ERROR
- Fix: Signal does not exist in the BUS definition.

### BUS_RULES.BUS_SIGNAL_READ_FROM_WRITABLE — Read from writable BUS signal
- Severity: ERROR
- Fix: This signal is write-only from the current role's perspective.

### BUS_RULES.BUS_SIGNAL_WRITE_TO_READABLE — Write to readable BUS signal
- Severity: ERROR
- Fix: This signal is read-only from the current role's perspective.

### BUS_RULES.BUS_WILDCARD_WIDTH_MISMATCH — Wildcard width mismatch
- Severity: ERROR
- Fix: RHS width must be 1 (broadcast) or equal to the array count (element-wise).

### BUS_RULES.BUS_TRISTATE_MISMATCH — Tri-state on non-writable BUS signal
- Severity: ERROR
- Fix: Only writable BUS signals (INOUT or OUT from this role) may be assigned `z`.

### BUS_RULES.BUS_BULK_BUS_MISMATCH — Bulk BUS assignment bus mismatch
- Severity: ERROR
- Fix: Both sides of a bulk BUS assignment must reference the same BUS definition.

### BUS_RULES.BUS_BULK_ROLE_CONFLICT — Bulk BUS assignment role conflict
- Severity: ERROR
- Fix: Cannot assign between instances with the same role (SOURCE-SOURCE or TARGET-TARGET).

---

## CLOCK_GEN Rules

### CLOCK_GEN_RULES.CLOCK_GEN_INPUT_NOT_DECLARED — Input clock not in CLOCKS
- Severity: ERROR
- Fix: CLOCK_GEN input must reference a clock declared in the CLOCKS block.

### CLOCK_GEN_RULES.CLOCK_GEN_INPUT_NO_PERIOD — Input clock missing period
- Severity: ERROR
- Fix: CLOCK_GEN input clock must have a period declared in CLOCKS.

### CLOCK_GEN_RULES.CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE — Input frequency out of range
- Severity: ERROR
- Fix: Input clock frequency is outside the chip's supported range for this generator type.

### CLOCK_GEN_RULES.CLOCK_GEN_OUTPUT_NOT_DECLARED — Output clock not in CLOCKS
- Severity: ERROR
- Fix: CLOCK_GEN output must reference a clock declared in the CLOCKS block.

### CLOCK_GEN_RULES.CLOCK_GEN_OUTPUT_HAS_PERIOD — Output clock has period
- Severity: ERROR
- Fix: CLOCK_GEN output clocks must not have a period in CLOCKS (derived automatically).

### CLOCK_GEN_RULES.CLOCK_GEN_OUTPUT_IS_INPUT_PIN — Output clock is input pin
- Severity: ERROR
- Fix: CLOCK_GEN outputs must not be declared as IN_PINS.

### CLOCK_GEN_RULES.CLOCK_GEN_MULTIPLE_DRIVERS — Clock multiply driven
- Severity: ERROR
- Fix: A clock may only be driven by one CLOCK_GEN output.

### CLOCK_GEN_RULES.CLOCK_GEN_INPUT_IS_SELF_OUTPUT — Self-referencing clock
- Severity: ERROR
- Fix: CLOCK_GEN input must not be an output of the same block.

### CLOCK_GEN_RULES.CLOCK_GEN_INVALID_TYPE — Invalid generator type
- Severity: ERROR
- Fix: Generator type must be PLL, DLL, CLKDIV, or OSC.

### CLOCK_GEN_RULES.CLOCK_GEN_MISSING_INPUT — Missing IN clock
- Severity: ERROR
- Fix: Generator must declare an IN clock (except OSC which has no external input).

### CLOCK_GEN_RULES.CLOCK_GEN_MISSING_OUTPUT — Missing OUT clock
- Severity: ERROR
- Fix: Generator must declare at least one OUT clock.

### CLOCK_GEN_RULES.CLOCK_GEN_PARAM_OUT_OF_RANGE — CONFIG parameter out of range
- Severity: ERROR
- Fix: Parameter value is outside the chip's valid range.

### CLOCK_GEN_RULES.CLOCK_GEN_DERIVED_OUT_OF_RANGE — Derived frequency out of range
- Severity: ERROR
- Fix: Computed VCO or output frequency is outside the chip's valid range. Adjust CONFIG parameters.

---

## Path Security

### PATH_SECURITY.PATH_ABSOLUTE_FORBIDDEN — Absolute path used
- Severity: ERROR
- Cause: An `@import` or `@file()` path begins with `/` (or drive letter on Windows).
- Fix: Use relative paths or pass `--allow-absolute-paths`.

### PATH_SECURITY.PATH_TRAVERSAL_FORBIDDEN — Directory traversal
- Severity: ERROR
- Cause: Path contains `..` component.
- Fix: Remove `..` from path or pass `--allow-traversal`.

### PATH_SECURITY.PATH_OUTSIDE_SANDBOX — Path outside sandbox
- Severity: ERROR
- Cause: Resolved canonical path does not start with any permitted sandbox root.
- Fix: Use `--sandbox-root=<dir>` to add additional permitted directories.

### PATH_SECURITY.PATH_SYMLINK_ESCAPE — Symlink escapes sandbox
- Severity: ERROR
- Cause: A symbolic link inside the sandbox resolves to a target outside the sandbox.
- Fix: Remove the symlink or add the target directory to `--sandbox-root`.

---

## lit() Intrinsic Rules

### FUNCTIONS_AND_LIT.LIT_WIDTH_INVALID — lit() width invalid
- Severity: ERROR
- Fix: Width must be a positive integer constant expression.

### FUNCTIONS_AND_LIT.LIT_VALUE_INVALID — lit() value invalid
- Severity: ERROR
- Fix: Value must be a nonnegative integer constant expression.

### FUNCTIONS_AND_LIT.LIT_VALUE_OVERFLOW — lit() value overflows width
- Severity: ERROR
- Fix: Value exceeds the declared width.

### FUNCTIONS_AND_LIT.LIT_INVALID_CONTEXT — lit() in compile-time context
- Severity: ERROR
- Fix: lit() produces a runtime literal; it is not valid where a compile-time integer constant is required.

---

## Testbench Rules

### TESTBENCH.TB_MODULE_NOT_FOUND — Module not found
- Severity: ERROR
- Fix: @testbench module name must refer to a module in scope.

### TESTBENCH.TB_PORT_NOT_CONNECTED — Port not connected
- Severity: ERROR
- Fix: All module ports must be connected in @new.

### TESTBENCH.TB_PORT_WIDTH_MISMATCH — Port width mismatch
- Severity: ERROR
- Fix: Port width must match module declared width.

### TESTBENCH.TB_NEW_RHS_INVALID — Invalid @new RHS
- Severity: ERROR
- Fix: @new RHS must be a testbench CLOCK or WIRE.

### TESTBENCH.TB_SETUP_POSITION — @setup position invalid
- Severity: ERROR
- Fix: @setup must appear exactly once per TEST, after @new, before other directives.

### TESTBENCH.TB_CLOCK_NOT_DECLARED — Clock not declared
- Severity: ERROR
- Fix: @clock clock identifier must refer to a declared CLOCK.

### TESTBENCH.TB_CLOCK_CYCLE_NOT_POSITIVE — Non-positive cycle count
- Severity: ERROR
- Fix: @clock cycle count must be a positive integer.

### TESTBENCH.TB_UPDATE_NOT_WIRE — @update targets non-wire
- Severity: ERROR
- Fix: @update may only assign testbench WIRE identifiers.

### TESTBENCH.TB_UPDATE_CLOCK_ASSIGN — @update assigns clock
- Severity: ERROR
- Fix: @update may not assign clock signals.

### TESTBENCH.TB_EXPECT_WIDTH_MISMATCH — @expect width mismatch
- Severity: ERROR
- Fix: @expect value width must match signal width.

### TESTBENCH.TB_NO_TEST_BLOCKS — No TEST blocks
- Severity: ERROR
- Fix: @testbench must contain at least one TEST block.

### TESTBENCH.TB_MULTIPLE_NEW — Multiple @new
- Severity: ERROR
- Fix: Each TEST must contain exactly one @new instantiation.

### TESTBENCH.TB_PROJECT_MIXED — Mixed @project and @testbench
- Severity: ERROR
- Fix: A file may not contain both @project and @testbench.

---

## Simulation Rules

### SIMULATION.SIM_PROJECT_MIXED — Mixed @project and @simulation
- Severity: ERROR
- Fix: A file may not contain both @project and @simulation.
