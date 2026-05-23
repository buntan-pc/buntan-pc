/*
 * Copyright (c) 2026 tas0dev
 */

#include "api.h"
#include <verilated.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "Vcpu.h"
#include "spi.h"

namespace {

constexpr uint32_t kAddrWidth = 14;
constexpr uint32_t kAddrMask = (1u << kAddrWidth) - 1u;  // 0x3fff

constexpr uint32_t kDmemWords = 1u << (kAddrWidth - 1);  // 0x2000 words (16-bit)
constexpr uint32_t kPmemWords = 1u << kAddrWidth;        // 0x4000 words (18-bit)

static bool readmemh_like(const std::string& path, std::vector<uint32_t>* out_words,
                          uint32_t start_index, uint32_t bitmask) {
  FILE* f = std::fopen(path.c_str(), "r");
  if (!f) return false;

  char line[256];
  uint32_t idx = start_index;
  while (std::fgets(line, sizeof(line), f)) {
    const char* p = line;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == '\0' || *p == '\n' || *p == '#') continue;
    if (*p == '/' && *(p + 1) == '/') continue;
    if (*p == '@') {
      // Address directive like "@0010"
      ++p;
      char* endp = nullptr;
      unsigned long addr = std::strtoul(p, &endp, 16);
      if (endp == p) {
        std::fclose(f);
        return false;
      }
      idx = static_cast<uint32_t>(addr);
      continue;
    }

    char* endp = nullptr;
    unsigned long val = std::strtoul(p, &endp, 16);
    if (endp == p) continue;
    if (idx >= out_words->size()) break;
    (*out_words)[idx] = static_cast<uint32_t>(val) & bitmask;
    ++idx;
  }

  std::fclose(f);
  return true;
}

static std::string join_path(const char* dir, const char* file) {
  if (!dir || !*dir) return std::string(file);
  std::string d(dir);
  if (!d.empty() && d.back() != '/') d.push_back('/');
  d += file;
  return d;
}

}  // namespace

struct bemu_cpu {
  VerilatedContext* context = nullptr;
  Vcpu* top = nullptr;

  // dmem: split into lo/hi bytes, like cpu/mem.sv
  std::vector<uint8_t> dmem_lo;
  std::vector<uint8_t> dmem_hi;

  // pmem: 18-bit per entry, stored in uint32_t
  std::vector<uint32_t> pmem;

  // Registered read data (synchronous read)
  uint16_t dmem_rdata_reg = 0;
  uint32_t pmem_rdata_reg = 0;  // 18-bit

  // Last sampled addresses (for sync read)
  uint16_t dmem_addr_q = 0;
  uint16_t pmem_addr_q = 0;

  bemu_spi_t spi{};

  bemu_cpu()
      : dmem_lo(kDmemWords, 0),
        dmem_hi(kDmemWords, 0),
        pmem(kPmemWords, 0) {}

  void drive_inputs() {
    top->dmem_rdata = dmem_rdata_reg;
    top->pmem_rdata = pmem_rdata_reg;
  }

  void eval_settle() { top->eval(); }

  void posedge_mem() {
    bemu_spi_tick(&spi);

    // Sample addresses at the clock edge (matching synchronous RAM behavior).
    const uint16_t daddr = static_cast<uint16_t>(top->dmem_addr) & kAddrMask;
    const uint16_t paddr = static_cast<uint16_t>(top->pmem_addr) & kAddrMask;

    // Read-before-write semantics (matches nonblocking behavior in cpu/mem.sv).
    {
      // MMIO window (0x0000-0x00ff). For now only SPI is emulated.
      if ((daddr & 0xff00u) == 0x0000u) {
        dmem_rdata_reg = bemu_spi_read16(&spi, daddr);
      } else {
        const uint32_t widx = (daddr >> 1) & (kDmemWords - 1);
        dmem_rdata_reg = static_cast<uint16_t>(dmem_lo[widx]) |
                         static_cast<uint16_t>(static_cast<uint16_t>(dmem_hi[widx]) << 8);
      }
      dmem_addr_q = daddr;
    }
    {
      pmem_rdata_reg = pmem[paddr] & 0x3ffffu;
      pmem_addr_q = paddr;
    }

    // Writes
    if (top->dmem_wen) {
      if ((daddr & 0xff00u) == 0x0000u) {
        // For now, forward as 16-bit MMIO write (byte writes are expanded by CPU already).
        bemu_spi_write16(&spi, daddr, static_cast<uint16_t>(top->dmem_wdata));
      } else {
        const uint32_t widx = (daddr >> 1) & (kDmemWords - 1);
        const uint16_t din = static_cast<uint16_t>(top->dmem_wdata);
        const bool byt = (top->dmem_byt != 0);
        const bool addr_lsb = (daddr & 1u) != 0;

        const bool wen_lo = (!byt) || (!addr_lsb);
        const bool wen_hi = (!byt) || (addr_lsb);

        if (wen_lo) dmem_lo[widx] = static_cast<uint8_t>(din & 0xffu);
        if (wen_hi) dmem_hi[widx] = static_cast<uint8_t>((din >> 8) & 0xffu);
      }
    }

    if (top->pmem_wenh || top->pmem_wenl) {
      const uint32_t din = static_cast<uint32_t>(top->pmem_wdata) & 0x3ffffu;
      uint32_t cur = pmem[paddr] & 0x3ffffu;
      if (top->pmem_wenh) cur = (cur & 0x0ffffu) | (din & 0x30000u);
      if (top->pmem_wenl) cur = (cur & 0x30000u) | (din & 0x0ffffu);
      pmem[paddr] = cur;
    }

    drive_inputs();
  }

