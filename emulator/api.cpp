// SPDX-License-Identifier: MIT
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
#include <cstring>
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

constexpr uint32_t kAddrWidth = 14;                     // アドレス幅 (ビット数)
constexpr uint32_t kAddrMask = (1u << kAddrWidth) - 1u; // アドレスマスク
constexpr uint32_t kDmemWords = 1u << (kAddrWidth - 1); // DMEM数
constexpr uint32_t kPmemWords = 1u << kAddrWidth;       // PMEM数

constexpr uint16_t kAddrSpiData = 0x0020;               // SPI データアドレス
constexpr uint16_t kAddrKbcQueue = 0x0024;              // KBCキューアドレス
constexpr uint16_t kAddrKbcStatus = 0x0026;             // KBCステータスアドレス
constexpr uint16_t kAddrUart3Data = 0x0030;             // UART3データアドレス

constexpr uint32_t kPmemMask18 = 0x3ffffu;              // PMEMアドレスマスク
constexpr uint32_t kDmemMask16 = 0xffffu;               // DMEMアドレスマスク
constexpr uint32_t kDmemHexStartWord = 0x100u >> 1;

constexpr int kSectorSize = 512;                        // セクタサイズ
constexpr int kCmdSize = 6;                             // コマンドサイズ
constexpr int kWriteCrcSize = 2;                        // CRCサイズ
constexpr int kUartDiv = 200;                           // UART割り込み周期
constexpr int kSpiDebugPrintLimit = 40;                 // SPIデバッグ出力制限
constexpr int kSpiTracePrintLimit = 100;                // SPIトレース出力制限

class TerminalRawMode {
 public:
  TerminalRawMode() = default;
  TerminalRawMode(const TerminalRawMode&) = delete;
  TerminalRawMode& operator=(const TerminalRawMode&) = delete;

  ~TerminalRawMode() { restore(); }

  /// ターミナルのrawモードを有効にする
  void enable() {
    if (enabled_) return;

    if (!isatty(STDIN_FILENO)) {
      std::printf("[tty] stdin is not tty\n");
      return;
    }

    if (tcgetattr(STDIN_FILENO, &original_) != 0) {
      std::perror("tcgetattr");
      return;
    }

    termios raw = original_;
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    raw.c_iflag &= static_cast<tcflag_t>(~(ICRNL | IXON));
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
      std::perror("tcsetattr");
      return;
    }

    enabled_ = true;
    std::printf("[tty] raw mode enabled\n");
  }

  /// ターミナルのrawモードを無効にする
  void restore() {
    if (!enabled_) return;
    tcsetattr(STDIN_FILENO, TCSANOW, &original_);
    enabled_ = false;
  }

 private:
  termios original_{};
  bool enabled_ = false;
};

TerminalRawMode g_terminal;

/// stdinを非ブロッキングモードに設定する
bool set_nonblocking_stdin() {
  const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (flags < 0) return false;
  return fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == 0;
}

/// ファイルから読み込んだデータをワードリストに変換する
bool load_readmemh_like(const std::string& path,
                        std::vector<uint32_t>* out_words, uint32_t start_index,
                        uint32_t bitmask) {
  if (!out_words) return false;

  FILE* file = std::fopen(path.c_str(), "r");
  if (!file) return false;

  uint32_t index = start_index;
  char line[256];

  while (std::fgets(line, sizeof(line), file)) {
    const char* p = line;
    while (*p == ' ' || *p == '\t') ++p;

    if (*p == '\0' || *p == '\n' || *p == '#') continue;
    if (*p == '/' && *(p + 1) == '/') continue;

    if (*p == '@') {
      char* end = nullptr;
      const unsigned long addr = std::strtoul(p + 1, &end, 16);
      if (end == p + 1) {
        std::fclose(file);
        return false;
      }
      index = static_cast<uint32_t>(addr);
      continue;
    }

    char* end = nullptr;
    const unsigned long value = std::strtoul(p, &end, 16);
    if (end == p) continue;

    if (index >= out_words->size()) break;
    (*out_words)[index++] = static_cast<uint32_t>(value) & bitmask;
  }

  std::fclose(file);
  return true;
}

