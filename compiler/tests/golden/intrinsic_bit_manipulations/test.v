// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module BitManipCore (
    clk,
    rst_n,
    data8,
    data16,
    pop8,
    rev8,
    swap16
);
    // Ports
    input clk;
    input rst_n;
    input [7:0] data8;
    input [15:0] data16;
    output reg [3:0] pop8;
    output reg [7:0] rev8;
    output reg [15:0] swap16;

    // Signals



    always @* begin
        pop8 = ((data8 & 1'b1) + ((data8 >> 1) & 1'b1) + ((data8 >> 2) & 1'b1) + ((data8 >> 3) & 1'b1) + ((data8 >> 4) & 1'b1) + ((data8 >> 5) & 1'b1) + ((data8 >> 6) & 1'b1) + ((data8 >> 7) & 1'b1));
        rev8 = {((data8 >> 0) & 1'b1), ((data8 >> 1) & 1'b1), ((data8 >> 2) & 1'b1), ((data8 >> 3) & 1'b1), ((data8 >> 4) & 1'b1), ((data8 >> 5) & 1'b1), ((data8 >> 6) & 1'b1), ((data8 >> 7) & 1'b1)};
        swap16 = {((data16 >> 0) & 8'hFF), ((data16 >> 8) & 8'hFF)};
    end
endmodule

module top (
    SCLK,
    DONE,
    data8,
    data16,
    pop8,
    rev8,
    swap16
);
    input SCLK;
    input DONE;
    input [7:0] data8;
    input [15:0] data16;
    output [3:0] pop8;
    output [7:0] rev8;
    output [15:0] swap16;

    // Top-level logical→physical pin mapping
    //   BitManipCore.clk -> SCLK (board 4)
    //   BitManipCore.rst_n -> DONE (board IOR32B)
    //   BitManipCore.data8[7] -> data8[7] (board 17)
    //   BitManipCore.data8[6] -> data8[6] (board 16)
    //   BitManipCore.data8[5] -> data8[5] (board 15)
    //   BitManipCore.data8[4] -> data8[4] (board 14)
    //   BitManipCore.data8[3] -> data8[3] (board 13)
    //   BitManipCore.data8[2] -> data8[2] (board 12)
    //   BitManipCore.data8[1] -> data8[1] (board 11)
    //   BitManipCore.data8[0] -> data8[0] (board 10)
    //   BitManipCore.data16[15] -> data16[15] (board 53)
    //   BitManipCore.data16[14] -> data16[14] (board 52)
    //   BitManipCore.data16[13] -> data16[13] (board 51)
    //   BitManipCore.data16[12] -> data16[12] (board 50)
    //   BitManipCore.data16[11] -> data16[11] (board 49)
    //   BitManipCore.data16[10] -> data16[10] (board 48)
    //   BitManipCore.data16[9] -> data16[9] (board 47)
    //   BitManipCore.data16[8] -> data16[8] (board 46)
    //   BitManipCore.data16[7] -> data16[7] (board 45)
    //   BitManipCore.data16[6] -> data16[6] (board 44)
    //   BitManipCore.data16[5] -> data16[5] (board 43)
    //   BitManipCore.data16[4] -> data16[4] (board 42)
    //   BitManipCore.data16[3] -> data16[3] (board 41)
    //   BitManipCore.data16[2] -> data16[2] (board 40)
    //   BitManipCore.data16[1] -> data16[1] (board 39)
    //   BitManipCore.data16[0] -> data16[0] (board 38)
    //   BitManipCore.pop8[3] -> pop8[3] (board 33)
    //   BitManipCore.pop8[2] -> pop8[2] (board 32)
    //   BitManipCore.pop8[1] -> pop8[1] (board 31)
    //   BitManipCore.pop8[0] -> pop8[0] (board 30)
    //   BitManipCore.rev8[7] -> rev8[7] (board 61)
    //   BitManipCore.rev8[6] -> rev8[6] (board 60)
    //   BitManipCore.rev8[5] -> rev8[5] (board 59)
    //   BitManipCore.rev8[4] -> rev8[4] (board 58)
    //   BitManipCore.rev8[3] -> rev8[3] (board 57)
    //   BitManipCore.rev8[2] -> rev8[2] (board 56)
    //   BitManipCore.rev8[1] -> rev8[1] (board 55)
    //   BitManipCore.rev8[0] -> rev8[0] (board 54)
    //   BitManipCore.swap16[15] -> swap16[15] (board 77)
    //   BitManipCore.swap16[14] -> swap16[14] (board 76)
    //   BitManipCore.swap16[13] -> swap16[13] (board 75)
    //   BitManipCore.swap16[12] -> swap16[12] (board 74)
    //   BitManipCore.swap16[11] -> swap16[11] (board 73)
    //   BitManipCore.swap16[10] -> swap16[10] (board 72)
    //   BitManipCore.swap16[9] -> swap16[9] (board 71)
    //   BitManipCore.swap16[8] -> swap16[8] (board 70)
    //   BitManipCore.swap16[7] -> swap16[7] (board 69)
    //   BitManipCore.swap16[6] -> swap16[6] (board 68)
    //   BitManipCore.swap16[5] -> swap16[5] (board 67)
    //   BitManipCore.swap16[4] -> swap16[4] (board 66)
    //   BitManipCore.swap16[3] -> swap16[3] (board 65)
    //   BitManipCore.swap16[2] -> swap16[2] (board 64)
    //   BitManipCore.swap16[1] -> swap16[1] (board 63)
    //   BitManipCore.swap16[0] -> swap16[0] (board 62)



    BitManipCore u_top (
        .clk(SCLK),
        .rst_n(DONE),
        .data8({data8[7], data8[6], data8[5], data8[4], data8[3], data8[2], data8[1], data8[0]}),
        .data16({data16[15], data16[14], data16[13], data16[12], data16[11], data16[10], data16[9], data16[8], data16[7], data16[6], data16[5], data16[4], data16[3], data16[2], data16[1], data16[0]}),
        .pop8({pop8[3], pop8[2], pop8[1], pop8[0]}),
        .rev8({rev8[7], rev8[6], rev8[5], rev8[4], rev8[3], rev8[2], rev8[1], rev8[0]}),
        .swap16({swap16[15], swap16[14], swap16[13], swap16[12], swap16[11], swap16[10], swap16[9], swap16[8], swap16[7], swap16[6], swap16[5], swap16[4], swap16[3], swap16[2], swap16[1], swap16[0]})
    );
endmodule
