// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module JZHDL_LIB_RESET_SYNC (
    clk,
    rst_async_n,
    rst_sync_n
);
    // Ports
    input clk;
    input rst_async_n;
    output rst_sync_n;

    // Signals
    reg sync_ff1;
    reg sync_ff2;

    assign rst_sync_n = sync_ff2;


    always @(posedge clk or negedge rst_async_n) begin
        if (!rst_async_n) begin
            sync_ff1 <= 1'b0;
            sync_ff2 <= 1'b0;
        end
        else begin
            sync_ff1 <= 1'b1;
            sync_ff2 <= sync_ff1;
        end
    end
endmodule

module register_test_module (
    clk1,
    clk2,
    clk3,
    clk4,
    rst,
    en,
    d_in,
    q_imm,
    q_clk
);
    // Ports
    input clk1;
    input clk2;
    input clk3;
    input clk4;
    input rst;
    input en;
    input [7:0] d_in;
    output [7:0] q_imm;
    output [7:0] q_clk;

    // Signals
    reg [7:0] reg_immediate;
    reg [7:0] reg_clocked;
    reg [7:0] reg_low;
    reg [7:0] reg_falling;
    wire rst_sync_0;

    assign q_imm = reg_immediate;
    assign q_clk = reg_clocked;

    JZHDL_LIB_RESET_SYNC u_rst_sync_0 (
        .clk(clk1),
        .rst_async_n(rst),
        .rst_sync_n(rst_sync_0)
    );


    always @(posedge clk1) begin
        if (rst_sync_0) begin
            reg_immediate <= 8'b00000000;
        end
        else begin
            if (en) begin
                reg_immediate <= d_in;
            end
        end
    end

    always @(posedge clk2) begin
        if (rst) begin
            reg_clocked <= 8'b00000000;
        end
        else begin
            if (en) begin
                reg_clocked <= d_in;
            end
        end
    end

    always @(posedge clk3) begin
        if (!rst) begin
            reg_low <= 8'b00000000;
        end
        else begin
            if (en) begin
                reg_low <= d_in;
            end
        end
    end

    always @(negedge clk4) begin
        if (!rst) begin
            reg_falling <= 8'b00000000;
        end
        else begin
            if (en) begin
                reg_falling <= d_in;
            end
        end
    end
endmodule

module top (
    sys_clk,
    sys_rst,
    update_en,
    data_in,
    imm_out,
    clk_out
);
    input sys_clk;
    input sys_rst;
    input update_en;
    input [7:0] data_in;
    output [7:0] imm_out;
    output [7:0] clk_out;

    // Top-level logical→physical pin mapping
    //   register_test_module.clk1 -> sys_clk (board 52)
    //   register_test_module.clk2 -> sys_clk (board 52)
    //   register_test_module.clk3 -> sys_clk (board 52)
    //   register_test_module.clk4 -> sys_clk (board 52)
    //   register_test_module.rst -> sys_rst (board 53)
    //   register_test_module.en -> update_en (board 54)
    //   register_test_module.d_in[7] -> data_in[7] (board 8)
    //   register_test_module.d_in[6] -> data_in[6] (board 7)
    //   register_test_module.d_in[5] -> data_in[5] (board 6)
    //   register_test_module.d_in[4] -> data_in[4] (board 5)
    //   register_test_module.d_in[3] -> data_in[3] (board 4)
    //   register_test_module.d_in[2] -> data_in[2] (board 3)
    //   register_test_module.d_in[1] -> data_in[1] (board 2)
    //   register_test_module.d_in[0] -> data_in[0] (board 1)
    //   register_test_module.q_imm[7] -> imm_out[7] (board 17)
    //   register_test_module.q_imm[6] -> imm_out[6] (board 16)
    //   register_test_module.q_imm[5] -> imm_out[5] (board 15)
    //   register_test_module.q_imm[4] -> imm_out[4] (board 14)
    //   register_test_module.q_imm[3] -> imm_out[3] (board 13)
    //   register_test_module.q_imm[2] -> imm_out[2] (board 12)
    //   register_test_module.q_imm[1] -> imm_out[1] (board 11)
    //   register_test_module.q_imm[0] -> imm_out[0] (board 10)
    //   register_test_module.q_clk[7] -> clk_out[7] (board 27)
    //   register_test_module.q_clk[6] -> clk_out[6] (board 26)
    //   register_test_module.q_clk[5] -> clk_out[5] (board 25)
    //   register_test_module.q_clk[4] -> clk_out[4] (board 24)
    //   register_test_module.q_clk[3] -> clk_out[3] (board 23)
    //   register_test_module.q_clk[2] -> clk_out[2] (board 22)
    //   register_test_module.q_clk[1] -> clk_out[1] (board 21)
    //   register_test_module.q_clk[0] -> clk_out[0] (board 20)



    register_test_module u_top (
        .clk1(sys_clk),
        .clk2(sys_clk),
        .clk3(sys_clk),
        .clk4(sys_clk),
        .rst(sys_rst),
        .en(update_en),
        .d_in({data_in[7], data_in[6], data_in[5], data_in[4], data_in[3], data_in[2], data_in[1], data_in[0]}),
        .q_imm({imm_out[7], imm_out[6], imm_out[5], imm_out[4], imm_out[3], imm_out[2], imm_out[1], imm_out[0]}),
        .q_clk({clk_out[7], clk_out[6], clk_out[5], clk_out[4], clk_out[3], clk_out[2], clk_out[1], clk_out[0]})
    );
endmodule