/// データメモリのアドレスからワードインデックスを計算する
uint32_t dmem_word_index(uint16_t addr) {
  return ((addr & static_cast<uint16_t>(kAddrMask)) >> 1) & (kDmemWords - 1);
}

/// 16ビットの値を構成する
uint16_t make_u16(uint8_t lo, uint8_t hi) {
  return static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
}

/// stdinの文字を正規化する
uint8_t normalize_stdin_char(uint8_t ch) {
  if (ch == '\r') return '\n';
  if (ch == 0x7f) return 0x08;
  return ch;
}

/// SPI経由でSDカードを制御するクラス
class SdOverSpi {
 public:
  /// トランザクションをリセットする
  void reset_transaction() {
    cmd_len_ = 0;
    resp_fifo_.clear();
  }

  /// SPIバイトを転送する
  uint8_t transfer(uint8_t mosi_byte) {
    if (busy_count_ > 0) {
      --busy_count_;
      return 0x00;
    }

    if (!resp_fifo_.empty()) {
      const uint8_t byte = resp_fifo_.front();
      resp_fifo_.pop_front();
      return byte;
    }

    if (write_active_) {
      return transfer_write_data(mosi_byte);
    }

    return transfer_command(mosi_byte);
  }

 private:
  enum : uint8_t {
    kDataToken = 0xfe,
    kR1Idle = 0x01,
    kR1Ready = 0x00,
    kR1IllegalCommand = 0x04,
    kR1CommandCrcError = 0x08,
    kDataResponseAccepted = 0xe5,
  };

  void queue_bytes(std::initializer_list<uint8_t> bytes) {
    for (uint8_t byte : bytes) resp_fifo_.push_back(byte);
  }

  bool read_sector(uint32_t lba, uint8_t sector[kSectorSize]) const {
    FILE* file = std::fopen(IMAGE_FILE, "rb");
    if (!file) {
      std::memset(sector, 0, kSectorSize);
      return false;
    }

    if (std::fseek(file, static_cast<long>(lba) * kSectorSize, SEEK_SET) != 0) {
      std::fclose(file);
      std::memset(sector, 0, kSectorSize);
      return false;
    }

    const size_t read_bytes = std::fread(sector, 1, kSectorSize, file);
    std::fclose(file);

    if (read_bytes != kSectorSize) {
      std::memset(sector, 0, kSectorSize);
      return false;
    }

    return true;
  }

  bool write_sector(uint32_t lba, const uint8_t sector[kSectorSize]) const {
    FILE* file = std::fopen(IMAGE_FILE, "r+b");
    if (!file) return false;

    if (std::fseek(file, static_cast<long>(lba) * kSectorSize, SEEK_SET) != 0) {
      std::fclose(file);
      return false;
    }

    const size_t written = std::fwrite(sector, 1, kSectorSize, file);
    std::fflush(file);
    std::fclose(file);

    return written == kSectorSize;
  }

  uint8_t idle_status() const { return idle_ ? kR1Idle : kR1Ready; }

  void queue_r1(uint8_t r1) { resp_fifo_.push_back(r1); }

  void queue_data_block(const uint8_t* data, size_t size) {
    queue_bytes({kR1Ready, 0xff, kDataToken});
    for (size_t i = 0; i < size; ++i) {
      resp_fifo_.push_back(data[i]);
    }
    queue_bytes({0xff, 0xff});
  }

  void handle_command() {
    const uint8_t cmd = cmd_buf_[0] & 0x3f;
    const uint32_t arg = (uint32_t(cmd_buf_[1]) << 24) |
                         (uint32_t(cmd_buf_[2]) << 16) |
                         (uint32_t(cmd_buf_[3]) << 8) | uint32_t(cmd_buf_[4]);

    last_arg_ = arg;

    switch (cmd) {
      case 0:
        handle_cmd0();
        break;
      case 8:
        handle_cmd8(arg);
        break;
      case 9:
        handle_cmd9();
        break;
      case 17:
        handle_cmd17(arg);
        break;
      case 24:
        handle_cmd24(arg);
        break;
      case 41:
        handle_acmd41();
        break;
      case 55:
        handle_cmd55();
        break;
      case 58:
        handle_cmd58();
        break;
      default:
        queue_r1(static_cast<uint8_t>(kR1IllegalCommand | idle_status()));
        break;
      // TODO
    }
  }

