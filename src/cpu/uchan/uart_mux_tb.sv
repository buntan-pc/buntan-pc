module uart_mux_tb();

logic rst, clk;
logic rx, tx, rd, rx_full, wr, tx_ready, prog_recv, end_prog_recv;
logic [7:0] rx_data, tx_data;

integer i;

initial begin
  rst <= 1;
  clk <= 1;
  rx <= 1;
  rd <= 0;
  wr <= 0;
  end_prog_recv <= 0;
  $monitor("%7t: rst=%d rx=%d data=%x full=%d rd=%d prog_recv=%d end=%d",
           $time, rst, rx, rx_data, rx_full, rd, prog_recv, end_prog_recv,
           " tx=%d data=%x ready=%d wr=%d",
           tx, tx_data, tx_ready, wr
           );

  #13 rst <= 0;
  @(posedge clk);

  // 55 AA を受け取ったらプログラム転送モードになり、
  // それ以外では受信が継続することを確認

  // 55 以外はすぐに受信
  recv_byte(8'h61);
  #50;
  test_read(8'h61);

  // 55 の直後に他の値が来たら、その時点で受信
  recv_byte(8'h55);
  recv_byte(8'hAA);
  #50;
  test_read(8'h55);
  @(posedge clk)
  test_read(8'hAA);

  // 55 の直後に他の値が来たら、その時点で受信（その2）
  recv_byte(8'h55);
  recv_byte(8'h62);
  #50;
  test_read(8'h55);
  @(posedge clk)
  test_read(8'h62);

  // 55 単体は 20ms 後に受信
  recv_byte(8'h55);
  #50; // 受信直後なので、まだ rx_data には現れない。
  if (rx_full !== 0) $error("rx_full must be 0");
  if (rx_data !== 8'h62) $error("rx_data must be 62: %x", rx_data);

  #17000; // 17ms 経過。まだまだ。
  $display("17ms from 55 received");
  if (rx_full !== 0) $error("rx_full must be 0");
  if (rx_data !== 8'h62) $error("rx_data must be 62: %x", rx_data);

  #4000; // 4ms 経過。合計で 20ms 以上！
  $display("21ms from 55 received");
  test_read(8'h55);

  // 55 受信後、20ms 以降に AA が来ても、プログラム転送モードにはならない
  recv_byte(8'h55);
  #25000; // 25ms 経過。
  $display("25ms from 55 received");
  if (prog_recv !== 0) $error("prog_recv must be 0");
  recv_byte(8'hAA);
  #50;
  if (prog_recv !== 0) $error("prog_recv must be 0");
  test_read(8'h55);
  @(posedge clk)
  test_read(8'hAA);

  // 55 受信後 2ms～20ms の間に AA が来たら、プログラム転送モードへ
  recv_byte(8'h55);
  #2500; // 2.5ms 経過。
  $display("2.5ms from 55 received");
  if (prog_recv !== 0) $error("prog_recv must be 0");
  recv_byte(8'hAA);
  #50;
  if (prog_recv !== 1) $error("prog_recv must be 1");

  #100;

  recv_byte(8'h23);
  #50;
  test_read(8'h23);

  // prog_recv == 1 のときは 55 もすぐ受信する
  recv_byte(8'h55);
  #50;
  test_read(8'h55);

  recv_byte(8'hAA);
  #50;
  test_read(8'hAA);

  // end_prog_recv == 1 になるまでプログラム転送モードが継続
  #100;
  if (prog_recv !== 1) $error("prog_recv must be 1");
  recv_byte(8'h7F);
  recv_byte(8'hFF);
  #50;
  test_read(8'h7F);
  @(posedge clk);
  test_read(8'hFF);
  if (prog_recv !== 1) $error("prog_recv must be 1");

  // 7F FF でプログラム転送モードを止めるのは uart_mux より外の機能
  end_prog_recv <= 1;
  repeat(2) @(posedge clk);
  if (prog_recv !== 0) $error("prog_recv must be 0");

  recv_byte(8'h55);
  #25000; // 25ms 経過。
  test_read(8'h55);

  $finish;
end

always #5 begin
  clk <= ~clk;
end

task recv_byte(input [7:0] value);
begin
  rx <= 0; // START
  for (i = 0; i < 8; i++) begin
    repeat(10) @(posedge clk);
    rx <= value[i];
  end
  #100 rx <= 1; // STOP
  #100;
  $display("received %x", value);
end
endtask

task read;
begin
  rd <= 1;
  @(posedge clk);
  rd <= 0;
  @(posedge clk);
end
endtask

task test_read(input [7:0] value);
begin
  if (rx_full !== 1) $error("rx_full must be 1");
  if (rx_data !== value) $error("rx_data must be %x: %x", value, rx_data);
  read();
  if (rx_full !== 0) $error("rx_full must be 0");
end
endtask

uart_mux#(.CLOCK_HZ(100000), .BAUD(10000)) uart(.*);

endmodule
