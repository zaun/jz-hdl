// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: Version 0.1.3 (3981ed5)
// Intended for use with yosys.

`default_nettype none

module mem_reader__SPEC_DATA_FILE_data_lut_bin (
    clk,
    rst_n,
    addr,
    dout
);
    // Ports
    input clk;
    input rst_n;
    input [2:0] addr;
    output reg [7:0] dout;

    // Signals
    reg [7:0] out_r;
    reg [2:0] lut_read_addr;

    // Memories
    (* ram_style = "block" *) reg [7:0] lut[0:7];

    initial begin
        $readmemh("data/lut.bin.hex", lut);
    end



    always @* begin
        dout = out_r;
    end


    // BSRAM read port: lut.read (no reset - required for BSRAM inference)
    reg [7:0] lut_bsram_out;
    always @(posedge clk) begin
        lut_bsram_out <= lut[addr];
    end

    // Main logic (BLOCK memory ports in separate blocks above)
    always @(posedge clk) begin
        if (!rst_n) begin
            lut_read_addr <= 3'b000;
            out_r <= 8'b00000000;
        end
        else begin
            lut_read_addr <= addr;
            out_r <= lut_bsram_out;
        end
    end
endmodule

module mem_test_top (
    clk,
    rst_n,
    addr,
    dout
);
    // Ports
    input clk;
    input rst_n;
    input [2:0] addr;
    output [7:0] dout;

    // Signals


    mem_reader__SPEC_DATA_FILE_data_lut_bin rd0 (
        .clk(clk),
        .rst_n(rst_n),
        .addr(addr),
        .dout(dout)
    );
endmodule

module top (
    clk,
    rst_n,
    addr,
    dout
);
    input clk;
    input rst_n;
    input [2:0] addr;
    output [7:0] dout;

    // Top-level logical→physical pin mapping
    //   mem_test_top.clk -> clk (board 1)
    //   mem_test_top.rst_n -> rst_n (board 2)
    //   mem_test_top.addr[2] -> addr[2] (board 5)
    //   mem_test_top.addr[1] -> addr[1] (board 4)
    //   mem_test_top.addr[0] -> addr[0] (board 3)
    //   mem_test_top.dout[7] -> dout[7] (board 13)
    //   mem_test_top.dout[6] -> dout[6] (board 12)
    //   mem_test_top.dout[5] -> dout[5] (board 11)
    //   mem_test_top.dout[4] -> dout[4] (board 10)
    //   mem_test_top.dout[3] -> dout[3] (board 9)
    //   mem_test_top.dout[2] -> dout[2] (board 8)
    //   mem_test_top.dout[1] -> dout[1] (board 7)
    //   mem_test_top.dout[0] -> dout[0] (board 6)



    mem_test_top u_top (
        .clk(clk),
        .rst_n(rst_n),
        .addr({addr[2], addr[1], addr[0]}),
        .dout({dout[7], dout[6], dout[5], dout[4], dout[3], dout[2], dout[1], dout[0]})
    );
endmodule
