// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module bus_driver (
    sel,
    addr,
    cmd,
    valid,
    din,
    dout,
    done_out,
    pbus_SEL,
    pbus_ADDR,
    pbus_CMD,
    pbus_VALID,
    pbus_DATA,
    pbus_DONE
);
    // Ports
    input [2:0] sel;
    input [12:0] addr;
    input cmd;
    input valid;
    input [15:0] din;
    output reg [15:0] dout;
    output reg done_out;
    output [2:0] pbus_SEL;
    output [12:0] pbus_ADDR;
    output pbus_CMD;
    output pbus_VALID;
    inout [15:0] pbus_DATA;
    input pbus_DONE;

    // Signals

    assign pbus_SEL = sel;
    assign pbus_ADDR = addr;
    assign pbus_CMD = cmd;
    assign pbus_VALID = valid;

    assign pbus_DATA = valid == 1'b1 && cmd == 1'b1 ? din : 16'bzzzzzzzzzzzzzzzz;

    always @* begin
        dout = pbus_DATA;
        done_out = pbus_DONE;
    end
endmodule

module ram (
    clk,
    rst_n,
    sel,
    pbus_SEL,
    pbus_ADDR,
    pbus_CMD,
    pbus_VALID,
    pbus_DATA,
    pbus_DONE
);
    // Ports
    input clk;
    input rst_n;
    input [2:0] sel;
    input [2:0] pbus_SEL;
    input [12:0] pbus_ADDR;
    input pbus_CMD;
    input pbus_VALID;
    inout [15:0] pbus_DATA;
    output reg pbus_DONE;

    // Signals
    reg pending_read;
    reg data_ready;
    reg [15:0] read_data;
    reg [7:0] ram_read_addr;

    // Memories
    (* ram_style = "block" *) reg [15:0] ram_mem[0:255];
    integer jz_mem_init_i_0;

    initial begin
        for (jz_mem_init_i_0 = 0; jz_mem_init_i_0 < 256; jz_mem_init_i_0 = jz_mem_init_i_0 + 1) begin
            ram_mem[jz_mem_init_i_0] = 16'b0000000000000000;
        end
    end


    assign pbus_DATA = pbus_SEL == sel && pbus_VALID && pbus_CMD == 1'b0 && data_ready == 1'b1 ? read_data : 16'bzzzzzzzzzzzzzzzz;

    always @* begin
        if (pbus_SEL == sel && pbus_VALID) begin
            if (pbus_CMD == 1'b1 || data_ready == 1'b1) begin
                pbus_DONE = 1'b1;
            end
            else begin
                pbus_DONE = 1'b0;
            end
        end
        else begin
            pbus_DONE = 1'bz;
        end
    end


    // BSRAM read port: ram_mem.read (no reset - required for BSRAM inference)
    reg [15:0] ram_mem_bsram_out;
    always @(posedge clk) begin
        ram_mem_bsram_out <= ram_mem[pbus_ADDR[7:0]];
    end

    // BSRAM write port: ram.write
    always @(posedge clk) begin
        if (!!rst_n && pbus_SEL == sel && pbus_VALID && pbus_CMD == 1'b1) begin
            ram_mem[pbus_ADDR[7:0]] <= pbus_DATA;
        end
    end

    // Main logic (BLOCK memory ports in separate blocks above)
    always @(posedge clk) begin
        if (!rst_n) begin
            data_ready <= 1'b0;
            read_data <= 16'b0000000000000000;
            pending_read <= 1'b0;
            ram_read_addr <= 8'b00000000;
        end
        else begin
            if (data_ready == 1'b1) begin
                data_ready <= 1'b0;
            end
            else if (pending_read == 1'b1) begin
                read_data <= ram_mem_bsram_out;
                data_ready <= 1'b1;
                pending_read <= 1'b0;
            end
            else if (pbus_SEL == sel && pbus_VALID && pbus_CMD == 1'b0) begin
                ram_read_addr <= pbus_ADDR[7:0];
                pending_read <= 1'b1;
            end
            if (pbus_SEL == sel && pbus_VALID && pbus_CMD == 1'b1) begin
            end
        end
    end
endmodule

module ram_wrapper (
    clk,
    rst_n,
    sel,
    addr,
    cmd,
    valid,
    din,
    dout,
    done_out
);
    // Ports
    input clk;
    input rst_n;
    input [2:0] sel;
    input [12:0] addr;
    input cmd;
    input valid;
    input [15:0] din;
    output [15:0] dout;
    output done_out;

    // Signals
    wire [34:0] bus;


    bus_driver drv0 (
        .sel(sel),
        .addr(addr),
        .cmd(cmd),
        .valid(valid),
        .din(din),
        .dout(dout),
        .done_out(done_out),
        .pbus_SEL(bus[2:0]),
        .pbus_ADDR(bus[15:3]),
        .pbus_CMD(bus[16]),
        .pbus_VALID(bus[17]),
        .pbus_DATA(bus[33:18]),
        .pbus_DONE(bus[34])
    );
    ram ram0 (
        .clk(clk),
        .rst_n(rst_n),
        .sel(sel),
        .pbus_SEL(bus[2:0]),
        .pbus_ADDR(bus[15:3]),
        .pbus_CMD(bus[16]),
        .pbus_VALID(bus[17]),
        .pbus_DATA(bus[33:18]),
        .pbus_DONE(bus[34])
    );
endmodule

module top (
    clk,
    rst_n,
    sel,
    addr,
    cmd,
    valid,
    din,
    dout,
    done_out
);
    input clk;
    input rst_n;
    input [2:0] sel;
    input [12:0] addr;
    input cmd;
    input valid;
    input [15:0] din;
    output [15:0] dout;
    output done_out;

    // Top-level logical→physical pin mapping
    //   ram_wrapper.clk -> clk (board 1)
    //   ram_wrapper.rst_n -> rst_n (board 2)
    //   ram_wrapper.sel[2] -> sel[2] (board 5)
    //   ram_wrapper.sel[1] -> sel[1] (board 4)
    //   ram_wrapper.sel[0] -> sel[0] (board 3)
    //   ram_wrapper.addr[12] -> addr[12] (board 18)
    //   ram_wrapper.addr[11] -> addr[11] (board 17)
    //   ram_wrapper.addr[10] -> addr[10] (board 16)
    //   ram_wrapper.addr[9] -> addr[9] (board 15)
    //   ram_wrapper.addr[8] -> addr[8] (board 14)
    //   ram_wrapper.addr[7] -> addr[7] (board 13)
    //   ram_wrapper.addr[6] -> addr[6] (board 12)
    //   ram_wrapper.addr[5] -> addr[5] (board 11)
    //   ram_wrapper.addr[4] -> addr[4] (board 10)
    //   ram_wrapper.addr[3] -> addr[3] (board 9)
    //   ram_wrapper.addr[2] -> addr[2] (board 8)
    //   ram_wrapper.addr[1] -> addr[1] (board 7)
    //   ram_wrapper.addr[0] -> addr[0] (board 6)
    //   ram_wrapper.cmd -> cmd (board 19)
    //   ram_wrapper.valid -> valid (board 20)
    //   ram_wrapper.din[15] -> din[15] (board 36)
    //   ram_wrapper.din[14] -> din[14] (board 35)
    //   ram_wrapper.din[13] -> din[13] (board 34)
    //   ram_wrapper.din[12] -> din[12] (board 33)
    //   ram_wrapper.din[11] -> din[11] (board 32)
    //   ram_wrapper.din[10] -> din[10] (board 31)
    //   ram_wrapper.din[9] -> din[9] (board 30)
    //   ram_wrapper.din[8] -> din[8] (board 29)
    //   ram_wrapper.din[7] -> din[7] (board 28)
    //   ram_wrapper.din[6] -> din[6] (board 27)
    //   ram_wrapper.din[5] -> din[5] (board 26)
    //   ram_wrapper.din[4] -> din[4] (board 25)
    //   ram_wrapper.din[3] -> din[3] (board 24)
    //   ram_wrapper.din[2] -> din[2] (board 23)
    //   ram_wrapper.din[1] -> din[1] (board 22)
    //   ram_wrapper.din[0] -> din[0] (board 21)
    //   ram_wrapper.dout[15] -> dout[15] (board 52)
    //   ram_wrapper.dout[14] -> dout[14] (board 51)
    //   ram_wrapper.dout[13] -> dout[13] (board 50)
    //   ram_wrapper.dout[12] -> dout[12] (board 49)
    //   ram_wrapper.dout[11] -> dout[11] (board 48)
    //   ram_wrapper.dout[10] -> dout[10] (board 47)
    //   ram_wrapper.dout[9] -> dout[9] (board 46)
    //   ram_wrapper.dout[8] -> dout[8] (board 45)
    //   ram_wrapper.dout[7] -> dout[7] (board 44)
    //   ram_wrapper.dout[6] -> dout[6] (board 43)
    //   ram_wrapper.dout[5] -> dout[5] (board 42)
    //   ram_wrapper.dout[4] -> dout[4] (board 41)
    //   ram_wrapper.dout[3] -> dout[3] (board 40)
    //   ram_wrapper.dout[2] -> dout[2] (board 39)
    //   ram_wrapper.dout[1] -> dout[1] (board 38)
    //   ram_wrapper.dout[0] -> dout[0] (board 37)
    //   ram_wrapper.done_out -> done_out (board 53)



    ram_wrapper u_top (
        .clk(clk),
        .rst_n(rst_n),
        .sel({sel[2], sel[1], sel[0]}),
        .addr({addr[12], addr[11], addr[10], addr[9], addr[8], addr[7], addr[6], addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]}),
        .cmd(cmd),
        .valid(valid),
        .din({din[15], din[14], din[13], din[12], din[11], din[10], din[9], din[8], din[7], din[6], din[5], din[4], din[3], din[2], din[1], din[0]}),
        .dout({dout[15], dout[14], dout[13], dout[12], dout[11], dout[10], dout[9], dout[8], dout[7], dout[6], dout[5], dout[4], dout[3], dout[2], dout[1], dout[0]}),
        .done_out(done_out)
    );
endmodule
