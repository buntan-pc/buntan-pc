`include "common.sv"

module signals_tb;

logic rst, clk, irq;
logic [17:0] insn;
logic sign,
  wr_stk1, pop, push,
  load_stk, load_fp, load_gp, load_ip, load_insn, load_isr,
  cpop, cpush, byt, dmem_ren, dmem_wen,
  set_ien, clear_ien, pmem_wenh, pmem_wenl;
logic [2:0] src_a_sel;
logic [1:0] src_b_sel;
logic [15:0] imm_mask;
logic [5:0] alu_sel;
logic [1:0] phase;

signals signals(.*);

//`define x 1'bx
logic x;
assign x = 1'bx;

initial begin
  $dumpvars;
  $monitor("%d: rst=%d phase=%d irq=%d insn=%04x imm_mask=%04x",
           $time, rst, phase, irq, insn, imm_mask,
           " src_a_sel=%d src_b_sel=%d alu=%02x",
           src_a_sel, src_b_sel, alu_sel,
           " wr_stk1=%d pop/sh=%d%d",
           wr_stk1, pop, push,
           " load=%d%d%d%d%d%d",
           load_stk, load_fp, load_gp, load_ip, load_insn, load_isr,
           " cpop/sh=%d%d byt=%d rd=%d wr=%d",
           cpop, cpush, byt, dmem_ren, dmem_wen,
           " insn_cpush=%d", signals.insn_cpush,
           " set/clr_ien=%d/%d", set_ien, clear_ien
         );

  rst <= 1;
  clk <= 1;
  irq <= 0;
  insn <= 0;

  #13
    rst <= 0;
    test_sig_rdmem;

  @(posedge clk)
  @(negedge clk)
    test_sig_fetch;

  @(posedge clk)
    insn <= 18'h300A5; // PUSH 0xA5
    test_sig_phases(0,         // call
                    16'hffff,  // imm_mask
                    `SRCA_X,   // src_a_sel
                    `SRCB_IMM, // src_b_sel
                    `ALU_B,    // alu_sel
                    x,         // wr_stk1
                    0,         // pop
                    1,         // push
                    1,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    0,         // load_ip
                    0,         // load_isr
                    0,         // cpop
                    0,         // cpush
                    x,         // byt
                    0,         // dmem_ren
                    0,         // dmem_wen
                    0,         // set_ien
                    0          // clear_ien
                  );

  @(posedge clk)
    insn <= 18'h04234; // JMP 0x234
    test_sig_phases(0,         // call
                    16'h0fff,  // imm_mask
                    `SRCA_IP,  // src_a_sel
                    `SRCB_IMM, // src_b_sel
                    `ALU_ADD,  // alu_sel
                    x,         // wr_stk1
                    0,         // pop
                    0,         // push
                    0,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    1,         // load_ip
                    0,         // load_isr
                    0,         // cpop
                    0,         // cpush
                    x,         // byt
                    0,         // dmem_ren
                    0,         // dmem_wen
                    0,         // set_ien
                    0          // clear_ien
                  );

  @(posedge clk)
    insn <= 18'h00234; // CALL 0x234
    test_sig_phases(1,         // call
                    16'h3fff,  // imm_mask
                    `SRCA_IP,  // src_a_sel
                    `SRCB_IMM, // src_b_sel
                    `ALU_ADD,  // alu_sel
                    x,         // wr_stk1
                    0,         // pop
                    0,         // push
                    0,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    1,         // load_ip
                    0,         // load_isr
                    0,         // cpop
                    0,         // cpush
                    x,         // byt
                    0,         // dmem_ren
                    0,         // dmem_wen
                    0,         // set_ien
                    0          // clear_ien
                  );

  @(posedge clk)
    insn <= 18'h06234; // JZ 0x234
    test_sig_phases(0,         // call
                    16'h0fff,  // imm_mask
                    `SRCA_IP,  // src_a_sel
                    `SRCB_IMM, // src_b_sel
                    `ALU_ADDZ, // alu_sel
                    x,         // wr_stk1
                    1,         // pop
                    0,         // push
                    0,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    1,         // load_ip
                    0,         // load_isr
                    0,         // cpop
                    0,         // cpush
                    x,         // byt
                    0,         // dmem_ren
                    0,         // dmem_wen
                    0,         // set_ien
                    0          // clear_ien
                  );

  @(posedge clk)
    insn <= 18'h12021; // ST GP+0x20
    test_sig_phases(0,         // call
                    16'h0ffe,  // imm_mask
                    `SRCA_GP,  // src_a_sel
                    `SRCB_IMM, // src_b_sel
                    `ALU_ADD,  // alu_sel
                    0,         // wr_stk1
                    1,         // pop
                    0,         // push
                    0,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    0,         // load_ip
                    0,         // load_isr
                    0,         // cpop
                    0,         // cpush
                    0,         // byt
                    0,         // dmem_ren
                    1,         // dmem_wen
                    0,         // set_ien
                    0          // clear_ien
                  );

  @(posedge clk)
    insn <= 18'h16FFE; // PUSH GP+0xFFE
    test_sig_phases(0,         // call
                    16'h0fff,  // imm_mask
                    `SRCA_GP,  // src_a_sel
                    `SRCB_IMM, // src_b_sel
                    `ALU_ADD,  // alu_sel
                    0,         // wr_stk1
                    0,         // pop
                    1,         // push
                    1,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    0,         // load_ip
                    0,         // load_isr
                    0,         // cpop
                    0,         // cpush
                    x,         // byt
                    0,         // dmem_ren
                    0,         // dmem_wen
                    0,         // set_ien
                    0          // clear_ien
                  );

  @(posedge clk)
    insn <= 18'h1C04F; // POP
    test_sig_phases(0,         // call
                    16'hxxxx,  // imm_mask
                    `SRCA_X,   // src_a_sel
                    `SRCB_X,   // src_b_sel
                    `ALU_B,    // alu_sel
                    x,         // wr_stk1
                    1,         // pop
                    0,         // push
                    1,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    0,         // load_ip
                    0,         // load_isr
                    0,         // cpop
                    0,         // cpush
                    x,         // byt
                    0,         // dmem_ren
                    0,         // dmem_wen
                    0,         // set_ien
                    0          // clear_ien
                  );

  @(posedge clk)
    insn <= 18'h1C004; // NOT
    test_sig_phases(0,         // call
                    16'hxxxx,  // imm_mask
                    `SRCA_STK0,// src_a_sel
                    `SRCB_X,   // src_b_sel
                    `ALU_NOT,  // alu_sel
                    x,         // wr_stk1
                    0,         // pop
                    0,         // push
                    1,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    0,         // load_ip
                    0,         // load_isr
                    0,         // cpop
                    0,         // cpush
                    x,         // byt
                    0,         // dmem_ren
                    0,         // dmem_wen
                    0,         // set_ien
                    0          // clear_ien
                  );

  @(posedge clk)
    insn <= 18'h1C809; // LDD1
  @(negedge clk)
    if (~signals.phase_decode) $error("phase_decode must be 1");
    test_sig_decode(0);

  @(negedge clk)
    if (~signals.phase_exec) $error("phase_exec must be 1");
    test_sig(16'hxxxx,  // imm_mask
             `SRCA_STK0,// src_a_sel
             `SRCB_X,   // src_b_sel
             `ALU_A,    // alu_sel
             x,         // wr_stk1
             0,         // pop
             0,         // push
             0,         // load_stk
             0,         // load_fp
             0,         // load_gp
             0,         // load_ip
             0,         // load_insn
             0,         // load_isr
             0,         // cpop
             0,         // cpush
             x,         // byt
             x,         // dmem_ren
             0,         // dmem_wen
             0,         // set_ien
             0          // clear_ien
           );

  @(negedge clk)
    if (~signals.phase_rdmem) $error("phase_rdmem must be 1");
    test_sig(16'hxxxx,  // imm_mask
             `SRCA_IP,  // src_a_sel
             `SRCB_X,   // src_b_sel
             `ALU_A,    // alu_sel
             x,         // wr_stk1
             0,         // pop
             0,         // push
             1,         // load_stk
             0,         // load_fp
             0,         // load_gp
             0,         // load_ip
             0,         // load_insn
             0,         // load_isr
             0,         // cpop
             0,         // cpush
             1,         // byt
             1,         // dmem_ren
             0,         // dmem_wen
             0,         // set_ien
             0          // clear_ien
           );

  @(negedge clk)
    if (~signals.phase_fetch) $error("phase_fetch must be 1");
    test_sig_fetch;

  @(posedge clk)
    insn <= 18'h1C800; // RET
    test_sig_phases(0,         // call
                    16'hxxxx,  // imm_mask
                    `SRCA_CSTK,// src_a_sel
                    `SRCB_X,   // src_b_sel
                    `ALU_A,    // alu_sel
                    x,         // wr_stk1
                    0,         // pop
                    0,         // push
                    0,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    1,         // load_ip
                    0,         // load_isr
                    1,         // cpop
                    0,         // cpush
                    x,         // byt
                    x,         // dmem_ren
                    0,         // dmem_wen
                    0,         // set_ien
                    0          // clear_ien
                  );

  @(posedge clk)
    insn <= 18'h1C801; // CALL
    test_sig_phases(1,         // call
                    16'hxxxx,  // imm_mask
                    `SRCA_STK0,// src_a_sel
                    `SRCB_X,   // src_b_sel
                    `ALU_A,    // alu_sel
                    x,         // wr_stk1
                    1,         // pop
                    0,         // push
                    0,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    1,         // load_ip
                    0,         // load_isr
                    0,         // cpop
                    0,         // cpush
                    x,         // byt
                    0,         // dmem_ren
                    0,         // dmem_wen
                    0,         // set_ien
                    0          // clear_ien
                  );

  @(posedge clk)
    insn <= 18'h1C810; // INT
    test_sig_phases(1,         // call
                    16'hxxxx,  // imm_mask
                    `SRCA_IP,  // src_a_sel
                    `SRCB_ISR, // src_b_sel
                    `ALU_B,    // alu_sel
                    x,         // wr_stk1
                    0,         // pop
                    0,         // push
                    0,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    1,         // load_ip
                    0,         // load_isr
                    0,         // cpop
                    0,         // cpush
                    x,         // byt
                    x,         // dmem_ren
                    0,         // dmem_wen
                    0,         // set_ien
                    0          // clear_ien
                  );

  @(posedge clk)
    insn <= 18'h1C822; // POP ISR
    test_sig_phases(0,         // call
                    16'hxxxx,  // imm_mask
                    `SRCA_STK0,// src_a_sel
                    `SRCB_X,   // src_b_sel
                    `ALU_A,    // alu_sel
                    x,         // wr_stk1
                    1,         // pop
                    0,         // push
                    0,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    0,         // load_ip
                    1,         // load_isr
                    0,         // cpop
                    0,         // cpush
                    x,         // byt
                    x,         // dmem_ren
                    0,         // dmem_wen
                    0,         // set_ien
                    0          // clear_ien
                  );

  @(negedge clk)
    irq <= 1;
    // IRQ 信号は次の fetch で処理される

    insn <= 18'h300FF; // PUSH 0xFF
    if (~signals.phase_decode) $error("phase_decode must be 1");
    test_sig_decode(0);

  @(negedge clk)
    if (~signals.phase_exec) $error("phase_exec must be 1");
    test_sig(16'hFFFF,  // imm_mask
             `SRCA_X,   // src_a_sel
             `SRCB_IMM, // src_b_sel
             `ALU_B,    // alu_sel
             x,         // wr_stk1
             0,         // pop
             1,         // push
             1,         // load_stk
             0,         // load_fp
             0,         // load_gp
             0,         // load_ip
             0,         // load_insn
             0,         // load_isr
             0,         // cpop
             0,         // cpush
             x,         // byt
             0,         // dmem_ren
             0,         // dmem_wen
             0,         // set_ien
             0          // clear_ien
           );

  @(negedge clk)
    if (~signals.phase_rdmem) $error("phase_rdmem must be 1");
    test_sig_rdmem;

  @(negedge clk)
    if (~signals.phase_fetch) $error("phase_fetch must be 1");
    test_sig(16'hxxxx,  // imm_mask
             `SRCA_X,   // src_a_sel
             `SRCB_X,   // src_b_sel
             `ALU_INC,  // alu_sel
             x,         // wr_stk1
             0,         // pop
             0,         // push
             0,         // load_stk
             0,         // load_fp
             0,         // load_gp
             0,         // load_ip  IRQ=1 なら IP を更新しない
             1,         // load_insn
             0,         // load_isr
             0,         // cpop
             0,         // cpush
             x,         // byt
             x,         // dmem_ren
             0,         // dmem_wen
             0,         // set_ien
             0          // clear_ien
           );

  @(posedge clk)
    irq <= 0;
    insn <= 18'h300FE; // PUSH 0xFE
    test_sig_phases(1,         // call
                    16'hxxxx,  // imm_mask
                    `SRCA_X,   // src_a_sel
                    `SRCB_ISR, // src_b_sel
                    `ALU_B,    // alu_sel
                    x,         // wr_stk1
                    0,         // pop
                    0,         // push
                    0,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    1,         // load_ip
                    0,         // load_isr
                    0,         // cpop
                    0,         // cpush
                    x,         // byt
                    x,         // dmem_ren
                    0,         // dmem_wen
                    0,         // set_ien
                    1          // clear_ien
                  );

  @(posedge clk)
    irq <= 0;
    insn <= 18'h1C812; // IRET
    test_sig_phases(0,         // call
                    16'hxxxx,  // imm_mask
                    `SRCA_CSTK,// src_a_sel
                    `SRCB_X,   // src_b_sel
                    `ALU_A,    // alu_sel
                    x,         // wr_stk1
                    0,         // pop
                    0,         // push
                    0,         // load_stk
                    0,         // load_fp
                    0,         // load_gp
                    1,         // load_ip
                    0,         // load_isr
                    1,         // cpop
                    0,         // cpush
                    x,         // byt
                    x,         // dmem_ren
                    0,         // dmem_wen
                    1,         // set_ien
                    0          // clear_ien
                  );

  @(posedge clk)
    insn <= 18'h1C820; // POP FP
    test_sig_phases(0,         // call
                    16'hxxxx,  // imm_mask
                    `SRCA_STK0,// src_a_sel
                    `SRCB_X,   // src_b_sel
                    `ALU_A,    // alu_sel
                    x,         // wr_stk1
                    1,         // pop
                    0,         // push
                    0,         // load_stk
                    1,         // load_fp
                    0,         // load_gp
                    0,         // load_ip
                    0,         // load_isr
                    0,         // cpop
                    0,         // cpush
                    x,         // byt
                    x,         // dmem_ren
                    0,         // dmem_wen
                    0,         // set_ien
                    0          // clear_ien
                  );

  $finish;
end

always #5 begin
  clk <= ~clk;
end

`define test_sig1(SIG) \
  if (e_``SIG !== 1'bx && SIG !== e_``SIG) $error("%s must be %d", `"SIG`", e_``SIG);

task test_sig(
  input [15:0] e_imm_mask,
  input [2:0] e_src_a_sel,
  input [1:0] e_src_b_sel,
  input [5:0] e_alu_sel,
  input e_wr_stk1,
  input e_pop,
  input e_push,
  input e_load_stk,
  input e_load_fp,
  input e_load_gp,
  input e_load_ip,
  input e_load_insn,
  input e_load_isr,
  input e_cpop,
  input e_cpush,
  input e_byt,
  input e_dmem_ren,
  input e_dmem_wen,
  input e_set_ien, e_clear_ien);
begin
  if (e_imm_mask !== 16'hxxxx && imm_mask !== e_imm_mask)
    $error("imm_mask must be 0x%04x", e_imm_mask);
  if (e_src_a_sel !== 3'hx && src_a_sel !== e_src_a_sel)
    $error("src_a_sel must be %d", e_src_a_sel);
  if (e_src_b_sel !== 2'hx && src_b_sel !== e_src_b_sel)
    $error("src_b_sel must be %d", e_src_b_sel);
  if (e_alu_sel !== 6'hxx && alu_sel !== e_alu_sel)
    $error("alu_sel must be 0x%02x", e_alu_sel);
  `test_sig1(wr_stk1);
  `test_sig1(pop);
  `test_sig1(push);
  `test_sig1(load_stk);
  `test_sig1(load_fp);
  `test_sig1(load_gp);
  `test_sig1(load_ip);
  `test_sig1(load_insn);
  `test_sig1(load_isr);
  `test_sig1(cpop);
  `test_sig1(cpush);
  `test_sig1(byt);
  `test_sig1(dmem_ren);
  `test_sig1(dmem_wen);
  `test_sig1(set_ien);
  `test_sig1(clear_ien);
end
endtask

task test_sig_decode(input call);
begin
  if (call)
    test_sig(16'hxxxx,  // imm_mask
             `SRCA_IP,  // src_a_sel
             `SRCB_X,   // src_b_sel
             `ALU_A,    // alu_sel
             x,         // wr_stk1
             0,         // pop
             0,         // push
             0,         // load_stk
             0,         // load_fp
             0,         // load_gp
             0,         // load_ip
             0,         // load_insn
             0,         // load_isr
             0,         // cpop
             1,         // cpush
             x,         // byt
             x,         // dmem_ren
             x,         // dmem_wen
             0,         // set_ien
             0          // clear_ien
           );
  else
    test_sig(16'hxxxx,  // imm_mask
             `SRCA_X,   // src_a_sel
             `SRCB_X,   // src_b_sel
             6'hxx,     // alu_sel
             x,         // wr_stk1
             0,         // pop
             0,         // push
             0,         // load_stk
             0,         // load_fp
             0,         // load_gp
             0,         // load_ip
             0,         // load_insn
             0,         // load_isr
             0,         // cpop
             0,         // cpush
             x,         // byt
             x,         // dmem_ren
             x,         // dmem_wen
             0,         // set_ien
             0          // clear_ien
           );
end
endtask

task test_sig_rdmem;
begin
  if (~signals.phase_rdmem) $error("phase_rdmem must be 1");
  test_sig(16'hxxxx,  // imm_mask
           `SRCA_IP,  // src_a_sel
           `SRCB_X,   // src_b_sel
           `ALU_A,    // alu_sel
           x,         // wr_stk1
           0,         // pop
           0,         // push
           0,         // load_stk
           0,         // load_fp
           0,         // load_gp
           0,         // load_ip
           0,         // load_insn
           0,         // load_isr
           0,         // cpop
           0,         // cpush
           x,         // byt
           x,         // dmem_ren
           0,         // dmem_wen
           0,         // set_ien
           0          // clear_ien
         );
end
endtask

task test_sig_fetch;
begin
  if (~signals.phase_fetch) $error("phase_fetch must be 1");
  test_sig(16'hxxxx,  // imm_mask
           `SRCA_IP,  // src_a_sel
           `SRCB_X,   // src_b_sel
           `ALU_INC,  // alu_sel
           x,         // wr_stk1
           0,         // pop
           0,         // push
           0,         // load_stk
           0,         // load_fp
           0,         // load_gp
           1,         // load_ip
           1,         // load_insn
           0,         // load_isr
           0,         // cpop
           0,         // cpush
           x,         // byt
           x,         // dmem_ren
           0,         // dmem_wen
           0,         // set_ien
           0          // clear_ien
         );
end
endtask

task test_sig_phases(
  input call,
  input [15:0] e_imm_mask,
  input [2:0] e_src_a_sel,
  input [1:0] e_src_b_sel,
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
  input e_set_ien, e_clear_ien);
begin
  @(negedge clk)
    if (~signals.phase_decode) $error("phase_decode must be 1");
    test_sig_decode(call);

  @(negedge clk)
    if (~signals.phase_exec) $error("phase_exec must be 1");
    test_sig(e_imm_mask, e_src_a_sel, e_src_b_sel,
             e_alu_sel, e_wr_stk1, e_pop, e_push, e_load_stk, e_load_fp, e_load_gp, e_load_ip,
             0 /* load_insn */, e_load_isr,
             e_cpop, e_cpush, e_byt, e_dmem_ren, e_dmem_wen,
             e_set_ien, e_clear_ien);

  @(negedge clk)
    if (~signals.phase_rdmem) $error("phase_rdmem must be 1");
    test_sig_rdmem;

  @(negedge clk)
    if (~signals.phase_fetch) $error("phase_fetch must be 1");
    test_sig_fetch;
end
endtask

endmodule
