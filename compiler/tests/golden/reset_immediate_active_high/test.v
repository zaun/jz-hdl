// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module JZHDL_LIB_RESET_SYNC (
    clk,
    rst_async_n,
    rst_sync_n
);
    // Ports
    input clk;
    input rst_async_n;
    output rst_sync_n;

    // Signals
    reg sync_ff1;
    reg sync_ff2;

    assign rst_sync_n = sync_ff2;


    always @(posedge clk or negedge rst_async_n) begin
        if (!rst_async_n) begin
            sync_ff1 <= 1'b0;
            sync_ff2 <= 1'b0;
        end
        else begin
            sync_ff1 <= 1'b1;
            sync_ff2 <= sync_ff1;
        end
    end
endmodule

module reset_imm_hi (
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
    wire rst_sync_0;

    assign q = reg_a;

    JZHDL_LIB_RESET_SYNC u_rst_sync_0 (
        .clk(clk),
        .rst_async_n(rst),
        .rst_sync_n(rst_sync_0)
    );


    always @(posedge clk) begin
        if (rst_sync_0) begin
            reg_a <= 8'b00000000;
        end
        else begin
            if (en) begin
                reg_a <= d_in;
            end
        end
    end
endmodule
