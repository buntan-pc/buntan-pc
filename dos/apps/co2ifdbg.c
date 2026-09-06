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
  while (i < 100) {
    i++;
  }
}

int buntan_main(int *info) {
  init_syscall(info);
  buntan_printf("CO2-IF Debugger\n");

  gpio = 0x55;
  fastio = 1;
  delay_little();

  gpio = 0xAA;
  fastio = 2;
  delay_little();

  gpio = 0x33;
  fastio = 3;
  delay_little();

  gpio = 0xCC;
  fastio = 4;
  delay_little();

  gpio = 0x81;
  fastio = 5;
  gpio = 0xC3;
  fastio = 6;
  gpio = 0xE7;
  fastio = 7;

  buntan_printf("Quitting...\n");
  return 0;
}
