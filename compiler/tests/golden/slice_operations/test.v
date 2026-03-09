// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module slice_demo (
    select,
    \input ,
    mem_addr,
    out,
    bit_out,
    mem_out,
    mux_data
);
    // Ports
    input [1:0] select;
    input [7:0] \input ;
    input [3:0] mem_addr;
    output reg [7:0] out;
    output reg bit_out;
    output reg [7:0] mem_out;
    output reg [7:0] mux_data;

    // Signals
    reg [15:0] bus;
    reg [3:0] slice_a;
    reg [3:0] slice_b;
    reg [7:0] slice_c;
    reg single_a;
    reg single_b;
    reg [7:0] ch_a;
    reg [7:0] ch_b;
    reg [7:0] ch_c;
    reg [7:0] ch_d;
    reg [31:0] wide_reg;
    reg gbit_result;
    reg [3:0] gslice_result;
    reg [7:0] sbit_result;
    reg [15:0] sslice_result;
    reg [15:0] lhs_target;
    reg [3:0] expr_slice_hi;
    reg [3:0] expr_slice_lo;

    // Memories
    (* ram_style = "distributed" *) reg [7:0] test_mem[0:15];
    integer jz_mem_init_i_0;

    initial begin
        for (jz_mem_init_i_0 = 0; jz_mem_init_i_0 < 16; jz_mem_init_i_0 = jz_mem_init_i_0 + 1) begin
            test_mem[jz_mem_init_i_0] = 8'b00000000;
        end
    end



    always @* begin
        bus = {\input , \input };
        slice_a = bus[15:12];
        slice_b = bus[7:4];
        single_a = \input [0];
        single_b = \input [7];
        slice_c = bus[7:0];
        lhs_target[7:0] = \input ;
        lhs_target[15:8] = 8'b11111111;
        ch_a = 8'b00010001;
        ch_b = 8'b00100010;
        ch_c = 8'b00110011;
        ch_d = 8'b01000100;
        wide_reg = 32'b11011101110011001011101110101010;
        mem_out = test_mem[mem_addr];
        gbit_result = ((\input  >> select[0]) & 1'b1);
        gslice_result = ((\input  >> (select * 4)) & {4{1'b1}});
        sbit_result = ((\input  & ~(8'b00000001 << 2'b00)) | (({8{1'b1}} & (8'b00000001 << 2'b00))));
        sslice_result = ((lhs_target & ~({16{1'b1}} << 3'b000)) | (({{12{1'b0}}, 4'b1111}) << 3'b000) & ({16{1'b1}} << 3'b000));
        expr_slice_hi = (((\input  + 8'b00010000) >> 4) & {4{1'b1}});
        expr_slice_lo = (((\input  + 8'b00010000) >> 0) & {4{1'b1}});
        if (select == 2'b00) begin
            out = {slice_a, slice_b};
            bit_out = single_a;
            mux_data = 8'b00000000;
        end
        else if (select == 2'b01) begin
            out = (({ch_d, ch_c, ch_b, ch_a} >> (select * 8)) & {8{1'b1}});
            bit_out = single_b;
            mux_data = (({ch_d, ch_c, ch_b, ch_a} >> (select * 8)) & {8{1'b1}});
        end
        else if (select == 2'b10) begin
            out = ((wide_reg >> (select * 8)) & {8{1'b1}});
            bit_out = gbit_result;
            mux_data = ((wide_reg >> (select * 8)) & {8{1'b1}});
        end
        else begin
            out = {expr_slice_hi, expr_slice_lo};
            bit_out = gbit_result;
            mux_data = {gslice_result, gslice_result};
        end
    end
endmodule

module top (
    sel,
    data_in,
    addr,
    result,
    single,
    mem_data,
    mux_out
);
    input [1:0] sel;
    input [7:0] data_in;
    input [3:0] addr;
    output [7:0] result;
    output single;
    output [7:0] mem_data;
    output [7:0] mux_out;

    // Top-level logical→physical pin mapping
    //   slice_demo.select[1] -> sel[1] (board 2)
    //   slice_demo.select[0] -> sel[0] (board 1)
    //   slice_demo.input[7] -> data_in[7] (board 17)
    //   slice_demo.input[6] -> data_in[6] (board 16)
    //   slice_demo.input[5] -> data_in[5] (board 15)
    //   slice_demo.input[4] -> data_in[4] (board 14)
    //   slice_demo.input[3] -> data_in[3] (board 13)
    //   slice_demo.input[2] -> data_in[2] (board 12)
    //   slice_demo.input[1] -> data_in[1] (board 11)
    //   slice_demo.input[0] -> data_in[0] (board 10)
    //   slice_demo.mem_addr[3] -> addr[3] (board 23)
    //   slice_demo.mem_addr[2] -> addr[2] (board 22)
    //   slice_demo.mem_addr[1] -> addr[1] (board 21)
    //   slice_demo.mem_addr[0] -> addr[0] (board 20)
    //   slice_demo.out[7] -> result[7] (board 37)
    //   slice_demo.out[6] -> result[6] (board 36)
    //   slice_demo.out[5] -> result[5] (board 35)
    //   slice_demo.out[4] -> result[4] (board 34)
    //   slice_demo.out[3] -> result[3] (board 33)
    //   slice_demo.out[2] -> result[2] (board 32)
    //   slice_demo.out[1] -> result[1] (board 31)
    //   slice_demo.out[0] -> result[0] (board 30)
    //   slice_demo.bit_out -> single (board 40)
    //   slice_demo.mem_out[7] -> mem_data[7] (board 57)
    //   slice_demo.mem_out[6] -> mem_data[6] (board 56)
    //   slice_demo.mem_out[5] -> mem_data[5] (board 55)
    //   slice_demo.mem_out[4] -> mem_data[4] (board 54)
    //   slice_demo.mem_out[3] -> mem_data[3] (board 53)
    //   slice_demo.mem_out[2] -> mem_data[2] (board 52)
    //   slice_demo.mem_out[1] -> mem_data[1] (board 51)
    //   slice_demo.mem_out[0] -> mem_data[0] (board 50)
    //   slice_demo.mux_data[7] -> mux_out[7] (board 67)
    //   slice_demo.mux_data[6] -> mux_out[6] (board 66)
    //   slice_demo.mux_data[5] -> mux_out[5] (board 65)
    //   slice_demo.mux_data[4] -> mux_out[4] (board 64)
    //   slice_demo.mux_data[3] -> mux_out[3] (board 63)
    //   slice_demo.mux_data[2] -> mux_out[2] (board 62)
    //   slice_demo.mux_data[1] -> mux_out[1] (board 61)
    //   slice_demo.mux_data[0] -> mux_out[0] (board 60)



    slice_demo u_top (
        .select({sel[1], sel[0]}),
        .\input ({data_in[7], data_in[6], data_in[5], data_in[4], data_in[3], data_in[2], data_in[1], data_in[0]}),
        .mem_addr({addr[3], addr[2], addr[1], addr[0]}),
        .out({result[7], result[6], result[5], result[4], result[3], result[2], result[1], result[0]}),
        .bit_out(single),
        .mem_out({mem_data[7], mem_data[6], mem_data[5], mem_data[4], mem_data[3], mem_data[2], mem_data[1], mem_data[0]}),
        .mux_data({mux_out[7], mux_out[6], mux_out[5], mux_out[4], mux_out[3], mux_out[2], mux_out[1], mux_out[0]})
    );
endmodule
