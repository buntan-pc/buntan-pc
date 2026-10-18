// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2024 Kota UCHIDA
 */
`include "common.sv"

module signals(
  input rst,
  input clk,
  input irq,
  input [17:0] insn,
  output logic sign,
  output logic [15:0] imm_mask,
  output logic src_a_stk0,
  output logic src_a_fp,
  output logic src_a_gp,
  output logic src_a_ip,
  output logic src_a_cstk,
  output logic src_a_sr,
  output logic [1:0] src_b_sel,
  output logic [5:0] alu_sel,
  output logic wr_stk1,
  output logic pop,
  output logic push,
  output logic load_stk,
  output logic load_fp,
  output logic load_gp,
  output logic load_ip,
  output logic load_insn,
  output logic load_isr,
  output logic cpop,
  output logic cpush,
  output logic byt,
  output logic dmem_ren,
  output logic dmem_wen,
  output logic set_ien, clear_ien,
  output logic [7:0] phase,
  output logic pmem_wenh, pmem_wenl,
  output logic load_sr, rst_sr
);

logic phase_decode, phase_exec, phase_rdmem, phase_fetch;
signalizer signalizer(.*);

logic dec_sign, dec_wr_stk1;
logic [15:0] dec_imm_mask;
logic dec_src_a_stk0, dec_src_a_fp, dec_src_a_gp, dec_src_a_ip,
  dec_src_a_cstk, dec_src_a_sr;
logic [1:0] dec_src_b;
logic [5:0] dec_alu_sel;
logic dec_pop, dec_push, dec_stk, dec_fp, dec_gp, dec_ip, dec_isr,
  dec_cpop, dec_cpush, dec_byt, dec_dmem_ren, dec_dmem_wen,
  dec_set_ien, dec_clear_ien, dec_call, dec_pmem_wenh, dec_pmem_wenl,
  dec_load_sr, dec_rst_sr;
decoder decoder(
  .insn(insn),
  .sign(dec_sign),
  .imm_mask(dec_imm_mask),
  .src_a_stk0(dec_src_a_stk0),
  .src_a_fp(dec_src_a_fp),
  .src_a_gp(dec_src_a_gp),
  .src_a_ip(dec_src_a_ip),
  .src_a_cstk(dec_src_a_cstk),
  .src_a_sr(dec_src_a_sr),
  .src_b(dec_src_b),
  .alu_sel(dec_alu_sel),
  .wr_stk1(dec_wr_stk1),
  .pop(dec_pop),
  .push(dec_push),
  .load_stk(dec_stk),
  .load_fp(dec_fp),
  .load_gp(dec_gp),
  .load_ip(dec_ip),
  .load_isr(dec_isr),
  .cpop(dec_cpop),
  .cpush(dec_cpush),
  .byt(dec_byt),
  .dmem_ren(dec_dmem_ren),
  .dmem_wen(dec_dmem_wen),
  .set_ien(dec_set_ien),
  .clear_ien(dec_clear_ien),
  .call(dec_call),
  .pmem_wenh(dec_pmem_wenh),
  .pmem_wenl(dec_pmem_wenl),
  .load_sr(dec_load_sr),
  .rst_sr(dec_rst_sr)
);

logic irq_prev, irq_pend, irq_do, reload_ip;

assign reload_ip = (dec_ip | irq_do) & next_phase_exec;
assign phase = phase_decode ? "D"
             : phase_exec ? "E"
             : phase_rdmem ? "R" : "F";

// タイミング問題を無くすため、デコードと後続の信号の間にレジスタを挟み、つながりを切断する
always @(posedge clk) begin
  sign        <= dec_sign;
  imm_mask    <= dec_imm_mask;
  src_a_stk0  <= next_phase_half & dec_src_a_stk0 & ~irq_do;
  src_a_fp    <= next_phase_half & dec_src_a_fp & ~irq_do;
  src_a_gp    <= next_phase_half & dec_src_a_gp & ~irq_do;
  src_a_ip    <= ~next_phase_half | (next_phase_half & (dec_src_a_ip | irq_do)) | (dec_call & next_phase_decode);
  src_a_cstk  <= next_phase_half & dec_src_a_cstk & ~irq_do;
  src_a_sr    <= next_phase_half & dec_src_a_sr & ~irq_do;
  src_b_sel   <= irq_do ? `SRCB_ISR : dec_src_b;
  alu_sel     <= next_phase_exec ? (irq_do ? `ALU_B : dec_alu_sel) : next_phase_fetch ? `ALU_INC : `ALU_A;
  wr_stk1     <= dec_wr_stk1;
  pop         <= (dec_pop & ~irq_do) & next_phase_exec;
  push        <= (dec_push & ~irq_do) & (dec_dmem_ren ? next_phase_rdmem : next_phase_exec);
  load_stk    <= (dec_stk & ~irq_do) & (dec_dmem_ren ? next_phase_rdmem : next_phase_exec);
  load_fp     <= (dec_fp & ~irq_do) & next_phase_exec;
  load_gp     <= (dec_gp & ~irq_do) & next_phase_exec;

  // 割り込み要求がある場合（irq_pend=1）、次の命令は実行されない。
  // そのときは IP を更新しないままにする。
  load_ip     <= reload_ip | (next_phase_fetch & ~irq_pend /* not irq_do */);

  load_insn   <= next_phase_fetch;
  load_isr    <= (dec_isr & ~irq_do) & next_phase_exec;
  cpop        <= (dec_cpop & ~irq_do) & next_phase_exec;
  cpush       <= (dec_cpush | irq_do) & next_phase_exec;
  byt         <= (dec_byt & ~irq_do);
  dmem_ren    <= next_phase_rdmem & dec_dmem_ren;
  dmem_wen    <= (dec_dmem_wen & ~irq_do) & next_phase_exec;
  set_ien     <= (dec_set_ien & ~irq_do) & next_phase_exec;
  clear_ien   <= (dec_clear_ien | irq_do) & next_phase_exec;
  pmem_wenh   <= (dec_pmem_wenh & ~irq_do) & next_phase_exec;
  pmem_wenl   <= (dec_pmem_wenl & ~irq_do) & next_phase_exec;
  load_sr     <= (dec_load_sr & ~irq_do) & next_phase_exec;
  rst_sr      <= (dec_rst_sr & ~irq_do) & next_phase_exec;
end

logic next_phase_half, next_phase_fetch, next_phase_decode, next_phase_exec, next_phase_rdmem;
assign next_phase_half = next_phase_decode | next_phase_exec;
assign next_phase_fetch  = rst ? phase_fetch  : phase_rdmem;
assign next_phase_decode = rst ? phase_decode : phase_fetch;
assign next_phase_exec   = rst ? phase_exec   : phase_decode;
assign next_phase_rdmem  = rst ? phase_rdmem  : phase_exec;

always @(posedge rst, posedge clk) begin
  if (rst)
    irq_prev <= 1'b0;
  else
    irq_prev <= irq;
end

always @(posedge rst, posedge clk) begin
  if (rst)
    irq_pend <= 1'b0;
  else if (~irq_prev & irq) // irq のエッジ検出
    irq_pend <= 1'b1;
  else if (next_phase_fetch)
    irq_pend <= 1'b0;
end

always @(posedge rst, posedge clk) begin
  if (rst)
    irq_do <= 1'b0;
  else if (next_phase_fetch)
    irq_do <= irq_pend;
end

endmodule
