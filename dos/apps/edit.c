#include "syscall.h"

char text_buf[1600]; // 20x80
int num_line;
int cursor_x;
int cursor_y;

char block_buf[512];

int char_at(int x, int y) {
  return text_buf[80 * y + x];
}

int char_at_cursor() {
  return char_at(cursor_x, cursor_y);
}

void write_at(int x, int y, int c) {
  text_buf[80 * y + x] = c;
}

// カーソルが末尾を飛び越えていたら末尾まで移動させ、タブ文字の途中ならタブ文字の端に移動する
void align_cursor_x() {
  char *line_buf = text_buf + 80 * cursor_y;
  while (cursor_x > 0 & line_buf[cursor_x - 1] == 0) {
    --cursor_x;
    sys_put_string("\x1b[D", -1); // カーソルを左に移動
  }
}

void cursor_up() {
  if (cursor_y > 0) {
    --cursor_y;
    sys_put_string("\x1b[A", -1); // カーソルを上に移動
    align_cursor_x();
  }
}

void cursor_down() {
  if (cursor_y < 19 & cursor_y + 1 < num_line) {
    ++cursor_y;
    sys_put_string("\x1b[B", -1);
    align_cursor_x();
  }
}

void cursor_right() {
  if (cursor_x < 80 && char_at_cursor() != 0) {
    ++cursor_x;
    sys_put_string("\x1b[C", -1);
    // cursor_x == 80 のとき、text_buf からは 1 文字右にはみ出た状態になるので注意
  }
}

void cursor_left() {
  if (cursor_x > 0) {
    --cursor_x;
    sys_put_string("\x1b[D", -1);
  }
}

void print_all_text() {
  for (int line = 0; line < 20; ++line) {
    char *line_buf = text_buf + 80*line;
    if (*line_buf != 0) {
      sys_put_string(line_buf, 80);
    }
    sys_put_string("\x1b[B", -1);
  }
}

void insert_char(int c) {
  switch (c) {
  case '\b':
    if (cursor_x == 0 & cursor_y > 0) {
      // 行を結合する
      char *line_buf = text_buf + 80 * cursor_y;
      char *prev_buf = line_buf - 80;
      cursor_x = 80;
      for (; cursor_x > 0 & prev_buf[cursor_x - 1] == 0; --cursor_x);

      for (int x = cursor_x; x < 80; ++x) {
        prev_buf[x] = *line_buf++;
      }
      // line_buf の内容が prev_buf に結合された

      --cursor_y;

      sys_put_string("\x1b[M\x1b[A\x1b[", -1); // 1 行削除し、カーソルを上に移動
      // カーソルの X 座標を調整し、カーソルを一旦非表示に
      char buf[2];
      int n = sys_int2dec(cursor_x + 1, buf, 2);
      sys_put_string(buf + n, 2 - n);
      sys_put_string("G\x1b[?25l", -1);

      sys_put_string(prev_buf + cursor_x, 80 - cursor_x);

      // カーソルの X 座標を cursor_x に合わせ、カーソルを表示する
      sys_put_string("\x1b[", -1);
      sys_put_string(buf + n, 2 - n);
      sys_put_string("G\x1b[?25h", -1);

      // 行を切り詰める
      char *dst = text_buf + 80 * (cursor_y + 1);
      char *src = dst + 80;
      for (int i = 0; i < 80 * (18 - cursor_y); ++i) {
        dst[i] = src[i];
      }
      --num_line;
    } else if (cursor_x > 0) {
      char *line_buf = text_buf + 80 * cursor_y;
      int tab_x = cursor_x & 0x07;
      int orig_x = cursor_x;
      sys_put_string("\b\x1b[P", -1);
      --cursor_x;
      // ずらし処理
      for (int x = cursor_x; x < 79; ++x) {
        line_buf[x] = line_buf[x + 1];
      }
      line_buf[79] = 0;
    }
    break;
  case '\t':
    if (cursor_x < 80) {
      int tab_x = cursor_x & 0x07;
      int num_space = 8 - tab_x;
      int end_x = cursor_x + num_space;
      char *line_buf = text_buf + 80 * cursor_y;
      for (int x = 79; x > end_x; --x) {
        line_buf[x] = line_buf[x - num_space];
      }
      for (int i = 0; i < num_space & cursor_x < 80; ++i) {
        line_buf[cursor_x++] = ' ';
        sys_put_string("\x1b[@", -1);
      }
      sys_put_string("\t", 1);
    }
    break;
  case '\n':
    if (cursor_y < 19) {
      // 行のずらし処理
      for (int y = 18; y > cursor_y; --y) {
        char *src = text_buf + 80 * y;
        char *dst = src + 80;
        for (int x = 0; x < 80; ++x) {
          *dst++ = *src++;
        }
      }
      char *next_line = text_buf + 80 * (cursor_y + 1);
      for (int i = 0; i < 80; ++i) {
        next_line[i] = 0;
      }
      char *src = text_buf + 80 * cursor_y + cursor_x;
      if (*src != 0) {
        for (int i = cursor_x; i < 80; ++i) {
          *next_line++ = *src;
          *src++ = 0;
        }
      }
      ++cursor_y;
      ++num_line;
      cursor_x = 0;
      // CSI K カーソルから右を削除
      // CSI L 行を挿入
      // CSI ? 25 l カーソル非表示
      sys_put_string("\x1b[K\r\n\x1b[L\x1b[?25l", -1);
      sys_put_string(text_buf + 80 * cursor_y, 80);

      // CSI G カーソルを行頭に移動
      // CSI ? 25 h カーソルを表示
      sys_put_string("\x1b[G\x1b[?25h", -1);
    }
    break;
  default:
    if (cursor_x < 80) {
      char *line_buf = text_buf + 80 * cursor_y;
      int tab_x = cursor_x & 0x07;
      if (line_buf[cursor_x] == 0) {
        line_buf[cursor_x++] = c;
        sys_put_string(&c, 1);
      } else {
        if (char_at_cursor() != 0) {
          // ずらし処理
          for (int x = 79; x > cursor_x; --x) {
            line_buf[x] = line_buf[x - 1];
          }
        }
        line_buf[cursor_x++] = c;
        sys_put_string("\x1b[@", -1); // カーソル位置に空白を挿入
        sys_put_string(&c, 1);
      }
    }
  }
}

