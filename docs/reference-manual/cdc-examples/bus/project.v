// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: Version 0.1.7 (c6d52fc)
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

module cdc_bus (
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
    reg [7:0] gray_ptr;
    reg [7:0] read_ptr;
    wire [7:0] gray_ptr_sync;


    JZHDL_LIB_CDC_BUS__W8 u_cdc_bus_gray_ptr_sync (
        .clk_src(clk_a),
        .clk_dest(clk_b),
        .data_in(gray_ptr),
        .data_out(gray_ptr_sync)
    );


    always @* begin
        leds = read_ptr[5:0];
    end

    always @(posedge clk_a) begin
        if (!rst_n) begin
            gray_ptr <= 8'b00000000;
        end
        else begin
            gray_ptr <= gray_ptr + 8'b00000001;
        end
    end

    always @(posedge clk_b) begin
        if (!rst_n) begin
            read_ptr <= 8'b00000000;
        end
        else begin
            read_ptr <= gray_ptr_sync;
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
    //   cdc_bus.clk_a -> SCLK (board 52)
    //   cdc_bus.clk_b -> CLK_FAST (clock gen)
    //   cdc_bus.rst_n -> KEY[0] (board 3)
    //   cdc_bus.leds[5] -> LED[5] (board 16)
    //   cdc_bus.leds[4] -> LED[4] (board 15)
    //   cdc_bus.leds[3] -> LED[3] (board 14)
    //   cdc_bus.leds[2] -> LED[2] (board 13)
    //   cdc_bus.leds[1] -> LED[1] (board 11)
    //   cdc_bus.leds[0] -> LED[0] (board 10)

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

    cdc_bus u_top (
        .clk_a(SCLK),
        .clk_b(CLK_FAST),
        .rst_n(KEY[0]),
        .leds({jz_inv_leds[5], jz_inv_leds[4], jz_inv_leds[3], jz_inv_leds[2], jz_inv_leds[1], jz_inv_leds[0]})
    );
endmodule
