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
  output [7:0] adc_vref, // ADC の DAC 入力
  output logic [7:0] adc_result // ADC の変換結果
);

localparam logic [5:0] TICK_LAST = 6'd53; // 54 clocks = 2.0us
localparam logic [5:0] TICK_PRE_LAST = 6'd52;

logic [7:0] adc_sar;         // 逐次比較レジスタ
logic [5:0] tick, tick_next; // 0–53 を数えるため 6 ビットで足りる
logic tick_edge_zero;        // tick が次に 0 になる瞬間に 1 になる
logic [1:0] rst_sync;
logic rst_adc;

typedef enum logic {
  SAMPLE,
  CONVERT
} adc_phase_t;
adc_phase_t adc_phase;
logic [2:0] bit_index;  // 変換中のビット位置

assign tick_next = tick >= TICK_LAST ? 0 : tick + 1'b1;
assign adc_sh_on = adc_phase == SAMPLE;
assign adc_vref = adc_sar;
assign rst_adc = rst_sync[1];

// clk125 ドメインでは、リセットを非同期にアサートし、解除を同期する。
always @(posedge clk125 or posedge rst) begin
  if (rst)
    rst_sync <= 2'b11;
  else
    rst_sync <= {rst_sync[0], 1'b0};
end

always @(posedge rst_adc, posedge clk125) begin
  if (rst_adc)
    adc_sar <= 8'h80;
  else if (adc_phase == CONVERT & tick_edge_zero) begin
    adc_sar[bit_index] <= adc_cmp;
    if (bit_index > 0)
      adc_sar[bit_index - 1] <= 1;
  end
  else if (adc_phase == SAMPLE & tick == 0)
    adc_sar <= 8'h80;
end

always @(posedge rst_adc, posedge clk125) begin
  if (rst_adc)
    tick <= 0;
  else
    tick <= tick_next;
end

// One cycle ahead of tick wrapping, so the event is already registered
// when the SAR and phase registers consume it.
always @(posedge rst_adc, posedge clk125) begin
  if (rst_adc)
    tick_edge_zero <= 0;
  else
    tick_edge_zero <= tick == TICK_PRE_LAST;
end

always @(posedge rst_adc, posedge clk125) begin
  if (rst_adc)
    adc_phase <= SAMPLE;
  else if (adc_phase == SAMPLE & tick_edge_zero)
    adc_phase <= CONVERT;
  else if (adc_phase == CONVERT & tick_edge_zero)
    if (bit_index == 0)
      adc_phase <= SAMPLE;
end

always @(posedge rst_adc, posedge clk125) begin
  if (rst_adc)
    bit_index <= 7;
  else if (adc_phase == CONVERT & tick_edge_zero)
    bit_index <= bit_index == 0 ? 7 : bit_index - 1;
end

always @(posedge rst_adc, posedge clk125) begin
  if (rst_adc)
    adc_result <= 0;
  else if (adc_phase == SAMPLE & tick == 0)
    adc_result <= adc_sar;
end

endmodule
