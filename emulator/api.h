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
uint16_t bemu_cpu_mmio_read16(bemu_cpu_t* cpu, uint16_t addr);
void bemu_cpu_mmio_write16(bemu_cpu_t* cpu, uint16_t addr, uint16_t value);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* BUNTAN_EMULATOR_API_H */
