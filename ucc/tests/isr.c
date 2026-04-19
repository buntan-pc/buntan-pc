void _ISRfoo() {
}
int buntan_main() {
  asm("push _ISRfoo\n\t"
      "isr\n\t");
  return 0;
}
