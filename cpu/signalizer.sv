// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2024 Kota UCHIDA
 */
`include "common.sv"

module signalizer(
  input rst,
  input clk,
  output logic phase_decode,
  output logic phase_exec,
  output logic phase_rdmem,
  output logic phase_fetch
);

always @(posedge clk, posedge rst) begin
  if (rst) begin
    phase_decode <= 1'b0;
    phase_exec   <= 1'b0;
    phase_rdmem  <= 1'b1;
    phase_fetch  <= 1'b0;
  end else begin
    phase_decode <= phase_fetch;
    phase_exec   <= phase_decode;
    phase_rdmem  <= phase_exec;
    phase_fetch  <= phase_rdmem;
  end
end

endmodule
