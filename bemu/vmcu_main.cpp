// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 tas0dev
 */

#include <fcntl.h>
#include <pty.h>
#include <termios.h>
#include <unistd.h>
#include <verilated.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Vmcu.h"

int main() {
  Verilated::commandArgs(0, nullptr);
  VerilatedContext* context = new VerilatedContext();
  context->debug(0);
  Vmcu* top = new Vmcu(context);

  // uart3 用の PTY を作成
  int master_fd;
  char slave_name[128];
  if (openpty(&master_fd, NULL, slave_name, NULL, NULL) == -1) {
    perror("openpty");
    return 1;
  }
  // ユーザが接続できるようスレーブパスを表示
  printf("UART3 PTY: %s\n", slave_name);
  fflush(stdout);

  // マスターをノンブロッキングに設定（ホスト→デバイス読み取り用）
  int flags = fcntl(master_fd, F_GETFL, 0);
  fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);

  // 初期信号
  top->rst = 1;
  top->clk = 0;
  top->uart_rx = 1;  // idle high
  top->uart2_rx = 1;
  top->uart3_rx = 1;
  top->clk125 = 0;
  top->adc_cmp = 0;
  top->spi_miso = 1;
  top->key_col_n = 0xff;
  top->i2c_sda = 1;

  context->timeInc(0);
  top->eval();

  // 数サイクル後にリセット解除
  for (int i = 0; i < 10; ++i) {
    top->clk = 1;
    top->eval();
    top->clk = 0;
    top->eval();
    context->timeInc(1);
  }
  top->rst = 0;

  const unsigned long CLOCK_HZ = 27000000UL;
  const unsigned long BAUD = 9600UL;
  const unsigned long BIT_PERIOD = CLOCK_HZ / BAUD;

  unsigned long cycle = 0;
  int uart_tx_prev = 1;
  bool awaiting_samples = false;
  unsigned long sample_base = 0;
  int sample_count = 0;
  unsigned char sample_byte = 0;

  const unsigned long MAX_CYCLES = 1000UL * 1000UL * 10UL;
  for (; cycle < MAX_CYCLES; ++cycle) {
    // 1サイクルを進める
    top->clk = 1;
    top->eval();
    // PTY からホスト→デバイスを読み取る
    char buf[64];
    ssize_t r = read(master_fd, buf, sizeof(buf));
    if (r > 0) {
      // ignore
    }

    // 上昇エッジで uart3_tx をサンプリング
    int uart_tx = top->uart3_tx;

    if (!awaiting_samples) {
      // スタートビット検出：HIGH→LOW の遷移
      if (uart_tx_prev == 1 && uart_tx == 0) {
        // このサイクルでスタートビットを検出
        awaiting_samples = true;
        sample_base = cycle;
        sample_count = 0;
        sample_byte = 0;
        // printf("start at cycle %lu\n", cycle);
      }
    } else {
      unsigned long target =
          sample_base + (BIT_PERIOD / 2) + (sample_count + 1) * BIT_PERIOD;
      if (cycle >= target) {
        // データビットをサンプリング
        int bit = uart_tx;  // LSB first
        sample_byte |= (bit & 1) << sample_count;
        sample_count++;
        if (sample_count >= 8) {
          write(master_fd, &sample_byte, 1);
          awaiting_samples = false;
        }
      }
    }

    uart_tx_prev = uart_tx;

    // 下降エッジ（ハーフ）
    top->clk = 0;
    top->eval();
    context->timeInc(1);
  }

  delete top;
  delete context;
  return 0;
}
