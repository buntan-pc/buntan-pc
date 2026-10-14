/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Kota UCHIDA
 *
 * ファンクションジェネレータ
 */

#include "mmio.h"
#include "syscall.h"
#include "printf.h"

unsigned int phase; // 0–0xFFFF で波形 1 周

void delay_little(unsigned int n) {
  for (unsigned int i = 0; i < n; i++);
}

int saw() {
  return phase >> 8;
}

int square() {
  return 255 * (phase >> 15);
}

int triangle() {
  if (phase >= 0x7FFF) {
    return (0xFFFF - phase) >> 7;
  } else {
    return phase >> 7;
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

int sine() {
  return sine_table[phase >> 9];
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

int atoi(char *s) {
  int v = 0;
  int c;
  while ((c = *s++)) {
    if (c < '0' || '9' < c) {
      break;
    }
    v = 10 * v + c - '0';
  }
  return v;
}

unsigned int period_to_step(unsigned int period) {
  // 0x10000 / period を筆算による除算で求める
  unsigned int q = 0; // 商
  unsigned int r = 1; // 余り（初期値1は0x10000の先頭1を意味する）

  for (int i = 0; i < 16; ++i) {
    r = r << 1;
    q = q << 1;

    if (r >= period) {
      r -= period;
      q |= 1;
    }
  }

  return q;
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
      f_addr = &saw;
    } else if (strcmp(argv[1], "tri") == 0) { // triangle
      f_addr = &triangle;
    } else if (strcmp(argv[1], "sq") == 0) { // square
      f_addr = &square;
    } else if (strcmp(argv[1], "sin") == 0) { // sine
      f_addr = &sine;
    }
  }
  int (*f)() = f_addr + info[0];

  int vol = 31;
  if (argc >= 3) {
    int new_vol = atoi(argv[2]);
    if (new_vol != 0) {
      vol = new_vol;
    }
  }

  int variable_period = 0;
  unsigned int period = 127;
  if (argc >= 4) {
    if (strcmp(argv[3], "cds") == 0) {
      variable_period = 1;
    } else {
      period = atoi(argv[3]);
    }
  }

  unsigned int step = 256;
  if (period != 0) {
    step = period_to_step(period);
  }

  unsigned int prev_phase = 0;
  while (sys_getc_nonblock() != 'q') {
    int val = f() & 0x00FF;
    val = (vol * val) >> 8;
    adc_config = 0x0200 | val; // spk_on = 1

    prev_phase = phase;
    phase += step;

    if (variable_period && phase < prev_phase) {
      // 波形が1周した
      adc_config = 0x0000 | val; // adc_sel = 0, spk_on = 0, en_adc = 0
      adc_config = 0x0100 | val; // adc_sel = 0, spk_on = 0, en_adc = 1
      delay_little(5);
      step = adc_result * 40;
      adc_config = val; // adc_sel = 0, spk_on = 0, en_adc = 0
    }
  }

  return 0;
}
