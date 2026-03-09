// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module cdc_raw_test (
    clk_a,
    clk_b,
    q
);
    // Ports
    input clk_a;
    input clk_b;
    output [7:0] q;

    // Signals
    reg [7:0] data_reg;
    reg [7:0] dest_reg;

    assign q = data_reg;


    always @(posedge clk_a) begin
        data_reg <= data_reg + 8'b00000001;
    end

    always @(posedge clk_b) begin
        dest_reg <= q;
    end
endmodule
