`include "common.sv"

module sim_top#(
  parameter CLOCK_HZ = 27_000_000
) (
  input rst, clk, uart_rx, uart2_rx, uart3_rx,
  output uart_tx, uart2_tx, uart3_tx,
  output [`ADDR_WIDTH-1:0] dmem_addr,
  output dmem_wen, dmem_byt,
  input  [15:0] dmem_rdata_io,
  output [15:0] dmem_wdata,
  output [17:0] uart_recv_data,
  output [`ADDR_WIDTH-1:0] img_pmem_size,
  input  clk125,
  input  adc_cmp,
  output adc_sh_ctl,
  output adc_dac_pwm,
  output [8:0] uf_xadr,
  output [5:0] uf_yadr,
  output uf_xe, uf_ye, uf_se, uf_erase, uf_prog, uf_nvstr,
  output [31:0] uf_din,
  input  [31:0] uf_dout,
  output spi_cs,
  output spi_sclk, spi_mosi,
  input  spi_miso,
  inout  [7:0] key_col_n,
  output [7:0] key_row,
  inout  i2c_scl, i2c_sda,
  output dbgio,
  output [7:0] uart_out_data,
  output uart_out_full
);

localparam UART_BAUD = CLOCK_HZ / 10;

mcu #(
  .CLOCK_HZ(CLOCK_HZ),
  .UART_BAUD(UART_BAUD)
) mcu(
  .*
);

uart #(
  .CLOCK_HZ(CLOCK_HZ),
  .BAUD(UART_BAUD)
) uart_out_decoder(
  .rst(rst),
  .clk(clk),
  .rx(uart_tx),
  .tx(),
  .rx_data(uart_out_data),
  .tx_data(8'hff),
  .rd(uart_out_full),
  .rx_full(uart_out_full),
  .wr(1'b0),
  .tx_ready()
);

endmodule
