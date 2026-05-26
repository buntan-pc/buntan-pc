/*
 * Copyright (c) 2026 tas0dev
 */

#include "api.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <verilated.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <initializer_list>
#include <queue>
#include <string>
#include <vector>

#include "Vmcu.h"
#include "Vmcu___024root.h"
#include "debug.h"

#ifndef BEMU_DEBUG_SPI
#define BEMU_DEBUG_SPI 0
#endif

namespace {

constexpr uint32_t kAddrWidth = 14;
constexpr uint32_t kAddrMask = (1u << kAddrWidth) - 1u;  // 0x3fff
constexpr uint32_t kDmemWords = 1u
                                << (kAddrWidth - 1);  // 0x2000 words (16-bit)
constexpr uint32_t kPmemWords = 1u << kAddrWidth;     // 0x4000 words (18-bit)

static struct termios g_orig_termios;
static bool g_termios_saved = false;

static void restore_terminal(void) {
  if (g_termios_saved) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
  }
}

static void enable_raw_terminal(void) {
  if (!isatty(STDIN_FILENO)) {
    printf("[tty] stdin is not tty\n");
    return;
  }

  printf("[tty] raw mode enabled\n");

  if (tcgetattr(STDIN_FILENO, &g_orig_termios) == 0) {
    g_termios_saved = true;
    atexit(restore_terminal);

    struct termios raw = g_orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_iflag &= ~(ICRNL | IXON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  }
}

static bool readmemh_like(const std::string& path,
                          std::vector<uint32_t>* out_words,
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
  std::deque<uint8_t> resp_fifo;
  uint8_t cmd_buf[6]{};
  int cmd_len = 0;
  bool idle = true;
  bool seen_cmd55 = false;
  bool ccs = true;  // SDHC扱い
  uint32_t last_arg = 0;
  bool write_active = false;
  uint32_t write_lba = 0;
  int write_pos = 0;
  uint8_t write_buf[512]{};
  int busy_count = 0;

  void reset_transaction() {
    cmd_len = 0;
    resp_fifo.clear();
  }

  bool read_sector(uint32_t lba, uint8_t sector[512]) {
    FILE* f = std::fopen(IMAGE_FILE, "rb");
    if (!f) {
      std::memset(sector, 0, 512);
      return false;
    }

    if (std::fseek(f, static_cast<long>(lba) * 512L, SEEK_SET) != 0) {
      std::fclose(f);
      std::memset(sector, 0, 512);
      return false;
    }

    size_t n = std::fread(sector, 1, 512, f);
    std::fclose(f);

    if (n != 512) {
      std::memset(sector, 0, 512);
      return false;
    }

    return true;
  }

  void queue_bytes(std::initializer_list<uint8_t> bytes) {
    for (uint8_t b : bytes) resp_fifo.push_back(b);
  }

  bool write_sector(uint32_t lba, const uint8_t sector[512]) {
    FILE* f = std::fopen(IMAGE_FILE, "r+b");
    if (!f) return false;

    if (std::fseek(f, static_cast<long>(lba) * 512L, SEEK_SET) != 0) {
      std::fclose(f);
      return false;
    }

    size_t n = std::fwrite(sector, 1, 512, f);
    std::fflush(f);
    std::fclose(f);

    return n == 512;
  }

  void handle_command() {
    const uint8_t cmd = cmd_buf[0] & 0x3f;
    const uint32_t arg = (uint32_t(cmd_buf[1]) << 24) |
                         (uint32_t(cmd_buf[2]) << 16) |
                         (uint32_t(cmd_buf[3]) << 8) | uint32_t(cmd_buf[4]);
    last_arg = arg;

    switch (cmd) {
      case 0:  // CMD0
        idle = true;
        seen_cmd55 = false;
        queue_bytes({0x01});
        break;
      case 8:  // CMD8: SEND_OP_COND
        queue_bytes({0x01, 0x00, 0x00, 0x01, uint8_t(arg & 0xff)});
        break;
      case 9: {  // CMD9: SEND_CSD
        queue_bytes({
            0x00,  // R1
            0xff,  // wait
            0xfe,  // data token
            // CSD 16 bytes
            0x40,
            0x0e,
            0x00,
            0x32,
            0x5b,
            0x59,
            0x00,
            0x00,
            0x1d,
            0x69,
            0x7f,
            0x80,
            0x0a,
            0x40,
            0x00,
            0x00,
            // ダミーCRC
            0xff,
            0xff,
        });
        break;
      }
      case 17: {  // CMD17: READ_SINGLE_BLOCK
        queue_bytes({
            0x00,
            0xff,
            0xfe,
        });

        uint8_t sector[512];
        read_sector(arg, sector);

        for (int i = 0; i < 512; i++) {
          resp_fifo.push_back(sector[i]);
        }

        resp_fifo.push_back(0xff);
        resp_fifo.push_back(0xff);
        break;
      }
      case 24: {  // CMD24: WRITE_SINGLE_BLOCK
        queue_bytes({0x00});

        write_active = true;
        write_lba = arg;
        write_pos = -1;
        break;
      }
      case 55:  // CMD55
        seen_cmd55 = true;
        queue_bytes({uint8_t(idle ? 0x01 : 0x00)});
        break;
      case 41:  // ACMD41
        if (!seen_cmd55) {
          queue_bytes({0x05});
        } else {
          idle = false;
          seen_cmd55 = false;
          queue_bytes({0x00});
        }
        break;
      case 58: {  // CMD58
        queue_bytes({uint8_t(idle ? 0x01 : 0x00)});

        const uint8_t ocr0 = uint8_t(0x80 | (ccs ? 0x40 : 0x00));  // 0xC0
        const uint8_t ocr1 = 0x00;
        const uint8_t ocr2 = 0x00;
        const uint8_t ocr3 = 0x00;

        queue_bytes({ocr0, ocr1, ocr2, ocr3});
        break;
      }

      default:
        queue_bytes({uint8_t(0x04 | (idle ? 0x01 : 0x00))});
        break;
    }
  }

  // 1バイト送受信単位で呼ぶ
  uint8_t transfer(uint8_t mosi_byte) {
    if (busy_count > 0) {
      busy_count--;
      return 0x00;
    }

    // 応答FIFO最優先
    if (!resp_fifo.empty()) {
      uint8_t b = resp_fifo.front();
      resp_fifo.pop_front();
      return b;
    }

    // CMD24書き込み中
    if (write_active) {
      if (write_pos < 0) {
        if (mosi_byte == 0xfe) write_pos = 0;

        return 0xff;
      }

      if (write_pos < 512) {
        write_buf[write_pos++] = mosi_byte;
        return 0xff;
      }

      // CRC2バイト
      if (write_pos < 514) {
        write_pos++;

        if (write_pos == 514) {
          write_active = false;
          write_sector(write_lba, write_buf);

          resp_fifo.push_back(0xe5);
          for (int i = 0; i < 8; i++) {
            resp_fifo.push_back(0x00);
          }
          resp_fifo.push_back(0xff);
        }

        return 0xff;
      }
    }

    // 新規コマンド待ち
    if (cmd_len == 0) {
      if ((mosi_byte & 0xc0) != 0x40) return 0xff;
    }

    cmd_buf[cmd_len++] = mosi_byte;

    if (cmd_len == 6) {
      handle_command();
      cmd_len = 0;
    }

    return 0xff;
  }
};

}  // namespace

