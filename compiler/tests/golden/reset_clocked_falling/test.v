// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module reset_clk_fall (
    clk,
    rst,
    en,
    d_in,
    q
);
    // Ports
    input clk;
    input rst;
    input en;
    input [7:0] d_in;
    output [7:0] q;

    // Signals
    reg [7:0] reg_a;

    assign q = reg_a;


    always @(negedge clk) begin
        if (!rst) begin
            reg_a <= 8'b00000000;
        end
        else begin
            if (en) begin
                reg_a <= d_in;
            end
        end
    end
endmodule
