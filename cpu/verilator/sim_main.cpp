#include "Vsim_top.h"
#include "Vsim_top___024root.h"
#include "verilated.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sstream>
#include <utility>
#include <vector>

#ifndef SIM_CLOCK_HZ
#define SIM_CLOCK_HZ 27'000'000
#endif

constexpr uint32_t ADDR_WIDTH = 14;
constexpr uint32_t PMEM_WORDS = 1u << ADDR_WIDTH;
constexpr uint32_t DMEM_WORDS = 1u << (ADDR_WIDTH - 1);
constexpr uint32_t DMEM_GLOBAL_START = 0x100;
constexpr uint32_t IO_GLOBAL_START = 0x80;
constexpr uint32_t IO_GLOBAL_END = 0x100;
constexpr uint32_t IO_WORD_START = IO_GLOBAL_START >> 1;
constexpr uint32_t IO_WORD_END = IO_GLOBAL_END >> 1;
constexpr uint32_t IO_WORDS = IO_WORD_END - IO_WORD_START;
constexpr uint32_t HALT_ADDR = 0x0006;
constexpr uint32_t CLOCK_HZ = SIM_CLOCK_HZ;
constexpr uint32_t UART_BAUD = CLOCK_HZ/10; //115'200;
constexpr uint32_t UART_BIT_PERIOD = CLOCK_HZ / UART_BAUD;

struct Options {
  std::string pmem_file;
  std::string dmem_file;
  std::string uart_in_file;
  std::string uart_out_file;
  uint64_t max_cycles = 1 * CLOCK_HZ; // 1 秒間でタイムアウト
  bool verbose = false;
};

bool starts_with(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

Options parse_options(int argc, char** argv) {
  Options opt;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg(argv[i]);

    if (starts_with(arg, "+pmem=")) {
      opt.pmem_file = std::string(arg.substr(6));
    } else if (starts_with(arg, "+dmem=")) {
      opt.dmem_file = std::string(arg.substr(6));
    } else if (starts_with(arg, "+uart_in=")) {
      opt.uart_in_file = std::string(arg.substr(9));
    } else if (starts_with(arg, "+uart_out=")) {
      opt.uart_out_file = std::string(arg.substr(10));
    } else if (starts_with(arg, "+max-cycles=")) {
      opt.max_cycles = std::stoull(std::string(arg.substr(12)));
    } else if (arg == "+verbose") {
      opt.verbose = true;
    }
  }

  if (opt.pmem_file.empty()) {
    throw std::runtime_error("missing +pmem=<file>");
  }

  return opt;
}

uint32_t parse_hex_word(const std::string& s) {
  uint32_t value = std::stoul(s, nullptr, 16);
  return value;
}

void load_pmem(Vsim_top& top, const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("failed to open pmem file: " + path);
  }

  std::string tok;
  uint32_t addr = 0;

  while (in >> tok) {
    if (addr >= PMEM_WORDS) {
      throw std::runtime_error("pmem image too large");
    }

    uint32_t insn = parse_hex_word(tok) & 0x3ffffu;

    // pmem_sim.sv:
    //   logic [17:0] mem [(1<<`ADDR_WIDTH)-1:0];
    //
    // アクセスするには --public-flat-rw が必要。
    top.rootp->sim_top__DOT__mcu__DOT__pmem__DOT__mem[addr] = insn;
    ++addr;
  }
}

void load_dmem(Vsim_top& top, const std::string& path) {
  if (path.empty()) {
    return;
  }

  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("failed to open dmem file: " + path);
  }

  std::string tok;
  uint32_t word_addr = DMEM_GLOBAL_START >> 1;

  while (in >> tok) {
    if (word_addr >= DMEM_WORDS) {
      throw std::runtime_error("dmem image too large");
    }

    uint16_t value = parse_hex_word(tok);

    // mem.sv:
    //   logic [7:0] mem_lo[0:13'h1fff];
    //   logic [7:0] mem_hi[0:13'h1fff];
    //
    // 先頭 0x100 バイトは MMIO なのでその後ろから。
    top.rootp->sim_top__DOT__mcu__DOT__dmem__DOT__mem_lo[word_addr] = value & 0xffu;
    top.rootp->sim_top__DOT__mcu__DOT__dmem__DOT__mem_hi[word_addr] = value >> 8;
    ++word_addr;
  }
}

std::vector<uint8_t> load_uart_bytes(const std::string& path) {
  std::vector<uint8_t> bytes;
  if (path.empty()) {
    return bytes;
  }

  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("failed to open uart input file: " + path);
  }

  std::string tok;
  while (in >> tok) {
    bytes.push_back(static_cast<uint8_t>(parse_hex_word(tok)));
  }

  return bytes;
}

