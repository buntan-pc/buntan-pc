`ifndef COMMON_SV
`define COMMON_SV

`define ADDR_WIDTH 14

`define ALU_A     6'h00
`define ALU_INC   6'h01
`define ALU_INC2  6'h02
`define ALU_INC3  6'h03
`define ALU_NOT   6'h04
`define ALU_SIGN  6'h05
`define ALU_B     6'h0f
`define ALU_AND   6'h10
`define ALU_OR    6'h11
`define ALU_XOR   6'h12
`define ALU_SHR   6'h14
`define ALU_SAR   6'h15
`define ALU_SHL   6'h16
`define ALU_ADD   6'h20
`define ALU_SUB   6'h21
`define ALU_MUL   6'h22
`define ALU_LT    6'h28
`define ALU_EQ    6'h29
`define ALU_NEQ   6'h2a
`define ALU_ADDZ  6'h30
`define ALU_ADDNZ 6'h31

// Source A
`define SRCA_STK0 3'h0
`define SRCA_FP   3'h1
`define SRCA_GP   3'h2
`define SRCA_IP   3'h3
`define SRCA_CSTK 3'h4
`define SRCA_X    3'hx

// Source B
`define SRCB_STK1 2'h0
`define SRCB_IMM  2'h1
`define SRCB_ISR  2'h2
`define SRCB_X    2'hx

`endif // `ifndef COMMON_SV
