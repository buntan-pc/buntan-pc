int (*syscall)();

int init_syscall(int *info) {
  syscall = info[1];
}

int sys_get_os_version() {
  return syscall(0, 0);
}

int sys_put_string(char *s, int n) {
  int args[2] = {s, n};
  return syscall(1, args);
}

int sys_get_string(char *s, int n) {
  int args[2] = {s, n};
  return syscall(2, args);
}

int sys_open_entry_fatsd(char *filename, char *file_entry) {
  int args[2] = {filename, file_entry};
  return syscall(3, args);
}

int sys_read_block_fatsd(char *file_entry, char *block_buf, unsigned int off, unsigned int len) {
  int args[4] = {file_entry, block_buf, off, len};
  return syscall(4, args);
}

int sys_write_block_fatsd(char *file_entry, char *block_buf, unsigned int off, unsigned int len) {
  int args[4] = {file_entry, block_buf, off, len};
  return syscall(5, args);
}

int sys_write_entry_fatsd(char *file_entry, unsigned int mode) {
  int args[2] = {file_entry, mode};
  return syscall(6, args);
}

int sys_int2hex(int val, char *s, int n) {
  int args[3] = {val, s, n};
  return syscall(7, args);
}

int sys_int2dec(int val, char *s, int n) {
  int args[3] = {val, s, n};
  return syscall(8, args);
}

int sys_getc() {
  return syscall(9, 0);
}
