---
title: "--tristate-default Compiler Flag"
lang: en-US

layout: doc
outline: deep
---

# `--tristate-default` Compiler Flag

## Purpose and Overview

The `--tristate-default` flag converts **internal tri-state nets** into explicit priority-chained conditional logic, eliminating high-impedance (`z`) states for synthesis targets that do not support internal tri-state (such as most FPGA implementations).

This flag enables a single JZ-HDL design to function on both ASIC platforms (which support tri-state) and FPGA platforms (which do not), without requiring design changes.

**Syntax:**
```bash
jz-hdl --tristate-default=GND   design.jz   # Replace z with 0 (GND)
jz-hdl --tristate-default=VCC   design.jz   # Replace z with 1 (VCC)
jz-hdl                          design.jz   # Default: allow z (ASIC/Sim mode)
```

**Effect:**
- When `--tristate-default=GND` or `--tristate-default=VCC` is specified:
  - All tri-state nets (nets with multiple drivers, at least one assigning `z`) are identified.
  - Each tri-state net is transformed into a priority-chained mux with a default value (`0` or `1`).
  - External INOUT_PINS remain unchanged (their tri-state behavior is preserved for I/O pads).
- When the flag is omitted (default):
  - Tri-state nets are permitted; no transformation occurs.
  - Behavior is consistent with ASIC or simulation semantics.

## Applicability and Scope

### Applicable Contexts

The `--tristate-default` flag transformation applies to:

- **ASYNCHRONOUS blocks only** (combinational tri-state nets).
- Tri-state nets driven by multiple drivers, where at least one driver assigns `z` and the others assign `0`, `1`, or expressions.
- Internal nets: `WIRE`, `PORT`, and implicit nets created by aliasing.

### Non-Applicable Contexts

The transformation does **not** apply to:

- **SYNCHRONOUS blocks** (registers and latches cannot hold or produce `z`; tri-state in synchronous context is invalid per the core concepts specification).
- **INOUT ports on the top-level module** when mapped to INOUT_PINS (tri-state is preserved as an I/O pad property).
- **External blackbox ports** (transformation is not applied to opaque module interfaces).
- **Single-driver nets** (nets with only one driver are not tri-state, regardless of whether that driver assigns `z`).

## Tri-State Net Identification

A net is classified as tri-state when all three criteria are met:

1. The net has **two or more drivers** in the same ASYNCHRONOUS block.
2. **At least one driver** assigns `z` (high-impedance) in some execution path.
3. The net is **not excluded** from transformation (i.e., it is not a top-level INOUT pin or blackbox port).

## Transformation Algorithm

### Priority-Chain Conversion

Once a tri-state net is identified, it is converted to a **priority-chained ternary cascade** using the following transformation:

**Original (tri-state):**
```text
signal <=  cond₀ ? data₀ : N'bz;
signal <=  cond₁ ? data₁ : N'bz;
signal <=  cond₂ ? data₂ : N'bz;
```

**Transformed (with --tristate-default=GND):**
```text
signal <=  cond₀ ? data₀ :
           cond₁ ? data₁ :
           cond₂ ? data₂ :
           N'h00;
```

**Transformed (with --tristate-default=VCC):**
```text
signal <=  cond₀ ? data₀ :
           cond₁ ? data₁ :
           cond₂ ? data₂ :
           N'hFF;
```

**Semantics:**

- **Source order determines priority:** Drivers are chained in the order they appear in the source code; the first driver to assert its condition wins.
- **Default on all `z`:** If no condition is true, the net assumes the default value (`N'h00` for GND, `N'hFF` for VCC).
- **Behavioral equivalence:** If conditions are mutually exclusive and exactly one is true in every cycle, the transformed logic produces the same result as the original tri-state.

### Whole-Signal Transformation Policy

The transformation is applied **whole-signal**, not per-bit.

**Rationale:**

- Simpler to reason about and implement.
- Cleaner generated logic (single mux chain vs. per-bit decomposition).
- Reduces edge cases and compiler complexity.

**Constraint:** If individual bits of the same signal have **different sets of drivers**, a compile-time error is issued:

```
ERROR: TRISTATE_TRANSFORM_PER_BIT_FAIL
Cannot transform tri-state net 'signal': drivers are not uniform across all bits.
Bits [7:4] driven by {driver_a, driver_b}; bits [3:0] driven by {driver_c}.
Per-bit tri-state transformation is not supported.
```

