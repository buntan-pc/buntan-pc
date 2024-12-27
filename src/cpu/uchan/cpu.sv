`include "common.sv"

module cpu#(
  parameter CLOCK_HZ = 27_000_000
) (
  input  rst,
  input  clk,
  input  irq,
  output dmem_ren, // dmem read enable: 明示的データメモリ読み込み命令のとき 1
  output dmem_wen, // dmem write enable: データメモリ書き込み命令のとき 1
  output dmem_byt, // dmem byte access: バイトアクセスなら 1
  output [`ADDR_WIDTH-1:0] dmem_addr,
  input  [15:0] dmem_rdata, // データメモリからの読み込みデータ
  output [15:0] dmem_wdata, // データメモリへの書き込みデータ
  output pmem_wenh, pmem_wenl,
  output [`ADDR_WIDTH-1:0] pmem_addr,
  input  [17:0] pmem_rdata, // プログラムメモリからの読み込みデータ
  output [17:0] pmem_wdata  // プログラムメモリへの書き込みデータ
);

/*
dmem_byt の意味

dmem_byt=0: dmem_rdata/dmem_wdata の [15:0] が有効
dmem_byt=1: dmem_addr の最下位ビットに応じて [15:8] か [7:0] が有効


命令リスト（即値有り）

mnemonic        17   12   7      0  説明
----------------------------------------
PUSH uimm16    |11     uimm16     | uimm16 を stack にプッシュ
CALL simm14    |0000    simm14    | コールスタックに ip をプッシュし、ip+simm14 にジャンプ
JMP simm12     |000100   simm12   | ip+simm12 にジャンプ
ADD fp,simm12  |000101   simm12   | fp += simm12
JZ  simm12     |000110   simm12   | stack から値をポップし、0 なら ip+simm12 にジャンプ
               |000111xxxxxxxxxxxx| 予約
LD1 X+uimm12   |0010xx   uimm12   | バイトバージョン
ST1 X+uimm12   |0011xx   uimm12   | バイトバージョン
LD  X+uimm12   |0100xx   uimm11  0| mem[X+uimm12] から読んだ値を stack にプッシュ
ST  X+uimm12   |0100xx   uimm11  1| stack からポップした値を mem[X+uimm12] に書く
PUSH X+uimm12  |0101xx   uimm12   | X+uimm12 を stack にプッシュ
                                    X の選択: 0=0, 1=fp, 2=gp, 3=予約
               |0110xxxxxxxxxxxxxx| 予約
               |0111xxxxxxxxxxxxxx| 即値なし命令（別表）
binop uimm10   |10 ALU6   uimm10  | uimm10 と stack[0] を使った 2 項演算

命令リスト（即値なし）

mnemonic    17     87      0  説明
------------------------------------
NOP        |011100000000000000| stack[0] に ALU-A をロードするので、ALU=00h
POP        |011100000001001111| stack をポップ
                                stack[0] に ALU-B をロードするので、ALU=0fh
POP1       |011100000001000000| stack[1] 以降をポップ（stack[0] を保持）
                                stack[0] に ALU-A をロードするので、ALU=00h
INC        |011100000000000001| stack[0]++
INC2       |011100000000000010| stack[0] += 2
NOT        |011100000000000100| stack[0] = ~stack[0]
SIGN       |011100000000000101| stack[0] = stack[0] ^ 0x8000
EXTS       |011100000000000110| stack[0] = stack[0] | (stack[0][7] ? 0xff00 : 0)

AND        |011100000001010000| stack[0] = stack[1] & stack[0]
OR         |011100000001010001| stack[0] = stack[1] | stack[0]
XOR        |011100000001010010| stack[0] = stack[1] ^ stack[0]
SHR        |011100000001010100| stack[0] = stack[1] >> stack[0]（符号なしシフト）
SAR        |011100000001010101| stack[0] = stack[1] >> stack[0]（符号付きシフト）
SHL        |011100000001010110| stack[0] = stack[1] << stack[0]

ADD        |011100000001100000| stack[0] = stack[1] + stack[0]
SUB        |011100000001100001| stack[0] = stack[1] - stack[0]
MUL        |011100000001100010| stack[0] = stack[1] * stack[0]
EQ         |011100000001101000| stack[0] = stack[1] == stack[0]
NEQ        |011100000001101001| stack[0] = stack[1] != stack[0]
LT         |011100000001101010| stack[0] = stack[1] < stack[0] （符号付き比較）
LE         |011100000001101011| stack[0] = stack[1] <= stack[0]（符号付き比較）
BT         |011100000001101100| stack[0] = stack[1] < stack[0] （符号なし比較）
BE         |011100000001101101| stack[0] = stack[1] <= stack[0]（符号なし比較）

DUP        |011100000010000000| stack[0] を stack にプッシュ
DUP1       |011100000010001111| stack[1] を stack にプッシュ
RET        |011100100000000000| コールスタックからアドレスをポップし、ジャンプ
CALL       |011100100000000001| IP を cstack にプッシュ。stack からアドレスをポップしジャンプ。
CPOP FP    |011100100000000010| コールスタックから値をポップし FP に書く
CPUSH FP   |011100100000000011| コールスタックに FP をプッシュ
LDD        |011100100000001000| stack からアドレスをポップし、mem[addr] を stack にプッシュ
STA        |011100100000001100| stack から値とアドレスをポップしメモリに書き、アドレスをプッシュ
STD        |011100100000001110| stack から値とアドレスをポップしメモリに書き、値をプッシュ
                                stack[1] = data, stack[0] = addr
LDD1       |011100100000001001| byte version
STA1       |011100100000001101| byte version
STD1       |011100100000001111| byte version
INT        |011100100000010000| ソフトウェア割り込みを発生
IRET       |011100100000010010| 割り込みハンドラから戻る
POP X      |0111001000001000xx| stack から値を取り出し、レジスタ X に書く
                              X の選択: 0=fp, 1=gp, 2=isr
SPHA       |011100100000100100| stack から値とアドレスをポップし pmem の上位 3 ビットに書き、アドレスをプッシュ
SPLA       |011100100000100101| SPHA の下位 16 ビット版
                                stack[1] = data, stack[0] = addr



即値無し命令の構造

 17    12 11  10  8    7     6   5  4 3    0
| 011100 | 0 | 000 | Push | Pop |    ALU    |  stack だけを使う演算系命令
| 011100 | 1 | 000     0     0    00 | Func |  その他の即値無し命令


信号線の構成

名前      説明
-----------------------
alu_sel   ALU の機能選択
alu_out   ALU 出力
src_a     ALU-A 入力
src_a_sel ALU-A 入力選択（`SRCA_xxx マクロ）
src_b     ALU-B 入力
src_b_sel ALU-B 入力選択（`SRCB_xxx マクロ）
wr_stk1   0/1: dmem_wdata に stack[0/1] を出力
pop/push  stack をポップ/プッシュ
load_stk  stack[0] に stack_in をロード
load_fp   FP に alu_out をロード
load_gp   GP に alu_out をロード
load_ip   IP に alu_out をロード
load_insn INSN に pmem_rdata をロード
load_isr  ISR に alu_out をロード
cpop      cstack をポップ
cpush     cstack に値をプッシュ
dmem_ren  stack_in に接続する値の選択
          0: alu_out, 1: dmem_rdata
dmem_wen  データメモリに dmem_wdata を書き込む
stack_in  stack[0] の入力値（alu_out, dmem_rdata）
imm_mask  insn から即値を取り出すためのビットマスク
pmem_wenh プログラムメモリの上位ビットに dmem_wdata[1:0] を書き込む
pmem_wenl プログラムメモリの下位ビットに dmem_wdata[15:0] を書き込む

- stack: 演算用スタック
- cstack: コールスタック（CALL の戻り先アドレスと FP の記憶）


レジスタ

名前         説明
-----------------------
fp           フレームポインタ（スタックフレームの先頭を指す）
ip           命令（instruction）ポインタ（次に実行する命令を指す）
insn         命令（instruction）レジスタ
dmem_addr_d  dmem_addr を 1 クロック遅延した値
isr          割り込みハンドラ（ISR）のアドレスを保持するレジスタ


メモリマップ（データメモリ）

addr        説明
-----------------
0000h-007fh MCU 内蔵機能
0080h-00ffh 周辺機器
0100h-3fffh データメモリ（約 16KiB）

関数のスタックフレームを指す fp の初期値は 4000h で、0 方向に成長する


メモリマップトレジスタ

addr      説明
---------------
000h-001h 無効
002h-003h カウントダウンタイマ
004h-005h カウントダウンタイマ設定
006h-007h UART 入出力（下位 1 バイトのみ有効）
008h-009h UART 設定
00Ah-00Bh ADC 変換結果
010h-011h FLASH608K: XADR
012h-013h FLASH608K: YADR
014h-015h FLASH608K: 5:NVSTR 4:PROG 3:ERASE 2:SE 1:YE 0:XE
018h-019h FLASH608K: DIN low 16 bits
01Ah-01Bh FLASH608K: DIN high 16 bits
01Ch-01Dh FLASH608K: DOUT low 16 bits
01Eh-01Fh FLASH608K: DOUT high 16 bits
020h-021h SPI シフトレジスタ（書き込みで送信開始）
022h-023h SPI ステータスレジスタ（1: CS, 0: TX_READY）
024h-025h KBC データキュー
026h-027h KBC ステータス
028h-029h I2C シフトレジスタ
02Ah-02Bh I2C ステータスレジスタ
02Ch-02Dh UART2 入出力（下位 1 バイトのみ有効）
02Eh-02Fh UART2 設定
080h      ドットマトリクス LED
081h      キャラクタ LCD
082h      GPIO
083h      Stop ボタン

004h-005h の説明
ビット 0: IF 割り込みフラグ
ビット 1: IE 割り込み有効（IF & IE == 1 で割り込み発生）

081h の説明
ビット 0: E
ビット 1: R/W
ビット 2: RS
ビット 4 - 7: DB4 - DB7

082h-083h の説明
入出力とも、下位 8 ビットが有効
入力は最後に受信された値が読める（一度読んでもクリアされない）
入力データは上位 8 ビットを 0xfe とし、下位 8 ビットに有効値を載せる

084h-085h の説明
ビット 0: RW  RF 受信データ存在フラグ（データレジスタ読み込みでクリア）
ビット 1: RW  RIE 受信割り込み有効（RF & RIE == 1 で割り込み発生）
ビット 2: RW  TF 送信データ空フラグ（データレジスタ書き込みでクリア）
ビット 3: RW  TIE 送信割り込み有効（TF & TIE == 1 で割り込み発生）


信号タイミング
doc/signal-timing-design に記載
*/

// CPU コアの信号
logic sign, wr_stk1, pop, push,
  load_stk, load_fp, load_gp, load_ip, load_isr, cpop, cpush,
  irq_masked, ien, set_ien, clear_ien;
logic [2:0] src_a_sel;
logic [1:0] src_b_sel;
logic [1:0] phase;
logic [5:0] alu_sel;
logic [15:0] stack0, stack1, stack_in, cstack0,
             alu_out, src_a, src_b, imm_mask, dmem_wdata_raw;

// レジスタ群
logic [15:0] fp, gp, ip, isr;
logic [`ADDR_WIDTH-1:0] dmem_addr_d;
logic [17:0] insn;