class UartInput {
public:
  explicit UartInput(std::vector<uint8_t> data)
    : bytes_(std::move(data)) {}

  uint8_t rx_sig() const {
    if (!started_ || byte_index_ >= bytes_.size()) {
      return 1; // 無信号時は 1
    }

    if (bit_phase_ == 0) {
      return 0; // スタートビット
    }
    if (bit_phase_ == 9) {
      return 1; // ストップビット
    }
    return (bytes_[byte_index_] >> (bit_phase_ - 1)) & 1u;
  }

  void advance() {
    if (byte_index_ >= bytes_.size()) {
      return;
    }

    ++bit_cycle_;
    if (bit_cycle_ < UART_BIT_PERIOD) {
      return;
    }

    bit_cycle_ = 0;
    if (!started_) {
      started_ = true;
    } else if (bit_phase_ < 9) {
      ++bit_phase_;
    } else {
      bit_phase_ = 0;
      ++byte_index_;
    }
  }

private:
  std::vector<uint8_t> bytes_;
  size_t byte_index_ = 0;
  uint8_t bit_phase_ = 0;
  uint32_t bit_cycle_ = 0;
  bool started_ = false;
};

class UartOutput {
public:
  explicit UartOutput(const std::string& path) {
    if (path.empty()) {
      return;
    }

    out_.open(path, std::ios::binary);
    if (!out_) {
      throw std::runtime_error("failed to open uart output file: " + path);
    }
  }

  void write(uint8_t value) {
    if (!out_.is_open()) {
      return;
    }

    out_.put(static_cast<char>(value));
    if (!out_) {
      throw std::runtime_error("failed to write uart output");
    }
  }

  bool is_open() const {
    return out_.is_open();
  }

private:
  std::ofstream out_;
};

void set_default_inputs(Vsim_top& top) {
  top.uart_rx = 1;
  top.uart2_rx = 1;
  top.uart3_rx = 1;
  top.dmem_rdata_io = 0;

  top.clk125 = 0;
  top.adc_cmp = 0;
  top.uf_dout = 0xdeadbeefu;
  top.spi_miso = 0;

  // sim.sv では tri1 [7:0] key_col_n; なので、未押下相当として 1 にしておく。
  top.key_col_n = 0xff;

  // I2C を使わないテストでは pull-up 相当。
  top.i2c_scl = 1;
  top.i2c_sda = 1;
}

void eval_half_cycle(VerilatedContext& ctx, Vsim_top& top, uint8_t clk) {
  top.clk = clk;
  top.eval();
  ctx.timeInc(1);
}

bool match_insn(uint32_t insn, uint32_t mask, uint32_t value) {
  return (insn & mask) == value;
}

const char* insn_name(uint32_t insn) {
#include "insn_names_cpp.inc"
  return "UNDEF";
}

uint8_t phase_num(const Vsim_top___024root& root) {
  return root.sim_top__DOT__mcu__DOT__cpu__DOT__signals__DOT__phase_decode ? 0
    : root.sim_top__DOT__mcu__DOT__cpu__DOT__signals__DOT__phase_exec ? 1
    : root.sim_top__DOT__mcu__DOT__cpu__DOT__signals__DOT__phase_rdmem ? 2
    : 3;
}

std::string format_hex_or_z(bool valid, uint32_t value, int width) {
  if (!valid) {
    return std::string(width, 'z');
  }

  std::ostringstream oss;
  oss << std::hex << std::nouppercase << std::setfill('0') << std::setw(width) << value;
  return oss.str();
}

std::string monitor_line(const VerilatedContext& ctx, const Vsim_top& top) {
  const auto& root = *top.rootp;
  std::ostringstream oss;
  oss << std::setw(7) << std::dec << ctx.time()
      << ": rst=" << static_cast<int>(top.rst)
      << " ip=" << std::hex << std::nouppercase << std::setfill('0') << std::setw(2)
      << root.sim_top__DOT__mcu__DOT__cpu__DOT__ip
      << "." << std::dec << static_cast<int>(phase_num(root))
      << " " << std::hex << std::setw(5) << (root.sim_top__DOT__mcu__DOT__cpu__DOT__insn & 0x3ffffu)
      << " " << std::left << std::setfill(' ') << std::setw(6)
      << insn_name(root.sim_top__DOT__mcu__DOT__cpu__DOT__insn) << std::right
      << " addr=" << std::hex << std::setfill('0') << std::setw(3) << top.dmem_addr
      << " r=" << std::setw(4) << root.sim_top__DOT__mcu__DOT__cpu__DOT__dmem_rdata
      << " w=" << format_hex_or_z(top.dmem_wen, top.dmem_wdata, 4)
      << " byt=" << std::dec << static_cast<int>(top.dmem_byt)
      << " alu_out=" << std::hex << std::setw(4) << root.sim_top__DOT__mcu__DOT__cpu__DOT__alu_out
      << " stack{" << std::setw(2) << root.sim_top__DOT__mcu__DOT__cpu__DOT__stack0
      << " " << std::setw(2) << root.sim_top__DOT__mcu__DOT__cpu__DOT__stack1
      << "} in=" << std::setw(4) << root.sim_top__DOT__mcu__DOT__cpu__DOT__stack_in
      << " fp=" << std::setw(4) << root.sim_top__DOT__mcu__DOT__cpu__DOT__fp
      << " load_sr=" << std::dec << static_cast<int>(root.sim_top__DOT__mcu__DOT__cpu__DOT__load_sr)
      << " rst_sr=" << static_cast<int>(root.sim_top__DOT__mcu__DOT__cpu__DOT__rst_sr)
      << " fpmin=" << std::hex << std::setw(4) << root.sim_top__DOT__mcu__DOT__cpu__DOT__fpmin;
  return oss.str();
}

