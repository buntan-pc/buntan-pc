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
  printf("pmem[0]=0x%05x pmem[1]=0x%05x\n",
         (unsigned)bemu_cpu_pmem_read18(cpu, 0),
         (unsigned)bemu_cpu_pmem_read18(cpu, 1));

  bemu_cpu_reset(cpu);

  // SPIモデルの簡易自己テスト
  spi_selftest(cpu);

  // DOSは起動直後にUART3へ文字列を出力する（SDカードの処理未実装やから途中で止まる）
  printf("ip(before) = 0x%04x\n", bemu_cpu_debug_get_ip(cpu));
  bemu_cpu_step(cpu, 200000);
  printf("ip(after)  = 0x%04x insn=0x%05x\n",
         bemu_cpu_debug_get_ip(cpu),
         (unsigned)bemu_cpu_debug_get_insn(cpu));
  printf("uart3_tx_count=%llu\n", (unsigned long long)bemu_cpu_debug_get_uart3_tx_count(cpu));

  bemu_cpu_destroy(cpu);
  return 0;
}
