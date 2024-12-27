// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2024 Kota UCHIDA
 */
`include "../../../cpu/common.sv"

module main(
  input sys_clk,
  input rst_n_raw,
  output [5:0] onboard_led,
  input  uart_rx, uart2_rx, uart3_rx,
  output uart_tx, uart2_tx, uart3_tx,
  input  [7:0] key_col_n,
  output [7:0] key_row,
  output lcd_e,
  output lcd_rw,
  output lcd_rs,
  output [7:4] lcd_db,
  input  adc_cmp,     // ADC のコンパレータ出力
  output adc_sh_ctl,  // ADC のサンプル&ホールドスイッチ制御
  output adc_dac_pwm, // ADC の DAC PWM 信号
  output tf_cs, tf_mosi, tf_sclk,
  input  tf_miso,
  inout  scl, sda,    // I2C Clock & Data
  output [7:0] gpio,
  input  stop_n_raw
);

// logic 定義
logic rst_n;
logic stop_n;

// ********
// 継続代入
// ********
assign onboard_led = ~io_led[5:0];

// ****************
// その他のロジック
// ****************

always @(posedge sys_clk) begin
  rst_n <= rst_n_raw;
  stop_n <= stop_n_raw;
end

logic dmem_wen, dmem_byt;
logic [`ADDR_WIDTH-1:0] dmem_addr, dmem_addr_d;
logic [15:0] dmem_rdata_io, dmem_wdata;

logic [15:0] dmem_rdata_raw;

logic [7:0] cpu_out;
logic [17:0] cpu_uart_recv_data;
logic [`ADDR_WIDTH-1:0] img_pmem_size;

logic [7:0] io_led, io_lcd, io_gpio;
logic clk125;

// 継続代入
assign lcd_e  = io_lcd[0];
assign lcd_rw = io_lcd[1];
assign lcd_rs = io_lcd[2];
assign lcd_db = io_lcd[7:4];

assign dmem_rdata_io = io_mux(dmem_addr_d, io_led, io_lcd, io_gpio, ~stop_n);

assign gpio = io_gpio;

always @(posedge sys_clk, negedge rst_n) begin
  if (!rst_n) begin
    io_led <= 0;
    io_lcd <= 0;
    io_gpio <= 0;
  end
  else if (dmem_wen && dmem_addr == `ADDR_WIDTH'h080)
    if (dmem_byt)
      io_led <= dmem_wdata[7:0];
    else
      {io_lcd, io_led} <= dmem_wdata;
  else if (dmem_wen && dmem_addr == `ADDR_WIDTH'h081)
    io_lcd <= dmem_wdata[15:8];
  else if (dmem_wen && dmem_addr == `ADDR_WIDTH'h082)
    if (dmem_byt)
      io_gpio <= dmem_wdata[7:0];
    else
      io_gpio <= dmem_wdata[7:0];
end

always @(posedge sys_clk, negedge rst_n) begin
  if (!rst_n)
    dmem_addr_d <= `ADDR_WIDTH'd0;
  else
    dmem_addr_d <= dmem_addr;
end

// 周辺機能用高速クロック
Gowin_OSC internal_osc_125mhz(
  .oscout(clk125) // 125MHz
);

// MCU 内蔵周辺機能：ユーザーフラッシュ
logic [8:0] uf_xadr;
logic [5:0] uf_yadr;
logic uf_xe, uf_ye, uf_se, uf_erase, uf_prog, uf_nvstr;
logic [31:0] uf_din, uf_dout;
FLASH608K flash608k_instance(
  .XADR(uf_xadr),
  .YADR(uf_yadr),
  .XE(uf_xe),
  .YE(uf_ye),
  .SE(uf_se),
  .ERASE(uf_erase),
  .PROG(uf_prog),
  .NVSTR(uf_nvstr),
  .DIN(uf_din),
  .DOUT(uf_dout)
);

// 自作 CPU を接続する
mcu mcu(
  .rst(~rst_n),
  .clk(sys_clk),
  .uart_rx(uart_rx),
  .uart2_rx(uart2_rx),
  .uart3_rx(uart3_rx),
  .uart_tx(uart_tx),
  .uart2_tx(uart2_tx),
  .uart3_tx(uart3_tx),
  .dmem_addr(dmem_addr),
  .dmem_wen(dmem_wen),
  .dmem_byt(dmem_byt),
  .dmem_rdata_io(dmem_rdata_io),
  .dmem_wdata(dmem_wdata),
  .uart_recv_data(cpu_uart_recv_data),
  .img_pmem_size(img_pmem_size),
  .clk125(clk125),
  .adc_cmp(~adc_cmp),
  .adc_sh_ctl(adc_sh_ctl),
  .adc_dac_pwm(adc_dac_pwm),
  .uf_xadr(uf_xadr),
  .uf_yadr(uf_yadr),
  .uf_xe(uf_xe),
  .uf_ye(uf_ye),
  .uf_se(uf_se),
  .uf_erase(uf_erase),
  .uf_prog(uf_prog),
  .uf_nvstr(uf_nvstr),
  .uf_din(uf_din),
  .uf_dout(uf_dout),
  .spi_cs(tf_cs),
  .spi_sclk(tf_sclk),
  .spi_mosi(tf_mosi),
  .spi_miso(tf_miso),
  .key_col_n(key_col_n),
  .key_row(key_row),
  .i2c_scl(scl),
  .i2c_sda(sda)
);

function [15:0] io_mux(
  input [`ADDR_WIDTH-1:0] addr,
  [7:0] io_led, io_lcd, io_gpio,
  input io_stop
);
begin
  casex (addr)
    `ADDR_WIDTH'b1000_000x: return {io_lcd, io_led};
    `ADDR_WIDTH'b1000_001x: return {{7'd0, io_stop}, io_gpio};
    default:                return 16'd0;
  endcase
end
endfunction

endmodule
