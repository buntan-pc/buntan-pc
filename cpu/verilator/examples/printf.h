int buntan_printf(char *fmt, ...) {
  int len = 0;
  int c;
  char s[6];
  va_list ap;
  va_start(ap, fmt);

  while ((c = *fmt++) != 0) {
    if (c != '%' || (c = *fmt++) == '%') {
      sys_put_string(&c, 1);
      ++len;
      continue;
    }
    if (c == 'c') {
      s[0] = va_arg(ap, int);
      sys_put_string(s, 1);
      ++len;
    } else if (c == 'd') {
      int nzero = sys_int2dec(va_arg(ap, int), s, 5);
      int digit = 5 - nzero;
      sys_put_string(s + nzero, digit);
      len += digit;
    } else if (c == 'X') {
      int nzero = sys_int2hex(va_arg(ap, int), s, 4);
      int digit = 4 - nzero;
      sys_put_string(s + nzero, digit);
      len += digit;
    }
  }
  return len;
}
