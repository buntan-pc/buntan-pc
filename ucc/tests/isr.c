void _ISRfoo() {
}
int main() {
  asm("push _ISRfoo\n\t"
      "isr\n\t");
  return 0;
}
