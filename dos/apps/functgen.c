/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Kota UCHIDA
 *
 * ファンクションジェネレータ
 */

#include "mmio.h"
#include "syscall.h"
#include "printf.h"

void delay_little(unsigned int n) {
  for (unsigned int i = 0; i < n; i++);
}

int linear(unsigned int i) {
  return i;
}

int square(unsigned int i) {
  return 255 * (i >= 64);
}

int triangle(unsigned int i) {
  if (i < 64) {
    return i << 2;
  } else {
    return (127 - i) << 2;
  }
}

char sine_table[128] = {
  128, 134, 140, 146, 152, 158, 165, 170,
  176, 182, 188, 193, 198, 203, 208, 213,
  218, 222, 226, 230, 234, 237, 240, 243,
  245, 248, 250, 251, 253, 254, 254, 255,
  255, 255, 254, 254, 253, 251, 250, 248,
  245, 243, 240, 237, 234, 230, 226, 222,
  218, 213, 208, 203, 198, 193, 188, 182,
  176, 170, 165, 158, 152, 146, 140, 134,
  128, 121, 115, 109, 103,  97,  90,  85,
   79,  73,  67,  62,  57,  52,  47,  42,
   37,  33,  29,  25,  21,  18,  15,  12,
   10,   7,   5,   4,   2,   1,   1,   0,
    0,   0,   1,   1,   2,   4,   5,   7,
   10,  12,  15,  18,  21,  25,  29,  33,
   37,  42,  47,  52,  57,  62,  67,  73,
   79,  85,  90,  97, 103, 109, 115, 121
};

int sine(unsigned int i) {
  return sine_table[i];
}

int strcmp(char *a, char *b) {
  int c;
  while ((c = *a++)) {
    int v = c - *b++;
    if (v != 0 | c == 0) {
      return v;
    }
  }
  return c - *b;
}

int buntan_main(int *info) {
  int adc;
  unsigned int conf;
  init_syscall(info);
  int argc = info[2];
  char **argv = info[3];
  int f_addr = &square;
  if (argc >= 2) {
    if (strcmp(argv[1], "saw") == 0) { // sawtooth
      f_addr = &linear;
    } else if (strcmp(argv[1], "tri") == 0) { // triangle
      f_addr = &triangle;
    } else if (strcmp(argv[1], "sq") == 0) { // square
      f_addr = &square;
    } else if (strcmp(argv[1], "sin") == 0) { // sine
      f_addr = &sine;
    }
  }
  int (*f)(unsigned int) = f_addr + info[0];

  unsigned int i = 0;
  int vol = 127;

  while (sys_getc_nonblock() != 'q') {
    int val = f(i) & 0x00FF;
    val = (vol * val) >> 8;
    adc_config = 0x0200 | val; // spk_on = 1

    i = (i + 1) & 0x007F;
  }

  return 0;
}
