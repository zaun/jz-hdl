// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module dual_driver (
    sel,
    port_a,
    port_b
);
    // Ports
    input sel;
    inout [7:0] port_a;
    inout [7:0] port_b;

    // Signals


    assign port_a = sel == 1'b1 ? 8'b10101010 : 8'bzzzzzzzz;
    assign port_b = sel == 1'b0 ? 8'b10111011 : 8'bzzzzzzzz;

    always @* begin
    end
endmodule

module other_driver (
    sel,
    data
);
    // Ports
    input sel;
    inout [7:0] data;

    // Signals


    assign data = sel == 1'b1 ? 8'b11001100 : 8'bzzzzzzzz;

    always @* begin
    end
endmodule

module top_mod (
    clk,
    sel,
    data
);
    // Ports
    input clk;
    input sel;
    inout [7:0] data;

    // Signals


    dual_driver drv0 (
        .sel(sel),
        .port_a(data),
        .port_b(data)
    );
    other_driver drv1 (
        .sel(sel),
        .data(data)
    );

endmodule

module top (
    clk_pin,
    sel_pin,
    data_pin
);
    input clk_pin;
    input sel_pin;
    inout [7:0] data_pin;

    // Top-level logical→physical pin mapping
    //   top_mod.clk -> clk_pin (board 4)
    //   top_mod.sel -> sel_pin (board 5)
    //   top_mod.data[7] -> data_pin[7] (board 17)
    //   top_mod.data[6] -> data_pin[6] (board 16)
    //   top_mod.data[5] -> data_pin[5] (board 15)
    //   top_mod.data[4] -> data_pin[4] (board 14)
    //   top_mod.data[3] -> data_pin[3] (board 13)
    //   top_mod.data[2] -> data_pin[2] (board 12)
    //   top_mod.data[1] -> data_pin[1] (board 11)
    //   top_mod.data[0] -> data_pin[0] (board 10)



    top_mod u_top (
        .clk(clk_pin),
        .sel(sel_pin),
        .data({data_pin[7], data_pin[6], data_pin[5], data_pin[4], data_pin[3], data_pin[2], data_pin[1], data_pin[0]})
    );
endmodule
