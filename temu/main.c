// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 tas0dev
 */

#include <stdio.h>

#include "api.h"
#include "debug.h"

#define DEBUG 1

int main(void) {
  bemu_cpu_t* cpu = bemu_cpu_create();
  if (!cpu) {
    fprintf(stderr, "bemu_cpu_create failed\n");
    return 1;
  }

  // DOSをロードして起動する
  int dos_ok = 0;
  if (bemu_cpu_load_pmem_hex(cpu, "../dos/dos.pmem.hex") == 0 &&
      bemu_cpu_load_dmem_hex16(cpu, "../dos/dos.dmem.hex") == 0) {
    dos_ok = 1;
  } else if (bemu_cpu_load_pmem_hex(cpu, "dos/dos.pmem.hex") == 0 &&
             bemu_cpu_load_dmem_hex16(cpu, "dos/dos.dmem.hex") == 0) {
    dos_ok = 1;
  }
  if (!dos_ok) {
    debug("error: failed to load DOS hex files (../dos or dos)\n");
    bemu_cpu_destroy(cpu);
    return 1;
  }

  bemu_cpu_reset(cpu);

  int i = 0;
  while (1) {
    bemu_cpu_step(cpu, 100000);

    debug("step=%d spi_shift_count=%llu uart3_tx_count=%llu\n", i,
          (unsigned long long)bemu_cpu_debug_get_spi_shift_count(cpu),
          (unsigned long long)bemu_cpu_debug_get_uart3_tx_count(cpu));

    i++;
  }

  bemu_cpu_destroy(cpu);
  return 0;
}
