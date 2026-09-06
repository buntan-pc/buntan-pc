// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 Kota UCHIDA
 * CO2 濃度センサーインターフェース回路のデバッグ用アプリ
 */
#include "mmio.h"
#include "delay.h"
#include "syscall.h"
#include "printf.h"

void delay_little() {
  int i = 0;
  while (i < 10) {
    i++;
  }
}

int buntan_main(int *info) {
  init_syscall(info);
  buntan_printf("CO2-IF Debugger ('q' to quit)\n");

  int i = 0;
  while (1) {
    fastio = (i & 1) | ((i >> 1) & 2) | ((i >> 2) & 4);
    delay_little();
    i++;

    int c = sys_getc_nonblock();
    if (c == 'q' || c == 'Q') {
      break;
    }
  }

  buntan_printf("Quitting...\n");
  return 0;
}