  void handle_cmd0() {
    idle_ = true;
    seen_cmd55_ = false;
    queue_r1(kR1Idle);
  }

  void handle_cmd8(uint32_t arg) {
    queue_bytes({kR1Idle, 0x00, 0x00, 0x01, static_cast<uint8_t>(arg & 0xff)});
  }

  void handle_cmd9() {
    static constexpr uint8_t csd[] = {
        0x40, 0x0e, 0x00, 0x32, 0x5b, 0x59, 0x00, 0x00,
        0x1d, 0x69, 0x7f, 0x80, 0x0a, 0x40, 0x00, 0x00,
    };

    queue_data_block(csd, sizeof(csd));
  }

  void handle_cmd17(uint32_t lba) {
    uint8_t sector[kSectorSize]{};
    read_sector(lba, sector);
    queue_data_block(sector, sizeof(sector));
  }

  void handle_cmd24(uint32_t lba) {
    queue_r1(kR1Ready);
    write_active_ = true;
    write_lba_ = lba;
    write_pos_ = -1;
  }

  void handle_cmd55() {
    seen_cmd55_ = true;
    queue_r1(idle_status());
  }

  void handle_acmd41() {
    if (!seen_cmd55_) {
      queue_r1(static_cast<uint8_t>(kR1IllegalCommand | kR1Idle));
      return;
    }

    idle_ = false;
    seen_cmd55_ = false;
    queue_r1(kR1Ready);
  }

  void handle_cmd58() {
    const uint8_t ocr0 = static_cast<uint8_t>(0x80 | (ccs_ ? 0x40 : 0x00));
    queue_bytes({idle_status(), ocr0, 0x00, 0x00, 0x00});
  }

  uint8_t transfer_write_data(uint8_t mosi_byte) {
    if (write_pos_ < 0) {
      if (mosi_byte == kDataToken) write_pos_ = 0;
      return 0xff;
    }

    if (write_pos_ < kSectorSize) {
      write_buf_[write_pos_++] = mosi_byte;
      return 0xff;
    }

    if (write_pos_ < kSectorSize + kWriteCrcSize) {
      ++write_pos_;

      if (write_pos_ == kSectorSize + kWriteCrcSize) {
        finish_write();
      }

      return 0xff;
    }

    return 0xff;
  }

  void finish_write() {
    write_active_ = false;
    write_sector(write_lba_, write_buf_);

    resp_fifo_.push_back(kDataResponseAccepted);
    for (int i = 0; i < 8; ++i) resp_fifo_.push_back(0x00);
    resp_fifo_.push_back(0xff);
  }

  uint8_t transfer_command(uint8_t mosi_byte) {
    if (cmd_len_ == 0 && (mosi_byte & 0xc0) != 0x40) {
      return 0xff;
    }

    cmd_buf_[cmd_len_++] = mosi_byte;

    if (cmd_len_ == kCmdSize) {
      handle_command();
      cmd_len_ = 0;
    }

    return 0xff;
  }

  std::deque<uint8_t> resp_fifo_{};
  uint8_t cmd_buf_[kCmdSize]{};
  int cmd_len_ = 0;

  bool idle_ = true;
  bool seen_cmd55_ = false;
  bool ccs_ = true;

  uint32_t last_arg_ = 0;

  bool write_active_ = false;
  uint32_t write_lba_ = 0;
  int write_pos_ = 0;
  uint8_t write_buf_[kSectorSize]{};

  int busy_count_ = 0;
};

}  // namespace

struct bemu_cpu {
  VerilatedContext* context = nullptr;      // Verilatorのコンテキスト
  Vmcu* top = nullptr;                      // Verilatorのトップモジュール

  SdOverSpi sd{};                           // SDカードのSPIオーバーライド

  uint64_t uart3_tx_count = 0;              // UART3のTXバイト数
  uint64_t spi_shift_count = 0;             // SPIシフトカウント

  uint8_t spi_last_tx_byte = 0xff;          // SPIの最後のTXバイト
  uint8_t spi_current_resp_byte = 0xff;     // SPIの現在のレスポンスバイト
  uint8_t prev_spi_tx_busy = 0;             // 前回のSPI TXバイスの状態
  uint8_t prev_spi_cs = 1;                  // 前回のSPI CSの状態

