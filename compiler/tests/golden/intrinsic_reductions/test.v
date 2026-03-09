// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module ReductionCore (
    clk,
    rst_n,
    data,
    r_and,
    r_or,
    r_xor
);
    // Ports
    input clk;
    input rst_n;
    input [7:0] data;
    output reg r_and;
    output reg r_or;
    output reg r_xor;

    // Signals



    always @* begin
        r_and = &(data);
        r_or = |(data);
        r_xor = ^(data);
    end
endmodule

module top (
    SCLK,
    DONE,
    data,
    r_and,
    r_or,
    r_xor
);
    input SCLK;
    input DONE;
    input [7:0] data;
    output r_and;
    output r_or;
    output r_xor;

    // Top-level logical→physical pin mapping
    //   ReductionCore.clk -> SCLK (board 4)
    //   ReductionCore.rst_n -> DONE (board IOR32B)
    //   ReductionCore.data[7] -> data[7] (board 17)
    //   ReductionCore.data[6] -> data[6] (board 16)
    //   ReductionCore.data[5] -> data[5] (board 15)
    //   ReductionCore.data[4] -> data[4] (board 14)
    //   ReductionCore.data[3] -> data[3] (board 13)
    //   ReductionCore.data[2] -> data[2] (board 12)
    //   ReductionCore.data[1] -> data[1] (board 11)
    //   ReductionCore.data[0] -> data[0] (board 10)
    //   ReductionCore.r_and -> r_and[0] (board 30)
    //   ReductionCore.r_or -> r_or[0] (board 31)
    //   ReductionCore.r_xor -> r_xor[0] (board 32)



    ReductionCore u_top (
        .clk(SCLK),
        .rst_n(DONE),
        .data({data[7], data[6], data[5], data[4], data[3], data[2], data[1], data[0]}),
        .r_and(r_and[0]),
        .r_or(r_or[0]),
        .r_xor(r_xor[0])
    );
endmodule
