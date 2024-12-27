`include "common.sv"

module cpu_tb;

logic rst, clk, irq, dmem_ren, dmem_wen, dmem_byt, pmem_wenh, pmem_wenl;
logic [`ADDR_WIDTH-1:0] dmem_addr, last_wr_addr, pmem_addr;
logic [15:0] dmem_rdata, dmem_wdata, last_wr_data;
logic [17:0] pmem_rdata;
logic [5:0] alu_sel;

cpu#(.CLOCK_HZ(1000)) cpu(.*);

initial begin
  $monitor("%d: insn=%04x alu[%02x]=%04x pmem_rdata=%04x pop/sh=%d%d",
           $time, cpu.insn, alu_sel, cpu.alu_out, pmem_rdata, cpu.pop, cpu.push,
           " stk=[%04x %04x] fp=%04x ip=%04x last_wr[%02x]=%04x",
           cpu.stack0, cpu.stack1, cpu.fp, cpu.ip, last_wr_addr, last_wr_data,
           " phase=%d",
           cpu.signals.phase,
           " irq=%d pmem_wenh/wenl=%d/%d",
           cpu.irq, pmem_wenh, pmem_wenl
           );

  rst <= 1;
  clk <= 1;
  pmem_rdata <= 18'h300A5; // 000: PUSH 0xA5
  irq <= 0;

  #1 rst <= 0;

  if (~cpu.signals.phase_rdmem) $error("CPU must start from rdmem phase");
  if (cpu.ip !== 16'h0000) $error("ip must be 0x0000");

  @(posedge clk)
  @(negedge clk)
    if (~cpu.load_insn) $error("load_insn must be 1");

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h1C004; // 001: NOT
    if (cpu.stack0 !== 16'h00A5) $error("stack0 must be 0x00A5");

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h30200; // 002: PUSH 0x200
    if (cpu.stack0 !== 16'hFF5A) $error("stack0 must be 0xFF5A");

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h1C821; // 003: POP gp
    if (cpu.stack0 !== 16'h0200) $error("stack0 must be 0x0200");
    if (cpu.gp !== 16'h0100) $error("gp must be 0x0100 (initial value)");

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h30001; // 004: PUSH 0x01
    if (cpu.stack0 !== 16'hFF5A) $error("stack0 must be 0xFF5A");
    if (cpu.gp !== 16'h0200) $error("gp must be 0x0200");

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h04FFF; // 005: JMP IP-1

  #11 // decode
  #10 // exec
    if (cpu.alu_out !== 16'h005) $error("alu_out must be 0x005");
  #10 // rdmem
    pmem_rdata <= 18'h06020; // 005: JZ 0x20
  @(posedge cpu.load_insn)
    if (cpu.stack0 !== 16'h0001) $error("stack0 must be 0x0001");
    if (cpu.stack1 !== 16'hFF5A) $error("stack1 must be 0xFF5A");

  #11 // decode
  #10 // exec
    if (cpu.stack0 !== 16'h0001) $error("stack0 must be 0x0001");
  #10 // rdmem
    if (~cpu.signals.phase_rdmem) $error("phase_rdmem must be 1");
    if (pmem_addr !== `ADDR_WIDTH'h006) $error("pmem_addr must be 0x006");
    pmem_rdata <= 18'h0602E; // 006: JZ 0x2E
  @(posedge cpu.load_insn)

  #11 // decode
  #10 // exec
    if (cpu.stack0 !== 16'hFF5A) $error("stack0 must be 0xFF5A");
  #10 // rdmem
    if (~cpu.signals.phase_rdmem) $error("phase_rdmem must be 1");
    if (pmem_addr !== `ADDR_WIDTH'h035) $error("pmem_addr must be 0x035");
    pmem_rdata <= 18'h0A103; // 035: LD1 gp+0x103
  @(posedge cpu.load_insn)

  #11 // decode
  #10 // exec
    if (dmem_addr !== 16'h0303) $error("dmem_addr must be 0x0303");
    if (dmem_ren !== 1'b0) $error("dmem_ren must be 0 (at exec stage)");
  #10 // rdmem
    if (dmem_ren !== 1'b1) $error("dmem_ren must be 1 (at rdmem stage)");
    dmem_rdata <= 16'hDEAD;

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h1C001; // 036: INC
    if (cpu.stack0 !== 16'h00DE) $error("stack0 must be 0x00DE");

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h10005; // 037: ST zero+4
    if (cpu.stack0 !== 16'h00DF) $error("stack0 must be 0x00DF");

  #11 // decode
  #10 // exec
    if (dmem_ren !== 1'b0) $error("dmem_ren must be 0");
  #10 // rdmem
    if (dmem_ren !== 1'b0) $error("dmem_ren must be 0");

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h30012; // 038: PUSH 0x12
    if (last_wr_addr !== 16'h0004) $error("last_wr_addr must be 0x0004");
    if (last_wr_data !== 16'h00DF) $error("last_wr_data must be 0x00DF");

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h30081; // 039: PUSH 0x81

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h1C80D; // 03A: STA1

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h1C809; // 03B: LDD1
    if (cpu.stack0 !== 16'h0081) $error("stack0 must be 0x0081");
    if (last_wr_addr !== 16'h0081) $error("last_wr_addr must be 0x0081");
    if (last_wr_data !== 16'h1200) $error("last_wr_data must be 0x1200");

  #11 // decode
  #10 // exec
    if (dmem_addr !== 16'h0081) $error("dmem_addr must be 0x0081");
  #10 // rdmem
    dmem_rdata <= 16'h1200;
  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h300FF; // 03C: PUSH 0xFF
    if (cpu.stack0 !== 16'h0012) $error("stack0 must be 0x0012");

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h30900; // 03D: PUSH 0x900

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h1C822; // 03E: POP ISR

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h300FE; // 03F: PUSH 0xFE
    irq <= 1;

  #11 // decode
  #10 // exec
  #10 // rdmem
    pmem_rdata <= 18'h1C812; // 900: IRET
    if (~cpu.signals.phase_rdmem) $error("phase_rdmem must be 1");
    if (cpu.ien !== 1'b0) $error("IEN must be 0");
    if (pmem_addr !== `ADDR_WIDTH'h900) $error("pmem_addr must be 0x900");
    if (cpu.stack0 !== 16'h00FF) $error("stack0 must be 0x00FF");
    irq <= 0;

  @(posedge cpu.load_insn)
  #11 // decode
  #10 // exec
  #10 // rdmem
    pmem_rdata <= 18'h30123; // 03F: PUSH 0x0123
    if (cpu.ien !== 1'b1) $error("IEN must be 1");
    if (pmem_addr !== `ADDR_WIDTH'h03F) $error("pmem_addr must be 0x03F");

  @(posedge cpu.load_insn)
  #11 // decode
  #10 // exec
  #10 // rdmem
    if (cpu.stack0 !== 16'h0123) $error("stack0 must be 0x0123");
    if (cpu.stack1 !== 16'h00FF) $error("stack1 must be 0x00FF");

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h30044; // 040: PUSH 0x044

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h1C825; // 041: SPLA (3xxxx to pmem[0x044])
    if (cpu.stack0 !== 16'h0044) $error("stack0 must be 0x0044");
    if (cpu.stack1 !== 16'h0123) $error("stack1 must be 0x0123");

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h1C824; // 042: SPHA (x0123 to pmem[0x044])
    if (cpu.stack0 !== 16'h0044) $error("stack0 must be 0x0044");
    if (cpu.stack1 !== 16'h00FF) $error("stack1 must be 0x00FF");

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h3CAFE; // 043: PUSH 0xCAFE

  @(posedge cpu.load_insn)
    pmem_rdata <= 18'h30123; // 044: PUSH 0x0123

  $finish;
end

// 10 単位時間周期のクロックを生成
always #5 begin
  clk <= ~clk;
end

always @(posedge clk, posedge rst) begin
  if (rst) begin
    last_wr_addr <= `ADDR_WIDTH'd0;
    last_wr_data <= 16'hFFFF;
  end
  else if (dmem_wen) begin
    last_wr_addr <= dmem_addr;
    last_wr_data <= dmem_wdata;
  end
end

endmodule
