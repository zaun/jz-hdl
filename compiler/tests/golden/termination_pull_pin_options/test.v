// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module term_pull_module (
    clk,
    din,
    dq,
    button,
    sda,
    irq_in,
    data,
    status
);
    // Ports
    input clk;
    input din;
    input dq;
    input button;
    input sda;
    input irq_in;
    input data;
    output status;

    // Signals
    reg sampled;

    assign status = sampled;


    always @(posedge clk) begin
        sampled <= din & dq & button & sda & irq_in & data;
    end
endmodule

module top (
    SCLK,
    diff_in_p,
    diff_in_n,
    mem_dq,
    btn,
    i2c_sda,
    irq,
    data_in,
    led
);
    input SCLK;
    input diff_in_p;
    input diff_in_n;
    input mem_dq;
    input btn;
    input i2c_sda;
    input irq;
    input data_in;
    output led;

    // Top-level logical→physical pin mapping
    //   term_pull_module.clk -> SCLK (board 4)
    //   term_pull_module.din -> diff_in (board 10)
    //   term_pull_module.dq -> mem_dq (board 20)
    //   term_pull_module.button -> btn (board 21)
    //   term_pull_module.sda -> i2c_sda (board 22)
    //   term_pull_module.irq_in -> irq (board 23)
    //   term_pull_module.data -> data_in (board 24)
    //   term_pull_module.status -> led (board 15)



    wire jz_diff_diff_in;
    TLVDS_IBUF u_ibuf_diff_in (
    .I(diff_in_p),
    .IB(diff_in_n),
    .O(jz_diff_diff_in)
);

    term_pull_module u_top (
        .clk(SCLK),
        .din(jz_diff_diff_in),
        .dq(mem_dq),
        .button(btn),
        .sda(i2c_sda),
        .irq_in(irq),
        .data(data_in),
        .status(led)
    );
endmodule
