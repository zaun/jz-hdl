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
    tgt2_DONE
);
    // Ports
    input sel;
    output reg tgt0_VALID;
    output reg [7:0] tgt0_ADDR;
    inout [7:0] tgt0_DATA;
    input tgt0_DONE;
    output reg tgt1_VALID;
    output reg [7:0] tgt1_ADDR;
    inout [7:0] tgt1_DATA;
    input tgt1_DONE;
    output reg tgt2_VALID;
    output reg [7:0] tgt2_ADDR;
    inout [7:0] tgt2_DATA;
    input tgt2_DONE;

    // Signals



    always @* begin
        tgt0_ADDR = 8'b10101010;
        tgt0_DATA = 8'b00000000;
        tgt0_VALID = sel == 1'b0 ? 1'b1 : 1'b0;
        tgt1_VALID = sel == 1'b1 ? 1'b1 : 1'b0;
        tgt2_VALID = 1'b0;
    end
endmodule

module periph (
    pbus_VALID,
    pbus_ADDR,
    pbus_DATA,
    pbus_DONE
);
    // Ports
    input pbus_VALID;
    input [7:0] pbus_ADDR;
    inout [7:0] pbus_DATA;
    output reg pbus_DONE;

    // Signals


    assign pbus_DATA = pbus_VALID == 1'b1 ? 8'b11111111 : 8'bzzzzzzzz;

    always @* begin
        if (pbus_VALID == 1'b1) begin
            pbus_DONE = 1'b1;
        end
        else begin
            pbus_DONE = 1'bz;
        end
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


    mini_arb arb0 (
        .sel(sel),
        .tgt0_VALID(bus_a[0]),
        .tgt0_ADDR(bus_a[8:1]),
        .tgt0_DATA(bus_a[16:9]),
        .tgt0_DONE(bus_a[17]),
        .tgt1_VALID(bus_a[0]),
        .tgt1_ADDR(bus_a[8:1]),
        .tgt1_DATA(bus_a[16:9]),
        .tgt1_DONE(bus_a[17]),
        .tgt2_VALID(bus_b[0]),
        .tgt2_ADDR(bus_b[8:1]),
        .tgt2_DATA(bus_b[16:9]),
        .tgt2_DONE(bus_b[17])
    );
    periph p0 (
        .pbus_VALID(bus_a[0]),
        .pbus_ADDR(bus_a[8:1]),
        .pbus_DATA(bus_a[16:9]),
        .pbus_DONE(bus_a[17])
    );
    periph p1 (
        .pbus_VALID(bus_b[0]),
        .pbus_ADDR(bus_b[8:1]),
        .pbus_DATA(bus_b[16:9]),
        .pbus_DONE(bus_b[17])
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
