// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module JZHDL_LIB_CDC_BUS__W8 (
    clk_src,
    clk_dest,
    data_in,
    data_out
);
    // Ports
    input clk_src;
    input clk_dest;
    input [7:0] data_in;
    output reg [7:0] data_out;

    // Signals
    reg [7:0] gray_src;
    reg [7:0] gray_sync1;
    reg [7:0] gray_sync2;



    always @* begin
        data_out[7] = gray_sync2[7];
        data_out[6] = data_out[7] ^ gray_sync2[6];
        data_out[5] = data_out[6] ^ gray_sync2[5];
        data_out[4] = data_out[5] ^ gray_sync2[4];
        data_out[3] = data_out[4] ^ gray_sync2[3];
        data_out[2] = data_out[3] ^ gray_sync2[2];
        data_out[1] = data_out[2] ^ gray_sync2[1];
        data_out[0] = data_out[1] ^ gray_sync2[0];
    end

    always @(posedge clk_src) begin
        gray_src <= data_in >> 8'b00000001 ^ data_in;
    end

    always @(posedge clk_dest) begin
        gray_sync1 <= gray_src;
        gray_sync2 <= gray_sync1;
    end
endmodule

module cdc_bus_test (
    clk_a,
    clk_b,
    q
);
    // Ports
    input clk_a;
    input clk_b;
    output [7:0] q;

    // Signals
    reg [7:0] counter;
    reg dest_reg;


    JZHDL_LIB_CDC_BUS__W8 u_cdc_bus_counter_sync (
        .clk_src(clk_a),
        .clk_dest(clk_b),
        .data_in(counter),
        .data_out(q)
    );


    always @(posedge clk_a) begin
        counter <= counter + 8'b00000001;
    end

    always @(posedge clk_b) begin
        dest_reg <= q[0];
    end
endmodule
