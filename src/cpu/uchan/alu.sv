`include "common.sv"

/*
ALU 機能

番号  名前  説明
----------------
00h   A     A
01h   INC   A + 1
02h   INC2  A + 2
03h   INC3  A + 3
04h   NOT   ~A
05h   SIGN  A ^ 0x8000
06h   EXTS  A | ((A & 0x80) ? 0xff00 : 0)
0fh   B     B
10h   AND   B & A
11h   OR    B | A
12h   XOR   B ^ A
14h   SHR   B >> A
15h   SAR   B >> A (符号付きシフト)
16h   SHL   B << A
20h   ADD   B + A
21h   SUB   B - A
22h   MUL   B * A
28h   EQ    B == A
29h   NEQ   B != A
2ah   LT    B < A (signed)
2bh   LE    B <= A (signed)
2ch   BT    B < A (unsigned)
2dh   BE    B <= A (unsigned)
30h   ADDZ  cond ? A : B+A
31h   ADDNZ cond ? B+A : A
*/

module alu#(
  parameter WIDTH = 16
) (
  input [WIDTH-1:0] a,
  input [WIDTH-1:0] b,
  input cond,
  input [5:0] sel,
  output [WIDTH-1:0] out
);

assign out = alu(a, b, cond, sel);

function [WIDTH-1:0] alu(
  input [WIDTH-1:0] a,
  input [WIDTH-1:0] b,
  input cond,
  input [5:0] sel);
begin
  logic [WIDTH-1:0] add;
  logic [WIDTH:0] sub;
  logic of;
  logic zf;
  logic sf; // sign flag
  logic lt; // less than
  logic cf; // carry flag
  logic signed [WIDTH-1:0] sb;
  add = b + a;
  sub = b - a;
  of = is_overflow(a[15], b[15], sub[15]);
  zf = sub == {WIDTH{1'b0}};
  sf = sub[15];
  lt = sf ^ of;
  cf = sub[16];
  sb = b;
  casex (sel)
    6'b0000xx: alu = a + sel[1:0];
    6'b000100: alu = ~a;
    6'b0001x1: alu = a ^ 16'h8000;
    6'b000110: alu = a | (a[7] ? 16'hff00 : 16'd0);
    6'b001xxx: alu = b;
    6'b010000: alu = b & a;
    6'b010001: alu = b | a;
    6'b01001x: alu = b ^ a;
    6'b010100: alu = b >> a;
    6'b010101: alu = sb >>> a;
    6'b010110: alu = b << a;
    6'b100000: alu = add;
    6'b100001: alu = sub;
    6'b100010: alu = b * a;
    6'b101000: alu = {{WIDTH-1{1'b0}}, zf};
    6'b101001: alu = {{WIDTH-1{1'b0}}, ~zf};
    6'b101010: alu = lt;
    6'b101011: alu = lt | zf;
    6'b101100: alu = cf;
    6'b101101: alu = cf | zf;
    6'b11xxx0: alu = cond ? a : add;
    6'b11xxx1: alu = cond ? add : a;
    default:   alu = {WIDTH{1'b0}};
  endcase
end
endfunction

function is_overflow(input a, input b, input b_sub_a);
begin
  is_overflow = (b & ~a & ~b_sub_a) | (~b & a & b_sub_a);
end
endfunction

endmodule

