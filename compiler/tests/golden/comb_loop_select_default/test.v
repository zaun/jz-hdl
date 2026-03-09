// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module select_palette (
    fg_idx,
    bg_idx,
    pixel_x,
    active,
    bitmap,
    red,
    green,
    blue
);
    // Ports
    input [3:0] fg_idx;
    input [3:0] bg_idx;
    input [3:0] pixel_x;
    input active;
    input [7:0] bitmap;
    output reg [7:0] red;
    output reg [7:0] green;
    output reg [7:0] blue;

    // Signals
    reg [7:0] fg_r;
    reg [7:0] fg_g;
    reg [7:0] fg_b;
    reg [7:0] bg_r;
    reg [7:0] bg_g;
    reg [7:0] bg_b;
    reg pixel_on;



    always @* begin
        case (fg_idx)
            4'b0000: begin
                fg_r = 8'b00000000;
                fg_g = 8'b00000000;
                fg_b = 8'b00000000;
            end
            4'b0001: begin
                fg_r = 8'b10101010;
                fg_g = 8'b00000000;
                fg_b = 8'b00000000;
            end
            4'b0010: begin
                fg_r = 8'b00000000;
                fg_g = 8'b10101010;
                fg_b = 8'b00000000;
            end
            4'b0011: begin
                fg_r = 8'b10101010;
                fg_g = 8'b01010101;
                fg_b = 8'b00000000;
            end
            4'b0100: begin
                fg_r = 8'b00000000;
                fg_g = 8'b00000000;
                fg_b = 8'b10101010;
            end
            4'b0101: begin
                fg_r = 8'b10101010;
                fg_g = 8'b00000000;
                fg_b = 8'b10101010;
            end
            4'b0110: begin
                fg_r = 8'b00000000;
                fg_g = 8'b10101010;
                fg_b = 8'b10101010;
            end
            4'b0111: begin
                fg_r = 8'b10101010;
                fg_g = 8'b10101010;
                fg_b = 8'b10101010;
            end
            default: begin
                fg_r = 8'b01010101;
                fg_g = 8'b01010101;
                fg_b = 8'b01010101;
            end
        endcase
        case (bg_idx)
            4'b0000: begin
                bg_r = 8'b00000000;
                bg_g = 8'b00000000;
                bg_b = 8'b00000000;
            end
            4'b0001: begin
                bg_r = 8'b10101010;
                bg_g = 8'b00000000;
                bg_b = 8'b00000000;
            end
            4'b0010: begin
                bg_r = 8'b00000000;
                bg_g = 8'b10101010;
                bg_b = 8'b00000000;
            end
            4'b0011: begin
                bg_r = 8'b10101010;
                bg_g = 8'b01010101;
                bg_b = 8'b00000000;
            end
            4'b0100: begin
                bg_r = 8'b00000000;
                bg_g = 8'b00000000;
                bg_b = 8'b10101010;
            end
            4'b0101: begin
                bg_r = 8'b10101010;
                bg_g = 8'b00000000;
                bg_b = 8'b10101010;
            end
            4'b0110: begin
                bg_r = 8'b00000000;
                bg_g = 8'b10101010;
                bg_b = 8'b10101010;
            end
            4'b0111: begin
                bg_r = 8'b10101010;
                bg_g = 8'b10101010;
                bg_b = 8'b10101010;
            end
            default: begin
                bg_r = 8'b00000000;
                bg_g = 8'b00000000;
                bg_b = 8'b00000000;
            end
        endcase
        if (active == 1'b0) begin
            pixel_on = 1'b0;
        end
        else begin
            case (pixel_x)
                4'b0000: begin
                    pixel_on = bitmap[7];
                end
                4'b0001: begin
                    pixel_on = bitmap[6];
                end
                4'b0010: begin
                    pixel_on = bitmap[5];
                end
                4'b0011: begin
                    pixel_on = bitmap[4];
                end
                4'b0100: begin
                    pixel_on = bitmap[3];
                end
                4'b0101: begin
                    pixel_on = bitmap[2];
                end
                4'b0110: begin
                    pixel_on = bitmap[1];
                end
                4'b0111: begin
                    pixel_on = bitmap[0];
                end
                default: begin
                    pixel_on = 1'b0;
                end
            endcase
        end
        if (pixel_on == 1'b1) begin
            red = fg_r;
            green = fg_g;
            blue = fg_b;
        end
        else begin
            red = bg_r;
            green = bg_g;
            blue = bg_b;
        end
    end
endmodule

module top (
    fg_in,
    bg_in,
    px_in,
    act_in,
    bmp_in,
    r_out,
    g_out,
    b_out
);
    input [3:0] fg_in;
    input [3:0] bg_in;
    input [3:0] px_in;
    input act_in;
    input [7:0] bmp_in;
    output [7:0] r_out;
    output [7:0] g_out;
    output [7:0] b_out;

    // Top-level logical→physical pin mapping
    //   select_palette.fg_idx[3] -> fg_in[3] (board 3)
    //   select_palette.fg_idx[2] -> fg_in[2] (board 2)
    //   select_palette.fg_idx[1] -> fg_in[1] (board 1)
    //   select_palette.fg_idx[0] -> fg_in[0] (board 0)
    //   select_palette.bg_idx[3] -> bg_in[3] (board 7)
    //   select_palette.bg_idx[2] -> bg_in[2] (board 6)
    //   select_palette.bg_idx[1] -> bg_in[1] (board 5)
    //   select_palette.bg_idx[0] -> bg_in[0] (board 4)
    //   select_palette.pixel_x[3] -> px_in[3] (board 11)
    //   select_palette.pixel_x[2] -> px_in[2] (board 10)
    //   select_palette.pixel_x[1] -> px_in[1] (board 9)
    //   select_palette.pixel_x[0] -> px_in[0] (board 8)
    //   select_palette.active -> act_in (board 12)
    //   select_palette.bitmap[7] -> bmp_in[7] (board 20)
    //   select_palette.bitmap[6] -> bmp_in[6] (board 19)
    //   select_palette.bitmap[5] -> bmp_in[5] (board 18)
    //   select_palette.bitmap[4] -> bmp_in[4] (board 17)
    //   select_palette.bitmap[3] -> bmp_in[3] (board 16)
    //   select_palette.bitmap[2] -> bmp_in[2] (board 15)
    //   select_palette.bitmap[1] -> bmp_in[1] (board 14)
    //   select_palette.bitmap[0] -> bmp_in[0] (board 13)
    //   select_palette.red[7] -> r_out[7] (board 37)
    //   select_palette.red[6] -> r_out[6] (board 36)
    //   select_palette.red[5] -> r_out[5] (board 35)
    //   select_palette.red[4] -> r_out[4] (board 34)
    //   select_palette.red[3] -> r_out[3] (board 33)
    //   select_palette.red[2] -> r_out[2] (board 32)
    //   select_palette.red[1] -> r_out[1] (board 31)
    //   select_palette.red[0] -> r_out[0] (board 30)
    //   select_palette.green[7] -> g_out[7] (board 45)
    //   select_palette.green[6] -> g_out[6] (board 44)
    //   select_palette.green[5] -> g_out[5] (board 43)
    //   select_palette.green[4] -> g_out[4] (board 42)
    //   select_palette.green[3] -> g_out[3] (board 41)
    //   select_palette.green[2] -> g_out[2] (board 40)
    //   select_palette.green[1] -> g_out[1] (board 39)
    //   select_palette.green[0] -> g_out[0] (board 38)
    //   select_palette.blue[7] -> b_out[7] (board 53)
    //   select_palette.blue[6] -> b_out[6] (board 52)
    //   select_palette.blue[5] -> b_out[5] (board 51)
    //   select_palette.blue[4] -> b_out[4] (board 50)
    //   select_palette.blue[3] -> b_out[3] (board 49)
    //   select_palette.blue[2] -> b_out[2] (board 48)
    //   select_palette.blue[1] -> b_out[1] (board 47)
    //   select_palette.blue[0] -> b_out[0] (board 46)



    select_palette u_top (
        .fg_idx({fg_in[3], fg_in[2], fg_in[1], fg_in[0]}),
        .bg_idx({bg_in[3], bg_in[2], bg_in[1], bg_in[0]}),
        .pixel_x({px_in[3], px_in[2], px_in[1], px_in[0]}),
        .active(act_in),
        .bitmap({bmp_in[7], bmp_in[6], bmp_in[5], bmp_in[4], bmp_in[3], bmp_in[2], bmp_in[1], bmp_in[0]}),
        .red({r_out[7], r_out[6], r_out[5], r_out[4], r_out[3], r_out[2], r_out[1], r_out[0]}),
        .green({g_out[7], g_out[6], g_out[5], g_out[4], g_out[3], g_out[2], g_out[1], g_out[0]}),
        .blue({b_out[7], b_out[6], b_out[5], b_out[4], b_out[3], b_out[2], b_out[1], b_out[0]})
    );
endmodule
