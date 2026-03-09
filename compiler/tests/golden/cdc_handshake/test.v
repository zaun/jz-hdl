// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module JZHDL_LIB_CDC_HANDSHAKE__W8 (
    clk_src,
    clk_dest,
    data_in,
    data_out
);
    // Ports
    input clk_src;
    input clk_dest;
    input [7:0] data_in;
    output [7:0] data_out;

    // Signals
    reg [7:0] src_data;
    reg req_src;
    reg req_sync1;
    reg req_sync2;
    reg ack_dest;
    reg ack_sync1;
    reg ack_sync2;

    assign data_out = src_data;


    always @(posedge clk_src) begin
        ack_sync1 <= ack_dest;
        ack_sync2 <= ack_sync1;
        if (!req_src && !ack_sync2) begin
            src_data <= data_in;
            req_src <= 1'b1;
        end
        if (ack_sync2) begin
            req_src <= 1'b0;
        end
    end

    always @(posedge clk_dest) begin
        req_sync1 <= req_src;
        req_sync2 <= req_sync1;
        if (req_sync2 && !ack_dest) begin
            ack_dest <= 1'b1;
        end
        if (!req_sync2) begin
            ack_dest <= 1'b0;
        end
    end
endmodule

module cdc_handshake_test (
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


    JZHDL_LIB_CDC_HANDSHAKE__W8 u_cdc_handshake_counter_sync (
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
