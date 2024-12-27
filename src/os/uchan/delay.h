void delay_ms(int ms) {
  timer_cnt = ms;
  while (timer_cnt > 0);
}
