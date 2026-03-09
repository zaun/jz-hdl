// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module configurable_unit__SPEC_DEPTH_8_WIDTH_16 (
    data_in,
    data_out
);
    // Ports
    input [15:0] data_in;
    output reg [15:0] data_out;

    // Signals



    always @* begin
        data_out = data_in;
    end
endmodule

module configurable_unit__SPEC_DEPTH_5_WIDTH_10 (
    data_in,
    data_out
);
    // Ports
    input [9:0] data_in;
    output reg [9:0] data_out;

    // Signals



    always @* begin
        data_out = data_in;
    end
endmodule

module configurable_unit__SPEC_DEPTH_6_WIDTH_12 (
    data_in,
    data_out
);
    // Ports
    input [11:0] data_in;
    output reg [11:0] data_out;

    // Signals



    always @* begin
        data_out = data_in;
    end
endmodule

module override_test (
    d_in,
    out_a,
    out_b,
    out_c
);
    // Ports
    input [15:0] d_in;
    output [15:0] out_a;
    output [9:0] out_b;
    output [11:0] out_c;

    // Signals


    configurable_unit__SPEC_DEPTH_8_WIDTH_16 inst_literal (
        .data_in(d_in),
        .data_out(out_a)
    );
    configurable_unit__SPEC_DEPTH_5_WIDTH_10 inst_const (
        .data_in(d_in[9:0]),
        .data_out(out_b)
    );
    configurable_unit__SPEC_DEPTH_6_WIDTH_12 inst_config (
        .data_in(d_in[11:0]),
        .data_out(out_c)
    );
endmodule

module top (
    d_in,
    out_a,
    out_b,
    out_c
);
    input [15:0] d_in;
    output [15:0] out_a;
    output [9:0] out_b;
    output [11:0] out_c;

    // Top-level logical→physical pin mapping
    //   override_test.d_in[15] -> d_in[15] (board 17)
    //   override_test.d_in[14] -> d_in[14] (board 16)
    //   override_test.d_in[13] -> d_in[13] (board 15)
    //   override_test.d_in[12] -> d_in[12] (board 14)
    //   override_test.d_in[11] -> d_in[11] (board 13)
    //   override_test.d_in[10] -> d_in[10] (board 12)
    //   override_test.d_in[9] -> d_in[9] (board 11)
    //   override_test.d_in[8] -> d_in[8] (board 10)
    //   override_test.d_in[7] -> d_in[7] (board 9)
    //   override_test.d_in[6] -> d_in[6] (board 8)
    //   override_test.d_in[5] -> d_in[5] (board 7)
    //   override_test.d_in[4] -> d_in[4] (board 6)
    //   override_test.d_in[3] -> d_in[3] (board 5)
    //   override_test.d_in[2] -> d_in[2] (board 4)
    //   override_test.d_in[1] -> d_in[1] (board 3)
    //   override_test.d_in[0] -> d_in[0] (board 2)
    //   override_test.out_a[15] -> out_a[15] (board 35)
    //   override_test.out_a[14] -> out_a[14] (board 34)
    //   override_test.out_a[13] -> out_a[13] (board 33)
    //   override_test.out_a[12] -> out_a[12] (board 32)
    //   override_test.out_a[11] -> out_a[11] (board 31)
    //   override_test.out_a[10] -> out_a[10] (board 30)
    //   override_test.out_a[9] -> out_a[9] (board 29)
    //   override_test.out_a[8] -> out_a[8] (board 28)
    //   override_test.out_a[7] -> out_a[7] (board 27)
    //   override_test.out_a[6] -> out_a[6] (board 26)
    //   override_test.out_a[5] -> out_a[5] (board 25)
    //   override_test.out_a[4] -> out_a[4] (board 24)
    //   override_test.out_a[3] -> out_a[3] (board 23)
    //   override_test.out_a[2] -> out_a[2] (board 22)
    //   override_test.out_a[1] -> out_a[1] (board 21)
    //   override_test.out_a[0] -> out_a[0] (board 20)
    //   override_test.out_b[9] -> out_b[9] (board 49)
    //   override_test.out_b[8] -> out_b[8] (board 48)
    //   override_test.out_b[7] -> out_b[7] (board 47)
    //   override_test.out_b[6] -> out_b[6] (board 46)
    //   override_test.out_b[5] -> out_b[5] (board 45)
    //   override_test.out_b[4] -> out_b[4] (board 44)
    //   override_test.out_b[3] -> out_b[3] (board 43)
    //   override_test.out_b[2] -> out_b[2] (board 42)
    //   override_test.out_b[1] -> out_b[1] (board 41)
    //   override_test.out_b[0] -> out_b[0] (board 40)
    //   override_test.out_c[11] -> out_c[11] (board 71)
    //   override_test.out_c[10] -> out_c[10] (board 70)
    //   override_test.out_c[9] -> out_c[9] (board 69)
    //   override_test.out_c[8] -> out_c[8] (board 68)
    //   override_test.out_c[7] -> out_c[7] (board 67)
    //   override_test.out_c[6] -> out_c[6] (board 66)
    //   override_test.out_c[5] -> out_c[5] (board 65)
    //   override_test.out_c[4] -> out_c[4] (board 64)
    //   override_test.out_c[3] -> out_c[3] (board 63)
    //   override_test.out_c[2] -> out_c[2] (board 62)
    //   override_test.out_c[1] -> out_c[1] (board 61)
    //   override_test.out_c[0] -> out_c[0] (board 60)



    override_test u_top (
        .d_in({d_in[15], d_in[14], d_in[13], d_in[12], d_in[11], d_in[10], d_in[9], d_in[8], d_in[7], d_in[6], d_in[5], d_in[4], d_in[3], d_in[2], d_in[1], d_in[0]}),
        .out_a({out_a[15], out_a[14], out_a[13], out_a[12], out_a[11], out_a[10], out_a[9], out_a[8], out_a[7], out_a[6], out_a[5], out_a[4], out_a[3], out_a[2], out_a[1], out_a[0]}),
        .out_b({out_b[9], out_b[8], out_b[7], out_b[6], out_b[5], out_b[4], out_b[3], out_b[2], out_b[1], out_b[0]}),
        .out_c({out_c[11], out_c[10], out_c[9], out_c[8], out_c[7], out_c[6], out_c[5], out_c[4], out_c[3], out_c[2], out_c[1], out_c[0]})
    );
endmodule
