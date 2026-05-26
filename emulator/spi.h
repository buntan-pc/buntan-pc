/*
 * Copyright (c) 2026 tas0dev
 */

#ifndef BUNTAN_EMULATOR_SPI_H
#define BUNTAN_EMULATOR_SPI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bemu_spi {
  uint8_t cs;
  uint8_t tx_ready;
  uint8_t pending_tx;
  uint8_t last_tx;
} bemu_spi_t;

void bemu_spi_init(bemu_spi_t* spi);
void bemu_spi_reset(bemu_spi_t* spi);
void bemu_spi_tick(bemu_spi_t* spi);
uint16_t bemu_spi_read16(bemu_spi_t* spi, uint16_t addr);
void bemu_spi_write16(bemu_spi_t* spi, uint16_t addr, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* BUNTAN_EMULATOR_SPI_H */
