// I2C driver
module i2c#(
  parameter CLOCK_HZ=27_000_000,
  parameter BAUD=100_000
) (
  input  rst, clk,
  inout  scl, sda,      // I2C clock &data signal
  input  cnd_start,     // send start condition
  input  cnd_stop,      // send stop condition
  input  rw,            // 1 = read (device -> mcu), 0 = write (mcu -> device)
  input  [7:0] tx_data, // initial value of shift registor
  output [7:0] rx_data, // current value of shift registor
  input  tx_start,      // start transmitting
  output tx_ready,      // ready to transmit
  input  tx_ack,        // ack (mcu -> device)
  output logic rx_ack   // ack (device -> mcu)
);

logic scl_out, sda_out;
logic tx_busy; // 0=idle, 1=trasnmitting
logic [7:0] sreg; // shift registor
logic [2:0] bit_cnt;

localparam TIM_PERIOD = CLOCK_HZ/BAUD;
logic [8:0] tim_cnt, tim_inc, tim_next;
// timer phase: n => tim_next = n/7 (n = 1..7)   otherwise 0
logic [2:0] tim_phase;
logic tim_full;

// 送受信の状態
typedef enum logic [2:0] {
  IDLE,  // 待機
  START, // スタートビットを送信している
  DATA,  // データビットを送受信している
  ACK,   // ACK ビットを送受信している
  STOP,  // ストップビットを送受信している
  NEXT   // 次のデータ送受信を待っている（tx_start | cnd_stop を待っている）
} state_t;
state_t state;

assign scl = scl_out ? 1'bz : 1'b0;
assign sda = sda_out ? 1'bz : 1'b0;

assign tx_ready = ~tx_busy;
assign rx_data = sreg;

assign tim_inc  = tim_cnt + 1;
assign tim_next = tim_inc < TIM_PERIOD ? tim_inc : 0;
assign tim_phase = (tim_next == 1*TIM_PERIOD/7) ? 1
                 : (tim_next == 2*TIM_PERIOD/7) ? 2
                 : (tim_next == 3*TIM_PERIOD/7) ? 3
                 : (tim_next == 4*TIM_PERIOD/7) ? 4
                 : (tim_next == 5*TIM_PERIOD/7) ? 5
                 : (tim_next == 6*TIM_PERIOD/7) ? 6
                 : (tim_next == 0) ? 7
                 : 0;
assign tim_full  = tim_next == 0;

always @(posedge rst, posedge clk) begin
  if (rst || state == IDLE || state == NEXT)
    tim_cnt <= 0;
  else
    tim_cnt <= tim_next;
end

always @(posedge rst, posedge clk) begin
  if (rst)
    state <= IDLE;
  else if (state == IDLE && tx_start)
    state <= cnd_start ? START : DATA;
  else if (state == START && tim_full)
    state <= DATA;
  else if (state == DATA && bit_cnt == 7 && tim_full)
    state <= ACK;
  else if (state == ACK && tim_full)
    state <= cnd_stop ? STOP : NEXT;
  else if (state == STOP && tim_full)
    state <= IDLE;
  else if (state == NEXT)
    state <= tx_start ? DATA : (cnd_stop ? STOP : NEXT);
end

always @(posedge rst, posedge clk) begin
  if (rst)
    scl_out <= 1;
  else if (state == IDLE)
    scl_out <= 1;
  else if (state == START) begin
    if (tim_phase == 5) scl_out <= 0;
  end
  else if (state == DATA | state == ACK) begin
    if      (tim_phase == 2) scl_out <= 1;
    else if (tim_phase == 5) scl_out <= 0;
  end
  else if (state == STOP) begin
    if (tim_phase == 2) scl_out <= 1;
  end
end

always @(posedge rst, posedge clk) begin
  if (rst)
    sda_out <= 1;
  else if (state == IDLE)
    sda_out <= 1;
  else if (state == START) begin
    if (tim_phase == 1) sda_out <= 0;
  end
  else if (state == DATA) begin
    if (rw /* read */) sda_out <= 1;
    else if (tim_phase == 1) sda_out <= sreg[7];
  end
  else if (state == ACK) begin
    if (rw & tim_phase == 1) sda_out <= tx_ack;
  end
  else if (state == STOP) begin
    if (tim_cnt == 0)   sda_out <= 0;
    if (tim_phase == 5) sda_out <= 1;
  end
  else if (state == NEXT) begin
    sda_out <= 0;
  end
end

always @(posedge rst, posedge clk) begin
  if (rst)
    rx_ack <= 1'b1;
  else if (state == ACK && tim_phase == 4) begin
    rx_ack <= sda;
  end
end

always @(posedge rst, posedge clk) begin
  if (rst)
    tx_busy <= 0;
  else if (tx_start | cnd_stop)
    tx_busy <= 1;
  else if (state == IDLE || state == NEXT)
    tx_busy <= 0;
end

always @(posedge rst, posedge clk) begin
  if (rst)
    sreg <= 8'hA5;
  else if (tx_start)
    sreg <= tx_data;
  else if (state == DATA && tim_full)
    sreg <= {sreg[6:0], 1'b0};
end

always @(posedge rst, posedge clk) begin
  if (rst)
    bit_cnt <= 0;
  else if (state == DATA & tim_full)
    bit_cnt <= bit_cnt + 1;
end

endmodule

// 参考
// I2Cプライマ、SMBus、PMBusの仕様を学ぶ | Analog Devices
// https://www.analog.com/jp/resources/analog-dialogue/articles/i2c-communication-protocol-understanding-i2c-primer-pmbus-and-smbus.html
