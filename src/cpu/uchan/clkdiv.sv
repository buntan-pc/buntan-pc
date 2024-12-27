`include "common.sv"

/* クロック分周器
*
* division に 1 以上の値を設定する。
* clk が division * 2 に分周され、divided から出力される。
*/
module clkdiv#(
  parameter WIDTH = 25
) (
  input rst,
  input clk,
  input [WIDTH-1:0] division,
  output divided
);

logic [WIDTH-1:0] cnt;
logic [WIDTH-1:0] inc;
assign inc = cnt + 1;

logic out;
assign divided = out;

always @(posedge clk, posedge rst) begin
  if (rst) begin
    cnt <= {WIDTH{1'd0}};
    out <= 1'b0;
  end
  else if (inc < division)
    cnt <= inc;
  else begin
    cnt <= {WIDTH{1'd0}};
    out <= ~out;
  end
end

endmodule
