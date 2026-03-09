// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module SafeArithCore (
    clk,
    rst_n,
    a,
    b,
    udiff,
    sdiff,
    absv
);
    // Ports
    input clk;
    input rst_n;
    input [7:0] a;
    input [7:0] b;
    output reg [8:0] udiff;
    output reg [8:0] sdiff;
    output reg [8:0] absv;

    // Signals



    always @* begin
        udiff = ({{1{1'b0}}, a} - {{1{1'b0}}, b});
        sdiff = ({{1{a[7]}}, a} - {{1{b[7]}}, b});
        absv = (((a >> 7) & 1'b1) ? ({9{1'b0}} - {1'b0, a}) : {1'b0, a});
    end
endmodule

module top (
    SCLK,
    DONE,
    a,
    b,
    udiff,
    sdiff,
    absv
);
    input SCLK;
    input DONE;
    input [7:0] a;
    input [7:0] b;
    output [8:0] udiff;
    output [8:0] sdiff;
    output [8:0] absv;

    // Top-level logical→physical pin mapping
    //   SafeArithCore.clk -> SCLK (board 4)
    //   SafeArithCore.rst_n -> DONE (board IOR32B)
    //   SafeArithCore.a[7] -> a[7] (board 17)
    //   SafeArithCore.a[6] -> a[6] (board 16)
    //   SafeArithCore.a[5] -> a[5] (board 15)
    //   SafeArithCore.a[4] -> a[4] (board 14)
    //   SafeArithCore.a[3] -> a[3] (board 13)
    //   SafeArithCore.a[2] -> a[2] (board 12)
    //   SafeArithCore.a[1] -> a[1] (board 11)
    //   SafeArithCore.a[0] -> a[0] (board 10)
    //   SafeArithCore.b[7] -> b[7] (board 27)
    //   SafeArithCore.b[6] -> b[6] (board 26)
    //   SafeArithCore.b[5] -> b[5] (board 25)
    //   SafeArithCore.b[4] -> b[4] (board 24)
    //   SafeArithCore.b[3] -> b[3] (board 23)
    //   SafeArithCore.b[2] -> b[2] (board 22)
    //   SafeArithCore.b[1] -> b[1] (board 21)
    //   SafeArithCore.b[0] -> b[0] (board 20)
    //   SafeArithCore.udiff[8] -> udiff[8] (board 38)
    //   SafeArithCore.udiff[7] -> udiff[7] (board 37)
    //   SafeArithCore.udiff[6] -> udiff[6] (board 36)
    //   SafeArithCore.udiff[5] -> udiff[5] (board 35)
    //   SafeArithCore.udiff[4] -> udiff[4] (board 34)
    //   SafeArithCore.udiff[3] -> udiff[3] (board 33)
    //   SafeArithCore.udiff[2] -> udiff[2] (board 32)
    //   SafeArithCore.udiff[1] -> udiff[1] (board 31)
    //   SafeArithCore.udiff[0] -> udiff[0] (board 30)
    //   SafeArithCore.sdiff[8] -> sdiff[8] (board 47)
    //   SafeArithCore.sdiff[7] -> sdiff[7] (board 46)
    //   SafeArithCore.sdiff[6] -> sdiff[6] (board 45)
    //   SafeArithCore.sdiff[5] -> sdiff[5] (board 44)
    //   SafeArithCore.sdiff[4] -> sdiff[4] (board 43)
    //   SafeArithCore.sdiff[3] -> sdiff[3] (board 42)
    //   SafeArithCore.sdiff[2] -> sdiff[2] (board 41)
    //   SafeArithCore.sdiff[1] -> sdiff[1] (board 40)
    //   SafeArithCore.sdiff[0] -> sdiff[0] (board 39)
    //   SafeArithCore.absv[8] -> absv[8] (board 56)
    //   SafeArithCore.absv[7] -> absv[7] (board 55)
    //   SafeArithCore.absv[6] -> absv[6] (board 54)
    //   SafeArithCore.absv[5] -> absv[5] (board 53)
    //   SafeArithCore.absv[4] -> absv[4] (board 52)
    //   SafeArithCore.absv[3] -> absv[3] (board 51)
    //   SafeArithCore.absv[2] -> absv[2] (board 50)
    //   SafeArithCore.absv[1] -> absv[1] (board 49)
    //   SafeArithCore.absv[0] -> absv[0] (board 48)



    SafeArithCore u_top (
        .clk(SCLK),
        .rst_n(DONE),
        .a({a[7], a[6], a[5], a[4], a[3], a[2], a[1], a[0]}),
        .b({b[7], b[6], b[5], b[4], b[3], b[2], b[1], b[0]}),
        .udiff({udiff[8], udiff[7], udiff[6], udiff[5], udiff[4], udiff[3], udiff[2], udiff[1], udiff[0]}),
        .sdiff({sdiff[8], sdiff[7], sdiff[6], sdiff[5], sdiff[4], sdiff[3], sdiff[2], sdiff[1], sdiff[0]}),
        .absv({absv[8], absv[7], absv[6], absv[5], absv[4], absv[3], absv[2], absv[1], absv[0]})
    );
endmodule