  int spi_debug_transfer_done_printed = 0;  // SPI転送完了時のデバッグ出力が表示されたかどうか
  int spi_debug_read_printed = 0;           // SPI読み取り時のデバッグ出力が表示されたかどうか

  std::queue<uint8_t> uart3_rx_queue;       // UART3のRXキュー
  int uart3_rx_busy = 0;                    // UART3のRXバイト受信中かどうか
  int uart3_rx_bit = 0;                     // UART3のRXビットカウント
  int uart3_rx_div = 0;                     // UART3のRXビットカウントの割り込みカウント
  uint16_t uart3_rx_frame = 0;              // UART3のRXフレーム

  /// Verilatorのコンテキストを評価する
  void eval_settle() { top->eval(); }

  /// ピンを初期化する
  void initialize_pins() {
    top->rst = 1;
    top->clk = 0;
    top->clk125 = 0;
    top->uart_rx = 1;
    top->uart2_rx = 1;
    top->uart3_rx = 1;
    top->adc_cmp = 0;
    top->key_col_n = 0xff;
    top->dmem_rdata_io = 0;
    top->uf_dout = 0;
    top->spi_miso = 1;
    eval_settle();
  }

  /// マイクロコントローラをリセットする
  void reset() {
    top->rst = 1;
    eval_settle();
    tick_one_cycle();
    tick_one_cycle();
    top->rst = 0;
    eval_settle();
  }

