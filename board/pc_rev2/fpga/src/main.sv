// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 Kota UCHIDA
 */
`include "../../../cpu/common.sv"

module main(
  input sys_clk,
  input rst_n_raw,
  output [5:0] onboard_led,
  input  uart_rx, uart2_rx, uart3_rx, uart3b_rx,
  output uart_tx, uart2_tx, uart3_tx, uart3b_tx,
  output spk_on,      // DAC をスピーカーへ接続するかどうかを制御
  input  adc_cmp,     // ADC のコンパレータ出力
  output adc_sh_on,   // ADC のサンプル&ホールドスイッチ制御
  output [2:0] adc_sel,  // ADC のチャンネル選択
  output [7:0] adc_vref, // ADC の DAC 入力
  output tf_cs, tf_mosi, tf_sclk,
  input  tf_miso,
  inout  scl, sda,    // I2C Clock & Data
  output [7:0] gpio,
  input  sw_n_raw,
  output rgbled,
  output [2:0] fastio
);

// logic 定義
logic rst_n;
logic sw_n;

// ********
// 継続代入
// ********
assign onboard_led = ~io_led[5:0];

// ****************
// その他のロジック
// ****************

always @(posedge sys_clk) begin
  rst_n <= rst_n_raw;
  sw_n <= sw_n_raw;
end

logic dmem_wen, dmem_byt;
logic [`ADDR_WIDTH-1:0] dmem_addr, dmem_addr_d;
logic [15:0] dmem_rdata_io, dmem_wdata;

logic [15:0] dmem_rdata_raw;

logic [7:0] cpu_out;
logic [17:0] cpu_uart_recv_data;
logic [`ADDR_WIDTH-1:0] img_pmem_size;

logic [7:0] io_led, io_gpio;
logic [2:0] io_fastio;
logic io_rgbled;
logic clk125;
logic clk_cpu;

// 継続代入
assign dmem_rdata_io = io_mux(dmem_addr_d, io_led, io_gpio, io_fastio, io_rgbled, ~sw_n);

assign gpio = io_gpio;
assign fastio = io_fastio;
assign rgbled = io_rgbled;

always @(posedge sys_clk, negedge rst_n) begin
  if (!rst_n) begin
    io_led <= 0;
    io_gpio <= 0;
    io_fastio <= 0;
    io_rgbled <= 0;
  end
  else if (dmem_wen && dmem_addr == `ADDR_WIDTH'h080) begin
    if (dmem_byt)
      io_led <= dmem_wdata[7:0];
    else
      io_led <= dmem_wdata[7:0];
  end
  else if (dmem_wen && dmem_addr == `ADDR_WIDTH'h082) begin
    if (dmem_byt) begin
      io_gpio <= dmem_wdata[7:0];
    end
    else begin
      io_gpio <= dmem_wdata[7:0];
      io_fastio <= dmem_wdata[10:8];
    end
  end
  else if (dmem_wen && dmem_addr == `ADDR_WIDTH'h083) begin
    io_fastio <= dmem_wdata[10:8];
  end
end

always @(posedge sys_clk, negedge rst_n) begin
  if (!rst_n)
    dmem_addr_d <= `ADDR_WIDTH'd0;
  else
    dmem_addr_d <= dmem_addr;
end

// CPU 用クロック
//Gowin_rPLL_10mhz clk_cpu_pll(
//  .clkout(clk_cpu), //output clkout
//  .lock(),        //output lock
//  .clkin(sys_clk) //input clkin
//);
assign clk_cpu = sys_clk;

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

logic uart3_rx_common, uart3_tx_common;
assign uart3_rx_common = uart3_rx & uart3b_rx;
assign uart3_tx = uart3_tx_common;
assign uart3b_tx = uart3_tx_common;

// 自作 CPU を接続する
mcu_rev2#(
  .CLOCK_HZ(27_000_000)
) mcu(
  .rst(~rst_n),
  .clk(clk_cpu),
  .uart_rx(uart_rx),
  .uart2_rx(uart2_rx),
  .uart3_rx(uart3_rx_common),
  .uart_tx(uart_tx),
  .uart2_tx(uart2_tx),
  .uart3_tx(uart3_tx_common),
  .dmem_addr(dmem_addr),
  .dmem_wen(dmem_wen),
  .dmem_byt(dmem_byt),
  .dmem_rdata_io(dmem_rdata_io),
  .dmem_wdata(dmem_wdata),
  .uart_recv_data(cpu_uart_recv_data),
  .img_pmem_size(img_pmem_size),
  .clk125(clk125),
  .spk_on(spk_on),
  .adc_cmp(adc_cmp),
  .adc_sh_on(adc_sh_on),
  .adc_sel(adc_sel),
  .adc_vref(adc_vref),
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
  .i2c_scl(scl),
  .i2c_sda(sda)
);

function [15:0] io_mux(
  input [`ADDR_WIDTH-1:0] addr,
  [7:0] io_led, io_gpio,
  [2:0] io_fastio,
  input io_rgbled,
  input io_sw
);
begin
  casex (addr)
    `ADDR_WIDTH'b1000_000x: return {7'd0, io_rgbled, io_led};
    `ADDR_WIDTH'b1000_001x: return {{io_sw, 3'd0, 1'd0, io_fastio}, io_gpio};
    default:                return 16'd0;
  endcase
end
endfunction

endmodule
