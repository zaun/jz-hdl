// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module mem_addr_cond_test (
    clk,
    rst_n,
    addr_a,
    addr_b,
    result
);
    // Ports
    input clk;
    input rst_n;
    input [3:0] addr_a;
    input [3:0] addr_b;
    output [7:0] result;

    // Signals
    (* fsm_encoding = "none" *) reg [1:0] state;
    reg [7:0] data_out;
    reg [3:0] rom_read_addr;

    // Memories
    (* ram_style = "block" *) reg [7:0] rom[0:15];
    integer jz_mem_init_i_0;

    initial begin
        for (jz_mem_init_i_0 = 0; jz_mem_init_i_0 < 16; jz_mem_init_i_0 = jz_mem_init_i_0 + 1) begin
            rom[jz_mem_init_i_0] = 8'b00000000;
        end
    end

    assign result = data_out;



    // BSRAM read port: rom.read (no reset - required for BSRAM inference)
    reg [7:0] rom_bsram_out;
    always @(posedge clk) begin
        rom_bsram_out <= rom[addr_a];
    end

    // Main logic (BLOCK memory ports in separate blocks above)
    always @(posedge clk) begin
        if (!rst_n) begin
            rom_read_addr <= 4'b0000;
            state <= 2'b00;
            data_out <= 8'b00000000;
        end
        else begin
            case (state)
                2'b00: begin  // ST.IDLE
                    rom_read_addr <= addr_a;
                    state <= 2'b01;
                end
                2'b01: begin  // ST.READ
                    data_out <= rom_bsram_out;
                    rom_read_addr <= addr_b;
                    state <= 2'b10;
                end
                2'b10: begin  // ST.DONE
                    state <= 2'b00;
                end
                default: begin
                    state <= 2'b00;
                end
            endcase
        end
    end
endmodule

module top (
    clk,
    rst_n,
    addr_a,
    addr_b,
    result
);
    input clk;
    input rst_n;
    input [3:0] addr_a;
    input [3:0] addr_b;
    output [7:0] result;

    // Top-level logical→physical pin mapping
    //   mem_addr_cond_test.clk -> clk (board 1)
    //   mem_addr_cond_test.rst_n -> rst_n (board 2)
    //   mem_addr_cond_test.addr_a[3] -> addr_a[3] (board 6)
    //   mem_addr_cond_test.addr_a[2] -> addr_a[2] (board 5)
    //   mem_addr_cond_test.addr_a[1] -> addr_a[1] (board 4)
    //   mem_addr_cond_test.addr_a[0] -> addr_a[0] (board 3)
    //   mem_addr_cond_test.addr_b[3] -> addr_b[3] (board 10)
    //   mem_addr_cond_test.addr_b[2] -> addr_b[2] (board 9)
    //   mem_addr_cond_test.addr_b[1] -> addr_b[1] (board 8)
    //   mem_addr_cond_test.addr_b[0] -> addr_b[0] (board 7)
    //   mem_addr_cond_test.result[7] -> result[7] (board 18)
    //   mem_addr_cond_test.result[6] -> result[6] (board 17)
    //   mem_addr_cond_test.result[5] -> result[5] (board 16)
    //   mem_addr_cond_test.result[4] -> result[4] (board 15)
    //   mem_addr_cond_test.result[3] -> result[3] (board 14)
    //   mem_addr_cond_test.result[2] -> result[2] (board 13)
    //   mem_addr_cond_test.result[1] -> result[1] (board 12)
    //   mem_addr_cond_test.result[0] -> result[0] (board 11)



    mem_addr_cond_test u_top (
        .clk(clk),
        .rst_n(rst_n),
        .addr_a({addr_a[3], addr_a[2], addr_a[1], addr_a[0]}),
        .addr_b({addr_b[3], addr_b[2], addr_b[1], addr_b[0]}),
        .result({result[7], result[6], result[5], result[4], result[3], result[2], result[1], result[0]})
    );
endmodule
