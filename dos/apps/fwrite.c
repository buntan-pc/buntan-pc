#include "syscall.h"

int main(int *info) {
  char block_buf[512];
  init_syscall(info);
  sys_put_string("fwrite.exe\n", -1);
  sys_put_string("Reading 'FWRITE.TXT'\n", -1);

  int file_entry[16] = {};

  if (sys_open_entry_fatsd("FWRITE.TXT", file_entry) < 0) {
    sys_put_string("No such file. Creating 'FWRITE.TXT'\n", -1);
    file_entry[14] = 1; // File size low
    file_entry[15] = 0; // File size high
    if (sys_write_entry_fatsd(file_entry, 2) < 0) { // mode=2 新規作成
      sys_put_string("Failed to create an entry\n", -1);
      return -1;
    }
    // この時点で file_entry の FstClusLO/HI が設定されているはず
    block_buf[0] = 'A';
    if (sys_write_block_fatsd(file_entry, block_buf, 0, 1) < 0) {
      sys_put_string("Failed to write a block\n", -1);
      return -1;
    }

    sys_put_string("'FWRITE.TXT' is created with initial data\n", -1);
    return 0;
  }

  if (sys_read_block_fatsd(file_entry, block_buf, 0, 1) < 0) {
    sys_put_string("Failed to read 'FWRITE.TXT'\n", -1);
    return -1;
  }

  unsigned int size_lo = file_entry[14];
  unsigned int size_hi = file_entry[15];
  sys_put_string("Content: ", -1);
  sys_put_string(block_buf, size_lo);
  sys_put_string("\n", -1);

  if (size_hi != 0 || size_lo >= 512) {
    sys_put_string("File is too big to append data\n", -1);
    return 0;
  }

  char next_char = 'A' + size_lo;
  if (next_char < 0x20 || 0x7F <= next_char) {
    next_char = 0x20;
  }
  block_buf[size_lo] = next_char;
  ++file_entry[14]; // File size low

  if (sys_write_block_fatsd(file_entry, block_buf, 0, 1) < 0) {
    sys_put_string("Failed to write block to 'FWRITE.TXT'\n", -1);
    return -1;
  }
  if (sys_write_entry_fatsd(file_entry, 1) < 0) { // mode=1 上書き
    sys_put_string("Failed to write entry for 'FWRITE.TXT'\n", -1);
    return -1;
  }

  sys_put_string("A char '", -1);
  sys_put_string(&next_char, 1);
  sys_put_string("' is appended to 'FWRITE.TXT'\n", -1);
  return next_char;
}
