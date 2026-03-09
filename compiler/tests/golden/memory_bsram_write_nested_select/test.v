// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module mini_cpu (
    clk,
    rst_n,
    data_in,
    data_out
);
    // Ports
    input clk;
    input rst_n;
    input [7:0] data_in;
    output reg [7:0] data_out;

    // Signals
    (* fsm_encoding = "none" *) reg [1:0] state;
    (* fsm_encoding = "none" *) reg [7:0] instr;
    reg [7:0] acc;
    reg [3:0] addr;
    reg [3:0] ram_read_addr;

    // Memories
    (* ram_style = "block" *) reg [7:0] ram[0:15];
    integer jz_mem_init_i_0;

    initial begin
        for (jz_mem_init_i_0 = 0; jz_mem_init_i_0 < 16; jz_mem_init_i_0 = jz_mem_init_i_0 + 1) begin
            ram[jz_mem_init_i_0] = 8'b00000000;
        end
    end



    always @* begin
        data_out = acc;
    end


    // BSRAM read port: ram.read (no reset - required for BSRAM inference)
    reg [7:0] ram_bsram_out;
    always @(posedge clk) begin
        ram_bsram_out <= ram[addr];
    end

    // BSRAM write port: ram.write
    always @(posedge clk) begin
        if (!!rst_n && state == 2'b01 && instr == 8'b00000010) begin
            ram[addr] <= acc;
        end
    end

    // Main logic (BLOCK memory ports in separate blocks above)
    always @(posedge clk) begin
        if (!rst_n) begin
            instr <= 8'b00000000;
            state <= 2'b00;
            ram_read_addr <= 4'b0000;
            acc <= 8'b00000000;
            addr <= 4'b0000;
        end
        else begin
            case (state)
                2'b00: begin  // ST.FETCH
                    instr <= data_in;
                    state <= 2'b01;
                end
                2'b01: begin  // ST.EXEC
                    case (instr)
                        8'b00000000: begin  // OPS.NOP
                            state <= 2'b00;
                        end
                        8'b00000001: begin  // OPS.LOAD
                            ram_read_addr <= addr;
                            acc <= ram_bsram_out;
                            state <= 2'b00;
                        end
                        8'b00000010: begin  // OPS.STORE
                            state <= 2'b00;
                        end
                        default: begin
                            state <= 2'b10;
                        end
                    endcase
                end
                2'b10: begin end  // ST.HALT
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
    data_in,
    data_out
);
    input clk;
    input rst_n;
    input [7:0] data_in;
    output [7:0] data_out;

    // Top-level logical→physical pin mapping
    //   mini_cpu.clk -> clk (board 1)
    //   mini_cpu.rst_n -> rst_n (board 2)
    //   mini_cpu.data_in[7] -> data_in[7] (board 10)
    //   mini_cpu.data_in[6] -> data_in[6] (board 9)
    //   mini_cpu.data_in[5] -> data_in[5] (board 8)
    //   mini_cpu.data_in[4] -> data_in[4] (board 7)
    //   mini_cpu.data_in[3] -> data_in[3] (board 6)
    //   mini_cpu.data_in[2] -> data_in[2] (board 5)
    //   mini_cpu.data_in[1] -> data_in[1] (board 4)
    //   mini_cpu.data_in[0] -> data_in[0] (board 3)
    //   mini_cpu.data_out[7] -> data_out[7] (board 18)
    //   mini_cpu.data_out[6] -> data_out[6] (board 17)
    //   mini_cpu.data_out[5] -> data_out[5] (board 16)
    //   mini_cpu.data_out[4] -> data_out[4] (board 15)
    //   mini_cpu.data_out[3] -> data_out[3] (board 14)
    //   mini_cpu.data_out[2] -> data_out[2] (board 13)
    //   mini_cpu.data_out[1] -> data_out[1] (board 12)
    //   mini_cpu.data_out[0] -> data_out[0] (board 11)



    mini_cpu u_top (
        .clk(clk),
        .rst_n(rst_n),
        .data_in({data_in[7], data_in[6], data_in[5], data_in[4], data_in[3], data_in[2], data_in[1], data_in[0]}),
        .data_out({data_out[7], data_out[6], data_out[5], data_out[4], data_out[3], data_out[2], data_out[1], data_out[0]})
    );
endmodule