struct bemu_cpu {
  VerilatedContext* context = nullptr;
  Vmcu* top = nullptr;
  SdOverSpi sd{};
  uint64_t uart3_tx_count = 0;
  uint64_t spi_shift_count = 0;
  uint8_t spi_last_tx_byte = 0xff;
  uint8_t spi_current_resp_byte = 0xff;
  uint8_t prev_spi_tx_busy = 0;
  int spi_debug_transfer_done_printed = 0;
  int spi_debug_read_printed = 0;
  uint8_t prev_spi_cs = 1;
  std::queue<uint8_t> uart3_rx_queue;
  int uart3_rx_busy = 0;
  int uart3_rx_bit = 0;
  int uart3_rx_div = 0;
  uint16_t uart3_rx_frame = 0;

  void eval_settle() { top->eval(); }

  void update_io_read_data() {
    uint16_t value = 0;

    if (top->rootp->mcu__DOT__cpu_dmem_ren) {
      const uint16_t addr =
          static_cast<uint16_t>(top->rootp->mcu__DOT__dmem_addr_d);

      switch (addr) {
        case 0x0024:  // kbc_queue
          if (!uart3_rx_queue.empty()) {
            value = uart3_rx_queue.front();
            uart3_rx_queue.pop();
          } else {
            value = 0;
          }
          break;

        case 0x0026:  // kbc_status
          value = uart3_rx_queue.empty() ? 0 : 1;
          break;

        default:
          value = 0;
          break;
      }
    }

    top->dmem_rdata_io = value;
  }

