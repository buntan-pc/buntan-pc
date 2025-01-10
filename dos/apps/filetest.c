#include "syscall.h"

unsigned int parse_hex4(char *s) {
  unsigned int val = 0;
  for (int i = 0; i < 4; ++i) {
    char c = *s++;
    if (c >= 'a') {
      c -= 'a' - 10;
    } else if (c >= 'A') {
      c -= 'A' - 10;
    } else {
      c -= '0';
    }
    val = (val << 4) | c;
  }
  return val;
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

int main(int *info) {
  init_syscall(info);
  sys_put_string("Sample application for read/write a file\n", -1);
  sys_put_string("Reading 'COUNTER.TXT'...\n", -1);

  int file_entry[16];
  if (sys_open_entry_fatsd("COUNTER.TXT", file_entry) < 0) {
    sys_put_string("Failed to open COUNTER.TXT\n", -1);
    return -1;
  }

  char block_buf[512];
  if (sys_read_block_fatsd(file_entry, block_buf, 0, 1) < 0) {
    sys_put_string("Failed to read COUNTER.TXT\n", -1);
    return -1;
  }

  unsigned int size_lo = file_entry[14];
  unsigned int size_hi = file_entry[15];
  sys_put_string("Content: ", -1);
  sys_put_string(block_buf, size_lo);
  sys_put_string("\n", -1);

  int count = parse_hex4(block_buf);
  ++count;
  int2hex(count, block_buf, 4);

  if (sys_write_block_fatsd(file_entry, block_buf, 0, 1) < 0) {
    sys_put_string("Failed to write COUNTER.TXT\n", -1);
    return -1;
  }

  return count;
}