  /// stdinをポーリングする
  void poll_stdin() {
    uint8_t ch = 0;
    const ssize_t n = read(STDIN_FILENO, &ch, 1);

    if (n == 1) {
      uart3_rx_queue.push(normalize_stdin_char(ch));
      return;
    }

    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      std::perror("read stdin");
    }
  }

  /// UART3のRXバイトをキューに注入する
  void inject_uart3_rx_register() {
    if (uart3_rx_queue.empty()) return;
    if (top->rootp->mcu__DOT__uart3_rx_full != 0) return;

    top->rootp->mcu__DOT__uart3_rx_byte = uart3_rx_queue.front();
    top->rootp->mcu__DOT__uart3_rx_full = 1;
    uart3_rx_queue.pop();
  }

  /// UART3のRXバイトを読み取った際にuart3_rx_fullをクリアする
  void clear_uart3_rx_full_on_read() {
    if (is_dmem_read(kAddrUart3Data)) {
      top->rootp->mcu__DOT__uart3_rx_full = 0;
    }
  }

  /// DMEM読み取り時にuart3_rx_fullをクリアする
  bool is_dmem_read(uint16_t addr) const {
    return top->rootp->mcu__DOT__cpu_dmem_ren &&
           top->rootp->mcu__DOT__dmem_addr_d == addr;
  }

  /// UART3のRXバイトを読み取った際にuart3_rx_fullをクリアする
  void set_uart_rx_line(uint8_t bit) {
    top->uart_rx = bit;
    top->uart2_rx = bit;
    top->uart3_rx = bit;
  }

  /// UART3のRXバイトを読み取った際にuart3_rx_fullをクリアする
  void tick_uart3_rx_line() {
    if (!uart3_rx_busy) {
      if (uart3_rx_queue.empty()) {
        set_uart_rx_line(1);
        return;
      }

      const uint8_t ch = uart3_rx_queue.front();
      uart3_rx_queue.pop();

      uart3_rx_frame =
          static_cast<uint16_t>((1u << 9) | (static_cast<uint16_t>(ch) << 1));
      uart3_rx_busy = 1;
      uart3_rx_bit = 0;
      uart3_rx_div = 0;
    }

    const uint8_t bit =
        static_cast<uint8_t>((uart3_rx_frame >> uart3_rx_bit) & 1u);
    set_uart_rx_line(bit);

    if (++uart3_rx_div < kUartDiv) return;

    uart3_rx_div = 0;
    ++uart3_rx_bit;

    if (uart3_rx_bit >= 10) {
      uart3_rx_busy = 0;
      set_uart_rx_line(1);
    }
  }

  /// SPIのCSラインを更新する
  void update_spi_chip_select() {
    const uint8_t cs = static_cast<uint8_t>(top->spi_cs);

    if (prev_spi_cs == 0 && cs != 0) {
      sd.reset_transaction();
    }

    prev_spi_cs = cs;
  }

  /// SPIのMISOラインをドライブする
  void drive_spi_miso() {
    if (top->spi_cs != 0) {
      top->spi_miso = 1;
      return;
    }

    if (!top->rootp->mcu__DOT__spi__DOT__tx_busy) {
      top->spi_miso = 1;
      return;
    }

    const uint8_t bit_idx =
        static_cast<uint8_t>(top->rootp->mcu__DOT__spi__DOT__bit_cnt & 7);
    top->spi_miso = (spi_current_resp_byte >> (7 - bit_idx)) & 1;
  }

  /// SPI読み取り時のデバッグ出力を観測する
  void observe_spi_read() {
    if (!is_dmem_read(kAddrSpiData)) return;
    if (!BEMU_DEBUG_SPI || spi_debug_read_printed >= kSpiDebugPrintLimit)
      return;

    debug("[spi.rd] ip=%04x sreg=%02x tx_ready=%d cs=%d\n",
          static_cast<unsigned>(top->rootp->mcu__DOT__cpu__DOT__ip),
          static_cast<unsigned>(top->rootp->mcu__DOT__spi__DOT__sreg),
          static_cast<int>(top->rootp->mcu__DOT__spi_tx_ready),
          static_cast<int>(top->spi_cs));
    ++spi_debug_read_printed;
  }

  /// SPI転送完了時のデバッグ出力を観測する
  void observe_posedge_writes() {
    if (top->dmem_wen && top->dmem_addr == kAddrUart3Data) {
      write_uart3_tx(static_cast<uint8_t>(top->dmem_wdata & 0xffu));
    }

    if (top->dmem_wen && top->dmem_addr == kAddrSpiData) {
      start_spi_transfer(static_cast<uint8_t>(top->dmem_wdata & 0xffu));
    }
  }

  /// UART3のTXバイトを書き込む
  void write_uart3_tx(uint8_t ch) {
    std::fwrite(&ch, 1, 1, stdout);
    std::fflush(stdout);
    ++uart3_tx_count;
  }

  /// SPI転送を開始する
  void start_spi_transfer(uint8_t tx_byte) {
    spi_last_tx_byte = tx_byte;
    ++spi_shift_count;

    uint8_t response = 0xff;
    if (top->spi_cs == 0) {
      response = sd.transfer(tx_byte);
    }

    spi_current_resp_byte = response;
    trace_spi_transfer(tx_byte, response);
  }

  /// SPI転送のトレースを出力する
  void trace_spi_transfer(uint8_t tx_byte, uint8_t response) const {
    static int printed = 0;
    if (printed >= kSpiTracePrintLimit) return;

    debug("[spi %02d] tx=%02x resp=%02x cs=%d\n", printed, tx_byte, response,
          static_cast<int>(top->spi_cs));
    ++printed;
  }

  /// SPI転送完了時のデバッグ出力を観測する
  void observe_spi_transfer_done() {
    const uint8_t tx_busy = top->rootp->mcu__DOT__spi__DOT__tx_busy;

    if (prev_spi_tx_busy && !tx_busy && BEMU_DEBUG_SPI &&
        spi_debug_transfer_done_printed < kSpiDebugPrintLimit) {
      debug("[spi.done] sreg=%02x (expect resp=%02x)\n",
            static_cast<unsigned>(top->rootp->mcu__DOT__spi__DOT__sreg),
            static_cast<unsigned>(spi_current_resp_byte));
      ++spi_debug_transfer_done_printed;
    }

    prev_spi_tx_busy = tx_busy;
  }

  void tick_one_cycle() {
    poll_stdin();
    clear_uart3_rx_full_on_read();

    top->clk = 0;
    eval_settle();

    inject_uart3_rx_register();
    observe_spi_read();
    update_spi_chip_select();
    drive_spi_miso();

    top->clk = 1;
    eval_settle();

    observe_posedge_writes();
    observe_spi_transfer_done();

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

  g_terminal.enable();
  if (!set_nonblocking_stdin()) {
    std::perror("fcntl stdin");
  }

  cpu->initialize_pins();
  cpu->reset();

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
  cpu->reset();
}

