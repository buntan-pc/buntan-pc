// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2024 Kota UCHIDA
 */
`include "common.sv"

// data memory
module dmem(
  input rst,
  input clk,
  input [`ADDR_WIDTH-1:0] addr,
  input wen,
  input byt,
  input [15:0] data_in,
  output [15:0] data_out
);

logic addr_lsb;
assign addr_lsb = addr & 1'd1;

logic wen_lo, wen_hi;
logic [`ADDR_WIDTH-2:0] addr_lo, addr_hi;
logic [7:0] in_lo, in_hi, out_lo, out_hi;

assign wen_lo = wen & (~byt | ~addr_lsb);
assign wen_hi = wen & (~byt | addr_lsb);
assign addr_lo = addr >> 1;
assign addr_hi = addr >> 1;
assign in_lo = data_in[7:0];
assign in_hi = data_in[15:8];
assign data_out = {out_hi, out_lo};

logic [7:0] mem_lo[0:13'h1fff];
logic [7:0] mem_hi[0:13'h1fff];

always @(posedge rst, posedge clk) begin
  if (rst) begin
  end
  else begin
    if (wen_lo)
      mem_lo[addr_lo] <= in_lo;
    if (wen_hi)
      mem_hi[addr_hi] <= in_hi;
  end
end

always @(posedge rst, posedge clk) begin
  if (rst) begin
    out_lo <= 0;
    out_hi <= 0;
  end
  else begin
    out_lo <= mem_lo[addr_lo];
    out_hi <= mem_hi[addr_hi];
  end
end

initial begin
  $readmemh("./ipl.dmem_lo.hex", mem_lo, `ADDR_WIDTH'h100 >> 1);
  $readmemh("./ipl.dmem_hi.hex", mem_hi, `ADDR_WIDTH'h100 >> 1);
end

endmodule

// program memory
module pmem(
  input rst,
  input clk,
  input [`ADDR_WIDTH-1:0] addr,
  input wenh, wenl,
  input [17:0] data_in,
  output [17:0] data_out
);

genvar i;
logic [35:0] pmem_out [15:0];
logic [1:0] pmem_buf;

assign data_out = pmem_out[addr[13:10]][17:0];

always @(posedge clk, posedge rst) begin
  if (rst) begin
    pmem_buf <= 18'd0;
  end
  else begin
    if (wenh) begin
      pmem_buf <= data_in[1:0];
    end
  end
end


