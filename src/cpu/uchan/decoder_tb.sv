`include "common.sv"

module decoder_tb;

logic [17:0] insn;
logic sign, wr_stk1, pop, push, load_stk, load_fp, load_gp, load_ip, load_isr,
  cpop, cpush, byt, dmem_ren, dmem_wen, set_ien, clear_ien, call, pmem_wenh, pmem_wenl;
logic [15:0] imm_mask;
logic [2:0] src_a;
logic [1:0] src_b;
logic [5:0] alu_sel;

decoder decoder(.*);

`define test_sig1(SIG) \
  if (e_``SIG !== 1'bx && SIG !== e_``SIG) $error("%s must be %d", `"SIG`", e_``SIG);

task test_sig(
  input e_sign,
  input [15:0] e_imm_mask,
  input [2:0] e_src_a,
  input [1:0] e_src_b,
  input [5:0] e_alu_sel,
  input e_wr_stk1,
  input e_pop,
  input e_push,
  input e_load_stk,
  input e_load_fp,
  input e_load_gp,
  input e_load_ip,
  input e_load_isr,
  input e_cpop,
  input e_cpush,
  input e_byt,
  input e_dmem_ren,
  input e_dmem_wen,
  input e_set_ien, e_clear_ien,
  input e_call,
  input e_pmem_wenh,
  input e_pmem_wenl
);
begin
  `test_sig1(sign);
  if (e_imm_mask !== 16'hxxxx && imm_mask !== e_imm_mask)
    $error("imm_mask must be 0x%04x", e_imm_mask);
  if (e_src_a  !== `SRCA_X && src_a  !== e_src_a)
    $error("src_a must be %d", e_src_a);
  if (e_src_b  !== 2'hx && src_b  !== e_src_b)
    $error("src_b must be %d", e_src_b);
  if (e_alu_sel  !== 6'hxx  && alu_sel  !== e_alu_sel)
    $error("alu_sel must be 0x%02x", e_alu_sel);
  `test_sig1(wr_stk1);
  `test_sig1(pop);
  `test_sig1(push);
  `test_sig1(load_stk);
  `test_sig1(load_fp);
  `test_sig1(load_gp);
  `test_sig1(load_ip);
  `test_sig1(load_isr);
  `test_sig1(cpop);
  `test_sig1(cpush);
  `test_sig1(byt);
  `test_sig1(dmem_ren);
  `test_sig1(dmem_wen);
  `test_sig1(set_ien);
  `test_sig1(clear_ien);
  `test_sig1(call);
  `test_sig1(pmem_wenh);
  `test_sig1(pmem_wenl);
end
endtask

`define x 1'bx

initial begin
  $monitor("%d: insn=%04x sign=%d mask=%04x src_a=%d src_b=%d alu=%02x wr_stk1=%d",
           $time, insn, sign, imm_mask, src_a, src_b, alu_sel, wr_stk1,
           " pop=%d push=%d load_stk=%d fp=%d ip=%d isr=%d bar=%d",
           pop, push, load_stk, load_fp, load_gp, load_ip, load_isr,
           " cpop=%d cpush=%d",
           cpop, cpush,
           " byt=%d ren=%d wen=%d set/clr_ien=%d/%d call=%d pmem_wenh/l=%d/%d",
           byt, dmem_ren, dmem_wen, set_ien, clear_ien, call, pmem_wenh, pmem_wenl);

  insn <= 18'h30BEF;    // PUSH uimm16
  #1 test_sig(0,        // sign,
              16'hffff, // imm_mask
              `SRCA_X,  // src_a
              `SRCB_IMM,// src_b
              `ALU_B,   // alu
              `x,       // wr_stk1
              0,        // pop
              1,        // push
              1,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              `x,       // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h04020; // JMP ip+0x20
  #1 test_sig(0,        // sign,
              16'h0fff, // imm_mask
              `SRCA_IP, // src_a
              `SRCB_IMM,// src_b
              `ALU_ADD, // alu
              `x,       // wr_stk1
              0,        // pop
              0,        // push
              0,        // load_stk
              0,        // load_fp
              0,        // load_gp
              1,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              `x,       // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h03ff0; // CALL ip+0x3ff0
  #1 test_sig(1,        // sign,
              16'h3fff, // imm_mask
              `SRCA_IP, // src_a
              `SRCB_IMM,// src_b
              `ALU_ADD, // alu
              `x,       // wr_stk1
              0,        // pop
              0,        // push
              0,        // load_stk
              0,        // load_fp
              0,        // load_gp
              1,        // load_ip
              0,        // load_isr
              0,        // cpop
              1,        // cpush
              `x,       // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              1,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h0A8CE; // LD1 gp+0x8CE
  #1 test_sig(0,        // sign,
              16'h0fff, // imm_mask
              `SRCA_GP, // src_a
              `SRCB_IMM,// src_b
              `ALU_ADD, // alu
              0,        // wr_stk1
              0,        // pop
              1,        // push
              1,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              1,        // byt
              1,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h0D039; // ST1 fp+0x39
  #1 test_sig(0,        // sign,
              16'h0fff, // imm_mask
              `SRCA_FP, // src_a
              `SRCB_IMM,// src_b
              `ALU_ADD, // alu
              0,        // wr_stk1
              1,        // pop
              0,        // push
              0,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              1,        // byt
              0,        // dmem_ren
              1,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h10E20; // LD zero+0xE20
  #1 test_sig(0,        // sign,
              16'h0ffe, // imm_mask
              `SRCA_X,  // src_a
              `SRCB_IMM,// src_b
              `ALU_B,   // alu
              0,        // wr_stk1
              0,        // pop
              1,        // push
              1,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              0,        // byt
              1,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h11039; // ST fp+0x38
  #1 test_sig(0,        // sign,
              16'h0ffe, // imm_mask
              `SRCA_FP, // src_a
              `SRCB_IMM,// src_b
              `ALU_ADD, // alu
              0,        // wr_stk1
              1,        // pop
              0,        // push
              0,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              0,        // byt
              0,        // dmem_ren
              1,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h10039; // ST 0+0x38
  #1 test_sig(0,        // sign,
              16'h0ffe, // imm_mask
              `SRCA_X,  // src_a
              `SRCB_IMM,// src_b
              `ALU_B,   // alu
              0,        // wr_stk1
              1,        // pop
              0,        // push
              0,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              0,        // byt
              0,        // dmem_ren
              1,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h05820; // ADD fp,0x820
  #1 test_sig(1,        // sign,
              16'h0fff, // imm_mask
              `SRCA_FP, // src_a
              `SRCB_IMM,// src_b
              `ALU_ADD, // alu
              `x,       // wr_stk1
              0,        // pop
              0,        // push
              0,        // load_stk
              1,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              `x,       // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C001; // INC
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_STK0,// src_a
              `SRCB_X,  // src_b
              `ALU_INC, // alu
              `x,       // wr_stk1
              0,        // pop
              0,        // push
              1,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              `x,       // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C005; // SIGN
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_STK0,// src_a
              `SRCB_X,  // src_b
              `ALU_SIGN,// alu
              `x,       // wr_stk1
              0,        // pop
              0,        // push
              1,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              `x,       // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C050; // AND
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_STK0,// src_a
              `SRCB_STK1,// src_b
              `ALU_AND, // alu
              `x,       // wr_stk1
              1,        // pop
              0,        // push
              1,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              `x,       // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C08F; // DUP1
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_X,  // src_a
              `SRCB_STK1,// src_b
              `ALU_B,   // alu
              `x,       // wr_stk1
              0,        // pop
              1,        // push
              1,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              `x,       // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C800; // RET
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_CSTK,// src_a
              `SRCB_X,  // src_b
              `ALU_A,   // alu
              `x,       // wr_stk1
              0,        // pop
              0,        // push
              0,        // load_stk
              0,        // load_fp
              0,        // load_gp
              1,        // load_ip
              0,        // load_isr
              1,        // cpop
              0,        // cpush
              0,        // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C801; // CALL
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_STK0,// src_a
              `SRCB_X,  // src_b
              `ALU_A,   // alu
              `x,       // wr_stk1
              1,        // pop
              0,        // push
              0,        // load_stk
              0,        // load_fp
              0,        // load_gp
              1,        // load_ip
              0,        // load_isr
              0,        // cpop
              1,        // cpush
              `x,       // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              1,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C808; // LDD
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_STK0,// src_a
              `SRCB_X,  // src_b
              `ALU_A,   // alu
              `x,       // wr_stk1
              0,        // pop
              0,        // push
              1,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              0,        // byt
              1,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C80C; // STA
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_STK0,// src_a
              `SRCB_X,  // src_b
              `ALU_A,   // alu
              1,        // wr_stk1
              1,        // pop
              0,        // push
              1,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              0,        // byt
              0,        // dmem_ren
              1,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C80E; // STD
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_STK0,// src_a
              `SRCB_X,  // src_b
              `ALU_A,   // alu
              1,        // wr_stk1
              1,        // pop
              0,        // push
              0,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              0,        // byt
              0,        // dmem_ren
              1,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C810; // INT
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_IP, // src_a
              `SRCB_ISR,// src_b
              `ALU_B,   // alu
              `x,       // wr_stk1
              0,        // pop
              0,        // push
              0,        // load_stk
              0,        // load_fp
              0,        // load_gp
              1,        // load_ip
              0,        // load_isr
              0,        // cpop
              1,        // cpush
              `x,       // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C822; // POP isr
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_STK0,// src_a
              `SRCB_X,  // src_b
              `ALU_A,   // alu
              `x,       // wr_stk1
              1,        // pop
              0,        // push
              0,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              1,        // load_isr
              0,        // cpop
              0,        // cpush
              `x,       // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C812; // IRET
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_CSTK,// src_a
              `SRCB_X,  // src_b
              `ALU_A,   // alu
              `x,       // wr_stk1
              0,        // pop
              0,        // push
              0,        // load_stk
              0,        // load_fp
              0,        // load_gp
              1,        // load_ip
              0,        // load_isr
              1,        // cpop
              0,        // cpush
              `x,       // byt
              0,        // dmem_ren
              0,        // dmem_wen
              1,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C821; // POP gp
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_STK0,// src_a
              `SRCB_X,  // src_b
              `ALU_A,   // alu
              `x,       // wr_stk1
              1,        // pop
              0,        // push
              0,        // load_stk
              0,        // load_fp
              1,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              `x,       // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C824; // SPHA
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_STK0,// src_a
              `SRCB_X,  // src_b
              `ALU_A,   // alu
              1,        // wr_stk1
              1,        // pop
              0,        // push
              1,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              0,        // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              1,        // pmem_wenh
              0         // pmem_wenl
            );

  #1 insn <= 18'h1C825; // SPLA
  #1 test_sig(`x,       // sign,
              16'hxxxx, // imm_mask
              `SRCA_STK0,// src_a
              `SRCB_X,  // src_b
              `ALU_A,   // alu
              1,        // wr_stk1
              1,        // pop
              0,        // push
              1,        // load_stk
              0,        // load_fp
              0,        // load_gp
              0,        // load_ip
              0,        // load_isr
              0,        // cpop
              0,        // cpush
              0,        // byt
              0,        // dmem_ren
              0,        // dmem_wen
              0,        // set_ien
              0,        // clear_ien
              0,        // call
              0,        // pmem_wenh
              1         // pmem_wenl
            );

end

endmodule
