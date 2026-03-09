// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module tri_state_manager (
    clk,
    drive_en,
    internal_val,
    shared_bus,
    monitor
);
    // Ports
    input clk;
    input drive_en;
    input [7:0] internal_val;
    inout [7:0] shared_bus;
    output [7:0] monitor;

    // Signals
    reg [7:0] bus_capture;

    assign monitor = bus_capture;

    assign shared_bus = drive_en ? internal_val : 8'bzzzzzzzz;

    always @* begin
    end

    always @(posedge clk) begin
        if (!drive_en) begin
            bus_capture <= shared_bus;
        end
    end
endmodule

module top (
    sys_clk,
    ext_data_bus
);
    input sys_clk;
    inout [7:0] ext_data_bus;

    // Top-level logical→physical pin mapping
    //   tri_state_manager.clk -> sys_clk (board 5)
    //   tri_state_manager.drive_en -> (no connect)
    //   tri_state_manager.shared_bus[7] -> ext_data_bus[7] (board 17)
    //   tri_state_manager.shared_bus[6] -> ext_data_bus[6] (board 16)
    //   tri_state_manager.shared_bus[5] -> ext_data_bus[5] (board 15)
    //   tri_state_manager.shared_bus[4] -> ext_data_bus[4] (board 14)
    //   tri_state_manager.shared_bus[3] -> ext_data_bus[3] (board 13)
    //   tri_state_manager.shared_bus[2] -> ext_data_bus[2] (board 12)
    //   tri_state_manager.shared_bus[1] -> ext_data_bus[1] (board 11)
    //   tri_state_manager.shared_bus[0] -> ext_data_bus[0] (board 10)

    wire [7:0] jz_top_monitor_nc;


    tri_state_manager u_top (
        .clk(sys_clk),
        .drive_en(1'b0),
        .internal_val({1'b0, 1'b0, 1'b0, 1'b0, 1'b0, 1'b0, 1'b0, 1'b0}),
        .shared_bus({ext_data_bus[7], ext_data_bus[6], ext_data_bus[5], ext_data_bus[4], ext_data_bus[3], ext_data_bus[2], ext_data_bus[1], ext_data_bus[0]}),
        .monitor({jz_top_monitor_nc[7], jz_top_monitor_nc[6], jz_top_monitor_nc[5], jz_top_monitor_nc[4], jz_top_monitor_nc[3], jz_top_monitor_nc[2], jz_top_monitor_nc[1], jz_top_monitor_nc[0]})
    );
endmodule