// 結線
assign src_a = src_a_sel === `SRCA_FP   ? fp
             : src_a_sel === `SRCA_GP   ? gp
             : src_a_sel === `SRCA_IP   ? ip
             : src_a_sel === `SRCA_CSTK ? cstack0
             : stack0;
assign src_b = src_b_sel === 2'd0 ? stack1
               : src_b_sel === 2'd1 ? mask_imm(insn[15:0], imm_mask, sign)
               : isr;
assign stack_in = dmem_ren ? byte_format(dmem_rdata, dmem_byt, dmem_addr_d[0]) : alu_out;
assign dmem_addr = alu_out[`ADDR_WIDTH-1:0];
assign dmem_wdata_raw = wr_stk1 ? stack1 : stack0;
assign dmem_wdata = dmem_addr[0] ? {dmem_wdata_raw[7:0], 8'd0} : dmem_wdata_raw;
assign pmem_addr = alu_out[`ADDR_WIDTH-1:0];
assign pmem_wdata = {dmem_wdata_raw[1:0], dmem_wdata_raw};
assign irq_masked = ien & irq;

// CPU コアモジュール群
alu alu(
  .a(src_a),
  .b(src_b),
  .cond(stack0[0]),
  .sel(alu_sel),
  .out(alu_out)
);

