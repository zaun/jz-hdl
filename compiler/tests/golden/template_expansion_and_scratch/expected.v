// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module template_demo (
    data_in,
    data_out
);
    // Ports
    input [3:0] data_in;
    output reg [3:0] data_out;

    // Signals
    reg [3:0] pass;
    wire [1:0] tap;
    reg EXPAND__tmp__0_0;
    reg EXPAND__tmp__0_1;

    assign tap[0] = EXPAND__tmp__0_0;
    assign tap[1] = EXPAND__tmp__0_1;


    always @* begin
        pass = data_in;
        EXPAND__tmp__0_0 = pass[0];
        EXPAND__tmp__0_1 = pass[1];
        data_out[0] = tap[0];
        data_out[1] = tap[1];
        data_out[2] = pass[2];
        data_out[3] = pass[3];
    end
endmodule

module top (
    in_a,
    out_a
);
    input [3:0] in_a;
    output [3:0] out_a;

    // Top-level logical→physical pin mapping
    //   template_demo.data_in[3] -> in_a[3] (board 13)
    //   template_demo.data_in[2] -> in_a[2] (board 12)
    //   template_demo.data_in[1] -> in_a[1] (board 11)
    //   template_demo.data_in[0] -> in_a[0] (board 10)
    //   template_demo.data_out[3] -> out_a[3] (board 23)
    //   template_demo.data_out[2] -> out_a[2] (board 22)
    //   template_demo.data_out[1] -> out_a[1] (board 21)
    //   template_demo.data_out[0] -> out_a[0] (board 20)



    template_demo u_top (
        .data_in({in_a[3], in_a[2], in_a[1], in_a[0]}),
        .data_out({out_a[3], out_a[2], out_a[1], out_a[0]})
    );
endmodule
