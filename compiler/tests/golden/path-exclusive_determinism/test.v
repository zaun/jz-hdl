// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module ped_logic_module (
    sel,
    in_a,
    in_b,
    logic_out,
    indicator
);
    // Ports
    input [1:0] sel;
    input [7:0] in_a;
    input [7:0] in_b;
    output reg [7:0] logic_out;
    output reg indicator;

    // Signals



    always @* begin
        if (sel == 2'b00) begin
            logic_out = in_a;
        end
        else if (sel == 2'b01) begin
            logic_out = in_b;
        end
        else begin
            if (in_a > in_b) begin
                logic_out = in_a;
            end
            else begin
                logic_out = 8'b11111111;
            end
        end
        case (sel)
            2'b00: begin
                indicator = 1'b0;
            end
            2'b01,
            2'b10: begin
                indicator = 1'b1;
            end
            default: begin
                indicator = in_a[0];
            end
        endcase
    end
endmodule

module top (
    mode_sel,
    data_a,
    data_b,
    result,
    status_led
);
    input [1:0] mode_sel;
    input [7:0] data_a;
    input [7:0] data_b;
    output [7:0] result;
    output status_led;

    // Top-level logical→physical pin mapping
    //   ped_logic_module.sel[1] -> mode_sel[1] (board 3)
    //   ped_logic_module.sel[0] -> mode_sel[0] (board 2)
    //   ped_logic_module.in_a[7] -> data_a[7] (board 27)
    //   ped_logic_module.in_a[6] -> data_a[6] (board 26)
    //   ped_logic_module.in_a[5] -> data_a[5] (board 25)
    //   ped_logic_module.in_a[4] -> data_a[4] (board 24)
    //   ped_logic_module.in_a[3] -> data_a[3] (board 23)
    //   ped_logic_module.in_a[2] -> data_a[2] (board 22)
    //   ped_logic_module.in_a[1] -> data_a[1] (board 21)
    //   ped_logic_module.in_a[0] -> data_a[0] (board 20)
    //   ped_logic_module.in_b[7] -> data_b[7] (board 37)
    //   ped_logic_module.in_b[6] -> data_b[6] (board 36)
    //   ped_logic_module.in_b[5] -> data_b[5] (board 35)
    //   ped_logic_module.in_b[4] -> data_b[4] (board 34)
    //   ped_logic_module.in_b[3] -> data_b[3] (board 33)
    //   ped_logic_module.in_b[2] -> data_b[2] (board 32)
    //   ped_logic_module.in_b[1] -> data_b[1] (board 31)
    //   ped_logic_module.in_b[0] -> data_b[0] (board 30)
    //   ped_logic_module.logic_out[7] -> result[7] (board 47)
    //   ped_logic_module.logic_out[6] -> result[6] (board 46)
    //   ped_logic_module.logic_out[5] -> result[5] (board 45)
    //   ped_logic_module.logic_out[4] -> result[4] (board 44)
    //   ped_logic_module.logic_out[3] -> result[3] (board 43)
    //   ped_logic_module.logic_out[2] -> result[2] (board 42)
    //   ped_logic_module.logic_out[1] -> result[1] (board 41)
    //   ped_logic_module.logic_out[0] -> result[0] (board 40)
    //   ped_logic_module.indicator -> status_led (board 10)



    ped_logic_module u_top (
        .sel({mode_sel[1], mode_sel[0]}),
        .in_a({data_a[7], data_a[6], data_a[5], data_a[4], data_a[3], data_a[2], data_a[1], data_a[0]}),
        .in_b({data_b[7], data_b[6], data_b[5], data_b[4], data_b[3], data_b[2], data_b[1], data_b[0]}),
        .logic_out({result[7], result[6], result[5], result[4], result[3], result[2], result[1], result[0]}),
        .indicator(status_led)
    );
endmodule
