// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 tas0dev
 */

#ifndef BUNTAN_EMULATOR_API_H
#define BUNTAN_EMULATOR_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bemu_cpu bemu_cpu_t;

bemu_cpu_t* bemu_cpu_create(void);
void bemu_cpu_destroy(bemu_cpu_t* cpu);
void bemu_cpu_reset(bemu_cpu_t* cpu);
void bemu_cpu_set_irq(bemu_cpu_t* cpu, int level);
void bemu_cpu_step(bemu_cpu_t* cpu, uint32_t cycles);
uint16_t bemu_cpu_dmem_read16(bemu_cpu_t* cpu, uint16_t addr);
void bemu_cpu_dmem_write16(bemu_cpu_t* cpu, uint16_t addr, uint16_t value);
uint32_t bemu_cpu_pmem_read18(bemu_cpu_t* cpu, uint16_t addr);
void bemu_cpu_pmem_write18(bemu_cpu_t* cpu, uint16_t addr, uint32_t value18);
int bemu_cpu_load_ipl(bemu_cpu_t* cpu, const char* cpu_dir);
int bemu_cpu_load_pmem_hex(bemu_cpu_t* cpu, const char* pmem_hex_path);
int bemu_cpu_load_dmem_hex16(bemu_cpu_t* cpu, const char* dmem_hex_path);
uint16_t bemu_cpu_mmio_read16(bemu_cpu_t* cpu, uint16_t addr);
void bemu_cpu_mmio_write16(bemu_cpu_t* cpu, uint16_t addr, uint16_t value);
uint16_t bemu_cpu_debug_get_ip(bemu_cpu_t* cpu);
uint32_t bemu_cpu_debug_get_insn(bemu_cpu_t* cpu);
uint64_t bemu_cpu_debug_get_uart3_tx_count(bemu_cpu_t* cpu);
uint64_t bemu_cpu_debug_get_spi_shift_count(bemu_cpu_t* cpu);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* BUNTAN_EMULATOR_API_H */
