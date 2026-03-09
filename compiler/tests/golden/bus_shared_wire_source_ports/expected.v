// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module mini_arb (
    sel,
    tgt0_VALID,
    tgt0_ADDR,
    tgt0_DATA,
    tgt0_DONE,
    tgt1_VALID,
    tgt1_ADDR,
    tgt1_DATA,
    tgt1_DONE,
    tgt2_VALID,
    tgt2_ADDR,
    tgt2_DATA,
    tgt2_DONE,
    tgt0_VALID_out,
    tgt0_VALID_oe,
    tgt1_VALID_out,
    tgt1_VALID_oe
);
    // Ports
    input sel;
    input tgt0_VALID;
    output reg [7:0] tgt0_ADDR;
    input [7:0] tgt0_DATA;
    input tgt0_DONE;
    input tgt1_VALID;
    output reg [7:0] tgt1_ADDR;
    input [7:0] tgt1_DATA;
    input tgt1_DONE;
    output reg tgt2_VALID;
    output reg [7:0] tgt2_ADDR;
    input [7:0] tgt2_DATA;
    input tgt2_DONE;
    output reg tgt0_VALID_out;
    output reg tgt0_VALID_oe;
    output reg tgt1_VALID_out;
    output reg tgt1_VALID_oe;

    // Signals
    reg [7:0] src_data;



    always @* begin
        tgt0_ADDR = 8'b10101010;
        src_data = 8'b00000000;
        tgt0_VALID_out = sel == 1'b0 ? 1'b1 : 1'b0;
        tgt1_VALID_out = sel == 1'b1 ? 1'b1 : 1'b0;
        tgt2_VALID = 1'b0;
        tgt0_VALID_oe = sel == 1'b0;
        tgt1_VALID_oe = sel == 1'b1;
    end
endmodule

module periph (
    pbus_VALID,
    pbus_ADDR,
    pbus_DATA,
    pbus_DONE,
    pbus_DATA_out,
    pbus_DATA_oe
);
    // Ports
    input pbus_VALID;
    input [7:0] pbus_ADDR;
    input [7:0] pbus_DATA;
    output reg pbus_DONE;
    output reg [7:0] pbus_DATA_out;
    output reg pbus_DATA_oe;

    // Signals



    always @* begin
        pbus_DATA_out = pbus_VALID == 1'b1 ? 8'b11111111 : 8'b00000000;
        if (pbus_VALID == 1'b1) begin
            pbus_DONE = 1'b1;
        end
        else begin
            pbus_DONE = 1'b0;
        end
        pbus_DATA_oe = pbus_VALID == 1'b1;
    end
endmodule

module top_mod (
    clk,
    result
);
    // Ports
    input clk;
    output reg result;

    // Signals
    wire [17:0] bus_a;
    wire [17:0] bus_b;
    reg sel;
    wire [7:0] arb0_tgt1_ADDR_dup_sink;
    wire arb0_tgt0_VALID_out;
    wire arb0_tgt0_VALID_oe;
    wire arb0_tgt1_VALID_out;
    wire arb0_tgt1_VALID_oe;
    wire arb0_tgt0_VALID_rd;
    wire arb0_tgt1_VALID_rd;
    wire [7:0] p0_pbus_DATA_out;
    wire p0_pbus_DATA_oe;
    wire [7:0] p1_pbus_DATA_out;
    wire p1_pbus_DATA_oe;
    wire [7:0] p0_pbus_DATA_rd;
    wire [7:0] p1_pbus_DATA_rd;

    assign bus_a[0] = arb0_tgt0_VALID_oe ? arb0_tgt0_VALID_out : arb0_tgt1_VALID_oe ? arb0_tgt1_VALID_out : 1'b0;
    assign arb0_tgt0_VALID_rd = arb0_tgt1_VALID_oe ? arb0_tgt1_VALID_out : 1'b0;
    assign arb0_tgt1_VALID_rd = arb0_tgt0_VALID_oe ? arb0_tgt0_VALID_out : 1'b0;
    assign bus_a[16:9] = p0_pbus_DATA_oe ? p0_pbus_DATA_out : p1_pbus_DATA_oe ? p1_pbus_DATA_out : 8'b00000000;
    assign p0_pbus_DATA_rd = p1_pbus_DATA_oe ? p1_pbus_DATA_out : 8'b00000000;
    assign p1_pbus_DATA_rd = p0_pbus_DATA_oe ? p0_pbus_DATA_out : 8'b00000000;
    assign bus_b[16:9] = bus_a[16:9];

    mini_arb arb0 (
        .sel(sel),
        .tgt0_VALID(arb0_tgt0_VALID_rd),
        .tgt0_ADDR(bus_a[8:1]),
        .tgt0_DATA(bus_a[16:9]),
        .tgt0_DONE(bus_a[17]),
        .tgt1_VALID(arb0_tgt1_VALID_rd),
        .tgt1_ADDR(arb0_tgt1_ADDR_dup_sink),
        .tgt1_DATA(bus_a[16:9]),
        .tgt1_DONE(bus_a[17]),
        .tgt2_VALID(bus_b[0]),
        .tgt2_ADDR(bus_b[8:1]),
        .tgt2_DATA(bus_b[16:9]),
        .tgt2_DONE(bus_b[17]),
        .tgt0_VALID_out(arb0_tgt0_VALID_out),
        .tgt0_VALID_oe(arb0_tgt0_VALID_oe),
        .tgt1_VALID_out(arb0_tgt1_VALID_out),
        .tgt1_VALID_oe(arb0_tgt1_VALID_oe)
    );
    periph p0 (
        .pbus_VALID(bus_a[0]),
        .pbus_ADDR(bus_a[8:1]),
        .pbus_DATA(p0_pbus_DATA_rd),
        .pbus_DONE(bus_a[17]),
        .pbus_DATA_out(p0_pbus_DATA_out),
        .pbus_DATA_oe(p0_pbus_DATA_oe)
    );
    periph p1 (
        .pbus_VALID(bus_b[0]),
        .pbus_ADDR(bus_b[8:1]),
        .pbus_DATA(p1_pbus_DATA_rd),
        .pbus_DONE(bus_b[17]),
        .pbus_DATA_out(p1_pbus_DATA_out),
        .pbus_DATA_oe(p1_pbus_DATA_oe)
    );


    always @* begin
        sel = 1'b0;
        result = 1'b0;
    end
endmodule

module top (
    clk_pin,
    out_pin
);
    input clk_pin;
    output out_pin;

    // Top-level logical→physical pin mapping
    //   top_mod.clk -> clk_pin (board 4)
    //   top_mod.result -> out_pin (board 5)



    top_mod u_top (
        .clk(clk_pin),
        .result(out_pin)
    );
endmodule
