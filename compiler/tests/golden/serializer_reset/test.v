// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: Version 0.1.5 (2b1ca2d)
// Intended for use with yosys.

`default_nettype none

module ser_top (
    clk,
    status,
    tclk,
    td0,
    td1
);
    // Ports
    input clk;
    output reg status;
    output reg tclk;
    output reg td0;
    output reg td1;

    // Signals
    reg [23:0] cnt;



    always @* begin
        status = cnt[23];
        tclk = 1'b1;
        td0 = 1'b0;
        td1 = 1'b0;
    end

    always @(posedge clk) begin
        cnt <= cnt + 24'b000000000000000000000001;
    end
endmodule

module top (
    SCLK,
    led,
    TMDS_CLK_p,
    TMDS_CLK_n,
    TMDS_D0_p,
    TMDS_D0_n,
    TMDS_D1_p,
    TMDS_D1_n
);
    input SCLK;
    output led;
    output TMDS_CLK_p;
    output TMDS_CLK_n;
    output TMDS_D0_p;
    output TMDS_D0_n;
    output TMDS_D1_p;
    output TMDS_D1_n;

    // Top-level logical→physical pin mapping
    //   ser_top.clk -> pixel_clk (clock gen)
    //   ser_top.status -> led (board 10)
    //   ser_top.tclk -> TMDS_CLK (board 69)
    //   ser_top.td0 -> TMDS_D[0] (board 71)
    //   ser_top.td1 -> TMDS_D[1] (board 73)


    wire serial_clk;
    wire pll_lock;
    wire pixel_clk;
    wire jz_unused_pll_PHASE_cg0_u0;
    wire jz_unused_pll_DIV_cg0_u0;
    wire jz_unused_pll_DIV3_cg0_u0;

    // CLOCK_GEN PLL instantiation (from chip data)
    rPLL #(
    .DEVICE("GW1N-9C"),          // Specify your device
    .FCLKIN("27.000"),       // Input frequency in MHz
    .IDIV_SEL(7),           // IDIV: Input divider
    .FBDIV_SEL(54),         // FBDIV: Feedback divider
    .ODIV_SEL(4),           // ODIV: Output divider
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
    .CLKOUT(serial_clk),   // Primary output
    .LOCK(pll_lock),     // High when stable
    .CLKOUTP(jz_unused_pll_PHASE_cg0_u0), // Phase shifted output
    .CLKOUTD(jz_unused_pll_DIV_cg0_u0),   // Divided output
    .CLKOUTD3(jz_unused_pll_DIV3_cg0_u0), // Divided by 3 output
    .RESET(1'b0),        // Reset signal
    .RESET_P(1'b0),      // PLL power down
    .CLKIN(SCLK),  // Reference clock input
    .CLKFB(1'b0)         // External feedback
);

    // CLOCK_GEN CLKDIV instantiation (from chip data)
    CLKDIV #(
    .DIV_MODE("5"),
    .GSREN("false")
) u_clkdiv_0_1 (
    .HCLKIN(serial_clk),
    .RESETN(1'b1),
    .CALIB(1'b0),
    .CLKOUT(pixel_clk)
);

    wire [3:0] jz_diff_TMDS_CLK;
    wire jz_ser_TMDS_CLK;
    OSER4 #(
    .GSREN("FALSE"),
    .LSREN("TRUE"),
    .HWL("false"),
    .TXCLK_POL(1'b0)
) u_oser_TMDS_CLK (
    .D0(jz_diff_TMDS_CLK[0]),
    .D1(jz_diff_TMDS_CLK[1]),
    .D2(jz_diff_TMDS_CLK[2]),
    .D3(jz_diff_TMDS_CLK[3]),
    .TX0(1'b0),
    .TX1(1'b0),
    .FCLK(serial_clk),
    .PCLK(pixel_clk),
    .RESET(~pll_lock),
    .Q0(jz_ser_TMDS_CLK),
    .Q1()
);
    ELVDS_OBUF u_obuf_TMDS_CLK (
    .I(jz_ser_TMDS_CLK),
    .O(TMDS_CLK_p),
    .OB(TMDS_CLK_n)
);

    wire [3:0] jz_diff_TMDS_D0;
    wire jz_ser_TMDS_D0;
    OSER4 #(
    .GSREN("FALSE"),
    .LSREN("TRUE"),
    .HWL("false"),
    .TXCLK_POL(1'b0)
) u_oser_TMDS_D0 (
    .D0(jz_diff_TMDS_D0[0]),
    .D1(jz_diff_TMDS_D0[1]),
    .D2(jz_diff_TMDS_D0[2]),
    .D3(jz_diff_TMDS_D0[3]),
    .TX0(1'b0),
    .TX1(1'b0),
    .FCLK(serial_clk),
    .PCLK(pixel_clk),
    .RESET(~pll_lock),
    .Q0(jz_ser_TMDS_D0),
    .Q1()
);
    ELVDS_OBUF u_obuf_TMDS_D0 (
    .I(jz_ser_TMDS_D0),
    .O(TMDS_D0_p),
    .OB(TMDS_D0_n)
);
    wire [3:0] jz_diff_TMDS_D1;
    wire jz_ser_TMDS_D1;
    OSER4 #(
    .GSREN("FALSE"),
    .LSREN("TRUE"),
    .HWL("false"),
    .TXCLK_POL(1'b0)
) u_oser_TMDS_D1 (
    .D0(jz_diff_TMDS_D1[0]),
    .D1(jz_diff_TMDS_D1[1]),
    .D2(jz_diff_TMDS_D1[2]),
    .D3(jz_diff_TMDS_D1[3]),
    .TX0(1'b0),
    .TX1(1'b0),
    .FCLK(serial_clk),
    .PCLK(pixel_clk),
    .RESET(~pll_lock),
    .Q0(jz_ser_TMDS_D1),
    .Q1()
);
    ELVDS_OBUF u_obuf_TMDS_D1 (
    .I(jz_ser_TMDS_D1),
    .O(TMDS_D1_p),
    .OB(TMDS_D1_n)
);

    ser_top u_top (
        .clk(pixel_clk),
        .status(led),
        .tclk(jz_diff_TMDS_CLK),
        .td0(jz_diff_TMDS_D0),
        .td1(jz_diff_TMDS_D1)
    );
endmodule
