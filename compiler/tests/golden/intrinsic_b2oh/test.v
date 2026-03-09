// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module B2ohCore (
    clk,
    rst_n,
    idx3,
    idx2,
    oh8,
    oh4
);
    // Ports
    input clk;
    input rst_n;
    input [2:0] idx3;
    input [1:0] idx2;
    output reg [7:0] oh8;
    output reg [3:0] oh4;

    // Signals



    always @* begin
        oh8 = ((idx3 < 8'd8) ? (8'b1 << idx3) : 8'b0);
        oh4 = ((idx2 < 4'd4) ? (4'b1 << idx2) : 4'b0);
    end
endmodule

module top (
    SCLK,
    DONE,
    idx3,
    idx2,
    oh8,
    oh4
);
    input SCLK;
    input DONE;
    input [2:0] idx3;
    input [1:0] idx2;
    output [7:0] oh8;
    output [3:0] oh4;

    // Top-level logical→physical pin mapping
    //   B2ohCore.clk -> SCLK (board 4)
    //   B2ohCore.rst_n -> DONE (board IOR32B)
    //   B2ohCore.idx3[2] -> idx3[2] (board 12)
    //   B2ohCore.idx3[1] -> idx3[1] (board 11)
    //   B2ohCore.idx3[0] -> idx3[0] (board 10)
    //   B2ohCore.idx2[1] -> idx2[1] (board 14)
    //   B2ohCore.idx2[0] -> idx2[0] (board 13)
    //   B2ohCore.oh8[7] -> oh8[7] (board 37)
    //   B2ohCore.oh8[6] -> oh8[6] (board 36)
    //   B2ohCore.oh8[5] -> oh8[5] (board 35)
    //   B2ohCore.oh8[4] -> oh8[4] (board 34)
    //   B2ohCore.oh8[3] -> oh8[3] (board 33)
    //   B2ohCore.oh8[2] -> oh8[2] (board 32)
    //   B2ohCore.oh8[1] -> oh8[1] (board 31)
    //   B2ohCore.oh8[0] -> oh8[0] (board 30)
    //   B2ohCore.oh4[3] -> oh4[3] (board 41)
    //   B2ohCore.oh4[2] -> oh4[2] (board 40)
    //   B2ohCore.oh4[1] -> oh4[1] (board 39)
    //   B2ohCore.oh4[0] -> oh4[0] (board 38)



    B2ohCore u_top (
        .clk(SCLK),
        .rst_n(DONE),
        .idx3({idx3[2], idx3[1], idx3[0]}),
        .idx2({idx2[1], idx2[0]}),
        .oh8({oh8[7], oh8[6], oh8[5], oh8[4], oh8[3], oh8[2], oh8[1], oh8[0]}),
        .oh4({oh4[3], oh4[2], oh4[1], oh4[0]})
    );
endmodule
