// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module feature_module (
    clk,
    rst,
    data_in,
    data_out
);
    // Ports
    input clk;
    input rst;
    input [7:0] data_in;
    output [7:0] data_out;

    // Signals
    reg [7:0] counter;

    assign data_out = counter;


    always @* begin
    end

    always @(posedge clk) begin
        if (!rst) begin
            counter <= 8'b00000000;
        end
        else begin
            counter <= counter + 8'b00000001;
        end
    end
endmodule

module top (
    d_in,
    clk_in,
    rst_in,
    d_out
);
    input [7:0] d_in;
    input clk_in;
    input rst_in;
    output [7:0] d_out;

    // Top-level logical→physical pin mapping
    //   feature_module.clk -> clk_in (board 4)
    //   feature_module.rst -> rst_in (board 88)
    //   feature_module.data_in[7] -> d_in[7] (board 37)
    //   feature_module.data_in[6] -> d_in[6] (board 36)
    //   feature_module.data_in[5] -> d_in[5] (board 35)
    //   feature_module.data_in[4] -> d_in[4] (board 34)
    //   feature_module.data_in[3] -> d_in[3] (board 33)
    //   feature_module.data_in[2] -> d_in[2] (board 32)
    //   feature_module.data_in[1] -> d_in[1] (board 31)
    //   feature_module.data_in[0] -> d_in[0] (board 30)
    //   feature_module.data_out[7] -> d_out[7] (board 27)
    //   feature_module.data_out[6] -> d_out[6] (board 26)
    //   feature_module.data_out[5] -> d_out[5] (board 25)
    //   feature_module.data_out[4] -> d_out[4] (board 24)
    //   feature_module.data_out[3] -> d_out[3] (board 23)
    //   feature_module.data_out[2] -> d_out[2] (board 22)
    //   feature_module.data_out[1] -> d_out[1] (board 21)
    //   feature_module.data_out[0] -> d_out[0] (board 20)



    feature_module u_top (
        .clk(clk_in),
        .rst(rst_in),
        .data_in({d_in[7], d_in[6], d_in[5], d_in[4], d_in[3], d_in[2], d_in[1], d_in[0]}),
        .data_out({d_out[7], d_out[6], d_out[5], d_out[4], d_out[3], d_out[2], d_out[1], d_out[0]})
    );
endmodule
