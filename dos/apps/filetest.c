#include "mmio.h"

int (*syscall)();

int syscall_put_string(char *s, int n) {
  int args[2] = {s, n};
  return syscall(1, args);
}

int syscall_open_fatsd(char *filename, char *file_entry) {
  int args[2] = {filename, file_entry};
  return syscall(3, args);
}

int syscall_read_fatsd(char *file_entry, char *block_buf, unsigned int off, unsigned int len) {
  int args[4] = {file_entry, block_buf, off, len};
  return syscall(4, args);
}

int syscall_write_fatsd(char *file_entry, char *block_buf, unsigned int off, unsigned int len) {
  int args[4] = {file_entry, block_buf, off, len};
  return syscall(5, args);
}

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
  syscall = info[1];
  syscall_put_string("Sample application for read/write a file\n", -1);
  syscall_put_string("Reading 'COUNTER.TXT'...\n", -1);

  int file_entry[16];
  if (syscall_open_fatsd("COUNTER.TXT", file_entry) < 0) {
    syscall_put_string("Failed to open COUNTER.TXT\n", -1);
    return -1;
  }

  char block_buf[512];
  if (syscall_read_fatsd(file_entry, block_buf, 0, 1) < 0) {
    syscall_put_string("Failed to read COUNTER.TXT\n", -1);
    return -1;
  }

  unsigned int size_lo = file_entry[14];
  unsigned int size_hi = file_entry[15];
  syscall_put_string("Content: ", -1);
  syscall_put_string(block_buf, size_lo);
  syscall_put_string("\n", -1);

  int count = parse_hex4(block_buf);
  ++count;
  int2hex(count, block_buf, 4);

  if (syscall_write_fatsd(file_entry, block_buf, 0, 1) < 0) {
    syscall_put_string("Failed to write COUNTER.TXT\n", -1);
    return -1;
  }

  return count;
}
