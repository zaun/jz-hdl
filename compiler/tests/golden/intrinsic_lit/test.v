// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module LitCore (
    clk,
    rst_n,
    data,
    dout,
    match
);
    // Ports
    input clk;
    input rst_n;
    input [7:0] data;
    output reg [7:0] dout;
    output reg match;

    // Signals
    reg eq_zero;
    reg eq_max;
    reg [7:0] counter;



    always @* begin
        eq_zero = data == 8'b00000000 ? 1'b1 : 1'b0;
        eq_max = counter == 8'b11000111 ? 1'b1 : 1'b0;
        dout = data ^ 8'b11111111;
        match = eq_zero | eq_max;
    end

    always @(posedge clk) begin
        if (!rst_n) begin
            counter <= 8'b00000000;
        end
        else begin
            if (counter == 8'b11000111) begin
                counter <= 8'b00000000;
            end
            else begin
                counter <= counter + 8'b00000001;
            end
        end
    end
endmodule

module top (
    SCLK,
    DONE,
    din,
    result,
    flag
);
    input SCLK;
    input DONE;
    input [7:0] din;
    output [7:0] result;
    output flag;

    // Top-level logical→physical pin mapping
    //   LitCore.clk -> SCLK (board 4)
    //   LitCore.rst_n -> DONE (board IOR32B)
    //   LitCore.data[7] -> din[7] (board 17)
    //   LitCore.data[6] -> din[6] (board 16)
    //   LitCore.data[5] -> din[5] (board 15)
    //   LitCore.data[4] -> din[4] (board 14)
    //   LitCore.data[3] -> din[3] (board 13)
    //   LitCore.data[2] -> din[2] (board 12)
    //   LitCore.data[1] -> din[1] (board 11)
    //   LitCore.data[0] -> din[0] (board 10)
    //   LitCore.dout[7] -> result[7] (board 37)
    //   LitCore.dout[6] -> result[6] (board 36)
    //   LitCore.dout[5] -> result[5] (board 35)
    //   LitCore.dout[4] -> result[4] (board 34)
    //   LitCore.dout[3] -> result[3] (board 33)
    //   LitCore.dout[2] -> result[2] (board 32)
    //   LitCore.dout[1] -> result[1] (board 31)
    //   LitCore.dout[0] -> result[0] (board 30)
    //   LitCore.match -> flag (board 40)



    LitCore u_top (
        .clk(SCLK),
        .rst_n(DONE),
        .data({din[7], din[6], din[5], din[4], din[3], din[2], din[1], din[0]}),
        .dout({result[7], result[6], result[5], result[4], result[3], result[2], result[1], result[0]}),
        .match(flag)
    );
endmodule
