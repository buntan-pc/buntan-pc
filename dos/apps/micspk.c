/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Kota UCHIDA
 *
 * マイクとスピーカーの動作テスト
 */

#include "mmio.h"
#include "syscall.h"

void delay_little(unsigned int n) {
  for (unsigned int i = 0; i < n; i++);
}

int buntan_main(int *info) {
  int adc;
  unsigned int conf;
  init_syscall(info);

  while (sys_getc_nonblock() != 'q') {
    delay_little(100);
    adc = adc_result;
    adc_config = adc; // en_adc = 0, spk_on = 0, DAC = ADC
    delay_little(10);
    adc_config |= 0x0200; // spk_on = 1
    delay_little(10);
    adc_config &= 0xFDFF; // spk_on = 0
    delay_little(10);
    adc_config = 0x0100; // en_adc = 1, spk_on = 0, DAC = 0
  }

  return 0;
}
