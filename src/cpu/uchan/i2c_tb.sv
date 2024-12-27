module i2c_tb();

logic rst, clk;
wire scl, sda;
logic sda_in;
logic rw;
logic cnd_start, cnd_stop;
logic [7:0] tx_data, rx_data;
logic tx_start, tx_ready;
logic tx_ack, rx_ack;

integer i;
parameter z = 1'bz;

assign sda = sda_in;

initial begin
  rst <= 1;
  clk <= 1;
  sda_in <= z;
  rw <= 0; // write
  cnd_start <= 0;
  cnd_stop <= 0;
  tx_data <= 8'h35;
  tx_start <= 0;
  tx_ack <= 0;

  $monitor("%5t: rst=%d scl=%d sda=%d cnd_start=%d cnd_stop=%d rx_data=%x tx_ready=%d rx_ack=%b",
           $time, rst, scl, sda, cnd_start, cnd_stop, rx_data, tx_ready, rx_ack,
           " i2c_state=%d i2c_bit_cnt=%d",
           i2c.state, i2c.bit_cnt
           );

  #13
    rst <= 0;
    if (tx_ready !== 1) $error("tx_ready must be 1");
    if (scl !== z) $error("scl must be Hi-Z");

  @(posedge clk)
    tx_start <= 1;
    cnd_start <= 1;
    if (scl !== z) $error("scl must be Hi-Z");
    if (sda !== z) $error("sda must be Hi-Z");

  @(posedge clk)
    tx_start <= 0;
    cnd_start <= 0;
  #10

  // Start bit
  #50
    if (tx_ready !== 0) $error("tx_ready must be 0");
    if (scl !== z) $error("scl must be Hi-Z");
    if (sda !== 0) $error("sda must be 0");

  #50
    if (scl !== 0) $error("scl must be 0");
    if (sda !== 0) $error("sda must be 0");

  test_databit(0); // Bit-7 (MSB)
  test_databit(0); // Bit-6
  test_databit(z); // Bit-5
  test_databit(z); // Bit-4
  test_databit(0); // Bit-3
  test_databit(z); // Bit-2
  test_databit(0); // Bit-1
  test_databit(z); // Bit-0 (LSB)

  sda_in <= 0;
  test_databit(0); // ACK
  sda_in <= z;

  if (rx_ack !== 0) $error("rx_ack must be 0");

  // Stop bit
  #150
    if (scl !== 0) $error("scl must be 0");
    if (sda !== 0) $error("sda must be 0");
    cnd_stop <= 1;

  #10
    cnd_stop <= 0;

  #10
    if (scl !== 0) $error("scl must be 0");
    if (sda !== 0) $error("sda must be 0");

  #30
    if (scl !== z) $error("scl must be 0");
    if (sda !== 0) $error("sda must be Hi-Z");

  #60
    if (scl !== z) $error("scl must be Hi-Z");
    if (sda !== z) $error("sda must be Hi-Z");

  #10
    if (tx_ready !== 1) $error("tx_ready must be 1");

  repeat(200) @(posedge clk);
  $finish;
end

always #5 begin
  clk <= ~clk;
end

i2c#(.CLOCK_HZ(100), .BAUD(10)) i2c(.*);

task test_databit(input e_sda);
  if (scl !== 0) $error("scl must be 0");

  #10
    if (scl !== 0) $error("scl must be 0");
    if (sda !== e_sda) $error("sda must be %x", e_sda);

  #40
    if (scl !== z) $error("scl must be Hi-Z");
    if (sda !== e_sda) $error("sda must be %x", e_sda);

  #30
    if (scl !== 0) $error("scl must be 0");
    if (sda !== e_sda) $error("sda must be %x", e_sda);

  #20
    if (scl !== 0) $error("scl must be 0");
endtask

endmodule
