---
title: Language Examples
lang: en-US

layout: doc
outline: deep
---

# Language Examples

Complete, self-contained JZ-HDL examples demonstrating core language features.

## Simple 1-Bit Register

```jz
@module flipflop
  PORT {
    IN  [1] d;
    OUT [1] q;
    IN  [1] clk;
  }

  REGISTER {
    state [1] = 1'b0;
  }

  ASYNCHRONOUS {
    q = state;
  }

  SYNCHRONOUS(CLK=clk) {
    state <= d;
  }
@endmod
```

## Bus Slice and Synchronous Update

```jz
@module slice_example
  CONST { W = 8; }

  PORT {
    IN  [16] inbus;
    OUT [8] out;
    IN  [1] clk;
  }

  REGISTER {
    r [8] = 8'h00;
  }

  ASYNCHRONOUS {
    out = r;
  }

  SYNCHRONOUS(CLK=clk) {
    r <= inbus[15:8];
  }
@endmod
```

## Module Instantiation

```jz
@module top
  PORT {
    IN  [8] a;
    IN  [8] b;
    OUT [9] sum;
  }

  @new adder_inst adder_module {
    IN  [8] a = a;
    IN  [8] b = b;
    OUT [9] sum = sum;
  }
@endmod
```

## Tri-State / Bidirectional Port

```jz
@module tristate_buffer
  PORT {
    IN    [8] data_in;
    IN    [1] enable;
    INOUT [8] data_bus;
  }

  ASYNCHRONOUS {
    data_bus = enable ? data_in : 8'bzzzz_zzzz;
  }
@endmod
```

## Counter with Load and Reset

```jz
@module counter
  PORT {
    IN  [1] clk;
    IN  [1] reset;
    IN  [1] load;
    IN  [8] load_value;
    OUT [16] count_wide;
  }

  REGISTER {
    counter_reg [16] = 16'h0000;
  }

  ASYNCHRONOUS {
    // Receive with explicit zero-extend (8 → 16)
    count_wide <=z load_value;
  }

  SYNCHRONOUS(
    CLK=clk
    RESET=reset
    RESET_ACTIVE=High
  ) {
    IF (load) {
      // Explicit zero-extend 8 → 16 into counter
      counter_reg <=z load_value;
    } ELSE {
      counter_reg <= counter_reg + 1;
    }
  }
@endmod
```

## ALU with SELECT / CASE

```jz
@module cpu_alu
  CONST {
    XLEN = 32;
  }

  PORT {
    IN  [XLEN] operand_a;
    IN  [XLEN] operand_b;
    IN  [4] control;
    OUT [XLEN] result;
    OUT [1] zero;
  }

  WIRE {
    alu_result [XLEN];
  }

  ASYNCHRONOUS {
    SELECT (control) {
      CASE 4'h0 {
        alu_result = operand_a + operand_b;
      }
      CASE 4'h1 {
        alu_result = operand_a - operand_b;
      }
      CASE 4'h2 {
        alu_result = operand_a & operand_b;
      }
      CASE 4'h3 {
        alu_result = operand_a | operand_b;
      }
      CASE 4'h4 {
        alu_result = operand_a ^ operand_b;
      }
      DEFAULT {
        alu_result = 32'h0000_0000;
      }
    }

    result = alu_result;
    zero = (alu_result == 32'h0000_0000) ? 1'b1 : 1'b0;
  }
@endmod
```

## Arithmetic with Carry Capture

```jz
@module adder_with_carry
  CONST { WIDTH = 8; }

  PORT {
    IN  [WIDTH] a;
    IN  [WIDTH] b;
    OUT [WIDTH] sum;
    OUT [1] carry;
    IN  [1] clk;
  }

  REGISTER {
    result [WIDTH + 1] = {1'b0, WIDTH'd0};
  }

  ASYNCHRONOUS {
    sum = result[WIDTH - 1:0];
    carry = result[WIDTH];
  }

  SYNCHRONOUS(CLK=clk) {
    result <= uadd(a, b);
  }
@endmod
```

## Sign-Extend in SYNCHRONOUS Assignment

```jz
@module sign_extend_example
  PORT {
    IN  [8] input_byte;
    OUT [16] extended_output;
    IN  [1] clk;
  }

  REGISTER {
    extended_reg [16] = 16'h0000;
  }

  ASYNCHRONOUS {
    extended_output = extended_reg;
  }

  SYNCHRONOUS(CLK=clk) {
    // Explicit sign-extend 8 → 16 bits
    extended_reg <=s input_byte;
  }
@endmod
```

## Sliced Register Updates

```jz
@module sliced_register_example
  PORT {
    IN  [1] clk;
    IN  [4] nibble_a;
    IN  [4] nibble_b;
    OUT [8] result;
  }

  REGISTER {
    data [8] = 8'h00;
  }

  ASYNCHRONOUS {
    result = data;
  }

  SYNCHRONOUS(CLK=clk) {
    // Update different nibbles without affecting each other
    data[7:4] <= nibble_a;
    data[3:0] <= nibble_b;
    // Each nibble assigned once; non-overlapping ranges
  }
@endmod
```

## Tri-State Transceiver with Read/Write Control

**Behavior:**
- `rw = 1`: Internal driver sends `buffer` onto `data`; `buffer` captures (echoes own write)
- `rw = 0`: Release bus (`z`); external drivers control `data`; `buffer` captures external data

```jz
@module tristate_transceiver
  PORT {
    IN    [1] clk;
    IN    [1] rw;           // 1 = drive, 0 = release
    INOUT [8] data;
  }

  REGISTER {
    buffer [8] = 8'h00;
  }

  ASYNCHRONOUS {
    data = rw ? buffer : 8'bzzzz_zzzz;
  }

  SYNCHRONOUS(CLK=clk) {
    buffer <= data;
  }
@endmod
```
