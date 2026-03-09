// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module mux_demo_top (
    sel,
    op_mode,
    data_out
);
    // Ports
    input [1:0] sel;
    input op_mode;
    output reg [7:0] data_out;

    // Signals
    reg [7:0] ch_a;
    reg [7:0] ch_b;
    reg [7:0] ch_c;
    reg [7:0] ch_d;
    reg [31:0] pipeline_reg;



    always @* begin
        ch_a = 8'b00010001;
        ch_b = 8'b00100010;
        ch_c = 8'b00110011;
        ch_d = 8'b01000100;
        pipeline_reg = 32'b10101010101110111100110011011101;
        if (op_mode == 1'b1) begin
            data_out = (({ch_d, ch_c, ch_b, ch_a} >> (sel * 8)) & {8{1'b1}});
        end
        else begin
            data_out = ((pipeline_reg >> (sel * 8)) & {8{1'b1}});
        end
    end
endmodule

module top (
    select,
    mode,
    byte_out
);
    input [1:0] select;
    input mode;
    output [7:0] byte_out;

    // Top-level logical→physical pin mapping
    //   mux_demo_top.sel[1] -> select[1] (board 3)
    //   mux_demo_top.sel[0] -> select[0] (board 2)
    //   mux_demo_top.op_mode -> mode (board 4)
    //   mux_demo_top.data_out[7] -> byte_out[7] (board 17)
    //   mux_demo_top.data_out[6] -> byte_out[6] (board 16)
    //   mux_demo_top.data_out[5] -> byte_out[5] (board 15)
    //   mux_demo_top.data_out[4] -> byte_out[4] (board 14)
    //   mux_demo_top.data_out[3] -> byte_out[3] (board 13)
    //   mux_demo_top.data_out[2] -> byte_out[2] (board 12)
    //   mux_demo_top.data_out[1] -> byte_out[1] (board 11)
    //   mux_demo_top.data_out[0] -> byte_out[0] (board 10)



    mux_demo_top u_top (
        .sel({select[1], select[0]}),
        .op_mode(mode),
        .data_out({byte_out[7], byte_out[6], byte_out[5], byte_out[4], byte_out[3], byte_out[2], byte_out[1], byte_out[0]})
    );
endmodule
