// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module z_checker (
    en,
    bus
);
    // Ports
    input en;
    output reg [7:0] bus;

    // Signals



    always @* begin
        bus = en ? 8'bzzzzzzzz : 8'b00000001;
    end
endmodule

module literal_checker (
    out_bin,
    out_hex,
    out_dec,
    out_x,
    out_z
);
    // Ports
    output reg [7:0] out_bin;
    output reg [7:0] out_hex;
    output reg [7:0] out_dec;
    output reg [7:0] out_x;
    output [7:0] out_z;

    // Signals


    z_checker z0 (
        .en(1'b0),
        .bus(out_z)
    );
    z_checker z1 (
        .en(1'b1),
        .bus(out_z)
    );


    always @* begin
        out_bin = 8'b00001010;
        out_hex = 8'b00001111;
        out_dec = 8'b00001101;
        out_x = 8'b00000000;
    end
endmodule

module top (
    result_bin,
    result_hex,
    result_dec,
    result_x,
    result_z
);
    output [7:0] result_bin;
    output [7:0] result_hex;
    output [7:0] result_dec;
    output [7:0] result_x;
    output [7:0] result_z;

    // Top-level logical→physical pin mapping
    //   literal_checker.out_bin[7] -> result_bin[7] (board 7)
    //   literal_checker.out_bin[6] -> result_bin[6] (board 6)
    //   literal_checker.out_bin[5] -> result_bin[5] (board 5)
    //   literal_checker.out_bin[4] -> result_bin[4] (board 4)
    //   literal_checker.out_bin[3] -> result_bin[3] (board 3)
    //   literal_checker.out_bin[2] -> result_bin[2] (board 2)
    //   literal_checker.out_bin[1] -> result_bin[1] (board 1)
    //   literal_checker.out_bin[0] -> result_bin[0] (board 0)
    //   literal_checker.out_hex[7] -> result_hex[7] (board 15)
    //   literal_checker.out_hex[6] -> result_hex[6] (board 14)
    //   literal_checker.out_hex[5] -> result_hex[5] (board 13)
    //   literal_checker.out_hex[4] -> result_hex[4] (board 12)
    //   literal_checker.out_hex[3] -> result_hex[3] (board 11)
    //   literal_checker.out_hex[2] -> result_hex[2] (board 10)
    //   literal_checker.out_hex[1] -> result_hex[1] (board 9)
    //   literal_checker.out_hex[0] -> result_hex[0] (board 8)
    //   literal_checker.out_dec[7] -> result_dec[7] (board 23)
    //   literal_checker.out_dec[6] -> result_dec[6] (board 22)
    //   literal_checker.out_dec[5] -> result_dec[5] (board 21)
    //   literal_checker.out_dec[4] -> result_dec[4] (board 20)
    //   literal_checker.out_dec[3] -> result_dec[3] (board 19)
    //   literal_checker.out_dec[2] -> result_dec[2] (board 18)
    //   literal_checker.out_dec[1] -> result_dec[1] (board 17)
    //   literal_checker.out_dec[0] -> result_dec[0] (board 16)
    //   literal_checker.out_x[7] -> result_x[7] (board 31)
    //   literal_checker.out_x[6] -> result_x[6] (board 30)
    //   literal_checker.out_x[5] -> result_x[5] (board 29)
    //   literal_checker.out_x[4] -> result_x[4] (board 28)
    //   literal_checker.out_x[3] -> result_x[3] (board 27)
    //   literal_checker.out_x[2] -> result_x[2] (board 26)
    //   literal_checker.out_x[1] -> result_x[1] (board 25)
    //   literal_checker.out_x[0] -> result_x[0] (board 24)
    //   literal_checker.out_z[7] -> result_z[7] (board 39)
    //   literal_checker.out_z[6] -> result_z[6] (board 38)
    //   literal_checker.out_z[5] -> result_z[5] (board 37)
    //   literal_checker.out_z[4] -> result_z[4] (board 36)
    //   literal_checker.out_z[3] -> result_z[3] (board 35)
    //   literal_checker.out_z[2] -> result_z[2] (board 34)
    //   literal_checker.out_z[1] -> result_z[1] (board 33)
    //   literal_checker.out_z[0] -> result_z[0] (board 32)



    literal_checker u_top (
        .out_bin({result_bin[7], result_bin[6], result_bin[5], result_bin[4], result_bin[3], result_bin[2], result_bin[1], result_bin[0]}),
        .out_hex({result_hex[7], result_hex[6], result_hex[5], result_hex[4], result_hex[3], result_hex[2], result_hex[1], result_hex[0]}),
        .out_dec({result_dec[7], result_dec[6], result_dec[5], result_dec[4], result_dec[3], result_dec[2], result_dec[1], result_dec[0]}),
        .out_x({result_x[7], result_x[6], result_x[5], result_x[4], result_x[3], result_x[2], result_x[1], result_x[0]}),
        .out_z({result_z[7], result_z[6], result_z[5], result_z[4], result_z[3], result_z[2], result_z[1], result_z[0]})
    );
endmodule
