// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module dual_driver (
    sel,
    port_a,
    port_b,
    port_a_out,
    port_a_oe,
    port_b_out,
    port_b_oe
);
    // Ports
    input sel;
    input [7:0] port_a;
    input [7:0] port_b;
    output reg [7:0] port_a_out;
    output reg port_a_oe;
    output reg [7:0] port_b_out;
    output reg port_b_oe;

    // Signals



    always @* begin
        port_a_out = sel == 1'b1 ? 8'b10101010 : 8'b00000000;
        port_b_out = sel == 1'b0 ? 8'b10111011 : 8'b00000000;
        port_a_oe = sel == 1'b1;
        port_b_oe = sel == 1'b0;
    end
endmodule

module other_driver (
    sel,
    data,
    data_out,
    data_oe
);
    // Ports
    input sel;
    input [7:0] data;
    output reg [7:0] data_out;
    output reg data_oe;

    // Signals



    always @* begin
        data_out = sel == 1'b1 ? 8'b11001100 : 8'b00000000;
        data_oe = sel == 1'b1;
    end
endmodule

module top_mod (
    clk,
    sel,
    data
);
    // Ports
    input clk;
    input sel;
    inout [7:0] data;

    // Signals
    wire [7:0] drv0_port_a_out;
    wire drv0_port_a_oe;
    wire [7:0] drv0_port_b_out;
    wire drv0_port_b_oe;
    wire [7:0] drv1_data_out;
    wire drv1_data_oe;
    wire [7:0] drv0_port_a_rd;
    wire [7:0] drv0_port_b_rd;
    wire [7:0] drv1_data_rd;

    assign data = drv0_port_a_oe ? drv0_port_a_out : drv0_port_b_oe ? drv0_port_b_out : drv1_data_oe ? drv1_data_out : 8'b00000000;
    assign drv0_port_a_rd = drv0_port_b_oe ? drv0_port_b_out : drv1_data_oe ? drv1_data_out : 8'b00000000;
    assign drv0_port_b_rd = drv0_port_a_oe ? drv0_port_a_out : drv1_data_oe ? drv1_data_out : 8'b00000000;
    assign drv1_data_rd = drv0_port_a_oe ? drv0_port_a_out : drv0_port_b_oe ? drv0_port_b_out : 8'b00000000;

    dual_driver drv0 (
        .sel(sel),
        .port_a(drv0_port_a_rd),
        .port_b(drv0_port_b_rd),
        .port_a_out(drv0_port_a_out),
        .port_a_oe(drv0_port_a_oe),
        .port_b_out(drv0_port_b_out),
        .port_b_oe(drv0_port_b_oe)
    );
    other_driver drv1 (
        .sel(sel),
        .data(drv1_data_rd),
        .data_out(drv1_data_out),
        .data_oe(drv1_data_oe)
    );

endmodule

module top (
    clk_pin,
    sel_pin,
    data_pin
);
    input clk_pin;
    input sel_pin;
    inout [7:0] data_pin;

    // Top-level logical→physical pin mapping
    //   top_mod.clk -> clk_pin (board 4)
    //   top_mod.sel -> sel_pin (board 5)
    //   top_mod.data[7] -> data_pin[7] (board 17)
    //   top_mod.data[6] -> data_pin[6] (board 16)
    //   top_mod.data[5] -> data_pin[5] (board 15)
    //   top_mod.data[4] -> data_pin[4] (board 14)
    //   top_mod.data[3] -> data_pin[3] (board 13)
    //   top_mod.data[2] -> data_pin[2] (board 12)
    //   top_mod.data[1] -> data_pin[1] (board 11)
    //   top_mod.data[0] -> data_pin[0] (board 10)



    top_mod u_top (
        .clk(clk_pin),
        .sel(sel_pin),
        .data({data_pin[7], data_pin[6], data_pin[5], data_pin[4], data_pin[3], data_pin[2], data_pin[1], data_pin[0]})
    );
endmodule
