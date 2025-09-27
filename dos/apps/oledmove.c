#include "mmio.h"
#include "delay.h"
#include "syscall.h"

int wait_ready() {
  while ((i2c_status & 0x2) == 0);
  return (i2c_status >> 3) & 1; // ACK
}

// アドレスかデータ 1 バイトを送る
int i2c_send(int val) {
  i2c_data = val;
  return wait_ready();
}

// ストップビットを送る
void i2c_stop() {
  i2c_status = 4;
  wait_ready();
}


/*
 * OLED ディスプレイコントローラ SSD1306
 * I2C アドレス = 0x78
 *
 * I2C write フォーマット  0:addr 1:ctrl 2:data
 *
 * ctrl byte
 * - bit7 Co (Continuation)
 *   - Co=1 で 1 バイトずつの ctrl と data の組が連なる。
 *   - Co=0 で 1 バイトの ctrl と 0 バイト以上の data が続く。
 *
 * - bit6 D/C# (Data/command)
 *   - D/C#=1 なら data はデータ
 *   - D/C#=0 なら data はコマンド
 */

// OLED ディスプレイへの通信を開始
void oled_start(char ctrl) {
  i2c_send(0x78);
  i2c_send(ctrl);
}

// OLED ディスプレイの可変長コマンドモードを開始
void oled_vcmd() {
  oled_start(0x00); // 可変長コマンド（Co=0, D/C#=0）
}

// OLED ディスプレイの可変長データモードを開始
void oled_vdata() {
  oled_start(0x40); // 可変長コマンド（Co=0, D/C#=1）
}

// OLED ディスプレイを初期化
void oled_init() {
  oled_vcmd();
  i2c_send(0xA1); // セグメントリマップ（左右の描画方向設定）
  i2c_send(0xAF); // display ON
  i2c_send(0x8D); // チャージポンプ設定
  i2c_send(0x14); // チャージポンプ ON
  i2c_send(0x20); // アドレッシングモード設定
  i2c_send(0x02); // ページアドレッシングモード
  i2c_send(0x00); // 列開始アドレスの下位 4 ビットを 0 に設定
  i2c_send(0x10); // 列開始アドレスの上位 4 ビットを 0 に設定
  i2c_stop();
}

void draw_cursor(char x, char y, char pattern) {
  oled_vcmd();
  i2c_send(x & 0x0f); // set lower column start addr
  i2c_send((x >> 4) | 0x10); // set higher column start addr
  i2c_send((y >> 3) | 0xB0); // set page start address
  i2c_stop();

  oled_vdata();
  int i;
  for (i = 0; i < 8; ++i) {
    i2c_send(pattern);
  }
  i2c_stop();
}

char patterns[4] = { 0xAA, 0xFF, 0xC3, 0x3C };

int main(int *info) {
  init_syscall(info);

  sys_put_string("Q: quit\n", -1);
  sys_put_string("C: change cursor\n", -1);

  oled_init();

  // 画面の初期化:黒塗り
  for (int page = 0; page < 8; page++) {
    oled_vcmd();
    i2c_send(0xB0 | page);
    i2c_stop();

    oled_vdata();
    for (int x = 0; x < 128; x++) {
      i2c_send(0x00);
    }
    i2c_stop();
  }

  char x = 64;
  char y = 32;
  int pat_i = 0;

  draw_cursor(x, y, patterns[pat_i]);

  while (1) {
    int c = sys_getc();

    draw_cursor(x, y, 0x00);
    if (c == 'q') {
      break;
    } else if (c == 0x1C) {
      if (y < 56) {
        y += 8;
      }
    } else if (c == 0x1F) {
      if (x >= 8) {
        x -= 8;
      }
    } else if (c == 0x1D) {
      if (y >= 8) {
        y -= 8;
      }
    } else if (c == 0x1E) {
      if (x < 120) {
        x += 8;
      }
    } else if (c == 'c') {
      pat_i = (pat_i + 1) & 3;
    }

    draw_cursor(x, y, patterns[pat_i]);
  }

  return 0;
}
