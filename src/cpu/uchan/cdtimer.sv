`include "common.sv"

/*
カウントダウンタイマ

書き込まれた初期値を PERIOD クロック毎にカウントダウンする。
（PERIOD は 0 <= PERIOD <= 65535 で指定可能）
カウンタが 0 になったらカウントダウン動作を終了する。

load=1 かつ clk の立ち上がりで data を初期値として取り込む。
*/

module cdtimer#(
  parameter PERIOD = 27_000,
  parameter WIDTH = 16
) (
  input rst,
  input clk,
  input load,
  input [WIDTH-1:0] data,
  output logic [WIDTH-1:0] counter,
  output logic timeout
);

// カウントダウン周期生成用
logic [15:0] period, inc;
assign inc = period + 16'd1;

always @(posedge clk, posedge rst) begin
  if (rst) begin
    counter <= 0;
    period <= 0;
    timeout <= 0;
  end else if (load) begin
    counter <= data;
    period <= 0;
    timeout <= 0;
  end else if (timeout) begin
    // pass
  end else if (inc < PERIOD) begin
    period <= inc;
  end else begin
    period <= 0;
    if (counter > 1)
      counter <= counter - 1;
    else if (counter === 1) begin
      counter <= 0;
      timeout <= 1;
    end
  end
end

endmodule
