`include "common.sv"

/*
汎用スタック構造

pop   1 でポップ（push より優先される）
push  1 でプッシュ（pop=1 のとき push は無視される）
load  1 で data_in から stack[0] へ読み込み

pop push load | 動作
--------------------
  0    0    0 | 状態を維持
  0    0    1 | stack[0] に data_in を書く（stack[1] 以降は維持）
  0    1    0 | 0 をプッシュ
  0    1    1 | data_in をプッシュ
  1    x    0 | ポップ
  1    x    1 | stack[0] に data_in を書き、stack[1] 以降をポップ
*/

module stack#(
  parameter WIDTH = 16,
  parameter DEPTH = 16
) (
  input rst,
  input clk,
  input pop,
  input push,
  input load,
  input [WIDTH-1:0] data_in,
  output [WIDTH-1:0] data0,
  output [WIDTH-1:0] data1
);

logic [WIDTH-1:0] data[0:DEPTH-1];
assign data0 = data[0];
assign data1 = data[1];

integer i;

always @(posedge clk, posedge rst) begin
  if (rst)
    for (i = 0; i < DEPTH; i = i+1) data[i] <= {WIDTH{1'b0}};
  else begin
    if (load)
      data[0] <= data_in;
    else if (pop)
      data[0] <= data[1];
    else if (push)
      data[0] <= {WIDTH{1'b0}};

    if (pop) begin
      for (i = 1; i < DEPTH-1; i = i+1) data[i] <= data[i + 1];
      data[DEPTH-1] <= {WIDTH{1'b0}};
    end
    else if (push) begin
      for (i = 1; i < DEPTH; i = i+1) data[i] <= data[i - 1];
    end
  end
end

endmodule
