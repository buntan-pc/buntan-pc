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
  output sign,
  output [15:0] imm_mask,
  //output [2:0] src_a_sel,
  output src_a_stk0,
  output src_a_fp,
  output src_a_gp,
  output src_a_ip,
  output src_a_cstk,
  output src_a_sr,
  output [1:0] src_b_sel,
  output [5:0] alu_sel,
  output wr_stk1,
  output pop,
  output push,
  output load_stk,
  output load_fp,
  output load_gp,
  output load_ip,
  output load_insn,
  output load_isr,
  output cpop,
  output cpush,
  output byt,
  output dmem_ren,
  output dmem_wen,
  output set_ien, clear_ien,
  output [1:0] phase,
  output pmem_wenh, pmem_wenl,
  output load_sr, rst_sr
);

logic src_a_fp, src_a_gp, src_a_ip, src_a_cstk, src_a_sr;

//assign src_a_sel = src_a_fp ? `SRCA_FP
//                   : src_a_gp ? `SRCA_GP
//                   : src_a_ip ? `SRCA_IP
//                   : src_a_cstk ? `SRCA_CSTK
//                   : src_a_sr ? `SRCA_SR
//                   : `SRCA_STK0;
assign src_a_stk0 = phase_half & (insn_src_a === `SRCA_FP) & ~irq_pend;
assign src_a_fp = phase_half & (insn_src_a === `SRCA_FP) & ~irq_pend;
assign src_a_gp = phase_half & (insn_src_a === `SRCA_GP) & ~irq_pend;
assign src_a_ip = ~phase_half | (insn_src_a === `SRCA_IP) | irq_pend | (insn_call & phase_decode);
assign src_a_cstk = phase_half & (insn_src_a === `SRCA_CSTK) & ~irq_pend;
assign src_a_sr = phase_half & (insn_src_a === `SRCA_SR) & ~irq_pend;
assign src_b_sel = irq_pend ? `SRCB_ISR : insn_src_b;
assign alu_sel = phase_exec ? (irq_pend ? `ALU_B : insn_alu_sel) : phase_fetch ? `ALU_INC : `ALU_A;
assign pop = (insn_pop & ~irq_pend) & phase_exec;
assign push = (insn_push & ~irq_pend) & (insn_dmem_ren ? phase_rdmem : phase_exec);
assign load_stk = (insn_stk & ~irq_pend) & (insn_dmem_ren ? phase_rdmem : phase_exec);
assign load_fp = (insn_fp & ~irq_pend) & phase_exec;
assign load_gp = (insn_gp & ~irq_pend) & phase_exec;
assign load_ip = reload_ip | (phase_fetch & ~irq /* not irq_pend */);
assign load_insn = phase_fetch;
assign load_isr = (insn_isr & ~irq_pend) & phase_exec;
assign cpop = (insn_cpop & ~irq_pend) & phase_exec;
assign cpush = (insn_cpush | irq_pend) & phase_decode;
assign byt = (insn_byt & ~irq_pend);
assign dmem_ren = phase_rdmem & insn_dmem_ren;
assign dmem_wen = (insn_dmem_wen & ~irq_pend) & phase_exec;
assign set_ien = (insn_set_ien & ~irq_pend) & phase_exec;
assign clear_ien = (insn_clear_ien | irq_pend) & phase_exec;
assign phase = phase_decode ? 2'd0
             : phase_exec ? 2'd1
             : phase_rdmem ? 2'd2 : 2'd3;
assign pmem_wenh = (insn_pmem_wenh & ~irq_pend) & phase_exec;
assign pmem_wenl = (insn_pmem_wenl & ~irq_pend) & phase_exec;
assign load_sr = (insn_load_sr & ~irq_pend) & phase_exec;
assign rst_sr = (insn_rst_sr & ~irq_pend) & phase_exec;

logic phase_decode, phase_exec, phase_rdmem, phase_fetch, irq_pend;
signalizer signalizer(.*);

logic dec_sign, dec_wr_stk1;
logic [15:0] dec_imm_mask;
//logic [2:0] dec_src_a;
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
  //.src_a(dec_src_a),
  .src_a_stk0(dec_src_stk0),
  .src_a_fp(dec_src_fp),
  .src_a_gp(dec_src_gp),
  .src_a_ip(dec_src_ip),
  .src_a_cstk(dec_src_cstk),
  .src_a_sr(dec_src_sr),
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

logic insn_sign, insn_wr_stk1;
logic [15:0] insn_imm_mask;
//logic [2:0] insn_src_a;
logic insn_src_a_stk0, insn_src_a_fp, insn_src_a_gp, insn_src_a_ip,
  insn_src_a_cstk, insn_src_a_sr;
logic [1:0] insn_src_b;
logic [5:0] insn_alu_sel;
logic insn_pop, insn_push, insn_stk, insn_fp, insn_gp, insn_ip, insn_isr,
  insn_cpop, insn_cpush, insn_byt, insn_dmem_ren, insn_dmem_wen,
  insn_set_ien, insn_clear_ien, insn_call, insn_pmem_wenh, insn_pmem_wenl,
  insn_load_sr, insn_rst_sr;

// タイミング問題を無くすため、デコードと後続の信号の間にレジスタを挟み、つながりを切断する
always @(posedge clk) begin
  insn_sign <= dec_sign;
  insn_imm_mask <= dec_imm_mask;
  //insn_src_a <= dec_src_a;
  insn_src_a_stk0 <= dec_src_a_stk0;
  insn_src_a_fp   <= dec_src_a_fp;
  insn_src_a_gp   <= dec_src_a_gp;
  insn_src_a_ip   <= dec_src_a_ip;
  insn_src_a_cstk <= dec_src_a_cstk;
  insn_src_a_sr   <= dec_src_a_sr;
  insn_src_b <= dec_src_b;
  insn_alu_sel <= dec_alu_sel;
  insn_wr_stk1 <= dec_wr_stk1;
  insn_pop <= dec_pop;
  insn_push <= dec_push;
  insn_stk <= dec_stk;
  insn_fp <= dec_fp;
  insn_gp <= dec_gp;
  insn_ip <= dec_ip;
  insn_isr <= dec_isr;
  insn_cpop <= dec_cpop;
  insn_cpush <= dec_cpush;
  insn_byt <= dec_byt;
  insn_dmem_ren <= dec_dmem_ren;
  insn_dmem_wen <= dec_dmem_wen;
  insn_set_ien <= dec_set_ien;
  insn_clear_ien <= dec_clear_ien;
  insn_call <= dec_call;
  insn_pmem_wenh <= dec_pmem_wenh;
  insn_pmem_wenl <= dec_pmem_wenl;
  insn_load_sr <= dec_load_sr;
  insn_rst_sr <= dec_rst_sr;
end

assign sign = insn_sign;
assign wr_stk1 = insn_wr_stk1;

logic reload_ip;
assign reload_ip = (insn_ip | irq_pend) & phase_exec;

logic phase_half;
assign phase_half = phase_decode | phase_exec;

always @(posedge rst, posedge clk) begin
  if (rst)
    irq_pend <= 1'b0;
  else if (phase_fetch)
    irq_pend <= irq;
end

endmodule
