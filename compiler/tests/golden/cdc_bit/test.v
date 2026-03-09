// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module JZHDL_LIB_CDC_BIT (
    clk_dest,
    data_in,
    data_out
);
    // Ports
    input clk_dest;
    input data_in;
    output data_out;

    // Signals
    reg sync_ff1;
    reg sync_ff2;

    assign data_out = sync_ff2;


    always @(posedge clk_dest) begin
        sync_ff1 <= data_in;
        sync_ff2 <= sync_ff1;
    end
endmodule

module cdc_bit_test (
    clk_a,
    clk_b,
    q
);
    // Ports
    input clk_a;
    input clk_b;
    output q;

    // Signals
    reg flag;
    reg dest_reg;


    JZHDL_LIB_CDC_BIT u_cdc_bit_flag_sync (
        .clk_dest(clk_b),
        .data_in(flag),
        .data_out(q)
    );


    always @(posedge clk_a) begin
        flag <= ~flag;
    end

    always @(posedge clk_b) begin
        dest_reg <= q;
    end
endmodule
