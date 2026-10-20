int fib(int n) {
  if (n < 3) {
    return 1;
  }
  return fib(n - 1) + fib(n - 2);
}

int buntan_main() {
  return fib(6);
}
