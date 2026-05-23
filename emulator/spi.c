/*
 * Copyright (c) 2026 tas0dev
 */

#include "spi.h"

static uint16_t spi_status(const bemu_spi_t* spi) {
  uint16_t v = 0;
  v |= (spi->tx_ready ? 1u : 0u) << 0;
  v |= (spi->cs ? 1u : 0u) << 1;
  return v;
}

void bemu_spi_init(bemu_spi_t* spi) {
  if (!spi) return;
  spi->cs = 0;
  spi->tx_ready = 1;
  spi->pending_tx = 0;
  spi->last_tx = 0;
}

void bemu_spi_reset(bemu_spi_t* spi) { bemu_spi_init(spi); }

void bemu_spi_tick(bemu_spi_t* spi) {
  if (!spi) return;
  if (spi->pending_tx) {
    spi->pending_tx = 0;
    spi->tx_ready = 1;
  }
}

uint16_t bemu_spi_read16(bemu_spi_t* spi, uint16_t addr) {
  if (!spi) return 0;
  switch (addr & 0xfffeu) {
    case 0x0020:
      return (uint16_t)spi->last_tx;
    case 0x0022:
      return spi_status(spi);
    default:
      return 0;
  }
}

void bemu_spi_write16(bemu_spi_t* spi, uint16_t addr, uint16_t value) {
  if (!spi) return;
  switch (addr & 0xfffeu) {
    case 0x0020:
      spi->last_tx = (uint8_t)(value & 0xffu);
      spi->tx_ready = 0;
      spi->pending_tx = 1;
      break;
    case 0x0022:
      spi->cs = (uint8_t)((value >> 1) & 1u);
      break;
    default:
      break;
  }
}
