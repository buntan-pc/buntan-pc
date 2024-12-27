void lcd_out4(int rs, int val) {
  lcd_port = (val << 4) | rs | 1;
  delay_ms(2);
  lcd_port = lcd_port & 0xfe;
}

void lcd_out8(int rs, int val) {
  lcd_out4(rs, val >> 4);
  lcd_out4(rs, val & 0x0f);
}

void lcd_cmd(int cmd) {
  lcd_out8(0, cmd);
}

void lcd_putc(int ch) {
  lcd_out8(4, ch);
}

void lcd_init() {
  lcd_out4(0, 3);
  lcd_out4(0, 3);
  lcd_out4(0, 3);
  lcd_out4(0, 2);

  // ここから 4 ビットモード
  lcd_cmd(0x28);
  lcd_cmd(0x0f);
  lcd_cmd(0x06);
  lcd_cmd(0x01);

  gpio = 0x80; // バックライト点灯
}

void lcd_puts(char *s) {
  while (*s) {
    lcd_putc(*s++);
  }
}
