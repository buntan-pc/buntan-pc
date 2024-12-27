// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2024 Kota UCHIDA
 */
void delay_ms(int ms) {
  timer_cnt = ms;
  while (timer_cnt > 0);
}
