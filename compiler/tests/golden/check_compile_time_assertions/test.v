// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module check_module (
    data_in,
    data_out
);
    // Ports
    input [7:0] data_in;
    output [7:0] data_out;

    // Signals

    assign data_out = data_in;

endmodule

module top (
    d_in,
    d_out
);
    input [7:0] d_in;
    output [7:0] d_out;

    // Top-level logical→physical pin mapping
    //   check_module.data_in[7] -> d_in[7] (board 9)
    //   check_module.data_in[6] -> d_in[6] (board 8)
    //   check_module.data_in[5] -> d_in[5] (board 7)
    //   check_module.data_in[4] -> d_in[4] (board 6)
    //   check_module.data_in[3] -> d_in[3] (board 5)
    //   check_module.data_in[2] -> d_in[2] (board 4)
    //   check_module.data_in[1] -> d_in[1] (board 3)
    //   check_module.data_in[0] -> d_in[0] (board 2)
    //   check_module.data_out[7] -> d_out[7] (board 27)
    //   check_module.data_out[6] -> d_out[6] (board 26)
    //   check_module.data_out[5] -> d_out[5] (board 25)
    //   check_module.data_out[4] -> d_out[4] (board 24)
    //   check_module.data_out[3] -> d_out[3] (board 23)
    //   check_module.data_out[2] -> d_out[2] (board 22)
    //   check_module.data_out[1] -> d_out[1] (board 21)
    //   check_module.data_out[0] -> d_out[0] (board 20)



    check_module u_top (
        .data_in({d_in[7], d_in[6], d_in[5], d_in[4], d_in[3], d_in[2], d_in[1], d_in[0]}),
        .data_out({d_out[7], d_out[6], d_out[5], d_out[4], d_out[3], d_out[2], d_out[1], d_out[0]})
    );
endmodule
