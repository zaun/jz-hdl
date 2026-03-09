// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module JZHDL_LIB_CDC_FIFO__W8 (
    clk_wr,
    clk_rd,
    rst_wr,
    rst_rd,
    data_in,
    write_en,
    read_en,
    data_out,
    full,
    empty
);
    // Ports
    input clk_wr;
    input clk_rd;
    input rst_wr;
    input rst_rd;
    input [7:0] data_in;
    input write_en;
    input read_en;
    output [7:0] data_out;
    output full;
    output empty;

    // Signals
    reg [4:0] wr_ptr_bin;
    reg [4:0] wr_ptr_gray;
    reg [4:0] rd_ptr_bin;
    reg [4:0] rd_ptr_gray;
    reg [4:0] wr_ptr_gray_sync1;
    reg [4:0] wr_ptr_gray_sync2;
    reg [4:0] rd_ptr_gray_sync1;
    reg [4:0] rd_ptr_gray_sync2;

    // Memories
    (* ram_style = "distributed" *) reg [7:0] mem[0:15];


    assign data_out = mem[rd_ptr_bin[3:0]];
    assign full = wr_ptr_gray == {~rd_ptr_gray_sync2[4:3], rd_ptr_gray_sync2[2:0]};
    assign empty = rd_ptr_gray == wr_ptr_gray_sync2;


    always @(posedge clk_wr or posedge rst_wr) begin
        if (rst_wr) begin
            wr_ptr_bin <= 5'b00000;
            wr_ptr_gray <= 5'b00000;
        end
        else begin
            if (write_en && ~full) begin
                mem[wr_ptr_bin[3:0]] <= data_in;
                wr_ptr_bin <= wr_ptr_bin + 5'b00001;
                wr_ptr_gray <= wr_ptr_bin + 5'b00001 >> 5'b00001 ^ wr_ptr_bin + 5'b00001;
            end
        end
    end

    always @(posedge clk_rd or posedge rst_rd) begin
        if (rst_rd) begin
            rd_ptr_bin <= 5'b00000;
            rd_ptr_gray <= 5'b00000;
        end
        else begin
            if (read_en && ~empty) begin
                rd_ptr_bin <= rd_ptr_bin + 5'b00001;
                rd_ptr_gray <= rd_ptr_bin + 5'b00001 >> 5'b00001 ^ rd_ptr_bin + 5'b00001;
            end
        end
    end

    always @(posedge clk_wr or posedge rst_wr) begin
        if (rst_wr) begin
            rd_ptr_gray_sync1 <= 5'b00000;
            rd_ptr_gray_sync2 <= 5'b00000;
        end
        else begin
            rd_ptr_gray_sync1 <= rd_ptr_gray;
            rd_ptr_gray_sync2 <= rd_ptr_gray_sync1;
        end
    end

    always @(posedge clk_rd or posedge rst_rd) begin
        if (rst_rd) begin
            wr_ptr_gray_sync1 <= 5'b00000;
            wr_ptr_gray_sync2 <= 5'b00000;
        end
        else begin
            wr_ptr_gray_sync1 <= wr_ptr_gray;
            wr_ptr_gray_sync2 <= wr_ptr_gray_sync1;
        end
    end
endmodule

module cdc_fifo_test (
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


    JZHDL_LIB_CDC_FIFO__W8 u_cdc_fifo_counter_sync (
        .clk_wr(clk_a),
        .clk_rd(clk_b),
        .rst_wr(1'b0),
        .rst_rd(1'b0),
        .data_in(counter),
        .write_en(1'b1),
        .read_en(1'b1),
        .data_out(q)
    );


    always @(posedge clk_a) begin
        counter <= counter + 8'b00000001;
    end

    always @(posedge clk_b) begin
        dest_reg <= q[0];
    end
endmodule
