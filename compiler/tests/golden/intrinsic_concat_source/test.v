// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module IntrinsicConcatCore (
    clk,
    rst_n,
    a,
    b,
    c,
    d,
    pop_out,
    rev_out,
    swap_out,
    prienc_out,
    lzc_out,
    abs_out,
    rand_out,
    ror_out,
    rxor_out,
    oh2b_out,
    b2oh_out,
    gbit_out,
    gslice_out
);
    // Ports
    input clk;
    input rst_n;
    input [3:0] a;
    input [3:0] b;
    input [7:0] c;
    input [7:0] d;
    output reg [3:0] pop_out;
    output reg [7:0] rev_out;
    output reg [15:0] swap_out;
    output reg [2:0] prienc_out;
    output reg [3:0] lzc_out;
    output reg [8:0] abs_out;
    output reg rand_out;
    output reg ror_out;
    output reg rxor_out;
    output reg [2:0] oh2b_out;
    output reg [7:0] b2oh_out;
    output reg gbit_out;
    output reg [3:0] gslice_out;

    // Signals



    always @* begin
        pop_out = (({a, b} & 1'b1) + (({a, b} >> 1) & 1'b1) + (({a, b} >> 2) & 1'b1) + (({a, b} >> 3) & 1'b1) + (({a, b} >> 4) & 1'b1) + (({a, b} >> 5) & 1'b1) + (({a, b} >> 6) & 1'b1) + (({a, b} >> 7) & 1'b1));
        rev_out = {(({a, b} >> 0) & 1'b1), (({a, b} >> 1) & 1'b1), (({a, b} >> 2) & 1'b1), (({a, b} >> 3) & 1'b1), (({a, b} >> 4) & 1'b1), (({a, b} >> 5) & 1'b1), (({a, b} >> 6) & 1'b1), (({a, b} >> 7) & 1'b1)};
        swap_out = {(({c, d} >> 0) & 8'hFF), (({c, d} >> 8) & 8'hFF)};
        prienc_out = (({a, b} >> 7) & 1'b1) ? 3'd7 : (({a, b} >> 6) & 1'b1) ? 3'd6 : (({a, b} >> 5) & 1'b1) ? 3'd5 : (({a, b} >> 4) & 1'b1) ? 3'd4 : (({a, b} >> 3) & 1'b1) ? 3'd3 : (({a, b} >> 2) & 1'b1) ? 3'd2 : (({a, b} >> 1) & 1'b1) ? 3'd1 : 3'd0;
        lzc_out = (({a, b} >> 7) & 1'b1) ? 4'd0 : (({a, b} >> 6) & 1'b1) ? 4'd1 : (({a, b} >> 5) & 1'b1) ? 4'd2 : (({a, b} >> 4) & 1'b1) ? 4'd3 : (({a, b} >> 3) & 1'b1) ? 4'd4 : (({a, b} >> 2) & 1'b1) ? 4'd5 : (({a, b} >> 1) & 1'b1) ? 4'd6 : (({a, b} >> 0) & 1'b1) ? 4'd7 : 4'd8;
        abs_out = ((({a, b} >> 7) & 1'b1) ? ({9{1'b0}} - {1'b0, {a, b}}) : {1'b0, {a, b}});
        rand_out = &({a, b});
        ror_out = |({a, b});
        rxor_out = ^({a, b});
        oh2b_out = {|((({a, b} >> 4) & 1'b1) | (({a, b} >> 5) & 1'b1) | (({a, b} >> 6) & 1'b1) | (({a, b} >> 7) & 1'b1)), |((({a, b} >> 2) & 1'b1) | (({a, b} >> 3) & 1'b1) | (({a, b} >> 6) & 1'b1) | (({a, b} >> 7) & 1'b1)), |((({a, b} >> 1) & 1'b1) | (({a, b} >> 3) & 1'b1) | (({a, b} >> 5) & 1'b1) | (({a, b} >> 7) & 1'b1))};
        b2oh_out = 1'b0;
        gbit_out = (({a, b} >> a) & 1'b1);
        gslice_out = (({c, d} >> (a * 16)) & {16{1'b1}});
    end
endmodule

module top (
    SCLK,
    DONE,
    a,
    b,
    c,
    d,
    pop_out,
    rev_out,
    swap_out,
    prienc_out,
    lzc_out,
    abs_out,
    rand_out,
    ror_out,
    rxor_out,
    oh2b_out,
    b2oh_out,
    gbit_out,
    gslice_out
);
    input SCLK;
    input DONE;
    input [3:0] a;
    input [3:0] b;
    input [7:0] c;
    input [7:0] d;
    output [3:0] pop_out;
    output [7:0] rev_out;
    output [15:0] swap_out;
    output [2:0] prienc_out;
    output [3:0] lzc_out;
    output [8:0] abs_out;
    output rand_out;
    output ror_out;
    output rxor_out;
    output [2:0] oh2b_out;
    output [7:0] b2oh_out;
    output gbit_out;
    output [3:0] gslice_out;

    // Top-level logical→physical pin mapping
    //   IntrinsicConcatCore.clk -> SCLK (board 4)
    //   IntrinsicConcatCore.rst_n -> DONE (board IOR32B)
    //   IntrinsicConcatCore.a[3] -> a[3] (board 13)
    //   IntrinsicConcatCore.a[2] -> a[2] (board 12)
    //   IntrinsicConcatCore.a[1] -> a[1] (board 11)
    //   IntrinsicConcatCore.a[0] -> a[0] (board 10)
    //   IntrinsicConcatCore.b[3] -> b[3] (board 17)
    //   IntrinsicConcatCore.b[2] -> b[2] (board 16)
    //   IntrinsicConcatCore.b[1] -> b[1] (board 15)
    //   IntrinsicConcatCore.b[0] -> b[0] (board 14)
    //   IntrinsicConcatCore.c[7] -> c[7] (board 32)
    //   IntrinsicConcatCore.c[6] -> c[6] (board 31)
    //   IntrinsicConcatCore.c[5] -> c[5] (board 30)
    //   IntrinsicConcatCore.c[4] -> c[4] (board 29)
    //   IntrinsicConcatCore.c[3] -> c[3] (board 28)
    //   IntrinsicConcatCore.c[2] -> c[2] (board 27)
    //   IntrinsicConcatCore.c[1] -> c[1] (board 26)
    //   IntrinsicConcatCore.c[0] -> c[0] (board 25)
    //   IntrinsicConcatCore.d[7] -> d[7] (board 40)
    //   IntrinsicConcatCore.d[6] -> d[6] (board 39)
    //   IntrinsicConcatCore.d[5] -> d[5] (board 38)
    //   IntrinsicConcatCore.d[4] -> d[4] (board 37)
    //   IntrinsicConcatCore.d[3] -> d[3] (board 36)
    //   IntrinsicConcatCore.d[2] -> d[2] (board 35)
    //   IntrinsicConcatCore.d[1] -> d[1] (board 34)
    //   IntrinsicConcatCore.d[0] -> d[0] (board 33)
    //   IntrinsicConcatCore.pop_out[3] -> pop_out[3] (board 44)
    //   IntrinsicConcatCore.pop_out[2] -> pop_out[2] (board 43)
    //   IntrinsicConcatCore.pop_out[1] -> pop_out[1] (board 42)
    //   IntrinsicConcatCore.pop_out[0] -> pop_out[0] (board 41)
    //   IntrinsicConcatCore.rev_out[7] -> rev_out[7] (board 52)
    //   IntrinsicConcatCore.rev_out[6] -> rev_out[6] (board 51)
    //   IntrinsicConcatCore.rev_out[5] -> rev_out[5] (board 50)
    //   IntrinsicConcatCore.rev_out[4] -> rev_out[4] (board 49)
    //   IntrinsicConcatCore.rev_out[3] -> rev_out[3] (board 48)
    //   IntrinsicConcatCore.rev_out[2] -> rev_out[2] (board 47)
    //   IntrinsicConcatCore.rev_out[1] -> rev_out[1] (board 46)
    //   IntrinsicConcatCore.rev_out[0] -> rev_out[0] (board 45)
    //   IntrinsicConcatCore.swap_out[15] -> swap_out[15] (board 68)
    //   IntrinsicConcatCore.swap_out[14] -> swap_out[14] (board 67)
    //   IntrinsicConcatCore.swap_out[13] -> swap_out[13] (board 66)
    //   IntrinsicConcatCore.swap_out[12] -> swap_out[12] (board 65)
    //   IntrinsicConcatCore.swap_out[11] -> swap_out[11] (board 64)
    //   IntrinsicConcatCore.swap_out[10] -> swap_out[10] (board 63)
    //   IntrinsicConcatCore.swap_out[9] -> swap_out[9] (board 62)
    //   IntrinsicConcatCore.swap_out[8] -> swap_out[8] (board 61)
    //   IntrinsicConcatCore.swap_out[7] -> swap_out[7] (board 60)
    //   IntrinsicConcatCore.swap_out[6] -> swap_out[6] (board 59)
    //   IntrinsicConcatCore.swap_out[5] -> swap_out[5] (board 58)
    //   IntrinsicConcatCore.swap_out[4] -> swap_out[4] (board 57)
    //   IntrinsicConcatCore.swap_out[3] -> swap_out[3] (board 56)
    //   IntrinsicConcatCore.swap_out[2] -> swap_out[2] (board 55)
    //   IntrinsicConcatCore.swap_out[1] -> swap_out[1] (board 54)
    //   IntrinsicConcatCore.swap_out[0] -> swap_out[0] (board 53)
    //   IntrinsicConcatCore.prienc_out[2] -> prienc_out[2] (board 71)
    //   IntrinsicConcatCore.prienc_out[1] -> prienc_out[1] (board 70)
    //   IntrinsicConcatCore.prienc_out[0] -> prienc_out[0] (board 69)
    //   IntrinsicConcatCore.lzc_out[3] -> lzc_out[3] (board 75)
    //   IntrinsicConcatCore.lzc_out[2] -> lzc_out[2] (board 74)
    //   IntrinsicConcatCore.lzc_out[1] -> lzc_out[1] (board 73)
    //   IntrinsicConcatCore.lzc_out[0] -> lzc_out[0] (board 72)
    //   IntrinsicConcatCore.abs_out[8] -> abs_out[8] (board 84)
    //   IntrinsicConcatCore.abs_out[7] -> abs_out[7] (board 83)
    //   IntrinsicConcatCore.abs_out[6] -> abs_out[6] (board 82)
    //   IntrinsicConcatCore.abs_out[5] -> abs_out[5] (board 81)
    //   IntrinsicConcatCore.abs_out[4] -> abs_out[4] (board 80)
    //   IntrinsicConcatCore.abs_out[3] -> abs_out[3] (board 79)
    //   IntrinsicConcatCore.abs_out[2] -> abs_out[2] (board 78)
    //   IntrinsicConcatCore.abs_out[1] -> abs_out[1] (board 77)
    //   IntrinsicConcatCore.abs_out[0] -> abs_out[0] (board 76)
    //   IntrinsicConcatCore.rand_out -> rand_out (board 85)
    //   IntrinsicConcatCore.ror_out -> ror_out (board 86)
    //   IntrinsicConcatCore.rxor_out -> rxor_out (board 18)
    //   IntrinsicConcatCore.oh2b_out[2] -> oh2b_out[2] (board 21)
    //   IntrinsicConcatCore.oh2b_out[1] -> oh2b_out[1] (board 20)
    //   IntrinsicConcatCore.oh2b_out[0] -> oh2b_out[0] (board 19)
    //   IntrinsicConcatCore.b2oh_out[7] -> b2oh_out[7] (board IOR30B)
    //   IntrinsicConcatCore.b2oh_out[6] -> b2oh_out[6] (board IOR30A)
    //   IntrinsicConcatCore.b2oh_out[5] -> b2oh_out[5] (board IOL14B)
    //   IntrinsicConcatCore.b2oh_out[4] -> b2oh_out[4] (board IOL14A)
    //   IntrinsicConcatCore.b2oh_out[3] -> b2oh_out[3] (board IOL7A)
    //   IntrinsicConcatCore.b2oh_out[2] -> b2oh_out[2] (board 24)
    //   IntrinsicConcatCore.b2oh_out[1] -> b2oh_out[1] (board 23)
    //   IntrinsicConcatCore.b2oh_out[0] -> b2oh_out[0] (board 22)
    //   IntrinsicConcatCore.gbit_out -> gbit_out (board IOR29A)
    //   IntrinsicConcatCore.gslice_out[3] -> gslice_out[3] (board IOR27A)
    //   IntrinsicConcatCore.gslice_out[2] -> gslice_out[2] (board IOR28B)
    //   IntrinsicConcatCore.gslice_out[1] -> gslice_out[1] (board IOR28A)
    //   IntrinsicConcatCore.gslice_out[0] -> gslice_out[0] (board IOR29B)



    IntrinsicConcatCore u_top (
        .clk(SCLK),
        .rst_n(DONE),
        .a({a[3], a[2], a[1], a[0]}),
        .b({b[3], b[2], b[1], b[0]}),
        .c({c[7], c[6], c[5], c[4], c[3], c[2], c[1], c[0]}),
        .d({d[7], d[6], d[5], d[4], d[3], d[2], d[1], d[0]}),
        .pop_out({pop_out[3], pop_out[2], pop_out[1], pop_out[0]}),
        .rev_out({rev_out[7], rev_out[6], rev_out[5], rev_out[4], rev_out[3], rev_out[2], rev_out[1], rev_out[0]}),
        .swap_out({swap_out[15], swap_out[14], swap_out[13], swap_out[12], swap_out[11], swap_out[10], swap_out[9], swap_out[8], swap_out[7], swap_out[6], swap_out[5], swap_out[4], swap_out[3], swap_out[2], swap_out[1], swap_out[0]}),
        .prienc_out({prienc_out[2], prienc_out[1], prienc_out[0]}),
        .lzc_out({lzc_out[3], lzc_out[2], lzc_out[1], lzc_out[0]}),
        .abs_out({abs_out[8], abs_out[7], abs_out[6], abs_out[5], abs_out[4], abs_out[3], abs_out[2], abs_out[1], abs_out[0]}),
        .rand_out(rand_out),
        .ror_out(ror_out),
        .rxor_out(rxor_out),
        .oh2b_out({oh2b_out[2], oh2b_out[1], oh2b_out[0]}),
        .b2oh_out({b2oh_out[7], b2oh_out[6], b2oh_out[5], b2oh_out[4], b2oh_out[3], b2oh_out[2], b2oh_out[1], b2oh_out[0]}),
        .gbit_out(gbit_out),
        .gslice_out({gslice_out[3], gslice_out[2], gslice_out[1], gslice_out[0]})
    );
endmodule
