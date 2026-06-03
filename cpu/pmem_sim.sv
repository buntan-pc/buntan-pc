// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 Kota UCHIDA
 */
`include "common.sv"

// program memory
module pmem(
  input rst,
  input clk,
  input [`ADDR_WIDTH-1:0] addr,
  input wenh, wenl,
  input [17:0] data_in,
  output [17:0] data_out
);

logic [`ADDR_WIDTH-1:0] addr_d;
logic [17:0] mem [(1<<`ADDR_WIDTH)-1:0];
logic [1:0] pmem_buf;

assign data_out = mem[addr_d];

always @(posedge clk, posedge rst) begin
  if (rst) begin
    pmem_buf <= 18'd0;
  end
  else if (wenh) begin
    pmem_buf <= data_in[1:0];
  end
end

always @(posedge rst, posedge clk) begin
  if (rst)
    addr_d <= 0;
  else
    addr_d <= addr;
end

always @(posedge rst, posedge clk) begin
  if (rst) begin
  end
  else if (wenl) begin
    mem[addr] <= {pmem_buf, data_in[15:0]};
  end
end

endmodule

