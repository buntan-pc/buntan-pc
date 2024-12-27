module uart_tb();

logic rst, clk;
logic rx, tx, rd, rx_full, wr, tx_ready;
logic [7:0] rx_data, tx_data;

initial begin
  rst <= 1;
  clk <= 1;
  rx <= 1;
  wr <= 0;
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
  $finish;
end

always #5 begin
  clk <= ~clk;
end

uart#(.CLOCK_HZ(100), .BAUD(10)) uart(.*);

endmodule
