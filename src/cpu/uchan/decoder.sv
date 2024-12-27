`include "common.sv"

module decoder(
  input [17:0] insn,
  output sign,
  output [15:0] imm_mask,
  output [2:0] src_a,
  output [1:0] src_b,
  output [5:0] alu_sel,
  output wr_stk1,
  output pop,
  output push,
  output load_stk,
  output load_fp,
  output load_gp,
  output load_ip,
  output load_isr,
  output cpop,
  output cpush,
  output byt,
  output dmem_ren,
  output dmem_wen,
  output set_ien, clear_ien,
  output call,
  output pmem_wenh, pmem_wenl
);

assign imm_mask = calc_imm_mask(insn[17:14]);

assign {sign, src_a, src_b, alu_sel,
  load_stk, load_fp, load_gp, load_ip, load_isr,
  push, pop, cpush, cpop,
  wr_stk1, byt, dmem_ren, dmem_wen,
  set_ien, clear_ien, call, pmem_wenh, pmem_wenl} = decode(insn);

`define STK0 `SRCA_STK0
`define FP   `SRCA_FP
`define IP   `SRCA_IP
`define CSTK `SRCA_CSTK

`define STK1 `SRCB_STK1
`define IMM  `SRCB_IMM
`define ISR  `SRCB_ISR

// 30bits = sign:1 + src_a:3 + src_b:2 + alu:6 + signals:18
function [29:0] decode(input [17:0] insn);
  logic sign;
  logic [2:0] src_a;
  logic [1:0] pp;
  logic [4:0] load;
  sign = insn[11];
  src_a = {1'b0, insn[13:12]};
  pp = insn[7:6];
  load = src_a === `IP;

  casex (insn)
                                //                   call/pmem_wenh/wenl
                                //                 set_ien/clear_ien | |
                                //       wr_stk1/byt/dmem_ren/wen || | |
                                //       push/pop/cpush/cpop |  | || | |
                                //load_stk/fp/gp/ip/isr |  | |  | || | |
    18'b11_xxxx_xxxx_xxxx_xxxx: // PUSH uimm16    |___| |__| |__| || | |
      return {1'b0, `STK0, `IMM,  `ALU_B,     18'b10000_1000_0000_00_000};
    18'b00_00xx_xxxx_xxxx_xxxx: // CALL simm14
      return {sign, `IP,   `IMM,  `ALU_ADD,   18'b00010_0010_0000_00_100};
    18'b00_0100_xxxx_xxxx_xxxx: // JMP simm12
      return {sign, `IP,   `IMM,  `ALU_ADD,   18'b00010_0000_0000_00_000};
    18'b00_0101_xxxx_xxxx_xxxx: // ADD FP,simm12
      return {sign, src_a, `IMM,  `ALU_ADD,   18'b01000_0000_0000_00_000};
    18'b00_0110_xxxx_xxxx_xxxx: // JZ simm12
      return {sign, `IP,   `IMM,  `ALU_ADDZ,  18'b00010_0100_0000_00_000};
    18'b00_1000_xxxx_xxxx_xxxx: // LD1 0+uimm12
      return {1'b0, src_a, `IMM,  `ALU_B,     18'b10000_1000_0110_00_000};
    18'b00_10xx_xxxx_xxxx_xxxx: // LD1 X+uimm12
      return {1'b0, src_a, `IMM,  `ALU_ADD,   18'b10000_1000_0110_00_000};
    18'b00_1100_xxxx_xxxx_xxxx: // ST1 0+uimm12
      return {1'b0, src_a, `IMM,  `ALU_B,     18'b00000_0100_0101_00_000};
    18'b00_11xx_xxxx_xxxx_xxxx: // ST1 X+uimm12
      return {1'b0, src_a, `IMM,  `ALU_ADD,   18'b00000_0100_0101_00_000};
    18'b01_0000_xxxx_xxxx_xxx0: // LD 0+uimm12
      return {1'b0, src_a, `IMM,  `ALU_B,     18'b10000_1000_0010_00_000};
    18'b01_00xx_xxxx_xxxx_xxx0: // LD X+uimm12
      return {1'b0, src_a, `IMM,  `ALU_ADD,   18'b10000_1000_0010_00_000};
    18'b01_0000_xxxx_xxxx_xxx1: // ST 0+uimm12
      return {1'b0, src_a, `IMM,  `ALU_B,     18'b00000_0100_0001_00_000};
    18'b01_00xx_xxxx_xxxx_xxx1: // ST X+uimm12
      return {1'b0, src_a, `IMM,  `ALU_ADD,   18'b00000_0100_0001_00_000};
    18'b01_01xx_xxxx_xxxx_xxxx: // PUSH X+uimm12
      return {1'b0, src_a, `IMM,  `ALU_ADD,   18'b10000_1000_0000_00_000};
    18'b01_11xx_0xxx_xxxx_xxxx: // stack だけを使う演算形命令
      return {1'b0, `STK0, `STK1, insn[5:0],  18'b10000_0000_0000_00_000 | (pp << 11)};
                                //                   call/pmem_wenh/wenl
                                //                 set_ien/clear_ien | |
                                //       wr_stk1/byt/dmem_ren/wen || | |
                                //       push/pop/cpush/cpop |  | || | |
                                //load_stk/fp/gp/ip/isr |  | |  | || | |
    18'b01_11xx_1xxx_xx00_0x00: // RET            |___| |__| |__| || | |
      return {1'b0, `CSTK, `IMM,  `ALU_A,     18'b00010_0001_0000_00_000};
    18'b01_11xx_1xxx_xx00_0x01: // CALL
      return {1'b0, `STK0, `IMM,  `ALU_A,     18'b00010_0110_0000_00_100};
    18'b01_11xx_1xxx_xx00_0x10: // CPOP FP
      return {1'b0, `CSTK, `IMM,  `ALU_A,     18'b01000_0001_0000_00_000};
    18'b01_11xx_1xxx_xx00_0x11: // CPUSH FP
      return {1'b0, `FP,   `IMM,  `ALU_A,     18'b00000_0010_0000_00_000};
    18'b01_11xx_1xxx_xx00_10x0: // LDD
      return {1'b0, `STK0, `IMM,  `ALU_A,     18'b10000_0000_0010_00_000};
    18'b01_11xx_1xxx_xx00_1100: // STA
      return {1'b0, `STK0, `IMM,  `ALU_A,     18'b10000_0100_1001_00_000};
    18'b01_11xx_1xxx_xx00_1110: // STD
      return {1'b0, `STK0, `IMM,  `ALU_A,     18'b00000_0100_1001_00_000};
    18'b01_11xx_1xxx_xx00_10x1: // LDD1
      return {1'b0, `STK0, `IMM,  `ALU_A,     18'b10000_0000_0110_00_000};
    18'b01_11xx_1xxx_xx00_1101: // STA1
      return {1'b0, `STK0, `IMM,  `ALU_A,     18'b10000_0100_1101_00_000};
    18'b01_11xx_1xxx_xx00_1111: // STD1
      return {1'b0, `STK0, `IMM,  `ALU_A,     18'b00000_0100_1101_00_000};
    18'b01_11xx_1xxx_xxx1_xx0x: // INT
      return {1'b0, `IP,   `ISR,  `ALU_B,     18'b00010_0010_0000_00_000};
    18'b01_11xx_1xxx_xxx1_xx1x: // IRET
      return {1'b0, `CSTK, `IMM,  `ALU_A,     18'b00010_0001_0000_10_000};
    18'b01_11xx_1xxx_xx1x_x000: // POP FP
      return {1'b0, `STK0, `IMM,  `ALU_A,     18'b01000_0100_0000_00_000};
    18'b01_11xx_1xxx_xx1x_x001: // POP GP
      return {1'b0, `STK0, `IMM,  `ALU_A,     18'b00100_0100_0000_00_000};
    18'b01_11xx_1xxx_xx1x_x010: // POP ISR
      return {1'b0, `STK0, `IMM,  `ALU_A,     18'b00001_0100_0000_00_000};
    18'b01_11xx_1xxx_xx1x_x100: // SPHA
      return {1'b0, `STK0, `IMM,  `ALU_A,     18'b10000_0100_1000_00_010};
    18'b01_11xx_1xxx_xx1x_x101: // SPLA
      return {1'b0, `STK0, `IMM,  `ALU_A,     18'b10000_0100_1000_00_001};
  endcase
endfunction

function [15:0] calc_imm_mask(input [3:0] op);
begin
  casex (op)
    4'b11xx: return 16'hffff; // imm16
    4'b0000: return 16'h3fff; // imm14
    4'b0100: return 16'h0ffe; // imm12 for ld/st
    default: return 16'h0fff; // imm12
  endcase
end
endfunction

endmodule
