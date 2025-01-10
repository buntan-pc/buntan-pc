#include "syscall.h"

void print_dec(int val) {
  char buf[5];
  int nzero = sys_int2dec(val, buf, 5);
  sys_put_string(buf + nzero, 5 - nzero);
}

void print_hex(int val) {
  char buf[4];
  sys_int2hex(val, buf, 4);
  sys_put_string(buf, 4);
}

int main(int *info) {
  init_syscall(info);
  int argc = info[2];
  char **argv = info[3];

  char buf[5];

  sys_put_string("Argument printer\n", -1);
  sys_put_string(  "info[0] .text addr  : ", -1);
  print_hex(info[0]);
  sys_put_string("\ninfo[1] syscall addr: ", -1);
  print_hex(info[1]);
  sys_put_string("\ninfo[2] argc        : ", -1);
  print_dec(info[2]);
  sys_put_string("\ninfo[3] argv        : ", -1);
  print_hex(info[3]);
  sys_put_string("\n", -1);

  for (int i = 0; i < argc; ++i) {
    sys_put_string("argv[", -1);
    print_dec(i);
    sys_put_string("]: \"", -1);
    sys_put_string(argv[i], -1);
    sys_put_string("\"\n", -1);
  }

  return 0;
}
