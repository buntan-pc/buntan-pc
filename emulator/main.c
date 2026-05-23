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

  // Load default IPL images from ../cpu (relative to emulator/)
  if (bemu_cpu_load_ipl(cpu, "../cpu") != 0) {
    fprintf(stderr, "warning: failed to load IPL hex files from ../cpu\n");
  }

  bemu_cpu_reset(cpu);
  bemu_cpu_step(cpu, 1000);

  // For quick sanity: dump a few words of dmem from 0x0100
  for (unsigned i = 0; i < 8; ++i) {
    unsigned addr = 0x0100u + (i * 2u);
    printf("dmem[%04x]=%04x\n", addr, bemu_cpu_dmem_read16(cpu, (uint16_t)addr));
  }

  bemu_cpu_destroy(cpu);
  return 0;
}
