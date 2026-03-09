// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module protocol_manager (
    clk,
    rst_n,
    por,
    rx_data,
    valid
);
    // Ports
    input clk;
    input rst_n;
    input por;
    input [2:0] rx_data;
    output valid;

    // Signals
    wire reset;
    wire [2:0] buffer_source;
    wire is_sync;
    reg [15:0] status_reg;
    reg match_flag;
    reg [1:0] packet_storage_port_a_addr;

    // Memories
    (* ram_style = "block" *) reg [2:0] packet_storage[0:2];
    integer jz_mem_init_i_0;

    initial begin
        for (jz_mem_init_i_0 = 0; jz_mem_init_i_0 < 3; jz_mem_init_i_0 = jz_mem_init_i_0 + 1) begin
            packet_storage[jz_mem_init_i_0] = 3'b000;
        end
    end

    assign reset = por & rst_n;
    assign is_sync = rx_data == 3'b101;
    assign valid = match_flag;


    always @(posedge clk) begin
        if (!reset) begin
            match_flag <= 1'b0;
            status_reg <= 16'b1010010110100101;
        end
        else begin
            if (is_sync) begin
                match_flag <= 1'b1;
                status_reg <= {{8{1'b0}}, 8'b00000001};
            end
            else begin
                match_flag <= 1'b0;
            end
        end
    end
endmodule

module top (
    SCLK,
    DONE,
    KEY,
    A,
    LED
);
    input SCLK;
    input DONE;
    input [1:0] KEY;
    input [2:0] A;
    output [5:0] LED;

    // Top-level logical→physical pin mapping
    //   protocol_manager.clk -> SCLK (board 4)
    //   protocol_manager.rst_n -> KEY[0] (board 88)
    //   protocol_manager.por -> DONE (board IOR32B)
    //   protocol_manager.rx_data[2] -> A[2] (board 75)
    //   protocol_manager.rx_data[1] -> A[1] (board 74)
    //   protocol_manager.rx_data[0] -> A[0] (board 73)
    //   protocol_manager.valid -> LED[0] (board 15)

    wire jz_inv_valid;
    assign LED[0] = ~jz_inv_valid;


    protocol_manager u_top (
        .clk(SCLK),
        .rst_n(~KEY[0]),
        .por(DONE),
        .rx_data({A[2], A[1], A[0]}),
        .valid(jz_inv_valid)
    );
endmodule