void bemu_cpu_set_irq(bemu_cpu_t* cpu, int level) {
  (void)cpu;
  (void)level;
  // TODO: 外部IRQ
}

void bemu_cpu_step(bemu_cpu_t* cpu, uint32_t cycles) {
  if (!cpu) return;

  for (uint32_t i = 0; i < cycles; ++i) {
    cpu->tick_one_cycle();
  }
}

uint16_t bemu_cpu_dmem_read16(bemu_cpu_t* cpu, uint16_t addr) {
  if (!cpu) return 0;

  const uint32_t index = dmem_word_index(addr);
  const uint8_t lo = cpu->top->rootp->mcu__DOT__dmem__DOT__mem_lo[index];
  const uint8_t hi = cpu->top->rootp->mcu__DOT__dmem__DOT__mem_hi[index];

  return make_u16(lo, hi);
}

void bemu_cpu_dmem_write16(bemu_cpu_t* cpu, uint16_t addr, uint16_t value) {
  if (!cpu) return;

  const uint32_t index = dmem_word_index(addr);
  cpu->top->rootp->mcu__DOT__dmem__DOT__mem_lo[index] =
      static_cast<uint8_t>(value & 0xffu);
  cpu->top->rootp->mcu__DOT__dmem__DOT__mem_hi[index] =
      static_cast<uint8_t>((value >> 8) & 0xffu);
}

uint32_t bemu_cpu_pmem_read18(bemu_cpu_t* cpu, uint16_t addr) {
  if (!cpu) return 0;

  addr &= static_cast<uint16_t>(kAddrMask);
  return cpu->top->rootp->mcu__DOT__pmem__DOT__mem[addr] & kPmemMask18;
}

void bemu_cpu_pmem_write18(bemu_cpu_t* cpu, uint16_t addr, uint32_t value18) {
  if (!cpu) return;

  addr &= static_cast<uint16_t>(kAddrMask);
  cpu->top->rootp->mcu__DOT__pmem__DOT__mem[addr] = value18 & kPmemMask18;
}

int bemu_cpu_load_ipl(bemu_cpu_t* cpu, const char* cpu_dir) {
  (void)cpu;
  (void)cpu_dir;
  return -99;
}

int bemu_cpu_load_pmem_hex(bemu_cpu_t* cpu, const char* pmem_hex_path) {
  if (!cpu || !pmem_hex_path) return -1;

  std::vector<uint32_t> words(kPmemWords, 0);
  if (!load_readmemh_like(pmem_hex_path, &words, 0, kPmemMask18)) {
    return -2;
  }

  for (uint32_t i = 0; i < kPmemWords; ++i) {
    cpu->top->rootp->mcu__DOT__pmem__DOT__mem[i] = words[i] & kPmemMask18;
  }

  cpu->eval_settle();
  return 0;
}

int bemu_cpu_load_dmem_hex16(bemu_cpu_t* cpu, const char* dmem_hex_path) {
  if (!cpu || !dmem_hex_path) return -1;

  std::vector<uint32_t> words(kDmemWords, 0);
  if (!load_readmemh_like(dmem_hex_path, &words, kDmemHexStartWord,
                          kDmemMask16)) {
    return -2;
  }

  for (uint32_t i = 0; i < kDmemWords; ++i) {
    const uint16_t value = static_cast<uint16_t>(words[i] & kDmemMask16);
    cpu->top->rootp->mcu__DOT__dmem__DOT__mem_lo[i] =
        static_cast<uint8_t>(value & 0xffu);
    cpu->top->rootp->mcu__DOT__dmem__DOT__mem_hi[i] =
        static_cast<uint8_t>((value >> 8) & 0xffu);
  }

  cpu->eval_settle();
  return 0;
}

uint16_t bemu_cpu_mmio_read16(bemu_cpu_t* cpu, uint16_t addr) {
  (void)cpu;
  (void)addr;
  return 0;
}

void bemu_cpu_mmio_write16(bemu_cpu_t* cpu, uint16_t addr, uint16_t value) {
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
         kPmemMask18;
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
