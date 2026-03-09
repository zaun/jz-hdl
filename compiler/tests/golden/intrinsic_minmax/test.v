// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module MinMaxCore (
    clk,
    rst_n,
    a,
    b,
    umn,
    umx,
    smn,
    smx
);
    // Ports
    input clk;
    input rst_n;
    input [7:0] a;
    input [7:0] b;
    output reg [7:0] umn;
    output reg [7:0] umx;
    output reg [7:0] smn;
    output reg [7:0] smx;

    // Signals



    always @* begin
        umn = ((a < b) ? a : b);
        umx = ((a > b) ? a : b);
        smn = (($signed(a) < $signed(b)) ? a : b);
        smx = (($signed(a) > $signed(b)) ? a : b);
    end
endmodule

module top (
    SCLK,
    DONE,
    a,
    b,
    umn,
    umx,
    smn,
    smx
);
    input SCLK;
    input DONE;
    input [7:0] a;
    input [7:0] b;
    output [7:0] umn;
    output [7:0] umx;
    output [7:0] smn;
    output [7:0] smx;

    // Top-level logical→physical pin mapping
    //   MinMaxCore.clk -> SCLK (board 4)
    //   MinMaxCore.rst_n -> DONE (board IOR32B)
    //   MinMaxCore.a[7] -> a[7] (board 17)
    //   MinMaxCore.a[6] -> a[6] (board 16)
    //   MinMaxCore.a[5] -> a[5] (board 15)
    //   MinMaxCore.a[4] -> a[4] (board 14)
    //   MinMaxCore.a[3] -> a[3] (board 13)
    //   MinMaxCore.a[2] -> a[2] (board 12)
    //   MinMaxCore.a[1] -> a[1] (board 11)
    //   MinMaxCore.a[0] -> a[0] (board 10)
    //   MinMaxCore.b[7] -> b[7] (board 27)
    //   MinMaxCore.b[6] -> b[6] (board 26)
    //   MinMaxCore.b[5] -> b[5] (board 25)
    //   MinMaxCore.b[4] -> b[4] (board 24)
    //   MinMaxCore.b[3] -> b[3] (board 23)
    //   MinMaxCore.b[2] -> b[2] (board 22)
    //   MinMaxCore.b[1] -> b[1] (board 21)
    //   MinMaxCore.b[0] -> b[0] (board 20)
    //   MinMaxCore.umn[7] -> umn[7] (board 37)
    //   MinMaxCore.umn[6] -> umn[6] (board 36)
    //   MinMaxCore.umn[5] -> umn[5] (board 35)
    //   MinMaxCore.umn[4] -> umn[4] (board 34)
    //   MinMaxCore.umn[3] -> umn[3] (board 33)
    //   MinMaxCore.umn[2] -> umn[2] (board 32)
    //   MinMaxCore.umn[1] -> umn[1] (board 31)
    //   MinMaxCore.umn[0] -> umn[0] (board 30)
    //   MinMaxCore.umx[7] -> umx[7] (board 45)
    //   MinMaxCore.umx[6] -> umx[6] (board 44)
    //   MinMaxCore.umx[5] -> umx[5] (board 43)
    //   MinMaxCore.umx[4] -> umx[4] (board 42)
    //   MinMaxCore.umx[3] -> umx[3] (board 41)
    //   MinMaxCore.umx[2] -> umx[2] (board 40)
    //   MinMaxCore.umx[1] -> umx[1] (board 39)
    //   MinMaxCore.umx[0] -> umx[0] (board 38)
    //   MinMaxCore.smn[7] -> smn[7] (board 53)
    //   MinMaxCore.smn[6] -> smn[6] (board 52)
    //   MinMaxCore.smn[5] -> smn[5] (board 51)
    //   MinMaxCore.smn[4] -> smn[4] (board 50)
    //   MinMaxCore.smn[3] -> smn[3] (board 49)
    //   MinMaxCore.smn[2] -> smn[2] (board 48)
    //   MinMaxCore.smn[1] -> smn[1] (board 47)
    //   MinMaxCore.smn[0] -> smn[0] (board 46)
    //   MinMaxCore.smx[7] -> smx[7] (board 61)
    //   MinMaxCore.smx[6] -> smx[6] (board 60)
    //   MinMaxCore.smx[5] -> smx[5] (board 59)
    //   MinMaxCore.smx[4] -> smx[4] (board 58)
    //   MinMaxCore.smx[3] -> smx[3] (board 57)
    //   MinMaxCore.smx[2] -> smx[2] (board 56)
    //   MinMaxCore.smx[1] -> smx[1] (board 55)
    //   MinMaxCore.smx[0] -> smx[0] (board 54)



    MinMaxCore u_top (
        .clk(SCLK),
        .rst_n(DONE),
        .a({a[7], a[6], a[5], a[4], a[3], a[2], a[1], a[0]}),
        .b({b[7], b[6], b[5], b[4], b[3], b[2], b[1], b[0]}),
        .umn({umn[7], umn[6], umn[5], umn[4], umn[3], umn[2], umn[1], umn[0]}),
        .umx({umx[7], umx[6], umx[5], umx[4], umx[3], umx[2], umx[1], umx[0]}),
        .smn({smn[7], smn[6], smn[5], smn[4], smn[3], smn[2], smn[1], smn[0]}),
        .smx({smx[7], smx[6], smx[5], smx[4], smx[3], smx[2], smx[1], smx[0]})
    );
endmodule
