/*
 * Copyright (c) 2026 tas0dev
 */

#include "api.h"

#include <stdio.h>

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
    fprintf(stderr, "warning: failed to load DOS hex files (../dos or dos)\n");
  }
  bemu_cpu_reset(cpu);

  // UART3 出力を見ながら実行を進める
  bemu_cpu_step(cpu, 5000000);
  fprintf(stderr, "spi_shift_count=%llu uart3_tx_count=%llu\n",
          (unsigned long long)bemu_cpu_debug_get_spi_shift_count(cpu),
          (unsigned long long)bemu_cpu_debug_get_uart3_tx_count(cpu));

  bemu_cpu_destroy(cpu);
  return 0;
}
