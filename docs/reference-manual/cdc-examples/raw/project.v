// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: Version 0.1.7 (c6d52fc)
// Intended for use with yosys.

`default_nettype none

module cdc_raw (
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
    reg [15:0] config_word;
    reg [15:0] local_config;
    wire [15:0] config_view;

    assign config_view = config_word;


    always @* begin
        leds = local_config[5:0];
    end

    always @(posedge clk_a) begin
        if (!rst_n) begin
            config_word <= 16'b1010010110100101;
        end
        else begin
            config_word <= config_word;
        end
    end

    always @(posedge clk_b) begin
        if (!rst_n) begin
            local_config <= 16'b0000000000000000;
        end
        else begin
            local_config <= config_view;
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
    //   cdc_raw.clk_a -> SCLK (board 52)
    //   cdc_raw.clk_b -> CLK_FAST (clock gen)
    //   cdc_raw.rst_n -> KEY[0] (board 3)
    //   cdc_raw.leds[5] -> LED[5] (board 16)
    //   cdc_raw.leds[4] -> LED[4] (board 15)
    //   cdc_raw.leds[3] -> LED[3] (board 14)
    //   cdc_raw.leds[2] -> LED[2] (board 13)
    //   cdc_raw.leds[1] -> LED[1] (board 11)
    //   cdc_raw.leds[0] -> LED[0] (board 10)

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

    cdc_raw u_top (
        .clk_a(SCLK),
        .clk_b(CLK_FAST),
        .rst_n(KEY[0]),
        .leds({jz_inv_leds[5], jz_inv_leds[4], jz_inv_leds[3], jz_inv_leds[2], jz_inv_leds[1], jz_inv_leds[0]})
    );
endmodule
