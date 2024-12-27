#include "mmio.h"
#include "delay.h"
#include "lcd.h"

void play_bootsound() {
  int i;
  int j;
  for (i = 0; i < 100; i++) {
    gpio |= 0x40;
    for (j = 0; j < 200; j++);
    gpio &= ~0x40;
    for (j = 0; j < 200; j++);
  }
}

void int2hex(int val, char *s, int n) {
  int i;
  for (i = 0; i < n; i++) {
    int v = val & 0xf;
    val = val >> 4;
    if (v >= 10) {
      v += 'A' - 10;
    } else {
      v += '0';
    }
    s[n - 1 - i] = v;
  }
}

int (*syscall)();

int syscall_get_os_version() {
  return syscall(0, 0);
}

int syscall_put_string(char *s, int n) {
  int args[2] = {s, n};
  return syscall(1, args);
}

int syscall_get_string(char *s, int n) {
  int args[2] = {s, n};
  return syscall(2, args);
}

int main(int *info) {
  char buf[5];
  led_port = 0;

  syscall = info[1];
  lcd_init();

  play_bootsound();

  syscall_put_string("info: ", -1);
  buf[4] = 0;
  int2hex(info[0], buf, 4);
  syscall_put_string(buf, -1);
  syscall_put_string(" ", -1);
  int2hex(info[1], buf, 4);
  syscall_put_string(buf, -1);
  syscall_put_string("\n", -1);

  syscall_put_string("os version: ", -1);
  int2hex(syscall_get_os_version(), buf, 4);
  syscall_put_string(buf, -1);
  syscall_put_string("\n", -1);

  syscall_put_string("waiting value...", -1);

  syscall_get_string(buf, 5);
  int value = 0;
  for (int i = 0; buf[i]; ++i) {
    value = value * 10 + (buf[i] - '0');
  }

  syscall_put_string("got value: ", -1);
  syscall_put_string(buf, -1);
  syscall_put_string("\n", -1);
  return value;
}
