// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module Oh2bCore (
    clk,
    rst_n,
    onehot_in,
    sel4_in,
    index3,
    index2,
    latched
);
    // Ports
    input clk;
    input rst_n;
    input [7:0] onehot_in;
    input [3:0] sel4_in;
    output reg [2:0] index3;
    output reg [1:0] index2;
    output reg [2:0] latched;

    // Signals
    reg [2:0] idx8;
    reg [1:0] idx4;
    reg [2:0] idx_reg;



    always @* begin
        idx8 = {|(((onehot_in >> 4) & 1'b1) | ((onehot_in >> 5) & 1'b1) | ((onehot_in >> 6) & 1'b1) | ((onehot_in >> 7) & 1'b1)), |(((onehot_in >> 2) & 1'b1) | ((onehot_in >> 3) & 1'b1) | ((onehot_in >> 6) & 1'b1) | ((onehot_in >> 7) & 1'b1)), |(((onehot_in >> 1) & 1'b1) | ((onehot_in >> 3) & 1'b1) | ((onehot_in >> 5) & 1'b1) | ((onehot_in >> 7) & 1'b1))};
        index3 = idx8;
        idx4 = {|(((sel4_in >> 2) & 1'b1) | ((sel4_in >> 3) & 1'b1)), |(((sel4_in >> 1) & 1'b1) | ((sel4_in >> 3) & 1'b1))};
        index2 = idx4;
        latched = idx_reg;
    end

    always @(posedge clk) begin
        if (!rst_n) begin
            idx_reg <= 3'b000;
        end
        else begin
            idx_reg <= {|(((onehot_in >> 4) & 1'b1) | ((onehot_in >> 5) & 1'b1) | ((onehot_in >> 6) & 1'b1) | ((onehot_in >> 7) & 1'b1)), |(((onehot_in >> 2) & 1'b1) | ((onehot_in >> 3) & 1'b1) | ((onehot_in >> 6) & 1'b1) | ((onehot_in >> 7) & 1'b1)), |(((onehot_in >> 1) & 1'b1) | ((onehot_in >> 3) & 1'b1) | ((onehot_in >> 5) & 1'b1) | ((onehot_in >> 7) & 1'b1))};
        end
    end
endmodule

module top (
    SCLK,
    DONE,
    onehot,
    sel4,
    idx3,
    idx2,
    latched
);
    input SCLK;
    input DONE;
    input [7:0] onehot;
    input [3:0] sel4;
    output [2:0] idx3;
    output [1:0] idx2;
    output [2:0] latched;

    // Top-level logical→physical pin mapping
    //   Oh2bCore.clk -> SCLK (board 4)
    //   Oh2bCore.rst_n -> DONE (board IOR32B)
    //   Oh2bCore.onehot_in[7] -> onehot[7] (board 17)
    //   Oh2bCore.onehot_in[6] -> onehot[6] (board 16)
    //   Oh2bCore.onehot_in[5] -> onehot[5] (board 15)
    //   Oh2bCore.onehot_in[4] -> onehot[4] (board 14)
    //   Oh2bCore.onehot_in[3] -> onehot[3] (board 13)
    //   Oh2bCore.onehot_in[2] -> onehot[2] (board 12)
    //   Oh2bCore.onehot_in[1] -> onehot[1] (board 11)
    //   Oh2bCore.onehot_in[0] -> onehot[0] (board 10)
    //   Oh2bCore.sel4_in[3] -> sel4[3] (board 23)
    //   Oh2bCore.sel4_in[2] -> sel4[2] (board 22)
    //   Oh2bCore.sel4_in[1] -> sel4[1] (board 21)
    //   Oh2bCore.sel4_in[0] -> sel4[0] (board 20)
    //   Oh2bCore.index3[2] -> idx3[2] (board 32)
    //   Oh2bCore.index3[1] -> idx3[1] (board 31)
    //   Oh2bCore.index3[0] -> idx3[0] (board 30)
    //   Oh2bCore.index2[1] -> idx2[1] (board 34)
    //   Oh2bCore.index2[0] -> idx2[0] (board 33)
    //   Oh2bCore.latched[2] -> latched[2] (board 37)
    //   Oh2bCore.latched[1] -> latched[1] (board 36)
    //   Oh2bCore.latched[0] -> latched[0] (board 35)



    Oh2bCore u_top (
        .clk(SCLK),
        .rst_n(DONE),
        .onehot_in({onehot[7], onehot[6], onehot[5], onehot[4], onehot[3], onehot[2], onehot[1], onehot[0]}),
        .sel4_in({sel4[3], sel4[2], sel4[1], sel4[0]}),
        .index3({idx3[2], idx3[1], idx3[0]}),
        .index2({idx2[1], idx2[0]}),
        .latched({latched[2], latched[1], latched[0]})
    );
endmodule