void maybe_print_monitor(const VerilatedContext& ctx, const Vsim_top& top, bool verbose, std::string& prev_line) {
  if (!verbose) {
    return;
  }

  const std::string line = monitor_line(ctx, top);
  if (line != prev_line) {
    std::cout << line << "\n";
    prev_line = line;
  }
}

void tick(VerilatedContext& ctx, Vsim_top& top, std::array<uint16_t, IO_WORDS>& io_regs, uint16_t& dmem_addr_d) {
  // クロック変化前の各種の値を記録
  const uint16_t dmem_addr = top.dmem_addr;
  const uint16_t dmem_wdata = top.dmem_wdata;
  const uint8_t dmem_wen = top.dmem_wen;

  const uint16_t io_addr_d = (dmem_addr_d >> 1) - IO_WORD_START;
  if (io_addr_d < IO_WORDS) {
    top.dmem_rdata_io = io_regs[io_addr_d];
  } else {
    top.dmem_rdata_io = 0;
  }

  eval_half_cycle(ctx, top, 0);
  eval_half_cycle(ctx, top, 1);

  if (dmem_wen && dmem_addr >= IO_GLOBAL_START && dmem_addr < IO_GLOBAL_END) {
    io_regs[(dmem_addr >> 1) - IO_WORD_START] = dmem_wdata;
  }

  dmem_addr_d = dmem_addr;
}

void reset(VerilatedContext& ctx, Vsim_top& top, std::array<uint16_t, IO_WORDS>& io_regs, uint16_t& dmem_addr_d) {
  top.rst = 1;
  for (int i = 0; i < 4; ++i) {
    tick(ctx, top, io_regs, dmem_addr_d);
  }

  top.rst = 0;
  for (int i = 0; i < 2; ++i) {
    tick(ctx, top, io_regs, dmem_addr_d);
  }
}

int main(int argc, char** argv) {
  try {
    VerilatedContext ctx;
    ctx.commandArgs(argc, argv);

    auto opt = parse_options(argc, argv);

    Vsim_top top(&ctx);
    std::array<uint16_t, IO_WORDS> io_regs{};

    set_default_inputs(top);

    // Verilator の initial 処理を先に走らせる。
    // mem.sv 内の $readmemh("./ipl.dmem_*.hex", ...) がここで実行されうる。
    top.rst = 1;
    top.clk = 0;
    top.eval();

    // C++ 側でテスト対象のメモリ内容を上書きする。
    load_pmem(top, opt.pmem_file);
    load_dmem(top, opt.dmem_file);
    UartInput uart_in(load_uart_bytes(opt.uart_in_file));
    UartOutput uart_out(opt.uart_out_file);

    uint16_t dmem_addr_d = 0;
    std::string prev_monitor_line;
    reset(ctx, top, io_regs, dmem_addr_d);
    maybe_print_monitor(ctx, top, opt.verbose, prev_monitor_line);

    for (uint64_t cycle = 0; cycle < opt.max_cycles; ++cycle) {
      top.uart_rx = uart_in.rx_sig();
      tick(ctx, top, io_regs, dmem_addr_d);
      uart_in.advance();
      maybe_print_monitor(ctx, top, opt.verbose, prev_monitor_line);
      if (top.uart_out_full) {
        if (uart_out.is_open() && top.uart_out_data != 4) {
          uart_out.write(top.uart_out_data);
        } else {
          std::printf("%02x\n", top.uart_out_data);
          top.final();
          return 0;
        }
      }

      set_default_inputs(top);
      top.uart_rx = uart_in.rx_sig();
    }

    std::cerr << "timeout: exceeded " << opt.max_cycles << " cycles\n";
    std::cout << "timeout\n";
    top.final();
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
}
