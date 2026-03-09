// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
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

module JZHDL_LIB_RESET_SYNC (
    clk,
    rst_async_n,
    rst_sync_n
);
    // Ports
    input clk;
    input rst_async_n;
    output rst_sync_n;

    // Signals
    reg sync_ff1;
    reg sync_ff2;

    assign rst_sync_n = sync_ff2;


    always @(posedge clk or negedge rst_async_n) begin
        if (!rst_async_n) begin
            sync_ff1 <= 1'b0;
            sync_ff2 <= 1'b0;
        end
        else begin
            sync_ff1 <= 1'b1;
            sync_ff2 <= sync_ff1;
        end
    end
endmodule

module JZHDL_LIB_CDC_BUS__W30 (
    clk_src,
    clk_dest,
    data_in,
    data_out
);
    // Ports
    input clk_src;
    input clk_dest;
    input [29:0] data_in;
    output reg [29:0] data_out;

    // Signals
    reg [29:0] gray_src;
    reg [29:0] gray_sync1;
    reg [29:0] gray_sync2;



    always @* begin
        data_out[29] = gray_sync2[29];
        data_out[28] = data_out[29] ^ gray_sync2[28];
        data_out[27] = data_out[28] ^ gray_sync2[27];
        data_out[26] = data_out[27] ^ gray_sync2[26];
        data_out[25] = data_out[26] ^ gray_sync2[25];
        data_out[24] = data_out[25] ^ gray_sync2[24];
        data_out[23] = data_out[24] ^ gray_sync2[23];
        data_out[22] = data_out[23] ^ gray_sync2[22];
        data_out[21] = data_out[22] ^ gray_sync2[21];
        data_out[20] = data_out[21] ^ gray_sync2[20];
        data_out[19] = data_out[20] ^ gray_sync2[19];
        data_out[18] = data_out[19] ^ gray_sync2[18];
        data_out[17] = data_out[18] ^ gray_sync2[17];
        data_out[16] = data_out[17] ^ gray_sync2[16];
        data_out[15] = data_out[16] ^ gray_sync2[15];
        data_out[14] = data_out[15] ^ gray_sync2[14];
        data_out[13] = data_out[14] ^ gray_sync2[13];
        data_out[12] = data_out[13] ^ gray_sync2[12];
        data_out[11] = data_out[12] ^ gray_sync2[11];
        data_out[10] = data_out[11] ^ gray_sync2[10];
        data_out[9] = data_out[10] ^ gray_sync2[9];
        data_out[8] = data_out[9] ^ gray_sync2[8];
        data_out[7] = data_out[8] ^ gray_sync2[7];
        data_out[6] = data_out[7] ^ gray_sync2[6];
        data_out[5] = data_out[6] ^ gray_sync2[5];
        data_out[4] = data_out[5] ^ gray_sync2[4];
        data_out[3] = data_out[4] ^ gray_sync2[3];
        data_out[2] = data_out[3] ^ gray_sync2[2];
        data_out[1] = data_out[2] ^ gray_sync2[1];
        data_out[0] = data_out[1] ^ gray_sync2[0];
    end

    always @(posedge clk_src) begin
        gray_src <= data_in >> 30'b000000000000000000000000000001 ^ data_in;
    end

    always @(posedge clk_dest) begin
        gray_sync1 <= gray_src;
        gray_sync2 <= gray_sync1;
    end
endmodule

module cdc_top (
    clk_fast,
    clk_slow,
    rst_n,
    por,
    display
);
    // Ports
    input clk_fast;
    input clk_slow;
    input rst_n;
    input por;
    output reg [2:0] display;

    // Signals
    wire reset;
    reg fast_is_even;
    reg slow_is_set;
    reg [29:0] fast_counter;
    reg slow_flag;
    reg sync_out;
    wire [29:0] counter_slow_view;
    wire flag_fast_view;
    wire rst_sync_0;

    assign reset = por & rst_n;

    JZHDL_LIB_CDC_BUS__W30 u_cdc_bus_counter_slow_view (
        .clk_src(clk_fast),
        .clk_dest(clk_slow),
        .data_in(fast_counter),
        .data_out(counter_slow_view)
    );
    JZHDL_LIB_CDC_BIT u_cdc_bit_flag_fast_view (
        .clk_dest(clk_fast),
        .data_in(slow_flag),
        .data_out(flag_fast_view)
    );
    JZHDL_LIB_RESET_SYNC u_rst_sync_0 (
        .clk(clk_fast),
        .rst_async_n(reset),
        .rst_sync_n(rst_sync_0)
    );


    always @* begin
        fast_is_even = fast_counter[0] == 1'b0;
        slow_is_set = slow_flag == 1'b1;
        display = {fast_counter[29], fast_is_even, slow_is_set};
    end

    always @(posedge clk_fast) begin
        if (!rst_sync_0) begin
            fast_counter <= 30'b000000000000000000000000000000;
            sync_out <= 1'b0;
        end
        else begin
            fast_counter <= fast_counter + 30'b000000000000000000000000000001;
            if (flag_fast_view == 1'b1) begin
                sync_out <= 1'b1;
            end
            else begin
                sync_out <= 1'b0;
            end
        end
    end

    always @(posedge clk_slow) begin
        if (!reset) begin
            slow_flag <= 1'b0;
        end
        else begin
            if (counter_slow_view > 8'b10000000) begin
                slow_flag <= 1'b1;
            end
            else begin
                slow_flag <= 1'b0;
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
    output [2:0] LED;

    // Top-level logical→physical pin mapping
    //   cdc_top.clk_fast -> FAST_CLK (clock gen)
    //   cdc_top.clk_slow -> SCLK (board 4)
    //   cdc_top.rst_n -> KEY[0] (board 88)
    //   cdc_top.por -> DONE (board IOR32B)
    //   cdc_top.display[2] -> LED[2] (board 17)
    //   cdc_top.display[1] -> LED[1] (board 16)
    //   cdc_top.display[0] -> LED[0] (board 15)


    wire FAST_CLK;
    wire jz_unused_pll_LOCK_cg0_u0;
    wire jz_unused_pll_PHASE_cg0_u0;
    wire jz_unused_pll_DIV_cg0_u0;
    wire jz_unused_pll_DIV3_cg0_u0;

    // CLOCK_GEN PLL instantiation (from chip data)
    rPLL #(
    .DEVICE("GW2AR-18"),         // Specify your device
    .FCLKIN("100.000"),       // Input frequency in MHz
    .IDIV_SEL(3),           // IDIV: Input divider
    .FBDIV_SEL(8),         // FBDIV: Feedback divider
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
    .CLKOUT(FAST_CLK),   // Primary output
    .LOCK(jz_unused_pll_LOCK_cg0_u0),     // High when stable
    .CLKOUTP(jz_unused_pll_PHASE_cg0_u0), // Phase shifted output
    .CLKOUTD(jz_unused_pll_DIV_cg0_u0),   // Divided output
    .CLKOUTD3(jz_unused_pll_DIV3_cg0_u0), // Divided by 3 output
    .RESET(1'b0),        // Reset signal
    .RESET_P(1'b0),      // PLL power down
    .CLKIN(SCLK),  // Reference clock input
    .CLKFB(1'b0)         // External feedback
);

    cdc_top u_top (
        .clk_fast(FAST_CLK),
        .clk_slow(SCLK),
        .rst_n(~KEY[0]),
        .por(DONE),
        .display({LED[2], LED[1], LED[0]})
    );
endmodule
