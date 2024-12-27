module spi_tb();

logic rst, clk;
logic sclk, miso, mosi, tx_start, tx_ready;
logic [7:0] tx_data, rx_data;

logic tx_data_bit;
localparam logic [7:0] mosi_dat = 8'hC5;
localparam logic [7:0] miso_dat = 8'hDE;

integer i;

initial begin
  rst <= 1;
  clk <= 1;
  miso <= 0;
  tx_data <= 0;
  tx_start <= 0;

  $monitor("%5t: rst=%d sclk=%d miso=%d mosi=%d tx_start=%d tx_ready=%d rx_data=%x sreg=%b",
           $time, rst, sclk, miso, mosi, tx_start, tx_ready, rx_data, spi.sreg
           );

  #13
    rst <= 0;
    if (tx_ready !== 1) $error("tx_ready must be 1");
    if (sclk !== 0) $error("sclk must be 0");

  #25
    if (sclk !== 0) $error("sclk must be 0");
  #50
    if (sclk !== 0) $error("sclk must be 0");

  @(posedge clk);
  tx_data <= mosi_dat;
  tx_start <= 1;
  @(posedge clk);
  tx_data <= 0;
  tx_start <= 0;

  miso <= miso_dat[7];

  #25
    if (sclk !== 0) $error("sclk must be 0"); // _
    if (mosi !== 1) $error("mosi must be 1"); // 1100 0101

  #50
    if (sclk !== 1) $error("sclk must be 1"); // ^
    if (mosi !== 1) $error("mosi must be 1"); // 1100 0101

  for (i = 1; i < 8; i = i+1) begin
    miso <= miso_dat[7 - i];
    tx_data_bit = mosi_dat[7 - i];

    #50
    if (sclk !== 0) $error("sclk must be 0");
    if (mosi !== tx_data_bit) $error("mosi must be %d", tx_data_bit);

    #50
    if (sclk !== 1) $error("sclk must be 1");
    if (mosi !== tx_data_bit) $error("mosi must be %d", tx_data_bit);
  end

  if (rx_data !== miso_dat) $error("rx_data must be %x", miso_dat);

  repeat(200) @(posedge clk);
  $finish;
end

always #5 begin
  clk <= ~clk;
end

spi#(.CLOCK_HZ(100), .BAUD(10)) spi(.*);

endmodule
