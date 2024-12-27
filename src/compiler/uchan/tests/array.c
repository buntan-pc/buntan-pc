int main() {
  char ac[2] = {3, 5};
  int ai[3] = {2, 3};
  int *p = ai;
  return ac[1] - p[1]; // 2
}
