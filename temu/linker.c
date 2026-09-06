// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 tas0dev
 */

#include <stdio.h>

#ifdef TEMU_LINK_DOS_MAIN

/// DOS側のエントリポイント
typedef int (*dos_main_fn_t)(void);
extern int buntan_main(void) __attribute__((weak));

static dos_main_fn_t get_dos_main(void) {
  return buntan_main ? (dos_main_fn_t)buntan_main : (dos_main_fn_t)0;
}

int main(int argc, char** argv) {
  dos_main_fn_t dos = get_dos_main();
  if (dos) {
    (void)argc;
    (void)argv;
    return dos();
  }

  fprintf(stderr, "error: no entrypoint found (expected `buntan_main`)\n");
  return 1;
}
#endif