  void tick_one_cycle() {
    // Ensure inputs are visible before the clock edge.
    top->clk = 0;
    drive_inputs();
    eval_settle();

    // Rising edge
    top->clk = 1;
    eval_settle();
    posedge_mem();

    // Falling edge
    top->clk = 0;
    eval_settle();

    context->timeInc(1);
  }
};

extern "C" {

bemu_cpu_t* bemu_cpu_create(void) {
  auto* cpu = new bemu_cpu();
  cpu->context = new VerilatedContext();
  cpu->context->debug(0);
  cpu->context->randReset(0);
  cpu->top = new Vcpu(cpu->context);

  cpu->top->rst = 1;
  cpu->top->irq = 0;
  cpu->top->clk = 0;
  bemu_spi_init(&cpu->spi);
  cpu->drive_inputs();
  cpu->eval_settle();

  // Deassert reset after a couple cycles to match typical usage.
  cpu->tick_one_cycle();
  cpu->tick_one_cycle();
  cpu->top->rst = 0;
  cpu->eval_settle();

  return cpu;
}

void bemu_cpu_destroy(bemu_cpu_t* cpu) {
  if (!cpu) return;
  cpu->top->final();
  delete cpu->top;
  delete cpu->context;
  delete cpu;
}

void bemu_cpu_reset(bemu_cpu_t* cpu) {
  if (!cpu) return;
  cpu->top->rst = 1;
  bemu_spi_reset(&cpu->spi);
  cpu->eval_settle();
  cpu->tick_one_cycle();
  cpu->tick_one_cycle();
  cpu->top->rst = 0;
  cpu->eval_settle();
}

void bemu_cpu_set_irq(bemu_cpu_t* cpu, int level) {
  if (!cpu) return;
  cpu->top->irq = (level ? 1 : 0);
  cpu->eval_settle();
}

void bemu_cpu_step(bemu_cpu_t* cpu, uint32_t cycles) {
  if (!cpu) return;
  for (uint32_t i = 0; i < cycles; ++i) cpu->tick_one_cycle();
}

uint16_t bemu_cpu_dmem_read16(bemu_cpu_t* cpu, uint16_t addr) {
  if (!cpu) return 0;
  addr &= static_cast<uint16_t>(kAddrMask);
  const uint32_t widx = (addr >> 1) & (kDmemWords - 1);
  return static_cast<uint16_t>(cpu->dmem_lo[widx]) |
         static_cast<uint16_t>(static_cast<uint16_t>(cpu->dmem_hi[widx]) << 8);
}

void bemu_cpu_dmem_write16(bemu_cpu_t* cpu, uint16_t addr, uint16_t value) {
  if (!cpu) return;
  addr &= static_cast<uint16_t>(kAddrMask);
  const uint32_t widx = (addr >> 1) & (kDmemWords - 1);
  cpu->dmem_lo[widx] = static_cast<uint8_t>(value & 0xffu);
  cpu->dmem_hi[widx] = static_cast<uint8_t>((value >> 8) & 0xffu);
}

uint32_t bemu_cpu_pmem_read18(bemu_cpu_t* cpu, uint16_t addr) {
  if (!cpu) return 0;
  addr &= static_cast<uint16_t>(kAddrMask);
  return cpu->pmem[addr] & 0x3ffffu;
}

void bemu_cpu_pmem_write18(bemu_cpu_t* cpu, uint16_t addr, uint32_t value18) {
  if (!cpu) return;
  addr &= static_cast<uint16_t>(kAddrMask);
  cpu->pmem[addr] = value18 & 0x3ffffu;
}

int bemu_cpu_load_ipl(bemu_cpu_t* cpu, const char* cpu_dir) {
  if (!cpu) return -1;

  // pmem
  if (!readmemh_like(join_path(cpu_dir, "ipl.pmem.hex"), &cpu->pmem, 0, 0x3ffffu)) {
    return -2;
  }

  // dmem lo/hi are byte-wide; load to temp 32-bit vectors then narrow.
  std::vector<uint32_t> tmp_lo(kDmemWords, 0);
  std::vector<uint32_t> tmp_hi(kDmemWords, 0);

  // In cpu/mem.sv, $readmemh loads starting at (0x100 >> 1)
  const uint32_t start = (0x100u >> 1);
  if (!readmemh_like(join_path(cpu_dir, "ipl.dmem_lo.hex"), &tmp_lo, start, 0xffu)) {
    return -3;
  }
  if (!readmemh_like(join_path(cpu_dir, "ipl.dmem_hi.hex"), &tmp_hi, start, 0xffu)) {
    return -4;
  }
  for (uint32_t i = 0; i < kDmemWords; ++i) {
    cpu->dmem_lo[i] = static_cast<uint8_t>(tmp_lo[i] & 0xffu);
    cpu->dmem_hi[i] = static_cast<uint8_t>(tmp_hi[i] & 0xffu);
  }

  // Reset read registers to reflect freshly loaded memory.
  cpu->dmem_rdata_reg = 0;
  cpu->pmem_rdata_reg = 0;
  cpu->drive_inputs();
  cpu->eval_settle();
  return 0;
}

}  // extern "C"
