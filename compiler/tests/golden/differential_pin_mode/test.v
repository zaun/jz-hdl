// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module diff_module (
    clk,
    din,
    rx_data,
    status
);
    // Ports
    input clk;
    input din;
    input [1:0] rx_data;
    output status;

    // Signals
    reg sampled;

    assign status = sampled;


    always @(posedge clk) begin
        sampled <= din & rx_data[0] & rx_data[1];
    end
endmodule

module top (
    SCLK,
    diff_in_p,
    diff_in_n,
    lvds_rx0_p,
    lvds_rx0_n,
    lvds_rx1_p,
    lvds_rx1_n,
    led
);
    input SCLK;
    input diff_in_p;
    input diff_in_n;
    input lvds_rx0_p;
    input lvds_rx0_n;
    input lvds_rx1_p;
    input lvds_rx1_n;
    output led;

    // Top-level logical→physical pin mapping
    //   diff_module.clk -> SCLK (board 4)
    //   diff_module.din -> diff_in (board 10)
    //   diff_module.rx_data[1] -> lvds_rx[1] (board 27)
    //   diff_module.rx_data[0] -> lvds_rx[0] (board 25)
    //   diff_module.status -> led (board 15)



    wire jz_diff_diff_in;
    TLVDS_IBUF u_ibuf_diff_in (
    .I(diff_in_p),
    .IB(diff_in_n),
    .O(jz_diff_diff_in)
);

    wire jz_diff_lvds_rx0;
    TLVDS_IBUF u_ibuf_lvds_rx0 (
    .I(lvds_rx0_p),
    .IB(lvds_rx0_n),
    .O(jz_diff_lvds_rx0)
);
    wire jz_diff_lvds_rx1;
    TLVDS_IBUF u_ibuf_lvds_rx1 (
    .I(lvds_rx1_p),
    .IB(lvds_rx1_n),
    .O(jz_diff_lvds_rx1)
);

    diff_module u_top (
        .clk(SCLK),
        .din(jz_diff_diff_in),
        .rx_data(jz_diff_lvds_rx0),
        .status(led)
    );
endmodule
