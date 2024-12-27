`include "common.sv"

module signals(
  input rst,
  input clk,
  input irq,
  input [17:0] insn,
  output sign,
  output [15:0] imm_mask,
  output [2:0] src_a_sel,
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
  output pmem_wenh, pmem_wenl
);

logic src_a_fp, src_a_gp, src_a_ip, src_a_cstk;

assign src_a_sel = src_a_fp ? `SRCA_FP
                   : src_a_gp ? `SRCA_GP
                   : src_a_ip ? `SRCA_IP
                   : src_a_cstk ? `SRCA_CSTK
                   : `SRCA_STK0;
assign src_a_fp = phase_half & (insn_src_a === `SRCA_FP) & ~irq_pend;
assign src_a_gp = phase_half & (insn_src_a === `SRCA_GP) & ~irq_pend;
assign src_a_ip = ~phase_half | (insn_src_a === `SRCA_IP) | irq_pend | (insn_call & phase_decode);
assign src_a_cstk = phase_half & (insn_src_a === `SRCA_CSTK) & ~irq_pend;
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

logic phase_decode, phase_exec, phase_rdmem, phase_fetch, irq_pend;
signalizer signalizer(.*);

logic [2:0] insn_src_a;
logic [1:0] insn_src_b;
logic [5:0] insn_alu_sel;
logic insn_pop, insn_push, insn_stk, insn_fp, insn_gp, insn_ip, insn_isr,
  insn_cpop, insn_cpush, insn_byt, insn_dmem_ren, insn_dmem_wen,
  insn_set_ien, insn_clear_ien, insn_call, insn_pmem_wenh, insn_pmem_wenl;
decoder decoder(
  .insn(insn),
  .sign(sign),
  .imm_mask(imm_mask),
  .src_a(insn_src_a),
  .src_b(insn_src_b),
  .alu_sel(insn_alu_sel),
  .wr_stk1(wr_stk1),
  .pop(insn_pop),
  .push(insn_push),
  .load_stk(insn_stk),
  .load_fp(insn_fp),
  .load_gp(insn_gp),
  .load_ip(insn_ip),
  .load_isr(insn_isr),
  .cpop(insn_cpop),
  .cpush(insn_cpush),
  .byt(insn_byt),
  .dmem_ren(insn_dmem_ren),
  .dmem_wen(insn_dmem_wen),
  .set_ien(insn_set_ien),
  .clear_ien(insn_clear_ien),
  .call(insn_call),
  .pmem_wenh(insn_pmem_wenh),
  .pmem_wenl(insn_pmem_wenl)
);

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
