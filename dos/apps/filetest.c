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

int main(int *info) {
  syscall = info[1];
  syscall_put_string("Sample application for reading a file\n", -1);
  syscall_put_string("Reading 'README'...\n", -1);

  int file_entry[16];
  if (syscall_open_fatsd("README", file_entry) < 0) {
    syscall_put_string("Failed to open README\n", -1);
    return 1;
  }

  char block_buf[512];
  if (syscall_read_fatsd(file_entry, block_buf, 0, 1) < 0) {
    syscall_put_string("Failed to read README\n", -1);
    return 1;
  }

  unsigned int size_lo = file_entry[14];
  unsigned int size_hi = file_entry[15];
  syscall_put_string("Content:\n", -1);
  syscall_put_string(block_buf, size_lo);

  return 0;
}
