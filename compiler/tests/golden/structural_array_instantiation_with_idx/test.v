// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module node_unit (
    clk,
    reset,
    en,
    node_ready,
    data
);
    // Ports
    input clk;
    input reset;
    input en;
    output node_ready;
    output [7:0] data;

    // Signals
    reg active;
    reg [7:0] storage;

    assign node_ready = active;
    assign data = storage;


    always @(posedge clk) begin
        if (!reset) begin
            active <= 1'b0;
            storage <= 8'b00000000;
        end
        else begin
            if (en) begin
                active <= 1'b1;
                storage <= 8'b10101010;
            end
        end
    end
endmodule

module top_module (
    clk,
    rst_n,
    por,
    en,
    flags,
    data
);
    // Ports
    input clk;
    input rst_n;
    input por;
    input en;
    output [5:0] flags;
    output reg [5:0] data;

    // Signals
    wire reset;
    reg [47:0] storage;
    reg [2:0] view;

    assign reset = por & rst_n;

    node_unit worker[0] (
        .clk(clk),
        .reset(reset),
        .en(en),
        .node_ready(flags[0]),
        .data(storage[7:0])
    );
    node_unit worker[1] (
        .clk(clk),
        .reset(reset),
        .en(en),
        .node_ready(flags[1]),
        .data(storage[15:8])
    );
    node_unit worker[2] (
        .clk(clk),
        .reset(reset),
        .en(en),
        .node_ready(flags[2]),
        .data(storage[23:16])
    );
    node_unit worker[3] (
        .clk(clk),
        .reset(reset),
        .en(en),
        .node_ready(flags[3]),
        .data(storage[31:24])
    );
    node_unit worker[4] (
        .clk(clk),
        .reset(reset),
        .en(en),
        .node_ready(flags[4]),
        .data(storage[39:32])
    );
    node_unit worker[5] (
        .clk(clk),
        .reset(reset),
        .en(en),
        .node_ready(flags[5]),
        .data(storage[47:40])
    );


    always @* begin
        data = ((storage >> (({{5{1'b0}}, view} * 8'b00001000) * 6)) & {6{1'b1}});
    end

    always @(posedge clk) begin
        view <= view + 3'b001;
    end
endmodule

module top (
    SCLK,
    DONE,
    KEY,
    LED,
    DATA
);
    input SCLK;
    input DONE;
    input [1:0] KEY;
    output [5:0] LED;
    output [5:0] DATA;

    // Top-level logical→physical pin mapping
    //   top_module.clk -> SCLK (board 4)
    //   top_module.rst_n -> KEY[0] (board 88)
    //   top_module.por -> DONE (board IOR32B)
    //   top_module.en -> KEY[1] (board 87)
    //   top_module.flags[5] -> LED[5] (board 20)
    //   top_module.flags[4] -> LED[4] (board 19)
    //   top_module.flags[3] -> LED[3] (board 18)
    //   top_module.flags[2] -> LED[2] (board 17)
    //   top_module.flags[1] -> LED[1] (board 16)
    //   top_module.flags[0] -> LED[0] (board 15)
    //   top_module.data[5] -> DATA[5] (board 30)
    //   top_module.data[4] -> DATA[4] (board 29)
    //   top_module.data[3] -> DATA[3] (board 26)
    //   top_module.data[2] -> DATA[2] (board 25)
    //   top_module.data[1] -> DATA[1] (board 28)
    //   top_module.data[0] -> DATA[0] (board 27)



    top_module u_top (
        .clk(SCLK),
        .rst_n(~KEY[0]),
        .por(DONE),
        .en(KEY[1]),
        .flags({LED[5], LED[4], LED[3], LED[2], LED[1], LED[0]}),
        .data({DATA[5], DATA[4], DATA[3], DATA[2], DATA[1], DATA[0]})
    );
endmodule
