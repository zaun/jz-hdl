// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module ArithmeticCore (
    clk,
    rst_n,
    por,
    btn,
    val_a,
    val_b,
    display
);
    // Ports
    input clk;
    input rst_n;
    input por;
    input btn;
    input [2:0] val_a;
    input [2:0] val_b;
    output [5:0] display;

    // Signals
    wire reset;
    (* fsm_encoding = "none" *) reg [2:0] func;
    reg func_done;
    reg [2:0] sum;
    reg carry;
    reg [5:0] data;
    reg [24:0] blink;

    assign display = data;
    assign reset = por & rst_n;


    always @(posedge clk) begin
        if (!reset) begin
            func <= 3'b000;
            func_done <= 1'b0;
            blink <= 25'b0000000000000000000000000;
            data <= 6'b000000;
            carry <= 1'b0;
            sum <= 3'b000;
        end
        else begin
            if (btn == 1'b1 && func_done == 1'b0) begin
                func <= func + 3'b001;
                func_done <= 1'b1;
            end
            else if (btn == 1'b0) begin
                func_done <= 1'b0;
            end
            blink <= blink + 25'b0000000000000000000000001;
            case (func)
                3'b000: begin
                    data[5:3] <= blink[24] ? {3{1'b1}} : {3{1'b0}};
                    data[2:0] <= blink[24] ? {3{1'b0}} : {3{1'b1}};
                end
                3'b001: begin
                    carry <= ({{1{1'b0}}, val_a} + {{1{1'b0}}, val_b}) >> 32'b00000000000000000000000000000011 & 1'b1;
                    sum <= ({{1{1'b0}}, val_a} + {{1{1'b0}}, val_b}) & 3'b111;
                    data <= {carry, 2'b00, sum};
                end
                3'b010: begin
                    carry <= ({{1{val_a[2]}}, val_a} + {{1{val_b[2]}}, val_b}) >> 32'b00000000000000000000000000000011 & 1'b1;
                    sum <= ({{1{val_a[2]}}, val_a} + {{1{val_b[2]}}, val_b}) & 3'b111;
                    data <= {carry, 2'b00, sum};
                end
                3'b011: begin
                    data <= (val_a * val_b);
                end
                3'b100: begin
                    data <= ($signed(val_a) * $signed(val_b));
                end
                default: begin
                    data <= {6{1'b0}};
                end
            endcase
        end
    end
endmodule

module top (
    SCLK,
    DONE,
    A,
    B,
    KEY,
    LED
);
    input SCLK;
    input DONE;
    input [2:0] A;
    input [2:0] B;
    input [1:0] KEY;
    output [5:0] LED;

    // Top-level logical→physical pin mapping
    //   ArithmeticCore.clk -> SCLK (board 4)
    //   ArithmeticCore.rst_n -> KEY[0] (board 88)
    //   ArithmeticCore.por -> DONE (board IOR32B)
    //   ArithmeticCore.btn -> KEY[1] (board 87)
    //   ArithmeticCore.val_a[2] -> A[2] (board 75)
    //   ArithmeticCore.val_a[1] -> A[1] (board 74)
    //   ArithmeticCore.val_a[0] -> A[0] (board 73)
    //   ArithmeticCore.val_b[2] -> B[2] (board 52)
    //   ArithmeticCore.val_b[1] -> B[1] (board 53)
    //   ArithmeticCore.val_b[0] -> B[0] (board 71)
    //   ArithmeticCore.display[5] -> LED[5] (board 20)
    //   ArithmeticCore.display[4] -> LED[4] (board 19)
    //   ArithmeticCore.display[3] -> LED[3] (board 18)
    //   ArithmeticCore.display[2] -> LED[2] (board 17)
    //   ArithmeticCore.display[1] -> LED[1] (board 16)
    //   ArithmeticCore.display[0] -> LED[0] (board 15)



    ArithmeticCore u_top (
        .clk(SCLK),
        .rst_n(~KEY[0]),
        .por(DONE),
        .btn(KEY[1]),
        .val_a({A[2], A[1], A[0]}),
        .val_b({B[2], B[1], B[0]}),
        .display({LED[5], LED[4], LED[3], LED[2], LED[1], LED[0]})
    );
endmodule