stack stack(
  .rst(rst),
  .clk(clk),
  .pop(pop),
  .push(push),
  .load(load_stk),
  .data_in(stack_in),
  .data0(stack0),
  .data1(stack1)
);

stack cstack(
  .rst(rst),
  .clk(clk),
  .pop(cpop),
  .push(cpush),
  .load(cpush),
  .data_in(alu_out),
  .data0(cstack0)
);

signals signals(
  .rst(rst),
  .clk(clk),
  .irq(irq_masked),
  .insn(insn),
  .sign(sign),
  .imm_mask(imm_mask),
  .src_a_sel(src_a_sel),
  .src_b_sel(src_b_sel),
  .alu_sel(alu_sel),
  .wr_stk1(wr_stk1),
  .pop(pop),
  .push(push),
  .load_stk(load_stk),
  .load_fp(load_fp),
  .load_gp(load_gp),
  .load_ip(load_ip),
  .load_insn(load_insn),
  .load_isr(load_isr),
  .cpop(cpop),
  .cpush(cpush),
  .byt(dmem_byt),
  .dmem_ren(dmem_ren),
  .dmem_wen(dmem_wen),
  .set_ien(set_ien),
  .clear_ien(clear_ien),
  .phase(phase),
  .pmem_wenh(pmem_wenh),
  .pmem_wenl(pmem_wenl)
);

