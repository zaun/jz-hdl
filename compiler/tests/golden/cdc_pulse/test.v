// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module JZHDL_LIB_CDC_PULSE (
    clk_src,
    clk_dest,
    pulse_in,
    pulse_out
);
    // Ports
    input clk_src;
    input clk_dest;
    input pulse_in;
    output pulse_out;

    // Signals
    reg toggle_src;
    reg toggle_sync1;
    reg toggle_sync2;
    reg toggle_last;

    assign pulse_out = toggle_sync2 ^ toggle_last;


    always @(posedge clk_src) begin
        if (pulse_in) begin
            toggle_src <= ~toggle_src;
        end
    end

    always @(posedge clk_dest) begin
        toggle_sync1 <= toggle_src;
        toggle_sync2 <= toggle_sync1;
        toggle_last <= toggle_sync2;
    end
endmodule

module cdc_pulse_test (
    clk_a,
    clk_b,
    q
);
    // Ports
    input clk_a;
    input clk_b;
    output q;

    // Signals
    reg flag;
    reg dest_reg;


    JZHDL_LIB_CDC_PULSE u_cdc_pulse_flag_sync (
        .clk_src(clk_a),
        .clk_dest(clk_b),
        .pulse_in(flag),
        .pulse_out(q)
    );


    always @(posedge clk_a) begin
        flag <= ~flag;
    end

    always @(posedge clk_b) begin
        dest_reg <= q;
    end
endmodule
