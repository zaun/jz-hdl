// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module PriencLzcCore (
    clk,
    rst_n,
    src8,
    pri8,
    lzc8
);
    // Ports
    input clk;
    input rst_n;
    input [7:0] src8;
    output reg [2:0] pri8;
    output reg [3:0] lzc8;

    // Signals



    always @* begin
        pri8 = ((src8 >> 7) & 1'b1) ? 3'd7 : ((src8 >> 6) & 1'b1) ? 3'd6 : ((src8 >> 5) & 1'b1) ? 3'd5 : ((src8 >> 4) & 1'b1) ? 3'd4 : ((src8 >> 3) & 1'b1) ? 3'd3 : ((src8 >> 2) & 1'b1) ? 3'd2 : ((src8 >> 1) & 1'b1) ? 3'd1 : 3'd0;
        lzc8 = ((src8 >> 7) & 1'b1) ? 4'd0 : ((src8 >> 6) & 1'b1) ? 4'd1 : ((src8 >> 5) & 1'b1) ? 4'd2 : ((src8 >> 4) & 1'b1) ? 4'd3 : ((src8 >> 3) & 1'b1) ? 4'd4 : ((src8 >> 2) & 1'b1) ? 4'd5 : ((src8 >> 1) & 1'b1) ? 4'd6 : ((src8 >> 0) & 1'b1) ? 4'd7 : 4'd8;
    end
endmodule

module top (
    SCLK,
    DONE,
    src8,
    pri8,
    lzc8
);
    input SCLK;
    input DONE;
    input [7:0] src8;
    output [2:0] pri8;
    output [3:0] lzc8;

    // Top-level logical→physical pin mapping
    //   PriencLzcCore.clk -> SCLK (board 4)
    //   PriencLzcCore.rst_n -> DONE (board IOR32B)
    //   PriencLzcCore.src8[7] -> src8[7] (board 17)
    //   PriencLzcCore.src8[6] -> src8[6] (board 16)
    //   PriencLzcCore.src8[5] -> src8[5] (board 15)
    //   PriencLzcCore.src8[4] -> src8[4] (board 14)
    //   PriencLzcCore.src8[3] -> src8[3] (board 13)
    //   PriencLzcCore.src8[2] -> src8[2] (board 12)
    //   PriencLzcCore.src8[1] -> src8[1] (board 11)
    //   PriencLzcCore.src8[0] -> src8[0] (board 10)
    //   PriencLzcCore.pri8[2] -> pri8[2] (board 32)
    //   PriencLzcCore.pri8[1] -> pri8[1] (board 31)
    //   PriencLzcCore.pri8[0] -> pri8[0] (board 30)
    //   PriencLzcCore.lzc8[3] -> lzc8[3] (board 36)
    //   PriencLzcCore.lzc8[2] -> lzc8[2] (board 35)
    //   PriencLzcCore.lzc8[1] -> lzc8[1] (board 34)
    //   PriencLzcCore.lzc8[0] -> lzc8[0] (board 33)



    PriencLzcCore u_top (
        .clk(SCLK),
        .rst_n(DONE),
        .src8({src8[7], src8[6], src8[5], src8[4], src8[3], src8[2], src8[1], src8[0]}),
        .pri8({pri8[2], pri8[1], pri8[0]}),
        .lzc8({lzc8[3], lzc8[2], lzc8[1], lzc8[0]})
    );
endmodule
