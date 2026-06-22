#include <stdarg.h>
#include <string.h>
// syscall.h と printf.h は <stdio.h> より先にインクルードする。
// <stdio.h> 後だと putc マクロが printf.h の static void putc(int) と衝突する。
#include "../syscall.h"
#include "../printf.h"
#include <stdio.h>

// --- output capture ---
static char out_buf[256];
static int out_len;
static int capturing;

static void capture_start(void) {
  out_len = 0;
  capturing = 1;
}

static void capture_stop(void) {
  capturing = 0;
}

static int capture_putc(char c) {
  if (out_len < (int)sizeof(out_buf)) {
    out_buf[out_len++] = c;
  }
  return c;
}

static int capture_equals(const char *want) {
  size_t len = strlen(want);
  if ((size_t)out_len != len) {
    return 0;
  }
  return strncmp(out_buf, want, len) == 0;
}

int buntan_syscall(int num, int *args);

// sys_put_string (num=1) をラップし、他は buntan_syscall に委譲
static int buntan_syscall_wrapper(int num, int *args) {
  if (num != 1) {
    return buntan_syscall(num, args);
  }

  char *s = (char *)args[0];
  int len = args[1];
  int i;
  int (*putc_func)(int) = putchar;
  if (capturing) {
    putc_func = capture_putc;
  }
  if (len < 0) {
    for (i = 0; s[i] != '\0'; i++) {
      putc_func(s[i]);
    }
  } else {
    for (i = 0; i < len; i++) {
      putc_func(s[i]);
    }
  }
  return i;
}

// --- test framework ---
static int pass_count;
static int fail_count;

static void check(const char *expected) {
  if (capture_equals(expected)) {
    pass_count++;
    printf("[PASS] got: \"%s\"\n",
           expected);
  } else {
    fail_count++;
    printf("[FAIL] got: \"%.*s\" want: \"%s\"\n",
           out_len, out_buf, expected);
  }
}

#define TEST(expected, ...) \
  do { capture_start(); buntan_printf(__VA_ARGS__); capture_stop(); check(expected); } while (0)

int main() {
  int info[4] = {0, (int)(void *)buntan_syscall_wrapper, 0, 0};
  init_syscall(info);

  pass_count = 0;
  fail_count = 0;

  //   want        fmt, ...
  TEST("hello\n%", "hello\n%%");
  TEST("0",        "%d", 0);
  TEST("A",        "%c", 'A');
  TEST("10",       "%X", 0x10);
  TEST("d=32767",  "d=%d", 0x7FFF);
  TEST("X=ABCD",   "X=%X", 0xABCD);

  if (fail_count == 0) {
    printf("All %d tests passed.\n", pass_count);
    return 0;
  }
  printf("%d tests FAILED.\n", fail_count);
  return 1;
}
