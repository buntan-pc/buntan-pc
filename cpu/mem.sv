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
  output logic [17:0] data_out
);

genvar i;
logic [17:0] pmem_out [15:0];
logic pmem_wre;

assign data_out = pmem_out[addr[13:10]];

generate
for (i = 0; i < 16; i = i + 1) begin: genpmem
  SPX9#(
    .READ_MODE(1'b0),
    .WRITE_MODE(2'b00),
    .BIT_WIDTH(18),
    .BLK_SEL(i[2:0]),
    .RESET_MODE("ASYNC")
  ) pmem(
    .DO(pmem_out[i]),
    .CLK(clk),
    .OCE(1'b0),
    .CE(1'b1),
    .RESET(rst),
    .WRE(addr[13] == i[3] & pmem_wre),
    .BLKSEL(addr[12:10]),
    .AD(addr[9:0]),
    .DI(data_in)
  );
end
endgenerate

`include "pmem_init.sv"

//initial begin
//  $readmemh("./ipl.pmem.hex", mem, 0);
//end

endmodule
