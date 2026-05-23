/*
 *  Copyright (c) 2026 tas0dev
 */

#include "cpu/Vcpu.h"

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t bus_read(uint16_t addr);

void bus_write(uint16_t addr, uint16_t value, uint8_t byt);

uint32_t pmem_read(uint16_t addr);

void pmem_write(uint16_t addr, uint32_t value, uint8_t wenh, uint8_t wenl);
#ifdef __cplusplus
}
#endif

static Vcpu *cpu = nullptr;

#ifdef __cplusplus
extern "C" {
#endif

void cpu_init(void) {
	cpu = new Vcpu;
	cpu->clk = 0;
	cpu->rst = 0;
	cpu->irq = 0;
	cpu->dmem_rdata = 0;
	cpu->pmem_rdata = 0;
	cpu->eval();
}

void cpu_destroy(void) {
	if (cpu) {
		cpu->final();
		delete cpu;
		cpu = nullptr;
	}
}

void cpu_reset(void) {
	cpu->rst = 1;

	for (int i = 0; i < 8; i++) {
		cpu->clk = 0;
		cpu->eval();

		cpu->clk = 1;
		cpu->eval();
	}

	cpu->rst = 0;
	cpu->clk = 0;
	cpu->eval();
}

void cpu_set_irq(uint8_t irq) {
	cpu->irq = irq ? 1 : 0;
	cpu->eval();
}

void cpu_step(void) {
	uint16_t paddr;
	uint16_t daddr;

	cpu->clk = 0;

	paddr = cpu->pmem_addr & 0x3fff;
	cpu->pmem_rdata = pmem_read(paddr) & 0x3ffff;

	if (cpu->dmem_ren) {
		daddr = cpu->dmem_addr & 0x3fff;
		cpu->dmem_rdata = bus_read(daddr);
	}

	cpu->eval();

	cpu->clk = 1;

	paddr = cpu->pmem_addr & 0x3fff;
	cpu->pmem_rdata = pmem_read(paddr) & 0x3ffff;

	if (cpu->dmem_ren) {
		daddr = cpu->dmem_addr & 0x3fff;
		cpu->dmem_rdata = bus_read(daddr);
	}

	cpu->eval();

	if (cpu->dmem_wen) {
		bus_write(
			cpu->dmem_addr & 0x3fff,
			cpu->dmem_wdata,
			cpu->dmem_byt
		);
	}

	if (cpu->pmem_wenl || cpu->pmem_wenh) {
		pmem_write(
			cpu->pmem_addr & 0x3fff,
			cpu->pmem_wdata & 0x3ffff,
			cpu->pmem_wenh,
			cpu->pmem_wenl
		);
	}
}

#ifdef __cplusplus
}
#endif
