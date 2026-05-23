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
#include "Vcpu___024root.h"
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
      // アドレス指定ディレクティブ
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

  // low/highバイトに分割して保持する
  std::vector<uint8_t> dmem_lo;
  std::vector<uint8_t> dmem_hi;

  // 1エントリ18bitを保持する
  std::vector<uint32_t> pmem;

  // 同期メモリ読み出しのレジスタ値
  uint16_t dmem_rdata_reg = 0;
  uint32_t pmem_rdata_reg = 0;

  // 同期読み出し用に直近のアドレスを保持（デバッグ用）
  uint16_t dmem_addr_q = 0;
  uint16_t pmem_addr_q = 0;

  bemu_spi_t spi{};
  uint8_t uart3_tx_ready = 1;
  uint8_t uart3_rx_ready = 0;
  uint8_t uart3_rx_data = 0;
  uint64_t uart3_tx_count = 0;

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

    // クロック立ち上がりでアドレスをサンプル
    const uint16_t daddr = static_cast<uint16_t>(top->dmem_addr) & kAddrMask;
    const uint16_t paddr = static_cast<uint16_t>(top->pmem_addr) & kAddrMask;

    {
      // MMIO領域
      if ((daddr & 0xff00u) == 0x0000u) {
        switch (daddr & 0x00ffu) {
          case 0x0030:  // uart3_data
            dmem_rdata_reg = uart3_rx_data;
            break;
          case 0x0032: {  // uart3_flag
            // bit0: RX ready, bit2: TX ready（dos/dos.c が参照）
            uint16_t f = 0;
            if (uart3_rx_ready) f |= 0x0001u;
            if (uart3_tx_ready) f |= 0x0004u;
            dmem_rdata_reg = f;
            break;
          }
          default:
            dmem_rdata_reg = bemu_spi_read16(&spi, daddr);
            break;
        }
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

    // 書き込み
    if (top->dmem_wen) {
      if ((daddr & 0xff00u) == 0x0000u) {
        switch (daddr & 0x00ffu) {
          case 0x0030: {  // uart3_data
            const uint8_t ch = static_cast<uint8_t>(top->dmem_wdata & 0xffu);
            std::fwrite(&ch, 1, 1, stdout);
            std::fflush(stdout);
            uart3_tx_ready = 1;
            uart3_tx_count++;
            break;
          }
          default:
            // 現状は16bit MMIO書き込みとして転送
            bemu_spi_write16(&spi, daddr, static_cast<uint16_t>(top->dmem_wdata));
            break;
        }
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
    // 立ち上がりエッジの前に入力が見えるようにしておく
    top->clk = 0;
    drive_inputs();
    eval_settle();

    // 立ち上がり
    top->clk = 1;
    eval_settle();
    posedge_mem();

    // 立ち下がり
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
  cpu->uart3_tx_ready = 1;
  cpu->uart3_rx_ready = 0;
  cpu->uart3_rx_data = 0;
  cpu->drive_inputs();
  cpu->eval_settle();

  // 典型的な使い方に合わせて、数サイクル後にリセット解除する
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
  cpu->uart3_tx_ready = 1;
  cpu->uart3_rx_ready = 0;
  cpu->uart3_rx_data = 0;
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

  // dmem lo/hiは8bit幅のため、一旦32bit配列へ読み込んでから8bitへ詰める
  std::vector<uint32_t> tmp_lo(kDmemWords, 0);
  std::vector<uint32_t> tmp_hi(kDmemWords, 0);

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

  // ロード直後の状態に合わせて、読み出しレジスタを初期化する
  cpu->dmem_rdata_reg = 0;
  cpu->pmem_rdata_reg = 0;
  cpu->drive_inputs();
  cpu->eval_settle();
  return 0;
}

int bemu_cpu_load_pmem_hex(bemu_cpu_t* cpu, const char* pmem_hex_path) {
  if (!cpu || !pmem_hex_path) return -1;
  if (!readmemh_like(std::string(pmem_hex_path), &cpu->pmem, 0, 0x3ffffu)) return -2;
  cpu->pmem_rdata_reg = 0;
  cpu->drive_inputs();
  cpu->eval_settle();
  return 0;
}

int bemu_cpu_load_dmem_hex16(bemu_cpu_t* cpu, const char* dmem_hex_path) {
  if (!cpu || !dmem_hex_path) return -1;
  std::vector<uint32_t> tmp(kDmemWords, 0);
  // ucc は通常のグローバル変数領域を 0x0100 から配置する（cpu/sim.sv と同じロード方式）
  const uint32_t start = (0x100u >> 1);
  if (!readmemh_like(std::string(dmem_hex_path), &tmp, start, 0xffffu)) return -2;
  for (uint32_t i = 0; i < kDmemWords; ++i) {
    const uint16_t v = static_cast<uint16_t>(tmp[i] & 0xffffu);
    cpu->dmem_lo[i] = static_cast<uint8_t>(v & 0xffu);
    cpu->dmem_hi[i] = static_cast<uint8_t>((v >> 8) & 0xffu);
  }
  cpu->dmem_rdata_reg = 0;
  cpu->drive_inputs();
  cpu->eval_settle();
  return 0;
}

uint16_t bemu_cpu_mmio_read16(bemu_cpu_t* cpu, uint16_t addr) {
  if (!cpu) return 0;
  addr &= 0x00ffu;
  // 16bitアクセスとして扱う（偶数アドレス前提）
  switch (addr & 0xfffeu) {
    case 0x0030:
      return cpu->uart3_rx_data;
    case 0x0032: {
      uint16_t f = 0;
      if (cpu->uart3_rx_ready) f |= 0x0001u;
      if (cpu->uart3_tx_ready) f |= 0x0004u;
      return f;
    }
    default:
      return bemu_spi_read16(&cpu->spi, addr);
  }
}

void bemu_cpu_mmio_write16(bemu_cpu_t* cpu, uint16_t addr, uint16_t value) {
  if (!cpu) return;
  addr &= 0x00ffu;
  switch (addr & 0xfffeu) {
    case 0x0030: {
      const uint8_t ch = static_cast<uint8_t>(value & 0xffu);
      std::fwrite(&ch, 1, 1, stdout);
      std::fflush(stdout);
      cpu->uart3_tx_ready = 1;
      break;
    }
    default:
      bemu_spi_write16(&cpu->spi, addr, value);
      break;
  }
}

uint16_t bemu_cpu_debug_get_ip(bemu_cpu_t* cpu) {
  if (!cpu) return 0;
  // Verilator が生成する内部表現へ直接アクセス（デバッグ用）
  return static_cast<uint16_t>(cpu->top->rootp->cpu__DOT__ip);
}

uint32_t bemu_cpu_debug_get_insn(bemu_cpu_t* cpu) {
  if (!cpu) return 0;
  return static_cast<uint32_t>(cpu->top->rootp->cpu__DOT__insn) & 0x3ffffu;
}

uint64_t bemu_cpu_debug_get_uart3_tx_count(bemu_cpu_t* cpu) {
  if (!cpu) return 0;
  return cpu->uart3_tx_count;
}

}  // extern "C"
