#include "mmio.h"

int ticks;
void _ISR_timer() {
  ticks++;
  timer_cnt = 1; // カウンタを再ロードする。タイムアウト状態はハードウェアが解除する。
}

int buntan_main(int *info) {
  int i;
  __builtin_set_isr(info[0] + _ISR_timer);
  timer_ctrl = 2; // bit 1: タイマ割り込みを許可
  timer_cnt = 1; // カウンタをロードしてタイマを開始
  for (i = 0; i < 30000; i++) {}
  while (ticks < 5) {}
  timer_ctrl = 0; // タイマ割り込みを禁止
  timer_cnt = 0; // カウンタを 0 にして停止
  return i;
}
