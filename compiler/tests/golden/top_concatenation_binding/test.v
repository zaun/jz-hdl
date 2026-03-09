// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module concat_mod (
    clk,
    btn,
    bidir_in,
    leds
);
    // Ports
    input clk;
    input [1:0] btn;
    input bidir_in;
    output reg [5:0] leds;

    // Signals
    reg [4:0] count;



    always @* begin
        leds = {bidir_in, count};
    end

    always @(posedge clk) begin
        count <= count + {3'b000, btn};
    end
endmodule

module top (
    SCLK,
    BTN,
    LED,
    BIDIR
);
    input SCLK;
    input [1:0] BTN;
    output [4:0] LED;
    inout BIDIR;

    // Top-level logical→physical pin mapping
    //   concat_mod.clk -> SCLK (board 1)
    //   concat_mod.btn[1] -> BTN[1] (board 3)
    //   concat_mod.btn[0] -> BTN[0] (board 2)
    //   concat_mod.bidir_in -> BIDIR (board 20)
    //   concat_mod.leds[5] -> BIDIR[0]
    //   concat_mod.leds[4] -> LED[4] (board 14)
    //   concat_mod.leds[3] -> LED[3] (board 13)
    //   concat_mod.leds[2] -> LED[2] (board 12)
    //   concat_mod.leds[1] -> LED[1] (board 11)
    //   concat_mod.leds[0] -> LED[0] (board 10)



    concat_mod u_top (
        .clk(SCLK),
        .btn({BTN[1], BTN[0]}),
        .bidir_in(BIDIR),
        .leds({BIDIR[0], LED[4], LED[3], LED[2], LED[1], LED[0]})
    );
endmodule
