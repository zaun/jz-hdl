// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: Version 0.1.7 (c6d52fc)
// Intended for use with yosys.

`default_nettype none

module JZHDL_LIB_CDC_BIT (
    clk_dest,
    data_in,
    data_out
);
    // Ports
    input clk_dest;
    input data_in;
    output data_out;

    // Signals
    reg sync_ff1;
    reg sync_ff2;

    assign data_out = sync_ff2;


    always @(posedge clk_dest) begin
        sync_ff1 <= data_in;
        sync_ff2 <= sync_ff1;
    end
endmodule

module cdc_bit (
    clk_a,
    clk_b,
    rst_n,
    trigger,
    led
);
    // Ports
    input clk_a;
    input clk_b;
    input rst_n;
    input trigger;
    output reg led;

    // Signals
    reg event_flag;
    reg cpu_seen;
    wire event_flag_sync;


    JZHDL_LIB_CDC_BIT u_cdc_bit_event_flag_sync (
        .clk_dest(clk_b),
        .data_in(event_flag),
        .data_out(event_flag_sync)
    );


    always @* begin
        led = cpu_seen;
    end

    always @(posedge clk_a) begin
        if (!rst_n) begin
            event_flag <= 1'b0;
        end
        else begin
            if (trigger) begin
                event_flag <= 1'b1;
            end
            else begin
                event_flag <= 1'b0;
            end
        end
    end

    always @(posedge clk_b) begin
        if (!rst_n) begin
            cpu_seen <= 1'b0;
        end
        else begin
            if (event_flag_sync) begin
                cpu_seen <= 1'b1;
            end
            else begin
                cpu_seen <= 1'b0;
            end
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
    input [1:0] KEY;
    output LED;

    // Top-level logical→physical pin mapping
    //   cdc_bit.clk_a -> SCLK (board 52)
    //   cdc_bit.clk_b -> CLK_FAST (clock gen)
    //   cdc_bit.rst_n -> KEY[0] (board 3)
    //   cdc_bit.trigger -> KEY[1] (board 4)
    //   cdc_bit.led -> LED[0] (board 10)

    wire jz_inv_led;
    assign LED[0] = ~jz_inv_led;

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

    cdc_bit u_top (
        .clk_a(SCLK),
        .clk_b(CLK_FAST),
        .rst_n(KEY[0]),
        .trigger(KEY[1]),
        .led(jz_inv_led)
    );
endmodule