unsigned int set_text_to_block_buf() {
  unsigned int len = 0;
  for (unsigned int y = 0; y < num_line; ++y) {
    char *line_buf = text_buf + 80*y;
    for (unsigned int x = 0; x < 80; ++x) {
      int c = line_buf[x];
      if (c == 0) {
        break;
      }
      block_buf[len++] = c;
    }
    block_buf[len++] = '\n';
  }
  return len;
}

void show_error(char *msg, char *val, char *postfix) {
  sys_put_string(msg, -1);
  sys_put_string(val, -1);
  sys_put_string(postfix, -1);
}

void enter_status_mode() {
  sys_put_string("\x1b7\x1b[21;1H", -1);
}

void leave_status_mode() {
  sys_put_string("\x1b8", -1);
}

void save_file(char *file_name, int *file_entry) {
  char buf[5];
  int n;
  unsigned int len = set_text_to_block_buf();
  file_entry[14] = len; // File size low
  file_entry[15] = 0; // File size high

  // ファイルがなければ新たに作り、有れば上書き保存
  if (sys_write_entry_fatsd(file_entry, 3) < 0) { // mode= 1|2 上書き|新規作成
    show_error("Failed to write entry :", file_name, "\n");
    return -1;
  }

  // データ本体を書き込む
  if (sys_write_block_fatsd(file_entry, block_buf, 0, 1) < 0) {
    show_error("Failed to write a block: ", file_name, "\n");
    return -1;
  }

  enter_status_mode();
  sys_put_string("File saved: ", -1);
  sys_put_string(file_name, -1);
  sys_put_string(" (", -1);
  n = sys_int2dec(len, buf, 5);
  sys_put_string(buf + n, 5 - n);
  sys_put_string(" bytes)", -1);
  leave_status_mode();
}

int main(int *info) {
  init_syscall(info);
  int argc = info[2];
  char **argv = info[3];
  int file_entry[16] = {};

  if (argc <= 1) {
    show_error("Usage: ", argv[0], " <file>\n");
    return 1;
  }
  for (int i = 0; i < 1600; ++i) {
    text_buf[i] = 0;
  }
  num_line = cursor_y = cursor_x = 0;

  sys_put_string("\x1b[?1049h", -1); // 代替バッファへ切り替え
  //sys_put_string("\x1b[2J", -1); // 全画面消去
  //sys_put_string("\x1b[H", -1);  // カーソルをホームへ
  sys_put_string("\x1b[1;20r", -1); // スクロール領域を 1 to 20 行に限定

  if (sys_open_entry_fatsd(argv[1], file_entry) == 0) {
    int file_size = file_entry[14];

    // file_size をこのエディタが扱える最大値に制限する
    if (file_entry[15] > 0 || file_size > 512) {
      file_size = 512;
    }

    if (sys_read_block_fatsd(file_entry, block_buf, 0, 1) < 0) {
      show_error("Failed to read: ", argv[1], "\n");
      return 1;
    }

    int x = 0;
    int c;
    char *line_buf = text_buf;
    for (int i = 0; i < file_size && num_line < 20; ++i) {
      c = block_buf[i];
      if (c == '\n') {
        ++num_line;
        x = 0;
        line_buf += 80;
        sys_put_string("\r\n", -1);
      } else if (c == '\t') {
        int num_space = 8 - (x & 0x07);
        for (int i = 0; i < num_space; ++i) {
          line_buf[x++] = ' ';
          sys_put_string(" ", 1);
        }
      } else if (x < 80) {
        line_buf[x++] = c;
        sys_put_string(&c, 1);
      }
    }

    // ファイル末尾が改行でなければ、強制的に改行があることにする
    if (c != '\n') {
      ++num_line;
    }

    sys_put_string("\x1b[H", -1);  // カーソルをホームへ
  }

  if (num_line == 0) {
    num_line = 1;
  }

  enter_status_mode();
  sys_put_string("Editing: ", -1);
  sys_put_string(argv[1], -1);
  leave_status_mode();

  int c = sys_getc();
  while (1) {
    if (c == '\x1b') {
      c = sys_getc();
      if (c == '\x1b') {
        continue;
      } else if (c == '[') {
        c = sys_getc();
        switch (c) {
        case '\x1b':
          continue;
        case 'A':
          cursor_up();
          break;
        case 'B':
          cursor_down();
          break;
        case 'C':
          cursor_right();
          break;
        case 'D':
          cursor_left();
          break;
        }
      }
    } else if (c == 'S' - 0x40) {
      save_file(argv[1], file_entry); // Ctrl-S で保存
    } else if (c == 'X' - 0x40) {
      break; // Ctrl-X でループを抜ける
    } else {
      // 文字の挿入
      insert_char(c);
    }
    c = sys_getc();
  }

  // カーソルを最下行に持って行くことで、DOS プロンプトが正しい位置から再開するようにする
  //sys_put_string("\x1b[22;1H", -1);
  sys_put_string("\x1b[r", -1); // スクロール領域を全画面にリセット
  sys_put_string("\x1b[?1049l", -1); // メインバッファへ戻す

  return 0;
}
