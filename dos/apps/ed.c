#include "syscall.h"

char text_buf[513];
unsigned int text_len;
char *cur_line; // 現在アドレスされている行の先頭

int strcmp(char *a, char *b) {
  int c;
  while ((c = *a++)) {
    int v = c - *b++;
    if (v != 0 | c == 0) {
      return v;
    }
  }
  return 0;
}

char *parse_int(char *p, int *val) {
  int v = 0;
  int c = *p;
  while ('0' <= c & c <= '9') {
    v = 10*v + c - '0';
    c = *++p;
  }
  *val = v;
  return p;
}

// コマンド入力をパースし、アドレス範囲とコマンド文字を返す。
// アドレスが範囲ではない場合は addr_end が指定されたものとみなす。
int parse_cmd(char *line, int *addr_begin, int *addr_end) {
  char *p = line;
  int c = *p;
  int val = 0x8000;

  if ('0' <= c & c <= '9') {
    c = *(p = parse_int(p, &val));
  }
  if (c != ',') {
    *addr_end = val;
  } else { // アドレス範囲
    *addr_begin = val;
    c = *++p;
    if ('0' <= c & c <= '9') {
      p = parse_int(p, addr_end);
    } else {
      *addr_end = 0x7fff;
    }
  }
  if ('a' <= c & c <= 'z') {
    return c;
  }
  return 0;
}

int save_file(int *file_entry) {
  file_entry[14] = text_len; // File size low
  file_entry[15] = 0; // File size high

  // ファイルがなければ新たに作り、有れば上書き保存
  if (sys_write_entry_fatsd(file_entry, 3) < 0) { // mode= 1|2 上書き|新規作成
    return -1;
  }

  // データ本体を書き込む
  if (sys_write_block_fatsd(file_entry, text_buf, 0, 1) < 0) {
    return -1;
  }

  return 0;
}

void put_question() {
  sys_put_string("?\n", 2);
}

void put_text_len() {
  char buf[3];
  int n = sys_int2dec(text_len, buf, 3);
  sys_put_string(buf + n, 3 - n);
  sys_put_string("\n", 1);
}

void update_cur_line(int addr) {
  if (addr <= 1) {
    cur_line = text_buf;
    return;
  }

  int line = 1;
  for (int i = 0; i < text_len; ++i) {
    if (line == addr) {
      cur_line = text_buf + i;
      return;
    }

    if (text_buf[i] == '\n') {
      ++line;
    }
  }
}

// 追加された文字数を返す
int append(int addr) {
  if (addr != 0) {
    while (*cur_line++ != '\n');
  }

  char line[64];
  unsigned int initial_len = text_len;

  while (1) {
    int len = sys_get_string(line, 64);
    if (len == 1 & line[0] == '.') {
      break;
    } else if (text_len + len + 1 >= 512) {
      put_question();
      break;
    }

    char *src = text_buf + text_len;
    char *dst = src + len + 1;
    while (src > cur_line) {
      *--dst = *--src;
    }
    for (int i = 0; i < len; ++i) {
      *cur_line++ = line[i];
    }
    *cur_line++ = '\n';

    text_len += len + 1;
  }

  return text_len - initial_len;
}

int main(int *info) {
  init_syscall(info);
  int argc = info[2];
  char **argv = info[3];
  int file_entry[16] = {};
  char buf[4];

  if (argc <= 1) {
    sys_put_string("Usage: ed <file>\n", -1);
    return 1;
  }
  for (int i = 0; i < 513; ++i) {
    text_buf[i] = 0;
  }
  text_len = 0;
  cur_line = text_buf;

  if (sys_open_entry_fatsd(argv[1], file_entry) == 0) {
    if (sys_read_block_fatsd(file_entry, text_buf, 0, 1) < 0) {
      put_question();
      return 1;
    }

    text_len = file_entry[14];
    if (file_entry[15] > 0 || text_len > 512) {
      text_len = 512;
    }

    // ファイル末尾は必ず改行文字
    if (text_len > 0 & text_buf[text_len - 1] != '\n') {
      text_buf[text_len++] = '\n';
    }

    put_text_len();
  }

  int need_save = 0;
  int prev_cmd = 0;
  char line[64];
  while (1) {
    sys_get_string(line, 64);
    int addr_begin = 0x8000;
    int addr_end = 0x8000;
    int cmd = parse_cmd(line, &addr_begin, &addr_end);

    if (addr_end != 0x8000) {
      update_cur_line(addr_end);
    }

    switch (cmd) {
    case 'a':
      if (append(addr_end) > 0) {
        need_save = 1;
      }
      break; // 'a'

    case 'q':
      if (need_save == 0 | prev_cmd == 'q') {
        return 0;
      }
      put_question();
      break;

    case 'w':
      if (save_file(file_entry) == 0) {
        need_save = 0;
        put_text_len();
      } else {
        put_question();
      }
      break;

    default:
      put_question();
    }

    prev_cmd = cmd;
  }

  return 0;
}
