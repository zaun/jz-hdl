// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module concat_child (
    clk,
    rst_n,
    cfg,
    sel,
    mix,
    result
);
    // Ports
    input clk;
    input rst_n;
    input [3:0] cfg;
    input [1:0] sel;
    input [2:0] mix;
    output [7:0] result;

    // Signals
    wire [8:0] combined;
    reg [7:0] out_reg;

    assign combined = {sel, cfg, mix};
    assign result = out_reg;


    always @(posedge clk) begin
        if (!rst_n) begin
            out_reg <= 8'b00000000;
        end
        else begin
            out_reg <= combined[7:0];
        end
    end
endmodule

module concat_parent (
    clk,
    rst_n,
    en,
    out
);
    // Ports
    input clk;
    input rst_n;
    input en;
    output [7:0] out;

    // Signals
    wire sel_b;
    wire [1:0] mode;
    reg toggle;

    assign sel_b = ~en;
    assign mode = {en, toggle};

    concat_child u0 (
        .clk(clk),
        .rst_n(rst_n),
        .cfg({2'b10, 2'b01}),
        .sel({en, sel_b}),
        .mix({mode, 1'b0}),
        .result(out)
    );


    always @(posedge clk) begin
        if (!rst_n) begin
            toggle <= 1'b0;
        end
        else begin
            toggle <= ~toggle;
        end
    end
endmodule

module top (
    sys_clk,
    sys_rst,
    enable,
    result
);
    input sys_clk;
    input sys_rst;
    input enable;
    output [7:0] result;

    // Top-level logical→physical pin mapping
    //   concat_parent.clk -> sys_clk (board 4)
    //   concat_parent.rst_n -> sys_rst (board 88)
    //   concat_parent.en -> enable (board 87)
    //   concat_parent.out[7] -> result[7] (board 26)
    //   concat_parent.out[6] -> result[6] (board 25)
    //   concat_parent.out[5] -> result[5] (board 20)
    //   concat_parent.out[4] -> result[4] (board 19)
    //   concat_parent.out[3] -> result[3] (board 18)
    //   concat_parent.out[2] -> result[2] (board 17)
    //   concat_parent.out[1] -> result[1] (board 16)
    //   concat_parent.out[0] -> result[0] (board 15)



    concat_parent u_top (
        .clk(sys_clk),
        .rst_n(sys_rst),
        .en(enable),
        .out({result[7], result[6], result[5], result[4], result[3], result[2], result[1], result[0]})
    );
endmodule
