// SPI Mode0
module spi#(
  parameter CLOCK_HZ=27_000_000,
  parameter BAUD=100_000
) (
  input  rst, clk,
  output logic sclk,// SPI clock signal
  output mosi,      // SPI master-out signal
  input  miso,      // SPI master-in signal
  input  [7:0] tx_data, // initial value of shift registor
  output [7:0] rx_data, // current value of shift registor
  input  tx_start,  // start to transmit data
  output tx_ready   // ready to transmit
);

logic tx_busy; // 0=idle, 1=trasnmitting
logic tim_half, tim_full;
logic [7:0] sreg; // shift register
logic [2:0] bit_cnt;

simple_timer#(.PERIOD(CLOCK_HZ / BAUD)) tim(
  .rst(~tx_busy),
  .clk(clk),
  .half(tim_half),
  .full(tim_full)
);

assign tx_ready = ~tx_busy;
assign mosi = sreg[7];
assign rx_data = sreg;

always @(posedge rst, posedge clk) begin
  if (rst)
    sclk <= 0;
  else if (tim_half)
    sclk <= 1;
  else if (tim_full)
    sclk <= 0;
end

always @(posedge rst, posedge clk) begin
  if (rst)
    tx_busy <= 0;
  else if (tx_start)
    tx_busy <= 1;
  else if (bit_cnt === 7 & tim_full)
    tx_busy <= 0;
end

always @(posedge rst, posedge clk) begin
  if (rst)
    sreg <= 8'hA5;
  else if (tx_start)
    sreg <= tx_data;
  else if (tim_full)
    sreg <= {sreg[6:0], miso};
end

always @(posedge rst, posedge clk) begin
  if (rst)
    bit_cnt <= 0;
  else if (tx_busy & tim_full)
    bit_cnt <= bit_cnt + 1;
end

endmodule
