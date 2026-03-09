// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module tristate_device (
    clk,
    bus_sel,
    my_sel,
    data
);
    // Ports
    input clk;
    input [1:0] bus_sel;
    input [1:0] my_sel;
    output reg [7:0] data;

    // Signals
    reg [7:0] counter;



    always @* begin
        data = bus_sel == my_sel ? counter : 8'bzzzzzzzz;
    end

    always @(posedge clk) begin
        counter <= counter + 8'b00000001;
    end
endmodule

module register_file (
    clk,
    rd_sel,
    rd_data,
    wr_en,
    wr_sel,
    wr_data
);
    // Ports
    input clk;
    input [1:0] rd_sel;
    output reg [7:0] rd_data;
    input wr_en;
    input [1:0] wr_sel;
    input [7:0] wr_data;

    // Signals
    reg [7:0] zero_val;
    reg [7:0] r1;
    reg [7:0] r2;
    reg [7:0] r3;



    always @* begin
        zero_val = 8'b00000000;
        rd_data = (({r3, r2, r1, zero_val} >> (rd_sel * 8)) & {8{1'b1}});
    end

    always @(posedge clk) begin
        if (wr_en == 1'b1) begin
            case (wr_sel)
                2'b01: begin
                    r1 <= wr_data;
                end
                2'b10: begin
                    r2 <= wr_data;
                end
                2'b11: begin
                    r3 <= wr_data;
                end
                default: begin end
            endcase
        end
    end
endmodule

module top_mod (
    clk,
    sel,
    wr_en,
    wr_data,
    leds
);
    // Ports
    input clk;
    input [1:0] sel;
    input wr_en;
    input [7:0] wr_data;
    output [7:0] leds;

    // Signals
    wire [7:0] global_data;
    wire [7:0] local_data;
    wire [7:0] rf_out;

    assign leds = rf_out ^ global_data ^ local_data;

    tristate_device dev_global_a (
        .clk(clk),
        .bus_sel(sel),
        .my_sel(2'b00),
        .data(global_data)
    );
    tristate_device dev_global_b (
        .clk(clk),
        .bus_sel(sel),
        .my_sel(2'b01),
        .data(global_data)
    );
    tristate_device dev_global_c (
        .clk(clk),
        .bus_sel(sel),
        .my_sel(2'b10),
        .data(global_data)
    );
    tristate_device dev_local_a (
        .clk(clk),
        .bus_sel(sel),
        .data(local_data)
    );
    tristate_device dev_local_b (
        .clk(clk),
        .bus_sel(sel),
        .data(local_data)
    );
    register_file regfile (
        .clk(clk),
        .rd_sel(sel),
        .rd_data(rf_out),
        .wr_en(wr_en),
        .wr_sel(sel),
        .wr_data(wr_data)
    );

endmodule

module top (
    MCLK,
    SEL,
    WR_EN,
    WR_DATA,
    LEDS
);
    input MCLK;
    input [1:0] SEL;
    input WR_EN;
    input [7:0] WR_DATA;
    output [7:0] LEDS;

    // Top-level logical→physical pin mapping
    //   top_mod.clk -> MCLK (board 4)
    //   top_mod.sel[1] -> SEL[1] (board 88)
    //   top_mod.sel[0] -> SEL[0] (board 87)
    //   top_mod.wr_en -> WR_EN (board 86)
    //   top_mod.wr_data[7] -> WR_DATA[7] (board 81)
    //   top_mod.wr_data[6] -> WR_DATA[6] (board 80)
    //   top_mod.wr_data[5] -> WR_DATA[5] (board 79)
    //   top_mod.wr_data[4] -> WR_DATA[4] (board 77)
    //   top_mod.wr_data[3] -> WR_DATA[3] (board 76)
    //   top_mod.wr_data[2] -> WR_DATA[2] (board 75)
    //   top_mod.wr_data[1] -> WR_DATA[1] (board 74)
    //   top_mod.wr_data[0] -> WR_DATA[0] (board 73)
    //   top_mod.leds[7] -> LEDS[7] (board 22)
    //   top_mod.leds[6] -> LEDS[6] (board 21)
    //   top_mod.leds[5] -> LEDS[5] (board 20)
    //   top_mod.leds[4] -> LEDS[4] (board 19)
    //   top_mod.leds[3] -> LEDS[3] (board 18)
    //   top_mod.leds[2] -> LEDS[2] (board 17)
    //   top_mod.leds[1] -> LEDS[1] (board 16)
    //   top_mod.leds[0] -> LEDS[0] (board 15)



    top_mod u_top (
        .clk(MCLK),
        .sel({SEL[1], SEL[0]}),
        .wr_en(WR_EN),
        .wr_data({WR_DATA[7], WR_DATA[6], WR_DATA[5], WR_DATA[4], WR_DATA[3], WR_DATA[2], WR_DATA[1], WR_DATA[0]}),
        .leds({LEDS[7], LEDS[6], LEDS[5], LEDS[4], LEDS[3], LEDS[2], LEDS[1], LEDS[0]})
    );
endmodule
