module uart_mux#(
  parameter CLOCK_HZ=27_000_000,
  parameter BAUD=9600,
  parameter DATA_BITS=8,
  parameter TIM_WIDTH=16
) (
  input  rst, clk,
  input  rx,             // UART RX signal
  output tx,             // UART TX signal
  output logic [DATA_BITS-1:0] rx_data,
  input  logic [DATA_BITS-1:0] tx_data,
  input  rd,             // read rx buffer
  output logic rx_full,  // rx buffer full
  input  wr,             // write tx buffer
  output tx_ready,       // ready to transmit
  output logic img_recv, // asserted during memory image receive mode
  input end_img_recv     // stop memory image receive mode
);

/*
*  --------------     --------     ---------
* | uart.rx_data |-->| rx_buf |-->| rx_data |
*  --------------     --------     ---------
*   uart.rx_full     rx_buf_full    rx_full
*   uart.rd          rd_rx_buf      rd
*/

logic [DATA_BITS-1:0] rx_buf, uart_rx_data;
logic uart_rx_full, uart_rd;
logic rx_buf_full, rd_rx_buf, rx_buf_55, recv_55aa, start_img_recv;

assign uart_rd = (~rx_buf_full | start_img_recv) & uart_rx_full;
assign rd_rx_buf = ~rx_full & rx_buf_full & (img_recv | ~rx_buf_55 |
  tim > TIM_UPPER | (tim < TIM_LOWER & uart_rx_data !== 8'h55));
assign rx_buf_55 = (rx_buf === 8'h55);
assign recv_55aa = rx_buf_55 & rx_buf_full & (uart_rx_data === 8'hAA);
assign start_img_recv = recv_55aa & TIM_LOWER < tim & tim < TIM_UPPER;

always @(posedge rst, posedge clk) begin
  if (rst) begin
    rx_buf <= 0;
    rx_buf_full <= 0;
  end
  else if (start_img_recv)
    rx_buf_full <= 0;
  else if (uart_rd) begin
    rx_buf <= uart_rx_data;
    rx_buf_full <= 1;
  end
  else if (rd_rx_buf)
    rx_buf_full <= 0;
end

always @(posedge rst, posedge clk) begin
  if (rst) begin
    rx_data <= 0;
    rx_full <= 0;
  end
  else if (start_img_recv)
    rx_full <= 0;
  else if (rd_rx_buf) begin
    rx_data <= rx_buf;
    rx_full <= 1;
  end
  else if (rd)
    rx_full <= 0;
end

always @(posedge rst, posedge clk) begin
  if (rst)
    img_recv <= 0;
  else if (start_img_recv)
    img_recv <= 1;
  else if (end_img_recv)
    img_recv <= 0;
end

localparam TIM_LOWER = CLOCK_HZ/500; // 2ms
localparam TIM_UPPER = CLOCK_HZ/50; // 20ms

logic [19:0] tim;

always @(posedge rst, posedge clk) begin
  if (rst | ~rx_buf_55 | img_recv)
    tim <= 0;
  else if (tim <= TIM_UPPER)
    tim <= tim + 1;
end

uart#(
  .CLOCK_HZ(CLOCK_HZ),
  .BAUD(BAUD),
  .DATA_BITS(DATA_BITS),
  .TIM_WIDTH(TIM_WIDTH)
) uart(
  .rx_data(uart_rx_data),
  .rd(uart_rd),
  .rx_full(uart_rx_full),
  .*
);

endmodule
