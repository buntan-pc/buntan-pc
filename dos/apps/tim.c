/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Kota UCHIDA
 *
 * キッチンタイマー
 * Usage: tim <seconds>
 *
 * 指定した秒数を数えるタイマーを起動する。
 * 手をかざすとタイマーを始動し、タイムアウトすると音で知らせる。
 * タイマー動作中に手をかざすと途中で止まる。
 */

#include "mmio.h"
#include "delay.h"
#include "syscall.h"

void play_sound(int len) {
  int i;
  int j;
  for (i = 0; i < len; i++) {
    gpio |= 0x40;
    for (j = 0; j < 200; j++);
    gpio &= ~0x40;
    for (j = 0; j < 200; j++);
  }
}

int atoi(char *s) {
  // skip space
  while (*s == ' ') {
    ++s;
  }

  int minus = 0;
  if (*s == '-') {
    minus = 1;
    ++s;
  } else if (*s == '+') {
    ++s;
  }

  int i = 0;
  while ('0' <= *s && *s <= '9') {
    i = 10 * i + (*s - '0');
    ++s;
  }

  return i;
}

void print_dec(int i) {
  char s[6];
  int nzero = sys_int2dec(i, s, 5);
  s[5] = '\n';
  sys_put_string(s + nzero, 6 - nzero);
}

int main(int *info) {
  char s[6];
  int adc;

  init_syscall(info);

  int argc = info[2];
  char **argv = info[3];
  int adc_thres = 220;

  int tim_sec = 3;
  if (argc == 2) {
    tim_sec = atoi(argv[1]);
  }
  print_dec(tim_sec);
  sys_put_string("waiting hand...\n", -1);

  // 手を離したら 1 になる
  int hand_removed = 0;

  // 1 ならアプリを終了する
  int stop = 0;

  // 手をかざすまで待つ
  while (1) {
    if (adc_result < adc_thres) {
      play_sound(100);
      break;
    }
  }

  while (1) {
    timer_cnt = 700; // タイマー始動
    while (timer_cnt > 0) {
      if (adc_result < adc_thres) {
        // 手をかざした
        if (hand_removed) {
          stop = 1;
          break;
        }
      } else {
        // 手をどかした
        hand_removed = 1;
      }
    }

    --tim_sec;

    /*
     * print_dec の中で LCD 表示を行うが、そこでタイマを使うため、
     * print_dec をまたいだ timer_cnt の使用はできない
     */
    print_dec(tim_sec);

    if (stop) {
      play_sound(100);
      break;
    }
    if (tim_sec == 0) {
      for (int i = 0; i < 5; ++i) {
        play_sound(100);
        delay_ms(100);
      }
      break;
    }
  }

  return 0;
}
