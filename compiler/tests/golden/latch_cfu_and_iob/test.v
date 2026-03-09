// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module latch_module (
    enable,
    data_in,
    iob_out,
    comb_out
);
    // Ports
    input enable;
    input [7:0] data_in;
    output reg [7:0] iob_out;
    output reg [7:0] comb_out;

    // Signals
    (* IOB = "TRUE" *) reg [7:0] iob_latch;
    reg [7:0] cfu_latch;



    always @* begin
        if (enable) iob_latch <= data_in;
        if (enable) cfu_latch <= data_in;
        iob_out = iob_latch;
        comb_out = cfu_latch ^ 8'b11111111;
    end
endmodule

module top (
    sys_clk,
    enable,
    data_in,
    iob_out,
    comb_out
);
    input sys_clk;
    input enable;
    input [7:0] data_in;
    output [7:0] iob_out;
    output [7:0] comb_out;

    // Top-level logical→physical pin mapping
    //   latch_module.enable -> enable (board 3)
    //   latch_module.data_in[7] -> data_in[7] (board 32)
    //   latch_module.data_in[6] -> data_in[6] (board 31)
    //   latch_module.data_in[5] -> data_in[5] (board 30)
    //   latch_module.data_in[4] -> data_in[4] (board 29)
    //   latch_module.data_in[3] -> data_in[3] (board 28)
    //   latch_module.data_in[2] -> data_in[2] (board 27)
    //   latch_module.data_in[1] -> data_in[1] (board 26)
    //   latch_module.data_in[0] -> data_in[0] (board 25)
    //   latch_module.iob_out[7] -> iob_out[7] (board 18)
    //   latch_module.iob_out[6] -> iob_out[6] (board 17)
    //   latch_module.iob_out[5] -> iob_out[5] (board 16)
    //   latch_module.iob_out[4] -> iob_out[4] (board 15)
    //   latch_module.iob_out[3] -> iob_out[3] (board 14)
    //   latch_module.iob_out[2] -> iob_out[2] (board 13)
    //   latch_module.iob_out[1] -> iob_out[1] (board 11)
    //   latch_module.iob_out[0] -> iob_out[0] (board 10)
    //   latch_module.comb_out[7] -> comb_out[7] (board 40)
    //   latch_module.comb_out[6] -> comb_out[6] (board 39)
    //   latch_module.comb_out[5] -> comb_out[5] (board 38)
    //   latch_module.comb_out[4] -> comb_out[4] (board 37)
    //   latch_module.comb_out[3] -> comb_out[3] (board 36)
    //   latch_module.comb_out[2] -> comb_out[2] (board 35)
    //   latch_module.comb_out[1] -> comb_out[1] (board 34)
    //   latch_module.comb_out[0] -> comb_out[0] (board 33)



    latch_module u_top (
        .enable(enable),
        .data_in({data_in[7], data_in[6], data_in[5], data_in[4], data_in[3], data_in[2], data_in[1], data_in[0]}),
        .iob_out({iob_out[7], iob_out[6], iob_out[5], iob_out[4], iob_out[3], iob_out[2], iob_out[1], iob_out[0]}),
        .comb_out({comb_out[7], comb_out[6], comb_out[5], comb_out[4], comb_out[3], comb_out[2], comb_out[1], comb_out[0]})
    );
endmodule
