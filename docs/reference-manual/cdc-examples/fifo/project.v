// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: Version 0.1.7 (c6d52fc)
// Intended for use with yosys.

`default_nettype none

module JZHDL_LIB_CDC_FIFO__W64 (
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
    input [63:0] data_in;
    input write_en;
    input read_en;
    output [63:0] data_out;
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
    (* ram_style = "distributed" *) reg [63:0] mem[0:15];


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

module cdc_fifo (
    clk_a,
    clk_b,
    rst_n,
    leds
);
    // Ports
    input clk_a;
    input clk_b;
    input rst_n;
    output reg [5:0] leds;

    // Signals
    reg [63:0] packet_word;
    reg [63:0] data_out;
    wire [63:0] packet_view;


    JZHDL_LIB_CDC_FIFO__W64 u_cdc_fifo_packet_view (
        .clk_wr(clk_a),
        .clk_rd(clk_b),
        .rst_wr(rst_n),
        .rst_rd(rst_n),
        .data_in(packet_word),
        .write_en(1'b1),
        .read_en(1'b1),
        .data_out(packet_view)
    );


    always @* begin
        leds = data_out[5:0];
    end

    always @(posedge clk_a) begin
        if (!rst_n) begin
            packet_word <= 64'b0000000000000000000000000000000000000000000000000000000000000000;
        end
        else begin
            packet_word <= packet_word + 64'b0000000000000000000000000000000000000000000000000000000000000001;
        end
    end

    always @(posedge clk_b) begin
        if (!rst_n) begin
            data_out <= 64'b0000000000000000000000000000000000000000000000000000000000000000;
        end
        else begin
            data_out <= packet_view;
        end
    end
endmodule

module top (
    SCLK,
    DONE,
    KEY,
    LED
);
    input SCLK;
    input DONE;
    input KEY;
    output [5:0] LED;

    // Top-level logical→physical pin mapping
    //   cdc_fifo.clk_a -> SCLK (board 52)
    //   cdc_fifo.clk_b -> CLK_FAST (clock gen)
    //   cdc_fifo.rst_n -> KEY[0] (board 3)
    //   cdc_fifo.leds[5] -> LED[5] (board 16)
    //   cdc_fifo.leds[4] -> LED[4] (board 15)
    //   cdc_fifo.leds[3] -> LED[3] (board 14)
    //   cdc_fifo.leds[2] -> LED[2] (board 13)
    //   cdc_fifo.leds[1] -> LED[1] (board 11)
    //   cdc_fifo.leds[0] -> LED[0] (board 10)

    wire [5:0] jz_inv_leds;
    assign LED[5] = ~jz_inv_leds[5];
    assign LED[4] = ~jz_inv_leds[4];
    assign LED[3] = ~jz_inv_leds[3];
    assign LED[2] = ~jz_inv_leds[2];
    assign LED[1] = ~jz_inv_leds[1];
    assign LED[0] = ~jz_inv_leds[0];

    wire CLK_FAST;
    wire jz_unused_pll_LOCK_cg0_u0;
    wire jz_unused_pll_PHASE_cg0_u0;
    wire jz_unused_pll_DIV_cg0_u0;
    wire jz_unused_pll_DIV3_cg0_u0;

    // CLOCK_GEN PLL instantiation (from chip data)
    rPLL #(
    .DEVICE("GW1N-9C"),          // Specify your device
    .FCLKIN("26.998"),       // Input frequency in MHz
    .IDIV_SEL(2),           // IDIV: Input divider
    .FBDIV_SEL(7),         // FBDIV: Feedback divider
    .ODIV_SEL(8),           // ODIV: Output divider
    .PSDA_SEL("0000"),
    .DUTYDA_SEL("1000"),
    .DYN_IDIV_SEL("FALSE"),
    .DYN_FBDIV_SEL("FALSE"),
    .DYN_ODIV_SEL("FALSE"),
    .DYN_DA_EN("FALSE"),
    .DYN_SDIV_SEL(2),
    .CLKOUT_FT_DIR(1'b1),
    .CLKOUTP_FT_DIR(1'b1),
    .CLKOUT_DLY_STEP(0),
    .CLKOUTP_DLY_STEP(0),
    .CLKFB_SEL("internal"),
    .CLKOUT_BYPASS("FALSE"),
    .CLKOUTP_BYPASS("FALSE"),
    .CLKOUTD_BYPASS("FALSE"),
    .CLKOUTD_SRC("CLKOUT"),
    .CLKOUTD3_SRC("CLKOUT")
) u_pll_0_0 (
    .CLKOUT(CLK_FAST),   // Primary output
    .LOCK(jz_unused_pll_LOCK_cg0_u0),     // High when stable
    .CLKOUTP(jz_unused_pll_PHASE_cg0_u0), // Phase shifted output
    .CLKOUTD(jz_unused_pll_DIV_cg0_u0),   // Divided output
    .CLKOUTD3(jz_unused_pll_DIV3_cg0_u0), // Divided by 3 output
    .RESET(1'b0),        // Reset signal
    .RESET_P(1'b0),      // PLL power down
    .CLKIN(SCLK),  // Reference clock input
    .CLKFB(1'b0)         // External feedback
);

    cdc_fifo u_top (
        .clk_a(SCLK),
        .clk_b(CLK_FAST),
        .rst_n(KEY[0]),
        .leds({jz_inv_leds[5], jz_inv_leds[4], jz_inv_leds[3], jz_inv_leds[2], jz_inv_leds[1], jz_inv_leds[0]})
    );
endmodule