// CPU コアのレジスタ群
always @(posedge clk, posedge rst) begin
  if (rst)
    fp <= 16'h4000;
  else if (load_fp)
    fp <= alu_out;
end

always @(posedge clk, posedge rst) begin
  if (rst)
    gp <= 16'h0100;
  else if (load_gp)
    gp <= alu_out;
end

always @(posedge clk, posedge rst) begin
  if (rst)
    ip <= 16'h0;
  else if (load_ip)
    ip <= alu_out;
end

always @(posedge clk, posedge rst) begin
  if (rst)
    isr <= 16'd0;
  else if (load_isr)
    isr <= alu_out;
end

always @(posedge clk, posedge rst) begin
  if (rst | load_insn)
    insn <= pmem_rdata;
end

always @(posedge clk, posedge rst) begin
  if (rst)
    dmem_addr_d <= `ADDR_WIDTH'd0;
  else
    dmem_addr_d <= dmem_addr;
end

always @(posedge clk, posedge rst) begin
  if (rst)
    ien <= 1'b1;
  else if (set_ien)
    ien <= 1'b1;
  else if (clear_ien)
    ien <= 1'b0;
end

// CPU コア用の function 定義
function [15:0] byte_format(input [15:0] val16, input dmem_byt, input addr1);
begin
  if (~dmem_byt)
    byte_format = val16;
  else begin
    if (addr1)
      byte_format = val16 >> 8;
    else
      byte_format = val16 & 16'h00ff;
  end
end
endfunction

function [15:0] mask_imm(input [15:0] imm16, input [15:0] imm_mask, input sign);
begin
  logic [5:0] sign_bits;
  logic [15:0] masked;
  sign_bits = 6'b111111 ^ imm_mask[15:10];
  masked = imm16 & imm_mask;
  mask_imm = sign ? (masked | (sign_bits << 10)) : masked;
end
endfunction

endmodule
