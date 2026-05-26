/*
 * Copyright (c) 2026 tas0dev
 */

#include "api.h"
#include <verilated.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <initializer_list>
#include <string>
#include <vector>
#include "Vmcu.h"
#include "Vmcu___024root.h"

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

struct SdOverSpi {
  // 受信/送信シフト
  uint8_t rx_byte = 0;
  int rx_bit = 0;
  uint8_t tx_byte = 0xff;
  int tx_bit = 0;
  bool tx_loaded = false;
  std::deque<uint8_t> tx_fifo;

  // コマンド解析（6バイト固定）
  uint8_t cmd_buf[6]{};
  int cmd_len = 0;

  // SD状態（最小）
  bool idle = true;
  bool seen_cmd55 = false;
  bool ccs = true;  // SDHC扱い

  void reset_transaction() {
    rx_byte = 0;
    rx_bit = 0;
    tx_byte = 0xff;
    tx_bit = 0;
    tx_loaded = false;
    tx_fifo.clear();
    cmd_len = 0;
  }

  void queue_bytes(std::initializer_list<uint8_t> bytes) {
    for (uint8_t b : bytes) tx_fifo.push_back(b);
  }

  void handle_command() {
    static bool printed = false;
    const uint8_t cmd = cmd_buf[0] & 0x3f;
    const uint32_t arg = (uint32_t(cmd_buf[1]) << 24) | (uint32_t(cmd_buf[2]) << 16) |
                         (uint32_t(cmd_buf[3]) << 8) | uint32_t(cmd_buf[4]);

    switch (cmd) {
      case 0:  // CMD0: GO_IDLE_STATE
        if (!printed) {
          std::fprintf(stderr, "[sd] CMD0 arg=%08x\n", arg);
          printed = true;
        }
        idle = true;
        seen_cmd55 = false;
        queue_bytes({0x01});  // R1=idle
        break;
      case 8:  // CMD8: SEND_IF_COND
        // R7: R1 + 32bit
        queue_bytes({0x01, 0x00, 0x00, 0x01, uint8_t(arg & 0xff)});
        break;
      case 55:  // CMD55: APP_CMD
        seen_cmd55 = true;
        queue_bytes({uint8_t(idle ? 0x01 : 0x00)});
        break;
      case 41:  // ACMD41: SD_SEND_OP_COND
        if (!seen_cmd55) {
          queue_bytes({0x05});
        } else {
          idle = false;
          seen_cmd55 = false;
          queue_bytes({0x00});
        }
        break;
      case 58: {  // CMD58: READ_OCR
        queue_bytes({uint8_t(idle ? 0x01 : 0x00)});
        // OCR: bit30(CCS)
        const uint8_t ocr0 = 0x00;
        const uint8_t ocr1 = 0xff;
        const uint8_t ocr2 = uint8_t(0x80 | (ccs ? 0x40 : 0x00));
        const uint8_t ocr3 = 0x00;
        queue_bytes({ocr0, ocr1, ocr2, ocr3});
        break;
      }
      default:
        queue_bytes({uint8_t(0x04 | (idle ? 0x01 : 0x00))});
        break;
    }
  }

  uint8_t shift_bit(uint8_t mosi_bit) {
    // 送信bit
    if (tx_bit == 0 && !tx_loaded) {
      if (!tx_fifo.empty()) {
        tx_byte = tx_fifo.front();
        tx_fifo.pop_front();
      } else {
        tx_byte = 0xff;
      }
      tx_loaded = true;
    }
    const uint8_t miso_bit = (tx_byte >> (7 - tx_bit)) & 1u;

    // 受信を進める
    rx_byte = uint8_t((rx_byte << 1) | (mosi_bit & 1u));
    rx_bit++;
    tx_bit = (tx_bit + 1) & 7;
    if (tx_bit == 0) tx_loaded = false;

    if (rx_bit == 8) {
      rx_bit = 0;
      if (cmd_len < 6) {
        cmd_buf[cmd_len++] = rx_byte;
        if (cmd_len == 6) {
          static bool printed_cmd = false;
          if (!printed_cmd) {
            std::fprintf(stderr, "[sd] cmd bytes: %02x %02x %02x %02x %02x %02x\n",
                         cmd_buf[0], cmd_buf[1], cmd_buf[2], cmd_buf[3], cmd_buf[4], cmd_buf[5]);
            printed_cmd = true;
          }
        }
        if (cmd_len == 6) {
          handle_command();
          cmd_len = 0;
          if (!tx_fifo.empty()) {
            tx_byte = tx_fifo.front();
            tx_fifo.pop_front();
            tx_bit = 0;
            tx_loaded = true;
          }
        }
      }
      rx_byte = 0;
    }

    return miso_bit;
  }
};

}  // namespace

