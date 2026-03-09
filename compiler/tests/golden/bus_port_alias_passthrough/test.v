// This Verilog was transpiled from JZ-HDL.
// jz-hdl version: jz-hdl 0.1 (prototype)
// Intended for use with yosys.

`default_nettype none

module bus_source (
    addr,
    cmd,
    valid,
    data,
    pbus_ADDR,
    pbus_CMD,
    pbus_VALID,
    pbus_DATA,
    pbus_DONE
);
    // Ports
    input [7:0] addr;
    input cmd;
    input valid;
    inout [7:0] data;
    output reg [7:0] pbus_ADDR;
    output reg pbus_CMD;
    output reg pbus_VALID;
    inout [7:0] pbus_DATA;
    input pbus_DONE;

    // Signals


    assign pbus_DATA = data;

    always @* begin
        pbus_ADDR = addr;
        pbus_CMD = cmd;
        pbus_VALID = valid;
    end
endmodule

module bus_passthrough (
    src_ADDR,
    src_CMD,
    src_VALID,
    src_DATA,
    src_DONE,
    dst_ADDR,
    dst_CMD,
    dst_VALID,
    dst_DATA,
    dst_DONE
);
    // Ports
    input [7:0] src_ADDR;
    input src_CMD;
    input src_VALID;
    inout [7:0] src_DATA;
    output src_DONE;
    output [7:0] dst_ADDR;
    output dst_CMD;
    output dst_VALID;
    inout [7:0] dst_DATA;
    input dst_DONE;

    // Signals

    assign dst_ADDR = src_ADDR;
    assign dst_CMD = src_CMD;
    assign dst_VALID = src_VALID;
    assign dst_DATA = src_DATA;
    assign src_DONE = dst_DONE;

endmodule

module bus_sink (
    pbus_ADDR,
    pbus_CMD,
    pbus_VALID,
    pbus_DATA,
    pbus_DONE,
    done_flag
);
    // Ports
    input [7:0] pbus_ADDR;
    input pbus_CMD;
    input pbus_VALID;
    inout [7:0] pbus_DATA;
    output reg pbus_DONE;
    output reg done_flag;

    // Signals


    assign pbus_DATA = pbus_VALID == 1'b1 && pbus_CMD == 1'b1 ? 8'b10101010 : 8'bzzzzzzzz;

    always @* begin
        done_flag = pbus_VALID;
        pbus_DONE = 1'b1;
    end
endmodule

module bus_test_top (
    addr,
    cmd,
    valid,
    data,
    done
);
    // Ports
    input [7:0] addr;
    input cmd;
    input valid;
    inout [7:0] data;
    output done;

    // Signals
    wire [18:0] src_bus;
    wire [18:0] dst_bus;


    bus_passthrough pass0 (
        .src_ADDR(src_bus[7:0]),
        .src_CMD(src_bus[8]),
        .src_VALID(src_bus[9]),
        .src_DATA(src_bus[17:10]),
        .src_DONE(src_bus[18]),
        .dst_ADDR(dst_bus[7:0]),
        .dst_CMD(dst_bus[8]),
        .dst_VALID(dst_bus[9]),
        .dst_DATA(dst_bus[17:10]),
        .dst_DONE(dst_bus[18])
    );
    bus_sink sink0 (
        .pbus_ADDR(dst_bus[7:0]),
        .pbus_CMD(dst_bus[8]),
        .pbus_VALID(dst_bus[9]),
        .pbus_DATA(dst_bus[17:10]),
        .pbus_DONE(dst_bus[18]),
        .done_flag(done)
    );
    bus_source source0 (
        .addr(addr),
        .cmd(cmd),
        .valid(valid),
        .data(data),
        .pbus_ADDR(src_bus[7:0]),
        .pbus_CMD(src_bus[8]),
        .pbus_VALID(src_bus[9]),
        .pbus_DATA(src_bus[17:10]),
        .pbus_DONE(src_bus[18])
    );
endmodule

module top (
    addr_in,
    cmd_in,
    valid_in,
    data_io,
    done_out
);
    input [7:0] addr_in;
    input cmd_in;
    input valid_in;
    inout [7:0] data_io;
    output done_out;

    // Top-level logical→physical pin mapping
    //   bus_test_top.addr[7] -> addr_in[7] (board 17)
    //   bus_test_top.addr[6] -> addr_in[6] (board 16)
    //   bus_test_top.addr[5] -> addr_in[5] (board 15)
    //   bus_test_top.addr[4] -> addr_in[4] (board 14)
    //   bus_test_top.addr[3] -> addr_in[3] (board 13)
    //   bus_test_top.addr[2] -> addr_in[2] (board 12)
    //   bus_test_top.addr[1] -> addr_in[1] (board 11)
    //   bus_test_top.addr[0] -> addr_in[0] (board 10)
    //   bus_test_top.cmd -> cmd_in (board 20)
    //   bus_test_top.valid -> valid_in (board 21)
    //   bus_test_top.data[7] -> data_io[7] (board 37)
    //   bus_test_top.data[6] -> data_io[6] (board 36)
    //   bus_test_top.data[5] -> data_io[5] (board 35)
    //   bus_test_top.data[4] -> data_io[4] (board 34)
    //   bus_test_top.data[3] -> data_io[3] (board 33)
    //   bus_test_top.data[2] -> data_io[2] (board 32)
    //   bus_test_top.data[1] -> data_io[1] (board 31)
    //   bus_test_top.data[0] -> data_io[0] (board 30)
    //   bus_test_top.done -> done_out (board 40)



    bus_test_top u_top (
        .addr({addr_in[7], addr_in[6], addr_in[5], addr_in[4], addr_in[3], addr_in[2], addr_in[1], addr_in[0]}),
        .cmd(cmd_in),
        .valid(valid_in),
        .data({data_io[7], data_io[6], data_io[5], data_io[4], data_io[3], data_io[2], data_io[1], data_io[0]}),
        .done(done_out)
    );
endmodule
