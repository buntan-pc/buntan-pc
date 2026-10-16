// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2024 Kota UCHIDA
 */
module uart#(
  parameter CLOCK_HZ=27_000_000,
  parameter BAUD=9600,
  parameter DATA_BITS=8,
  parameter TIM_WIDTH=16,
  parameter RQ_DEPTH=1
) (
  input  rst, clk,
  input  rx,      // UART RX signal
  output tx,      // UART TX signal
  output logic [DATA_BITS-1:0] rx_data,
  input  logic [DATA_BITS-1:0] tx_data,
  input  rd,      // read rx buffer
  output rx_full, // rx buffer full
  input  wr,      // write tx buffer
  output tx_ready // ready to transmit
);

localparam BIT_PERIOD = CLOCK_HZ / BAUD;

logic rx1buf; // 非同期信号 rx を同期するためのレジスタ
logic [3:0] rx_filter;
logic [DATA_BITS-1:0] rx_shift, tx_shift; // シフトレジスタ
logic [2:0] rx_bit_cnt, tx_bit_cnt; // 送受信済みビット数
logic rxtim_rst, txtim_rst;
logic [DATA_BITS-1:0] rx_queue [RQ_DEPTH];
logic [$clog2(RQ_DEPTH+1)-1:0] rx_queue_len;
// カウンタは 0–RQ_DEPTH を表現できなければならないので clog2(RQ_DEPTH+1) とする。
logic rx_complete;

// 受信状態
typedef enum logic [1:0] {
  WAIT,  // スタートビットを待っている
  START, // スタートビットを送受信している
  DATA,  // データビットを送受信している
  STOP   // ストップビットを送受信している
} state_t;
state_t rx_state, tx_state;

logic rxtim_full, txtim_full; // タイマが次のクロックでオーバーフローするなら 1
logic rxtim_half, txtim_half;

// 1 ビットのタイミングを作り出すタイマ
simple_timer #(.PERIOD(BIT_PERIOD), .BITS(TIM_WIDTH))
  rxtim(
    .rst(rxtim_rst),
    .clk(clk),
    .half(rxtim_half),
    .full(rxtim_full)
  );
simple_timer #(.PERIOD(BIT_PERIOD), .BITS(TIM_WIDTH))
  txtim(
    .rst(txtim_rst),
    .clk(clk),
    .half(txtim_half),
    .full(txtim_full)
  );

assign tx = tx_signal(tx_state, tx_shift[0]);
assign rxtim_rst = rst || rx_state == WAIT;
assign txtim_rst = rst || tx_state == WAIT;
assign tx_ready = ~wr && tx_state == WAIT;
assign rx_data = rx_queue[0];
assign rx_full = rx_queue_len != 0;
assign rx_complete = rx_state == STOP && rxtim_half;

always @(posedge clk) begin
  rx_filter <= {rx, rx_filter[3:1]};
end

always @(posedge rst, posedge clk) begin
  if (rst)
    rx1buf <= 1;
  else if ((rx_filter[3] & rx_filter[2] & rx_filter[1] & rx_filter[0]) == 1)
    rx1buf <= 1;
  else if ((rx_filter[3] | rx_filter[2] | rx_filter[1] | rx_filter[0]) == 0)
    rx1buf <= 0;
end

always @(posedge rst, posedge clk) begin
  if (rst)
    rx_state <= WAIT;
  else if (rx_state == WAIT && ~rx1buf)
    rx_state <= START;
  else if (rx_state == START && rxtim_full)
    rx_state <= DATA;
  else if (rx_state == DATA && rxtim_full && rx_bit_cnt == DATA_BITS-1)
    rx_state <= STOP;
  else if (rx_state == STOP && rxtim_half)
    rx_state <= WAIT; // ストップビットだけは half で受信を終える
end

always @(posedge rst, posedge clk) begin
  if (rst)
    tx_state <= WAIT;
  else if (tx_state == WAIT && wr)
    tx_state <= START;
  else if (tx_state == START && txtim_full)
    tx_state <= DATA;
  else if (tx_state == DATA && txtim_full && tx_bit_cnt == DATA_BITS-1)
    tx_state <= STOP;
  else if (tx_state == STOP && txtim_full)
    tx_state <= WAIT;
end

always @(posedge rst, posedge clk) begin
  if (rst || rx_state == WAIT)
    rx_bit_cnt <= 0;
  else if (rx_state == DATA && rxtim_full)
    rx_bit_cnt <= rx_bit_cnt + 1;
end

always @(posedge rst, posedge clk) begin
  if (rst || tx_state == WAIT)
    tx_bit_cnt <= 0;
  else if (tx_state == DATA && txtim_full)
    tx_bit_cnt <= tx_bit_cnt + 1;
end

always @(posedge rst, posedge clk) begin
  if (rst)
    rx_shift <= 0;
  else if (rx_state == DATA && rxtim_half)
    rx_shift <= {rx1buf, rx_shift[DATA_BITS-1:1]};
end

always @(posedge rst, posedge clk) begin
  if (rst) begin
    for (int i = 0; i < RQ_DEPTH; i++) begin
      rx_queue[i] <= 0;
    end
  end
  else begin
    if (rd) begin
      for (int i = 0; i < RQ_DEPTH - 1; i++)
        rx_queue[i] <= rx_queue[i + 1];
    end
    if (rx_complete) begin
      if (rd && rx_queue_len != 0)
        rx_queue[rx_queue_len - 1] <= rx_shift;
      else // !rd || rx_queue_len == 0
        rx_queue[rx_queue_len] <= rx_shift;
    end
  end
end

always @(posedge rst, posedge clk) begin
  if (rst) begin
    rx_queue_len <= 0;
  end
  else begin
    if (!rd && rx_complete) begin
      if (rx_queue_len != RQ_DEPTH)
        rx_queue_len <= rx_queue_len + 1;
    end
    else if (rd && !rx_complete) begin
      if (rx_queue_len != 0)
        rx_queue_len <= rx_queue_len - 1;
    end
    else if (rd && rx_complete) begin
      // キューが空のときは、何も読み出されず、キューに 1 文字貯まる
      // キューにデータがあるときは、読み出しと追加が同時に起こるため、
      // rx_queue_len は変わらない
      if (rx_queue_len == 0)
        rx_queue_len <= 1;
    end
  end
end

always @(posedge rst, posedge clk) begin
  if (rst)
    tx_shift <= 0;
  else if (tx_state == WAIT && wr)
    tx_shift <= tx_data;
  else if (tx_state == DATA && txtim_full)
    tx_shift <= {1'b0, tx_shift[DATA_BITS-1:1]};
end

function logic tx_signal(input state_t tx_state, logic tx_lsb);
  case (tx_state)
    WAIT:  tx_signal = 1;
    START: tx_signal = 0;
    DATA:  tx_signal = tx_lsb;
    STOP:  tx_signal = 1;
  endcase
endfunction

endmodule
