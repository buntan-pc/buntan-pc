int init_syscall(int *info) {
  return 0;
}

int add(int a, ...) {
  va_list ap;
  va_start(ap, a);
  int b = va_arg(ap, int);
  return a + b;
}

int buntan_main(int *info) {
  // 誤って info が 1 回しか使われないと認識されないことをテスト
  init_syscall(info);

  // 可変長引数をテスト
  return add(info[0], info[2]); // info[0] = addr of .text, info[2] = argc
}