generate
for (i = 0; i < 16; i = i + 1) begin: genpmem
  SPX9#(
    .READ_MODE(1'b0),
    .WRITE_MODE(2'b00),
    .BIT_WIDTH(18),
    .BLK_SEL(i[2:0]),
    .RESET_MODE("SYNC"),
    .INIT_RAM_00(pmem_init_values[(i << 6) | 0]),
    .INIT_RAM_01(pmem_init_values[(i << 6) | 1]),
    .INIT_RAM_02(pmem_init_values[(i << 6) | 2]),
    .INIT_RAM_03(pmem_init_values[(i << 6) | 3]),
    .INIT_RAM_04(pmem_init_values[(i << 6) | 4]),
    .INIT_RAM_05(pmem_init_values[(i << 6) | 5]),
    .INIT_RAM_06(pmem_init_values[(i << 6) | 6]),
    .INIT_RAM_07(pmem_init_values[(i << 6) | 7]),
    .INIT_RAM_08(pmem_init_values[(i << 6) | 8]),
    .INIT_RAM_09(pmem_init_values[(i << 6) | 9]),
    .INIT_RAM_0A(pmem_init_values[(i << 6) | 10]),
    .INIT_RAM_0B(pmem_init_values[(i << 6) | 11]),
    .INIT_RAM_0C(pmem_init_values[(i << 6) | 12]),
    .INIT_RAM_0D(pmem_init_values[(i << 6) | 13]),
    .INIT_RAM_0E(pmem_init_values[(i << 6) | 14]),
    .INIT_RAM_0F(pmem_init_values[(i << 6) | 15]),
    .INIT_RAM_10(pmem_init_values[(i << 6) | 16]),
    .INIT_RAM_11(pmem_init_values[(i << 6) | 17]),
    .INIT_RAM_12(pmem_init_values[(i << 6) | 18]),
    .INIT_RAM_13(pmem_init_values[(i << 6) | 19]),
    .INIT_RAM_14(pmem_init_values[(i << 6) | 20]),
    .INIT_RAM_15(pmem_init_values[(i << 6) | 21]),
    .INIT_RAM_16(pmem_init_values[(i << 6) | 22]),
    .INIT_RAM_17(pmem_init_values[(i << 6) | 23]),
    .INIT_RAM_18(pmem_init_values[(i << 6) | 24]),
    .INIT_RAM_19(pmem_init_values[(i << 6) | 25]),
    .INIT_RAM_1A(pmem_init_values[(i << 6) | 26]),
    .INIT_RAM_1B(pmem_init_values[(i << 6) | 27]),
    .INIT_RAM_1C(pmem_init_values[(i << 6) | 28]),
    .INIT_RAM_1D(pmem_init_values[(i << 6) | 29]),
    .INIT_RAM_1E(pmem_init_values[(i << 6) | 30]),
    .INIT_RAM_1F(pmem_init_values[(i << 6) | 31]),
    .INIT_RAM_20(pmem_init_values[(i << 6) | 32]),
    .INIT_RAM_21(pmem_init_values[(i << 6) | 33]),
    .INIT_RAM_22(pmem_init_values[(i << 6) | 34]),
    .INIT_RAM_23(pmem_init_values[(i << 6) | 35]),
    .INIT_RAM_24(pmem_init_values[(i << 6) | 36]),
    .INIT_RAM_25(pmem_init_values[(i << 6) | 37]),
    .INIT_RAM_26(pmem_init_values[(i << 6) | 38]),
    .INIT_RAM_27(pmem_init_values[(i << 6) | 39]),
    .INIT_RAM_28(pmem_init_values[(i << 6) | 40]),
    .INIT_RAM_29(pmem_init_values[(i << 6) | 41]),
    .INIT_RAM_2A(pmem_init_values[(i << 6) | 42]),
    .INIT_RAM_2B(pmem_init_values[(i << 6) | 43]),
    .INIT_RAM_2C(pmem_init_values[(i << 6) | 44]),
    .INIT_RAM_2D(pmem_init_values[(i << 6) | 45]),
    .INIT_RAM_2E(pmem_init_values[(i << 6) | 46]),
    .INIT_RAM_2F(pmem_init_values[(i << 6) | 47]),
    .INIT_RAM_30(pmem_init_values[(i << 6) | 48]),
    .INIT_RAM_31(pmem_init_values[(i << 6) | 49]),
    .INIT_RAM_32(pmem_init_values[(i << 6) | 50]),
    .INIT_RAM_33(pmem_init_values[(i << 6) | 51]),
    .INIT_RAM_34(pmem_init_values[(i << 6) | 52]),
    .INIT_RAM_35(pmem_init_values[(i << 6) | 53]),
    .INIT_RAM_36(pmem_init_values[(i << 6) | 54]),
    .INIT_RAM_37(pmem_init_values[(i << 6) | 55]),
    .INIT_RAM_38(pmem_init_values[(i << 6) | 56]),
    .INIT_RAM_39(pmem_init_values[(i << 6) | 57]),
    .INIT_RAM_3A(pmem_init_values[(i << 6) | 58]),
    .INIT_RAM_3B(pmem_init_values[(i << 6) | 59]),
    .INIT_RAM_3C(pmem_init_values[(i << 6) | 60]),
    .INIT_RAM_3D(pmem_init_values[(i << 6) | 61]),
    .INIT_RAM_3E(pmem_init_values[(i << 6) | 62]),
    .INIT_RAM_3F(pmem_init_values[(i << 6) | 63])
  ) pmem(
    .DO(pmem_out[i]),
    .CLK(clk),
    .OCE(1'b0),
    .CE(1'b1),
    .RESET(rst),
    .WRE((addr[13] == i[3]) & wenl),
    .BLKSEL(addr[12:10]),
    .AD({addr[9:0], 4'd0}),
    .DI({18'd0, pmem_buf, data_in[15:0]})
  );
end
endgenerate

`include "pmem_init.sv"

endmodule
