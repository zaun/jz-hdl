// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module unwritten_reg_test_module (
    clk,
    rst_n,
    d_in,
    q
);
    // Ports
    input clk;
    input rst_n;
    input [7:0] d_in;
    output [7:0] q;

    // Signals
    wire [15:0] scaled;
    reg [7:0] data;
    reg [7:0] factor;

    assign scaled = d_in * factor;
    assign q = scaled[15:8];


    always @(posedge clk) begin
        if (!rst_n) begin
            data <= 8'b00000000;
            factor <= 8'b10101011;
        end
        else begin
            data <= d_in;
        end
    end
endmodule

module top (
    sys_clk,
    sys_rst,
    data_in,
    result
);
    input sys_clk;
    input sys_rst;
    input [7:0] data_in;
    output [7:0] result;

    // Top-level logical→physical pin mapping
    //   unwritten_reg_test_module.clk -> sys_clk (board 52)
    //   unwritten_reg_test_module.rst_n -> sys_rst (board 53)
    //   unwritten_reg_test_module.d_in[7] -> data_in[7] (board 8)
    //   unwritten_reg_test_module.d_in[6] -> data_in[6] (board 7)
    //   unwritten_reg_test_module.d_in[5] -> data_in[5] (board 6)
    //   unwritten_reg_test_module.d_in[4] -> data_in[4] (board 5)
    //   unwritten_reg_test_module.d_in[3] -> data_in[3] (board 4)
    //   unwritten_reg_test_module.d_in[2] -> data_in[2] (board 3)
    //   unwritten_reg_test_module.d_in[1] -> data_in[1] (board 2)
    //   unwritten_reg_test_module.d_in[0] -> data_in[0] (board 1)
    //   unwritten_reg_test_module.q[7] -> result[7] (board 17)
    //   unwritten_reg_test_module.q[6] -> result[6] (board 16)
    //   unwritten_reg_test_module.q[5] -> result[5] (board 15)
    //   unwritten_reg_test_module.q[4] -> result[4] (board 14)
    //   unwritten_reg_test_module.q[3] -> result[3] (board 13)
    //   unwritten_reg_test_module.q[2] -> result[2] (board 12)
    //   unwritten_reg_test_module.q[1] -> result[1] (board 11)
    //   unwritten_reg_test_module.q[0] -> result[0] (board 10)



    unwritten_reg_test_module u_top (
        .clk(sys_clk),
        .rst_n(sys_rst),
        .d_in({data_in[7], data_in[6], data_in[5], data_in[4], data_in[3], data_in[2], data_in[1], data_in[0]}),
        .q({result[7], result[6], result[5], result[4], result[3], result[2], result[1], result[0]})
    );
endmodule