struct bemu_cpu {
  VerilatedContext* context = nullptr;
  Vmcu* top = nullptr;
  SdOverSpi sd{};
  uint64_t uart3_tx_count = 0;
  uint64_t spi_shift_count = 0;

  void eval_settle() { top->eval(); }

  void posedge_observe() {
    // UART3データレジスタ(0x0030)への書き込みを監視
    if (top->dmem_wen && top->dmem_addr == 0x0030) {
      const uint8_t ch = static_cast<uint8_t>(top->dmem_wdata & 0xffu);
      std::fwrite(&ch, 1, 1, stdout);
      std::fflush(stdout);
      uart3_tx_count++;
    }
  }

  void spi_force_rx_byte_if_needed() {
    if (top->spi_cs != 0) return;
    if (top->rootp->mcu__DOT__spi__DOT__tx_busy) return;
    if (!sd.tx_fifo.empty()) {
      top->rootp->mcu__DOT__spi__DOT__sreg = sd.tx_fifo.front();
      sd.tx_fifo.pop_front();
    }
  }

  void tick_one_cycle() {
    // clk=0側で組み合わせを安定化してからposedgeを入れる
    top->clk = 0;
    eval_settle();

    if (top->spi_cs == 0) {
      constexpr uint32_t period = 1;  // CLOCK_HZ(100k)/BAUD(100k)=1
      const uint32_t cnt = top->rootp->mcu__DOT__spi__DOT__tim__DOT__cnt;
      const uint32_t inc = cnt + 1;
      const uint32_t next = (inc < period) ? inc : 0;
      const bool tim_full = (next == 0);
      if (top->rootp->mcu__DOT__spi__DOT__tx_busy && tim_full) {
        const uint8_t mosi_bit = top->spi_mosi & 1u;
        top->spi_miso = sd.shift_bit(mosi_bit);
        spi_shift_count++;
      } else {
        top->spi_miso = 1;
      }
    } else {
      top->spi_miso = 1;
      sd.reset_transaction();
    }

    // posedgeを評価
    top->clk = 1;
    eval_settle();

    spi_force_rx_byte_if_needed();
    posedge_observe();

    // negedgeを評価
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

  cpu->top = new Vmcu(cpu->context);
  cpu->top->rst = 1;
  cpu->top->clk = 0;
  cpu->top->clk125 = 0;
  cpu->top->uart_rx = 1;
  cpu->top->uart2_rx = 1;
  cpu->top->uart3_rx = 1;
  cpu->top->adc_cmp = 0;
  cpu->top->key_col_n = 0xff;
  cpu->top->dmem_rdata_io = 0;
  cpu->top->uf_dout = 0;
  cpu->top->spi_miso = 1;
  cpu->eval_settle();

  // リセット解除
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
  cpu->eval_settle();
  cpu->tick_one_cycle();
  cpu->tick_one_cycle();
  cpu->top->rst = 0;
  cpu->eval_settle();
}

void bemu_cpu_set_irq(bemu_cpu_t* cpu, int level) {
  (void)cpu;
  (void)level;
  // TODO: 外部IRQ
}

void bemu_cpu_step(bemu_cpu_t* cpu, uint32_t cycles) {
  if (!cpu) return;
  for (uint32_t i = 0; i < cycles; ++i) cpu->tick_one_cycle();
}

uint16_t bemu_cpu_dmem_read16(bemu_cpu_t* cpu, uint16_t addr) {
  if (!cpu) return 0;
  addr &= static_cast<uint16_t>(kAddrMask);
  const uint32_t widx = (addr >> 1) & (kDmemWords - 1);
  const uint8_t lo = cpu->top->rootp->mcu__DOT__dmem__DOT__mem_lo[widx];
  const uint8_t hi = cpu->top->rootp->mcu__DOT__dmem__DOT__mem_hi[widx];
  return static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
}

void bemu_cpu_dmem_write16(bemu_cpu_t* cpu, uint16_t addr, uint16_t value) {
  if (!cpu) return;
  addr &= static_cast<uint16_t>(kAddrMask);
  const uint32_t widx = (addr >> 1) & (kDmemWords - 1);
  cpu->top->rootp->mcu__DOT__dmem__DOT__mem_lo[widx] = static_cast<uint8_t>(value & 0xffu);
  cpu->top->rootp->mcu__DOT__dmem__DOT__mem_hi[widx] = static_cast<uint8_t>((value >> 8) & 0xffu);
}

uint32_t bemu_cpu_pmem_read18(bemu_cpu_t* cpu, uint16_t addr) {
  if (!cpu) return 0;
  addr &= static_cast<uint16_t>(kAddrMask);
  return cpu->top->rootp->mcu__DOT__pmem__DOT__mem[addr] & 0x3ffffu;
}

void bemu_cpu_pmem_write18(bemu_cpu_t* cpu, uint16_t addr, uint32_t value18) {
  if (!cpu) return;
  addr &= static_cast<uint16_t>(kAddrMask);
  cpu->top->rootp->mcu__DOT__pmem__DOT__mem[addr] = value18 & 0x3ffffu;
}

int bemu_cpu_load_ipl(bemu_cpu_t* cpu, const char* cpu_dir) {
  // TODO
  (void)cpu;
  (void)cpu_dir;
  return -99;
}

int bemu_cpu_load_pmem_hex(bemu_cpu_t* cpu, const char* pmem_hex_path) {
  if (!cpu || !pmem_hex_path) return -1;
  std::vector<uint32_t> tmp(kPmemWords, 0);
  if (!readmemh_like(std::string(pmem_hex_path), &tmp, 0, 0x3ffffu)) return -2;
  for (uint32_t i = 0; i < kPmemWords; ++i) {
    cpu->top->rootp->mcu__DOT__pmem__DOT__mem[i] = tmp[i] & 0x3ffffu;
  }
  cpu->eval_settle();
  return 0;
}

int bemu_cpu_load_dmem_hex16(bemu_cpu_t* cpu, const char* dmem_hex_path) {
  if (!cpu || !dmem_hex_path) return -1;
  std::vector<uint32_t> tmp(kDmemWords, 0);
  // uccのdmem.hexは0x0100からの連番
  const uint32_t start = (0x100u >> 1);
  if (!readmemh_like(std::string(dmem_hex_path), &tmp, start, 0xffffu)) return -2;
  for (uint32_t i = 0; i < kDmemWords; ++i) {
    const uint16_t v = static_cast<uint16_t>(tmp[i] & 0xffffu);
    cpu->top->rootp->mcu__DOT__dmem__DOT__mem_lo[i] = static_cast<uint8_t>(v & 0xffu);
    cpu->top->rootp->mcu__DOT__dmem__DOT__mem_hi[i] = static_cast<uint8_t>((v >> 8) & 0xffu);
  }
  cpu->eval_settle();
  return 0;
}

uint16_t bemu_cpu_mmio_read16(bemu_cpu_t* cpu, uint16_t addr) {
  // TODO
  (void)cpu;
  (void)addr;
  return 0;
}

void bemu_cpu_mmio_write16(bemu_cpu_t* cpu, uint16_t addr, uint16_t value) {
  // TODO
  (void)cpu;
  (void)addr;
  (void)value;
}

uint16_t bemu_cpu_debug_get_ip(bemu_cpu_t* cpu) {
  if (!cpu) return 0;
  return static_cast<uint16_t>(cpu->top->rootp->mcu__DOT__cpu__DOT__ip);
}

uint32_t bemu_cpu_debug_get_insn(bemu_cpu_t* cpu) {
  if (!cpu) return 0;
  return static_cast<uint32_t>(cpu->top->rootp->mcu__DOT__cpu__DOT__insn) & 0x3ffffu;
}

uint64_t bemu_cpu_debug_get_uart3_tx_count(bemu_cpu_t* cpu) {
  if (!cpu) return 0;
  return cpu->uart3_tx_count;
}

uint64_t bemu_cpu_debug_get_spi_shift_count(bemu_cpu_t* cpu) {
  if (!cpu) return 0;
  return cpu->spi_shift_count;
}

}  // extern "C"
