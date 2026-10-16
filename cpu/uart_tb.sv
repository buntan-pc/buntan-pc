// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2024 Kota UCHIDA
 */
module uart_tb #(parameter RQ_DEPTH=4)();

logic rst, clk;
logic rx, tx, rd, rx_full, wr, tx_ready;
logic [7:0] rx_data, tx_data;

initial begin
  rst <= 1;
  clk <= 1;
  rx <= 1;
  wr <= 0;
  rd <= 0;
  $monitor("%5t: rst=%d rx=%d rx1buf=%d data=%x full=%d state=%d rxtim_half=%d",
           $time, rst, rx, uart.rx1buf, rx_data, rx_full, uart.rx_state, uart.rxtim_half,
           " tx=%d data=%x ready=%d state=%d txtim_full=%d",
           tx, tx_data, tx_ready, uart.tx_state, uart.txtim_full
           );

  #13
    rst <= 0;

  repeat(2) @(posedge clk);
    rx <= 0; // START
  if (rx_full !== 0) $error("rx_full must be 0");

  #100 rx <= 1; // LSB
  #100 rx <= 0;
  #100 rx <= 1;
  #100 rx <= 0;
  #100 rx <= 0;
  #100 rx <= 0;
  #100 rx <= 1;
  if (rx_full !== 0) $error("rx_full must be 0");
  #100 rx <= 0; // MSB

  #100 rx <= 1; // STOP
  #150;

  if (rx_full !== 1) $error("rx_full must be 1");
  if (rx_data !== 8'h45) $error("rx_data must be 45: %x", rx_data);

  @(posedge clk);
  rd <= 1;
  @(posedge clk);
  rd <= 0;

  #1;
  if (rx_full !== 0) $error("rx_full must be 0");

  @(posedge clk);
  tx_data <= 8'h49;
  wr <= 1;
  if (tx !== 1) $error("tx must be 1");
  if (tx_ready !== 1) $error("tx_ready must be 1");

  @(posedge clk) wr <= 0;
  if (tx_ready !== 0) $error("tx_ready must be 0");

  repeat(2) @(posedge clk);
  if (tx !== 0) $error("tx must be 0 (start bit)");

  repeat(10) @(posedge clk);
  if (tx !== 1) $error("tx must be 1 (LSB)");

  repeat(10) @(posedge clk);
  if (tx !== 0) $error("tx must be 0 (bit 1)");

  repeat(60) @(posedge clk);
  if (tx !== 0) $error("tx must be 0 (MSB)");

  repeat(10) @(posedge clk);
  if (tx !== 1) $error("tx must be 0 (stop bit)");
  if (tx_ready !== 0) $error("tx_ready must be 0");

  repeat(10) @(posedge clk);
  if (tx_ready !== 1) $error("tx_ready must be 1");

  repeat(200) @(posedge clk);
  // UART 受信と受信データの読み出しが同クロックで発生した場合のテスト
  // 空、一部格納、満杯の各状態で確認する
  test_simultaneous_rxrd(0);
  test_simultaneous_rxrd(1);
  test_simultaneous_rxrd(RQ_DEPTH);
  $finish;
end

always #5 begin
  clk <= ~clk;
end

uart#(.CLOCK_HZ(100), .BAUD(10), .RQ_DEPTH(RQ_DEPTH)) uart(.*);

// RX ピンから 1 文字送る。
// simultaneous_read=1 の場合、受信完了と同時に rd=1 とする。
task receive_byte(input logic [7:0] value, input logic simultaneous_read);
  fork
    begin
      @(negedge clk);
      // スタートビット
      rx = 0;
      repeat(10) @(negedge clk);
      // データ
      for (int i = 0; i < 8; i++) begin
        rx = value[i];
        repeat(10) @(negedge clk);
      end
      // ストップビット
      rx = 1;
      repeat(10) @(negedge clk);
    end
    begin
      rd = 0;
      @(negedge clk);
      while (!uart.rx_complete)
        @(negedge clk);
      // 受信完了と同時に rd=1 とする
      rd = simultaneous_read;
      @(posedge clk);
      if (simultaneous_read && !(rd && uart.rx_complete))
        $fatal(1, "test setup: rd did not overlap receive completion");
      #1;
      rd = 0;
    end
  join
endtask

// rx_data が期待値であることを確認し、読み出す
task expect_and_read(input logic [7:0] expected, input int queued);
  @(negedge clk);
  if (rx_full !== 1'b1 || rx_data !== expected) begin
    $error("queued=%0d: expected (rx_full, rx_data)=(1, %02h), got (%b, %02h)",
           queued, expected, rx_full, rx_data);
  end
  rd = 1;
  @(negedge clk);
  rd = 0;
endtask

task test_simultaneous_rxrd(input int queued);
  @(negedge clk);
  rst = 1;
  rd = 0;
  rx = 1;
  repeat(10) @(negedge clk);
  rst = 0;
  repeat(10) @(negedge clk);

  // 既存データは 30, 31, ...。同時に到着する文字は 42。
  for (int i = 0; i < queued; i++)
    receive_byte(8'h30 + i, 0);
  if (rx_full !== (queued > 0))
    $fatal(1, "test setup: unexpected rx_full before overlap (queued=%0d)", queued);
  if (queued !== 0 && rx_data !== 8'h30)
    $fatal(1, "test setup: unexpected head before overlap (queued=%0d)", queued);

  receive_byte(8'h42, 1);

  // 既存の先頭だけが消費され、残りの後ろに新しい文字が入る。
  // 空の場合の rd は無視し、新しく受信した文字を保持する。
  for (int i = 1; i < queued; i++) begin
    expect_and_read(8'h30 + i, queued);
  end
  //if (queued == 0)
  //  repeat(10) @(posedge clk);
  expect_and_read(8'h42, queued);

  @(negedge clk);
  if (rx_full !== 1'b0) begin
    $error("queued=%0d: queue must be empty after reading all data", queued);
  end
endtask

initial begin
  #2000000;
  $fatal(1, "uart_tb timed out");
end

endmodule
