// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: Version 0.1.5 (aa10d91)
// Intended for use with yosys.

`default_nettype none

module multi_pll_top (
    clk_a,
    clk_b,
    clk_c,
    clk_d,
    rst_n,
    leds
);
    // Ports
    input clk_a;
    input clk_b;
    input clk_c;
    input clk_d;
    input rst_n;
    output reg [3:0] leds;

    // Signals
    reg [23:0] cnt_a;
    reg [23:0] cnt_b;
    reg [23:0] cnt_c;
    reg [23:0] cnt_d;



    always @* begin
        leds[0] = cnt_a[23];
        leds[1] = cnt_b[23];
        leds[2] = cnt_c[23];
        leds[3] = cnt_d[23];
    end

    always @(posedge clk_a) begin
        if (!rst_n) begin
            cnt_a <= 24'b000000000000000000000000;
        end
        else begin
            cnt_a <= cnt_a + 24'b000000000000000000000001;
        end
    end

    always @(posedge clk_b) begin
        if (!rst_n) begin
            cnt_b <= 24'b000000000000000000000000;
        end
        else begin
            cnt_b <= cnt_b + 24'b000000000000000000000001;
        end
    end

    always @(posedge clk_c) begin
        if (!rst_n) begin
            cnt_c <= 24'b000000000000000000000000;
        end
        else begin
            cnt_c <= cnt_c + 24'b000000000000000000000001;
        end
    end

    always @(posedge clk_d) begin
        if (!rst_n) begin
            cnt_d <= 24'b000000000000000000000000;
        end
        else begin
            cnt_d <= cnt_d + 24'b000000000000000000000001;
        end
    end
endmodule

module top (
    SCLK_p,
    SCLK_n,
    KEY,
    LED
);
    input SCLK_p;
    input SCLK_n;
    input KEY;
    output [3:0] LED;

    // Top-level logical→physical pin mapping
    //   multi_pll_top.clk_a -> CLK_A (clock gen)
    //   multi_pll_top.clk_b -> CLK_B (clock gen)
    //   multi_pll_top.clk_c -> CLK_C (clock gen)
    //   multi_pll_top.clk_d -> CLK_D (clock gen)
    //   multi_pll_top.rst_n -> KEY[0] (board W21)
    //   multi_pll_top.leds[3] -> LED[3] (board K18)
    //   multi_pll_top.leds[2] -> LED[2] (board H19)
    //   multi_pll_top.leds[1] -> LED[1] (board J19)
    //   multi_pll_top.leds[0] -> LED[0] (board J21)


    wire CLK_A;
    wire CLK_B;
    wire CLK_C;
    wire CLK_D;
    wire jz_unused_pll_LOCK_cg0_u0;
    wire clkfb_0_0;

    // CLOCK_GEN PLL instantiation (from chip data)
    PLLE2_BASE #(
    .BANDWIDTH("OPTIMIZED"),
    .CLKFBOUT_MULT(5),
    .CLKFBOUT_PHASE(0.0),
    .CLKIN1_PERIOD(5.000),
    .CLKOUT0_DIVIDE(4),
    .CLKOUT0_PHASE(0.0),
    .CLKOUT0_DUTY_CYCLE(0.5),
    .CLKOUT1_DIVIDE(1),
    .CLKOUT1_PHASE(0.0),
    .CLKOUT1_DUTY_CYCLE(0.5),
    .CLKOUT2_DIVIDE(1),
    .CLKOUT2_PHASE(0.0),
    .CLKOUT2_DUTY_CYCLE(0.5),
    .CLKOUT3_DIVIDE(1),
    .CLKOUT3_PHASE(0.0),
    .CLKOUT3_DUTY_CYCLE(0.5),
    .CLKOUT4_DIVIDE(1),
    .CLKOUT4_PHASE(0.0),
    .CLKOUT4_DUTY_CYCLE(0.5),
    .CLKOUT5_DIVIDE(1),
    .CLKOUT5_PHASE(0.0),
    .CLKOUT5_DUTY_CYCLE(0.5),
    .DIVCLK_DIVIDE(1),
    .STARTUP_WAIT("FALSE")
) u_pll_0_0 (
    .CLKIN1(jz_diff_SCLK),
    .CLKFBIN(clkfb_0_0),
    .CLKFBOUT(clkfb_0_0),
    .CLKOUT0(CLK_A),
    .CLKOUT1(/* CLKOUT1 */),
    .CLKOUT2(/* CLKOUT2 */),
    .CLKOUT3(/* CLKOUT3 */),
    .CLKOUT4(/* CLKOUT4 */),
    .CLKOUT5(/* CLKOUT5 */),
    .LOCKED(jz_unused_pll_LOCK_cg0_u0),
    .RST(1'b0),
    .PWRDWN(1'b0)
);
    wire jz_unused_pll_LOCK_cg0_u1;
    wire clkfb_0_1;

    // CLOCK_GEN PLL instantiation (from chip data)
    PLLE2_BASE #(
    .BANDWIDTH("OPTIMIZED"),
    .CLKFBOUT_MULT(5),
    .CLKFBOUT_PHASE(0.0),
    .CLKIN1_PERIOD(5.000),
    .CLKOUT0_DIVIDE(10),
    .CLKOUT0_PHASE(0.0),
    .CLKOUT0_DUTY_CYCLE(0.5),
    .CLKOUT1_DIVIDE(1),
    .CLKOUT1_PHASE(0.0),
    .CLKOUT1_DUTY_CYCLE(0.5),
    .CLKOUT2_DIVIDE(1),
    .CLKOUT2_PHASE(0.0),
    .CLKOUT2_DUTY_CYCLE(0.5),
    .CLKOUT3_DIVIDE(1),
    .CLKOUT3_PHASE(0.0),
    .CLKOUT3_DUTY_CYCLE(0.5),
    .CLKOUT4_DIVIDE(1),
    .CLKOUT4_PHASE(0.0),
    .CLKOUT4_DUTY_CYCLE(0.5),
    .CLKOUT5_DIVIDE(1),
    .CLKOUT5_PHASE(0.0),
    .CLKOUT5_DUTY_CYCLE(0.5),
    .DIVCLK_DIVIDE(1),
    .STARTUP_WAIT("FALSE")
) u_pll_0_1 (
    .CLKIN1(jz_diff_SCLK),
    .CLKFBIN(clkfb_0_1),
    .CLKFBOUT(clkfb_0_1),
    .CLKOUT0(CLK_B),
    .CLKOUT1(/* CLKOUT1 */),
    .CLKOUT2(/* CLKOUT2 */),
    .CLKOUT3(/* CLKOUT3 */),
    .CLKOUT4(/* CLKOUT4 */),
    .CLKOUT5(/* CLKOUT5 */),
    .LOCKED(jz_unused_pll_LOCK_cg0_u1),
    .RST(1'b0),
    .PWRDWN(1'b0)
);
    wire jz_unused_pll_LOCK_cg0_u2;
    wire clkfb_0_2;

    // CLOCK_GEN PLL2 instantiation (from chip data)
    MMCME2_BASE #(
    .BANDWIDTH("OPTIMIZED"),
    .CLKFBOUT_MULT_F(5.125),
    .CLKFBOUT_PHASE(0.0),
    .CLKIN1_PERIOD(5.000),
    .CLKOUT0_DIVIDE_F(5.25),
    .CLKOUT0_PHASE(0.0),
    .CLKOUT0_DUTY_CYCLE(0.5),
    .CLKOUT1_DIVIDE(8),
    .CLKOUT1_PHASE(0.0),
    .CLKOUT1_DUTY_CYCLE(0.5),
    .CLKOUT2_DIVIDE(1),
    .CLKOUT2_PHASE(0.0),
    .CLKOUT2_DUTY_CYCLE(0.5),
    .CLKOUT3_DIVIDE(1),
    .CLKOUT3_PHASE(0.0),
    .CLKOUT3_DUTY_CYCLE(0.5),
    .CLKOUT4_DIVIDE(1),
    .CLKOUT4_PHASE(0.0),
    .CLKOUT4_DUTY_CYCLE(0.5),
    .CLKOUT5_DIVIDE(1),
    .CLKOUT5_PHASE(0.0),
    .CLKOUT5_DUTY_CYCLE(0.5),
    .CLKOUT6_DIVIDE(1),
    .CLKOUT6_PHASE(0.0),
    .CLKOUT6_DUTY_CYCLE(0.5),
    .DIVCLK_DIVIDE(1),
    .STARTUP_WAIT("FALSE")
) u_mmcm_0_2 (
    .CLKIN1(jz_diff_SCLK),
    .CLKFBIN(clkfb_0_2),
    .CLKFBOUT(clkfb_0_2),
    .CLKOUT0(CLK_C),
    .CLKOUT1(CLK_D),
    .CLKOUT2(/* CLKOUT2 */),
    .CLKOUT3(/* CLKOUT3 */),
    .CLKOUT4(/* CLKOUT4 */),
    .CLKOUT5(/* CLKOUT5 */),
    .CLKOUT6(/* CLKOUT6 */),
    .LOCKED(jz_unused_pll_LOCK_cg0_u2),
    .RST(1'b0),
    .PWRDWN(1'b0)
);

    wire jz_diff_SCLK;
    wire jz_ibuf_ibuf_SCLK;
IBUFDS #(
    .DIFF_TERM("FALSE"),
    .IBUF_LOW_PWR("FALSE")
) u_ibuf_SCLK (
    .I(SCLK_p),
    .IB(SCLK_n),
    .O(jz_ibuf_ibuf_SCLK)
);
BUFG u_bufg_ibuf_SCLK (
    .I(jz_ibuf_ibuf_SCLK),
    .O(jz_diff_SCLK)
);

    multi_pll_top u_top (
        .clk_a(CLK_A),
        .clk_b(CLK_B),
        .clk_c(CLK_C),
        .clk_d(CLK_D),
        .rst_n(~KEY[0]),
        .leds({LED[3], LED[2], LED[1], LED[0]})
    );
endmodule
