int gv = 3;

int main(int *info) {
  return gv + (&gv & 0xfff);
}