  void poll_stdin() {
    uint8_t ch;

    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n == 1) {
      if (ch == '\r') ch = '\n';  // Enter
      if (ch == 0x7f) ch = 0x08;  // Backspace

      uart3_rx_queue.push(ch);
    } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      std::perror("read stdin");
    }
  }

  void inject_uart3_rx() {
    if (uart3_rx_queue.empty()) {
      return;
    }

    if (top->rootp->mcu__DOT__uart3_rx_full != 0) {
      return;
    }

    uint8_t ch = uart3_rx_queue.front();
    uart3_rx_queue.pop();

    top->rootp->mcu__DOT__uart3_rx_byte = ch;
    top->rootp->mcu__DOT__uart3_rx_full = 1;
  }

  void set_uart_rx_line(uint8_t bit) {
    top->uart_rx = bit;
    top->uart2_rx = bit;
    top->uart3_rx = bit;
  }

  void tick_uart3_rx() {
    constexpr int UART_DIV = 200;

    if (!uart3_rx_busy) {
      if (uart3_rx_queue.empty()) {
        set_uart_rx_line(1);
        return;
      }

      uint8_t ch = uart3_rx_queue.front();
      uart3_rx_queue.pop();

      uart3_rx_frame =
          static_cast<uint16_t>((1u << 9) | (static_cast<uint16_t>(ch) << 1));
      uart3_rx_busy = 1;
      uart3_rx_bit = 0;
      uart3_rx_div = 0;
    }

    uint8_t bit = static_cast<uint8_t>((uart3_rx_frame >> uart3_rx_bit) & 1u);

    set_uart_rx_line(bit);

    uart3_rx_div++;
    if (uart3_rx_div >= UART_DIV) {
      uart3_rx_div = 0;
      uart3_rx_bit++;

      if (uart3_rx_bit >= 10) {
        uart3_rx_busy = 0;
        set_uart_rx_line(1);
      }
    }
  }

  void posedge_observe() {
    if (top->dmem_wen && top->dmem_addr == 0x0030) {
      const uint8_t ch = static_cast<uint8_t>(top->dmem_wdata & 0xffu);
      std::fwrite(&ch, 1, 1, stdout);
      std::fflush(stdout);
      uart3_tx_count++;
    }

    if (top->dmem_wen && top->dmem_addr == 0x0020) {
      spi_last_tx_byte = static_cast<uint8_t>(top->dmem_wdata & 0xffu);
      spi_shift_count++;

      uint8_t resp = 0xff;
      if (top->spi_cs == 0) {
        resp = sd.transfer(spi_last_tx_byte);
      }

      spi_current_resp_byte = resp;

      // DBG
      static int printed = 0;
      if (printed < 100) {
        debug("[spi %02d] tx=%02x resp=%02x cs=%d\n", printed, spi_last_tx_byte,
              resp, (int)top->spi_cs);
        printed++;
      }
    }
  }

  void tick_one_cycle() {
    poll_stdin();

    if (top->rootp->mcu__DOT__cpu_dmem_ren &&
        top->rootp->mcu__DOT__dmem_addr_d == 0x0030) {
      top->rootp->mcu__DOT__uart3_rx_full = 0;
    }

    top->clk = 0;
    eval_settle();

    inject_uart3_rx();

    // CPUがSPIデータレジスタ(0x0020)を読むタイミング（phase_rdmem）を観測する。
    // mcu側は dmem_addr_d を使って dmem_rdata
    // を生成するため、ここを見れば「読み出し値」が分かる。
    if (top->rootp->mcu__DOT__cpu_dmem_ren &&
        top->rootp->mcu__DOT__dmem_addr_d == 0x0020) {
      if (BEMU_DEBUG_SPI && spi_debug_read_printed < 40) {
        debug("[spi.rd] ip=%04x sreg=%02x tx_ready=%d cs=%d\n",
              (unsigned)top->rootp->mcu__DOT__cpu__DOT__ip,
              (unsigned)top->rootp->mcu__DOT__spi__DOT__sreg,
              (int)top->rootp->mcu__DOT__spi_tx_ready, (int)top->spi_cs);
        spi_debug_read_printed++;
      }
    }

    const uint8_t cs = static_cast<uint8_t>(top->spi_cs);

    if (prev_spi_cs == 0 && cs != 0) {
      sd.reset_transaction();
    }

    prev_spi_cs = cs;

    if (cs != 0) {
      top->spi_miso = 1;
    } else if (top->rootp->mcu__DOT__spi__DOT__tx_busy) {
      const uint8_t bit_idx =
          static_cast<uint8_t>(top->rootp->mcu__DOT__spi__DOT__bit_cnt & 7);
      top->spi_miso = (spi_current_resp_byte >> (7 - bit_idx)) & 1;
    } else {
      top->spi_miso = 1;
    }

    // posedgeを評価
    top->clk = 1;
    eval_settle();

    // 書き込みを拾う（SPI転送開始やUART3出力など）
    posedge_observe();

    // 1バイト転送完了時の sreg を確認する（MISOエミュレーションの検証用）
    {
      const uint8_t tx_busy = top->rootp->mcu__DOT__spi__DOT__tx_busy;
      if (prev_spi_tx_busy && !tx_busy) {
        if (BEMU_DEBUG_SPI && spi_debug_transfer_done_printed < 40) {
          debug("[spi.done] sreg=%02x (expect resp=%02x)\n",
                (unsigned)top->rootp->mcu__DOT__spi__DOT__sreg,
                (unsigned)spi_current_resp_byte);
          spi_debug_transfer_done_printed++;
        }
      }
      prev_spi_tx_busy = tx_busy;
    }

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

  enable_raw_terminal();

  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

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
  cpu->top->rootp->mcu__DOT__dmem__DOT__mem_lo[widx] =
      static_cast<uint8_t>(value & 0xffu);
  cpu->top->rootp->mcu__DOT__dmem__DOT__mem_hi[widx] =
      static_cast<uint8_t>((value >> 8) & 0xffu);
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
  if (!readmemh_like(std::string(dmem_hex_path), &tmp, start, 0xffffu))
    return -2;
  for (uint32_t i = 0; i < kDmemWords; ++i) {
    const uint16_t v = static_cast<uint16_t>(tmp[i] & 0xffffu);
    cpu->top->rootp->mcu__DOT__dmem__DOT__mem_lo[i] =
        static_cast<uint8_t>(v & 0xffu);
    cpu->top->rootp->mcu__DOT__dmem__DOT__mem_hi[i] =
        static_cast<uint8_t>((v >> 8) & 0xffu);
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
  return static_cast<uint32_t>(cpu->top->rootp->mcu__DOT__cpu__DOT__insn) &
         0x3ffffu;
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
