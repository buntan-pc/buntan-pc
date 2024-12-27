`include "common.sv"

module adc#(
  parameter CLOCK_HZ = 27_000_000
) (
  input  rst, clk,
  input  clk125,      // 125MHz のクロック
  input  adc_cmp,     // ADC のコンパレータ出力
  output adc_sh_ctl,  // ADC のサンプル&ホールドスイッチ制御
  output adc_dac_pwm, // ADC の DAC PWM 信号
  output logic [7:0] adc_result // ADC の変換結果
);

localparam PWM_PERIOD = 255;
localparam TICK_SAMPLE_END = 100;
localparam TICK_CONVERT_BIT = 100;

logic [7:0] pwm_period, pwm_period_next;
logic [7:0] adc_sar;         // 逐次比較レジスタ
logic [8:0] tick, tick_next; // PWM が周回するたびにインクリメントされる
logic tick_edge_zero;        // tick が次に 0 になる瞬間に 1 になる

typedef enum logic {
  SAMPLE,
  CONVERT
} adc_phase_t;
adc_phase_t adc_phase;
logic [2:0] bit_index;  // 変換中のビット位置

assign pwm_period_next = pwm_period >= PWM_PERIOD - 1 ? 0 : pwm_period + 1;
assign tick_next =
  adc_phase == SAMPLE
    ? (tick >= TICK_SAMPLE_END - 1 ? 0 : tick + 1)
    : (tick >= TICK_CONVERT_BIT - 1 ? 0 : tick + 1);
assign tick_edge_zero = pwm_period_next == 0 & tick_next == 0;
assign adc_sh_ctl = ~(adc_phase == SAMPLE);
assign adc_dac_pwm = pwm_period < adc_sar;

always @(posedge rst, posedge clk125) begin
  if (rst)
    pwm_period <= 0;
  else
    pwm_period <= pwm_period_next;
end

always @(posedge rst, posedge clk125) begin
  if (rst)
    adc_sar <= 8'h80;
  else if (adc_phase == CONVERT & tick_edge_zero) begin
    adc_sar[bit_index] <= adc_cmp;
    if (bit_index > 0)
      adc_sar[bit_index - 1] <= 1;
  end
  else if (adc_phase == SAMPLE & tick == 0 & pwm_period_next == 0)
    adc_sar <= 8'h80;
end

always @(posedge rst, posedge clk125) begin
  if (rst)
    tick <= 0;
  else if (pwm_period_next == 0)
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
  else if (adc_phase == SAMPLE & tick == 0 & pwm_period_next == 0)
    adc_result <= adc_sar;
end

endmodule
