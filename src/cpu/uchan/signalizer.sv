`include "common.sv"

module signalizer(
  input rst,
  input clk,
  output phase_decode,
  output phase_exec,
  output phase_rdmem,
  output phase_fetch
);

logic reg1, reg2;
assign phase_decode = ~reg1 & ~reg2;
assign phase_exec   =  reg1 & ~reg2;
assign phase_rdmem  =  reg1 &  reg2;
assign phase_fetch  = ~reg1 &  reg2;

always @(posedge clk, posedge rst) begin
  if (rst) begin
    reg1 <= 1'b1;
    reg2 <= 1'b1;
  end else begin
    reg1 <= ~reg2;
    reg2 <= reg1;
  end
end

endmodule
