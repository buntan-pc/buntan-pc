// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2024 Kota UCHIDA
 */
`include "common.sv"

module adc_rev2#(
  parameter CLOCK_HZ = 27_000_000
) (
  input  rst, clk,
  input  clk125,      // 125MHz のクロック
  input  adc_cmp,     // ADC のコンパレータ出力
  output adc_sh_on,   // ADC のサンプル&ホールドスイッチ制御
  output [2:0] adc_sel,  // ADC のチャンネル選択
  output [7:0] adc_vref, // ADC の DAC 入力
  output logic [7:0] adc_result // ADC の変換結果
);

localparam TICK_SAMPLE_END = 54; // 54 clocks = 2.0us
localparam TICK_CONVERT_BIT = 54; // 54 clocks = 2.0us

logic [7:0] adc_sar;         // 逐次比較レジスタ
logic [8:0] tick, tick_next; // クロックを数えるレジスタ
logic tick_edge_zero;        // tick が次に 0 になる瞬間に 1 になる

typedef enum logic {
  SAMPLE,
  CONVERT
} adc_phase_t;
adc_phase_t adc_phase;
logic [2:0] bit_index;  // 変換中のビット位置

assign tick_next =
  adc_phase == SAMPLE
    ? (tick >= TICK_SAMPLE_END - 1 ? 0 : tick + 1)
    : (tick >= TICK_CONVERT_BIT - 1 ? 0 : tick + 1);
assign tick_edge_zero = tick_next == 0;
assign adc_sh_on = adc_phase == SAMPLE;
assign adc_sel = 3'd1;
assign adc_vref = adc_sar;

always @(posedge rst, posedge clk125) begin
  if (rst)
    adc_sar <= 8'h80;
  else if (adc_phase == CONVERT & tick_edge_zero) begin
    adc_sar[bit_index] <= adc_cmp;
    if (bit_index > 0)
      adc_sar[bit_index - 1] <= 1;
  end
  else if (adc_phase == SAMPLE & tick == 0)
    adc_sar <= 8'h80;
end

always @(posedge rst, posedge clk125) begin
  if (rst)
    tick <= 0;
  else
    tick <= tick_next;
end

always @(posedge rst, posedge clk125) begin
  if (rst)
    adc_phase <= SAMPLE;
  else if (adc_phase == SAMPLE & tick_edge_zero)
    adc_phase <= CONVERT;
  else if (adc_phase == CONVERT & tick_edge_zero)
    if (bit_index == 0)
      adc_phase <= SAMPLE;
end

always @(posedge rst, posedge clk125) begin
  if (rst)
    bit_index <= 7;
  else if (adc_phase == CONVERT & tick_edge_zero)
    bit_index <= bit_index == 0 ? 7 : bit_index - 1;
end

always @(posedge rst, posedge clk125) begin
  if (rst)
    adc_result <= 0;
  else if (adc_phase == SAMPLE & tick == 0)
    adc_result <= adc_sar;
end

endmodule
