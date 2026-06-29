// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 tas0dev
 */
#include "debug.h"

#include <stdarg.h>
#include <stdio.h>

void debug(const char* fmt, ...) {
#ifdef DEBUG
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
#endif
}