If such a situation is encountered, the user must manually refactor to ensure all bits of the signal have the same driver set.

## Validation Rules

After transformation, the generated priority-chain mux is verified to:

1. **Maintain Width:** Result width matches original signal width.
2. **Valid Ternary Nesting:** All operands and branch widths are compatible.
3. **No Side Effects:** Conditions are pure (no assignment side effects).
4. **No New Loops:** The priority chain does not introduce new combinational loops.

If any post-transformation check fails, the transformation is **rolled back**, and a compile error is issued.

## Handling of INOUT Ports and External Pins

**INOUT ports on the top-level module** that are mapped to `INOUT_PINS` in the project retain their full tri-state semantics, **regardless of the `--tristate-default` flag**.

**Rationale:** INOUT_PINS represent physical I/O pads, which are inherently tri-state hardware. The flag only affects internal tri-state nets.

**Example:**
```text
// INOUT port on top module
PORT {
  INOUT [8] data_bus;
}

// Mapped to INOUT_PIN in project
INOUT_PINS {
  data_bus [8] = { standard=LVCMOS33, drive=8 };
}

// In ASYNCHRONOUS block: tri-state is NOT transformed
ASYNCHRONOUS {
  data_bus <= enable ? output_data : 8'bzzzz_zzzz;
  // Remains tri-state; flag does not apply
}
```

### Driving INOUT with Internal Tri-State

If an internal tri-state net is **connected to** an INOUT port, the transformation applies to the internal net, but the connection to the INOUT port remains valid.

**Example:**
```text
WIRE {
  internal_mux [8];
}

PORT {
  INOUT [8] bus;
}

ASYNCHRONOUS {
  // internal_mux is tri-state; will be transformed
  internal_mux <= cond_a ? data_a : 8'bz;
  internal_mux <= cond_b ? data_b : 8'bz;

  // Connect to INOUT (tri-state preserved for I/O pad)
  bus <= enable ? internal_mux : 8'bzzzz_zzzz;
}
```

With `--tristate-default=GND`:
```text
ASYNCHRONOUS {
  internal_mux <= cond_a ? data_a :
                  cond_b ? data_b :
                  GND;  // Transformed

  bus <= enable ? internal_mux : 8'bzzzz_zzzz;  // Unchanged
}
```

## Error Conditions and Warnings

### Transformation Errors

A compile error is issued if:

| Error | Cause | Mitigation |
| :--- | :--- | :--- |
| **TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL** | Conditions are not provably mutually exclusive | Refactor to use explicit `IF`/`ELIF`/`ELSE` or add explicit guards to ensure exclusion |
| **TRISTATE_TRANSFORM_PER_BIT_FAIL** | Different bits have different driver sets | Restructure so all bits share the same drivers |
| **TRISTATE_TRANSFORM_BLACKBOX_PORT** | Attempting to transform external blackbox port | Tri-state in opaque modules is preserved as-is |
| **TRISTATE_TRANSFORM_OE_EXTRACT_FAIL** | Cannot extract output-enable condition from a driver | Refactor driver to use a clear `condition ? data : z` pattern |

### Warnings

A warning is issued in these cases (transformation proceeds unless conversion is explicitly forbidden):

| Warning | Cause | Recommendation |
| :--- | :--- | :--- |
| **TRISTATE_TRANSFORM_SINGLE_DRIVER** | Net marked as tri-state but only one driver present in execution | Remove unnecessary `z` assignment; net is not actually tri-state |
| **TRISTATE_TRANSFORM_UNUSED_DEFAULT** | All execution paths covered; default value never used | Consider the transformation unnecessary; default is unreachable |

## Portability Guidelines

### Best Practices for Portable Designs

To write JZ-HDL designs that work with and without `--tristate-default`:

1. **Use explicit guards:** Prefer `IF`/`ELIF`/`ELSE` over independent conditional assignments.
   ```text
   // Good: explicit structure
   IF (sel == 2'b00) {
     data <= rom_data;
   } ELIF (sel == 2'b01) {
     data <= ram_data;
   } ELSE {
     data <= 8'h00;  // Explicit default
   }

   // Less ideal: separate assignments (harder to transform)
   data <= (sel == 2'b00) ? rom_data : 8'bz;
   data <= (sel == 2'b01) ? ram_data : 8'bz;
   ```

2. **Limit tri-state to I/O boundaries:** Keep tri-state logic at top-level INOUT ports; use internal muxes elsewhere.

3. **Test both modes:** Verify design behavior with and without the flag.
