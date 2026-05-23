/*
 * Copyright (c) 2026 tas0dev
 */

#include "api.h"

#include <stdio.h>

static void spi_selftest(bemu_cpu_t* cpu) {
  // MMIOアドレス（dos/mmio.h と合わせる）
  const uint16_t SPI_DAT = 0x0020;
  const uint16_t SPI_CTL = 0x0022;

  uint16_t st0 = bemu_cpu_mmio_read16(cpu, SPI_CTL);
  printf("spi: status0=0x%04x (tx_ready=%u cs=%u)\n", st0, st0 & 1u, (st0 >> 1) & 1u);

  bemu_cpu_mmio_write16(cpu, SPI_CTL, 0x0000);
  uint16_t st1 = bemu_cpu_mmio_read16(cpu, SPI_CTL);
  printf("spi: status1=0x%04x (tx_ready=%u cs=%u)\n", st1, st1 & 1u, (st1 >> 1) & 1u);

  bemu_cpu_mmio_write16(cpu, SPI_DAT, 0x00ab);
  uint16_t st2 = bemu_cpu_mmio_read16(cpu, SPI_CTL);
  printf("spi: status2=0x%04x (after write)\n", st2);

  bemu_cpu_step(cpu, 1);
  uint16_t st3 = bemu_cpu_mmio_read16(cpu, SPI_CTL);
  printf("spi: status3=0x%04x (after 1 tick)\n", st3);

  bemu_cpu_mmio_write16(cpu, SPI_CTL, 0x0002);
  uint16_t st4 = bemu_cpu_mmio_read16(cpu, SPI_CTL);
  printf("spi: status4=0x%04x (tx_ready=%u cs=%u)\n", st4, st4 & 1u, (st4 >> 1) & 1u);
}

int main(void) {
  bemu_cpu_t* cpu = bemu_cpu_create();
  if (!cpu) {
    fprintf(stderr, "bemu_cpu_create failed\n");
    return 1;
  }

  if (bemu_cpu_load_ipl(cpu, "../cpu") != 0) {
    fprintf(stderr, "warning: failed to load IPL hex files from ../cpu\n");
  }

  bemu_cpu_reset(cpu);
  bemu_cpu_step(cpu, 1000);

  spi_selftest(cpu);

  for (unsigned i = 0; i < 8; ++i) {
    unsigned addr = 0x0100u + (i * 2u);
    printf("dmem[%04x]=%04x\n", addr, bemu_cpu_dmem_read16(cpu, (uint16_t)addr));
  }

  bemu_cpu_destroy(cpu);
  return 0;
}
