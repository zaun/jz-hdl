#include <string.h>

#include "rules.h"

/*
 * NOTE: Modes are mapped as follows from rules.ini:
 *   ERROR  -> JZ_RULE_MODE_ERR
 *   WARN   -> JZ_RULE_MODE_WRN
 *   IGNORE -> JZ_RULE_MODE_WRN (still present in the table but may be
 *              suppressed by higher-level policy).
 */

/* ASSIGN_CONCAT_WIDTH_MISMATCH > ASSIGN_WIDTH_NO_MODIFIER > WIDTH_ASSIGN_MISMATCH_NO_EXT */
/* WARN_UNCONNECTED_OUTPUT > WARN_UNUSED_REGISTER */
/* CONFIG_CIRCULAR_DEP > CONFIG_FORWARD_REF */

const JZRuleInfo jz_rule_table[] = {
    /* [PARSE] (done) */
    { "PARSE", "COMMENT_IN_TOKEN",                                  0, JZ_RULE_MODE_ERR, "S1.4 Comment appears inside a token (identifier/number/operator/literal)" },
    { "PARSE", "COMMENT_NESTED_BLOCK",                              0, JZ_RULE_MODE_ERR, "S1.4 Nested block comment `/* ... /* ... */ ... */` detected" },
    { "PARSE", "DIRECTIVE_INVALID_CONTEXT",                         0, JZ_RULE_MODE_ERR, "S1.1/S6.2 Structural directives (@project/@module/@endproj/@endmod/@blackbox/@new/@import) used in invalid location" },
    { "PARSE", "KEYWORD_AS_IDENTIFIER",                             0, JZ_RULE_MODE_ERR, "S1.1 Reserved keyword used as identifier" },
    { "PARSE", "IF_COND_MISSING_PARENS",                            0, JZ_RULE_MODE_ERR, "S5.3 IF/ELIF condition missing required parentheses" },
    { "PARSE", "INSTANCE_UNDEFINED_MODULE",                         0, JZ_RULE_MODE_ERR, "S4.13/S6.9 Instantiation references non-existent module" },
    { "PARSE", "LIT_DECIMAL_HAS_XZ",                                0, JZ_RULE_MODE_ERR, "S2.1 Decimal literal with `x` or `z` digits (e.g. `8'd10x`)" },
    { "PARSE", "LIT_INVALID_DIGIT_FOR_BASE",                        0, JZ_RULE_MODE_ERR, "S2.1 Literal contains digit not allowed for its base (b/d/h)" },

    /* [LEXICAL] (done) */
    { "LEXICAL", "ID_SYNTAX_INVALID",                               0, JZ_RULE_MODE_ERR, "S1.1 Identifier does not match identifier regex or exceeds 255 chars" },
    { "LEXICAL", "ID_SINGLE_UNDERSCORE",                            0, JZ_RULE_MODE_ERR, "S1.1 Single underscore `_` used as regular identifier outside no-connect context" },

    /* [LITERALS_AND_TYPES] */
    { "LITERALS_AND_TYPES", "LIT_UNSIZED",                          0, JZ_RULE_MODE_ERR, "S2.1 Unsized literal (e.g. 'hFF) is not permitted" },
    { "LITERALS_AND_TYPES", "LIT_UNDERSCORE_AT_EDGES",              0, JZ_RULE_MODE_ERR, "S2.1 Literal has underscore as first or last character of value" },
    { "LITERALS_AND_TYPES", "LIT_UNDEFINED_CONST_WIDTH",            0, JZ_RULE_MODE_ERR, "S2.1 Width uses undefined CONST name" },
    { "LITERALS_AND_TYPES", "LIT_WIDTH_NOT_POSITIVE",               1, JZ_RULE_MODE_ERR, "S2.1 Literal width is non-positive or non-integer" },
    { "LITERALS_AND_TYPES", "LIT_OVERFLOW",                         0, JZ_RULE_MODE_ERR, "S2.1/S8.1 Literal numeric value exceeds declared width" },
    { "LITERALS_AND_TYPES", "TYPE_BINOP_WIDTH_MISMATCH",            0, JZ_RULE_MODE_ERR, "S2.2/3.2/8.1 Binary operator requires equal operand widths but receives mismatched widths" },

    /* [WIDTHS_AND_SLICING] */
    { "WIDTHS_AND_SLICING", "WIDTH_NONPOSITIVE_OR_NONINT",          1, JZ_RULE_MODE_ERR, "S2.2/S8.1 Declared width <= 0 or not an integer" },
    { "WIDTHS_AND_SLICING", "WIDTH_ASSIGN_MISMATCH_NO_EXT",         0, JZ_RULE_MODE_ERR, "S4.10/S5.0/S5.1 Width mismatch in assignment; add `=z` (zero-extend), `=s` (sign-extend), or use a slice" },
    { "WIDTHS_AND_SLICING", "SLICE_MSB_LESS_THAN_LSB",              0, JZ_RULE_MODE_ERR, "S1.3/S8.1 Slice uses MSB < LSB" },
    { "WIDTHS_AND_SLICING", "SLICE_INDEX_OUT_OF_RANGE",             0, JZ_RULE_MODE_ERR, "S1.3/S8.1 Slice indices are < 0 or >= signal width" },
    { "WIDTHS_AND_SLICING", "SLICE_INDEX_INVALID",                  0, JZ_RULE_MODE_ERR, "S1.3/S8.1 Slice index is not an integer/CONST or CONST is undefined/negative" },

    /* [OPERATORS_AND_EXPRESSIONS] */
    { "OPERATORS_AND_EXPRESSIONS", "UNARY_ARITH_MISSING_PARENS",    0, JZ_RULE_MODE_ERR, "S3.2/S8.1 Unary arithmetic `-flag`/`+flag` used without required parentheses" },
    { "OPERATORS_AND_EXPRESSIONS", "LOGICAL_WIDTH_NOT_1",           0, JZ_RULE_MODE_ERR, "S3.2/S8.1 Logical `&&`, `||`, `!` require 1-bit operands; did you mean bitwise `&`, `|`, `~`?" },
    { "OPERATORS_AND_EXPRESSIONS", "TERNARY_COND_WIDTH_NOT_1",      0, JZ_RULE_MODE_ERR, "S3.2/S8.1 Ternary `?:` condition must be 1 bit wide; use a comparison or reduction operator" },
    { "OPERATORS_AND_EXPRESSIONS", "TERNARY_BRANCH_WIDTH_MISMATCH", 0, JZ_RULE_MODE_ERR, "S3.2/S8.1 Ternary true/false branches have mismatched widths" },
    { "OPERATORS_AND_EXPRESSIONS", "CONCAT_EMPTY",                  0, JZ_RULE_MODE_ERR, "S3.2/S8.1 Empty concatenation `{}` is not allowed" },
    { "OPERATORS_AND_EXPRESSIONS", "DIV_CONST_ZERO",                0, JZ_RULE_MODE_ERR, "S3.2 Division/modulus by compile-time constant zero divisor" },
    { "OPERATORS_AND_EXPRESSIONS", "DIV_UNGUARDED_RUNTIME_ZERO",    0, JZ_RULE_MODE_WRN, "S3.2 Divisor may be zero at runtime; guard with IF (divisor != 0) or use a compile-time constant" },
    { "OPERATORS_AND_EXPRESSIONS", "SPECIAL_DRIVER_IN_EXPRESSION", 0, JZ_RULE_MODE_ERR, "S2.3 GND/VCC may not appear in arithmetic/logical expressions" },
    { "OPERATORS_AND_EXPRESSIONS", "SPECIAL_DRIVER_IN_CONCAT",     0, JZ_RULE_MODE_ERR, "S2.3 GND/VCC may not appear in concatenations" },
    { "OPERATORS_AND_EXPRESSIONS", "SPECIAL_DRIVER_SLICED",        0, JZ_RULE_MODE_ERR, "S2.3 GND/VCC may not be sliced or indexed" },
    { "OPERATORS_AND_EXPRESSIONS", "SPECIAL_DRIVER_IN_INDEX",      0, JZ_RULE_MODE_ERR, "S2.3 GND/VCC may not appear in slice/index expressions" },
    { "OPERATORS_AND_EXPRESSIONS", "SBIT_SET_WIDTH_NOT_1",        0, JZ_RULE_MODE_ERR, "S5.5.7 sbit() third argument (set) must be a width-1 expression" },
    { "OPERATORS_AND_EXPRESSIONS", "GBIT_INDEX_OUT_OF_RANGE",     0, JZ_RULE_MODE_ERR, "S5.5.6 gbit() index is out of range for source width" },
    { "OPERATORS_AND_EXPRESSIONS", "SBIT_INDEX_OUT_OF_RANGE",     0, JZ_RULE_MODE_ERR, "S5.5.7 sbit() index is out of range for source width" },
    { "OPERATORS_AND_EXPRESSIONS", "GSLICE_INDEX_OUT_OF_RANGE",   0, JZ_RULE_MODE_ERR, "S5.5.8 gslice() index is out of range for source width" },
    { "OPERATORS_AND_EXPRESSIONS", "GSLICE_WIDTH_INVALID",        0, JZ_RULE_MODE_ERR, "S5.5.8 gslice() width parameter must be a positive integer" },
    { "OPERATORS_AND_EXPRESSIONS", "SSLICE_INDEX_OUT_OF_RANGE",   0, JZ_RULE_MODE_ERR, "S5.5.9 sslice() index is out of range for source width" },
    { "OPERATORS_AND_EXPRESSIONS", "SSLICE_WIDTH_INVALID",        0, JZ_RULE_MODE_ERR, "S5.5.9 sslice() width parameter must be a positive integer" },
    { "OPERATORS_AND_EXPRESSIONS", "SSLICE_VALUE_WIDTH_MISMATCH", 0, JZ_RULE_MODE_ERR, "S5.5.9 sslice() value argument width does not match width parameter" },
    { "OPERATORS_AND_EXPRESSIONS", "OH2B_INPUT_TOO_NARROW",      0, JZ_RULE_MODE_ERR, "S5.5.12 oh2b() source must be at least 2 bits wide" },
    { "OPERATORS_AND_EXPRESSIONS", "B2OH_WIDTH_INVALID",         0, JZ_RULE_MODE_ERR, "S5.5.13 b2oh() width must be a positive constant >= 2" },
    { "OPERATORS_AND_EXPRESSIONS", "PRIENC_INPUT_TOO_NARROW",    0, JZ_RULE_MODE_ERR, "S5.5.14 prienc() source must be at least 2 bits wide" },
    { "OPERATORS_AND_EXPRESSIONS", "BSWAP_WIDTH_NOT_BYTE_ALIGNED", 0, JZ_RULE_MODE_ERR, "S5.5.26 bswap() source width must be a multiple of 8" },

    /* [IDENTIFIERS_AND_SCOPE] */
    { "IDENTIFIERS_AND_SCOPE", "ID_DUP_IN_MODULE",                  0, JZ_RULE_MODE_ERR, "S4.2/S8.1 Duplicate identifier within module (ports, wires, registers, consts, instances)" },
    { "IDENTIFIERS_AND_SCOPE", "MODULE_NAME_DUP_IN_PROJECT",        1, JZ_RULE_MODE_ERR, "S4.2/S6.10/S8.1 Module name not unique across project" },
    { "IDENTIFIERS_AND_SCOPE", "BLACKBOX_NAME_DUP_IN_PROJECT",      0, JZ_RULE_MODE_ERR, "S6.7/S6.10 Blackbox name conflicts with existing module/blackbox name" },
    { "IDENTIFIERS_AND_SCOPE", "INSTANCE_NAME_DUP_IN_MODULE",       0, JZ_RULE_MODE_ERR, "S4.2/S8.1 Multiple instances with same name in parent module" },
    { "IDENTIFIERS_AND_SCOPE", "INSTANCE_NAME_CONFLICT",            0, JZ_RULE_MODE_ERR, "S4.2/S8.1 Instance name matches another identifier (port/wire/register/CONST) in parent module" },
    { "IDENTIFIERS_AND_SCOPE", "UNDECLARED_IDENTIFIER",             1, JZ_RULE_MODE_ERR, "S4.2/S8.1 Use of undeclared identifier in expression or statement" },
    { "IDENTIFIERS_AND_SCOPE", "AMBIGUOUS_REFERENCE",               0, JZ_RULE_MODE_ERR, "S4.2/S8.1 Identifier reference ambiguous without instance prefix" },

    /* [CONST_RULES] */
    { "CONST_RULES", "CONST_NEGATIVE_OR_NONINT",                    0, JZ_RULE_MODE_ERR, "S4.3/S7.10 CONST initialized with negative or non-integer value where nonnegative integer required" },
    { "CONST_RULES", "CONST_UNDEFINED_IN_WIDTH_OR_SLICE",           0, JZ_RULE_MODE_ERR, "S1.3/S2.1/S7.10 CONST used in width/slice not declared or evaluates invalidly" },
    { "CONST_RULES", "CONST_CIRCULAR_DEP",                         1, JZ_RULE_MODE_ERR, "S4.3/S7.10 Circular dependency in CONST/CONFIG definitions" },

    /* [PORT_WIRE_REGISTER_DECLS] */
    { "PORT_WIRE_REGISTER_DECLS", "PORT_MISSING_WIDTH",             0, JZ_RULE_MODE_ERR, "S4.4/S8.1 Port declaration without mandatory `[N]` width" },
    { "PORT_WIRE_REGISTER_DECLS", "PORT_DIRECTION_MISMATCH_IN",     0, JZ_RULE_MODE_ERR, "S4.4/S5.1 Cannot assign to IN port; IN ports are read-only inside the module" },
    { "PORT_WIRE_REGISTER_DECLS", "PORT_DIRECTION_MISMATCH_OUT",    0, JZ_RULE_MODE_ERR, "S4.4/S5.1/S5.2/S8.1 Reading from OUT port inside module (outputs are write-only)" },
    { "PORT_WIRE_REGISTER_DECLS", "PORT_TRISTATE_MISMATCH",         0, JZ_RULE_MODE_ERR, "S4.4/S4.10/S8.1 Only INOUT ports may use `z` for tri-state; IN/OUT ports must drive 0/1/x" },
    { "PORT_WIRE_REGISTER_DECLS", "WIRE_MULTI_DIMENSIONAL",         0, JZ_RULE_MODE_ERR, "S4.5 WIRE declared with multi-dimensional syntax" },
    { "PORT_WIRE_REGISTER_DECLS", "REG_MULTI_DIMENSIONAL",          0, JZ_RULE_MODE_ERR, "S4.7 REGISTER declared with multi-dimensional syntax" },
    { "PORT_WIRE_REGISTER_DECLS", "REG_MISSING_INIT_LITERAL",       0, JZ_RULE_MODE_ERR, "S4.7 Register declared without mandatory reset/power-on literal" },
    { "PORT_WIRE_REGISTER_DECLS", "REG_INIT_CONTAINS_X",            0, JZ_RULE_MODE_ERR, "S2.1/S4.7 Register initialization literal must not contain `x` bits" },
    { "PORT_WIRE_REGISTER_DECLS", "REG_INIT_CONTAINS_Z",            0, JZ_RULE_MODE_ERR, "S2.1/S4.7 Register initialization literal must not contain `z` bits" },
    { "PORT_WIRE_REGISTER_DECLS", "REG_INIT_WIDTH_MISMATCH",       0, JZ_RULE_MODE_ERR, "S4.7 Register initialization literal width does not match declared register width" },
    { "PORT_WIRE_REGISTER_DECLS", "WRITE_WIRE_IN_SYNC",             2, JZ_RULE_MODE_ERR, "S4.5/S5.2 Cannot assign to WIRE in SYNCHRONOUS block; use a REGISTER, or move to ASYNCHRONOUS" },
    { "PORT_WIRE_REGISTER_DECLS", "ASSIGN_TO_NON_REGISTER_IN_SYNC", 1, JZ_RULE_MODE_ERR, "S5.2 Only REGISTERs may be assigned in SYNCHRONOUS blocks; declare as REGISTER or move to ASYNCHRONOUS" },
    { "PORT_WIRE_REGISTER_DECLS", "MODULE_MISSING_PORT",            0, JZ_RULE_MODE_ERR, "S4.2/S4.4/S8.1 Module missing required PORT block or PORT block is empty (no ports declared)" },
    { "PORT_WIRE_REGISTER_DECLS", "MODULE_PORT_IN_ONLY",            0, JZ_RULE_MODE_WRN, "S4.2/S4.4/S8.3 Module declares only IN ports (no OUT/INOUT; likely dead code)" },

    /* [LATCH_RULES] */
    { "LATCH_RULES", "LATCH_ASSIGN_NON_GUARDED",                    0, JZ_RULE_MODE_ERR, "S4.8/S4.10 LATCH must be written via guarded assignment '<name> <= enable : data;' inside ASYNCHRONOUS block; other assignments to LATCH are forbidden" },
    { "LATCH_RULES", "LATCH_ASSIGN_IN_SYNC",                        2, JZ_RULE_MODE_ERR, "S4.8/S4.11 LATCH may not be written in SYNCHRONOUS blocks; use REGISTER for edge-triggered storage" },
    { "LATCH_RULES", "LATCH_ENABLE_WIDTH_NOT_1",                    0, JZ_RULE_MODE_ERR, "S4.8 D-latch enable expression must have width [1]" },
    { "LATCH_RULES", "LATCH_ALIAS_FORBIDDEN",                       0, JZ_RULE_MODE_ERR, "S4.8 LATCH may not be aliased using '='; latches must not be merged into other nets" },
    { "LATCH_RULES", "LATCH_INVALID_TYPE",                         0, JZ_RULE_MODE_ERR, "S4.8 LATCH type must be D or SR" },
    { "LATCH_RULES", "LATCH_WIDTH_INVALID",                        0, JZ_RULE_MODE_ERR, "S4.8 LATCH width must be a positive integer" },
    { "LATCH_RULES", "LATCH_SR_WIDTH_MISMATCH",                    0, JZ_RULE_MODE_ERR, "S4.8 SR latch set/reset expression width does not match latch width" },
    { "LATCH_RULES", "LATCH_AS_CLOCK_OR_CDC",                      0, JZ_RULE_MODE_ERR, "S4.8/S4.12 LATCH may not be used as a clock signal or in CDC declarations" },
    { "LATCH_RULES", "LATCH_IN_CONST_CONTEXT",                     0, JZ_RULE_MODE_ERR, "S4.8 LATCH identifier may not be used in compile-time constant contexts (@check/@feature conditions)" },
    { "LATCH_RULES", "LATCH_CHIP_UNSUPPORTED",                    0, JZ_RULE_MODE_ERR, "S4.8 LATCH type not supported by selected CHIP" },

    /* [MODULE_AND_INSTANTIATION] */
    { "MODULE_AND_INSTANTIATION", "INSTANCE_MISSING_PORT",                 0, JZ_RULE_MODE_ERR, "S4.13/S6.9 Not all child module ports listed in @new block" },
    { "MODULE_AND_INSTANTIATION", "INSTANCE_PORT_WIDTH_MISMATCH",          0, JZ_RULE_MODE_ERR, "S4.13/S6.9/S8.1 Instantiated port width does not match child module effective port width" },
    { "MODULE_AND_INSTANTIATION", "INSTANCE_PORT_DIRECTION_MISMATCH",      0, JZ_RULE_MODE_ERR, "S4.13/S6.9 Child port direction incompatible with connection/pin category" },
    { "MODULE_AND_INSTANTIATION", "INSTANCE_BUS_MISMATCH",                 0, JZ_RULE_MODE_ERR, "S4.13/S6.9 Instance BUS binding must match child BUS port BUS id and role" },
    { "MODULE_AND_INSTANTIATION", "INSTANCE_OVERRIDE_CONST_UNDEFINED",     0, JZ_RULE_MODE_ERR, "S4.13 Override LHS not a CONST declared in child module" },
    { "MODULE_AND_INSTANTIATION", "INSTANCE_PORT_WIDTH_EXPR_INVALID",      0, JZ_RULE_MODE_ERR, "S4.13 Width expression in instantiation uses undefined CONST/CONFIG or invalid expression" },
    { "MODULE_AND_INSTANTIATION", "INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH", 0, JZ_RULE_MODE_ERR, "S4.13/S8.1 Bound parent signal width does not match instantiation port width" },
    { "MODULE_AND_INSTANTIATION", "INSTANCE_ARRAY_COUNT_INVALID",          0, JZ_RULE_MODE_ERR, "S4.13.1 Instance array count [N] must be a positive integer expression" },
    { "MODULE_AND_INSTANTIATION", "INSTANCE_ARRAY_IDX_INVALID_CONTEXT",    2, JZ_RULE_MODE_ERR, "S4.13.1 IDX may be used only in parent-signal bindings of instance arrays, not in scalar @new or OVERRIDE expressions" },
    { "MODULE_AND_INSTANTIATION", "INSTANCE_ARRAY_PARENT_BIT_OVERLAP",     0, JZ_RULE_MODE_ERR, "S4.13.1 Instance array OUT mappings drive overlapping bits of the same parent signal for different instance indices" },
    { "MODULE_AND_INSTANTIATION", "INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE", 0, JZ_RULE_MODE_ERR, "S4.13.1 Instance array IDX-dependent slice evaluates to indices outside the parent signal width for some instance index" },
    { "MODULE_AND_INSTANTIATION", "INSTANCE_OUT_PORT_LITERAL",            0, JZ_RULE_MODE_ERR, "S4.13 OUT port binding in @new may not be a literal value" },
    { "MODULE_AND_INSTANTIATION", "INSTANCE_ARRAY_MULTI_DIMENSIONAL",     0, JZ_RULE_MODE_ERR, "S4.13.1 Multi-dimensional instance arrays are not supported" },

    /* [MUX_RULES] */
    { "MUX_RULES", "MUX_ASSIGN_LHS",                                2, JZ_RULE_MODE_ERR, "S4.6 Assigning to MUX id or its indexed form on LHS is forbidden (MUX is read-only)" },
    { "MUX_RULES", "MUX_AGG_SOURCE_WIDTH_MISMATCH",                 0, JZ_RULE_MODE_ERR, "S4.6 Aggregation form sources must all have identical bit-width" },
    { "MUX_RULES", "MUX_AGG_SOURCE_INVALID",                        0, JZ_RULE_MODE_ERR, "S4.6 Aggregation source not a valid readable signal in module scope" },
    { "MUX_RULES", "MUX_SLICE_WIDTH_NOT_DIVISOR",                   0, JZ_RULE_MODE_ERR, "S4.6 Auto-slicing form requires wide_source width to be exact multiple of element_width" },
    { "MUX_RULES", "MUX_SELECTOR_OUT_OF_RANGE_CONST",               0, JZ_RULE_MODE_ERR, "S4.6 Selector statically provable outside valid index range" },
    { "MUX_RULES", "MUX_NAME_DUPLICATE",                            0, JZ_RULE_MODE_ERR, "S4.6 MUX identifier duplicates another identifier in module" },

    /* [ASSIGNMENTS_AND_EXCLUSIVE] */
    { "ASSIGNMENTS_AND_EXCLUSIVE", "ASSIGN_WIDTH_NO_MODIFIER",          1, JZ_RULE_MODE_ERR, "S4.10/S5.0 Width mismatch: use `<=z`/`<=s` (zero/sign-extend), `<=t` (truncate), or match widths explicitly" },
    { "ASSIGNMENTS_AND_EXCLUSIVE", "ASSIGN_TRUNCATES",              1, JZ_RULE_MODE_ERR, "S4.10/S5.0 Assignment truncates RHS into smaller LHS; use a slice or explicit truncation modifier" },
    { "ASSIGNMENTS_AND_EXCLUSIVE", "ASSIGN_SLICE_WIDTH_MISMATCH",       2, JZ_RULE_MODE_ERR, "S4.10/S5.1/S5.2 Slice assignment where source and destination slice widths differ" },
    { "ASSIGNMENTS_AND_EXCLUSIVE", "ASSIGN_CONCAT_WIDTH_MISMATCH",      2, JZ_RULE_MODE_ERR, "S4.10/S5.1/S5.2 Concatenation on either side of assignment has width sum that does not match paired expression width" },
    { "ASSIGNMENTS_AND_EXCLUSIVE", "ASSIGN_MULTIPLE_SAME_BITS",         0, JZ_RULE_MODE_ERR, "S1.5/S5.2 Same bits assigned more than once on a single execution path (exclusive assignment violation)" },
    { "ASSIGNMENTS_AND_EXCLUSIVE", "ASSIGN_INDEPENDENT_IF_SELECT",      1, JZ_RULE_MODE_ERR, "S1.5/S5.3/S5.4/S8.1 Same identifier assigned in multiple independent IF/SELECT chains at same nesting level" },
    { "ASSIGNMENTS_AND_EXCLUSIVE", "ASSIGN_SHADOWING",                  0, JZ_RULE_MODE_ERR, "S1.5/S5.2/S8.1 Assignment at higher nesting level followed by nested assignment to same bits (sequential shadowing)" },
    { "ASSIGNMENTS_AND_EXCLUSIVE", "ASSIGN_SLICE_OVERLAP",              0, JZ_RULE_MODE_ERR, "S5.2/S8.1 Overlapping part-select assignments to same identifier bits in any single execution path" },
    { "ASSIGNMENTS_AND_EXCLUSIVE", "ASYNC_UNDEFINED_PATH_NO_DRIVER",    0, JZ_RULE_MODE_ERR, "S1.5/S4.10/S5.1 Signal undriven on some ASYNCHRONOUS paths; add an ELSE branch or DEFAULT case" },

    /* [ASYNC_BLOCK_RULES] */
    { "ASYNC_BLOCK_RULES", "ASYNC_INVALID_STATEMENT_TARGET",        1, JZ_RULE_MODE_ERR, "S4.10/S5.1/S8.1 LHS in ASYNCHRONOUS assignment is not assignable (e.g. CONST, function call)" },
    { "ASYNC_BLOCK_RULES", "ASYNC_ASSIGN_REGISTER",                 0, JZ_RULE_MODE_ERR, "S4.7/S5.1 Cannot write REGISTER in ASYNCHRONOUS block; move assignment to a SYNCHRONOUS block" },
    { "ASYNC_BLOCK_RULES", "ASYNC_ALIAS_LITERAL_RHS",               0, JZ_RULE_MODE_ERR, "S4.10/S5.1 Literal on RHS of `=` in ASYNCHRONOUS block; did you mean `<=` or `=>`?" },
    { "ASYNC_BLOCK_RULES", "ASYNC_FLOATING_Z_READ",                 0, JZ_RULE_MODE_ERR, "S4.10/S1.5/S8.1 Net has sinks but all drivers assign `z` (tri-state bus fully released while read)" },

    /* [SYNC_BLOCK_RULES] */
    { "SYNC_BLOCK_RULES", "SYNC_MULTI_ASSIGN_SAME_REG_BITS",        0, JZ_RULE_MODE_ERR, "S5.2/S8.1 Same register bits assigned more than once along any execution path in SYNCHRONOUS block" },
    { "SYNC_BLOCK_RULES", "SYNC_ROOT_AND_CONDITIONAL_ASSIGN",       0, JZ_RULE_MODE_ERR, "S5.2/S1.5/S8.1 Root-level register assignment combined with nested conditional assignment to same bits" },
    { "SYNC_BLOCK_RULES", "SYNC_SLICE_WIDTH_MISMATCH",              0, JZ_RULE_MODE_ERR, "S5.2 Register slice assignment expression width not equal to slice width" },
    { "SYNC_BLOCK_RULES", "SYNC_CONCAT_DUP_REG",                    1, JZ_RULE_MODE_ERR, "S5.2 Concatenation LHS includes same register more than once" },
    { "SYNC_BLOCK_RULES", "SYNC_NO_ALIAS",                          0, JZ_RULE_MODE_ERR, "S5.2 Aliasing `=` is forbidden in SYNCHRONOUS blocks; did you mean `<=` (receive) or `=>` (drive)?" },
    { "SYNC_BLOCK_RULES", "DOMAIN_CONFLICT",                        0, JZ_RULE_MODE_ERR, "S4.11/S4.12 Register or CDC alias used in SYNCHRONOUS block whose CLK does not match its home-domain clock" },
    { "SYNC_BLOCK_RULES", "DUPLICATE_BLOCK",                        0, JZ_RULE_MODE_ERR, "S4.11/S4.12 More than one SYNCHRONOUS block declared for the same clock signal in a module" },
    { "SYNC_BLOCK_RULES", "MULTI_CLK_ASSIGN",                       0, JZ_RULE_MODE_ERR, "S4.11/S4.12 Same register assigned in SYNCHRONOUS blocks driven by different clocks or conflicting CDC home domains" },
    { "SYNC_BLOCK_RULES", "CDC_SOURCE_NOT_REGISTER",                0, JZ_RULE_MODE_ERR, "S4.12 CDC source must be a REGISTER declared in the module" },
    { "SYNC_BLOCK_RULES", "CDC_BIT_WIDTH_NOT_1",                    0, JZ_RULE_MODE_ERR, "S4.12 BIT CDC source register must have width [1]" },
    { "SYNC_BLOCK_RULES", "SYNC_CLK_WIDTH_NOT_1",                  0, JZ_RULE_MODE_ERR, "S4.11 SYNCHRONOUS block CLK signal must have width [1]" },
    { "SYNC_BLOCK_RULES", "SYNC_RESET_WIDTH_NOT_1",                0, JZ_RULE_MODE_ERR, "S4.11 SYNCHRONOUS block RESET signal must have width [1]" },
    { "SYNC_BLOCK_RULES", "SYNC_EDGE_INVALID",                     0, JZ_RULE_MODE_ERR, "S4.11 SYNCHRONOUS block EDGE must be Rising, Falling, or Both" },
    { "SYNC_BLOCK_RULES", "SYNC_RESET_ACTIVE_INVALID",             0, JZ_RULE_MODE_ERR, "S4.11 SYNCHRONOUS block RESET_ACTIVE must be High or Low" },
    { "SYNC_BLOCK_RULES", "SYNC_RESET_TYPE_INVALID",               0, JZ_RULE_MODE_ERR, "S4.11 SYNCHRONOUS block RESET_TYPE must be Clocked or Immediate" },
    { "SYNC_BLOCK_RULES", "SYNC_UNKNOWN_PARAM",                    0, JZ_RULE_MODE_ERR, "S4.11 Unknown SYNCHRONOUS block parameter; valid parameters are CLK, RESET, EDGE, RESET_ACTIVE, RESET_TYPE" },
    { "SYNC_BLOCK_RULES", "SYNC_MISSING_CLK",                      0, JZ_RULE_MODE_ERR, "S4.11 SYNCHRONOUS block must declare a CLK parameter" },
    { "SYNC_BLOCK_RULES", "CDC_SOURCE_NOT_PLAIN_REG",              0, JZ_RULE_MODE_ERR, "S4.12 CDC source must be a plain register identifier, not a slice or expression" },
    { "SYNC_BLOCK_RULES", "CDC_DEST_ALIAS_ASSIGNED",               0, JZ_RULE_MODE_ERR, "S4.12 CDC destination alias may not be assigned directly in block statements" },
    { "SYNC_BLOCK_RULES", "CDC_STAGES_INVALID",                    0, JZ_RULE_MODE_ERR, "S4.12 CDC stages parameter must be a positive integer" },
    { "SYNC_BLOCK_RULES", "CDC_TYPE_INVALID",                      0, JZ_RULE_MODE_ERR, "S4.12 CDC type must be BIT, BUS, FIFO, HANDSHAKE, PULSE, MCP, or RAW" },
    { "SYNC_BLOCK_RULES", "CDC_RAW_STAGES_FORBIDDEN",              0, JZ_RULE_MODE_ERR, "S4.12 RAW CDC type does not accept a stages parameter" },
    { "SYNC_BLOCK_RULES", "CDC_PULSE_WIDTH_NOT_1",                 0, JZ_RULE_MODE_ERR, "S4.12 PULSE CDC source register must have width [1]" },
    { "SYNC_BLOCK_RULES", "CDC_DEST_ALIAS_DUP",                    0, JZ_RULE_MODE_ERR, "S4.12 CDC destination alias name conflicts with an existing identifier in module scope" },
    { "SYNC_BLOCK_RULES", "SYNC_EDGE_BOTH_WARNING",                0, JZ_RULE_MODE_WRN, "S4.11 EDGE=Both (dual-edge clocking) may not be supported by all FPGA architectures" },

    /* [CONTROL_FLOW_IF_SELECT] */
    { "CONTROL_FLOW_IF_SELECT", "IF_COND_WIDTH_NOT_1",              0, JZ_RULE_MODE_ERR, "S5.3 IF/ELIF condition must be 1 bit wide; use a comparison or reduction operator" },
    { "CONTROL_FLOW_IF_SELECT", "CONTROL_FLOW_OUTSIDE_BLOCK",       0, JZ_RULE_MODE_ERR, "S5.3/S5.4/S8.1 Control-flow statements (IF/SELECT) used outside ASYNCHRONOUS or SYNCHRONOUS block" },
    { "CONTROL_FLOW_IF_SELECT", "SELECT_DUP_CASE_VALUE",            0, JZ_RULE_MODE_ERR, "S5.4/S8.1 Multiple CASE labels with same value in SELECT" },
    { "CONTROL_FLOW_IF_SELECT", "ASYNC_ALIAS_IN_CONDITIONAL",       0, JZ_RULE_MODE_ERR, "S4.10/S5.3 Alias `=` inside IF/SELECT is forbidden; did you mean `<=` or `=>`?" },
    { "CONTROL_FLOW_IF_SELECT", "SELECT_DEFAULT_RECOMMENDED_ASYNC", 0, JZ_RULE_MODE_WRN, "S5.4/S8.3 ASYNCHRONOUS SELECT without DEFAULT (may cause floating nets)" },
    { "CONTROL_FLOW_IF_SELECT", "SELECT_NO_MATCH_SYNC_OK",          0, JZ_RULE_MODE_WRN, "S5.4 In SYNCHRONOUS, missing DEFAULT simply holds registers (no error)" },
    { "CONTROL_FLOW_IF_SELECT", "SELECT_CASE_WIDTH_MISMATCH",      0, JZ_RULE_MODE_ERR, "S5.4 SELECT CASE value width does not match selector expression width" },

    /* [FEATURE_GUARDS] */
    { "FEATURE_GUARDS", "FEATURE_COND_WIDTH_NOT_1",                 0, JZ_RULE_MODE_ERR, "S4.14 Feature guard condition expression width must be 1" },
    { "FEATURE_GUARDS", "FEATURE_EXPR_INVALID_CONTEXT",             0, JZ_RULE_MODE_ERR, "S4.14 Feature guard condition may only reference CONFIG.<name>, module CONST, and literals" },
    { "FEATURE_GUARDS", "FEATURE_NESTED",                          0, JZ_RULE_MODE_ERR, "S4.14 @feature guards may not be nested inside other @feature guards" },
    { "FEATURE_GUARDS", "FEATURE_VALIDATION_BOTH_PATHS",           0, JZ_RULE_MODE_ERR, "S4.14 Both branches of @feature guard must pass full semantic validation" },

    /* [FUNCTIONS_AND_CLOG2] */
    { "FUNCTIONS_AND_CLOG2", "CLOG2_NONPOSITIVE_ARG",               0, JZ_RULE_MODE_ERR, "S5.5.5 Argument to clog2() <= 0" },
    { "FUNCTIONS_AND_CLOG2", "CLOG2_INVALID_CONTEXT",               0, JZ_RULE_MODE_ERR, "S5.5.5 clog2() used outside compile-time constant integer expression context" },
    { "FUNCTIONS_AND_CLOG2", "WIDTHOF_INVALID_CONTEXT",             0, JZ_RULE_MODE_ERR, "S5.5.10 widthof() used outside compile-time constant integer expression context" },
    { "FUNCTIONS_AND_LIT",  "LIT_WIDTH_INVALID",                   0, JZ_RULE_MODE_ERR, "S5.5.11 lit() width must be a positive integer constant expression" },
    { "FUNCTIONS_AND_LIT",  "LIT_VALUE_INVALID",                   0, JZ_RULE_MODE_ERR, "S5.5.11 lit() value must be a nonnegative integer constant expression" },
    { "FUNCTIONS_AND_LIT",  "LIT_VALUE_OVERFLOW",                  0, JZ_RULE_MODE_ERR, "S5.5.11 lit() value exceeds declared width" },
    { "FUNCTIONS_AND_LIT",  "LIT_INVALID_CONTEXT",                 0, JZ_RULE_MODE_ERR, "S5.5.11 lit() used where a compile-time integer constant is required" },
    { "FUNCTIONS_AND_CLOG2", "FUNC_RESULT_TRUNCATED_SILENTLY",      0, JZ_RULE_MODE_ERR, "S5.5/S5.2 Function result truncated by assignment without explicit slice or width check" },
    { "FUNCTIONS_AND_CLOG2", "WIDTHOF_INVALID_TARGET",             0, JZ_RULE_MODE_ERR, "S5.5.10 widthof() argument does not resolve to a WIRE, REGISTER, PORT, or BUS" },
    { "FUNCTIONS_AND_CLOG2", "WIDTHOF_INVALID_SYNTAX",             0, JZ_RULE_MODE_ERR, "S5.5.10 widthof() argument must be a plain identifier, not a slice/concat/qualified expression" },
    { "FUNCTIONS_AND_CLOG2", "WIDTHOF_WIDTH_NOT_RESOLVABLE",       0, JZ_RULE_MODE_ERR, "S5.5.10 widthof() target found but its width cannot be resolved" },

    /* [NET_DRIVERS_AND_TRI_STATE] */
    { "NET_DRIVERS_AND_TRI_STATE", "NET_FLOATING_WITH_SINK",        0, JZ_RULE_MODE_ERR, "S1.2/S4.10 Signal is read but never driven (floating net); add a driver or remove the read" },
    { "NET_DRIVERS_AND_TRI_STATE", "NET_TRI_STATE_ALL_Z_READ",      0, JZ_RULE_MODE_ERR, "S4.10 All drivers assign `z` (tri-state) but signal is read; at least one driver must provide a value" },
    { "NET_DRIVERS_AND_TRI_STATE", "NET_MULTIPLE_ACTIVE_DRIVERS",   1, JZ_RULE_MODE_ERR, "S1.2/S1.5/S4.10 Multiple active drivers on same signal; for tri-state, all but one must assign `z`" },
    { "NET_DRIVERS_AND_TRI_STATE", "NET_DANGLING_UNUSED",           1, JZ_RULE_MODE_WRN, "S5.1/S8.3 Signal is neither driven nor read; remove it or connect it" },

    /* [OBSERVABILITY_X] */
    { "OBSERVABILITY_X", "OBS_X_TO_OBSERVABLE_SINK",                0, JZ_RULE_MODE_ERR, "S1.2/S2.1/S3.2 Expression containing `x` bits drives REGISTER, MEM, or output; mask `x` bits before use" },

    /* [COMBINATIONAL_LOOPS] */
    { "COMBINATIONAL_LOOPS", "COMB_LOOP_UNCONDITIONAL",             0, JZ_RULE_MODE_ERR, "S5.3/S8.2 Combinational loop: signal feeds back to itself through ASYNCHRONOUS assignments" },
    { "COMBINATIONAL_LOOPS", "COMB_LOOP_CONDITIONAL_SAFE",          0, JZ_RULE_MODE_WRN, "S5.3/S8.2 Cycles only within mutually exclusive branches considered safe (no error)" },

    /* [BUS_RULES] */
    { "BUS_RULES", "BUS_DEF_DUP_NAME",                               0, JZ_RULE_MODE_ERR, "S6.8 Duplicate BUS definition name in project" },
    { "BUS_RULES", "BUS_DEF_SIGNAL_DUP_NAME",                        0, JZ_RULE_MODE_ERR, "S6.8 Duplicate signal name inside BUS definition" },
    { "BUS_RULES", "BUS_DEF_INVALID_DIR",                            0, JZ_RULE_MODE_ERR, "S6.8 BUS signal direction must be IN, OUT, or INOUT" },
    { "BUS_RULES", "BUS_PORT_UNKNOWN_BUS",                           0, JZ_RULE_MODE_ERR, "S4.4.1/S6.8 BUS port references BUS name not declared in project" },
    { "BUS_RULES", "BUS_PORT_INVALID_ROLE",                          2, JZ_RULE_MODE_ERR, "S4.4.1 BUS port role must be SOURCE or TARGET" },
    { "BUS_RULES", "BUS_PORT_ARRAY_COUNT_INVALID",                   0, JZ_RULE_MODE_ERR, "S4.4.1 BUS array count must be a positive integer constant expression" },
    { "BUS_RULES", "BUS_PORT_INDEX_REQUIRED",                        0, JZ_RULE_MODE_ERR, "S4.4.1 Arrayed BUS access requires an explicit index or wildcard" },
    { "BUS_RULES", "BUS_PORT_INDEX_NOT_ARRAY",                       0, JZ_RULE_MODE_ERR, "S4.4.1 Indexed BUS access requires an arrayed BUS port" },
    { "BUS_RULES", "BUS_PORT_INDEX_OUT_OF_RANGE",                    0, JZ_RULE_MODE_ERR, "S4.4.1 BUS port index is outside the declared range" },
    { "BUS_RULES", "BUS_PORT_NOT_BUS",                               0, JZ_RULE_MODE_ERR, "S4.4.1 BUS member access used on non-BUS port" },
    { "BUS_RULES", "BUS_SIGNAL_UNDEFINED",                           0, JZ_RULE_MODE_ERR, "S4.4.1 BUS signal does not exist in BUS definition" },
    { "BUS_RULES", "BUS_SIGNAL_READ_FROM_WRITABLE",                  0, JZ_RULE_MODE_ERR, "S4.4.1 Read access to writable BUS signal is not allowed" },
    { "BUS_RULES", "BUS_SIGNAL_WRITE_TO_READABLE",                   0, JZ_RULE_MODE_ERR, "S4.4.1 Write access to readable BUS signal is not allowed" },
    { "BUS_RULES", "BUS_WILDCARD_WIDTH_MISMATCH",                    0, JZ_RULE_MODE_ERR, "S4.4.1 BUS wildcard assignment requires RHS width of 1 or array count" },
    { "BUS_RULES", "BUS_TRISTATE_MISMATCH",                          0, JZ_RULE_MODE_ERR, "S4.4.1 Only writable BUS signals (INOUT or OUT from this role) may be assigned 'z' for tri-state" },
    { "BUS_RULES", "BUS_BULK_BUS_MISMATCH",                          0, JZ_RULE_MODE_ERR, "S6.8 Bulk BUS assignment requires both sides to reference the same BUS id" },
    { "BUS_RULES", "BUS_BULK_ROLE_CONFLICT",                         0, JZ_RULE_MODE_ERR, "S6.8 Bulk BUS assignment between instances with the same BUS role (SOURCE/SOURCE or TARGET/TARGET) is not allowed" },

    /* [PROJECT_AND_IMPORTS] */
    { "PROJECT_AND_IMPORTS", "PROJECT_MULTIPLE_PER_FILE",           0, JZ_RULE_MODE_ERR, "S6.2/S6.9 Multiple @project directives in same file" },
    { "PROJECT_AND_IMPORTS", "PROJECT_NAME_NOT_UNIQUE",             0, JZ_RULE_MODE_ERR, "S6.10 Project name conflicts within design environment or with module name" },
    { "PROJECT_AND_IMPORTS", "PROJECT_MISSING_TOP_MODULE",          0, JZ_RULE_MODE_ERR, "S6.9 Project does not declare a top-level @top module binding" },
    { "PROJECT_AND_IMPORTS", "PROJECT_CHIP_DATA_NOT_FOUND",         0, JZ_RULE_MODE_ERR, "S6.1 Chip data not found for CHIP id (no local JSON and no built-in data)" },
    { "PROJECT_AND_IMPORTS", "PROJECT_CHIP_DATA_INVALID",           0, JZ_RULE_MODE_ERR, "S6.1 Chip JSON data could not be parsed" },
    { "PROJECT_AND_IMPORTS", "IMPORT_OUTSIDE_PROJECT",              0, JZ_RULE_MODE_ERR, "S6.2.1 @import used outside @project block" },
    { "PROJECT_AND_IMPORTS", "IMPORT_NOT_AT_PROJECT_TOP",           0, JZ_RULE_MODE_ERR, "S6.2.1 @import appears after CONFIG/CLOCKS/PIN/blackbox/top-level new blocks" },
    { "PROJECT_AND_IMPORTS", "IMPORT_FILE_HAS_PROJECT",             0, JZ_RULE_MODE_ERR, "S6.2.1 Imported file contains its own @project/@endproj (forbidden)" },
    { "PROJECT_AND_IMPORTS", "IMPORT_DUP_MODULE_OR_BLACKBOX",       0, JZ_RULE_MODE_ERR, "S6.2.1/S6.10 Imported module/blackbox name duplicates existing project name" },
    { "PROJECT_AND_IMPORTS", "IMPORT_FILE_MULTIPLE_TIMES",          0, JZ_RULE_MODE_ERR, "S6.2.1 Same source file imported more than once into a single project (duplicate @import or nested re-import)" },
    { "PROJECT_AND_IMPORTS", "PROJECT_MISSING_ENDPROJ",           0, JZ_RULE_MODE_ERR, "S6.2 @project block missing @endproj terminator" },

    /* [CONFIG_BLOCK] */
    { "CONFIG_BLOCK", "CONFIG_MULTIPLE_BLOCKS",                     0, JZ_RULE_MODE_ERR, "S6.3 More than one CONFIG block defined in project" },
    { "CONFIG_BLOCK", "CONFIG_NAME_DUPLICATE",                      0, JZ_RULE_MODE_ERR, "S6.3 Duplicate config_id within CONFIG block" },
    { "CONFIG_BLOCK", "CONFIG_INVALID_EXPR_TYPE",                   0, JZ_RULE_MODE_ERR, "S6.3 CONFIG value not a nonnegative integer expression" },
    { "CONFIG_BLOCK", "CONFIG_FORWARD_REF",                         0, JZ_RULE_MODE_ERR, "S6.3 CONFIG entry references later CONFIG.<name> (forward reference)" },
    { "CONFIG_BLOCK", "CONFIG_USE_UNDECLARED",                      0, JZ_RULE_MODE_ERR, "S6.3 Use of CONFIG.<name> not declared in project CONFIG" },
    { "CONFIG_BLOCK", "CONFIG_CIRCULAR_DEP",                        1, JZ_RULE_MODE_ERR, "S6.3 Circular dependency between CONFIG entries" },
    { "CONFIG_BLOCK", "CONFIG_USED_WHERE_FORBIDDEN",                0, JZ_RULE_MODE_ERR, "S6.3 CONFIG.<name> used outside compile-time constant expression contexts (runtime expression)" },
    { "CONFIG_BLOCK", "CONST_USED_WHERE_FORBIDDEN",                 0, JZ_RULE_MODE_ERR, "S4.3/S6.3 CONST identifier used outside compile-time constant expression contexts (runtime expression)" },
    { "CONFIG_BLOCK", "CONST_STRING_IN_NUMERIC_CONTEXT",           1, JZ_RULE_MODE_ERR, "S4.3/S6.3 String CONST/CONFIG value used where a numeric expression is expected" },
    { "CONFIG_BLOCK", "CONST_NUMERIC_IN_STRING_CONTEXT",           1, JZ_RULE_MODE_ERR, "S4.3/S6.3 Numeric CONST/CONFIG value used where a string is expected (e.g. @file path)" },
 
    /* [CHECK_RULES] */
    { "CHECK_RULES", "CHECK_FAILED",                                 0, JZ_RULE_MODE_ERR, "S9.2 @check compile-time assertion failed" },
    { "CHECK_RULES", "CHECK_INVALID_EXPR_TYPE",                      0, JZ_RULE_MODE_ERR, "S9.1 @check condition must be a nonnegative integer constant expression over literals, module CONST, and project CONFIG.<name> (no runtime signals)" },
    { "CHECK_RULES", "CHECK_INVALID_PLACEMENT",                     0, JZ_RULE_MODE_ERR, "S9.3 @check may not appear inside conditional or @feature bodies" },

     /* [GLOBAL_BLOCK] */
    { "GLOBAL_BLOCK", "GLOBAL_NAMESPACE_DUPLICATE",                  0, JZ_RULE_MODE_ERR, "S8.4 Duplicate @global namespace name in compilation root" },
    { "GLOBAL_BLOCK", "GLOBAL_CONST_NAME_DUPLICATE",                0, JZ_RULE_MODE_ERR, "S8.4 Duplicate constant identifier within a single @global block" },
    { "GLOBAL_BLOCK", "GLOBAL_FORWARD_REF",                          0, JZ_RULE_MODE_ERR, "S8.4 Global constant expression references a later constant in the same @global block" },
    { "GLOBAL_BLOCK", "GLOBAL_CIRCULAR_DEP",                         1, JZ_RULE_MODE_ERR, "S8.4 Circular dependency between constants in an @global block" },
    { "GLOBAL_BLOCK", "GLOBAL_INVALID_EXPR_TYPE",                    0, JZ_RULE_MODE_ERR, "S8.4 Global value must be a sized literal <width>'<base><value>" },
    { "GLOBAL_BLOCK", "GLOBAL_CONST_USE_UNDECLARED",                 0, JZ_RULE_MODE_ERR, "S8.4 Use of GLOBAL.<name> where the namespace or constant is not declared" },
    { "GLOBAL_BLOCK", "GLOBAL_USED_WHERE_FORBIDDEN",                 0, JZ_RULE_MODE_ERR, "S8.4 GLOBAL.<name> used in a context where global constants are forbidden (CONST/OVERRIDE/CONFIG)" },
    { "GLOBAL_BLOCK", "GLOBAL_ASSIGN_FORBIDDEN",                    0, JZ_RULE_MODE_ERR, "S8.5 Assignment to GLOBAL constant is forbidden; globals are read-only" },

    /* [CLOCKS_PINS_MAP] */
    { "CLOCKS_PINS_MAP", "CLOCK_PORT_WIDTH_NOT_1",                  0, JZ_RULE_MODE_ERR, "S6.4/S6.5/S6.9 Clock pin width is not [1]" },
    { "CLOCKS_PINS_MAP", "CLOCK_NAME_NOT_IN_PINS",                  0, JZ_RULE_MODE_ERR, "S6.4/S6.5/S6.9 Clock name in CLOCKS has no matching IN_PINS declaration" },
    { "CLOCKS_PINS_MAP", "CLOCK_DUPLICATE_NAME",                    0, JZ_RULE_MODE_ERR, "S6.4/S6.10/S6.9 Duplicate clock names in CLOCKS block" },
    { "CLOCKS_PINS_MAP", "CLOCK_PERIOD_NONPOSITIVE",                0, JZ_RULE_MODE_ERR, "S6.4/S6.9 Clock period <= 0" },
    { "CLOCKS_PINS_MAP", "CLOCK_EDGE_INVALID",                      0, JZ_RULE_MODE_ERR, "S6.4/S6.9 Invalid edge specifier (not Rising/Falling)" },
    { "CLOCKS_PINS_MAP", "PIN_DECLARED_MULTIPLE_BLOCKS",            0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Same pin name appears in more than one of IN_PINS/OUT_PINS/INOUT_PINS" },
    { "CLOCKS_PINS_MAP", "PIN_INVALID_STANDARD",                    0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Invalid electrical standard in PIN declaration" },
    { "CLOCKS_PINS_MAP", "PIN_DRIVE_MISSING_OR_INVALID",            0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Missing or nonpositive drive value for OUT_PINS/INOUT_PINS" },
    { "CLOCKS_PINS_MAP", "PIN_BUS_WIDTH_INVALID",                   0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Bus pin width non-integer or <= 0" },
    { "CLOCKS_PINS_MAP", "PIN_DUP_NAME_WITHIN_BLOCK",               0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Duplicate pin names within same PIN block" },
    { "CLOCKS_PINS_MAP", "MAP_PIN_DECLARED_NOT_MAPPED",             0, JZ_RULE_MODE_ERR, "S6.6/S6.10/S6.9 Pin declared in PIN blocks but not mapped in MAP" },
    { "CLOCKS_PINS_MAP", "MAP_PIN_MAPPED_NOT_DECLARED",             0, JZ_RULE_MODE_ERR, "S6.6/S6.9 MAP entry references undeclared pin" },
    { "CLOCKS_PINS_MAP", "MAP_DUP_PHYSICAL_LOCATION",               0, JZ_RULE_MODE_WRN, "S6.6/S6.9 Two logical pins mapped to same physical board pin (warning or error; treated as warning)" },
    { "CLOCKS_PINS_MAP", "MAP_INVALID_BOARD_PIN_ID",                0, JZ_RULE_MODE_ERR, "S6.6/S6.9 Board pin ID format invalid for target device" },
    { "CLOCKS_PINS_MAP", "PIN_MODE_INVALID",                       0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Invalid pin mode value (must be SINGLE or DIFFERENTIAL)" },
    { "CLOCKS_PINS_MAP", "PIN_MODE_STANDARD_MISMATCH",             0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Pin mode conflicts with I/O standard (e.g. DIFFERENTIAL with single-ended standard)" },
    { "CLOCKS_PINS_MAP", "PIN_PULL_INVALID",                       0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Invalid pull value (must be UP, DOWN, or NONE)" },
    { "CLOCKS_PINS_MAP", "PIN_PULL_ON_OUTPUT",                     0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Pull resistor specified on output-only pin (OUT_PINS)" },
    { "CLOCKS_PINS_MAP", "PIN_TERM_INVALID",                       0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Invalid termination value (must be ON or OFF)" },
    { "CLOCKS_PINS_MAP", "PIN_TERM_INVALID_FOR_STANDARD",          0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Termination not supported for this I/O standard" },
    { "CLOCKS_PINS_MAP", "PIN_DIFF_OUT_MISSING_FCLK",              0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Differential output pin missing required fclk attribute" },
    { "CLOCKS_PINS_MAP", "PIN_DIFF_OUT_MISSING_PCLK",              0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Differential output pin missing required pclk attribute" },
    { "CLOCKS_PINS_MAP", "PIN_DIFF_OUT_MISSING_RESET",             0, JZ_RULE_MODE_ERR, "S6.5/S6.9 Differential output pin missing required reset attribute" },
    { "CLOCKS_PINS_MAP", "MAP_DIFF_EXPECTED_PAIR",                 0, JZ_RULE_MODE_ERR, "S6.6/S6.9 Differential pin must use { P=<id>, N=<id> } MAP syntax" },
    { "CLOCKS_PINS_MAP", "MAP_SINGLE_UNEXPECTED_PAIR",             0, JZ_RULE_MODE_ERR, "S6.6/S6.9 Single-ended pin must not use { P, N } MAP syntax" },
    { "CLOCKS_PINS_MAP", "MAP_DIFF_MISSING_PN",                    0, JZ_RULE_MODE_ERR, "S6.6/S6.9 Differential MAP entry missing P or N identifier" },
    { "CLOCKS_PINS_MAP", "MAP_DIFF_SAME_PIN",                      0, JZ_RULE_MODE_ERR, "S6.6/S6.9 Differential MAP entry has same physical pin for P and N" },

    /* [CLOCK_GEN_RULES] */
    { "CLOCK_GEN_RULES", "CLOCK_GEN_INPUT_NOT_DECLARED",           0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN input clock not declared in CLOCKS block" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_INPUT_NO_PERIOD",              0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN input clock must have a period declared in CLOCKS block" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE",     0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN input frequency outside chip's supported reference clock range" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_OUTPUT_INVALID_SELECTOR",      0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN output selector not valid for this generator type" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_OUT_NOT_CLOCK",                0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN OUT used for non-clock output; use WIRE instead" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_WIRE_IS_CLOCK",                0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN WIRE used for clock output; use OUT instead" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_WIRE_IN_CLOCKS",               0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN WIRE output must not be declared in CLOCKS block" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_OUTPUT_NOT_DECLARED",          0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN output clock not declared in CLOCKS block" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_OUTPUT_HAS_PERIOD",            0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN output clock must not have a period in CLOCKS block" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_OUTPUT_IS_INPUT_PIN",          0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN output clock must not be declared as IN_PINS" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_MULTIPLE_DRIVERS",             0, JZ_RULE_MODE_ERR, "S6.4.1 Clock is driven by multiple CLOCK_GEN outputs" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_INPUT_IS_SELF_OUTPUT",         0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN input clock is an output of the same CLOCK_GEN block" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_INVALID_TYPE",                 0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN generator must be PLL, DLL, CLKDIV, OSC, or BUF (with optional numeric suffix)" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_MISSING_INPUT",                0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN unit must have an IN clock declaration" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_REQUIRED_INPUT_MISSING",      0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN required input is not provided" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_MISSING_OUTPUT",               0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN unit must have at least one OUT clock declaration" },
    { "CLOCK_GEN_RULES", "CLOCK_EXTERNAL_NO_PERIOD",               0, JZ_RULE_MODE_ERR, "S6.4 External clock (IN_PINS) must have period declared in CLOCKS block" },
    { "CLOCK_GEN_RULES", "CLOCK_SOURCE_AMBIGUOUS",                 0, JZ_RULE_MODE_ERR, "S6.4 Clock declared as both IN_PINS and CLOCK_GEN output" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_PARAM_OUT_OF_RANGE",          0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN CONFIG parameter value outside valid chip-defined range" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_PARAM_TYPE_MISMATCH",        0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN CONFIG parameter type mismatch (integer parameter given decimal value)" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_DERIVED_OUT_OF_RANGE",        0, JZ_RULE_MODE_ERR, "S6.4.1 CLOCK_GEN derived value (frequency/VCO) outside valid chip-defined range" },
    { "CLOCK_GEN_RULES", "CLOCK_GEN_NO_CHIP_DATA",               0, JZ_RULE_MODE_INF, "S6.4.1 No chip specified; CLOCK_GEN parameters and constraints cannot be validated" },

    /* [BLACKBOX_RULES] */
    { "BLACKBOX_RULES", "BLACKBOX_BODY_DISALLOWED",                 0, JZ_RULE_MODE_ERR, "S6.7 Blackbox contains forbidden blocks (ASYNCHRONOUS/SYNCHRONOUS/WIRE/REGISTER/MEM)" },
    { "BLACKBOX_RULES", "BLACKBOX_UNDEFINED_IN_NEW",                0, JZ_RULE_MODE_ERR, "S6.7/S6.9 @new references undefined blackbox name" },
    { "BLACKBOX_RULES", "BLACKBOX_OVERRIDE_UNCHECKED",              0, JZ_RULE_MODE_INF, "S6.7/S4.13 OVERRIDE in blackbox instantiation is not validated and is passed through to vendor IP" },

    /* [TOP_LEVEL_INSTANTIATION] */
    { "TOP_LEVEL_INSTANTIATION", "TOP_PORT_NOT_LISTED",             0, JZ_RULE_MODE_ERR, "S6.9/S6.10 Top module port omitted from project-level @top block" },
    { "TOP_LEVEL_INSTANTIATION", "TOP_PORT_WIDTH_MISMATCH",         0, JZ_RULE_MODE_ERR, "S6.9/S6.10 Instantiated top port width does not match module port width" },
    { "TOP_LEVEL_INSTANTIATION", "TOP_PORT_PIN_DECL_MISSING",       0, JZ_RULE_MODE_ERR, "S6.9/S6.10 Connected top port has no corresponding IN_PINS/OUT_PINS/INOUT_PINS/CLOCKS declaration" },
    { "TOP_LEVEL_INSTANTIATION", "TOP_PORT_PIN_DIRECTION_MISMATCH", 0, JZ_RULE_MODE_ERR, "S6.9/S6.10 Module IN/OUT/INOUT direction incompatible with pin category" },
    { "TOP_LEVEL_INSTANTIATION", "TOP_OUT_LITERAL_BINDING",         0, JZ_RULE_MODE_ERR, "S6.9 OUT ports may not be bound to literal values in project-level @top" },
    { "TOP_LEVEL_INSTANTIATION", "TOP_NO_CONNECT_WITHOUT_WIDTH",    0, JZ_RULE_MODE_ERR, "S6.9 Port bound to `_` but missing explicit width in top-level @top list" },

    /* [MEM_DECLARATION] */
    { "MEM_DECLARATION", "MEM_UNDEFINED_NAME",                      0, JZ_RULE_MODE_ERR, "S7.7.1 Access to MEM name not declared in module" },
    { "MEM_DECLARATION", "MEM_DUP_NAME",                            0, JZ_RULE_MODE_ERR, "S7.1/S7.7.1 Two MEM blocks declared with same name in module" },
    { "MEM_DECLARATION", "MEM_INVALID_WORD_WIDTH",                  0, JZ_RULE_MODE_ERR, "S7.1/S7.7.1 Word width <= 0 or non-integer" },
    { "MEM_DECLARATION", "MEM_INVALID_DEPTH",                       0, JZ_RULE_MODE_ERR, "S7.1/S7.7.1 Depth <= 0 or non-integer" },
    { "MEM_DECLARATION", "MEM_UNDEFINED_CONST_IN_WIDTH",            0, JZ_RULE_MODE_ERR, "S7.1/S7.7.1 CONST in word_width/depth not declared" },
    { "MEM_DECLARATION", "MEM_INIT_LITERAL_OVERFLOW",               0, JZ_RULE_MODE_ERR, "S7.5.1/S7.7.1 Literal init value exceeds word width" },
    { "MEM_DECLARATION", "MEM_DUP_PORT_NAME",                       0, JZ_RULE_MODE_ERR, "S7.1/S7.7.1 Duplicate IN/OUT port name inside MEM block" },
    { "MEM_DECLARATION", "MEM_PORT_NAME_CONFLICT_MODULE_ID",        1, JZ_RULE_MODE_ERR, "S7.1/S7.7.1 MEM port name conflicts with module-level identifier" },
    { "MEM_DECLARATION", "MEM_EMPTY_PORT_LIST",                     0, JZ_RULE_MODE_ERR, "S7.1/S7.7.1 MEM declared with no IN, OUT, or INOUT ports" },
    { "MEM_DECLARATION", "MEM_INVALID_PORT_TYPE",                   1, JZ_RULE_MODE_ERR, "S7.1/S7.7.1 Invalid port type or qualifier (e.g. OUT ASYNC, unknown keyword)" },
    { "MEM_DECLARATION", "MEM_MISSING_INIT",                        0, JZ_RULE_MODE_ERR, "S7.5.1/S7.7.1/S7.7.3 MEM declared without required `= <init>` or `= @file(...)` initialization" },
    { "MEM_DECLARATION", "MEM_INIT_FILE_NOT_FOUND",                 0, JZ_RULE_MODE_ERR, "S7.5.2/S7.7.1 @file path does not resolve to readable file" },
    { "MEM_DECLARATION", "MEM_INIT_CONTAINS_X",                      0, JZ_RULE_MODE_ERR, "S2.1/S7.5.1 Memory initialization literal must not contain `x` bits" },
    { "MEM_DECLARATION", "MEM_INIT_FILE_CONTAINS_X",               0, JZ_RULE_MODE_ERR, "S7.5.2 Memory initialization file contains x or z values" },
    { "MEM_DECLARATION", "MEM_INIT_FILE_TOO_LARGE",                 0, JZ_RULE_MODE_ERR, "S7.5.2/S7.7.1 Initialization file larger than depth*word_width" },
    { "MEM_DECLARATION", "MEM_TYPE_INVALID",                        0, JZ_RULE_MODE_ERR, "S7.1 MEM TYPE value is not recognized; expected BLOCK or DISTRIBUTED" },
    { "MEM_DECLARATION", "MEM_TYPE_BLOCK_WITH_ASYNC_OUT",           1, JZ_RULE_MODE_ERR, "S7.1 Strict mode: MEM(TYPE=BLOCK) has IN port declared ASYNC" },
    { "MEM_DECLARATION", "MEM_CHIP_CONFIG_UNSUPPORTED",             0, JZ_RULE_MODE_ERR, "S7.1/S6.1 MEM configuration not supported by selected chip" },
    { "MEM_DECLARATION", "MEM_INOUT_MIXED_WITH_IN_OUT",             0, JZ_RULE_MODE_ERR, "S7.1 INOUT ports cannot be mixed with IN/OUT ports in the same MEM declaration" },
    { "MEM_DECLARATION", "MEM_INOUT_ASYNC",                         0, JZ_RULE_MODE_ERR, "S7.1 INOUT ports are always synchronous; ASYNC/SYNC keyword not permitted" },
    { "MEM_DECLARATION", "MEM_BLOCK_MULTI",                         0, JZ_RULE_MODE_INF, "S7.1 BLOCK memory requires multiple BSRAM blocks" },
    { "MEM_DECLARATION", "MEM_BLOCK_RESOURCE_EXCEEDED",             0, JZ_RULE_MODE_ERR, "S7.1/S6.1 Total BLOCK memory usage exceeds chip resources" },
    { "MEM_DECLARATION", "MEM_DISTRIBUTED_RESOURCE_EXCEEDED",       0, JZ_RULE_MODE_ERR, "S7.1/S6.1 Total DISTRIBUTED memory usage exceeds chip resources" },

    /* [MEM_ACCESS] */
    { "MEM_ACCESS", "MEM_PORT_UNDEFINED",                           0, JZ_RULE_MODE_ERR, "S7.2/S7.3/S7.7.2 Access to MEM port name not declared in MEM block" },
    { "MEM_ACCESS", "MEM_PORT_FIELD_UNDEFINED",                     0, JZ_RULE_MODE_ERR, "S7.2/S7.7.2 Invalid MEM port field; expected '.addr', '.data', or '.wdata'" },
    { "MEM_ACCESS", "MEM_SYNC_PORT_INDEXED",                        0, JZ_RULE_MODE_ERR, "S7.2.1/S7.3.2/S7.7.2 SYNC read port indexed; use mem.port.addr and mem.port.data" },
    { "MEM_ACCESS", "MEM_PORT_USED_AS_SIGNAL",                      0, JZ_RULE_MODE_ERR, "S7.2.1/S7.3.2/S7.7.2 MEM ports are not signals; use mem.port.addr or mem.port.data" },
    { "MEM_ACCESS", "MEM_PORT_ADDR_READ",                           0, JZ_RULE_MODE_ERR, "S7.2.1/S7.3.2/S7.7.2 MEM read address inputs are not readable; use mem.port.data" },
    { "MEM_ACCESS", "MEM_ASYNC_PORT_FIELD_DATA",                    0, JZ_RULE_MODE_ERR, "S7.2.1/S7.3.1/S7.7.2 ASYNC read ports must be indexed; mem.port.data is SYNC-only" },
    { "MEM_ACCESS", "MEM_SYNC_ADDR_INVALID_PORT",                   0, JZ_RULE_MODE_ERR, "S7.2.1/S7.3.2/S7.7.2 mem.port.addr is only valid for SYNC OUT ports" },
    { "MEM_ACCESS", "MEM_SYNC_ADDR_IN_ASYNC_BLOCK",                 0, JZ_RULE_MODE_ERR, "S7.2.1/S7.3.2/S7.7.2 SYNC read addresses must be assigned in SYNCHRONOUS blocks" },
    { "MEM_ACCESS", "MEM_SYNC_DATA_IN_ASYNC_BLOCK",                 0, JZ_RULE_MODE_ERR, "S7.2.1/S7.3.2/S7.7.2 SYNC MEM read data (mem.port.data) may not be read in ASYNCHRONOUS blocks" },
    { "MEM_ACCESS", "MEM_SYNC_ADDR_WITHOUT_RECEIVE",                0, JZ_RULE_MODE_ERR, "S7.2.1/S7.3.2 MEM read address must use `<=` in SYNCHRONOUS block; did you mean `<=` instead of `=`?" },
    { "MEM_ACCESS", "MEM_READ_SYNC_WITH_EQUALS",                    0, JZ_RULE_MODE_ERR, "S7.2.1/S7.3.2 Synchronous MEM read used `=` in SYNCHRONOUS block; did you mean `<=`?" },
    { "MEM_ACCESS", "MEM_IN_PORT_FIELD_ACCESS",                      0, JZ_RULE_MODE_ERR, "S7.2.2/S7.3.3 IN (write) port requires bracket syntax mem.port[addr] <= data; .addr/.data fields are not valid" },
    { "MEM_ACCESS", "MEM_WRITE_IN_ASYNC_BLOCK",                     2, JZ_RULE_MODE_ERR, "S7.2.2/S7.3.3 MEM writes must be in SYNCHRONOUS blocks; move this assignment out of ASYNCHRONOUS" },
    { "MEM_ACCESS", "MEM_WRITE_TO_READ_PORT",                       1, JZ_RULE_MODE_ERR, "S7.2.1/S7.3.2 Writing to MEM read port (IN); did you mean to use an OUT port?" },
    { "MEM_ACCESS", "MEM_READ_FROM_WRITE_PORT",                     1, JZ_RULE_MODE_ERR, "S7.2.2/S7.3.3 Reading from MEM write port (OUT); did you mean to use an IN port?" },
    { "MEM_ACCESS", "MEM_ADDR_WIDTH_TOO_WIDE",                      0, JZ_RULE_MODE_ERR, "S7.2/S7.3/S7.7.2 Address width exceeds ceil(log2(depth)) in constant-time-provable way" },
    { "MEM_ACCESS", "MEM_MULTIPLE_WRITES_SAME_IN",                  0, JZ_RULE_MODE_ERR, "S7.2.2/S7.3.3/S7.7.2 Multiple writes to same IN port within single SYNCHRONOUS block" },
    { "MEM_ACCESS", "MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT",            0, JZ_RULE_MODE_ERR, "S7.2.1/S7.3.2/S7.7.2 Synchronous read address sampled from multiple addresses in one execution path" },
    { "MEM_ACCESS", "MEM_CONST_ADDR_OUT_OF_RANGE",                  0, JZ_RULE_MODE_ERR, "S7.2/S7.3/S7.7.2 Constant address index >= depth" },
    { "MEM_ACCESS", "MEM_INVALID_WRITE_MODE",                       0, JZ_RULE_MODE_ERR, "S7.4/S7.7.2 WRITE_MODE not in [WRITE_FIRST|READ_FIRST|NO_CHANGE]" },
    { "MEM_ACCESS", "MEM_INOUT_INDEXED",                            0, JZ_RULE_MODE_ERR, "S7.2.3 INOUT port may not be indexed; use .addr/.data/.wdata pseudo-fields" },
    { "MEM_ACCESS", "MEM_INOUT_WDATA_IN_ASYNC",                     0, JZ_RULE_MODE_ERR, "S7.2.3 INOUT port .wdata may only be assigned in SYNCHRONOUS blocks" },
    { "MEM_ACCESS", "MEM_INOUT_ADDR_IN_ASYNC",                      0, JZ_RULE_MODE_ERR, "S7.2.3 INOUT port .addr may only be assigned in SYNCHRONOUS blocks" },
    { "MEM_ACCESS", "MEM_INOUT_WDATA_WRONG_OP",                     0, JZ_RULE_MODE_ERR, "S7.2.3 INOUT port .wdata must be assigned with '<=' operator in SYNCHRONOUS blocks" },
    { "MEM_ACCESS", "MEM_MULTIPLE_ADDR_ASSIGNS",                    0, JZ_RULE_MODE_ERR, "S7.2.3 Multiple assignments to INOUT .addr within same execution path" },
    { "MEM_ACCESS", "MEM_MULTIPLE_WDATA_ASSIGNS",                   0, JZ_RULE_MODE_ERR, "S7.2.3 Multiple assignments to INOUT .wdata within same execution path" },

    /* [MEM_WARNINGS] */
    { "MEM_WARNINGS", "MEM_WARN_PORT_NEVER_ACCESSED",               0, JZ_RULE_MODE_WRN, "S7.7.3 MEM IN/OUT/INOUT port declared but never used" },
    { "MEM_WARNINGS", "MEM_WARN_PARTIAL_INIT",                      0, JZ_RULE_MODE_WRN, "S7.5.2/S7.7.3 Initialization file smaller than depth (zero-padded)" },
    { "MEM_WARNINGS", "MEM_WARN_DEAD_CODE_ACCESS",                  0, JZ_RULE_MODE_WRN, "S7.7.3 Memory access appears only in unreachable code" },

    /* [TRISTATE_TRANSFORM] */
    { "TRISTATE_TRANSFORM", "TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL", 0, JZ_RULE_MODE_ERR, "S11.7 Tri-state drivers for same signal have non-mutually-exclusive enable conditions; cannot build safe priority chain" },
    { "TRISTATE_TRANSFORM", "TRISTATE_TRANSFORM_PER_BIT_FAIL",          0, JZ_RULE_MODE_ERR, "S11.7 Per-bit tri-state pattern detected; only full-width z assignments can be transformed" },
    { "TRISTATE_TRANSFORM", "TRISTATE_TRANSFORM_BLACKBOX_PORT",         0, JZ_RULE_MODE_ERR, "S11.7 Tri-state signal driven by blackbox port cannot be transformed; use external pull resistor" },
    { "TRISTATE_TRANSFORM", "TRISTATE_TRANSFORM_SINGLE_DRIVER",         0, JZ_RULE_MODE_WRN, "S11.7 Single-driver tri-state net transformed to default value; original z replaced with GND/VCC" },
    { "TRISTATE_TRANSFORM", "TRISTATE_TRANSFORM_OE_EXTRACT_FAIL",       0, JZ_RULE_MODE_ERR, "S11.7 Could not extract output-enable condition from tri-state port; _oe driven high as fallback" },
    { "TRISTATE_TRANSFORM", "TRISTATE_TRANSFORM_UNUSED_DEFAULT",        0, JZ_RULE_MODE_WRN, "S11.7 --tristate-default specified but no internal tri-state nets found to transform" },
    { "TRISTATE_TRANSFORM", "INFO_TRISTATE_TRANSFORM",                  0, JZ_RULE_MODE_INF, "S11 Tri-state net transformed by --tristate-default" },

    /* [SERIALIZER] */
    { "SERIALIZER", "INFO_SERIALIZER_CASCADE",                         0, JZ_RULE_MODE_INF, "Differential output uses cascaded serializers (master+slave) for extended serialization ratio" },
    { "SERIALIZER", "SERIALIZER_WIDTH_EXCEEDS_RATIO",                  0, JZ_RULE_MODE_ERR, "Differential output port width exceeds chip serializer ratio and cascade is not supported" },

    /* [TEMPLATE] */
    { "TEMPLATE", "TEMPLATE_UNDEFINED",              0, JZ_RULE_MODE_ERR, "S10.5 @apply references undefined template" },
    { "TEMPLATE", "TEMPLATE_ARG_COUNT_MISMATCH",     0, JZ_RULE_MODE_ERR, "S10.5 @apply argument count does not match template parameter count" },
    { "TEMPLATE", "TEMPLATE_COUNT_NOT_NONNEG_INT",   0, JZ_RULE_MODE_ERR, "S10.5 @apply count expression does not resolve to a non-negative integer" },
    { "TEMPLATE", "TEMPLATE_NESTED_DEF",             0, JZ_RULE_MODE_ERR, "S10.4 Nested @template definitions are not allowed" },
    { "TEMPLATE", "TEMPLATE_FORBIDDEN_DECL",         0, JZ_RULE_MODE_ERR, "S10.4 Declaration blocks (WIRE, REGISTER, etc.) are not allowed inside template body; use @scratch for temporary signals" },
    { "TEMPLATE", "TEMPLATE_FORBIDDEN_BLOCK_HEADER", 0, JZ_RULE_MODE_ERR, "S10.4 SYNCHRONOUS/ASYNCHRONOUS block headers are not allowed inside template body" },
    { "TEMPLATE", "TEMPLATE_FORBIDDEN_DIRECTIVE",    0, JZ_RULE_MODE_ERR, "S10.4 Structural directives (@new, @module, @feature, etc.) are not allowed inside template body" },
    { "TEMPLATE", "TEMPLATE_SCRATCH_OUTSIDE",        0, JZ_RULE_MODE_ERR, "S10.3 @scratch may only appear inside a @template body" },
    { "TEMPLATE", "TEMPLATE_APPLY_OUTSIDE_BLOCK",    0, JZ_RULE_MODE_ERR, "S10.5 @apply may only appear inside ASYNCHRONOUS or SYNCHRONOUS blocks" },
    { "TEMPLATE", "TEMPLATE_DUP_NAME",               0, JZ_RULE_MODE_ERR, "S10.2 Duplicate template name in the same scope" },
    { "TEMPLATE", "TEMPLATE_DUP_PARAM",              0, JZ_RULE_MODE_ERR, "S10.2 Duplicate parameter name in template definition" },
    { "TEMPLATE", "TEMPLATE_SCRATCH_WIDTH_INVALID",  0, JZ_RULE_MODE_ERR, "S10.3 @scratch width must be a positive integer constant expression" },
    { "TEMPLATE", "TEMPLATE_EXTERNAL_REF",           0, JZ_RULE_MODE_ERR, "S10.3 Identifier in template body must be a parameter, @scratch wire, or compile-time constant; pass external signals as arguments" },

    /* [TESTBENCH] */
    { "TESTBENCH", "TB_WRONG_TOOL",               0, JZ_RULE_MODE_ERR, "File contains @testbench blocks; use --test to run testbenches" },
    { "TESTBENCH", "TB_PROJECT_MIXED",            0, JZ_RULE_MODE_ERR, "TB-020 A file may not contain both @project and @testbench" },
    { "TESTBENCH", "TB_MODULE_NOT_FOUND",          0, JZ_RULE_MODE_ERR, "TB-001 @testbench module name must refer to a module in scope" },
    { "TESTBENCH", "TB_PORT_NOT_CONNECTED",        0, JZ_RULE_MODE_ERR, "TB-002 All module ports must be connected in @new" },
    { "TESTBENCH", "TB_PORT_WIDTH_MISMATCH",       0, JZ_RULE_MODE_ERR, "TB-003 Port width must match module declared width" },
    { "TESTBENCH", "TB_NEW_RHS_INVALID",           0, JZ_RULE_MODE_ERR, "TB-004 @new RHS must be a testbench CLOCK or WIRE" },
    { "TESTBENCH", "TB_SETUP_POSITION",            0, JZ_RULE_MODE_ERR, "TB-005 @setup must appear exactly once per TEST, after @new, before other directives" },
    { "TESTBENCH", "TB_CLOCK_NOT_DECLARED",        0, JZ_RULE_MODE_ERR, "TB-007 @clock clock identifier must refer to a declared CLOCK" },
    { "TESTBENCH", "TB_CLOCK_CYCLE_NOT_POSITIVE",  0, JZ_RULE_MODE_ERR, "TB-008 @clock cycle count must be a positive integer" },
    { "TESTBENCH", "TB_UPDATE_NOT_WIRE",           0, JZ_RULE_MODE_ERR, "TB-009 @update may only assign testbench WIRE identifiers" },
    { "TESTBENCH", "TB_UPDATE_CLOCK_ASSIGN",       0, JZ_RULE_MODE_ERR, "TB-010 @update may not assign clock signals" },
    { "TESTBENCH", "TB_EXPECT_WIDTH_MISMATCH",     0, JZ_RULE_MODE_ERR, "TB-011 @expect value width must match signal width" },
    { "TESTBENCH", "TB_NO_TEST_BLOCKS",            0, JZ_RULE_MODE_ERR, "TB-012 @testbench must contain at least one TEST block" },
    { "TESTBENCH", "TB_MULTIPLE_NEW",              0, JZ_RULE_MODE_ERR, "TB-013 Each TEST must contain exactly one @new instantiation" },

    /* [SIMULATION] */
    { "SIMULATION", "SIM_WRONG_TOOL",             0, JZ_RULE_MODE_ERR, "File contains @simulation blocks; use --simulate to run simulations" },
    { "SIMULATION", "SIM_PROJECT_MIXED",          0, JZ_RULE_MODE_ERR, "SIM-020 A file may not contain both @project and @simulation" },

    /* [SIMULATION_RUNTIME] */
    { "SIMULATION", "SIM_RUN_COND_TIMEOUT",         0, JZ_RULE_MODE_ERR, "SIM-030 @run_until/@run_while condition not met within timeout" },

    /* [REPEAT] */
    { "REPEAT", "RPT_COUNT_INVALID",              0, JZ_RULE_MODE_ERR, "RPT-001 @repeat requires a positive integer count" },
    { "REPEAT", "RPT_NO_MATCHING_END",            0, JZ_RULE_MODE_ERR, "RPT-002 @repeat without matching @end" },

    /* [GENERAL_WARNINGS] */
    { "GENERAL_WARNINGS", "WARN_UNUSED_REGISTER",                   0, JZ_RULE_MODE_WRN, "S8.3 Register is never read or written; remove it if unused" },
    { "GENERAL_WARNINGS", "WARN_UNSINKED_REGISTER",                0, JZ_RULE_MODE_WRN, "S8.3 Register is written but its value is never read" },
    { "GENERAL_WARNINGS", "WARN_UNDRIVEN_REGISTER",                0, JZ_RULE_MODE_WRN, "S8.3 Register is read but never written" },
    { "GENERAL_WARNINGS", "WARN_UNCONNECTED_OUTPUT",                2, JZ_RULE_MODE_WRN, "S8.3 Output port is unconnected; assign a value or use `_` for intentional no-connect" },
    { "GENERAL_WARNINGS", "WARN_INCOMPLETE_SELECT_ASYNC",           0, JZ_RULE_MODE_WRN, "S5.4/S8.3 Incomplete SELECT coverage without DEFAULT in ASYNCHRONOUS block" },
    { "GENERAL_WARNINGS", "WARN_DEAD_CODE_UNREACHABLE",             0, JZ_RULE_MODE_WRN, "S7.7.3/S8.3 Dead code (unreachable statements detected by analysis)" },
    { "GENERAL_WARNINGS", "WARN_UNUSED_MODULE",                     0, JZ_RULE_MODE_WRN, "S8.3 Module declared but never instantiated; remove it or add a @new" },
    { "GENERAL_WARNINGS", "WARN_UNUSED_WIRE",                      0, JZ_RULE_MODE_WRN, "S12.3 WIRE declared but never driven or read; remove it if unused" },
    { "GENERAL_WARNINGS", "WARN_UNUSED_PORT",                      0, JZ_RULE_MODE_WRN, "S12.3 PORT declared but never used; remove it if unused" },
    { "GENERAL_WARNINGS", "WARN_INTERNAL_TRISTATE",                0, JZ_RULE_MODE_WRN, "S11 Internal tri-state logic is not FPGA-compatible; use --tristate-default=GND or --tristate-default=VCC" },

    /* [IO] */
    { "IO", "IO_BACKEND",                                          0, JZ_RULE_MODE_ERR, "Failed to open or write backend output file" },
    { "IO", "IO_IR",                                               0, JZ_RULE_MODE_ERR, "Failed to write or finalize IR output file" },

    /* [PATH_SECURITY] */
    { "PATH_SECURITY", "PATH_ABSOLUTE_FORBIDDEN",                0, JZ_RULE_MODE_ERR, "S12.2 Absolute path used without --allow-absolute-paths" },
    { "PATH_SECURITY", "PATH_TRAVERSAL_FORBIDDEN",               0, JZ_RULE_MODE_ERR, "S12.2 Path contains '..' traversal without --allow-traversal" },
    { "PATH_SECURITY", "PATH_OUTSIDE_SANDBOX",                   0, JZ_RULE_MODE_ERR, "S12.2 Resolved path falls outside all permitted sandbox roots" },
    { "PATH_SECURITY", "PATH_SYMLINK_ESCAPE",                    0, JZ_RULE_MODE_ERR, "S12.2 Symbolic link resolves to target outside sandbox root" },
};

const size_t jz_rule_table_count = sizeof(jz_rule_table) / sizeof(jz_rule_table[0]);

const JZRuleInfo *jz_rule_lookup(const char *id)
{
    if (!id) return NULL;
    for (size_t i = 0; i < jz_rule_table_count; ++i) {
        if (strcmp(jz_rule_table[i].id, id) == 0) {
            return &jz_rule_table[i];
        }
    }
    return NULL;
}

static const char *mode_to_string(JZRuleMode mode)
{
    switch (mode) {
    case JZ_RULE_MODE_ERR: return "ERROR";
    case JZ_RULE_MODE_WRN: return "WARN";
    case JZ_RULE_MODE_INF: return "INFO";
    default:               return "UNKNOWN";
    }
}

void jz_rules_print_all(FILE *out)
{
    if (!out) {
        out = stdout;
    }

    const char *current_group = NULL;

    for (size_t i = 0; i < jz_rule_table_count; ++i) {
        const JZRuleInfo *rule = &jz_rule_table[i];
        if (!rule->group) {
            continue;
        }

        if (!current_group || strcmp(current_group, rule->group) != 0) {
            /* Start a new group with a blank line (except before the first). */
            if (current_group != NULL) {
                fputc('\n', out);
            }
            fprintf(out, "[%s]\n", rule->group);
            current_group = rule->group;
        }

        const char *mode_str = mode_to_string(rule->mode);
        fprintf(out, "  %-5s %-32s %s\n",
                mode_str,
                rule->id ? rule->id : "",
                rule->description ? rule->description : "");
    }
}
